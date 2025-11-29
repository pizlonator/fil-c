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

#include "gdksurface-x11.h"

#include "gdksurfaceprivate.h"
#include "gdkpopupprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-xi2-private.h"
#include "gdkframeclockidleprivate.h"
#include "gdkasync.h"
#include "gdkeventsource.h"
#include "gdkdisplay-x11.h"
#include "gdkglcontext-x11.h"
#include "gdkprivate-x11.h"
#include "gdktextureprivate.h"
#include "gdkdragsurfacesizeprivate.h"

#include "gdkseatprivate.h"
#include "gdkprivate.h"

#include <graphene.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cairo-xlib.h>

#include "MwmUtil.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <X11/extensions/shape.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

const int _gdk_x11_event_mask_table[21] =
{
  ExposureMask,
  PointerMotionMask,
  PointerMotionHintMask,
  ButtonMotionMask,
  Button1MotionMask,
  Button2MotionMask,
  Button3MotionMask,
  ButtonPressMask,
  ButtonReleaseMask,
  KeyPressMask,
  KeyReleaseMask,
  EnterWindowMask,
  LeaveWindowMask,
  FocusChangeMask,
  StructureNotifyMask,
  PropertyChangeMask,
  VisibilityChangeMask,
  0,                    /* PROXIMITY_IN */
  0,                    /* PROXIMTY_OUT */
  SubstructureNotifyMask,
  ButtonPressMask      /* SCROLL; on X mouse wheel events is treated as mouse button 4/5 */
};

typedef struct {
  GdkX11Surface parent_instance;
} GdkX11Toplevel;

typedef struct {
  GdkX11SurfaceClass parent_class;
} GdkX11ToplevelClass;

const int _gdk_x11_event_mask_table_size = G_N_ELEMENTS (_gdk_x11_event_mask_table);

/* Forward declarations */
static void     gdk_x11_surface_apply_fullscreen_mode (GdkSurface  *surface);
static gboolean gdk_surface_icon_name_set          (GdkSurface  *surface);
static void     set_wm_name                       (GdkDisplay  *display,
						   Window       xwindow,
						   const char *name);
static void     move_to_current_desktop           (GdkSurface *surface);
static void     gdk_x11_toplevel_state_callback   (GdkSurface *surface);
static gboolean gdk_x11_toplevel_event_callback   (GdkSurface *surface,
                                                   GdkEvent   *gdk_event);

static void     gdk_x11_surface_toplevel_resize   (GdkSurface *surface,
                                                   int         width,
                                                   int         height);

static void gdk_x11_surface_set_geometry_hints (GdkSurface        *surface,
                                                const GdkGeometry *geometry,
                                                GdkSurfaceHints    geom_mask);

/* Return whether time1 is considered later than time2 as far as xserver
 * time is concerned.  Accounts for wraparound.
 */
#define XSERVER_TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

G_DEFINE_TYPE (GdkX11Surface, gdk_x11_surface, GDK_TYPE_SURFACE)

static void
gdk_x11_surface_init (GdkX11Surface *impl)
{  
  impl->surface_scale = 1;
  impl->frame_sync_enabled = TRUE;
  impl->surface_is_on_monitor = NULL;
}

GdkToplevelX11 *
_gdk_x11_surface_get_toplevel (GdkSurface *surface)
{
  GdkX11Surface *impl;
  
  g_assert (GDK_IS_SURFACE (surface));

  impl = GDK_X11_SURFACE (surface);

  if (!impl->toplevel)
    {
      impl->toplevel = g_new0 (GdkToplevelX11, 1);
      impl->toplevel->have_focused = FALSE;
      g_signal_connect (surface, "notify::state",
                        G_CALLBACK (gdk_x11_toplevel_state_callback),
                        NULL);
      g_signal_connect (surface, "event",
                        G_CALLBACK (gdk_x11_toplevel_event_callback),
                        NULL);
    }

  return impl->toplevel;
}

/*
 * gdk_x11_surface_update_size:
 * @self: a `GdkX11Surface`
 * @width: the new width of the surface
 * @height: the new height of the surface
 * @scale: the new scale of the surface
 *
 * Updates the state of the surface (in particular the drawable's
 * cairo surface) when its size has changed.
 *
 * Returns: %TRUE if the surface was updated, %FALSE if no updates
 *   where necessary
 */
static gboolean
gdk_x11_surface_update_size (GdkX11Surface *self,
                             int            width,
                             int            height,
                             int            scale)
{
  GdkSurface *surface = GDK_SURFACE (self);

  if (surface->width == width &&
      surface->height == height &&
      self->surface_scale == scale)
    return FALSE;

  surface->width = width;
  surface->height = height;
  self->surface_scale = scale;

  _gdk_surface_update_size (surface);

  if (self->cairo_surface)
    {
      cairo_xlib_surface_set_size (self->cairo_surface,
                                   self->unscaled_width, self->unscaled_height);
      cairo_surface_set_device_scale (self->cairo_surface, scale, scale);
    }

  gdk_surface_invalidate_rect (surface, NULL);

  return TRUE;
}

static void
update_shadow_size (GdkSurface *surface,
                    int         shadow_left,
                    int         shadow_right,
                    int         shadow_top,
                    int         shadow_bottom)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  Atom frame_extents;
  gulong data[4];

  if (impl->shadow_left == shadow_left &&
      impl->shadow_right == shadow_right &&
      impl->shadow_top == shadow_top &&
      impl->shadow_bottom == shadow_bottom)
    return;

  impl->shadow_left = shadow_left;
  impl->shadow_right = shadow_right;
  impl->shadow_top = shadow_top;
  impl->shadow_bottom = shadow_bottom;

  data[0] = shadow_left * impl->surface_scale;
  data[1] = shadow_right * impl->surface_scale;
  data[2] = shadow_top * impl->surface_scale;
  data[3] = shadow_bottom * impl->surface_scale;

  frame_extents = gdk_x11_get_xatom_by_name_for_display (gdk_surface_get_display (surface),
                                                         "_GTK_FRAME_EXTENTS");
  XChangeProperty (GDK_SURFACE_XDISPLAY (surface),
                   GDK_SURFACE_XID (surface),
                   frame_extents, XA_CARDINAL,
                   32, PropModeReplace,
                   (guchar *) &data, 4);
}

#define UPDATE_GEOMETRY TRUE
#define DONT_UPDATE_GEOMETRY FALSE

static gboolean
compute_toplevel_size (GdkSurface *surface,
                       gboolean    update_geometry,
                       int        *width,
                       int        *height)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;
  GdkToplevelSize size;
  int bounds_width, bounds_height;

  monitor = gdk_display_get_monitor_at_surface (display, surface);
  if (monitor)
    {
      GdkRectangle workarea;

      gdk_x11_monitor_get_workarea (monitor, &workarea);
      bounds_width = workarea.width;
      bounds_height = workarea.height;
    }
  else
    {
      bounds_width = G_MAXINT;
      bounds_height = G_MAXINT;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (GDK_TOPLEVEL (surface), &size);

  if (size.shadow.is_valid)
    {
      update_shadow_size (surface,
                          size.shadow.left,
                          size.shadow.right,
                          size.shadow.top,
                          size.shadow.bottom);
    }

  if (update_geometry)
    {
      GdkGeometry geometry;
      GdkSurfaceHints mask;

      if (!impl->toplevel_layout || gdk_toplevel_layout_get_resizable (impl->toplevel_layout))
        {
          geometry.min_width = size.min_width;
          geometry.min_height = size.min_height;
          mask = GDK_HINT_MIN_SIZE;
        }
      else
        {
          geometry.max_width = geometry.min_width = size.width;
          geometry.max_height = geometry.min_height = size.height;
          mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
        }
      gdk_x11_surface_set_geometry_hints (surface, &geometry, mask);
    }

  if (!(surface->state & (GDK_TOPLEVEL_STATE_FULLSCREEN |
                          GDK_TOPLEVEL_STATE_MAXIMIZED |
                          GDK_TOPLEVEL_STATE_TILED |
                          GDK_TOPLEVEL_STATE_TOP_TILED |
                          GDK_TOPLEVEL_STATE_RIGHT_TILED |
                          GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                          GDK_TOPLEVEL_STATE_LEFT_TILED |
                          GDK_TOPLEVEL_STATE_MINIMIZED)) &&
      (!impl->next_layout.configure_pending || surface->resize_count > 0))
    {
      GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
      GdkGeometry geometry;
      GdkSurfaceHints mask;

      geometry = toplevel->last_geometry_hints;
      mask = toplevel->last_geometry_hints_mask;
      gdk_surface_constrain_size (&geometry, mask,
                                  size.width, size.height,
                                  &size.width, &size.height);
      if ((impl->last_computed_width != size.width ||
           impl->last_computed_height != size.height) &&
          (impl->next_layout.configured_width != size.width ||
           impl->next_layout.configured_height != size.height))
        {
          *width = size.width;
          *height = size.height;
          impl->last_computed_width = size.width;
          impl->last_computed_height = size.height;

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
compute_drag_surface_size (GdkSurface *surface,
                           int        *width,
                           int        *height)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkDragSurfaceSize size;

  gdk_drag_surface_size_init (&size);
  size.width = impl->next_layout.configured_width;
  size.height = impl->next_layout.configured_height;

  gdk_drag_surface_notify_compute_size (GDK_DRAG_SURFACE (surface), &size);

  if ((impl->last_computed_width != size.width ||
        impl->last_computed_height != size.height) &&
      (impl->next_layout.configured_width != size.width ||
        impl->next_layout.configured_height != size.height))
    {
      *width = size.width;
      *height = size.height;
      impl->last_computed_width = size.width;
      impl->last_computed_height = size.height;

      return TRUE;
    }

  return FALSE;
}

static gboolean
compute_size_idle (gpointer user_data)
{
  GdkSurface *surface = user_data;
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  int width, height;

  impl->compute_size_source_id = 0;
  if (compute_toplevel_size (surface, UPDATE_GEOMETRY, &width, &height))
    gdk_x11_surface_toplevel_resize (surface, width, height);

  return G_SOURCE_REMOVE;
}

static void
gdk_x11_surface_request_layout (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (!impl->compute_size_source_id &&
      GDK_IS_TOPLEVEL (surface))
    {
      impl->compute_size_source_id = g_idle_add_full (G_PRIORITY_HIGH - 10,
                                                      compute_size_idle,
                                                      surface,
                                                      NULL);
    }
}

static gboolean
gdk_x11_surface_compute_size (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (GDK_IS_TOPLEVEL (surface))
    {
      int width, height;

      if (compute_toplevel_size (surface, UPDATE_GEOMETRY, &width, &height))
        gdk_x11_surface_toplevel_resize (surface, width, height);

      if (surface->resize_count == 0)
        {
          gdk_x11_surface_update_size (impl,
                                       impl->next_layout.configured_width,
                                       impl->next_layout.configured_height,
                                       impl->surface_scale);
        }

      impl->next_layout.surface_geometry_dirty = FALSE;
      impl->next_layout.configure_pending = FALSE;
    }
  else if (GDK_IS_DRAG_SURFACE (surface))
    {
      int width, height;

      if (compute_drag_surface_size (surface, &width, &height))
        gdk_x11_surface_toplevel_resize (surface, width, height);

      if (surface->resize_count == 0)
        {
          gdk_x11_surface_update_size (impl,
                                       impl->next_layout.configured_width,
                                       impl->next_layout.configured_height,
                                       impl->surface_scale);
        }

      impl->next_layout.surface_geometry_dirty = FALSE;
      impl->next_layout.configure_pending = FALSE;
    }
  else
    {
      gdk_x11_surface_update_size (impl,
                                   impl->next_layout.configured_width,
                                   impl->next_layout.configured_height,
                                   impl->surface_scale);

      impl->next_layout.surface_geometry_dirty = FALSE;
    }

  return surface->resize_count > 0;
}

gboolean
gdk_x11_surface_supports_edge_constraints (GdkSurface *surface)
{
  return gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                              g_intern_static_string ("_GTK_EDGE_CONSTRAINTS"));
}

static void
set_sync_counter(Display     *display,
		 XSyncCounter counter,
                 gint64       value)
{
    XSyncValue sync_value;

    XSyncIntsToValue (&sync_value,
                      value & G_GINT64_CONSTANT(0xFFFFFFFF),
                      value >> 32);
    XSyncSetCounter (display, counter, sync_value);
}

void
gdk_x11_surface_pre_damage (GdkSurface *surface)
{
  GdkX11Surface *impl;

  impl = GDK_X11_SURFACE (surface);

  if (impl->toplevel->in_frame &&
      impl->toplevel->current_counter_value % 2 == 0)
    {
      impl->toplevel->current_counter_value += 1;
      set_sync_counter (GDK_SURFACE_XDISPLAY (surface),
		        impl->toplevel->extended_update_counter,
                        impl->toplevel->current_counter_value);
    }
}

static void
on_surface_changed (void *data)
{
  GdkSurface *surface = data;
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (impl->tracking_damage)
    gdk_x11_surface_pre_damage (surface);
}

/* We want to know when cairo drawing causes damage to the window,
 * so we engage in the _NET_WM_FRAME_DRAWN protocol with the
 * window only when there actually is drawing. To do that we use
 * a technique (hack) suggested by Uli Schlachter - if we set
 * a dummy "mime data" on the cairo surface (this facility is
 * used to attach JPEG data to an imager), then cairo will flush
 * and remove the mime data before making any changes to the window.
 */

static void
hook_surface_changed (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (impl->cairo_surface)
    {
      cairo_surface_set_mime_data (impl->cairo_surface,
                                   "x-gdk/change-notify",
                                   (unsigned char *)"X",
                                   1,
                                   on_surface_changed,
                                   surface);
      impl->tracking_damage = 1;
    }
}

static void
unhook_surface_changed (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (impl->cairo_surface)
    {
      impl->tracking_damage = 0;
      cairo_surface_set_mime_data (impl->cairo_surface,
                                   "x-gdk/change-notify",
                                   NULL, 0,
                                   NULL, NULL);
    }
}

static void
gdk_x11_surface_predict_presentation_time (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkFrameClock *clock;
  GdkFrameTimings *timings;
  gint64 presentation_time;
  gint64 refresh_interval;

  clock = gdk_surface_get_frame_clock (surface);

  timings = gdk_frame_clock_get_current_timings (clock);

  gdk_frame_clock_get_refresh_info (clock,
                                    timings->frame_time,
                                    &refresh_interval, &presentation_time);

  if (presentation_time != 0)
    {
      if (timings->slept_before)
        {
          presentation_time += refresh_interval;
        }
      else
        {
          if (presentation_time < timings->frame_time + refresh_interval / 2)
            presentation_time += refresh_interval;
        }
    }
  else
    {
      if (timings->slept_before)
        presentation_time = timings->frame_time + refresh_interval + refresh_interval / 2;
      else
        presentation_time = timings->frame_time + refresh_interval;
    }

  if (presentation_time < impl->toplevel->throttled_presentation_time)
    presentation_time = impl->toplevel->throttled_presentation_time;

  timings->predicted_presentation_time = presentation_time;
}

static void
gdk_x11_surface_begin_frame (GdkSurface *surface,
                            gboolean   force_frame)
{
  GdkX11Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_X11_SURFACE (surface);

  if (impl->toplevel->extended_update_counter == None)
    return;

  impl->toplevel->in_frame = TRUE;

  if (impl->toplevel->configure_counter_value != 0 &&
      impl->toplevel->configure_counter_value_is_extended)
    {
      impl->toplevel->current_counter_value = impl->toplevel->configure_counter_value;
      if ((impl->toplevel->current_counter_value % 2) == 1)
        impl->toplevel->current_counter_value += 1;

      impl->toplevel->configure_counter_value = 0;

      gdk_x11_surface_pre_damage (surface);
    }
  else if (force_frame)
    {
      /* When mapping the surface, we really want to freeze the
         rendering of the surface by the compositor until we've
         actually painted something into the surface's buffer. */
      gdk_x11_surface_pre_damage (surface);
    }
  else
    {
      hook_surface_changed (surface);
    }
}

gboolean
_gdk_x11_surface_syncs_frames (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  /* disabled client side */
  if (!impl->frame_sync_enabled)
    return FALSE;

  /* disabled compositor side */
  if (!gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                            g_intern_static_string ("_NET_WM_FRAME_DRAWN")))
    return FALSE;

  return TRUE;
}

static void
sync_counter_for_end_frame (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  g_assert (!impl->toplevel->in_frame);
  g_assert (impl->toplevel->extended_update_counter != None);
  g_assert ((impl->toplevel->current_counter_value % 2) == 0);

  set_sync_counter (GDK_SURFACE_XDISPLAY (surface),
                    impl->toplevel->extended_update_counter,
                    impl->toplevel->current_counter_value);
}

