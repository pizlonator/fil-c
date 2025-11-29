/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkwaylandtoplevel.h"

#include "gdkdeviceprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkglcontext-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkpopupprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkseat-wayland.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkdevice-wayland-private.h"

#include <wayland/presentation-time-client-protocol.h>
#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>
#include <wayland/xdg-dialog-v1-client-protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <unistd.h>

#include "gdksurface-wayland-private.h"
#include "gdktoplevel-wayland-private.h"

#define MAX_WL_BUFFER_SIZE (4083) /* 4096 minus header, string argument length and NUL byte */

static void gdk_wayland_toplevel_sync_parent             (GdkWaylandToplevel *toplevel);
static void gdk_wayland_toplevel_sync_parent_of_imported (GdkWaylandToplevel *toplevel);
static void gdk_wayland_surface_create_xdg_toplevel      (GdkWaylandToplevel *toplevel);
static void gdk_wayland_toplevel_sync_title              (GdkWaylandToplevel *toplevel);
static void unset_transient_for_exported                 (GdkWaylandToplevel *toplevel);

/* {{{ GdkWaylandToplevel definition */

typedef struct {
  struct zxdg_exported_v1 *xdg_exported;
  struct zxdg_exported_v2 *xdg_exported_v2;
  char *handle;
} GdkWaylandExported;

/**
 * GdkWaylandToplevel:
 *
 * The Wayland implementation of `GdkToplevel`.
 *
 * Beyond the [iface@Gdk.Toplevel] API, the Wayland implementation
 * has API to set up cross-process parent-child relationships between
 * surfaces with [method@GdkWayland.WaylandToplevel.export_handle] and
 * [method@GdkWayland.WaylandToplevel.set_transient_for_exported].
 */

struct _GdkWaylandToplevel
{
  GdkWaylandSurface parent_instance;

  struct {
    struct gtk_surface1 *gtk_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct zxdg_toplevel_v6 *zxdg_toplevel_v6;
    struct xdg_dialog_v1 *xdg_dialog;
  } display_server;

  GdkWaylandToplevel *transient_for;

  struct org_kde_kwin_server_decoration *server_decoration;
  GList *exported;

  struct {
    int width;
    int height;
    GdkToplevelState state;
    gboolean is_resizing;

    int bounds_width;
    int bounds_height;
    gboolean has_bounds;
  } pending;

  struct {
      gboolean should_constrain;
      gboolean size_is_fixed;
  } next_layout;

  struct {
    gboolean was_set;

    char *application_id;
    char *app_menu_path;
    char *menubar_path;
    char *window_object_path;
    char *application_object_path;
    char *unique_bus_name;
  } application;

  struct zwp_idle_inhibitor_v1 *idle_inhibitor;
  size_t idle_inhibitor_refcount;

  struct wl_output *initial_fullscreen_output;

  struct wp_presentation_feedback *feedback;

  struct {
    GdkToplevelState unset_flags;
    GdkToplevelState set_flags;
  } initial_state;

  int saved_width;
  int saved_height;

  GdkToplevelLayout *layout;
  int bounds_width;
  int bounds_height;
  gboolean has_bounds;

  char *title;

  GdkGeometry geometry_hints;
  GdkSurfaceHints geometry_mask;
  GdkGeometry last_sent_geometry_hints;

  struct zxdg_imported_v1 *imported_transient_for;
  struct zxdg_imported_v2 *imported_transient_for_v2;
  GHashTable *shortcuts_inhibitors;
};

typedef struct
{
  GdkWaylandSurfaceClass parent_class;
} GdkWaylandToplevelClass;

static void gdk_wayland_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandToplevel, gdk_wayland_toplevel, GDK_TYPE_WAYLAND_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                gdk_wayland_toplevel_iface_init))

/* }}} */
/* {{{ Utilities */

static gboolean
is_realized_shell_surface (GdkWaylandSurface *impl)
{
  return (impl->display_server.xdg_surface ||
          impl->display_server.zxdg_surface_v6);
}

static void
gdk_wayland_toplevel_save_size (GdkWaylandToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);

  if (surface->state & (GDK_TOPLEVEL_STATE_FULLSCREEN |
                        GDK_TOPLEVEL_STATE_MAXIMIZED |
                        GDK_TOPLEVEL_STATE_TILED))
    return;

  if (surface->width <= 1 || surface->height <= 1)
    return;

  toplevel->saved_width = surface->width - impl->shadow_left - impl->shadow_right;
  toplevel->saved_height = surface->height - impl->shadow_top - impl->shadow_bottom;
}

static void
gdk_wayland_toplevel_clear_saved_size (GdkWaylandToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);

  if (surface->state & (GDK_TOPLEVEL_STATE_FULLSCREEN | GDK_TOPLEVEL_STATE_MAXIMIZED))
    return;

  toplevel->saved_width = -1;
  toplevel->saved_height = -1;
}

/* }}} */
/* {{{ Toplevel implementation */

static void maybe_set_gtk_surface_dbus_properties (GdkWaylandToplevel *wayland_toplevel);
static void maybe_set_gtk_surface_modal (GdkWaylandToplevel *wayland_toplevel);
static gboolean maybe_set_xdg_dialog_modal (GdkWaylandToplevel *wayland_toplevel);

static void
gdk_wayland_toplevel_hide_surface (GdkWaylandSurface *wayland_surface)
{
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (wayland_surface);
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_clear_pointer (&toplevel->display_server.xdg_toplevel, xdg_toplevel_destroy);
  g_clear_pointer (&toplevel->display_server.zxdg_toplevel_v6, zxdg_toplevel_v6_destroy);
  g_clear_pointer (&toplevel->display_server.xdg_dialog, xdg_dialog_v1_destroy);

  if (toplevel->display_server.gtk_surface)
    {
      if (gtk_shell1_get_version (display_wayland->gtk_shell) >= GTK_SURFACE1_RELEASE_SINCE_VERSION)
        gtk_surface1_release (toplevel->display_server.gtk_surface);
      else
        gtk_surface1_destroy (toplevel->display_server.gtk_surface);
      toplevel->display_server.gtk_surface = NULL;
      toplevel->application.was_set = FALSE;
    }

  g_clear_pointer (&toplevel->layout, gdk_toplevel_layout_unref);

  toplevel->last_sent_geometry_hints.min_width = 0;
  toplevel->last_sent_geometry_hints.min_height = 0;
  toplevel->last_sent_geometry_hints.max_width = 0;
  toplevel->last_sent_geometry_hints.max_height = 0;

  gdk_wayland_toplevel_clear_saved_size (toplevel);

  unset_transient_for_exported (toplevel);
}

static gboolean
is_realized_toplevel (GdkWaylandSurface *impl)
{
  GdkWaylandToplevel *toplevel;

  if (!GDK_IS_WAYLAND_TOPLEVEL (impl))
    return FALSE;

  toplevel = GDK_WAYLAND_TOPLEVEL (impl);

  return (toplevel->display_server.xdg_toplevel ||
          toplevel->display_server.zxdg_toplevel_v6);
}

static void
gdk_wayland_toplevel_sync_parent (GdkWaylandToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandToplevel *parent;

  if (!is_realized_toplevel (GDK_WAYLAND_SURFACE (toplevel)))
    return;

  if (toplevel->transient_for)
    parent = toplevel->transient_for;
  else
    parent = NULL;

  /* XXX: Is this correct? */
  if (parent && !is_realized_shell_surface (GDK_WAYLAND_SURFACE (parent)))
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_toplevel *parent_toplevel;

        if (parent)
          parent_toplevel = parent->display_server.xdg_toplevel;
        else
          parent_toplevel = NULL;

        xdg_toplevel_set_parent (toplevel->display_server.xdg_toplevel, parent_toplevel);
        break;
      }
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_toplevel_v6 *parent_toplevel;

        if (parent)
          parent_toplevel = parent->display_server.zxdg_toplevel_v6;
        else
          parent_toplevel = NULL;

        zxdg_toplevel_v6_set_parent (toplevel->display_server.zxdg_toplevel_v6, parent_toplevel);
        break;
      }
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_toplevel_sync_parent_of_imported (GdkWaylandToplevel *toplevel)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);

  if (!impl->display_server.wl_surface)
    return;

  if (!is_realized_toplevel (impl))
    return;

  if (toplevel->imported_transient_for)
    zxdg_imported_v1_set_parent_of (toplevel->imported_transient_for,
                                    impl->display_server.wl_surface);
  else if (toplevel->imported_transient_for_v2)
    zxdg_imported_v2_set_parent_of (toplevel->imported_transient_for_v2,
                                    impl->display_server.wl_surface);
}

