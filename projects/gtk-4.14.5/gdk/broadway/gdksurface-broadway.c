/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdksurface-broadway.h"

#include "gdkbroadwaydisplay.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplay-broadway.h"
#include "gdkdevice-broadway.h"
#include "gdkdisplay.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsource.h"
#include "gdkframeclockidleprivate.h"
#include "gdkpopupprivate.h"
#include "gdkprivate-broadway.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktextureprivate.h"
#include "gdktoplevelprivate.h"

#include <graphene.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

G_DEFINE_TYPE (GdkBroadwaySurface, gdk_broadway_surface, GDK_TYPE_SURFACE)

/* We need to flush in an idle rather than AFTER_PAINT, as the clock
   is frozen during e.g. surface resizes so the paint will not happen
   and the surface resize request is never flushed. */
static void
queue_flush (GdkSurface *surface)
{
  gdk_broadway_display_flush_in_idle (gdk_surface_get_display (surface));
}

static void
gdk_broadway_surface_init (GdkBroadwaySurface *impl)
{
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface    *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  GdkBroadwayDisplay *broadway_display;

  impl->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);
  gdk_surface_freeze_updates (surface);

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  _gdk_broadway_server_roundtrip (broadway_display->server, impl->id, _gdk_display_get_next_serial (display));

  gdk_display_flush (display);
}

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkSurface    *surface)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 presentation_time;
  gint64 refresh_interval;

  if (surface->update_freeze_count > 0)
    return;

  gdk_frame_clock_get_refresh_info (clock,
                                    timings->frame_time,
                                    &refresh_interval, &presentation_time);
  if (presentation_time != 0)
    {
      timings->predicted_presentation_time = presentation_time + refresh_interval;
    }
  else
    {
      timings->predicted_presentation_time = timings->frame_time + refresh_interval / 2 + refresh_interval;
    }
}

static void
connect_frame_clock (GdkSurface *surface)
{
  GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);

  g_signal_connect (frame_clock, "before-paint",
                    G_CALLBACK (on_frame_clock_before_paint), surface);
  g_signal_connect (frame_clock, "after-paint",
                    G_CALLBACK (on_frame_clock_after_paint), surface);
}

static void
disconnect_frame_clock (GdkSurface *surface)
{
  GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);

  g_signal_handlers_disconnect_by_func (frame_clock,
                                        on_frame_clock_before_paint, surface);
  g_signal_handlers_disconnect_by_func (frame_clock,
                                        on_frame_clock_after_paint, surface);
}

static void
gdk_broadway_surface_constructed (GObject *object)
{
  GdkBroadwaySurface *self = GDK_BROADWAY_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (object);
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  if (!surface->parent)
    broadway_display->toplevels = g_list_prepend (broadway_display->toplevels, self);

  self->id = _gdk_broadway_server_new_surface (broadway_display->server,
                                               self->root_x,
                                               self->root_y,
                                               1, 1);
  g_hash_table_insert (broadway_display->id_ht, GINT_TO_POINTER (self->id), surface);

  g_object_ref (self);

  G_OBJECT_CLASS (gdk_broadway_surface_parent_class)->constructed (object);

  connect_frame_clock (surface);
}

static void
gdk_broadway_surface_finalize (GObject *object)
{
  GdkBroadwaySurface *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_BROADWAY_SURFACE (object));

  impl = GDK_BROADWAY_SURFACE (object);

  _gdk_broadway_surface_grab_check_destroy (GDK_SURFACE (impl));

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (GDK_SURFACE (impl)));

  g_hash_table_remove (broadway_display->id_ht, GINT_TO_POINTER (impl->id));

  if (impl->cursor)
    g_object_unref (impl->cursor);

  broadway_display->toplevels = g_list_remove (broadway_display->toplevels, impl);

  G_OBJECT_CLASS (gdk_broadway_surface_parent_class)->finalize (object);
}

static gboolean
thaw_updates_cb (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    gdk_surface_thaw_updates (surface);
  g_object_unref (surface);
  return G_SOURCE_REMOVE;
}