static void
maybe_sync_counter_for_end_frame (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  gboolean frame_sync_negotiated = _gdk_x11_surface_syncs_frames (surface);
  gboolean frame_done_painting;

#ifdef HAVE_XDAMAGE
  frame_done_painting = !impl->toplevel->frame_still_painting && frame_sync_negotiated;
#else
  frame_done_painting = !impl->toplevel->frame_pending;
#endif

  if (!impl->toplevel->frame_pending)
    {
      if (!frame_sync_negotiated || frame_done_painting)
        sync_counter_for_end_frame (surface);
    }
  else
    {
      if (frame_done_painting)
        sync_counter_for_end_frame (surface);
    }
}

#ifdef HAVE_XDAMAGE
void
_gdk_x11_surface_set_frame_still_painting (GdkSurface *surface,
                                           gboolean    painting)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (impl->toplevel->frame_still_painting == painting)
    return;

  impl->toplevel->frame_still_painting = painting;

  if (!impl->toplevel->frame_still_painting)
    maybe_sync_counter_for_end_frame (surface);
}
#endif

static void
gdk_x11_surface_end_frame (GdkSurface *surface)
{
  GdkFrameClock *clock;
  GdkFrameTimings *timings;
  GdkX11Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_X11_SURFACE (surface);

  if (impl->toplevel->extended_update_counter == None ||
      !impl->toplevel->in_frame)
    return;

  clock = gdk_surface_get_frame_clock (surface);
  timings = gdk_frame_clock_get_current_timings (clock);

  /* Make sure we request timing updates even if nothing was damaged.
   * We want the frame clock to be accurate. */
  gdk_x11_surface_pre_damage (surface);

  impl->toplevel->in_frame = FALSE;

  if (impl->toplevel->current_counter_value % 2 == 1)
    {
      if (GDK_DISPLAY_DEBUG_CHECK (gdk_surface_get_display (surface), FRAMES))
        {
          XImage *image = XGetImage (GDK_SURFACE_XDISPLAY (surface),
                                     GDK_SURFACE_XID (surface),
                                     0, 0, 1, 1,
                                     (1 << 24) - 1,
                                     ZPixmap);
          XDestroyImage (image);
        }

      /* An increment of 3 means that the frame was not drawn as fast as possible,
       * but rather at a particular time. This can trigger different handling from
       * the compositor.
       */
      if (timings->slept_before)
        impl->toplevel->current_counter_value += 3;
      else
        impl->toplevel->current_counter_value += 1;

      maybe_sync_counter_for_end_frame (surface);

      if (_gdk_x11_surface_syncs_frames (surface))
        {
          impl->toplevel->frame_pending = TRUE;
          gdk_surface_freeze_updates (surface);
          timings->cookie = impl->toplevel->current_counter_value;
        }
    }

  unhook_surface_changed (surface);

  if (impl->toplevel->configure_counter_value != 0 &&
      !impl->toplevel->configure_counter_value_is_extended)
    {
      set_sync_counter (GDK_SURFACE_XDISPLAY (surface),
                        impl->toplevel->update_counter,
                        impl->toplevel->configure_counter_value);

      impl->toplevel->configure_counter_value = 0;
    }

  if (!impl->toplevel->frame_pending)
    timings->complete = TRUE;
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static const char *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();
  if (!title)
    title = "";

  return title;
}

static void
set_wm_protocols (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  Atom protocols[4];
  int n = 0;
  
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW");
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS");
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PING");

#ifdef HAVE_XSYNC
  if (GDK_X11_DISPLAY (display)->use_sync)
    protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_SYNC_REQUEST");
#endif
  
  XSetWMProtocols (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface), protocols, n);
}

static void
check_leader_window_title (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->leader_window && !display_x11->leader_window_title_set)
    {
      set_wm_name (display,
		   display_x11->leader_window,
		   get_default_title ());
      
      display_x11->leader_window_title_set = TRUE;
    }
}

static Window
create_focus_window (GdkDisplay *display,
		     XID         parent)
{
  GdkX11Display *display_x11;
  GdkEventMask event_mask;
  Display *xdisplay;
  Window focus_window;
  XSetWindowAttributes attrs;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  display_x11 = GDK_X11_DISPLAY (display);

  focus_window = XCreateWindow (xdisplay, parent,
                                -1, -1, 1, 1, 0,
                                0, /* depth */
                                InputOnly,
                                CopyFromParent,
                                0, &attrs);

  event_mask = (GDK_KEY_PRESS_MASK |
                GDK_KEY_RELEASE_MASK |
                GDK_FOCUS_CHANGE_MASK);

  gdk_x11_event_source_select_events ((GdkEventSource *) display_x11->event_source,
                                      focus_window,
                                      event_mask, 0);

  XMapWindow (xdisplay, focus_window);

  return focus_window;
}

static void
ensure_sync_counter (GdkSurface *surface)
{
#ifdef HAVE_XSYNC
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);
      GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);

      if (toplevel &&
	  toplevel->update_counter == None &&
	  GDK_X11_DISPLAY (display)->use_sync)
	{
	  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
	  XSyncValue value;
	  Atom atom;
	  XID counters[2];

	  XSyncIntToValue (&value, 0);
	  
	  toplevel->update_counter = XSyncCreateCounter (xdisplay, value);
	  toplevel->extended_update_counter = XSyncCreateCounter (xdisplay, value);
	  
	  atom = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_SYNC_REQUEST_COUNTER");

	  counters[0] = toplevel->update_counter;
	  counters[1] = toplevel->extended_update_counter;
	  XChangeProperty (xdisplay, GDK_SURFACE_XID (surface),
			   atom, XA_CARDINAL,
			   32, PropModeReplace,
			   (guchar *)counters, 2);
	  
	  toplevel->current_counter_value = 0;
	}
    }
#endif
}

static void
setup_toplevel_window (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  Display *xdisplay = GDK_SURFACE_XDISPLAY (surface);
  XID xid = GDK_SURFACE_XID (surface);
  XSizeHints size_hints;
  Window leader_window;

  set_wm_protocols (surface);

  /* The focus surface is off the visible area, and serves to receive key
   * press events so they don't get sent to child surfaces.
   */
  toplevel->focus_window = create_focus_window (display, xid);
  _gdk_x11_display_add_window (display,
                               &toplevel->focus_window,
                               surface);

  check_leader_window_title (display);

  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  size_hints.flags = PSize;
  size_hints.width = surface->width * impl->surface_scale;
  size_hints.height = surface->height * impl->surface_scale;
  
  XSetWMNormalHints (xdisplay, xid, &size_hints);
  
  /* This will set WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (xdisplay, xid, NULL, NULL, NULL, 0, NULL, NULL, NULL);
  
  if (!gdk_running_in_sandbox ())
    {
      /* if sandboxed, we're likely in a pid namespace and would only confuse the wm with this */
      long pid = getpid ();
      XChangeProperty (xdisplay, xid,
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PID"),
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (guchar *)&pid, 1);
    }

  leader_window = GDK_X11_DISPLAY (display)->leader_window;
  if (!leader_window)
    leader_window = xid;
  XChangeProperty (xdisplay, xid, 
		   gdk_x11_get_xatom_by_name_for_display (display, "WM_CLIENT_LEADER"),
		   XA_WINDOW, 32, PropModeReplace,
		   (guchar *) &leader_window, 1);

  if (toplevel->focus_window != None)
    XChangeProperty (xdisplay, xid, 
                     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_USER_TIME_WINDOW"),
                     XA_WINDOW, 32, PropModeReplace,
                     (guchar *) &toplevel->focus_window, 1);

  if (GDK_X11_DISPLAY (display)->user_time != 0)
    gdk_x11_surface_set_user_time (surface, GDK_X11_DISPLAY (display)->user_time);

  ensure_sync_counter (surface);

  /* Start off in a frozen state - we'll finish this when we first paint */
  gdk_x11_surface_begin_frame (surface, TRUE);
}

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkSurface     *surface)
{
  if (surface->update_freeze_count > 0)
    return;

  gdk_x11_surface_predict_presentation_time (surface);
  gdk_x11_surface_begin_frame (surface, FALSE);
}

static void
on_frame_clock_after_update (GdkFrameClock *clock,
                             GdkSurface    *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (impl->compute_size_source_id)
    {
      g_clear_handle_id (&impl->compute_size_source_id, g_source_remove);
      compute_size_idle (surface);
    }
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface     *surface)
{
  if (surface->update_freeze_count > 0)
    return;

  gdk_x11_surface_end_frame (surface);
}

static void
connect_frame_clock (GdkSurface *surface)
{
  GdkX11Surface *impl;

  impl = GDK_X11_SURFACE (surface);
  if (!impl->frame_clock_connected)
    {
      GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);

      g_signal_connect (frame_clock, "before-paint",
                        G_CALLBACK (on_frame_clock_before_paint), surface);
      g_signal_connect_after (frame_clock, "update",
                              G_CALLBACK (on_frame_clock_after_update), surface);
      g_signal_connect (frame_clock, "after-paint",
                        G_CALLBACK (on_frame_clock_after_paint), surface);

      impl->frame_clock_connected = TRUE;
    }
}

static void
disconnect_frame_clock (GdkSurface *surface)
{
  GdkX11Surface *impl;

  impl = GDK_X11_SURFACE (surface);
  if (impl->frame_clock_connected)
    {
      GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);

      g_signal_handlers_disconnect_by_func (frame_clock,
                                            on_frame_clock_before_paint, surface);
      g_signal_handlers_disconnect_by_func (frame_clock,
                                            on_frame_clock_after_update, surface);
      g_signal_handlers_disconnect_by_func (frame_clock,
                                            on_frame_clock_after_paint, surface);

      impl->frame_clock_connected = FALSE;
    }
}

typedef enum
{
  GDK_SURFACE_TYPE_HINT_NORMAL,
  GDK_SURFACE_TYPE_HINT_DIALOG,
  GDK_SURFACE_TYPE_HINT_MENU,           /* Torn off menu */
  GDK_SURFACE_TYPE_HINT_TOOLBAR,
  GDK_SURFACE_TYPE_HINT_SPLASHSCREEN,
  GDK_SURFACE_TYPE_HINT_UTILITY,
  GDK_SURFACE_TYPE_HINT_DOCK,
  GDK_SURFACE_TYPE_HINT_DESKTOP,
  GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU,  /* A drop down menu (from a menubar) */
  GDK_SURFACE_TYPE_HINT_POPUP_MENU,     /* A popup menu (from right-click) */
  GDK_SURFACE_TYPE_HINT_TOOLTIP,
  GDK_SURFACE_TYPE_HINT_NOTIFICATION,
  GDK_SURFACE_TYPE_HINT_COMBO,
  GDK_SURFACE_TYPE_HINT_DND
} GdkSurfaceTypeHint;

static void gdk_x11_surface_set_title (GdkSurface *surface,
                                       const char *title);
static void
gdk_x11_surface_constructed (GObject *object)
{
  GdkX11Surface *self = GDK_X11_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (self);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  XClassHint *class_hint;

  g_assert (self->xid);

  g_object_ref (surface);
  _gdk_x11_display_add_window (display, &self->xid, surface);

  self->surface_scale = display_x11->screen->surface_scale;

  gdk_surface_set_egl_native_window (surface, (void *) self->xid);

  gdk_x11_surface_set_title (surface, get_default_title ());

  class_hint = XAllocClassHint ();
  class_hint->res_name = (char *) g_get_prgname ();
  if (display_x11->program_class)
    class_hint->res_class = (char *) display_x11->program_class;
  else
    class_hint->res_class = class_hint->res_name;
  XSetClassHint (GDK_DISPLAY_XDISPLAY (display), self->xid, class_hint);
  XFree (class_hint);

  setup_toplevel_window (surface);

  gdk_x11_event_source_select_events ((GdkEventSource *) display_x11->event_source,
                                      GDK_SURFACE_XID (surface), GDK_ALL_EVENTS_MASK,
                                      StructureNotifyMask | PropertyChangeMask);

  _gdk_x11_surface_register_dnd (surface);

  connect_frame_clock (surface);

  gdk_surface_freeze_updates (surface);

  G_OBJECT_CLASS (gdk_x11_surface_parent_class)->constructed (object);
}

static void
gdk_x11_surface_finalize (GObject *object)
{
  GdkX11Surface *impl;

  g_return_if_fail (GDK_IS_X11_SURFACE (object));

  impl = GDK_X11_SURFACE (object);

  if (impl->toplevel->in_frame)
    unhook_surface_changed (GDK_SURFACE (impl));

  g_signal_handlers_disconnect_by_func (GDK_SURFACE (impl),
                                        gdk_x11_toplevel_state_callback,
                                        NULL);
  g_signal_handlers_disconnect_by_func (GDK_SURFACE (impl),
                                        gdk_x11_toplevel_event_callback,
                                        NULL);

  _gdk_x11_surface_grab_check_destroy (GDK_SURFACE (impl));

  if (!GDK_SURFACE_DESTROYED (impl))
    {
      GdkDisplay *display = GDK_SURFACE_DISPLAY (GDK_SURFACE (impl));

      _gdk_x11_display_remove_window (display, impl->xid);
      if (impl->toplevel && impl->toplevel->focus_window)
        _gdk_x11_display_remove_window (display, impl->toplevel->focus_window);
    }

  g_clear_pointer (&impl->surface_is_on_monitor, g_list_free);
  g_clear_handle_id (&impl->compute_size_source_id, g_source_remove);
  g_clear_pointer (&impl->toplevel_layout, gdk_toplevel_layout_unref);

  g_free (impl->toplevel);

  if (impl->cursor)
    g_object_unref (impl->cursor);

  G_OBJECT_CLASS (gdk_x11_surface_parent_class)->finalize (object);
}

typedef struct {
  GdkDisplay *display;
  Pixmap pixmap;
} FreePixmapData;

static void
free_pixmap (gpointer datap)
{
  FreePixmapData *data = datap;

  if (!gdk_display_is_closed (data->display))
    {
      XFreePixmap (GDK_DISPLAY_XDISPLAY (data->display),
                   data->pixmap);
    }

  g_object_unref (data->display);
  g_free (data);
}

static void
attach_free_pixmap_handler (cairo_surface_t *surface,
                            GdkDisplay      *display,
                            Pixmap           pixmap)
{
  static const cairo_user_data_key_t key;
  FreePixmapData *data;
  
  data = g_new (FreePixmapData, 1);
  data->display = g_object_ref (display);
  data->pixmap = pixmap;

  cairo_surface_set_user_data (surface, &key, data, free_pixmap);
}

/* Cairo does not guarantee we get an xlib surface if we call
 * cairo_surface_create_similar(). In some cases however, we must use a
 * pixmap or bitmap in the X11 API.
 * These functions ensure an Xlib surface.
 */
cairo_surface_t *
_gdk_x11_display_create_bitmap_surface (GdkDisplay *display,
                                        int         width,
                                        int         height)
{
  cairo_surface_t *surface;
  Pixmap pixmap;

  pixmap = XCreatePixmap (GDK_DISPLAY_XDISPLAY (display),
                          GDK_SCREEN_XROOTWIN (GDK_X11_DISPLAY (display)->screen),
                          width, height, 1);
  surface = cairo_xlib_surface_create_for_bitmap (GDK_DISPLAY_XDISPLAY (display),
                                                  pixmap,
                                                  GDK_X11_SCREEN (GDK_X11_DISPLAY (display)->screen)->xscreen,
                                                  width, height);
  attach_free_pixmap_handler (surface, display, pixmap);

  return surface;
}

/* Create a surface backed with a pixmap without alpha on the same screen as surface */
static cairo_surface_t *
gdk_x11_surface_create_pixmap_surface (GdkSurface *surface,
                                      int        width,
                                      int        height)
{
  GdkDisplay *display;
  Display *dpy;
  cairo_surface_t *cairo_surface;
  Pixmap pixmap;

  display = gdk_surface_get_display (surface);
  dpy = GDK_DISPLAY_XDISPLAY (display);

  pixmap = XCreatePixmap (dpy,
                          GDK_SURFACE_XID (surface),
                          width, height,
                          DefaultDepth (dpy, DefaultScreen (dpy)));
  cairo_surface = cairo_xlib_surface_create (dpy,
                                       pixmap,
                                       DefaultVisual (dpy, DefaultScreen (dpy)),
                                       width, height);
  attach_free_pixmap_handler (cairo_surface, display, pixmap);

  return cairo_surface;
}

static void gdk_x11_surface_set_type_hint (GdkSurface        *surface,
                                           GdkSurfaceTypeHint hint);