static void
gdk_wayland_toplevel_sync_title (GdkWaylandToplevel *toplevel)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));

  if (!is_realized_toplevel (impl))
    return;

  if (!toplevel->title)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_title (toplevel->display_server.xdg_toplevel, toplevel->title);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_title (toplevel->display_server.zxdg_toplevel_v6, toplevel->title);
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gdk_wayland_toplevel_compute_size (GdkSurface *surface)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_toplevel);
  GdkDisplay *display = gdk_surface_get_display (surface);
  int bounds_width, bounds_height;
  GdkToplevelSize size;
  GdkToplevelLayout *layout;
  GdkGeometry geometry;
  GdkSurfaceHints mask;

  if (!wayland_surface->next_layout.surface_geometry_dirty)
    return FALSE;

  if (wayland_toplevel->has_bounds)
    {
      bounds_width = wayland_toplevel->bounds_width;
      bounds_height = wayland_toplevel->bounds_height;
    }
  else
    {
      GdkMonitor *monitor;
      GListModel *monitors;
      GdkRectangle monitor_geometry, display_geometry = { 0 };
      guint i;

      monitors = gdk_display_get_monitors (display);

      for (i = 0; i < g_list_model_get_n_items (monitors); i++)
        {
          monitor = g_list_model_get_item (monitors, i);
          gdk_monitor_get_geometry (monitor, &monitor_geometry);
          gdk_rectangle_union (&display_geometry, &monitor_geometry, &display_geometry);
          g_object_unref (monitor);
        }

      bounds_width = display_geometry.width;
      bounds_height = display_geometry.height;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (GDK_TOPLEVEL (surface), &size);
  g_warn_if_fail (size.width > 0);
  g_warn_if_fail (size.height > 0);

  layout = wayland_toplevel->layout;
  if (gdk_toplevel_layout_get_resizable (layout))
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

  gdk_wayland_toplevel_set_geometry_hints (wayland_toplevel, &geometry, mask);

  if (size.shadow.is_valid)
    {
      wayland_surface->shadow_left = size.shadow.left;
      wayland_surface->shadow_right = size.shadow.right;
      wayland_surface->shadow_top = size.shadow.top;
      wayland_surface->shadow_bottom = size.shadow.bottom;
    }

  if (wayland_surface->next_layout.configured_width > 0 &&
      wayland_surface->next_layout.configured_height > 0)
    {
      int width, height;

      width = wayland_surface->next_layout.configured_width +
        wayland_surface->shadow_left + wayland_surface->shadow_right;
      height = wayland_surface->next_layout.configured_height +
        wayland_surface->shadow_top + wayland_surface->shadow_bottom;

      if (wayland_toplevel->next_layout.should_constrain)
        {
          gdk_surface_constrain_size (&wayland_toplevel->geometry_hints,
                                      wayland_toplevel->geometry_mask,
                                      width, height,
                                      &width, &height);
        }
      gdk_wayland_surface_update_size (surface, width, height, &wayland_surface->scale);

      if (!wayland_toplevel->next_layout.size_is_fixed)
        {
          wayland_toplevel->next_layout.should_constrain = FALSE;
          wayland_surface->next_layout.configured_width = 0;
          wayland_surface->next_layout.configured_height = 0;
        }
    }
  else
    {
      int width, height;

      width = size.width;
      height = size.height;
      gdk_surface_constrain_size (&geometry, mask,
                                  width, height,
                                  &width, &height);
      gdk_wayland_surface_update_size (surface, width, height, &wayland_surface->scale);
    }

  wayland_surface->next_layout.surface_geometry_dirty = FALSE;

  return FALSE;
}

static gboolean
has_per_edge_tiling_info (GdkToplevelState state)
{
  return state & (GDK_TOPLEVEL_STATE_TOP_TILED |
                  GDK_TOPLEVEL_STATE_RIGHT_TILED |
                  GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                  GDK_TOPLEVEL_STATE_LEFT_TILED);
}

static GdkToplevelState
infer_edge_constraints (GdkToplevelState state)
{
  if (state & (GDK_TOPLEVEL_STATE_MAXIMIZED | GDK_TOPLEVEL_STATE_FULLSCREEN))
    return state;

  if (!(state & GDK_TOPLEVEL_STATE_TILED) || !has_per_edge_tiling_info (state))
    return state |
           GDK_TOPLEVEL_STATE_TOP_RESIZABLE |
           GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE |
           GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE |
           GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;

  if (!(state & GDK_TOPLEVEL_STATE_TOP_TILED))
    state |= GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
  if (!(state & GDK_TOPLEVEL_STATE_RIGHT_TILED))
    state |= GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE;
  if (!(state & GDK_TOPLEVEL_STATE_BOTTOM_TILED))
    state |= GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
  if (!(state & GDK_TOPLEVEL_STATE_LEFT_TILED))
    state |= GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;

  return state;
}

static gboolean
supports_native_edge_constraints (GdkWaylandToplevel*toplevel)
{
  struct gtk_surface1 *gtk_surface = toplevel->display_server.gtk_surface;
  if (!gtk_surface)
    return FALSE;
  return gtk_surface1_get_version (gtk_surface) >= GTK_SURFACE1_CONFIGURE_EDGES_SINCE_VERSION;
}

static void
gdk_wayland_toplevel_handle_configure (GdkWaylandSurface *wayland_surface)
{
  GdkSurface *surface = GDK_SURFACE (wayland_surface);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (wayland_surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkToplevelState new_state;
  int width, height;
  gboolean is_resizing;
  gboolean fixed_size;
  gboolean was_fixed_size;
  gboolean saved_size;

  new_state = wayland_toplevel->pending.state;
  wayland_toplevel->pending.state = 0;

  if (!supports_native_edge_constraints (wayland_toplevel))
    new_state = infer_edge_constraints (new_state);

  is_resizing = wayland_toplevel->pending.is_resizing;
  wayland_toplevel->pending.is_resizing = FALSE;

  if (wayland_toplevel->pending.has_bounds)
    {
      wayland_toplevel->bounds_width = wayland_toplevel->pending.bounds_width;
      wayland_toplevel->bounds_height = wayland_toplevel->pending.bounds_height;
      wayland_toplevel->has_bounds = TRUE;
    }

  fixed_size =
    new_state & (GDK_TOPLEVEL_STATE_MAXIMIZED |
                 GDK_TOPLEVEL_STATE_FULLSCREEN |
                 GDK_TOPLEVEL_STATE_TILED) ||
    is_resizing;

  was_fixed_size =
    surface->state & (GDK_TOPLEVEL_STATE_MAXIMIZED |
                      GDK_TOPLEVEL_STATE_FULLSCREEN |
                      GDK_TOPLEVEL_STATE_TILED);

  width = wayland_toplevel->pending.width;
  height = wayland_toplevel->pending.height;

  saved_size = (width == 0 && height == 0);
  /* According to xdg_shell, an xdg_surface.configure with size 0x0
   * should be interpreted as that it is up to the client to set a
   * size.
   *
   * When transitioning from maximize or fullscreen state, this means
   * the client should configure its size back to what it was before
   * being maximize or fullscreen.
   */
  if (saved_size && !fixed_size && was_fixed_size)
    {
      width = wayland_toplevel->saved_width;
      height = wayland_toplevel->saved_height;
    }

  if (width > 0 && height > 0)
    {
      if (!saved_size)
        {
          wayland_toplevel->next_layout.should_constrain = TRUE;

          /* Save size for next time we get 0x0 */
          gdk_wayland_toplevel_save_size (wayland_toplevel);
        }
      else if (is_resizing)
        {
          wayland_toplevel->next_layout.should_constrain = TRUE;
        }
      else
        {
          wayland_toplevel->next_layout.should_constrain = FALSE;
        }

      wayland_toplevel->next_layout.size_is_fixed = fixed_size;
      wayland_surface->next_layout.configured_width = width;
      wayland_surface->next_layout.configured_height = height;
    }
  else
    {
      wayland_toplevel->next_layout.should_constrain = FALSE;
      wayland_toplevel->next_layout.size_is_fixed = FALSE;
      wayland_surface->next_layout.configured_width = 0;
      wayland_surface->next_layout.configured_height = 0;
    }

  wayland_surface->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS,
                     "configure, surface %p %dx%d,%s%s%s%s",
                     surface, width, height,
                     (new_state & GDK_TOPLEVEL_STATE_FULLSCREEN) ? " fullscreen" : "",
                     (new_state & GDK_TOPLEVEL_STATE_MAXIMIZED) ? " maximized" : "",
                     (new_state & GDK_TOPLEVEL_STATE_FOCUSED) ? " focused" : "",
                     (new_state & GDK_TOPLEVEL_STATE_TILED) ? " tiled" : "");

  gdk_surface_queue_state_change (surface, ~0 & ~new_state, new_state);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_ack_configure (wayland_surface->display_server.xdg_surface,
                                 wayland_surface->pending.serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_ack_configure (wayland_surface->display_server.zxdg_surface_v6,
                                     wayland_surface->pending.serial);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
xdg_toplevel_configure (void                *data,
                        struct xdg_toplevel *xdg_toplevel,
                        int32_t              width,
                        int32_t              height,
                        struct wl_array     *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  uint32_t *p;
  GdkToplevelState pending_state = 0;

  toplevel->pending.is_resizing = FALSE;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
          pending_state |= GDK_TOPLEVEL_STATE_FULLSCREEN;
          break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
          pending_state |= GDK_TOPLEVEL_STATE_MAXIMIZED;
          break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
          pending_state |= GDK_TOPLEVEL_STATE_FOCUSED;
          break;
        case XDG_TOPLEVEL_STATE_RESIZING:
          toplevel->pending.is_resizing = TRUE;
          break;
        case XDG_TOPLEVEL_STATE_TILED_TOP:
          pending_state |= (GDK_TOPLEVEL_STATE_TILED |
                            GDK_TOPLEVEL_STATE_TOP_TILED);
          break;
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
          pending_state |= (GDK_TOPLEVEL_STATE_TILED |
                            GDK_TOPLEVEL_STATE_RIGHT_TILED);
          break;
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
          pending_state |= (GDK_TOPLEVEL_STATE_TILED |
                            GDK_TOPLEVEL_STATE_BOTTOM_TILED);
          break;
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
          pending_state |= (GDK_TOPLEVEL_STATE_TILED |
                            GDK_TOPLEVEL_STATE_LEFT_TILED);
          break;
#ifdef HAVE_TOPLEVEL_STATE_SUSPENDED
        case XDG_TOPLEVEL_STATE_SUSPENDED:
          pending_state |= GDK_TOPLEVEL_STATE_SUSPENDED;
          break;
#endif
        default:
          /* Unknown state */
          break;
        }
    }

  toplevel->pending.state |= pending_state;
  toplevel->pending.width = width;
  toplevel->pending.height = height;
}