void
_gdk_broadway_roundtrip_notify (GdkSurface  *surface,
                                guint32      tag,
                                gboolean     local_reply)
{
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  GdkFrameClock *clock = gdk_surface_get_frame_clock (surface);
  GdkFrameTimings *timings;

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  /* If there is no remote web client, rate limit update to once a second */
  if (local_reply)
    g_timeout_add_seconds (1, (GSourceFunc)thaw_updates_cb, g_object_ref (surface));
  else
    gdk_surface_thaw_updates (surface);

  if (timings)
    {
      timings->refresh_interval = 33333; /* default to 1/30th of a second */
      // This isn't quite right, since we've done a roundtrip back too, can we do better?
      timings->presentation_time = g_get_monotonic_time ();
      timings->complete = TRUE;


      if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
        _gdk_frame_clock_debug_print_timings (clock, timings);

  if (GDK_PROFILER_IS_RUNNING)
    _gdk_frame_clock_add_timings_to_profiler (clock, timings);
    }
}

static void
_gdk_broadway_surface_destroy (GdkSurface *surface,
                               gboolean    foreign_destroy)
{
  GdkBroadwaySurface *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_BROADWAY_SURFACE (surface);

  disconnect_frame_clock (surface);

  if (impl->node_data)
    g_array_unref (impl->node_data);
  if (impl->node_data_textures)
    g_ptr_array_unref (impl->node_data_textures);

  _gdk_broadway_surface_grab_check_destroy (surface);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  g_hash_table_remove (broadway_display->id_ht, GINT_TO_POINTER (impl->id));

  _gdk_broadway_server_destroy_surface (broadway_display->server, impl->id);
}

void
gdk_broadway_surface_set_nodes (GdkSurface *surface,
                                GArray     *nodes,
                                GPtrArray  *node_textures)
{
  GdkBroadwaySurface *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_BROADWAY_SURFACE (surface);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  if (nodes)
    g_array_ref (nodes);
  if (impl->node_data)
    g_array_unref (impl->node_data);
  impl->node_data = nodes;

  if (node_textures)
    g_ptr_array_ref (node_textures);
  if (impl->node_data_textures)
    g_ptr_array_unref (impl->node_data_textures);
  impl->node_data_textures = node_textures;

  gdk_broadway_server_surface_set_nodes (broadway_display->server, impl->id, impl->node_data);
}

/* This function is called when the XSurface is really gone.
 */
static void
gdk_broadway_surface_destroy_notify (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    _gdk_surface_destroy (surface, TRUE);

  g_object_unref (surface);
}

static void
gdk_broadway_surface_show (GdkSurface *surface,
                           gboolean    already_mapped)
{
  GdkBroadwaySurface *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_BROADWAY_SURFACE (surface);
  impl->visible = TRUE;

  /* FIXME: update state ? */

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  if (_gdk_broadway_server_surface_show (broadway_display->server, impl->id))
    queue_flush (surface);

}

static void
gdk_broadway_surface_hide (GdkSurface *surface)
{
  GdkBroadwaySurface *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_BROADWAY_SURFACE (surface);
  impl->visible = FALSE;

  /* FIXME: update state ? */

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  _gdk_broadway_surface_grab_check_unmap (surface,
                                         _gdk_broadway_server_get_next_serial (broadway_display->server));

  if (_gdk_broadway_server_surface_hide (broadway_display->server, impl->id))
    queue_flush (surface);

  _gdk_surface_clear_update_area (surface);
}

static double
gdk_broadway_surface_get_scale (GdkSurface *surface)
{
  return GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface))->scale_factor;
}

static void
sync_child_root_pos (GdkSurface *parent)
{
  GdkBroadwaySurface *parent_impl = GDK_BROADWAY_SURFACE (parent);
  GdkBroadwayDisplay *broadway_display;
  GList *l;

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (parent));

  for (l = parent->children; l; l = l->next)
    {
      GdkBroadwaySurface *child_impl = l->data;
      GdkSurface *child = GDK_SURFACE (child_impl);
      int root_x, root_y;

      root_x = child->x + parent_impl->root_x;
      root_y = child->y + parent_impl->root_y;

      if (root_x != child_impl->root_x ||
          root_y != child_impl->root_y)
        {
          child_impl->root_x = root_x;
          child_impl->root_y = root_y;

          _gdk_broadway_server_surface_move_resize (broadway_display->server,
                                                    child_impl->id,
                                                    TRUE,
                                                    child_impl->root_x, child_impl->root_y,
                                                    child->width, child->height);
          sync_child_root_pos (child);
        }
    }
}