static void
gdk_toplevel_x11_free_contents (GdkDisplay *display,
				GdkToplevelX11 *toplevel)
{
  if (toplevel->icon_pixmap)
    {
      cairo_surface_destroy (toplevel->icon_pixmap);
      toplevel->icon_pixmap = NULL;
    }
  if (toplevel->icon_mask)
    {
      cairo_surface_destroy (toplevel->icon_mask);
      toplevel->icon_mask = NULL;
    }
  if (toplevel->group_leader)
    {
      g_object_unref (toplevel->group_leader);
      toplevel->group_leader = NULL;
    }
#ifdef HAVE_XSYNC
  if (toplevel->update_counter != None)
    {
      XSyncDestroyCounter (GDK_DISPLAY_XDISPLAY (display), 
			   toplevel->update_counter);
      XSyncDestroyCounter (GDK_DISPLAY_XDISPLAY (display),
                           toplevel->extended_update_counter);
      toplevel->update_counter = None;
      toplevel->extended_update_counter = None;

      toplevel->current_counter_value = 0;
    }
#endif
}

static void
gdk_x11_surface_destroy (GdkSurface *surface,
                         gboolean    foreign_destroy)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkToplevelX11 *toplevel;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  toplevel = _gdk_x11_surface_get_toplevel (surface);
  if (toplevel)
    gdk_toplevel_x11_free_contents (GDK_SURFACE_DISPLAY (surface), toplevel);

  unhook_surface_changed (surface);
  disconnect_frame_clock (surface);
  g_clear_handle_id (&impl->compute_size_source_id, g_source_remove);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_destroy (impl->cairo_surface);
      impl->cairo_surface = NULL;
    }

  if (!foreign_destroy)
    {
      gdk_surface_set_egl_native_window (surface, NULL);
      gdk_x11_surface_destroy_glx_drawable (impl);

      XDestroyWindow (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XID (surface));
    }
}

/* This function is called when the XWindow is really gone.
 */
static void
gdk_x11_surface_destroy_notify (GdkSurface *surface)
{
  GdkX11Surface *surface_impl;

  surface_impl = GDK_X11_SURFACE (surface);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("GdkSurface %#lx unexpectedly destroyed", GDK_SURFACE_XID (surface));

      _gdk_surface_destroy (surface, TRUE);
    }

  _gdk_x11_display_remove_window (GDK_SURFACE_DISPLAY (surface), GDK_SURFACE_XID (surface));
  if (surface_impl->toplevel && surface_impl->toplevel->focus_window)
    _gdk_x11_display_remove_window (GDK_SURFACE_DISPLAY (surface), surface_impl->toplevel->focus_window);

  _gdk_x11_surface_grab_check_destroy (surface);

  g_object_unref (surface);
}

static void
update_wm_hints (GdkSurface *surface,
		 gboolean   force)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);
  XWMHints wm_hints;

  if (!force &&
      !toplevel->is_leader &&
      !GDK_SURFACE_IS_MAPPED (surface))
    return;

  wm_hints.flags = StateHint | InputHint;
  wm_hints.input = True;
  wm_hints.initial_state = NormalState;
  
  if (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED)
    {
      wm_hints.flags |= StateHint;
      wm_hints.initial_state = IconicState;
    }

  if (toplevel->icon_pixmap)
    {
      wm_hints.flags |= IconPixmapHint;
      wm_hints.icon_pixmap = cairo_xlib_surface_get_drawable (toplevel->icon_pixmap);
    }

  if (toplevel->icon_mask)
    {
      wm_hints.flags |= IconMaskHint;
      wm_hints.icon_mask = cairo_xlib_surface_get_drawable (toplevel->icon_mask);
    }
  
  wm_hints.flags |= WindowGroupHint;
  if (toplevel->group_leader && !GDK_SURFACE_DESTROYED (toplevel->group_leader))
    {
      wm_hints.flags |= WindowGroupHint;
      wm_hints.window_group = GDK_SURFACE_XID (toplevel->group_leader);
    }
  else
    wm_hints.window_group = GDK_X11_DISPLAY (display)->leader_window;

  if (toplevel->urgency_hint)
    wm_hints.flags |= XUrgencyHint;
  
  XSetWMHints (GDK_SURFACE_XDISPLAY (surface),
	       GDK_SURFACE_XID (surface),
	       &wm_hints);
}

static void
set_initial_hints (GdkSurface *surface)
{
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  Window xwindow = GDK_SURFACE_XID (surface);  
  GdkToplevelX11 *toplevel;
  Atom atoms[9];
  int i;

  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (!toplevel)
    return;

  update_wm_hints (surface, TRUE);
  
  /* We set the spec hints regardless of whether the spec is supported,
   * since it can't hurt and it's kind of expensive to check whether
   * it's supported.
   */
  
  i = 0;

  if (surface->state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MAXIMIZED_VERT");
      ++i;
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MAXIMIZED_HORZ");
      ++i;
      toplevel->have_maxhorz = toplevel->have_maxvert = TRUE;
    }

  if (surface->state & GDK_TOPLEVEL_STATE_ABOVE)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_ABOVE");
      ++i;
    }
  
  if (surface->state & GDK_TOPLEVEL_STATE_BELOW)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_BELOW");
      ++i;
    }
  
  if (surface->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_FULLSCREEN");
      ++i;
      toplevel->have_fullscreen = TRUE;
    }

  if (surface->modal_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MODAL");
      ++i;
    }

  if (toplevel->skip_taskbar_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_SKIP_TASKBAR");
      ++i;
    }

  if (toplevel->skip_pager_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_SKIP_PAGER");
      ++i;
    }

  if (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_HIDDEN");
      ++i;
      toplevel->have_hidden = TRUE;
    }

  if (i > 0)
    {
      XChangeProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
                       XA_ATOM, 32, PropModeReplace,
                       (guchar*) atoms, i);
    }
  else 
    {
      XDeleteProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"));
    }

  if (surface->state & GDK_TOPLEVEL_STATE_STICKY)
    {
      atoms[0] = 0xFFFFFFFF;
      XChangeProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"),
                       XA_CARDINAL, 32, PropModeReplace,
                       (guchar*) atoms, 1);
      toplevel->on_all_desktops = TRUE;
    }
  else
    {
      XDeleteProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"));
    }

  toplevel->map_serial = NextRequest (xdisplay);
}

void
gdk_x11_surface_show (GdkSurface *surface, gboolean already_mapped)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkToplevelX11 *toplevel;
  Display *xdisplay = GDK_SURFACE_XDISPLAY (surface);
  Window xwindow = GDK_SURFACE_XID (surface);

  if (!already_mapped)
    set_initial_hints (surface);
      
  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);
  toplevel = _gdk_x11_surface_get_toplevel (surface);
      
  if (toplevel->user_time != 0 &&
      display_x11->user_time != 0 &&
      XSERVER_TIME_IS_LATER (display_x11->user_time, toplevel->user_time))
    gdk_x11_surface_set_user_time (surface, display_x11->user_time);

 if (GDK_PROFILER_IS_RUNNING)
   {
     GdkX11Surface *impl = GDK_X11_SURFACE (surface);
     if (impl->map_time == 0)
       impl->map_time = g_get_monotonic_time ();
   }

  XMapWindow (xdisplay, xwindow);
  
  /* Fullscreen on current monitor is the default, no need to apply this mode
   * when mapping a window. This also ensures that the default behavior remains
   * consistent with pre-fullscreen mode implementation.
   */
  if (surface->fullscreen_mode != GDK_FULLSCREEN_ON_CURRENT_MONITOR)
    gdk_x11_surface_apply_fullscreen_mode (surface);
}

static void
gdk_x11_surface_withdraw (GdkSurface *surface)
{
  if (!surface->destroyed)
    {
      if (GDK_SURFACE_IS_MAPPED (surface))
        gdk_surface_set_is_mapped (surface, FALSE);

      g_assert (!GDK_SURFACE_IS_MAPPED (surface));
      XWithdrawWindow (GDK_SURFACE_XDISPLAY (surface),
                       GDK_SURFACE_XID (surface), 0);
    }
}

static void
gdk_x11_surface_hide (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  /* We'll get the unmap notify eventually, and handle it then,
   * but checking here makes things more consistent if we are
   * just doing stuff ourself.
   */
  _gdk_x11_surface_grab_check_unmap (surface,
                                    NextRequest (GDK_SURFACE_XDISPLAY (surface)));

  g_clear_handle_id (&impl->compute_size_source_id, g_source_remove);
  g_clear_pointer (&impl->toplevel_layout, gdk_toplevel_layout_unref);

  gdk_x11_surface_withdraw (surface);

  impl->glx_frame_counter = 0;
}

static inline void
x11_surface_move (GdkSurface *surface,
                  int         x,
                  int         y)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  XMoveWindow (GDK_SURFACE_XDISPLAY (surface),
               GDK_SURFACE_XID (surface),
               x * impl->surface_scale, y * impl->surface_scale);

  if (impl->override_redirect)
    {
      impl->abs_x = x;
      impl->abs_y = y;

      if (surface->parent)
        {
          surface->x = impl->abs_x - GDK_X11_SURFACE (surface->parent)->abs_x;
          surface->y = impl->abs_y - GDK_X11_SURFACE (surface->parent)->abs_y;
        }
      else
        {
          surface->x = x;
          surface->y = y;
        }

      impl->next_layout.surface_geometry_dirty = TRUE;
      gdk_surface_request_layout (surface);
    }
}

static inline void
x11_surface_resize (GdkSurface *surface,
                   int        width,
                   int        height)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (width < 1)
    width = 1;

  if (height < 1)
    height = 1;

  gdk_x11_surface_pre_damage (surface);

  XResizeWindow (GDK_SURFACE_XDISPLAY (surface),
                 GDK_SURFACE_XID (surface),
                 width * impl->surface_scale, height * impl->surface_scale);

  if (impl->override_redirect)
    {
      impl->unscaled_width = width * impl->surface_scale;
      impl->unscaled_height = height * impl->surface_scale;
      impl->next_layout.configured_width = width;
      impl->next_layout.configured_height = height;
      impl->next_layout.surface_geometry_dirty = TRUE;
      gdk_surface_request_layout (surface);
    }
  else
    {
      if (width * impl->surface_scale != impl->unscaled_width ||
          height * impl->surface_scale != impl->unscaled_height)
        {
          surface->resize_count++;
          if (surface->resize_count == 1)
            gdk_surface_freeze_updates (surface);
        }
    }
}

static inline void
x11_surface_move_resize (GdkSurface *surface,
                         int         x,
                         int         y,
                         int         width,
                         int         height)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (width < 1)
    width = 1;

  if (height < 1)
    height = 1;

  gdk_x11_surface_pre_damage (surface);

  XMoveResizeWindow (GDK_SURFACE_XDISPLAY (surface),
                     GDK_SURFACE_XID (surface),
                     x * impl->surface_scale, y * impl->surface_scale,
                     width * impl->surface_scale, height * impl->surface_scale);

  if (impl->override_redirect)
    {
      impl->abs_x = x;
      impl->abs_y = y;

      impl->unscaled_width = width * impl->surface_scale;
      impl->unscaled_height = height * impl->surface_scale;
      impl->next_layout.configured_width = width;
      impl->next_layout.configured_height = height;
      impl->next_layout.surface_geometry_dirty = TRUE;
      gdk_surface_request_layout (surface);

      if (surface->parent)
        {
          surface->x = impl->abs_x - GDK_X11_SURFACE (surface->parent)->abs_x;
          surface->y = impl->abs_y - GDK_X11_SURFACE (surface->parent)->abs_y;
        }
      else
        {
          surface->x = x;
          surface->y = y;
        }
    }
  else
    {
      if (width * impl->surface_scale != impl->unscaled_width ||
          height * impl->surface_scale != impl->unscaled_height)
        {
          surface->resize_count++;
          if (surface->resize_count == 1)
            gdk_surface_freeze_updates (surface);
        }
    }
}

static void
gdk_x11_surface_move_resize (GdkSurface *surface,
                             gboolean    with_move,
                             int         x,
                             int         y,
                             int         width,
                             int         height)
{
  if (with_move && (width < 0 && height < 0))
    x11_surface_move (surface, x, y);
  else
    {
      if (with_move)
        x11_surface_move_resize (surface, x, y, width, height);
      else
        x11_surface_resize (surface, width, height);
    }
}

static void
gdk_x11_surface_toplevel_resize (GdkSurface *surface,
                                 int         width,
                                 int         height)
{
  x11_surface_resize (surface, width, height);
}

void
gdk_x11_surface_move (GdkSurface *surface,
                      int         x,
                      int         y)
{
  gdk_x11_surface_move_resize (surface, TRUE, x, y, -1, -1);
}

static void
gdk_x11_surface_layout_popup (GdkSurface     *surface,
                              int             width,
                              int             height,
                              GdkPopupLayout *layout)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkMonitor *monitor;
  GdkRectangle bounds;
  GdkRectangle final_rect;
  int x, y;

  monitor = gdk_surface_get_layout_monitor (surface, layout,
                                            gdk_x11_monitor_get_workarea);
  if (monitor)
    gdk_x11_monitor_get_workarea (monitor, &bounds);
  else
    {
      monitor = gdk_surface_get_layout_monitor (surface, layout,
                                                gdk_monitor_get_geometry);
      gdk_monitor_get_geometry (monitor, &bounds);
    }

  gdk_popup_layout_get_shadow_width (layout, 
                                     &impl->shadow_left,
                                     &impl->shadow_right,
                                     &impl->shadow_top,
                                     &impl->shadow_bottom);

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

  gdk_surface_get_origin (surface->parent, &x, &y);
  x += final_rect.x;
  y += final_rect.y;

  if (final_rect.width != surface->width ||
      final_rect.height != surface->height)
    {
      gdk_x11_surface_move_resize (surface,
                                   TRUE,
                                   x,
                                   y,
                                   final_rect.width,
                                   final_rect.height);
    }
  else
    {
      gdk_x11_surface_move (surface, x, y);
    }
}