static void
gdk_wayland_surface_handle_close (GdkSurface *surface)
{
  GdkDisplay *display;
  GdkEvent *event;

  display = gdk_surface_get_display (surface);

  GDK_DISPLAY_DEBUG (display, EVENTS, "close %p", surface);

  event = gdk_delete_event_new (surface);

  _gdk_wayland_display_deliver_event (display, event);
}

static void
xdg_toplevel_close (void                *data,
                    struct xdg_toplevel *xdg_toplevel)
{
  gdk_wayland_surface_handle_close (GDK_SURFACE (data));
}

static void
xdg_toplevel_configure_bounds (void                *data,
                               struct xdg_toplevel *xdg_toplevel,
                               int32_t              width,
                               int32_t              height)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);

  toplevel->pending.bounds_width = width;
  toplevel->pending.bounds_height = height;
  toplevel->pending.has_bounds = TRUE;
}

static void
xdg_toplevel_wm_capabilities (void                *data,
                              struct xdg_toplevel *xdg_toplevel,
                              struct wl_array     *capabilities)
{
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  xdg_toplevel_configure,
  xdg_toplevel_close,
  xdg_toplevel_configure_bounds,
  xdg_toplevel_wm_capabilities,
};

static void
create_xdg_toplevel_resources (GdkWaylandToplevel *toplevel)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);

  toplevel->display_server.xdg_toplevel =
    xdg_surface_get_toplevel (impl->display_server.xdg_surface);
  xdg_toplevel_add_listener (toplevel->display_server.xdg_toplevel,
                             &xdg_toplevel_listener,
                             toplevel);
}

static void
zxdg_toplevel_v6_configure (void                    *data,
                            struct zxdg_toplevel_v6 *xdg_toplevel,
                            int32_t                  width,
                            int32_t                  height,
                            struct wl_array         *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  uint32_t *p;
  GdkToplevelState pending_state = 0;

  toplevel->pending.is_resizing = FALSE;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
          pending_state |= GDK_TOPLEVEL_STATE_FULLSCREEN;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
          pending_state |= GDK_TOPLEVEL_STATE_MAXIMIZED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
          pending_state |= GDK_TOPLEVEL_STATE_FOCUSED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
          toplevel->pending.is_resizing = TRUE;
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  toplevel->pending.state |= pending_state;
  toplevel->pending.width = width;
  toplevel->pending.height = height;
}

static void
zxdg_toplevel_v6_close (void                    *data,
                        struct zxdg_toplevel_v6 *xdg_toplevel)
{
  gdk_wayland_surface_handle_close (GDK_SURFACE (data));
}

static const struct zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
  zxdg_toplevel_v6_configure,
  zxdg_toplevel_v6_close,
};

static void
create_zxdg_toplevel_v6_resources (GdkWaylandToplevel *toplevel)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);

  toplevel->display_server.zxdg_toplevel_v6 =
    zxdg_surface_v6_get_toplevel (impl->display_server.zxdg_surface_v6);
  zxdg_toplevel_v6_add_listener (toplevel->display_server.zxdg_toplevel_v6,
                                 &zxdg_toplevel_v6_listener,
                                 toplevel);
}

static void
gdk_wayland_surface_create_xdg_toplevel (GdkWaylandToplevel *wayland_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (wayland_toplevel);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_toplevel);
  const char *app_id;

  gdk_surface_freeze_updates (surface);
  gdk_wayland_surface_create_xdg_surface_resources (surface);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      create_xdg_toplevel_resources (wayland_toplevel);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      create_zxdg_toplevel_v6_resources (wayland_toplevel);
      break;
    default:
      g_assert_not_reached ();
    }

  gdk_wayland_toplevel_sync_parent (wayland_toplevel);
  gdk_wayland_toplevel_sync_parent_of_imported (wayland_toplevel);
  gdk_wayland_toplevel_sync_title (wayland_toplevel);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      if (wayland_toplevel->initial_state.set_flags & GDK_TOPLEVEL_STATE_MAXIMIZED)
        xdg_toplevel_set_maximized (wayland_toplevel->display_server.xdg_toplevel);
      if (wayland_toplevel->initial_state.set_flags & GDK_TOPLEVEL_STATE_MINIMIZED)
        xdg_toplevel_set_minimized (wayland_toplevel->display_server.xdg_toplevel);
      if (wayland_toplevel->initial_state.set_flags & GDK_TOPLEVEL_STATE_FULLSCREEN)
        xdg_toplevel_set_fullscreen (wayland_toplevel->display_server.xdg_toplevel,
                                     wayland_toplevel->initial_fullscreen_output);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      if (wayland_toplevel->initial_state.set_flags & GDK_TOPLEVEL_STATE_MAXIMIZED)
        zxdg_toplevel_v6_set_maximized (wayland_toplevel->display_server.zxdg_toplevel_v6);
      if (wayland_toplevel->initial_state.set_flags & GDK_TOPLEVEL_STATE_MINIMIZED)
        zxdg_toplevel_v6_set_minimized (wayland_toplevel->display_server.zxdg_toplevel_v6);
      if (wayland_toplevel->initial_state.set_flags & GDK_TOPLEVEL_STATE_FULLSCREEN)
        zxdg_toplevel_v6_set_fullscreen (wayland_toplevel->display_server.zxdg_toplevel_v6,
                                         wayland_toplevel->initial_fullscreen_output);
      break;
    default:
      g_assert_not_reached ();
    }

  wayland_toplevel->initial_fullscreen_output = NULL;

  app_id = wayland_toplevel->application.application_id;
  if (app_id == NULL)
    app_id = g_get_prgname ();

  if (app_id == NULL)
    app_id = "GTK Application";

  gdk_wayland_toplevel_set_application_id (GDK_TOPLEVEL (wayland_toplevel), app_id);

  maybe_set_gtk_surface_dbus_properties (wayland_toplevel);
  if (!maybe_set_xdg_dialog_modal (wayland_toplevel))
    maybe_set_gtk_surface_modal (wayland_toplevel);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "Wayland surface commit", NULL);
  wl_surface_commit (wayland_surface->display_server.wl_surface);
}

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
gdk_wayland_toplevel_init (GdkWaylandToplevel *toplevel)
{
  toplevel->initial_fullscreen_output = NULL;
  toplevel->shortcuts_inhibitors = g_hash_table_new (NULL, NULL);
  toplevel->saved_width = -1;
  toplevel->saved_height = -1;

  toplevel->title = g_strdup (get_default_title ());
}

static void
gtk_surface_configure (void                *data,
                       struct gtk_surface1 *gtk_surface,
                       struct wl_array     *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  GdkToplevelState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case GTK_SURFACE1_STATE_TILED:
          new_state |= GDK_TOPLEVEL_STATE_TILED;
          break;

        /* Since v2 */
        case GTK_SURFACE1_STATE_TILED_TOP:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_TOP_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_RIGHT:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_RIGHT_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_BOTTOM:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_BOTTOM_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_LEFT:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_LEFT_TILED);
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  toplevel->pending.state |= new_state;
}