/* x, y is relative to parent */
static void
gdk_broadway_surface_move_resize_internal (GdkSurface *surface,
                                           gboolean    with_move,
                                           int         x,
                                           int         y,
                                           int         width,
                                           int         height)
{
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  GdkBroadwayDisplay *broadway_display;
  gboolean size_changed;

  if (with_move)
    {
      surface->x = x;
      surface->y = y;
      impl->root_x = x;
      impl->root_y = y;
      if (surface->parent)
        {
          GdkBroadwaySurface *parent_impl = GDK_BROADWAY_SURFACE (surface->parent);
          impl->root_x += parent_impl->root_x;
          impl->root_y += parent_impl->root_y;
        }
    }

  size_changed = FALSE;

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  if (width > 0 || height > 0)
    {
      if (width < 1)
        width = 1;

      if (height < 1)
        height = 1;

      if (width != surface->width ||
          height != surface->height)
        {
          size_changed = TRUE;

          /* Resize clears the content */
          impl->dirty = TRUE;
          impl->last_synced = FALSE;

          surface->width = width;
          surface->height = height;
        }
    }

  _gdk_broadway_server_surface_move_resize (broadway_display->server,
                                            impl->id,
                                            with_move,
                                            impl->root_x, impl->root_y,
                                            surface->width, surface->height);
  sync_child_root_pos (surface);

  queue_flush (surface);
  if (size_changed)
    {
      surface->resize_count++;
      _gdk_surface_update_size (surface);
    }
}

void
gdk_broadway_surface_move_resize (GdkSurface *surface,
                                  int         x,
                                  int         y,
                                  int         width,
                                  int         height)
{
  gdk_broadway_surface_move_resize_internal (surface, TRUE,
                                             x, y,
                                             width, height);
}

static void
gdk_broadway_surface_toplevel_resize (GdkSurface *surface,
                                      int         width,
                                      int         height)
{
  gdk_broadway_surface_move_resize_internal (surface, FALSE,
                                             0, 0,
                                             width, height);
}

static void
gdk_broadway_surface_move (GdkSurface *surface,
                           int         x,
                           int         y)
{
  gdk_broadway_surface_move_resize_internal (surface, TRUE, x, y, -1, -1);
}

static void
gdk_broadway_surface_layout_popup (GdkSurface     *surface,
                                   int             width,
                                   int             height,
                                   GdkPopupLayout *layout)
{
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  GdkMonitor *monitor;
  GdkRectangle bounds;
  GdkRectangle final_rect;
  int x, y;

  monitor = gdk_surface_get_layout_monitor (surface, layout,
                                            gdk_monitor_get_geometry);
  gdk_monitor_get_geometry (monitor, &bounds);

  gdk_surface_layout_popup_helper (surface,
                                   width,
                                   height,
                                   impl->shadow_left,
                                   impl->shadow_right,
                                   impl->shadow_top,
                                   impl->shadow_bottom,
                                   monitor,
                                   &bounds,
                                   layout,
                                   &final_rect);

  x = final_rect.x;
  y = final_rect.y;

  if (final_rect.width != surface->width ||
      final_rect.height != surface->height)
    {
      gdk_broadway_surface_move_resize (surface,
                                        x,
                                        y,
                                        final_rect.width,
                                        final_rect.height);
    }
  else
    {
      gdk_broadway_surface_move (surface, x, y);
    }
}

static void
show_popup (GdkSurface *surface)
{
  gdk_surface_set_is_mapped (surface, TRUE);
  gdk_broadway_surface_show (surface, FALSE);
  gdk_surface_invalidate_rect (surface, NULL);
}

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  show_popup (surface);
}

static gboolean
gdk_broadway_surface_present_popup (GdkSurface     *surface,
                                    int             width,
                                    int             height,
                                    GdkPopupLayout *layout)
{
  gdk_broadway_surface_layout_popup (surface, width, height, layout);

  if (GDK_SURFACE_IS_MAPPED (surface))
    return TRUE;

  if (surface->autohide)
    {
      gdk_seat_grab (gdk_display_get_default_seat (surface->display),
                     surface,
                     GDK_SEAT_CAPABILITY_ALL,
                     TRUE,
                     NULL, NULL,
                     show_grabbing_popup, NULL);
    }
  else
    {
      show_popup (surface);
    }

  return GDK_SURFACE_IS_MAPPED (surface);
}

static void
gdk_broadway_surface_focus (GdkSurface *surface,
                            guint32    timestamp)
{
  GdkBroadwaySurface *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_BROADWAY_SURFACE (surface);
  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  _gdk_broadway_server_surface_focus (broadway_display->server,
                                      impl->id);
}

static void
gdk_broadway_surface_set_geometry_hints (GdkSurface        *surface,
                                         const GdkGeometry *geometry,
                                         GdkSurfaceHints    geom_mask)
{
  GdkBroadwaySurface *impl;

  impl = GDK_BROADWAY_SURFACE (surface);

  impl->geometry_hints = *geometry;
  impl->geometry_hints_mask = geom_mask;
}