static void
show_popup (GdkSurface *surface)
{
  gdk_x11_surface_raise (surface);
  gdk_surface_set_is_mapped (surface, TRUE);
  gdk_x11_surface_show (surface, FALSE);
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
gdk_x11_surface_present_popup (GdkSurface     *surface,
                               int             width,
                               int             height,
                               GdkPopupLayout *layout)
{
  gdk_x11_surface_layout_popup (surface, width, height, layout);

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

static void gdk_x11_surface_restack_toplevel (GdkSurface *surface,
                                              GdkSurface *sibling,
                                              gboolean    above);

void
gdk_x11_surface_update_popups (GdkSurface *parent)
{
  GList *l;

  for (l = parent->children; l; l = l->next)
    {
      GdkX11Surface *popup_impl = l->data;
      GdkSurface *popup = GDK_SURFACE (popup_impl);
      int new_x, new_y;

      if (GDK_SURFACE_DESTROYED (popup))
        continue;

      new_x = GDK_X11_SURFACE (parent)->abs_x + popup->x;
      new_y = GDK_X11_SURFACE (parent)->abs_y + popup->y;

      if (new_x != popup_impl->abs_x || new_y != popup_impl->abs_y)
        x11_surface_move (popup, new_x, new_y);
      gdk_x11_surface_restack_toplevel (popup, parent, TRUE);
    }
}

static void
gdk_x11_surface_set_is_on_monitor (GdkSurface *surface,
                                   GdkMonitor *monitor,
                                   gboolean    is_on_monitor)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GList *was_on_monitor;

  was_on_monitor = g_list_find (impl->surface_is_on_monitor, monitor);

  if (!was_on_monitor && is_on_monitor)
    {
      impl->surface_is_on_monitor = g_list_append (impl->surface_is_on_monitor,
                                                   monitor);
      gdk_surface_enter_monitor (surface, monitor);
    }
  else if (was_on_monitor && !is_on_monitor)
    {
      impl->surface_is_on_monitor = g_list_remove (impl->surface_is_on_monitor,
                                                   monitor);
      gdk_surface_leave_monitor (surface, monitor);
    }
}

void
gdk_x11_surface_check_monitor (GdkSurface *surface,
                               GdkMonitor *monitor)
{
  GdkRectangle monitor_geometry;
  GdkRectangle surface_geometry;
  gboolean is_on_monitor;

  gdk_monitor_get_geometry (monitor, &monitor_geometry);
  gdk_surface_get_geometry (surface,
                            &surface_geometry.x,
                            &surface_geometry.y,
                            &surface_geometry.width,
                            &surface_geometry.height);

  is_on_monitor = gdk_rectangle_intersect (&surface_geometry,
                                           &monitor_geometry,
                                           NULL);

  gdk_x11_surface_set_is_on_monitor (surface, monitor, is_on_monitor);
}

void
gdk_x11_surface_enter_leave_monitors (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GListModel *monitors;
  guint i;

  monitors = gdk_display_get_monitors (display);
  for (i = 0; i < g_list_model_get_n_items (monitors); i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (monitors, i);
      gdk_x11_surface_check_monitor (surface, monitor);
      g_object_unref (monitor);
    }
}

void
_gdk_x11_surface_set_surface_scale (GdkSurface *surface,
				    int scale)
{
  GdkX11Surface *impl;
  GdkToplevelX11 *toplevel;
  GdkSurfaceHints geom_mask;

  impl = GDK_X11_SURFACE (surface);

  if (!gdk_x11_surface_update_size (impl, surface->width, surface->height, scale))
    return;

  toplevel = _gdk_x11_surface_get_toplevel (surface);
  if (toplevel)
    {
      /* These are affected by surface scale: */
      geom_mask = toplevel->last_geometry_hints_mask & (GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
      if (geom_mask)
        gdk_x11_surface_set_geometry_hints (surface,
                                            &toplevel->last_geometry_hints,
                                            geom_mask);
    }

  if (impl->override_redirect)
    {
      impl->unscaled_width = surface->width * impl->surface_scale;
      impl->unscaled_height = surface->height * impl->surface_scale;
    }

  XResizeWindow (GDK_SURFACE_XDISPLAY (surface),
                 GDK_SURFACE_XID (surface),
                 surface->width * impl->surface_scale,
                 surface->height * impl->surface_scale);

  gdk_surface_invalidate_rect (surface, NULL);

  g_object_notify (G_OBJECT (surface), "scale-factor");
  g_object_notify (G_OBJECT (surface), "scale");
}

void
gdk_x11_surface_raise (GdkSurface *surface)
{
  XRaiseWindow (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XID (surface));
}

static void
gdk_x11_surface_restack_toplevel (GdkSurface *surface,
                                  GdkSurface *sibling,
                                  gboolean    above)
{
  XWindowChanges changes;

  changes.sibling = GDK_SURFACE_XID (sibling);
  changes.stack_mode = above ? Above : Below;
  XReconfigureWMWindow (GDK_SURFACE_XDISPLAY (surface),
			GDK_SURFACE_XID (surface),
			gdk_x11_screen_get_screen_number (GDK_SURFACE_SCREEN (surface)),
			CWStackMode | CWSibling, &changes);
}

static void
gdk_x11_surface_lower (GdkSurface *surface)
{
  XLowerWindow (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XID (surface));
}

/**
 * gdk_x11_surface_move_to_current_desktop:
 * @surface: (type GdkX11Surface): a `GdkSurface`
 * 
 * Moves the surface to the correct workspace when running under a 
 * window manager that supports multiple workspaces, as described
 * in the [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 * Will not do anything if the surface is already on all workspaces.
 */
void
gdk_x11_surface_move_to_current_desktop (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (toplevel->on_all_desktops)
    return;
  
  move_to_current_desktop (surface);
}

static void
move_to_current_desktop (GdkSurface *surface)
{
  guint32 desktop;

  desktop = gdk_x11_screen_get_current_desktop (GDK_SURFACE_SCREEN (surface));
  gdk_x11_surface_move_to_desktop (surface, desktop);
}

static guint32
get_netwm_cardinal_property (GdkSurface   *surface,
                             const char *name)
{
  GdkX11Screen *x11_screen = GDK_SURFACE_SCREEN (surface);
  guint32 prop = 0;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;

  if (!gdk_x11_screen_supports_net_wm_hint (x11_screen, name))
    return 0;

  XGetWindowProperty (x11_screen->xdisplay,
                      GDK_SURFACE_XID (surface),
                      gdk_x11_get_xatom_by_name_for_display (GDK_SURFACE_DISPLAY (surface), name),
                      0, G_MAXLONG,
                      False, XA_CARDINAL, &type, &format, &nitems,
                      &bytes_after, &data);
  if (type == XA_CARDINAL)
    {
      prop = *(gulong *)data;
      XFree (data);
    }

  return prop;
}

/**
 * gdk_x11_surface_get_desktop:
 * @surface: (type GdkX11Surface): a `GdkSurface`
 *
 * Gets the number of the workspace @surface is on.
 *
 * Returns: the current workspace of @surface
 */
guint32
gdk_x11_surface_get_desktop (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  return get_netwm_cardinal_property (surface, "_NET_WM_DESKTOP");
}

/**
 * gdk_x11_surface_move_to_desktop:
 * @surface: (type GdkX11Surface): a `GdkSurface`
 * @desktop: the number of the workspace to move the surface to
 *
 * Moves the surface to the given workspace when running unde a
 * window manager that supports multiple workspaces, as described
 * in the [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 */
void
gdk_x11_surface_move_to_desktop (GdkSurface *surface,
                                guint32    desktop)
{
  const char *atom_name = "_NET_WM_DESKTOP";
  XClientMessageEvent xclient;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (!gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface), atom_name))
    return;

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.serial = 0;
  xclient.send_event = True;
  xclient.window = GDK_SURFACE_XID (surface);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (GDK_SURFACE_DISPLAY (surface), atom_name);
  xclient.format = 32;

  xclient.data.l[0] = desktop;
  xclient.data.l[1] = 1; /* source indication */
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;

  XSendEvent (GDK_SURFACE_XDISPLAY (surface),
              GDK_SURFACE_XROOTWIN (surface),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}

static void
gdk_x11_surface_focus (GdkSurface *surface,
		      guint32    timestamp)
{
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  display = GDK_SURFACE_DISPLAY (surface);

  if (gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
					   g_intern_static_string ("_NET_ACTIVE_WINDOW")))
    {
      XClientMessageEvent xclient;

      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_SURFACE_XID (surface);
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display,
                                                                    "_NET_ACTIVE_WINDOW");
      xclient.format = 32;
      xclient.data.l[0] = 1; /* requestor type; we're an app */
      xclient.data.l[1] = timestamp;
      xclient.data.l[2] = None; /* currently active window */
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;
      
      XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XROOTWIN (surface), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
  else
    {
      XRaiseWindow (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface));

      /* There is no way of knowing reliably whether we are viewable;
       * so trap errors asynchronously around the XSetInputFocus call
       */
      gdk_x11_display_error_trap_push (display);
      XSetInputFocus (GDK_DISPLAY_XDISPLAY (display),
                      GDK_SURFACE_XID (surface),
                      RevertToParent,
                      timestamp);
      gdk_x11_display_error_trap_pop_ignored (display);
    }
}

static void
gdk_x11_surface_set_type_hint (GdkSurface        *surface,
                               GdkSurfaceTypeHint hint)
{
  GdkDisplay *display;
  Atom atom;
  
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  display = gdk_surface_get_display (surface);

  switch (hint)
    {
    case GDK_SURFACE_TYPE_HINT_DIALOG:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DIALOG");
      break;
    case GDK_SURFACE_TYPE_HINT_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_MENU");
      break;
    case GDK_SURFACE_TYPE_HINT_TOOLBAR:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLBAR");
      break;
    case GDK_SURFACE_TYPE_HINT_UTILITY:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_UTILITY");
      break;
    case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_SPLASH");
      break;
    case GDK_SURFACE_TYPE_HINT_DOCK:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DOCK");
      break;
    case GDK_SURFACE_TYPE_HINT_DESKTOP:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DESKTOP");
      break;
    case GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
      break;
    case GDK_SURFACE_TYPE_HINT_POPUP_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
      break;
    case GDK_SURFACE_TYPE_HINT_TOOLTIP:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case GDK_SURFACE_TYPE_HINT_NOTIFICATION:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case GDK_SURFACE_TYPE_HINT_COMBO:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_COMBO");
      break;
    case GDK_SURFACE_TYPE_HINT_DND:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_surface_set_type_hint", hint);
      G_GNUC_FALLTHROUGH;
    case GDK_SURFACE_TYPE_HINT_NORMAL:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NORMAL");
      break;
    }

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface),
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&atom, 1);
}

static void
gdk_wmspec_change_state (gboolean    add,
			 GdkSurface *surface,
			 const char *state1,
			 const char *state2)
{
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);
  XClientMessageEvent xclient;
  
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_SURFACE_XID (surface);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE");
  xclient.format = 32;
  xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xclient.data.l[1] = gdk_x11_get_xatom_by_name_for_display (display, state1);
  xclient.data.l[2] = gdk_x11_get_xatom_by_name_for_display (display, state2);
  xclient.data.l[3] = 1; /* source indication */
  xclient.data.l[4] = 0;
  
  XSendEvent (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XROOTWIN (surface), False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      (XEvent *)&xclient);
}

static void
gdk_x11_surface_set_modal_hint (GdkSurface *surface,
			       gboolean   modal)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  surface->modal_hint = modal;

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_wmspec_change_state (modal, surface,
			     "_NET_WM_STATE_MODAL", 
			     NULL);
}

/**
 * gdk_x11_surface_set_skip_taskbar_hint:
 * @surface: (type GdkX11Surface): a native `GdkSurface`
 * @skips_taskbar: %TRUE to skip taskbars
 *
 * Sets a hint on @surface that taskbars should not
 * display it. See the EWMH for details.
 */
void
gdk_x11_surface_set_skip_taskbar_hint (GdkSurface *surface,
                                       gboolean    skips_taskbar)
{
  GdkToplevelX11 *toplevel;
  
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  toplevel = _gdk_x11_surface_get_toplevel (surface);
  toplevel->skip_taskbar_hint = skips_taskbar;

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_wmspec_change_state (skips_taskbar, surface,
			     "_NET_WM_STATE_SKIP_TASKBAR",
			     NULL);
}

/**
 * gdk_x11_surface_set_skip_pager_hint:
 * @surface: (type GdkX11Surface): a `GdkSurface`
 * @skips_pager: %TRUE to skip pagers
 *
 * Sets a hint on @surface that pagers should not
 * display it. See the EWMH for details.
 */
void
gdk_x11_surface_set_skip_pager_hint (GdkSurface *surface,
                                     gboolean    skips_pager)
{
  GdkToplevelX11 *toplevel;
    
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  toplevel = _gdk_x11_surface_get_toplevel (surface);
  toplevel->skip_pager_hint = skips_pager;
  
  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_wmspec_change_state (skips_pager, surface,
			     "_NET_WM_STATE_SKIP_PAGER",
			     NULL);
}

/**
 * gdk_x11_surface_set_urgency_hint:
 * @surface: (type GdkX11Surface): a native `GdkSurface`
 * @urgent: %TRUE to indicate urgenct attention needed
 *
 * Sets a hint on @surface that it needs user attention.
 * See the ICCCM for details.
 */
void
gdk_x11_surface_set_urgency_hint (GdkSurface *surface,
                                  gboolean    urgent)
{
  GdkToplevelX11 *toplevel;
    
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  toplevel = _gdk_x11_surface_get_toplevel (surface);
  toplevel->urgency_hint = urgent;
  
  update_wm_hints (surface, FALSE);
}

static void
gdk_x11_surface_set_geometry_hints (GdkSurface         *surface,
				   const GdkGeometry *geometry,
				   GdkSurfaceHints     geom_mask)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  XSizeHints size_hints;
  GdkToplevelX11 *toplevel;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  toplevel = _gdk_x11_surface_get_toplevel (surface);
  if (toplevel)
    {
      if (geometry)
        toplevel->last_geometry_hints = *geometry;
      toplevel->last_geometry_hints_mask = geom_mask;
    }
  
  size_hints.flags = 0;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = geometry->min_width * impl->surface_scale;
      size_hints.min_height = geometry->min_height * impl->surface_scale;
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = MAX (geometry->max_width, 1) * impl->surface_scale;
      size_hints.max_height = MAX (geometry->max_height, 1) * impl->surface_scale;
    }
  
  else if (impl->surface_scale > 1)
    {
      size_hints.flags |= PResizeInc;
      size_hints.width_inc = impl->surface_scale;
      size_hints.height_inc = impl->surface_scale;
    }

  /* FIXME: Would it be better to delete this property if
   *        geom_mask == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_SURFACE_XDISPLAY (surface),
		     GDK_SURFACE_XID (surface),
		     &size_hints);
}

static void
gdk_surface_get_geometry_hints (GdkSurface      *surface,
                               GdkGeometry    *geometry,
                               GdkSurfaceHints *geom_mask)
{
  GdkX11Surface *impl;
  XSizeHints *size_hints;  
  glong junk_supplied_mask = 0;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (geometry != NULL);
  g_return_if_fail (geom_mask != NULL);

  *geom_mask = 0;
  
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_X11_SURFACE (surface);

  size_hints = XAllocSizeHints ();
  if (!size_hints)
    return;
  
  if (!XGetWMNormalHints (GDK_SURFACE_XDISPLAY (surface),
                          GDK_SURFACE_XID (surface),
                          size_hints,
                          &junk_supplied_mask))
    size_hints->flags = 0;

  if (size_hints->flags & PMinSize)
    {
      *geom_mask |= GDK_HINT_MIN_SIZE;
      geometry->min_width = size_hints->min_width / impl->surface_scale;
      geometry->min_height = size_hints->min_height / impl->surface_scale;
    }

  if (size_hints->flags & PMaxSize)
    {
      *geom_mask |= GDK_HINT_MAX_SIZE;
      geometry->max_width = MAX (size_hints->max_width, 1) / impl->surface_scale;
      geometry->max_height = MAX (size_hints->max_height, 1) / impl->surface_scale;
    }

  XFree (size_hints);
}

static gboolean
utf8_is_latin1 (const char *str)
{
  const char *p = str;

  while (*p)
    {
      gunichar ch = g_utf8_get_char (p);

      if (ch > 0xff)
	return FALSE;
      
      p = g_utf8_next_char (p);
    }

  return TRUE;
}

/* Set the property to @utf8_str as STRING if the @utf8_str is fully
 * convertible to STRING, otherwise, set it as compound text
 */
static void
set_text_property (GdkDisplay  *display,
		   Window       xwindow,
		   Atom         property,
		   const char *utf8_str)
{
  char *prop_text = NULL;
  Atom prop_type;
  int prop_length;
  int prop_format;
  gboolean is_compound_text;
  
  if (utf8_is_latin1 (utf8_str))
    {
      prop_type = XA_STRING;
      prop_text = gdk_x11_utf8_to_string_target (utf8_str, TRUE);
      prop_length = prop_text ? strlen (prop_text) : 0;
      prop_format = 8;
      is_compound_text = FALSE;
    }
  else
    {
      const char *gdk_type;

      gdk_x11_display_utf8_to_compound_text (display,
                                             utf8_str, &gdk_type, &prop_format,
                                             (guchar **)&prop_text, &prop_length);
      prop_type = gdk_x11_get_xatom_by_name_for_display (display, gdk_type);
      is_compound_text = TRUE;
    }

  if (prop_text)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		       xwindow,
		       property,
		       prop_type, prop_format,
		       PropModeReplace, (guchar *)prop_text,
		       prop_length);

      if (is_compound_text)
	gdk_x11_free_compound_text ((guchar *)prop_text);
      else
	g_free (prop_text);
    }
}

/* Set WM_NAME and _NET_WM_NAME
 */
static void
set_wm_name (GdkDisplay  *display,
	     Window       xwindow,
	     const char *name)
{
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_NAME"),
		   gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		   PropModeReplace, (guchar *)name, strlen (name));
  
  set_text_property (display, xwindow,
		     gdk_x11_get_xatom_by_name_for_display (display, "WM_NAME"),
		     name);
}

static void
gdk_x11_surface_set_title (GdkSurface   *surface,
			  const char *title)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  
  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (surface)) 
    return;
  
  display = gdk_surface_get_display (surface);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  xwindow = GDK_SURFACE_XID (surface);

  set_wm_name (display, xwindow, title);
  
  if (!gdk_surface_icon_name_set (surface))
    {
      XChangeProperty (xdisplay, xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON_NAME"),
		       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		       PropModeReplace, (guchar *)title, strlen (title));
      
      set_text_property (display, xwindow,
			 gdk_x11_get_xatom_by_name_for_display (display, "WM_ICON_NAME"),
			 title);
    }
}

static void
gdk_x11_surface_set_startup_id (GdkSurface   *surface,
			       const char *startup_id)
{
  GdkDisplay *display;
  char *free_this = NULL;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  display = gdk_surface_get_display (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (startup_id)
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface),
                     gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"), 
                     gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
                     PropModeReplace, (unsigned char *)startup_id, strlen (startup_id));
  else
    XDeleteProperty (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface),
                     gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"));

  if (startup_id == NULL)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

      startup_id = free_this = display_x11->startup_notification_id;
      display_x11->startup_notification_id = NULL;

      if (startup_id == NULL)
        return;
    }

  gdk_x11_display_broadcast_startup_message (display, "remove",
                                             "ID", startup_id,
                                             NULL);

  g_free (free_this);
}

static void
gdk_x11_surface_set_transient_for (GdkSurface *surface,
				  GdkSurface *parent)
{
  if (GDK_SURFACE_DESTROYED (surface)) 
    return;

  /* XSetTransientForHint() doesn't allow unsetting, so do it manually */
  if (parent && !GDK_SURFACE_DESTROYED (parent))
    {
      XSetTransientForHint (GDK_SURFACE_XDISPLAY (surface), 
                            GDK_SURFACE_XID (surface),
                            GDK_SURFACE_XID (parent));
      gdk_x11_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_DIALOG);
    }
  else
    {
      XDeleteProperty (GDK_SURFACE_XDISPLAY (surface),
                       GDK_SURFACE_XID (surface),
                       gdk_x11_get_xatom_by_name_for_display (GDK_SURFACE_DISPLAY (surface), "WM_TRANSIENT_FOR"));
      gdk_x11_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_NORMAL);
    }
}