static void
gtk_surface_configure_edges (void                *data,
                             struct gtk_surface1 *gtk_surface,
                             struct wl_array     *edge_constraints)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  GdkToplevelState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, edge_constraints)
    {
      uint32_t constraint = *p;

      switch (constraint)
        {
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_TOP:
          new_state |= GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_RIGHT:
          new_state |= GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_BOTTOM:
          new_state |= GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_LEFT:
          new_state |= GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  toplevel->pending.state |= new_state;
}

static const struct gtk_surface1_listener gtk_surface_listener = {
  gtk_surface_configure,
  gtk_surface_configure_edges
};

static void
gdk_wayland_toplevel_init_gtk_surface (GdkWaylandToplevel *wayland_toplevel)
{
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_toplevel);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (wayland_toplevel)));

  if (wayland_toplevel->display_server.gtk_surface != NULL)
    return;
  if (!is_realized_toplevel (wayland_surface))
    return;
  if (display->gtk_shell == NULL)
    return;

  wayland_toplevel->display_server.gtk_surface =
    gtk_shell1_get_gtk_surface (display->gtk_shell,
                                wayland_surface->display_server.wl_surface);
  wl_proxy_set_queue ((struct wl_proxy *) wayland_toplevel->display_server.gtk_surface,
                      wayland_surface->event_queue);
  gdk_wayland_toplevel_set_geometry_hints (wayland_toplevel,
                                           &wayland_toplevel->geometry_hints,
                                           wayland_toplevel->geometry_mask);
  gtk_surface1_add_listener (wayland_toplevel->display_server.gtk_surface,
                             &gtk_surface_listener,
                             wayland_surface);
}

static void
gdk_wayland_toplevel_set_title (GdkWaylandToplevel *toplevel,
                                const char         *title)
{
  const char *end;
  gsize title_length;

  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (GDK_SURFACE (toplevel)))
    return;

  if (g_strcmp0 (toplevel->title, title) == 0)
    return;

  g_free (toplevel->title);

  title_length = MIN (strlen (title), MAX_WL_BUFFER_SIZE);
  if (g_utf8_validate (title, title_length, &end))
    {
      toplevel->title = g_malloc (end - title + 1);
      memcpy (toplevel->title, title, end - title);
      toplevel->title[end - title] = '\0';
    }
  else
    {
      toplevel->title = g_utf8_make_valid (title, title_length);
      g_warning ("Invalid utf8 passed to gdk_surface_set_title: '%s'", title);
    }

  gdk_wayland_toplevel_sync_title (toplevel);
}

static void
gdk_wayland_toplevel_set_startup_id (GdkWaylandToplevel *toplevel,
                                     const char         *startup_id)
{
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandSurface *surface = GDK_WAYLAND_SURFACE (toplevel);
  char *free_me = NULL;

  if (!startup_id)
    {
      free_me = g_steal_pointer (&display->startup_notification_id);
      startup_id = free_me;
    }

  if (display->xdg_activation && startup_id)
    xdg_activation_v1_activate (display->xdg_activation,
                                startup_id,
                                surface->display_server.wl_surface);

  g_free (free_me);
}

static void
maybe_set_gtk_surface_modal (GdkWaylandToplevel *wayland_toplevel)
{
  gdk_wayland_toplevel_init_gtk_surface (wayland_toplevel);
  if (wayland_toplevel->display_server.gtk_surface == NULL)
    return;

  if (GDK_SURFACE (wayland_toplevel)->modal_hint)
    gtk_surface1_set_modal (wayland_toplevel->display_server.gtk_surface);
  else
    gtk_surface1_unset_modal (wayland_toplevel->display_server.gtk_surface);

}

static gboolean
maybe_set_xdg_dialog_modal (GdkWaylandToplevel *wayland_toplevel)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (wayland_toplevel)));

  if (!display_wayland->xdg_wm_dialog)
    return FALSE;
  if (!is_realized_toplevel (GDK_WAYLAND_SURFACE (wayland_toplevel)))
    return FALSE;

  if (!wayland_toplevel->display_server.xdg_dialog)
    {
      wayland_toplevel->display_server.xdg_dialog =
        xdg_wm_dialog_v1_get_xdg_dialog (display_wayland->xdg_wm_dialog,
                                         wayland_toplevel->display_server.xdg_toplevel);
    }

  if (GDK_SURFACE (wayland_toplevel)->modal_hint)
    xdg_dialog_v1_set_modal (wayland_toplevel->display_server.xdg_dialog);
  else
    xdg_dialog_v1_unset_modal (wayland_toplevel->display_server.xdg_dialog);

  return TRUE;
}

static void
gdk_wayland_toplevel_set_modal_hint (GdkWaylandToplevel *wayland_toplevel,
                                     gboolean            modal)
{
  GDK_SURFACE (wayland_toplevel)->modal_hint = modal;

  if (!maybe_set_xdg_dialog_modal (wayland_toplevel))
    maybe_set_gtk_surface_modal (wayland_toplevel);
}

void
gdk_wayland_toplevel_set_geometry_hints (GdkWaylandToplevel *toplevel,
                                         const GdkGeometry  *geometry,
                                         GdkSurfaceHints     geom_mask)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  int min_width, min_height;
  int max_width, max_height;

  if (GDK_SURFACE_DESTROYED (toplevel))
    return;

  if (!geometry)
    {
      geometry = &toplevel->geometry_hints;
      geom_mask = toplevel->geometry_mask;
    }

  toplevel->geometry_hints = *geometry;
  toplevel->geometry_mask = geom_mask;

  if (!is_realized_toplevel (impl))
    return;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      min_width = MAX (0, (geometry->min_width -
                           (impl->shadow_left + impl->shadow_right)));
      min_height = MAX (0, (geometry->min_height -
                            (impl->shadow_top + impl->shadow_bottom)));
    }
  else
    {
      min_width = 0;
      min_height = 0;
    }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      max_width = MAX (0, (geometry->max_width -
                           (impl->shadow_left + impl->shadow_right)));
      max_height = MAX (0, (geometry->max_height -
                            (impl->shadow_top + impl->shadow_bottom)));
    }
  else
    {
      max_width = 0;
      max_height = 0;
    }

  if (toplevel->last_sent_geometry_hints.min_width == min_width &&
      toplevel->last_sent_geometry_hints.min_height == min_height &&
      toplevel->last_sent_geometry_hints.max_width == max_width &&
      toplevel->last_sent_geometry_hints.max_height == max_height)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_min_size (toplevel->display_server.xdg_toplevel,
                                 min_width, min_height);
      xdg_toplevel_set_max_size (toplevel->display_server.xdg_toplevel,
                                 max_width, max_height);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_min_size (toplevel->display_server.zxdg_toplevel_v6,
                                     min_width, min_height);
      zxdg_toplevel_v6_set_max_size (toplevel->display_server.zxdg_toplevel_v6,
                                     max_width, max_height);
      break;
    default:
      g_assert_not_reached ();
    }

  toplevel->last_sent_geometry_hints.min_width = min_width;
  toplevel->last_sent_geometry_hints.min_height = min_height;
  toplevel->last_sent_geometry_hints.max_width = max_width;
  toplevel->last_sent_geometry_hints.max_height = max_height;
}

static gboolean
check_transient_for_loop (GdkWaylandToplevel *toplevel,
                          GdkWaylandToplevel *parent)
{
  while (parent)
    {
      if (parent->transient_for == toplevel)
        return TRUE;
      parent = parent->transient_for;
    }
  return FALSE;
}

static void
gdk_wayland_toplevel_set_transient_for (GdkWaylandToplevel *toplevel,
                                        GdkSurface         *parent)
{
  g_return_if_fail (!parent || GDK_IS_WAYLAND_TOPLEVEL (parent));
  g_return_if_fail (!parent ||
                    gdk_surface_get_display (GDK_SURFACE (toplevel)) == gdk_surface_get_display (parent));

  if (parent)
    {
      GdkWaylandToplevel *parent_toplevel = GDK_WAYLAND_TOPLEVEL (parent);

      if (check_transient_for_loop (toplevel, parent_toplevel))
        {
          g_warning ("Setting %p transient for %p would create a loop",
                     toplevel, parent);
          return;
        }
    }

  unset_transient_for_exported (toplevel);

  if (parent)
    toplevel->transient_for = GDK_WAYLAND_TOPLEVEL (parent);
  else
    toplevel->transient_for = NULL;

  gdk_wayland_toplevel_sync_parent (toplevel);
}

#define LAST_PROP 1

static void
gdk_wayland_toplevel_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_wayland_toplevel_set_title (toplevel, g_value_get_string (value));
      g_object_notify_by_pspec (object, pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      gdk_wayland_toplevel_set_startup_id (toplevel, g_value_get_string (value));
      g_object_notify_by_pspec (object, pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_wayland_toplevel_set_transient_for (toplevel, g_value_get_object (value));
      g_object_notify_by_pspec (object, pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      gdk_wayland_toplevel_set_modal_hint (toplevel, g_value_get_boolean (value));
      g_object_notify_by_pspec (object, pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_wayland_toplevel_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      g_value_set_string (value, toplevel->title);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, toplevel->transient_for);
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
gdk_wayland_toplevel_finalize (GObject *object)
{
  GdkWaylandToplevel *self = GDK_WAYLAND_TOPLEVEL (object);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (object)));

  display_wayland->toplevels = g_list_remove (display_wayland->toplevels, self);

  g_free (self->application.application_id);
  g_free (self->application.app_menu_path);
  g_free (self->application.menubar_path);
  g_free (self->application.window_object_path);
  g_free (self->application.application_object_path);
  g_free (self->application.unique_bus_name);

  g_free (self->title);
  g_clear_pointer (&self->shortcuts_inhibitors, g_hash_table_unref);

  G_OBJECT_CLASS (gdk_wayland_toplevel_parent_class)->finalize (object);
}

static void
gdk_wayland_toplevel_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkFrameClock *frame_clock;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  display_wayland->toplevels = g_list_prepend (display_wayland->toplevels, object);

  G_OBJECT_CLASS (gdk_wayland_toplevel_parent_class)->constructed (object);
}