static void
gdk_broadway_surface_set_title (GdkSurface  *surface,
                                const char *title)
{
}

static void
gdk_broadway_surface_set_startup_id (GdkSurface  *surface,
                                     const char *startup_id)
{
}

static void
gdk_broadway_surface_set_transient_for (GdkSurface *surface,
                                        GdkSurface *parent)
{
  GdkBroadwayDisplay *display;
  GdkBroadwaySurface *impl;
  int parent_id;

  impl = GDK_BROADWAY_SURFACE (surface);

  /* We treat the real parent as a default transient for to get stacking right */
  if (parent == NULL)
    parent = surface->parent;

  parent_id = 0;
  if (parent)
    parent_id = GDK_BROADWAY_SURFACE (parent)->id;

  impl->transient_for = parent_id;

  display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  _gdk_broadway_server_surface_set_transient_for (display->server, impl->id, impl->transient_for);
}

static void
gdk_broadway_surface_set_modal_hint (GdkSurface *surface,
                                     gboolean   modal)
 {
  GdkBroadwayDisplay *display;
  GdkBroadwaySurface *impl;

  impl = GDK_BROADWAY_SURFACE (surface);

  impl->modal_hint = modal;

  display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  _gdk_broadway_server_surface_set_modal_hint (display->server, impl->id, impl->modal_hint);
 }

static void
gdk_broadway_surface_get_geometry (GdkSurface *surface,
                                   int        *x,
                                   int        *y,
                                   int        *width,
                                   int        *height)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  /* TODO: These should really roundtrip to the client to get the current data */

  if (x)
    *x = surface->x;
  if (y)
    *y = surface->y;
  if (width)
    *width = surface->width;
  if (height)
    *height = surface->height;

}

static void
gdk_broadway_surface_get_root_coords (GdkSurface *surface,
                                      int         x,
                                      int         y,
                                      int        *root_x,
                                      int        *root_y)
{
  GdkBroadwaySurface *impl;

  impl = GDK_BROADWAY_SURFACE (surface);

  if (root_x)
    *root_x = x + impl->root_x;
  if (root_y)
    *root_y = y + impl->root_y;
}

static gboolean
gdk_broadway_surface_get_device_state (GdkSurface      *surface,
                                       GdkDevice       *device,
                                       double          *x,
                                       double          *y,
                                       GdkModifierType *mask)
{
  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), FALSE);

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  gdk_broadway_device_query_state (device, surface, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;
}

static void
gdk_broadway_surface_set_input_region (GdkSurface     *surface,
                                       cairo_region_t *shape_region)
{
}

static void
gdk_broadway_surface_minimize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;
}

static void
gdk_broadway_surface_unminimize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;
}

static void
gdk_broadway_surface_maximize (GdkSurface *surface)
{
  GdkBroadwaySurface *impl;
  GdkDisplay *display;
  GdkRectangle geom;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_BROADWAY_SURFACE (surface);

  if (impl->maximized)
    return;

  impl->maximized = TRUE;

  gdk_synthesize_surface_state (surface, 0, GDK_TOPLEVEL_STATE_MAXIMIZED);

  impl->pre_maximize_x = surface->x;
  impl->pre_maximize_y = surface->y;
  impl->pre_maximize_width = surface->width;
  impl->pre_maximize_height = surface->height;

  display = gdk_surface_get_display (surface);
  gdk_monitor_get_geometry (GDK_BROADWAY_DISPLAY (display)->monitor, &geom);

  gdk_broadway_surface_move_resize (surface,
                                    geom.x, geom.y,
                                    geom.width, geom.height);
}

static void
gdk_broadway_surface_unmaximize (GdkSurface *surface)
{
  GdkBroadwaySurface *impl;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_BROADWAY_SURFACE (surface);

  if (!impl->maximized)
    return;

  impl->maximized = FALSE;

  gdk_synthesize_surface_state (surface, GDK_TOPLEVEL_STATE_MAXIMIZED, 0);

  gdk_broadway_surface_move_resize (surface,
                                    impl->pre_maximize_x,
                                    impl->pre_maximize_y,
                                    impl->pre_maximize_width,
                                    impl->pre_maximize_height);
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;

  GdkSurface *moveresize_surface;
  GdkSurface *moveresize_emulation_surface;
  gboolean is_resize;
  GdkSurfaceEdge resize_edge;
  int moveresize_button;
  int moveresize_x;
  int moveresize_y;
  int moveresize_orig_x;
  int moveresize_orig_y;
  int moveresize_orig_width;
  int moveresize_orig_height;
  long moveresize_process_time;
  GdkSurfaceHints moveresize_geom_mask;
  GdkGeometry moveresize_geometry;
  BroadwayInputMsg *moveresize_pending_event;
};