GdkCursor *
_gdk_x11_surface_get_cursor (GdkSurface *surface)
{
  GdkX11Surface *impl;
  
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
    
  impl = GDK_X11_SURFACE (surface);

  return impl->cursor;
}

static void
gdk_x11_surface_get_geometry (GdkSurface *surface,
                             int       *x,
                             int       *y,
                             int       *width,
                             int       *height)
{
  GdkX11Surface *impl;
  Window root;
  Window child;
  int tx;
  int ty;
  guint twidth;
  guint theight;
  guint tborder_width;
  guint tdepth;
  
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      impl = GDK_X11_SURFACE (surface);

      XGetGeometry (GDK_SURFACE_XDISPLAY (surface),
		    GDK_SURFACE_XID (surface),
		    &root, &tx, &ty, &twidth, &theight, &tborder_width, &tdepth);

      XTranslateCoordinates (GDK_SURFACE_XDISPLAY (surface),
                             GDK_SURFACE_XID (surface),
                             root, 0, 0, &tx, &ty, &child);

      if (x)
	*x = tx / impl->surface_scale;
      if (y)
	*y = ty / impl->surface_scale;
      if (width)
	*width = twidth / impl->surface_scale;
      if (height)
	*height = theight / impl->surface_scale;
    }
}

void
gdk_x11_surface_get_root_coords (GdkSurface *surface,
				int        x,
				int        y,
				int       *root_x,
				int       *root_y)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  Window child;
  int tx;
  int ty;
  
  XTranslateCoordinates (GDK_SURFACE_XDISPLAY (surface),
                         GDK_SURFACE_XID (surface),
                         GDK_SURFACE_XROOTWIN (surface),
                         x * impl->surface_scale, y * impl->surface_scale, &tx, &ty,
                         &child);

  if (root_x)
    *root_x = tx / impl->surface_scale;
  if (root_y)
    *root_y = ty / impl->surface_scale;
}

static void
gdk_x11_surface_get_frame_extents (GdkSurface    *surface,
                                  GdkRectangle *rect)
{
  GdkDisplay *display;
  GdkX11Surface *impl;
  Window xwindow;
  Window xparent;
  Window root;
  Window child;
  Window *children;
  guchar *data;
  Window *vroots;
  Atom type_return;
  guint nchildren;
  guint nvroots;
  gulong nitems_return;
  gulong bytes_after_return;
  int format_return;
  int i;
  guint ww, wh, wb, wd;
  int wx, wy;
  gboolean got_frame_extents = FALSE;

  g_return_if_fail (rect != NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;

  impl = GDK_X11_SURFACE (surface);

  /* Refine our fallback answer a bit using local information */
  rect->x = impl->abs_x;
  rect->y = impl->abs_y;
  rect->width = surface->width;
  rect->height = surface->height;

  if (GDK_SURFACE_DESTROYED (surface) || impl->override_redirect)
    return;

  rect->x *= impl->surface_scale;
  rect->y *= impl->surface_scale;
  rect->width *= impl->surface_scale;
  rect->height *= impl->surface_scale;

  nvroots = 0;
  vroots = NULL;

  display = gdk_surface_get_display (surface);

  gdk_x11_display_error_trap_push (display);

  xwindow = GDK_SURFACE_XID (surface);

  /* first try: use _NET_FRAME_EXTENTS */
  if (gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                           g_intern_static_string ("_NET_FRAME_EXTENTS")) &&
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
                          gdk_x11_get_xatom_by_name_for_display (display,
                                                                  "_NET_FRAME_EXTENTS"),
                          0, G_MAXLONG, False, XA_CARDINAL, &type_return,
                          &format_return, &nitems_return, &bytes_after_return,
                          &data)
      == Success)
    {
      if ((type_return == XA_CARDINAL) && (format_return == 32) &&
	  (nitems_return == 4) && (data))
        {
	  gulong *ldata = (gulong *) data;
	  got_frame_extents = TRUE;

	  /* try to get the real client window geometry */
	  if (XGetGeometry (GDK_DISPLAY_XDISPLAY (display), xwindow,
			    &root, &wx, &wy, &ww, &wh, &wb, &wd) &&
              XTranslateCoordinates (GDK_DISPLAY_XDISPLAY (display),
	  			     xwindow, root, 0, 0, &wx, &wy, &child))
            {
	      rect->x = wx;
	      rect->y = wy;
	      rect->width = ww;
	      rect->height = wh;
	    }

	  /* _NET_FRAME_EXTENTS format is left, right, top, bottom */
	  rect->x -= ldata[0];
	  rect->y -= ldata[2];
	  rect->width += ldata[0] + ldata[1];
	  rect->height += ldata[2] + ldata[3];
	}

      if (data)
	XFree (data);
    }

  if (got_frame_extents)
    goto out;

  /* no frame extents property available, which means we either have a WM that
     is not EWMH compliant or is broken - try fallback and walk up the window
     tree to get our window's parent which hopefully is the window frame */

  /* use NETWM_VIRTUAL_ROOTS if available */
  root = GDK_SURFACE_XROOTWIN (surface);

  if (gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                           g_intern_static_string ("_NET_VIRTUAL_ROOTS")) &&
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), root,
			  gdk_x11_get_xatom_by_name_for_display (display, 
								 "_NET_VIRTUAL_ROOTS"),
			  0, G_MAXLONG, False, XA_WINDOW, &type_return,
			  &format_return, &nitems_return, &bytes_after_return,
			  &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) && (data))
	{
	  nvroots = nitems_return;
	  vroots = (Window *)data;
	}
    }

  xparent = GDK_SURFACE_XID (surface);

  do
    {
      xwindow = xparent;

      if (!XQueryTree (GDK_DISPLAY_XDISPLAY (display), xwindow,
		       &root, &xparent,
		       &children, &nchildren))
	goto out;
      
      if (children)
	XFree (children);

      /* check virtual roots */
      for (i = 0; i < nvroots; i++)
	{
	  if (xparent == vroots[i])
	    {
	      root = xparent;
	      break;
           }
	}
    }
  while (xparent != root);

  if (XGetGeometry (GDK_DISPLAY_XDISPLAY (display), xwindow,
		    &root, &wx, &wy, &ww, &wh, &wb, &wd))
    {
      rect->x = wx;
      rect->y = wy;
      rect->width = ww;
      rect->height = wh;
    }

 out:
  if (vroots)
    XFree (vroots);

  /* Here we extend the size to include the extra pixels if we round x/y down
     as well as round the size up when we divide by scale so that the returned
     size is guaranteed to cover the real pixels, but it may overshoot a bit
     in case the window is not positioned/sized according to the scale */
  rect->width = (rect->width + rect->x % impl->surface_scale + impl->surface_scale - 1) / impl->surface_scale;
  rect->height = (rect->height + rect->y % impl->surface_scale + impl->surface_scale - 1) / impl->surface_scale;
  rect->x = rect->x / impl->surface_scale;
  rect->y = rect->y / impl->surface_scale;
  gdk_x11_display_error_trap_pop_ignored (display);
}

static gboolean
gdk_x11_surface_get_device_state (GdkSurface     *surface,
                                  GdkDevice       *device,
                                  double          *x,
                                  double          *y,
                                  GdkModifierType *mask)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  gdk_x11_device_xi2_query_state (device, surface, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;
}

static void 
gdk_x11_surface_set_input_region (GdkSurface     *surface,
                                  cairo_region_t *input_region)
{
#ifdef ShapeInput
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (!gdk_display_supports_input_shapes (GDK_SURFACE_DISPLAY (surface)))
    return;

  if (input_region == NULL)
    {
      XShapeCombineMask (GDK_SURFACE_XDISPLAY (surface),
                         GDK_SURFACE_XID (surface),
                         ShapeInput,
                         0, 0,
                         None,
                         ShapeSet);
      return;
    }
  else
    {
      int n_rects = 0;
      XRectangle *xrects = NULL;

      _gdk_x11_region_get_xrectangles (input_region,
                                       0, 0, impl->surface_scale,
                                       &xrects, &n_rects);
      
      XShapeCombineRectangles (GDK_SURFACE_XDISPLAY (surface),
                               GDK_SURFACE_XID (surface),
			       ShapeInput,
                               0, 0,
                               xrects, n_rects,
                               ShapeSet,
                               YXBanded);
      
      g_free (xrects);
    }
#endif
}

/**
 * gdk_x11_surface_set_user_time:
 * @surface: (type GdkX11Surface): A toplevel `GdkSurface`
 * @timestamp: An XServer timestamp to which the property should be set
 *
 * The application can use this call to update the _NET_WM_USER_TIME
 * property on a toplevel surface.  This property stores an Xserver
 * time which represents the time of the last user input event
 * received for this surface.  This property may be used by the window
 * manager to alter the focus, stacking, and/or placement behavior of
 * surfaces when they are mapped depending on whether the new surface
 * was created by a user action or is a "pop-up" surface activated by a
 * timer or some other event.
 *
 * Note that this property is automatically updated by GDK, so this
 * function should only be used by applications which handle input
 * events bypassing GDK.
 **/
void
gdk_x11_surface_set_user_time (GdkSurface *surface,
                              guint32    timestamp)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkToplevelX11 *toplevel;
  glong timestamp_long = (glong)timestamp;
  Window xid;

  if (GDK_SURFACE_DESTROYED (surface)) 
    return;

  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);
  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (!toplevel)
    {
      g_warning ("gdk_surface_set_user_time called on non-toplevel\n");
      return;
    }

  if (toplevel->focus_window != None &&
      gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                           g_intern_static_string ("_NET_WM_USER_TIME_WINDOW")))
    xid = toplevel->focus_window;
  else
    xid = GDK_SURFACE_XID (surface);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xid,
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_USER_TIME"),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *)&timestamp_long, 1);

  if (timestamp_long != GDK_CURRENT_TIME &&
      (display_x11->user_time == GDK_CURRENT_TIME ||
       XSERVER_TIME_IS_LATER (timestamp_long, display_x11->user_time)))
    display_x11->user_time = timestamp_long;

  if (toplevel)
    toplevel->user_time = timestamp_long;
}

/**
 * gdk_x11_surface_set_utf8_property:
 * @surface: (type GdkX11Surface): a `GdkSurface`
 * @name: Property name, will be interned as an X atom
 * @value: (nullable): Property value, or %NULL to delete
 *
 * This function modifies or removes an arbitrary X11 window
 * property of type UTF8_STRING.  If the given @surface is
 * not a toplevel surface, it is ignored.
 */
void
gdk_x11_surface_set_utf8_property  (GdkSurface *surface,
				   const char *name,
				   const char *value)
{
  GdkDisplay *display;

  display = gdk_surface_get_display (surface);

  if (value != NULL)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_SURFACE_XID (surface),
                       gdk_x11_get_xatom_by_name_for_display (display, name),
                       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
                       PropModeReplace, (guchar *)value, strlen (value));
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_SURFACE_XID (surface),
                       gdk_x11_get_xatom_by_name_for_display (display, name));
    }
}

/**
 * gdk_x11_surface_set_theme_variant:
 * @surface: (type GdkX11Surface): a `GdkSurface`
 * @variant: the theme variant to export
 *
 * GTK applications can request a dark theme variant. In order to
 * make other applications - namely window managers using GTK for
 * themeing - aware of this choice, GTK uses this function to
 * export the requested theme variant as _GTK_THEME_VARIANT property
 * on toplevel surfaces.
 *
 * Note that this property is automatically updated by GTK, so this
 * function should only be used by applications which do not use GTK
 * to create toplevel surfaces.
 */
void
gdk_x11_surface_set_theme_variant (GdkSurface  *surface,
                                  const char *variant)
{
  gdk_x11_surface_set_utf8_property (surface, "_GTK_THEME_VARIANT",
                                    variant ? variant : "");
}

#define GDK_SELECTION_MAX_SIZE(display)                                 \
  MIN(262144,                                                           \
      XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) == 0     \
       ? XMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100         \
       : XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100)

static void
gdk_surface_update_icon (GdkSurface *surface,
                        GList     *icon_list)
{
  GdkToplevelX11 *toplevel;
  GdkTexture *best_icon;
  GList *tmp_list;
  int best_size;

  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (toplevel->icon_pixmap != NULL)
    {
      cairo_surface_destroy (toplevel->icon_pixmap);
      toplevel->icon_pixmap = NULL;
    }

  if (toplevel->icon_mask != NULL)
    {
      cairo_surface_destroy (toplevel->icon_mask);
      toplevel->icon_mask = NULL;
    }

#define IDEAL_SIZE 48

  best_size = G_MAXINT;
  best_icon = NULL;
  for (tmp_list = icon_list; tmp_list; tmp_list = tmp_list->next)
    {
      GdkTexture *texture = tmp_list->data;
      int this;

      /* average width and height - if someone passes in a rectangular
       * icon they deserve what they get.
       */
      this = gdk_texture_get_width (texture) + gdk_texture_get_height (texture);
      this /= 2;

      if (best_icon == NULL)
        {
          best_icon = texture;
          best_size = this;
        }
      else
        {
          /* icon is better if it's 32 pixels or larger, and closer to
           * the ideal size than the current best.
           */
          if (this >= 32 &&
              (ABS (best_size - IDEAL_SIZE) <
               ABS (this - IDEAL_SIZE)))
            {
              best_icon = texture;
              best_size = this;
            }
        }
    }

  if (best_icon)
    {
      int width = gdk_texture_get_width (best_icon);
      int height = gdk_texture_get_height (best_icon);
      cairo_surface_t *cairo_surface;
      cairo_t *cr;

      toplevel->icon_pixmap = gdk_x11_surface_create_pixmap_surface (surface, width, height);

      cairo_surface = gdk_texture_download_surface (best_icon);

      cr = cairo_create (toplevel->icon_pixmap);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_surface (cr, cairo_surface, 0, 0);
      if (cairo_surface_get_content (cairo_surface) == CAIRO_CONTENT_COLOR_ALPHA)
        {
          /* Saturate the image, so it has bilevel alpha */
          cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
          cairo_paint (cr);
          cairo_set_operator (cr, CAIRO_OPERATOR_SATURATE);
          cairo_paint (cr);
          cairo_pop_group_to_source (cr);
        }
      cairo_paint (cr);
      cairo_destroy (cr);

      if (cairo_surface_get_content (cairo_surface) == CAIRO_CONTENT_COLOR_ALPHA)
        {
          GdkDisplay *display = gdk_surface_get_display (surface);

          toplevel->icon_mask = _gdk_x11_display_create_bitmap_surface (display, width, height);

          cr = cairo_create (toplevel->icon_mask);
          cairo_set_source_surface (cr, cairo_surface, 0, 0);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_paint (cr);
          cairo_destroy (cr);
        }

      cairo_surface_destroy (cairo_surface);
    }

  update_wm_hints (surface, FALSE);
}

static void
gdk_x11_surface_set_icon_list (GdkSurface *surface,
			      GList     *textures)
{
  gulong *data;
  gulong *p;
  int size;
  GList *l;
  int width, height;
  GdkTexture *texture;
  GdkDisplay *display;
  int i, n;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  display = gdk_surface_get_display (surface);

  size = 0;
  n = 0;
  for (l = textures; l != NULL; l = l->next)
    {
      texture = l->data;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      /* silently ignore overlarge icons */
      if (size + 2 + width * height > GDK_SELECTION_MAX_SIZE(display))
        break;

      n++;
      size += 2 + width * height;
    }

  data = g_malloc (size * sizeof (gulong));

  p = data;
  for (l = textures; l != NULL && n > 0; l = l->next)
    {
      texture = l->data;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      *p++ = width;
      *p++ = height;

      gdk_texture_download (texture, (guchar *) p, width * 4);
      if (sizeof (gulong) > 4)
        {
          i = width * height;
          while (i-- > 0)
            p[i] = ((guint32 *) p)[i];
        }

      p += width * height;
      n--;
    }

  if (size > 0)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_SURFACE_XID (surface),
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON"),
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (guchar*) data, size);
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_SURFACE_XID (surface),
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON"));
    }
  
  g_free (data);

  gdk_surface_update_icon (surface, textures);
}

static gboolean
gdk_surface_icon_name_set (GdkSurface *surface)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (surface),
					       g_quark_from_static_string ("gdk-icon-name-set")));
}