static void
gdk_wayland_toplevel_class_init (GdkWaylandToplevelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (class);
  GdkWaylandSurfaceClass *wayland_surface_class = GDK_WAYLAND_SURFACE_CLASS (class);

  object_class->get_property = gdk_wayland_toplevel_get_property;
  object_class->set_property = gdk_wayland_toplevel_set_property;
  object_class->finalize = gdk_wayland_toplevel_finalize;
  object_class->constructed = gdk_wayland_toplevel_constructed;

  surface_class->compute_size = gdk_wayland_toplevel_compute_size;

  wayland_surface_class->handle_configure = gdk_wayland_toplevel_handle_configure;
  wayland_surface_class->hide_surface = gdk_wayland_toplevel_hide_surface;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
synthesize_initial_surface_state (GdkWaylandToplevel *wayland_toplevel,
                                  GdkToplevelState    unset_flags,
                                  GdkToplevelState    set_flags)
{
  wayland_toplevel->initial_state.unset_flags |= unset_flags;
  wayland_toplevel->initial_state.set_flags &= ~unset_flags;

  wayland_toplevel->initial_state.set_flags |= set_flags;
  wayland_toplevel->initial_state.unset_flags &= ~set_flags;
}

static gboolean
gdk_wayland_toplevel_minimize (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  GdkWaylandDisplay *display_wayland;

  if (GDK_SURFACE_DESTROYED (surface))
    return TRUE;

  if (!is_realized_toplevel (impl))
    return TRUE;

  /* FIXME: xdg_toplevel does not come with a minimized state that we can
   * query or get notified of. This means we cannot implement the full
   * GdkSurface API, and our state will not reflect minimization.
   */
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_minimized (wayland_toplevel->display_server.xdg_toplevel);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_minimized (wayland_toplevel->display_server.zxdg_toplevel_v6);
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
gdk_wayland_toplevel_maximize (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  gdk_wayland_toplevel_save_size (wayland_toplevel);

  if (is_realized_toplevel (wayland_surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_maximized (wayland_toplevel->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_maximized (wayland_toplevel->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (wayland_toplevel, 0, GDK_TOPLEVEL_STATE_MAXIMIZED);
    }
}

static void
gdk_wayland_toplevel_unmaximize (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (is_realized_toplevel (wayland_surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_maximized (wayland_toplevel->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_maximized (wayland_toplevel->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (wayland_toplevel, GDK_TOPLEVEL_STATE_MAXIMIZED, 0);
    }
}

static void
gdk_wayland_toplevel_fullscreen_on_monitor (GdkWaylandToplevel *wayland_toplevel,
                                            GdkMonitor         *monitor)
{
  GdkSurface *surface = GDK_SURFACE (wayland_toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (surface);
  struct wl_output *output = ((GdkWaylandMonitor *)monitor)->output;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  gdk_wayland_toplevel_save_size (wayland_toplevel);

  if (is_realized_toplevel (wayland_surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (wayland_toplevel->display_server.xdg_toplevel, output);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (wayland_toplevel->display_server.zxdg_toplevel_v6, output);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (wayland_toplevel, 0, GDK_TOPLEVEL_STATE_FULLSCREEN);
      wayland_toplevel->initial_fullscreen_output = output;
    }
}

static void
gdk_wayland_toplevel_fullscreen (GdkWaylandToplevel *wayland_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (wayland_toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_toplevel);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  wayland_toplevel->initial_fullscreen_output = NULL;

  gdk_wayland_toplevel_save_size (wayland_toplevel);

  if (is_realized_toplevel (wayland_surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (wayland_toplevel->display_server.xdg_toplevel, NULL);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (wayland_toplevel->display_server.zxdg_toplevel_v6, NULL);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (wayland_toplevel, 0, GDK_TOPLEVEL_STATE_FULLSCREEN);
    }
}

static void
gdk_wayland_toplevel_unfullscreen (GdkWaylandToplevel *wayland_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (wayland_toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_toplevel);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  wayland_toplevel->initial_fullscreen_output = NULL;

  if (is_realized_toplevel (wayland_surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_fullscreen (wayland_toplevel->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_fullscreen (wayland_toplevel->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (wayland_toplevel, GDK_TOPLEVEL_STATE_FULLSCREEN, 0);
    }
}

static void
gdk_wayland_toplevel_show (GdkWaylandToplevel *toplevel)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);

  if (impl->mapped)
    return;

  gdk_wayland_surface_create_xdg_toplevel (toplevel);

  impl->mapped = TRUE;
}

static void
gdk_wayland_toplevel_present (GdkToplevel       *toplevel,
                              GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (toplevel);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  gboolean pending_configure = FALSE;
  gboolean maximize;
  gboolean fullscreen;

  if (gdk_toplevel_layout_get_maximized (layout, &maximize))
    {
      if (maximize)
        gdk_wayland_toplevel_maximize (toplevel);
      else
        gdk_wayland_toplevel_unmaximize (toplevel);
      pending_configure = TRUE;
    }

  if (gdk_toplevel_layout_get_fullscreen (layout, &fullscreen))
    {
      if (fullscreen)
        {
          GdkMonitor *monitor;

          monitor = gdk_toplevel_layout_get_fullscreen_monitor (layout);
          if (monitor)
            gdk_wayland_toplevel_fullscreen_on_monitor (wayland_toplevel, monitor);
          else
            gdk_wayland_toplevel_fullscreen (wayland_toplevel);
        }
      else
        {
          gdk_wayland_toplevel_unfullscreen (wayland_toplevel);
        }
      pending_configure = TRUE;
    }

  g_clear_pointer (&wayland_toplevel->layout, gdk_toplevel_layout_unref);
  wayland_toplevel->layout = gdk_toplevel_layout_copy (layout);

  gdk_wayland_toplevel_show (wayland_toplevel);

  if (!pending_configure)
    {
      wayland_surface->next_layout.surface_geometry_dirty = TRUE;
      gdk_surface_request_layout (surface);
    }
}

static gboolean
gdk_wayland_toplevel_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
inhibitor_active (void *data,
                  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
  GdkToplevel *toplevel = GDK_TOPLEVEL (data);
  GdkSurface *surface = GDK_SURFACE (toplevel);

  surface->shortcuts_inhibited = TRUE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
inhibitor_inactive (void *data,
                    struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
  GdkToplevel *toplevel = GDK_TOPLEVEL (data);
  GdkSurface *surface = GDK_SURFACE (toplevel);

  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static const struct zwp_keyboard_shortcuts_inhibitor_v1_listener
zwp_keyboard_shortcuts_inhibitor_listener = {
  inhibitor_active,
  inhibitor_inactive,
};

static struct zwp_keyboard_shortcuts_inhibitor_v1 *
gdk_wayland_toplevel_get_inhibitor (GdkWaylandToplevel *toplevel,
                                    GdkSeat            *gdk_seat)
{
  return g_hash_table_lookup (toplevel->shortcuts_inhibitors, gdk_seat);
}

void
gdk_wayland_surface_inhibit_shortcuts (GdkSurface *surface,
                                       GdkSeat    *gdk_seat)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_surface *wl_surface = impl->display_server.wl_surface;
  struct wl_seat *seat = gdk_wayland_seat_get_wl_seat (gdk_seat);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;
  GdkWaylandToplevel *toplevel;

  if (display->keyboard_shortcuts_inhibit == NULL)
    return;

  if (!is_realized_toplevel (GDK_WAYLAND_SURFACE (surface)))
    return;

  toplevel = GDK_WAYLAND_TOPLEVEL (surface);

  if (gdk_wayland_toplevel_get_inhibitor (toplevel, gdk_seat))
    return; /* Already inhibited */

  inhibitor =
      zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts (
          display->keyboard_shortcuts_inhibit, wl_surface, seat);

  g_hash_table_insert (toplevel->shortcuts_inhibitors, gdk_seat, inhibitor);
}

void
gdk_wayland_surface_restore_shortcuts (GdkSurface *surface,
                                       GdkSeat    *gdk_seat)
{
  GdkWaylandToplevel *toplevel;
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  if (!is_realized_toplevel (impl))
    return;

  toplevel = GDK_WAYLAND_TOPLEVEL (impl);

  inhibitor = gdk_wayland_toplevel_get_inhibitor (toplevel, gdk_seat);
  if (inhibitor == NULL)
    return; /* Not inhibitted */

  zwp_keyboard_shortcuts_inhibitor_v1_destroy (inhibitor);
  g_hash_table_remove (toplevel->shortcuts_inhibitors, gdk_seat);
}

static void
gdk_wayland_toplevel_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                               GdkEvent    *event)
{
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkSeat *gdk_seat;

  if (surface->shortcuts_inhibited)
    return;

  gdk_seat = gdk_surface_get_seat_from_event (surface, event);
  gdk_wayland_surface_inhibit_shortcuts (surface, gdk_seat);
  inhibitor = gdk_wayland_toplevel_get_inhibitor (wayland_toplevel, gdk_seat);
  if (!inhibitor)
    return;

  surface->current_shortcuts_inhibited_seat = gdk_seat;
  zwp_keyboard_shortcuts_inhibitor_v1_add_listener
    (inhibitor, &zwp_keyboard_shortcuts_inhibitor_listener, toplevel);
}

static void
gdk_wayland_toplevel_restore_system_shortcuts (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);

  gdk_wayland_surface_restore_shortcuts (surface, surface->current_shortcuts_inhibited_seat);
  surface->current_shortcuts_inhibited_seat = NULL;
  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
xdg_exported_handle_v1 (void                    *data,
                        struct zxdg_exported_v1 *zxdg_exported_v1,
                        const char              *handle)
{
  GTask *task = G_TASK (data);
  GdkWaylandExported *exported = (GdkWaylandExported *)g_task_get_task_data (task);

  exported->handle = g_strdup (handle);
  g_task_return_pointer (task, g_strdup (handle), g_free);
  g_object_unref (task);
}

static const struct zxdg_exported_v1_listener xdg_exported_listener_v1 = {
  xdg_exported_handle_v1
};

static void
xdg_exported_handle_v2 (void                    *data,
                        struct zxdg_exported_v2 *zxdg_exported_v2,
                        const char              *handle)
{
  GTask *task = G_TASK (data);
  GdkWaylandExported *exported = (GdkWaylandExported *)g_task_get_task_data (task);

  exported->handle = g_strdup (handle);
  g_task_return_pointer (task, g_strdup (handle), g_free);
  g_object_unref (task);
}

static const struct zxdg_exported_v2_listener xdg_exported_listener_v2 = {
  xdg_exported_handle_v2
};

static void
gdk_wayland_toplevel_real_export_handle (GdkToplevel          *toplevel,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GTask *task;

  task = g_task_new (toplevel, cancellable, callback, user_data);

  if (display_wayland->xdg_exporter_v2)
    {
      GdkWaylandExported *exported = g_new0 (GdkWaylandExported, 1);
      exported->xdg_exported_v2 =
        zxdg_exporter_v2_export_toplevel (display_wayland->xdg_exporter_v2,
                                          gdk_wayland_surface_get_wl_surface (surface));
      zxdg_exported_v2_add_listener (exported->xdg_exported_v2,
                                     &xdg_exported_listener_v2, task);

      wayland_toplevel->exported = g_list_prepend (wayland_toplevel->exported, exported);
      g_task_set_task_data (task, exported, NULL);
    }
  else if (display_wayland->xdg_exporter)
    {
      GdkWaylandExported *exported = g_new0 (GdkWaylandExported, 1);
      exported->xdg_exported =
        zxdg_exporter_v1_export (display_wayland->xdg_exporter,
                                 gdk_wayland_surface_get_wl_surface (surface));
      zxdg_exported_v1_add_listener (exported->xdg_exported,
                                     &xdg_exported_listener_v1, task);

      wayland_toplevel->exported = g_list_prepend (wayland_toplevel->exported, exported);
      g_task_set_task_data (task, exported, NULL);
    }
  else
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Exporting surface handles not supported");
      g_object_unref (task);
      return;
    }
}

static char *
gdk_wayland_toplevel_real_export_handle_finish (GdkToplevel   *toplevel,
                                                GAsyncResult  *result,
                                                GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
destroy_exported (GdkWaylandExported *exported)
{
  g_clear_pointer (&exported->handle, g_free);
  g_clear_pointer (&exported->xdg_exported_v2, zxdg_exported_v2_destroy);
  g_clear_pointer (&exported->xdg_exported, zxdg_exported_v1_destroy);
  g_free (exported);
}

static void
gdk_wayland_toplevel_real_unexport_handle (GdkToplevel *toplevel,
                                           const char  *handle)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  g_return_if_fail (handle != NULL);

  for (GList *l = wayland_toplevel->exported; l; l = l->next)
    {
      GdkWaylandExported *exported = l->data;

      if (exported->handle && strcmp (exported->handle, handle) == 0)
        {
          wayland_toplevel->exported = g_list_delete_link (wayland_toplevel->exported, l);
          destroy_exported (exported);
          return;
        }
    }

  g_warn_if_reached ();
}

static gboolean
gdk_wayland_toplevel_show_window_menu (GdkToplevel *toplevel,
                                       GdkEvent    *event)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkSeat *seat;
  struct wl_seat *wl_seat;
  double x, y;
  uint32_t serial;

  GdkEventType event_type = gdk_event_get_event_type (event);
  switch ((guint) event_type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  if (!is_realized_toplevel (impl))
    return FALSE;

  seat = gdk_event_get_seat (event);
  wl_seat = gdk_wayland_seat_get_wl_seat (seat);
  gdk_event_get_position (event, &x, &y);

  serial = _gdk_wayland_seat_get_implicit_grab_serial (seat,
                                                       gdk_event_get_device (event),
                                                       gdk_event_get_event_sequence (event));

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_show_window_menu (wayland_toplevel->display_server.xdg_toplevel,
                                     wl_seat, serial, x, y);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_show_window_menu (wayland_toplevel->display_server.zxdg_toplevel_v6,
                                         wl_seat, serial, x, y);
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
translate_gesture (GdkTitlebarGesture         gesture,
                   enum gtk_surface1_gesture *out_gesture)
{
  switch (gesture)
    {
    case GDK_TITLEBAR_GESTURE_DOUBLE_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_DOUBLE_CLICK;
      break;

    case GDK_TITLEBAR_GESTURE_RIGHT_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_RIGHT_CLICK;
      break;

    case GDK_TITLEBAR_GESTURE_MIDDLE_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_MIDDLE_CLICK;
      break;

    default:
      g_warning ("Not handling unknown titlebar gesture %u", gesture);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gdk_wayland_toplevel_titlebar_gesture (GdkToplevel        *toplevel,
                                       GdkTitlebarGesture  gesture)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  struct gtk_surface1 *gtk_surface = wayland_toplevel->display_server.gtk_surface;
  enum gtk_surface1_gesture gtk_gesture;
  GdkSeat *seat;
  struct wl_seat *wl_seat;
  uint32_t serial;

  if (!gtk_surface)
    return FALSE;

  if (gtk_surface1_get_version (gtk_surface) < GTK_SURFACE1_TITLEBAR_GESTURE_SINCE_VERSION)
    return FALSE;

  if (!translate_gesture (gesture, &gtk_gesture))
    return FALSE;

  seat = gdk_display_get_default_seat (surface->display);
  if (!seat)
    return FALSE;

  wl_seat = gdk_wayland_seat_get_wl_seat (seat);
  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (seat), NULL);

  gtk_surface1_titlebar_gesture (wayland_toplevel->display_server.gtk_surface,
                                 serial,
                                 wl_seat,
                                 gtk_gesture);

  return TRUE;
}

static gboolean
gdk_wayland_toplevel_supports_edge_constraints (GdkToplevel *toplevel)
{
  return TRUE;
}

static void
gdk_wayland_toplevel_begin_resize (GdkToplevel    *toplevel,
                                   GdkSurfaceEdge  edge,
                                   GdkDevice      *device,
                                   int             button,
                                   double          x,
                                   double          y,
                                   guint32         timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl;
  GdkWaylandToplevel *wayland_toplevel;
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t resize_edges, serial;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  switch (edge)
    {
    case GDK_SURFACE_EDGE_NORTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
      break;

    case GDK_SURFACE_EDGE_NORTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
      break;

    case GDK_SURFACE_EDGE_NORTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
      break;

    case GDK_SURFACE_EDGE_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
      break;

    case GDK_SURFACE_EDGE_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
      break;

    case GDK_SURFACE_EDGE_SOUTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
      break;

    case GDK_SURFACE_EDGE_SOUTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
      break;

    case GDK_SURFACE_EDGE_SOUTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
      break;

    default:
      g_warning ("gdk_toplevel_begin_resize: bad resize edge %d!", edge);
      return;
    }

  impl = GDK_WAYLAND_SURFACE (surface);
  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (impl))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (gdk_device_get_seat (device)),
                                                            &sequence);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_resize (wayland_toplevel->display_server.xdg_toplevel,
                           gdk_wayland_device_get_wl_seat (device),
                           serial, resize_edges);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_resize (wayland_toplevel->display_server.zxdg_toplevel_v6,
                               gdk_wayland_device_get_wl_seat (device),
                               serial, resize_edges);
      break;
    default:
      g_assert_not_reached ();
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);
}

static void
gdk_wayland_toplevel_begin_move (GdkToplevel *toplevel,
                                 GdkDevice   *device,
                                 int          button,
                                 double       x,
                                 double       y,
                                 guint32      timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl;
  GdkWaylandToplevel *wayland_toplevel;
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t serial;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_WAYLAND_SURFACE (surface);
  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (impl))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (gdk_device_get_seat (device)),
                                                            &sequence);
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_move (wayland_toplevel->display_server.xdg_toplevel,
                         gdk_wayland_device_get_wl_seat (device),
                         serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_move (wayland_toplevel->display_server.zxdg_toplevel_v6,
                             gdk_wayland_device_get_wl_seat (device),
                             serial);
      break;
    default:
      g_assert_not_reached ();
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);
}

static void
token_done (gpointer                        data,
            struct xdg_activation_token_v1 *provider,
            const char                     *token)
{
  char **token_out = data;

  *token_out = g_strdup (token);
}

static const struct xdg_activation_token_v1_listener token_listener = {
  token_done,
};

static void
gdk_wayland_toplevel_focus (GdkToplevel *toplevel,
                            guint32      timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (toplevel);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandSeat *seat =
    GDK_WAYLAND_SEAT (gdk_display_get_default_seat (display));
  gchar *startup_id = NULL;

  startup_id = g_steal_pointer (&display_wayland->startup_notification_id);

  if (seat && display_wayland->xdg_activation)
    {
      /* If the focus request does not have a startup ID associated, get a
       * new token to activate the window.
       */
      if (!startup_id)
        {
          struct xdg_activation_token_v1 *token;
          struct wl_event_queue *event_queue;
          struct wl_surface *wl_surface = NULL;
          GdkSurface *focus_surface;

          event_queue = wl_display_create_queue (display_wayland->wl_display);

          token = xdg_activation_v1_get_activation_token (display_wayland->xdg_activation);
          wl_proxy_set_queue ((struct wl_proxy *) token, event_queue);

          xdg_activation_token_v1_add_listener (token,
                                                &token_listener,
                                                &startup_id);
          xdg_activation_token_v1_set_serial (token,
                                              _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL),
                                              gdk_wayland_seat_get_wl_seat (GDK_SEAT (seat)));

          focus_surface = gdk_wayland_device_get_focus (gdk_seat_get_keyboard (GDK_SEAT (seat)));
          if (focus_surface)
            wl_surface = gdk_wayland_surface_get_wl_surface (focus_surface);
          if (wl_surface)
            xdg_activation_token_v1_set_surface (token, wl_surface);

          xdg_activation_token_v1_commit (token);

          while (startup_id == NULL)
            {
              gdk_wayland_display_dispatch_queue (GDK_DISPLAY (display_wayland),
                                                  event_queue);
            }

          xdg_activation_token_v1_destroy (token);
          wl_event_queue_destroy (event_queue);
        }

      xdg_activation_v1_activate (display_wayland->xdg_activation,
                                  startup_id,
                                  wayland_surface->display_server.wl_surface);
    }
  else if (wayland_toplevel->display_server.gtk_surface)
    {
      if (timestamp != GDK_CURRENT_TIME)
        gtk_surface1_present (wayland_toplevel->display_server.gtk_surface, timestamp);
      else if (startup_id && gtk_surface1_get_version (wayland_toplevel->display_server.gtk_surface) >= GTK_SURFACE1_REQUEST_FOCUS_SINCE_VERSION)
        gtk_surface1_request_focus (wayland_toplevel->display_server.gtk_surface,
                                    startup_id);
    }

  g_free (startup_id);
}

static void
gdk_wayland_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_wayland_toplevel_present;
  iface->minimize = gdk_wayland_toplevel_minimize;
  iface->lower = gdk_wayland_toplevel_lower;
  iface->focus = gdk_wayland_toplevel_focus;
  iface->show_window_menu = gdk_wayland_toplevel_show_window_menu;
  iface->titlebar_gesture = gdk_wayland_toplevel_titlebar_gesture;
  iface->supports_edge_constraints = gdk_wayland_toplevel_supports_edge_constraints;
  iface->inhibit_system_shortcuts = gdk_wayland_toplevel_inhibit_system_shortcuts;
  iface->restore_system_shortcuts = gdk_wayland_toplevel_restore_system_shortcuts;
  iface->begin_resize = gdk_wayland_toplevel_begin_resize;
  iface->begin_move = gdk_wayland_toplevel_begin_move;
  iface->export_handle = gdk_wayland_toplevel_real_export_handle;
  iface->export_handle_finish = gdk_wayland_toplevel_real_export_handle_finish;
  iface->unexport_handle = gdk_wayland_toplevel_real_unexport_handle;
}

/* }}} */
/* {{{ Private Toplevel API */

struct gtk_surface1 *
gdk_wayland_toplevel_get_gtk_surface (GdkWaylandToplevel *wayland_toplevel)
{
  return wayland_toplevel->display_server.gtk_surface;
}

static void
maybe_set_gtk_surface_dbus_properties (GdkWaylandToplevel *wayland_toplevel)
{
  if (wayland_toplevel->application.was_set)
    return;

  if (wayland_toplevel->application.application_id == NULL &&
      wayland_toplevel->application.app_menu_path == NULL &&
      wayland_toplevel->application.menubar_path == NULL &&
      wayland_toplevel->application.window_object_path == NULL &&
      wayland_toplevel->application.application_object_path == NULL &&
      wayland_toplevel->application.unique_bus_name == NULL)
    return;

  gdk_wayland_toplevel_init_gtk_surface (wayland_toplevel);
  if (wayland_toplevel->display_server.gtk_surface == NULL)
    return;

  gtk_surface1_set_dbus_properties (wayland_toplevel->display_server.gtk_surface,
                                    wayland_toplevel->application.application_id,
                                    wayland_toplevel->application.app_menu_path,
                                    wayland_toplevel->application.menubar_path,
                                    wayland_toplevel->application.window_object_path,
                                    wayland_toplevel->application.application_object_path,
                                    wayland_toplevel->application.unique_bus_name);
  wayland_toplevel->application.was_set = TRUE;
}

void
gdk_wayland_toplevel_set_dbus_properties (GdkToplevel *toplevel,
                                          const char  *application_id,
                                          const char  *app_menu_path,
                                          const char  *menubar_path,
                                          const char  *window_object_path,
                                          const char  *application_object_path,
                                          const char *unique_bus_name)
{
  GdkWaylandToplevel *wayland_toplevel;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));

  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  wayland_toplevel->application.application_id = g_strdup (application_id);
  wayland_toplevel->application.app_menu_path = g_strdup (app_menu_path);
  wayland_toplevel->application.menubar_path = g_strdup (menubar_path);
  wayland_toplevel->application.window_object_path = g_strdup (window_object_path);
  wayland_toplevel->application.application_object_path =
    g_strdup (application_object_path);
  wayland_toplevel->application.unique_bus_name = g_strdup (unique_bus_name);

  maybe_set_gtk_surface_dbus_properties (wayland_toplevel);
}

void
gdk_wayland_toplevel_destroy (GdkToplevel *toplevel)
{
  GdkWaylandToplevel *self = GDK_WAYLAND_TOPLEVEL (toplevel);

  while (self->exported)
    {
      GdkWaylandExported *exported = self->exported->data;
      self->exported = g_list_delete_link (self->exported, self->exported);
      if (exported->handle == NULL)
        {
          GTask *task;

          if (exported->xdg_exported_v2)
            task = G_TASK (wl_proxy_get_user_data ((struct wl_proxy *) exported->xdg_exported_v2));
          else
            task = G_TASK (wl_proxy_get_user_data ((struct wl_proxy *) exported->xdg_exported));

          g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Surface was destroyed");
          g_object_unref (task);
        }

      destroy_exported (exported);
    }
}

/* }}} */
/* {{{ Toplevel API */

/**
 * gdk_wayland_toplevel_set_application_id:
 * @toplevel: (type GdkWaylandToplevel): a `GdkToplevel`
 * @application_id: the application id for the @toplevel
 *
 * Sets the application id on a `GdkToplevel`.
 */
void
gdk_wayland_toplevel_set_application_id (GdkToplevel *toplevel,
                                         const char  *application_id)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));

  g_return_if_fail (application_id != NULL);

  if (GDK_SURFACE_DESTROYED (toplevel))
    return;

  impl = GDK_WAYLAND_SURFACE (toplevel);

  if (!is_realized_toplevel (impl))
    return;

  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_app_id (wayland_toplevel->display_server.xdg_toplevel, application_id);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_app_id (wayland_toplevel->display_server.zxdg_toplevel_v6, application_id);
      break;
    default:
      g_assert_not_reached ();
    }
}