static MoveResizeData *
get_move_resize_data (GdkDisplay *display,
                      gboolean    create)
{
  GdkBroadwayDisplay *broadway_display;
  MoveResizeData *mv_resize;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  mv_resize = broadway_display->move_resize_data;

  if (!mv_resize && create)
    {
      mv_resize = g_new0 (MoveResizeData, 1);
      mv_resize->display = display;

      broadway_display->move_resize_data = mv_resize;
    }

  return mv_resize;
}

static void
update_pos (MoveResizeData *mv_resize,
            int             new_root_x,
            int             new_root_y)
{
  int dx, dy;

  dx = new_root_x - mv_resize->moveresize_x;
  dy = new_root_y - mv_resize->moveresize_y;

  if (mv_resize->is_resize)
    {
      int x, y, w, h;

      x = mv_resize->moveresize_orig_x;
      y = mv_resize->moveresize_orig_y;

      w = mv_resize->moveresize_orig_width;
      h = mv_resize->moveresize_orig_height;

      switch (mv_resize->resize_edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          x += dx;
          y += dy;
          w -= dx;
          h -= dy;
          break;
        case GDK_SURFACE_EDGE_NORTH:
          y += dy;
          h -= dy;
          break;
        case GDK_SURFACE_EDGE_NORTH_EAST:
          y += dy;
          h -= dy;
          w += dx;
          break;
        case GDK_SURFACE_EDGE_SOUTH_WEST:
          h += dy;
          x += dx;
          w -= dx;
          break;
        case GDK_SURFACE_EDGE_SOUTH_EAST:
          w += dx;
          h += dy;
          break;
        case GDK_SURFACE_EDGE_SOUTH:
          h += dy;
          break;
        case GDK_SURFACE_EDGE_EAST:
          w += dx;
          break;
        case GDK_SURFACE_EDGE_WEST:
          x += dx;
          w -= dx;
          break;
        default:
          break;
        }

      x = MAX (x, 0);
      y = MAX (y, 0);
      w = MAX (w, 1);
      h = MAX (h, 1);

      if (mv_resize->moveresize_geom_mask)
        {
          gdk_surface_constrain_size (&mv_resize->moveresize_geometry,
                                     mv_resize->moveresize_geom_mask,
                                     w, h, &w, &h);
        }

      gdk_broadway_surface_move_resize (mv_resize->moveresize_surface,
                                        x, y, w, h);
    }
  else
    {
      int x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_broadway_surface_move (mv_resize->moveresize_surface, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_surface_destroy (mv_resize->moveresize_emulation_surface);
  mv_resize->moveresize_emulation_surface = NULL;
  g_object_unref (mv_resize->moveresize_surface);
  mv_resize->moveresize_surface = NULL;
  g_clear_pointer (&mv_resize->moveresize_pending_event, g_free);
}

static gboolean
moveresize_lookahead (GdkDisplay *display,
                      MoveResizeData *mv_resize,
                      BroadwayInputMsg *event)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  return !_gdk_broadway_server_lookahead_event (broadway_display->server, "mb");
}

gboolean
_gdk_broadway_moveresize_handle_event (GdkDisplay *display,
                                       BroadwayInputMsg *event)
{
  guint button_mask = 0;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || !mv_resize->moveresize_surface)
    return FALSE;

  button_mask = GDK_BUTTON1_MASK << (mv_resize->moveresize_button - 1);

  switch (event->base.type)
    {
    case BROADWAY_EVENT_TOUCH:
      if (event->touch.touch_type == 2) /* END */
        {
          update_pos (mv_resize,
                      event->touch.root_x,
                      event->touch.root_y);

          finish_drag (mv_resize);
        }
      else if (event->touch.touch_type == 1) /* UPDATE */
        {
          if (mv_resize->moveresize_surface->resize_count > 0)
            {
              if (mv_resize->moveresize_pending_event)
                *mv_resize->moveresize_pending_event = *event;
              else
                mv_resize->moveresize_pending_event =
                  g_memdup2 (event, sizeof (BroadwayInputMsg));

              break;
            }
          update_pos (mv_resize,
                      event->touch.root_x,
                      event->touch.root_y);
        }

      break;

    case BROADWAY_EVENT_POINTER_MOVE:
      if (mv_resize->moveresize_surface->resize_count > 0)
        {
          if (mv_resize->moveresize_pending_event)
            *mv_resize->moveresize_pending_event = *event;
          else
            mv_resize->moveresize_pending_event =
              g_memdup2 (event, sizeof (BroadwayInputMsg));

          break;
        }
      if (!moveresize_lookahead (display, mv_resize, event))
        break;

      update_pos (mv_resize,
                  event->pointer.root_x,
                  event->pointer.root_y);

      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->pointer.state & button_mask) == 0)
        finish_drag (mv_resize);
      break;

    case BROADWAY_EVENT_BUTTON_RELEASE:
      update_pos (mv_resize,
                  event->pointer.root_x,
                  event->pointer.root_y);

      if (event->button.button == mv_resize->moveresize_button)
        finish_drag (mv_resize);
      break;
    default:
      break;
    }
  return TRUE;
}