static void
gdk_x11_surface_minimize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    {  
      XIconifyWindow (GDK_SURFACE_XDISPLAY (surface),
		      GDK_SURFACE_XID (surface),
		      gdk_x11_screen_get_screen_number (GDK_SURFACE_SCREEN (surface)));
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_surface_state (surface,
                                    0,
                                    GDK_TOPLEVEL_STATE_MINIMIZED |
                                    GDK_TOPLEVEL_STATE_SUSPENDED);
      gdk_wmspec_change_state (TRUE, surface,
                               "_NET_WM_STATE_HIDDEN",
                               NULL);
    }
}

static void
gdk_x11_surface_unminimize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    {  
      gdk_x11_surface_show (surface, TRUE);
      gdk_wmspec_change_state (FALSE, surface,
                               "_NET_WM_STATE_HIDDEN",
                               NULL);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_surface_state (surface,
                                    GDK_TOPLEVEL_STATE_MINIMIZED |
                                    GDK_TOPLEVEL_STATE_SUSPENDED,
                                    0);
      gdk_wmspec_change_state (FALSE, surface,
                               "_NET_WM_STATE_HIDDEN",
                               NULL);
    }
}

static void
gdk_x11_surface_maximize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_wmspec_change_state (TRUE, surface,
			     "_NET_WM_STATE_MAXIMIZED_VERT",
			     "_NET_WM_STATE_MAXIMIZED_HORZ");
  else
    gdk_synthesize_surface_state (surface,
				 0,
				 GDK_TOPLEVEL_STATE_MAXIMIZED);
}

static void
gdk_x11_surface_unmaximize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_wmspec_change_state (FALSE, surface,
			     "_NET_WM_STATE_MAXIMIZED_VERT",
			     "_NET_WM_STATE_MAXIMIZED_HORZ");
  else
    gdk_synthesize_surface_state (surface,
				 GDK_TOPLEVEL_STATE_MAXIMIZED,
				 0);
}

static void
gdk_x11_surface_apply_fullscreen_mode (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  /* _NET_WM_FULLSCREEN_MONITORS gives an indication to the window manager as
   * to which monitors so span across when the surface is fullscreen, but it's
   * not a state in itself so this would have no effect if the surface is not
   * mapped.
   */

  if (GDK_SURFACE_IS_MAPPED (surface))
    {
      XClientMessageEvent xclient;
      int                 monitors[4];
      int                 i;

      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_SURFACE_XID (surface);
      xclient.display = GDK_SURFACE_XDISPLAY (surface);
      xclient.format = 32;

      switch (surface->fullscreen_mode)
	{
	case GDK_FULLSCREEN_ON_CURRENT_MONITOR:

	  /* FIXME: This is not part of the EWMH spec!
	   *
	   * There is no documented mechanism to remove the property
	   * _NET_WM_FULLSCREEN_MONITORS once set, so we use a set of
	   * invalid, largest possible value.
	   *
	   * When given values larger than actual possible monitor values, most
	   * window managers who support the _NET_WM_FULLSCREEN_MONITORS spec
	   * will simply unset _NET_WM_FULLSCREEN_MONITORS and revert to their
	   * default behavior.
	   *
	   * Successfully tested on mutter/metacity, kwin, compiz and xfwm4.
	   *
	   * Note, this (non documented) mechanism is unlikely to be an issue
	   * as it's used only for transitioning back from "all monitors" to
	   * "current monitor" mode.
	   *
	   * Applications who don't change the default mode won't trigger this
	   * mechanism.
	   */
	  for (i = 0; i < 4; ++i)
	    xclient.data.l[i] = G_MAXLONG;

	  break;

	case GDK_FULLSCREEN_ON_ALL_MONITORS:

	  _gdk_x11_screen_get_edge_monitors (GDK_SURFACE_SCREEN (surface),
					     &monitors[0],
					     &monitors[1],
					     &monitors[2],
					     &monitors[3]);
	  /* Translate all 4 monitors from the GDK set into XINERAMA indices */
	  for (i = 0; i < 4; ++i)
	    {
	      xclient.data.l[i] = monitors[i];
	      /* Sanity check, if XINERAMA is not available, we could have invalid
	       * negative values for the XINERAMA indices.
	       */
	      if (xclient.data.l[i] < 0)
		{
		  g_warning ("gdk_x11_surface_apply_fullscreen_mode: Invalid XINERAMA monitor index");
		  return;
		}
	    }
	  break;

	default:
	  g_warning ("gdk_x11_surface_apply_fullscreen_mode: Unhandled fullscreen mode %d",
		     surface->fullscreen_mode);
	  return;
	}

      /* Send fullscreen monitors client message */
      xclient.data.l[4] = 1; /* source indication */
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (GDK_SURFACE_DISPLAY (surface),
								    "_NET_WM_FULLSCREEN_MONITORS");
      XSendEvent (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XROOTWIN (surface), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
}

static void
gdk_x11_surface_fullscreen (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    {
      gdk_wmspec_change_state (TRUE, surface,
			       "_NET_WM_STATE_FULLSCREEN",
                               NULL);
      /* Actual XRandR layout may have change since we computed the fullscreen
       * monitors in GDK_FULLSCREEN_ON_ALL_MONITORS mode.
       */
      if (surface->fullscreen_mode == GDK_FULLSCREEN_ON_ALL_MONITORS)
	gdk_x11_surface_apply_fullscreen_mode (surface);
    }
  else
    gdk_synthesize_surface_state (surface,
                                 0,
                                 GDK_TOPLEVEL_STATE_FULLSCREEN);
}

static void
gdk_x11_surface_fullscreen_on_monitor (GdkSurface  *surface,
                                      GdkMonitor *monitor)
{
  GdkRectangle geom;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  gdk_monitor_get_geometry (monitor, &geom);
  gdk_x11_surface_move (surface, geom.x, geom.y);

  surface->fullscreen_mode = GDK_FULLSCREEN_ON_CURRENT_MONITOR;
  g_object_notify (G_OBJECT (surface), "fullscreen-mode");
  gdk_x11_surface_fullscreen (surface);
}

static void
gdk_x11_surface_unfullscreen (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_wmspec_change_state (FALSE, surface,
			     "_NET_WM_STATE_FULLSCREEN",
                             NULL);

  else
    gdk_synthesize_surface_state (surface,
				 GDK_TOPLEVEL_STATE_FULLSCREEN,
				 0);
}

/**
 * gdk_x11_surface_get_group:
 * @surface: (type GdkX11Surface): The `GdkSurface`
 *
 * Returns the group this surface belongs to.
 *
 * Returns: (transfer none) (nullable): The group of this surface;
 */
GdkSurface *
gdk_x11_surface_get_group (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel;
  
  if (GDK_SURFACE_DESTROYED (surface))
    return NULL;
  
  toplevel = _gdk_x11_surface_get_toplevel (surface);

  return toplevel->group_leader;
}

/**
 * gdk_x11_surface_set_group:
 * @surface: (type GdkX11Surface): a native `GdkSurface`
 * @leader: a `GdkSurface`
 *
 * Sets the group leader of @surface to be @leader.
 * See the ICCCM for details.
 */
void
gdk_x11_surface_set_group (GdkSurface *surface,
                           GdkSurface *leader)
{
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (leader == NULL || GDK_IS_SURFACE (leader));

  if (GDK_SURFACE_DESTROYED (surface) ||
      (leader != NULL && GDK_SURFACE_DESTROYED (leader)))
    return;

  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (leader == NULL)
    leader = gdk_x11_display_get_default_group (gdk_surface_get_display (surface));
  
  if (toplevel->group_leader != leader)
    {
      if (toplevel->group_leader)
	g_object_unref (toplevel->group_leader);
      toplevel->group_leader = g_object_ref (leader);
      (_gdk_x11_surface_get_toplevel (leader))->is_leader = TRUE;      
    }

  update_wm_hints (surface, FALSE);
}

static MotifWmHints *
gdk_surface_get_mwm_hints (GdkSurface *surface)
{
  GdkDisplay *display;
  Atom hints_atom = None;
  guchar *data;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_SURFACE_DESTROYED (surface))
    return NULL;

  display = gdk_surface_get_display (surface);
  
  hints_atom = gdk_x11_get_xatom_by_name_for_display (display, _XA_MOTIF_WM_HINTS);

  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, &data);

  if (type == None)
    return NULL;
  
  return (MotifWmHints *)data;
}

static void
gdk_surface_set_mwm_hints (GdkSurface *surface,
			  MotifWmHints *new_hints)
{
  GdkDisplay *display;
  Atom hints_atom = None;
  guchar *data;
  MotifWmHints *hints;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_SURFACE_DESTROYED (surface))
    return;
  
  display = gdk_surface_get_display (surface);
  
  hints_atom = gdk_x11_get_xatom_by_name_for_display (display, _XA_MOTIF_WM_HINTS);

  XGetWindowProperty (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XID (surface),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, &data);
  
  if (type == None)
    hints = new_hints;
  else
    {
      hints = (MotifWmHints *)data;
	
      if (new_hints->flags & MWM_HINTS_FUNCTIONS)
	{
	  hints->flags |= MWM_HINTS_FUNCTIONS;
	  hints->functions = new_hints->functions;
	}
      if (new_hints->flags & MWM_HINTS_DECORATIONS)
	{
	  hints->flags |= MWM_HINTS_DECORATIONS;
	  hints->decorations = new_hints->decorations;
	}
    }
  
  XChangeProperty (GDK_SURFACE_XDISPLAY (surface), GDK_SURFACE_XID (surface),
		   hints_atom, hints_atom, 32, PropModeReplace,
		   (guchar *)hints, sizeof (MotifWmHints)/sizeof (long));
  
  if (hints != new_hints)
    XFree (hints);
}

typedef enum
{
  GDK_DECOR_ALL         = 1 << 0,
  GDK_DECOR_BORDER      = 1 << 1,
  GDK_DECOR_RESIZEH     = 1 << 2,
  GDK_DECOR_TITLE       = 1 << 3,
  GDK_DECOR_MENU        = 1 << 4,
  GDK_DECOR_MINIMIZE    = 1 << 5,
  GDK_DECOR_MAXIMIZE    = 1 << 6
} GdkWMDecoration;

static void
gdk_x11_surface_set_decorations (GdkSurface      *surface,
				GdkWMDecoration decorations)
{
  MotifWmHints hints;

  if (GDK_SURFACE_DESTROYED (surface))
    return;
  
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&hints, 0, sizeof(hints));
  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = decorations;
  
  gdk_surface_set_mwm_hints (surface, &hints);
}

static gboolean
gdk_x11_surface_get_decorations(GdkSurface       *surface,
			       GdkWMDecoration *decorations)
{
  MotifWmHints *hints;
  gboolean result = FALSE;

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;
  
  hints = gdk_surface_get_mwm_hints (surface);
  
  if (hints)
    {
      if (hints->flags & MWM_HINTS_DECORATIONS)
	{
	  if (decorations)
	    *decorations = hints->decorations;
	  result = TRUE;
	}
      
      XFree (hints);
    }

  return result;
}

typedef enum
{
  GDK_FUNC_ALL          = 1 << 0,
  GDK_FUNC_RESIZE       = 1 << 1,
  GDK_FUNC_MOVE         = 1 << 2,
  GDK_FUNC_MINIMIZE     = 1 << 3,
  GDK_FUNC_MAXIMIZE     = 1 << 4,
  GDK_FUNC_CLOSE        = 1 << 5
} GdkWMFunction;

static void
gdk_x11_surface_set_functions (GdkSurface    *surface,
                              GdkWMFunction functions)
{
  MotifWmHints hints;
  
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;
  
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&hints, 0, sizeof(hints));
  hints.flags = MWM_HINTS_FUNCTIONS;
  hints.functions = functions;
  
  gdk_surface_set_mwm_hints (surface, &hints);
}

static gboolean
gdk_x11_surface_get_functions (GdkSurface       *surface,
			       GdkWMFunction    *functions)
{
  MotifWmHints *hints;
  gboolean result = FALSE;

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;
  
  hints = gdk_surface_get_mwm_hints (surface);
  
  if (hints)
    {
      if (hints->flags & MWM_HINTS_DECORATIONS)
	{
	  if (functions)
	    *functions = hints->functions;
	  result = TRUE;
	}
      
      XFree (hints);
    }

  return result;
}

cairo_region_t *
_gdk_x11_xwindow_get_shape (Display *xdisplay,
                            Window   window,
                            int      scale,
                            int      shape_type)
{
  cairo_region_t *shape;
  GdkRectangle *rl;
  XRectangle *xrl;
  int rn, ord, i;

  shape = NULL;
  rn = 0;

  /* Note that XShapeGetRectangles returns NULL in two situations:
   * - the server doesn't support the SHAPE extension
   * - the shape is empty
   *
   * Since we can't discriminate these here, we always return
   * an empty shape. It is the callers responsibility to check
   * whether the server supports the SHAPE extensions beforehand.
   */
  xrl = XShapeGetRectangles (xdisplay, window, shape_type, &rn, &ord);

  if (rn == 0)
    return cairo_region_create (); /* Empty */

  if (ord != YXBanded)
    {
      /* This really shouldn't happen with any xserver, as they
       * generally convert regions to YXBanded internally
       */
      g_warning ("non YXBanded shape masks not supported");
      XFree (xrl);
      return NULL;
    }

  /* NOTE: The scale divisions here may lose some precision if someone
     else set the shape to be non-scale precision */
  rl = g_new (GdkRectangle, rn);
  for (i = 0; i < rn; i++)
    {
      rl[i].x = xrl[i].x / scale;
      rl[i].y = xrl[i].y / scale;
      rl[i].width = xrl[i].width / scale;
      rl[i].height = xrl[i].height / scale;
    }
  XFree (xrl);

  shape = cairo_region_create_rectangles (rl, rn);
  g_free (rl);

  return shape;
}

/* From the WM spec */
#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */

static void
wmspec_send_message (GdkDisplay *display,
                     GdkSurface  *surface,
                     int         root_x,
                     int         root_y,
                     int         action,
                     int         button)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  XClientMessageEvent xclient;

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_SURFACE_XID (surface);
  xclient.message_type =
    gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_MOVERESIZE");
  xclient.format = 32;
  xclient.data.l[0] = root_x * impl->surface_scale;
  xclient.data.l[1] = root_y * impl->surface_scale;
  xclient.data.l[2] = action;
  xclient.data.l[3] = button;
  xclient.data.l[4] = 1;  /* source indication */

  XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XROOTWIN (surface), False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}

static void
handle_wmspec_button_release (GdkDisplay   *display,
                              const XEvent *xevent)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkSurface *surface;

  XIEvent *xiev = (XIEvent *) xevent->xcookie.data;
  XIDeviceEvent *xidev = (XIDeviceEvent *) xiev;

  if (xevent->xany.type == GenericEvent)
    surface = gdk_x11_surface_lookup_for_display (display, xidev->event);
  else
    surface = gdk_x11_surface_lookup_for_display (display, xevent->xany.window);

  if (display_x11->wm_moveresize_button != 0 && surface != NULL)
    {
      if ((xevent->xany.type == ButtonRelease &&
           xevent->xbutton.button == display_x11->wm_moveresize_button) ||
          (xevent->xany.type == GenericEvent &&
           xiev->evtype == XI_ButtonRelease &&
           xidev->detail == display_x11->wm_moveresize_button))
        {
          display_x11->wm_moveresize_button = 0;
          wmspec_send_message (display, surface, 0, 0, _NET_WM_MOVERESIZE_CANCEL, 0);
        }
    }
}

static void
wmspec_moveresize (GdkSurface *surface,
                   int        direction,
                   GdkDevice *device,
                   int        button,
                   int        root_x,
                   int        root_y,
                   guint32    timestamp)
{
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);

  if (button != 0)
    gdk_seat_ungrab (gdk_device_get_seat (device)); /* Release passive grab */
  GDK_X11_DISPLAY (display)->wm_moveresize_button = button;

  wmspec_send_message (display, surface, root_x, root_y, direction, button);
}

static void
wmspec_resize_drag (GdkSurface     *surface,
                    GdkSurfaceEdge  edge,
                    GdkDevice     *device,
                    int            button,
                    int            root_x,
                    int            root_y,
                    guint32        timestamp)
{
  int direction;
  
  if (button == 0)
    direction = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
  else
    switch (edge)
      {
      /* Let the compiler turn a switch into a table, instead
       * of doing the table manually, this way is easier to verify.
       */
      case GDK_SURFACE_EDGE_NORTH_WEST:
        direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
        break;

      case GDK_SURFACE_EDGE_NORTH:
        direction = _NET_WM_MOVERESIZE_SIZE_TOP;
        break;

      case GDK_SURFACE_EDGE_NORTH_EAST:
        direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
        break;

      case GDK_SURFACE_EDGE_WEST:
        direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
        break;

      case GDK_SURFACE_EDGE_EAST:
        direction = _NET_WM_MOVERESIZE_SIZE_RIGHT;
        break;

      case GDK_SURFACE_EDGE_SOUTH_WEST:
        direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
        break;

      case GDK_SURFACE_EDGE_SOUTH:
        direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
        break;

      case GDK_SURFACE_EDGE_SOUTH_EAST:
        direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
        break;

      default:
        g_warning ("gdk_toplevel_begin_resize: bad resize edge %d!",
                   edge);
        return;
      }
  
  wmspec_moveresize (surface, direction, device, button, root_x, root_y, timestamp);
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;

  GdkSurface *moveresize_surface;
  GdkSurface *moveresize_emulation_surface;
  gboolean is_resize;
  GdkSurfaceEdge resize_edge;
  GdkDevice *device;
  int moveresize_button;
  int moveresize_x;
  int moveresize_y;
  int moveresize_orig_x;
  int moveresize_orig_y;
  int moveresize_orig_width;
  int moveresize_orig_height;
  GdkSurfaceHints moveresize_geom_mask;
  GdkGeometry moveresize_geometry;
  Time moveresize_process_time;
  XEvent *moveresize_pending_event;
};