void
gdk_wayland_toplevel_announce_csd (GdkToplevel *toplevel)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandToplevel *toplevel_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  toplevel_wayland = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (!display_wayland->server_decoration_manager)
    return;
  toplevel_wayland->server_decoration =
      org_kde_kwin_server_decoration_manager_create (display_wayland->server_decoration_manager,
                                                     gdk_wayland_surface_get_wl_surface (GDK_SURFACE (toplevel_wayland)));
  if (toplevel_wayland->server_decoration)
    org_kde_kwin_server_decoration_request_mode (toplevel_wayland->server_decoration,
                                                 ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_CLIENT);
}

void
gdk_wayland_toplevel_announce_ssd (GdkToplevel *toplevel)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandToplevel *toplevel_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  toplevel_wayland = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (!display_wayland->server_decoration_manager)
    return;
  toplevel_wayland->server_decoration =
      org_kde_kwin_server_decoration_manager_create (display_wayland->server_decoration_manager,
                                                     gdk_wayland_surface_get_wl_surface (GDK_SURFACE (toplevel_wayland)));
  if (toplevel_wayland->server_decoration)
    org_kde_kwin_server_decoration_request_mode (toplevel_wayland->server_decoration,
                                                 ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

gboolean
gdk_wayland_toplevel_inhibit_idle (GdkToplevel *toplevel)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandToplevel *wayland_toplevel;

  g_return_val_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel), FALSE);
  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (!display_wayland->idle_inhibit_manager)
    return FALSE;

  if (!wayland_toplevel->idle_inhibitor)
    {
      g_assert (wayland_toplevel->idle_inhibitor_refcount == 0);

      wayland_toplevel->idle_inhibitor =
          zwp_idle_inhibit_manager_v1_create_inhibitor (display_wayland->idle_inhibit_manager,
                                                        gdk_wayland_surface_get_wl_surface (GDK_SURFACE (wayland_toplevel)));
    }
  ++wayland_toplevel->idle_inhibitor_refcount;

  return TRUE;
}