gboolean
_gdk_broadway_moveresize_configure_done (GdkDisplay *display,
                                         GdkSurface  *surface)
{
  BroadwayInputMsg *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || surface != mv_resize->moveresize_surface)
    return FALSE;

  if (mv_resize->moveresize_pending_event)
    {
      tmp_event = mv_resize->moveresize_pending_event;
      mv_resize->moveresize_pending_event = NULL;
      _gdk_broadway_moveresize_handle_event (display, tmp_event);
      g_free (tmp_event);
    }

  return TRUE;
}

static GdkSurface *
gdk_broadway_drag_surface_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DRAG_SURFACE,
                       "display", display,
                       NULL);
}

static void
create_moveresize_surface (MoveResizeData *mv_resize,
                           guint32         timestamp)
{
  GdkGrabStatus status;
  GdkSeat *seat;
  GdkDevice *pointer;

  g_assert (mv_resize->moveresize_emulation_surface == NULL);

  mv_resize->moveresize_emulation_surface =
      gdk_broadway_drag_surface_new (mv_resize->display);

  gdk_broadway_surface_move_resize_internal (mv_resize->moveresize_emulation_surface, TRUE, -100, -100, 1, 1);
  gdk_broadway_surface_show (mv_resize->moveresize_emulation_surface, FALSE);

  seat = gdk_display_get_default_seat (mv_resize->display);
  pointer = gdk_seat_get_pointer (seat);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  status = gdk_device_grab (pointer,
                            mv_resize->moveresize_emulation_surface,
                            FALSE,
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_POINTER_MOTION_MASK,
                            NULL,
                            timestamp);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the surface
       * already.
       */
      finish_drag (mv_resize);
    }

  mv_resize->moveresize_process_time = 0;
}

static void
calculate_unmoving_origin (MoveResizeData *mv_resize)
{
  gdk_surface_get_geometry (mv_resize->moveresize_surface,
                            &mv_resize->moveresize_orig_x,
                            &mv_resize->moveresize_orig_y,
                            NULL, NULL);
}

static void
gdk_broadway_toplevel_begin_resize (GdkToplevel    *toplevel,
                                    GdkSurfaceEdge  edge,
                                    GdkDevice      *device,
                                    int             button,
                                    double          x,
                                    double          y,
                                    guint32         timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  MoveResizeData *mv_resize;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_surface_get_display (surface), TRUE);

  if (mv_resize->moveresize_surface != NULL)
    return; /* already a drag operation in progress */

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->moveresize_x = x + surface->x;
  mv_resize->moveresize_y = y + surface->y;
  mv_resize->moveresize_surface = g_object_ref (surface);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (surface);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (surface);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_surface (mv_resize, timestamp);
}

static void
gdk_broadway_toplevel_begin_move (GdkToplevel *toplevel,
                                  GdkDevice   *device,
                                  int          button,
                                  double       x,
                                  double       y,
                                  guint32      timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  MoveResizeData *mv_resize;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_surface_get_display (surface), TRUE);

  if (mv_resize->moveresize_surface != NULL)
    return; /* already a drag operation in progress */

  mv_resize->is_resize = FALSE;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = x + surface->x;
  mv_resize->moveresize_y = y + surface->y;
  mv_resize->moveresize_surface = g_object_ref (surface);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (surface);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (surface);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_surface (mv_resize, timestamp);
}

static gboolean
gdk_broadway_surface_beep (GdkSurface *surface)
{
  return FALSE;
}