static MoveResizeData *
get_move_resize_data (GdkDisplay *display,
		      gboolean    create)
{
  MoveResizeData *mv_resize;
  static GQuark move_resize_quark = 0;

  if (!move_resize_quark)
    move_resize_quark = g_quark_from_static_string ("gdk-surface-moveresize");
  
  mv_resize = g_object_get_qdata (G_OBJECT (display), move_resize_quark);

  if (!mv_resize && create)
    {
      mv_resize = g_new0 (MoveResizeData, 1);
      mv_resize->display = display;
      
      g_object_set_qdata (G_OBJECT (display), move_resize_quark, mv_resize);
    }

  return mv_resize;
}

static void
check_maximize (MoveResizeData *mv_resize,
                double          x_root,
                double          y_root)
{
  GdkToplevelState state;
  int y;

  if (mv_resize->is_resize)
    return;

  state = gdk_toplevel_get_state (GDK_TOPLEVEL (mv_resize->moveresize_surface));

  if (state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    return;

  y = mv_resize->moveresize_orig_y + (y_root - mv_resize->moveresize_y);

  if (y < 10)
    gdk_x11_surface_maximize (mv_resize->moveresize_surface);
}

static void
check_unmaximize (MoveResizeData *mv_resize,
                  double          x_root,
                  double          y_root)
{
  GdkToplevelState state;
  int dx, dy;

  if (mv_resize->is_resize)
    return;

  state = gdk_toplevel_get_state (GDK_TOPLEVEL (mv_resize->moveresize_surface));

  if ((state & (GDK_TOPLEVEL_STATE_MAXIMIZED | GDK_TOPLEVEL_STATE_TILED)) == 0)
    return;

  dx = x_root - mv_resize->moveresize_x;
  dy = y_root - mv_resize->moveresize_y;

  if (ABS (dx) > 20 || ABS (dy) > 20)
    gdk_x11_surface_unmaximize (mv_resize->moveresize_surface);
}

static void
update_pos (MoveResizeData *mv_resize,
	    int             new_root_x,
	    int             new_root_y)
{
  int dx, dy;

  check_unmaximize (mv_resize, new_root_x, new_root_y);
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

      gdk_x11_surface_move_resize (mv_resize->moveresize_surface, TRUE,
                                   x, y, w, h);
    }
  else
    {
      int x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_x11_surface_move (mv_resize->moveresize_surface, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_surface_destroy (mv_resize->moveresize_emulation_surface);
  mv_resize->moveresize_emulation_surface = NULL;
  g_clear_object (&mv_resize->moveresize_surface);
  g_clear_pointer (&mv_resize->moveresize_pending_event, g_free);
}

static int
lookahead_motion_predicate (Display *xdisplay,
			    XEvent  *event,
			    XPointer arg)
{
  gboolean *seen_release = (gboolean *)arg;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (*seen_release)
    return False;

  switch (event->xany.type)
    {
    case ButtonRelease:
      *seen_release = TRUE;
      break;
    case MotionNotify:
      mv_resize->moveresize_process_time = event->xmotion.time;
      break;
    default:
      break;
    }

  return False;
}

static gboolean
moveresize_lookahead (MoveResizeData *mv_resize,
		      const XEvent   *event)
{
  XEvent tmp_event;
  gboolean seen_release = FALSE;

  if (mv_resize->moveresize_process_time)
    {
      if (event->xmotion.time == mv_resize->moveresize_process_time)
        {
          mv_resize->moveresize_process_time = 0;
          return TRUE;
        }
      else
        return FALSE;
    }

  XCheckIfEvent (event->xany.display, &tmp_event,
                 lookahead_motion_predicate, (XPointer) & seen_release);

  return mv_resize->moveresize_process_time == 0;
}

gboolean
_gdk_x11_moveresize_handle_event (const XEvent *event)
{
  guint button_mask = 0;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (event->xany.display);
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);
  GdkX11Surface *impl;

  if (!mv_resize || !mv_resize->moveresize_surface)
    {
      handle_wmspec_button_release (display, event);
      return FALSE;
    }

  impl = GDK_X11_SURFACE (mv_resize->moveresize_surface);

  if (mv_resize->moveresize_button != 0)
    button_mask = GDK_BUTTON1_MASK << (mv_resize->moveresize_button - 1);

  switch (event->xany.type)
    {
    case MotionNotify:
      if (mv_resize->moveresize_surface->resize_count > 0)
        {
          if (mv_resize->moveresize_pending_event)
            *mv_resize->moveresize_pending_event = *event;
          else
            mv_resize->moveresize_pending_event =
              g_memdup2 (event, sizeof (XEvent));

          break;
        }
      if (!moveresize_lookahead (mv_resize, event))
        break;

      update_pos (mv_resize,
                  event->xmotion.x_root / impl->surface_scale,
                  event->xmotion.y_root / impl->surface_scale);

      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->xmotion.state & button_mask) == 0)
        {
          check_maximize (mv_resize,
                          event->xmotion.x_root / impl->surface_scale,
                          event->xmotion.y_root / impl->surface_scale);
          finish_drag (mv_resize);
        }
      break;

    case ButtonRelease:
      update_pos (mv_resize,
                  event->xbutton.x_root / impl->surface_scale,
                  event->xbutton.y_root / impl->surface_scale);

      if (event->xbutton.button == mv_resize->moveresize_button)
        {
          check_maximize (mv_resize,
                          event->xmotion.x_root / impl->surface_scale,
                          event->xmotion.y_root / impl->surface_scale);
          finish_drag (mv_resize);
        }
      break;

    case GenericEvent:
      {
        /* we just assume this is an XI2 event */
        XIEvent *ev = (XIEvent *) event->xcookie.data;
        XIDeviceEvent *xev = (XIDeviceEvent *)ev;
        int state;
        switch (ev->evtype)
          {
          case XI_Motion:
            update_pos (mv_resize, xev->root_x / impl->surface_scale, xev->root_y / impl->surface_scale);
            state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
            if ((state & button_mask) == 0)
              {
                check_maximize (mv_resize,
                                xev->root_x / impl->surface_scale,
                                xev->root_y / impl->surface_scale);
                finish_drag (mv_resize);
              }
            break;

          case XI_ButtonRelease:
            update_pos (mv_resize, xev->root_x / impl->surface_scale, xev->root_y / impl->surface_scale);
            if (xev->detail == mv_resize->moveresize_button)
              {
                check_maximize (mv_resize,
                                xev->root_x / impl->surface_scale,
                                xev->root_y / impl->surface_scale);
                finish_drag (mv_resize);
              }
            break;
          default:
            break;
          }
      }
      break;

    default:
      break;

    }
  return TRUE;
}

gboolean
_gdk_x11_moveresize_configure_done (GdkDisplay *display,
                                    GdkSurface  *surface)
{
  XEvent *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  gdk_surface_thaw_updates (surface);
  gdk_surface_request_layout (surface);

  if (!mv_resize || surface != mv_resize->moveresize_surface)
    return FALSE;

  if (mv_resize->moveresize_pending_event)
    {
      tmp_event = mv_resize->moveresize_pending_event;
      mv_resize->moveresize_pending_event = NULL;
      _gdk_x11_moveresize_handle_event (tmp_event);
      g_free (tmp_event);
    }

  return TRUE;
}

static void
create_moveresize_surface (MoveResizeData *mv_resize,
                          guint32         timestamp)
{
  GdkGrabStatus status;

  g_assert (mv_resize->moveresize_emulation_surface == NULL);

  mv_resize->moveresize_emulation_surface = gdk_x11_drag_surface_new (mv_resize->display);

  gdk_surface_set_is_mapped (mv_resize->moveresize_emulation_surface, TRUE);
  gdk_x11_surface_show (mv_resize->moveresize_emulation_surface, FALSE);

  status = gdk_seat_grab (gdk_device_get_seat (mv_resize->device),
                          mv_resize->moveresize_emulation_surface,
                          GDK_SEAT_CAPABILITY_POINTER, FALSE,
                          NULL, NULL, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the surface
       * already.
       */
      finish_drag (mv_resize);
    }

  mv_resize->moveresize_process_time = 0;
}

/* 
   Calculate mv_resize->moveresize_orig_x and mv_resize->moveresize_orig_y
   so that calling XMoveWindow with these coordinates will not move the 
   surface.
   Note that this depends on the WM to implement ICCCM-compliant reference
   point handling.
*/
static void 
calculate_unmoving_origin (MoveResizeData *mv_resize)
{
  GdkRectangle rect;

  gdk_x11_surface_get_frame_extents (mv_resize->moveresize_surface, &rect);
  mv_resize->moveresize_orig_x = rect.x;
  mv_resize->moveresize_orig_y = rect.y;
}

static void
emulate_resize_drag (GdkSurface     *surface,
                     GdkSurfaceEdge  edge,
                     GdkDevice     *device,
                     int            button,
                     int            root_x,
                     int            root_y,
                     guint32        timestamp)
{
  MoveResizeData *mv_resize = get_move_resize_data (GDK_SURFACE_DISPLAY (surface), TRUE);

  if (mv_resize->moveresize_surface != NULL)
    return; /* already a drag operation in progress */

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->device = device;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_surface = g_object_ref (surface);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (surface);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (surface);

  mv_resize->moveresize_geom_mask = 0;
  gdk_surface_get_geometry_hints (surface,
				 &mv_resize->moveresize_geometry,
				 &mv_resize->moveresize_geom_mask);

  calculate_unmoving_origin (mv_resize);

  create_moveresize_surface (mv_resize, timestamp);
}

static void
emulate_move_drag (GdkSurface *surface,
                   GdkDevice  *device,
                   int         button,
                   int         root_x,
                   int         root_y,
                   guint32     timestamp)
{
  MoveResizeData *mv_resize = get_move_resize_data (GDK_SURFACE_DISPLAY (surface), TRUE);

  if (mv_resize->moveresize_surface != NULL)
    return; /* already a drag operation in progress */

  mv_resize->is_resize = FALSE;
  mv_resize->device = device;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;

  mv_resize->moveresize_surface = g_object_ref (surface);

  calculate_unmoving_origin (mv_resize);

  create_moveresize_surface (mv_resize, timestamp);
}

static gboolean
_should_perform_ewmh_drag (GdkSurface *surface,
                           GdkDevice *device)
{
  GdkPointerSurfaceInfo *info;
  GdkDisplay *display;

  display = gdk_surface_get_display (surface);
  info = _gdk_display_get_pointer_info (display, device);

  if ((info->last_physical_device == NULL ||
       gdk_device_get_source (info->last_physical_device) != GDK_SOURCE_TOUCHSCREEN) &&
      gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                           g_intern_static_string ("_NET_WM_MOVERESIZE")))
    return TRUE;

  return FALSE;
}

static void
gdk_x11_toplevel_begin_resize (GdkToplevel    *toplevel,
                               GdkSurfaceEdge  edge,
                               GdkDevice      *device,
                               int             button,
                               double          x,
                               double          y,
                               guint32         timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  int root_x, root_y;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  gdk_x11_surface_get_root_coords (surface, x, y, &root_x, &root_y);

  /* Avoid EWMH for touch devices */
  if (_should_perform_ewmh_drag (surface, device))
    wmspec_resize_drag (surface, edge, device, button, root_x, root_y, timestamp);
  else
    emulate_resize_drag (surface, edge, device, button, root_x, root_y, timestamp);
}

static void
gdk_x11_toplevel_begin_move (GdkToplevel *toplevel,
                             GdkDevice   *device,
                             int          button,
                             double       x,
                             double       y,
                             guint32      timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  int root_x, root_y;
  int direction;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (button == 0)
    direction = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
  else
    direction = _NET_WM_MOVERESIZE_MOVE;

  gdk_x11_surface_get_root_coords (surface, x, y, &root_x, &root_y);

  /* Avoid EWMH for touch devices */
  if (_should_perform_ewmh_drag (surface, device))
    wmspec_moveresize (surface, direction, device, button, root_x, root_y, timestamp);
  else
    emulate_move_drag (surface, device, button, root_x, root_y, timestamp);
}

static gboolean
gdk_x11_surface_beep (GdkSurface *surface)
{
  GdkDisplay *display;

  display = GDK_SURFACE_DISPLAY (surface);

  if (!GDK_X11_DISPLAY (display)->trusted_client)
    return FALSE;

#ifdef HAVE_XKB
  if (GDK_X11_DISPLAY (display)->use_xkb)
    {
      XkbBell (GDK_DISPLAY_XDISPLAY (display),
               GDK_SURFACE_XID (surface),
               0,
               None);
      return TRUE;
    }
#endif

  return FALSE;
}

void
gdk_x11_surface_set_opacity (GdkSurface *surface,
			     double      opacity)
{
  GdkDisplay *display;
  gulong cardinal;
  
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  display = gdk_surface_get_display (surface);

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  cardinal = opacity * 0xffffffff;

  if (cardinal == 0xffffffff)
    XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
		     GDK_SURFACE_XID (surface),
		     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_OPACITY"));
  else
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		     GDK_SURFACE_XID (surface),
		     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_OPACITY"),
		     XA_CARDINAL, 32,
		     PropModeReplace,
		     (guchar *) &cardinal, 1);
}

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  Window xwindow = GPOINTER_TO_UINT (arg);
  GdkDisplay *gdk_display = gdk_x11_lookup_xdisplay (display);

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == xwindow &&
      xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (gdk_display,
								       "GDK_TIMESTAMP_PROP"))
    return True;

  return False;
}

/**
 * gdk_x11_get_server_time:
 * @surface: (type GdkX11Surface): a `GdkSurface`, used for communication
 *   with the server. The surface must have `GDK_PROPERTY_CHANGE_MASK` in
 *   its events mask or a hang will result.
 *
 * Routine to get the current X server time stamp.
 *
 * Returns: the time stamp
 */
guint32
gdk_x11_get_server_time (GdkSurface *surface)
{
  Display *xdisplay;
  Window   xwindow;
  guchar c = 'a';
  XEvent xevent;
  Atom timestamp_prop_atom;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);
  g_return_val_if_fail (!GDK_SURFACE_DESTROYED (surface), 0);

  xdisplay = GDK_SURFACE_XDISPLAY (surface);
  xwindow = GDK_SURFACE_XID (surface);
  timestamp_prop_atom =
    gdk_x11_get_xatom_by_name_for_display (GDK_SURFACE_DISPLAY (surface),
					   "GDK_TIMESTAMP_PROP");

  XChangeProperty (xdisplay, xwindow, timestamp_prop_atom,
		   timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (xdisplay, &xevent,
	    timestamp_predicate, GUINT_TO_POINTER(xwindow));

  return xevent.xproperty.time;
}

/**
 * gdk_x11_surface_get_xid:
 * @surface: (type GdkX11Surface): a native `GdkSurface`.
 * 
 * Returns the X resource (surface) belonging to a `GdkSurface`.
 * 
 * Returns: the ID of @drawable’s X resource.
 **/
XID
gdk_x11_surface_get_xid (GdkSurface *surface)
{
  return GDK_X11_SURFACE (surface)->xid;
}

static double
gdk_x11_surface_get_scale (GdkSurface *surface)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);

  return impl->surface_scale;
}

/**
 * gdk_x11_surface_set_frame_sync_enabled:
 * @surface: (type GdkX11Surface): a native `GdkSurface`
 * @frame_sync_enabled: whether frame-synchronization should be enabled
 *
 * This function can be used to disable frame synchronization for a surface.
 * Normally frame synchronziation will be enabled or disabled based on whether
 * the system has a compositor that supports frame synchronization, but if
 * the surface is not directly managed by the window manager, then frame
 * synchronziation may need to be disabled. This is the case for a surface
 * embedded via the XEMBED protocol.
 */
void
gdk_x11_surface_set_frame_sync_enabled (GdkSurface *surface,
                                       gboolean   frame_sync_enabled)
{
  GDK_X11_SURFACE (surface)->frame_sync_enabled = FALSE;
}

static void
gdk_x11_surface_set_opaque_region (GdkSurface      *surface,
                                  cairo_region_t *region)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkDisplay *display;
  int nitems;
  gulong *data;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (region != NULL)
    {
      int i, nrects;

      nrects = cairo_region_num_rectangles (region);
      nitems = nrects * 4;
      data = g_new (gulong, nitems);

      for (i = 0; i < nrects; i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (region, i, &rect);
          data[i*4+0] = rect.x * impl->surface_scale;
          data[i*4+1] = rect.y * impl->surface_scale;
          data[i*4+2] = rect.width * impl->surface_scale;
          data[i*4+3] = rect.height * impl->surface_scale;
        }
    }
  else
    {
      nitems = 0;
      data = NULL;
    }

  display = gdk_surface_get_display (surface);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (surface),
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_OPAQUE_REGION"),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *) data, nitems);

  g_free (data);
}

static gboolean
gdk_x11_surface_show_window_menu (GdkSurface *surface,
                                  GdkEvent   *event)
{
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);
  GdkDevice *device;
  int device_id;
  double x, y;
  int x_root, y_root;
  XClientMessageEvent xclient = { 0 };

  GdkEventType event_type = gdk_event_get_event_type (event);

  switch ((guint) event_type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      break;
    default:
      return FALSE;
    }

  if (!gdk_x11_screen_supports_net_wm_hint (GDK_SURFACE_SCREEN (surface),
                                            g_intern_static_string ("_GTK_SHOW_WINDOW_MENU")))
    return FALSE;

  gdk_event_get_position (event, &x, &y);
  gdk_x11_surface_get_root_coords (surface, x, y, &x_root, &y_root);
  device = gdk_event_get_device (event);
  g_object_get (G_OBJECT (device),
                "device-id", &device_id,
                NULL);

  /* Ungrab the implicit grab */
  gdk_seat_ungrab (gdk_device_get_seat (device));

  xclient.type = ClientMessage;
  xclient.window = GDK_SURFACE_XID (surface);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_GTK_SHOW_WINDOW_MENU");
  xclient.data.l[0] = device_id;
  xclient.data.l[1] = x_root * impl->surface_scale;
  xclient.data.l[2] = y_root * impl->surface_scale;
  xclient.format = 32;

  XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XROOTWIN (surface), False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);

  return TRUE;
}

static void
gdk_x11_surface_class_init (GdkX11SurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);
  
  object_class->constructed = gdk_x11_surface_constructed;
  object_class->finalize = gdk_x11_surface_finalize;
  
  impl_class->hide = gdk_x11_surface_hide;
  impl_class->get_geometry = gdk_x11_surface_get_geometry;
  impl_class->get_root_coords = gdk_x11_surface_get_root_coords;
  impl_class->get_device_state = gdk_x11_surface_get_device_state;
  impl_class->set_input_region = gdk_x11_surface_set_input_region;
  impl_class->destroy = gdk_x11_surface_destroy;
  impl_class->beep = gdk_x11_surface_beep;

  impl_class->destroy_notify = gdk_x11_surface_destroy_notify;
  impl_class->drag_begin = _gdk_x11_surface_drag_begin;
  impl_class->get_scale = gdk_x11_surface_get_scale;
  impl_class->set_opaque_region = gdk_x11_surface_set_opaque_region;
  impl_class->request_layout = gdk_x11_surface_request_layout;
  impl_class->compute_size = gdk_x11_surface_compute_size;
}

static void
gdk_x11_surface_create_window (GdkX11Surface        *self,
                               XSetWindowAttributes *xattributes,
                               long                  xattributes_mask)
{
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (self));
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  g_assert (self->xid == 0);

  xattributes->background_pixmap = None;
  xattributes_mask |= CWBackPixmap;

  xattributes->border_pixel = BlackPixel (GDK_DISPLAY_XDISPLAY (display),
                                          display_x11->screen->screen_num);
  xattributes_mask |= CWBorderPixel;

  xattributes->bit_gravity = NorthWestGravity;
  xattributes_mask |= CWBitGravity;

  xattributes->colormap = gdk_x11_display_get_window_colormap (display_x11);
  xattributes_mask |= CWColormap;

  self->xid = XCreateWindow (GDK_DISPLAY_XDISPLAY (display),
                             display_x11->screen->xroot_window,
                             0, 0, 1, 1,
                             0,
                             gdk_x11_display_get_window_depth (display_x11),
                             InputOutput,
                             gdk_x11_display_get_window_visual (display_x11),
                             xattributes_mask, xattributes);
}

#define LAST_PROP 1

typedef struct {
  GdkX11Surface parent_instance;
} GdkX11Popup;

typedef struct {
  GdkX11SurfaceClass parent_class;
} GdkX11PopupClass;

static void gdk_x11_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkX11Popup, gdk_x11_popup, GDK_TYPE_X11_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                gdk_x11_popup_iface_init))

static void
gdk_x11_popup_init (GdkX11Popup *popup)
{
}

static void
gdk_x11_popup_constructed (GObject *object)
{
  GdkX11Surface *x11_surface = GDK_X11_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (x11_surface);
  XSetWindowAttributes xattributes;
  long xattributes_mask;

  xattributes.save_under = True;
  xattributes.override_redirect = True;
  xattributes_mask = CWSaveUnder | CWOverrideRedirect;

  gdk_x11_surface_create_window (x11_surface, &xattributes, xattributes_mask);

  x11_surface->override_redirect = TRUE;

  gdk_surface_set_frame_clock (surface, gdk_surface_get_frame_clock (surface->parent));

  G_OBJECT_CLASS (gdk_x11_popup_parent_class)->constructed (object);

  gdk_x11_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_MENU);
}

static void
gdk_x11_popup_get_property (GObject    *object,
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
gdk_x11_popup_set_property (GObject      *object,
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
gdk_x11_popup_class_init (GdkX11PopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_x11_popup_constructed;
  object_class->get_property = gdk_x11_popup_get_property;
  object_class->set_property = gdk_x11_popup_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
gdk_x11_popup_present (GdkPopup       *popup,
                       int             width,
                       int             height,
                       GdkPopupLayout *layout)
{
  return gdk_x11_surface_present_popup (GDK_SURFACE (popup), width, height, layout);
}

static GdkGravity
gdk_x11_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_x11_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_x11_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_x11_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_x11_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_x11_popup_present;
  iface->get_surface_anchor = gdk_x11_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_x11_popup_get_rect_anchor;
  iface->get_position_x = gdk_x11_popup_get_position_x;
  iface->get_position_y = gdk_x11_popup_get_position_y;
}

static void gdk_x11_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkX11Toplevel, gdk_x11_toplevel, GDK_TYPE_X11_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                gdk_x11_toplevel_iface_init))

static void
gdk_x11_toplevel_init (GdkX11Toplevel *toplevel)
{
}

static void
gdk_x11_toplevel_constructed (GObject *object)
{
  GdkX11Surface *x11_surface = GDK_X11_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (x11_surface);
  GdkFrameClock *frame_clock;
  XSetWindowAttributes xattributes;
  long xattributes_mask;

  xattributes_mask = 0;

  gdk_x11_surface_create_window (x11_surface, &xattributes, xattributes_mask);

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_x11_toplevel_parent_class)->constructed (object);

  gdk_x11_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_NORMAL);
}

static void
gdk_x11_toplevel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_x11_surface_set_title (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      gdk_x11_surface_set_startup_id (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_x11_surface_set_transient_for (surface, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      gdk_x11_surface_set_modal_hint (surface, g_value_get_boolean (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      gdk_x11_surface_set_icon_list (surface, g_value_get_pointer (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      gdk_x11_surface_set_decorations (surface, g_value_get_boolean (value) ? GDK_DECOR_ALL : 0);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      gdk_x11_surface_set_functions (surface, g_value_get_boolean (value) ? GDK_FUNC_ALL : GDK_FUNC_ALL | GDK_FUNC_CLOSE);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      gdk_x11_surface_apply_fullscreen_mode (surface);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_toplevel_get_property (GObject    *object,
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
      {
        GdkWMDecoration decorations = GDK_DECOR_ALL;
        gdk_x11_surface_get_decorations (surface, &decorations);
        g_value_set_boolean (value, decorations != 0);
      }
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      {
        GdkWMFunction functions = GDK_FUNC_ALL;
        gdk_x11_surface_get_functions (surface, &functions);
        g_value_set_boolean (value, functions == GDK_FUNC_ALL);
      }
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      g_value_set_enum (value, surface->fullscreen_mode);
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
gdk_x11_toplevel_class_init (GdkX11ToplevelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_x11_toplevel_constructed;
  object_class->get_property = gdk_x11_toplevel_get_property;
  object_class->set_property = gdk_x11_toplevel_set_property;

  gdk_toplevel_install_properties (object_class, LAST_PROP);
}

static void
gdk_x11_toplevel_present (GdkToplevel       *toplevel,
                          GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkX11Surface *impl = GDK_X11_SURFACE (surface);
  int width, height;
  gboolean was_mapped;
  gboolean maximize;
  gboolean fullscreen;

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  gdk_x11_surface_unminimize (surface);

  g_clear_pointer (&impl->toplevel_layout, gdk_toplevel_layout_unref);
  impl->toplevel_layout = gdk_toplevel_layout_copy (layout);

  if (compute_toplevel_size (surface, DONT_UPDATE_GEOMETRY, &width, &height))
    gdk_x11_surface_toplevel_resize (surface, width, height);

  if (gdk_toplevel_layout_get_maximized (layout, &maximize))
    {
      if (maximize)
        gdk_x11_surface_maximize (surface);
      else
        gdk_x11_surface_unmaximize (surface);
    }

  if (gdk_toplevel_layout_get_fullscreen (layout, &fullscreen))
    {
      if (fullscreen)
        {
          GdkMonitor *fullscreen_monitor =
            gdk_toplevel_layout_get_fullscreen_monitor (layout);

          if (fullscreen_monitor)
            gdk_x11_surface_fullscreen_on_monitor (surface, fullscreen_monitor);
          else
            gdk_x11_surface_fullscreen (surface);
        }
      else
        {
          gdk_x11_surface_unfullscreen (surface);
        }
    }

  impl->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);

  if (!was_mapped)
    gdk_surface_set_is_mapped (surface, TRUE);

  gdk_x11_surface_show (surface, was_mapped);

  if (!was_mapped)
    gdk_surface_invalidate_rect (surface, NULL);
}

static gboolean
gdk_x11_toplevel_minimize (GdkToplevel *toplevel)
{
  gdk_x11_surface_minimize (GDK_SURFACE (toplevel));

  return TRUE;
}

static gboolean
gdk_x11_toplevel_lower (GdkToplevel *toplevel)
{
  gdk_x11_surface_lower (GDK_SURFACE (toplevel));

  return TRUE;
}

static void
gdk_x11_toplevel_focus (GdkToplevel *toplevel,
                        guint32      timestamp)
{
  gdk_x11_surface_focus (GDK_SURFACE (toplevel), timestamp);
}

static gboolean
gdk_x11_toplevel_show_window_menu (GdkToplevel *toplevel,
                                   GdkEvent    *event)
{
  return gdk_x11_surface_show_window_menu (GDK_SURFACE (toplevel), event);
}

static gboolean
gdk_x11_toplevel_supports_edge_constraints (GdkToplevel *toplevel)
{
  return gdk_x11_surface_supports_edge_constraints (GDK_SURFACE (toplevel));
}

static void
gdk_x11_toplevel_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                           GdkEvent    *gdk_event)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkSeat *gdk_seat;
  GdkGrabStatus status;

  if (surface->shortcuts_inhibited)
    return; /* Already inhibited */

  if (!(surface->state & GDK_TOPLEVEL_STATE_FOCUSED))
    return;

  gdk_seat = gdk_surface_get_seat_from_event (surface, gdk_event);

  if (!(gdk_seat_get_capabilities (gdk_seat) & GDK_SEAT_CAPABILITY_KEYBOARD))
    return;

  status = gdk_seat_grab (gdk_seat, surface, GDK_SEAT_CAPABILITY_KEYBOARD,
                          TRUE, NULL, gdk_event, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    return;

  surface->shortcuts_inhibited = TRUE;
  surface->current_shortcuts_inhibited_seat = gdk_seat;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
gdk_x11_toplevel_restore_system_shortcuts (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkSeat *gdk_seat;

  if (!surface->shortcuts_inhibited)
    return; /* Not inhibited */

  gdk_seat = surface->current_shortcuts_inhibited_seat;
  gdk_seat_ungrab (gdk_seat);
  surface->current_shortcuts_inhibited_seat = NULL;

  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
gdk_x11_toplevel_state_callback (GdkSurface *surface)
{
  if (surface->state & GDK_TOPLEVEL_STATE_FOCUSED)
    return;

  if (surface->shortcuts_inhibited)
    gdk_x11_toplevel_restore_system_shortcuts (GDK_TOPLEVEL (surface));
}

static gboolean
gdk_x11_toplevel_event_callback (GdkSurface *surface,
                                 GdkEvent   *gdk_event)
{
  GdkSeat *gdk_seat;

  if (!surface->shortcuts_inhibited)
    return FALSE;

  if (gdk_event_get_event_type (gdk_event) != GDK_GRAB_BROKEN)
    return FALSE;

  gdk_seat = gdk_surface_get_seat_from_event (surface, gdk_event);
  if (gdk_seat != surface->current_shortcuts_inhibited_seat)
    return FALSE;

  surface->current_shortcuts_inhibited_seat = NULL;
  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (surface), "shortcuts-inhibited");

  return FALSE;
}

static void
gdk_x11_toplevel_export_handle (GdkToplevel          *toplevel,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
  guint32 xid;
  GTask *task;

  xid = (guint32) gdk_x11_surface_get_xid (GDK_SURFACE (toplevel));

  task = g_task_new (toplevel, cancellable, callback, user_data);
  g_task_return_pointer (task, g_strdup_printf ("%x", xid), g_free);
  g_object_unref (task);
}

static char *
gdk_x11_toplevel_export_handle_finish (GdkToplevel   *toplevel,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gdk_x11_toplevel_unexport_handle (GdkToplevel *toplevel,
                                  const char  *handle)
{
}

static void
gdk_x11_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_x11_toplevel_present;
  iface->minimize = gdk_x11_toplevel_minimize;
  iface->lower = gdk_x11_toplevel_lower;
  iface->focus = gdk_x11_toplevel_focus;
  iface->show_window_menu = gdk_x11_toplevel_show_window_menu;
  iface->supports_edge_constraints = gdk_x11_toplevel_supports_edge_constraints;
  iface->inhibit_system_shortcuts = gdk_x11_toplevel_inhibit_system_shortcuts;
  iface->restore_system_shortcuts = gdk_x11_toplevel_restore_system_shortcuts;
  iface->begin_resize = gdk_x11_toplevel_begin_resize;
  iface->begin_move = gdk_x11_toplevel_begin_move;
  iface->export_handle = gdk_x11_toplevel_export_handle;
  iface->export_handle_finish = gdk_x11_toplevel_export_handle_finish;
  iface->unexport_handle = gdk_x11_toplevel_unexport_handle;
}

typedef struct {
  GdkX11Surface parent_instance;
} GdkX11DragSurface;

typedef struct {
  GdkX11SurfaceClass parent_class;
} GdkX11DragSurfaceClass;

static void gdk_x11_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkX11DragSurface, gdk_x11_drag_surface, GDK_TYPE_X11_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                gdk_x11_drag_surface_iface_init))

static void
gdk_x11_drag_surface_constructed (GObject *object)
{
  GdkX11Surface *x11_surface = GDK_X11_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (object);
  GdkFrameClock *frame_clock;
  XSetWindowAttributes xattributes;
  long xattributes_mask;

  xattributes.save_under = True;
  xattributes.override_redirect = True;
  xattributes_mask = CWSaveUnder | CWOverrideRedirect;

  gdk_x11_surface_create_window (x11_surface, &xattributes, xattributes_mask);

  x11_surface->override_redirect = TRUE;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_x11_drag_surface_parent_class)->constructed (object);
}

static void
gdk_x11_drag_surface_init (GdkX11DragSurface *surface)
{
}

static void
gdk_x11_drag_surface_class_init (GdkX11DragSurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_x11_drag_surface_constructed;
}

static gboolean
gdk_x11_drag_surface_present (GdkDragSurface *drag_surface,
                              int             width,
                              int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);

  gdk_x11_surface_toplevel_resize (surface, width, height);
  gdk_surface_set_is_mapped (surface, TRUE);
  gdk_x11_surface_show (surface, FALSE);
  gdk_surface_invalidate_rect (surface, NULL);

  return TRUE;
}

static void
gdk_x11_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_x11_drag_surface_present;
}

GdkSurface *
gdk_x11_drag_surface_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_X11_DRAG_SURFACE,
                       "display", display,
                       NULL);
}