void
gdk_wayland_toplevel_uninhibit_idle (GdkToplevel *toplevel)
{
  GdkWaylandToplevel *wayland_toplevel;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  g_assert (wayland_toplevel->idle_inhibitor &&
            wayland_toplevel->idle_inhibitor_refcount > 0);

  if (--wayland_toplevel->idle_inhibitor_refcount == 0)
    {
      g_clear_pointer (&wayland_toplevel->idle_inhibitor,
                       zwp_idle_inhibitor_v1_destroy);
    }
}

/**
 * GdkWaylandToplevelExported:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` that is exported
 * @handle: the handle
 * @user_data: user data that was passed to [method@GdkWayland.WaylandToplevel.export_handle]
 *
 * Callback that gets called when the handle for a surface has been
 * obtained from the Wayland compositor.
 *
 * This callback is used in [method@GdkWayland.WaylandToplevel.export_handle].
 *
 * The @handle can be passed to other processes, for the purpose of
 * marking surfaces as transient for out-of-process surfaces.
 */

typedef struct {
  GdkWaylandToplevelExported callback;
  gpointer user_data;
  GDestroyNotify destroy;
} ExportHandleData;

static void
export_handle_done (GObject      *source,
                    GAsyncResult *result,
                    void         *user_data)
{
  GdkToplevel *toplevel = GDK_TOPLEVEL (source);
  ExportHandleData *data = (ExportHandleData *)user_data;
  char *handle;

  handle = gdk_toplevel_export_handle_finish (toplevel, result, NULL);
  data->callback (toplevel, handle, data->user_data);
  g_free (handle);

  if (data->destroy)
    data->destroy (data->user_data);

  g_free (data);
}