static void
gdk_broadway_surface_class_init (GdkBroadwaySurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = gdk_broadway_surface_constructed;
  object_class->finalize = gdk_broadway_surface_finalize;

  impl_class->hide = gdk_broadway_surface_hide;
  impl_class->get_geometry = gdk_broadway_surface_get_geometry;
  impl_class->get_root_coords = gdk_broadway_surface_get_root_coords;
  impl_class->get_device_state = gdk_broadway_surface_get_device_state;
  impl_class->set_input_region = gdk_broadway_surface_set_input_region;
  impl_class->destroy = _gdk_broadway_surface_destroy;
  impl_class->beep = gdk_broadway_surface_beep;
  impl_class->destroy_notify = gdk_broadway_surface_destroy_notify;
  impl_class->drag_begin = _gdk_broadway_surface_drag_begin;
  impl_class->get_scale = gdk_broadway_surface_get_scale;
}

#define LAST_PROP 1

typedef struct
{
  GdkBroadwaySurface parent_instance;
} GdkBroadwayPopup;

typedef struct
{
  GdkBroadwaySurfaceClass parent_class;
} GdkBroadwayPopupClass;

static void gdk_broadway_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkBroadwayPopup, gdk_broadway_popup, GDK_TYPE_BROADWAY_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                gdk_broadway_popup_iface_init))

static void
gdk_broadway_popup_init (GdkBroadwayPopup *popup)
{
}

static void
gdk_broadway_popup_constructed (GObject *object)
{
  GdkBroadwaySurface *self = GDK_BROADWAY_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (self);
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  self->root_x = GDK_BROADWAY_SURFACE (surface->parent)->root_x;
  self->root_y = GDK_BROADWAY_SURFACE (surface->parent)->root_y;

  gdk_surface_set_frame_clock (surface, gdk_surface_get_frame_clock (surface->parent));

  G_OBJECT_CLASS (gdk_broadway_popup_parent_class)->constructed (object);

  /* We treat the real parent as a default transient for to get stacking right */
  self->transient_for = GDK_BROADWAY_SURFACE (surface->parent)->id;
  _gdk_broadway_server_surface_set_transient_for (broadway_display->server, self->id, self->transient_for);
}

static void
gdk_broadway_popup_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_broadway_popup_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent != NULL)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_broadway_popup_class_init (GdkBroadwayPopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_broadway_popup_constructed;
  object_class->get_property = gdk_broadway_popup_get_property;
  object_class->set_property = gdk_broadway_popup_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
gdk_broadway_popup_present (GdkPopup       *popup,
                            int             width,
                            int             height,
                            GdkPopupLayout *layout)
{
  return gdk_broadway_surface_present_popup (GDK_SURFACE (popup), width, height, layout);
}

static GdkGravity
gdk_broadway_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_broadway_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_broadway_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_broadway_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_broadway_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_broadway_popup_present;
  iface->get_surface_anchor = gdk_broadway_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_broadway_popup_get_rect_anchor;
  iface->get_position_x = gdk_broadway_popup_get_position_x;
  iface->get_position_y = gdk_broadway_popup_get_position_y;
}

typedef struct
{
  GdkBroadwaySurface parent_instance;
} GdkBroadwayToplevel;

typedef struct
{
  GdkBroadwaySurfaceClass parent_class;
} GdkBroadwayToplevelClass;

static void gdk_broadway_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkBroadwayToplevel, gdk_broadway_toplevel, GDK_TYPE_BROADWAY_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                gdk_broadway_toplevel_iface_init))

static void
gdk_broadway_toplevel_init (GdkBroadwayToplevel *toplevel)
{
}

static void
gdk_broadway_toplevel_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkFrameClock *frame_clock;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_broadway_toplevel_parent_class)->constructed (object);
}

static void
gdk_broadway_toplevel_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_broadway_surface_set_title (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      gdk_broadway_surface_set_startup_id (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_broadway_surface_set_transient_for (surface, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      gdk_broadway_surface_set_modal_hint (surface, g_value_get_boolean (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_broadway_toplevel_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, surface->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, surface->modal_hint);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_broadway_toplevel_class_init (GdkBroadwayToplevelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_broadway_toplevel_constructed;
  object_class->get_property = gdk_broadway_toplevel_get_property;
  object_class->set_property = gdk_broadway_toplevel_set_property;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
show_surface (GdkSurface *surface)
{
  gboolean was_mapped;

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  if (!was_mapped)
    gdk_surface_set_is_mapped (surface, TRUE);

  gdk_broadway_surface_show (surface, FALSE);

  if (!was_mapped)
    gdk_surface_invalidate_rect (surface, NULL);
}

static void
gdk_broadway_toplevel_present (GdkToplevel       *toplevel,
                               GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkBroadwaySurface *impl = GDK_BROADWAY_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;
  GdkToplevelSize size;
  int bounds_width, bounds_height;
  int width, height;
  GdkGeometry geometry;
  GdkSurfaceHints mask;
  gboolean maximize;

  gdk_broadway_surface_unminimize (surface);

  monitor = gdk_display_get_monitor_at_surface (display, surface);
  if (monitor)
    {
      GdkRectangle monitor_geometry;

      gdk_monitor_get_geometry (monitor, &monitor_geometry);
      bounds_width = monitor_geometry.width;
      bounds_height = monitor_geometry.height;
    }
  else
    {
      bounds_width = G_MAXINT;
      bounds_height = G_MAXINT;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (toplevel, &size);
  g_warn_if_fail (size.width > 0);
  g_warn_if_fail (size.height > 0);
  width = size.width;
  height = size.height;

  if (gdk_toplevel_layout_get_resizable (layout))
    {
      geometry.min_width = size.min_width;
      geometry.min_height = size.min_height;
      mask = GDK_HINT_MIN_SIZE;
    }
  else
    {
      geometry.max_width = geometry.min_width = width;
      geometry.max_height = geometry.min_height = height;
      mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
    }
  gdk_broadway_surface_set_geometry_hints (surface, &geometry, mask);
  gdk_surface_constrain_size (&geometry, mask, width, height, &width, &height);
  gdk_broadway_surface_toplevel_resize (surface, width, height);

  if (gdk_toplevel_layout_get_maximized (layout, &maximize))
    {
      if (maximize)
        gdk_broadway_surface_maximize (surface);
      else
        gdk_broadway_surface_unmaximize (surface);
    }

  if (size.shadow.is_valid)
    {
      impl->shadow_left = size.shadow.left;
      impl->shadow_right = size.shadow.right;
      impl->shadow_top = size.shadow.top;
      impl->shadow_bottom = size.shadow.bottom;
    }

  show_surface (surface);
}

static gboolean
gdk_broadway_toplevel_minimize (GdkToplevel *toplevel)
{
  gdk_broadway_surface_minimize (GDK_SURFACE (toplevel));

  return TRUE;
}

static gboolean
gdk_broadway_toplevel_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_broadway_toplevel_focus (GdkToplevel *toplevel,
                             guint32      timestamp)
{
  gdk_broadway_surface_focus (GDK_SURFACE (toplevel), timestamp);
}

static gboolean
gdk_broadway_toplevel_show_window_menu (GdkToplevel *toplevel,
                                        GdkEvent    *event)
{
  return FALSE;
}

static void
gdk_broadway_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_broadway_toplevel_present;
  iface->minimize = gdk_broadway_toplevel_minimize;
  iface->lower = gdk_broadway_toplevel_lower;
  iface->focus = gdk_broadway_toplevel_focus;
  iface->show_window_menu = gdk_broadway_toplevel_show_window_menu;
  iface->begin_resize = gdk_broadway_toplevel_begin_resize;
  iface->begin_move = gdk_broadway_toplevel_begin_move;
}

typedef struct
{
  GdkBroadwaySurface parent_instance;
} GdkBroadwayDragSurface;

typedef struct
{
  GdkBroadwaySurfaceClass parent_class;
} GdkBroadwayDragSurfaceClass;

static void gdk_broadway_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkBroadwayDragSurface, gdk_broadway_drag_surface, GDK_TYPE_BROADWAY_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                gdk_broadway_drag_surface_iface_init))

static void
gdk_broadway_drag_surface_init (GdkBroadwayDragSurface *surface)
{
}

static void
gdk_broadway_drag_surface_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkFrameClock *frame_clock;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_broadway_drag_surface_parent_class)->constructed (object);
}

static void
gdk_broadway_drag_surface_class_init (GdkBroadwayDragSurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_broadway_drag_surface_constructed;
}

static gboolean
gdk_broadway_drag_surface_present (GdkDragSurface *drag_surface,
                                   int             width,
                                   int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);

  gdk_broadway_surface_toplevel_resize (surface, width, height);
  show_surface (surface);

  return TRUE;
}

static void
gdk_broadway_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_broadway_drag_surface_present;
}