/**
 * gdk_wayland_toplevel_export_handle:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` to obtain a handle for
 * @callback: callback to call with the handle
 * @user_data: (closure): user data for @callback
 * @destroy_func: destroy notify for @user_data
 *
 * Asynchronously obtains a handle for a surface that can be passed
 * to other processes.
 *
 * When the handle has been obtained, @callback will be called.
 *
 * It is an error to call this function on a surface that is already
 * exported.
 *
 * When the handle is no longer needed, [method@GdkWayland.WaylandToplevel.unexport_handle]
 * should be called to clean up resources.
 *
 * The main purpose for obtaining a handle is to mark a surface
 * from another surface as transient for this one, see
 * [method@GdkWayland.WaylandToplevel.set_transient_for_exported].
 *
 * Before 4.12, this API could not safely be used multiple times,
 * since there was no reference counting for handles. Starting with
 * 4.12, every call to this function obtains a new handle, and every
 * call to [method@GdkWayland.WaylandToplevel.drop_exported_handle] drops
 * just the handle that it is given.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the handle has been requested, %FALSE if
 *   an error occurred.
 */
gboolean
gdk_wayland_toplevel_export_handle (GdkToplevel                *toplevel,
                                    GdkWaylandToplevelExported  callback,
                                    gpointer                    user_data,
                                    GDestroyNotify              destroy_func)
{
  ExportHandleData *data;

  g_return_val_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel), FALSE);

  data = g_new (ExportHandleData, 1);
  data->callback = callback;
  data->user_data = user_data;
  data->destroy = destroy_func;

  gdk_toplevel_export_handle (toplevel, NULL, export_handle_done, data);

  return TRUE;
}

/**
 * gdk_wayland_toplevel_unexport_handle:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` to unexport
 *
 * Destroys the handle that was obtained with
 * gdk_wayland_toplevel_export_handle().
 *
 * It is an error to call this function on a surface that
 * does not have a handle.
 *
 * Since 4.12, this function does nothing. Use
 * [method@GdkWayland.WaylandToplevel.drop_exported_handle] instead to drop a
 * handle that was obtained with [method@GdkWayland.WaylandToplevel.export_handle].
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Deprecated: 4.12: Use [method@GdkWayland.WaylandToplevel.drop_exported_handle]
 *   instead, this function does nothing
 */
void
gdk_wayland_toplevel_unexport_handle (GdkToplevel *toplevel)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (wayland_toplevel->exported != NULL &&
      wayland_toplevel->exported->next == NULL)
    {
      GdkWaylandExported *exported = wayland_toplevel->exported->data;

      if (exported->handle)
        {
          gdk_toplevel_unexport_handle (toplevel, exported->handle);
          return;
        }
    }

  g_warning ("Use gdk_wayland_toplevel_drop_exported_handle()");
}

/**
 * gdk_wayland_toplevel_drop_exported_handle:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` that was exported
 * @handle: the handle to drop
 *
 * Destroy a handle that was obtained with gdk_wayland_toplevel_export_handle().
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Since: 4.12
 */
void
gdk_wayland_toplevel_drop_exported_handle (GdkToplevel *toplevel,
                                           const char  *handle)
{
  gdk_toplevel_unexport_handle (toplevel, handle);
}

static void
unset_transient_for_exported (GdkWaylandToplevel *toplevel)
{
  g_clear_pointer (&toplevel->imported_transient_for, zxdg_imported_v1_destroy);
  g_clear_pointer (&toplevel->imported_transient_for_v2, zxdg_imported_v2_destroy);
}

static void
xdg_imported_destroyed (void                    *data,
                        struct zxdg_imported_v1 *zxdg_imported_v1)
{
  unset_transient_for_exported (GDK_WAYLAND_TOPLEVEL (data));
}

static const struct zxdg_imported_v1_listener xdg_imported_listener = {
  xdg_imported_destroyed,
};

static void
xdg_imported_v2_destroyed (void                    *data,
                           struct zxdg_imported_v2 *zxdg_imported_v1)
{
  unset_transient_for_exported (GDK_WAYLAND_TOPLEVEL (data));
}

static const struct zxdg_imported_v2_listener xdg_imported_listener_v2 = {
  xdg_imported_v2_destroyed,
};

/**
 * gdk_wayland_toplevel_set_transient_for_exported:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` to make as transient
 * @parent_handle_str: an exported handle for a surface
 *
 * Marks @toplevel as transient for the surface to which the given
 * @parent_handle_str refers.
 *
 * Typically, the handle will originate from a
 * [method@GdkWayland.WaylandToplevel.export_handle] call in another process.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the surface has been marked as transient,
 *   %FALSE if an error occurred.
 */
gboolean
gdk_wayland_toplevel_set_transient_for_exported (GdkToplevel *toplevel,
                                                 const char  *parent_handle_str)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (toplevel);
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_return_val_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);

  if (!display_wayland->xdg_importer && !display_wayland->xdg_importer_v2)
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }

  gdk_wayland_toplevel_set_transient_for (GDK_WAYLAND_TOPLEVEL (impl), NULL);

  if (display_wayland->xdg_importer)
    {
      wayland_toplevel->imported_transient_for =
        zxdg_importer_v1_import (display_wayland->xdg_importer, parent_handle_str);
      zxdg_imported_v1_add_listener (wayland_toplevel->imported_transient_for,
                                     &xdg_imported_listener,
                                     toplevel);
    }
  else
    {
      wayland_toplevel->imported_transient_for_v2 =
        zxdg_importer_v2_import_toplevel (display_wayland->xdg_importer_v2, parent_handle_str);
      zxdg_imported_v2_add_listener (wayland_toplevel->imported_transient_for_v2,
                                     &xdg_imported_listener_v2,
                                     toplevel);
    }

  gdk_wayland_toplevel_sync_parent_of_imported (wayland_toplevel);

  return TRUE;
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */
