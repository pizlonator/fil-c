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

#include "gdksurface-wayland.h"

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

#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <unistd.h>

#include "gdksurface-wayland-private.h"

static void update_popup_layout_state (GdkWaylandPopup *wayland_popup,
                                       int              x,
                                       int              y,
                                       int              width,
                                       int              height,
                                       GdkPopupLayout  *layout);

/* {{{ Utilities */

static gboolean
is_realized_shell_surface (GdkWaylandSurface *impl)
{
  return (impl->display_server.xdg_surface ||
          impl->display_server.zxdg_surface_v6);
}

static enum xdg_positioner_anchor
rect_anchor_to_anchor (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case GDK_GRAVITY_NORTH:
      return XDG_POSITIONER_ANCHOR_TOP;
    case GDK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case GDK_GRAVITY_WEST:
      return XDG_POSITIONER_ANCHOR_LEFT;
    case GDK_GRAVITY_CENTER:
      return XDG_POSITIONER_ANCHOR_NONE;
    case GDK_GRAVITY_EAST:
      return XDG_POSITIONER_ANCHOR_RIGHT;
    case GDK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case GDK_GRAVITY_SOUTH:
      return XDG_POSITIONER_ANCHOR_BOTTOM;
    case GDK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
    default: 
      g_assert_not_reached ();
    } 
}

static enum xdg_positioner_gravity
surface_anchor_to_gravity (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
    case GDK_GRAVITY_NORTH:
      return XDG_POSITIONER_GRAVITY_BOTTOM;
    case GDK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case GDK_GRAVITY_WEST:
      return XDG_POSITIONER_GRAVITY_RIGHT;
    case GDK_GRAVITY_CENTER:
      return XDG_POSITIONER_GRAVITY_NONE;
    case GDK_GRAVITY_EAST:
      return XDG_POSITIONER_GRAVITY_LEFT;
    case GDK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case GDK_GRAVITY_SOUTH:
      return XDG_POSITIONER_GRAVITY_TOP;
    case GDK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    default:
      g_assert_not_reached ();
    }
}

static enum zxdg_positioner_v6_anchor
rect_anchor_to_anchor_legacy (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
              ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    case GDK_GRAVITY_NORTH:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP;
    case GDK_GRAVITY_NORTH_EAST:
      return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
              ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    case GDK_GRAVITY_WEST:
      return ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case GDK_GRAVITY_CENTER:
      return ZXDG_POSITIONER_V6_ANCHOR_NONE;
    case GDK_GRAVITY_EAST:
      return ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case GDK_GRAVITY_SOUTH_WEST:
      return (ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
              ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    case GDK_GRAVITY_SOUTH:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
    case GDK_GRAVITY_SOUTH_EAST:
      return (ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
              ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    default:
      g_assert_not_reached ();
    }

  return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
          ZXDG_POSITIONER_V6_ANCHOR_LEFT);
}

static enum zxdg_positioner_v6_gravity
surface_anchor_to_gravity_legacy (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
              ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    case GDK_GRAVITY_NORTH:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
    case GDK_GRAVITY_NORTH_EAST:
      return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
              ZXDG_POSITIONER_V6_GRAVITY_LEFT);
    case GDK_GRAVITY_WEST:
      return ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case GDK_GRAVITY_CENTER:
      return ZXDG_POSITIONER_V6_GRAVITY_NONE;
    case GDK_GRAVITY_EAST:
      return ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case GDK_GRAVITY_SOUTH_WEST:
      return (ZXDG_POSITIONER_V6_GRAVITY_TOP |
              ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    case GDK_GRAVITY_SOUTH:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP;
    case GDK_GRAVITY_SOUTH_EAST:
      return (ZXDG_POSITIONER_V6_GRAVITY_TOP |
              ZXDG_POSITIONER_V6_GRAVITY_LEFT);
    default:
      g_assert_not_reached ();
    }

  return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
          ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
}


/* }}} */
/* {{{ GdkWaylandPopup definition */

/**
 * GdkWaylandPopup:
 *
 * The Wayland implementation of `GdkPopup`.
 */

struct _GdkWaylandPopup
{
  GdkWaylandSurface parent_instance;

  struct {
    struct xdg_popup *xdg_popup;
    struct zxdg_popup_v6 *zxdg_popup_v6;
  } display_server;

  PopupState state;
  unsigned int thaw_upon_show : 1;
  GdkPopupLayout *layout;
  int unconstrained_width;
  int unconstrained_height;

  struct {
    int x;
    int y;
    int width;
    int height;
    uint32_t repositioned_token;
    gboolean has_repositioned_token;
  } pending;

  struct {
    int x;
    int y;
  } next_layout;

  uint32_t reposition_token;
  uint32_t received_reposition_token;

  GdkSeat *grab_input_seat;
};

typedef struct
{
  GdkWaylandSurfaceClass parent_class;
} GdkWaylandPopupClass;

static void gdk_wayland_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandPopup, gdk_wayland_popup, GDK_TYPE_WAYLAND_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                gdk_wayland_popup_iface_init))

/* }}} */
/* {{{ Popup implementation */

static GdkSurface *
get_popup_toplevel (GdkSurface *surface)
{
  if (surface->parent)
    return get_popup_toplevel (surface->parent);
  else
    return surface;
}

static void
freeze_popup_toplevel_state (GdkWaylandPopup *wayland_popup)
{
  GdkSurface *toplevel;

  toplevel = get_popup_toplevel (GDK_SURFACE (wayland_popup));
  gdk_wayland_surface_freeze_state (toplevel);
}

static void
thaw_popup_toplevel_state (GdkWaylandPopup *wayland_popup)
{
  GdkSurface *toplevel;

  toplevel = get_popup_toplevel (GDK_SURFACE (wayland_popup));
  gdk_wayland_surface_thaw_state (toplevel);
}

static void
gdk_wayland_popup_hide_surface (GdkWaylandSurface *wayland_surface)
{
  GdkWaylandPopup *popup = GDK_WAYLAND_POPUP (wayland_surface);
  GdkSurface *surface = GDK_SURFACE (popup);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_clear_pointer (&popup->display_server.xdg_popup, xdg_popup_destroy);
  g_clear_pointer (&popup->display_server.zxdg_popup_v6, zxdg_popup_v6_destroy);
  display_wayland->current_popups =
      g_list_remove (display_wayland->current_popups, surface);
  display_wayland->current_grabbing_popups =
      g_list_remove (display_wayland->current_grabbing_popups, surface);

  popup->thaw_upon_show = TRUE;
  gdk_surface_freeze_updates (surface);

  switch (popup->state)
    {
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
      gdk_surface_thaw_updates (surface);
      G_GNUC_FALLTHROUGH;
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
    case POPUP_STATE_WAITING_FOR_FRAME:
      thaw_popup_toplevel_state (popup);
      break;
    case POPUP_STATE_IDLE:
      break;
    default:
      g_assert_not_reached ();
    }

  popup->state = POPUP_STATE_IDLE;

  g_clear_pointer (&popup->layout, gdk_popup_layout_unref);
}

static gboolean
is_realized_popup (GdkWaylandSurface *impl)
{
  GdkWaylandPopup *popup;

  if (!GDK_IS_WAYLAND_POPUP (impl))
    return FALSE;

  popup = GDK_WAYLAND_POPUP (impl);

  return (popup->display_server.xdg_popup ||
          popup->display_server.zxdg_popup_v6);
}

static void
gdk_wayland_popup_handle_frame (GdkWaylandSurface *surface)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (surface);

  switch (wayland_popup->state)
    {
    case POPUP_STATE_IDLE:
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
      break;
    case POPUP_STATE_WAITING_FOR_FRAME:
      wayland_popup->state = POPUP_STATE_IDLE;
      thaw_popup_toplevel_state (wayland_popup);
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gdk_wayland_popup_compute_size (GdkSurface *surface)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (surface);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (surface);

  if (wayland_surface->next_layout.surface_geometry_dirty)
    {
      int x, y, width, height;

      x = wayland_popup->next_layout.x - wayland_surface->shadow_left;
      y = wayland_popup->next_layout.y - wayland_surface->shadow_top;
      width = wayland_surface->next_layout.configured_width +
              (wayland_surface->shadow_left + wayland_surface->shadow_right);
      height = wayland_surface->next_layout.configured_height +
               (wayland_surface->shadow_top + wayland_surface->shadow_bottom);

      gdk_wayland_surface_move_resize (surface, x, y, width, height);

      wayland_surface->next_layout.surface_geometry_dirty = FALSE;
    }

  return FALSE;
}

static void
gdk_wayland_popup_handle_configure (GdkWaylandSurface *wayland_surface)
{
  GdkSurface *surface = GDK_SURFACE (wayland_surface);
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (wayland_surface);
  GdkRectangle parent_geometry;
  int x, y, width, height;

  if (wayland_popup->display_server.xdg_popup)
    {
      xdg_surface_ack_configure (wayland_surface->display_server.xdg_surface,
                                 wayland_surface->pending.serial);
    }
  else if (wayland_popup->display_server.zxdg_popup_v6)
    {
      zxdg_surface_v6_ack_configure (wayland_surface->display_server.zxdg_surface_v6,
                                     wayland_surface->pending.serial);
    }
  else
    g_warn_if_reached ();

  if (wayland_popup->pending.has_repositioned_token)
    {
      wayland_popup->received_reposition_token =
        wayland_popup->pending.repositioned_token;
      wayland_popup->pending.has_repositioned_token = FALSE;
    }

  switch (wayland_popup->state)
    {
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
      if (wayland_popup->received_reposition_token != wayland_popup->reposition_token)
        return;
      else
        gdk_surface_thaw_updates (surface);
      G_GNUC_FALLTHROUGH;
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
      wayland_popup->state = POPUP_STATE_WAITING_FOR_FRAME;
      break;
    case POPUP_STATE_IDLE:
    case POPUP_STATE_WAITING_FOR_FRAME:
      break;
    default:
      g_assert_not_reached ();
    }

  x = wayland_popup->pending.x;
  y = wayland_popup->pending.y;
  width = wayland_popup->pending.width;
  height = wayland_popup->pending.height;

  gdk_wayland_surface_get_window_geometry (surface->parent, &parent_geometry);
  x += parent_geometry.x;
  y += parent_geometry.y;

  update_popup_layout_state (wayland_popup,
                             x, y,
                             width, height,
                             wayland_popup->layout);

  wayland_popup->next_layout.x = x;
  wayland_popup->next_layout.y = y;
  wayland_surface->next_layout.configured_width = width;
  wayland_surface->next_layout.configured_height = height;
  wayland_surface->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);
}

static void
gdk_wayland_surface_handle_configure_popup (GdkWaylandPopup *wayland_popup,
                                            int32_t          x,
                                            int32_t          y,
                                            int32_t          width,
                                            int32_t          height)
{
  wayland_popup->pending.x = x;
  wayland_popup->pending.y = y;
  wayland_popup->pending.width = width;
  wayland_popup->pending.height = height;
}

static void
xdg_popup_configure (void             *data,
                     struct xdg_popup *xdg_popup,
                     int32_t           x,
                     int32_t           y,
                     int32_t           width,
                     int32_t           height)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (data);

  gdk_wayland_surface_handle_configure_popup (wayland_popup, x, y, width, height);
}

static void
xdg_popup_done (void             *data,
                struct xdg_popup *xdg_popup)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (data);
  GdkSurface *surface = GDK_SURFACE (wayland_popup);

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS, "done %p", surface);

  gdk_surface_hide (surface);
}

static void
xdg_popup_repositioned (void             *data,
                        struct xdg_popup *xdg_popup,
                        uint32_t          token)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (data);

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (GDK_SURFACE (wayland_popup)), EVENTS,
                     "repositioned %p", wayland_popup);

  if (wayland_popup->state != POPUP_STATE_WAITING_FOR_REPOSITIONED)
    {
      g_warning ("Unexpected xdg_popup.repositioned event, probably buggy compositor");
      return;
    }

  wayland_popup->pending.repositioned_token = token;
  wayland_popup->pending.has_repositioned_token = TRUE;
}

static const struct xdg_popup_listener xdg_popup_listener = {
  xdg_popup_configure,
  xdg_popup_done,
  xdg_popup_repositioned,
};

static void
zxdg_popup_v6_configure (void                 *data,
                         struct zxdg_popup_v6 *xdg_popup,
                         int32_t               x,
                         int32_t               y,
                         int32_t               width,
                         int32_t               height)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (data);

  gdk_wayland_surface_handle_configure_popup (wayland_popup, x, y, width, height);
}

static void
zxdg_popup_v6_done (void                 *data,
                    struct zxdg_popup_v6 *xdg_popup)
{
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (data);
  GdkSurface *surface = GDK_SURFACE (wayland_popup);

  GDK_DEBUG (EVENTS, "done %p", surface);

  gdk_surface_hide (surface);
}

static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
  zxdg_popup_v6_configure,
  zxdg_popup_v6_done,
};

static void
calculate_popup_rect (GdkWaylandPopup *wayland_popup,
                      GdkPopupLayout  *layout,
                      GdkRectangle    *out_rect)
{
  int width, height;
  GdkRectangle anchor_rect;
  int dx, dy;
  int shadow_left, shadow_right, shadow_top, shadow_bottom;
  int x = 0, y = 0;

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);

  width = (wayland_popup->unconstrained_width - (shadow_left + shadow_right));
  height = (wayland_popup->unconstrained_height - (shadow_top + shadow_bottom));

  anchor_rect = *gdk_popup_layout_get_anchor_rect (layout);
  gdk_popup_layout_get_offset (layout, &dx, &dy);
  anchor_rect.x += dx;
  anchor_rect.y += dy;

  switch (gdk_popup_layout_get_rect_anchor (layout))
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_CENTER:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + anchor_rect.height;
      break;
    }

  switch (gdk_popup_layout_get_surface_anchor (layout))
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      break;
    case GDK_GRAVITY_NORTH:
      x -= width / 2;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x -= width;
      break;
    case GDK_GRAVITY_WEST:
      y -= height / 2;
      break;
    case GDK_GRAVITY_CENTER:
      x -= width / 2;
      y -= height / 2;
      break;
    case GDK_GRAVITY_EAST:
      x -= width;
      y -= height / 2;
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      y -= height;
      break;
    case GDK_GRAVITY_SOUTH:
      x -= width / 2;
      y -= height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x -= width;
      y -= height;
      break;
    }

  *out_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = width,
    .height = height
  };
}

static void
update_popup_layout_state (GdkWaylandPopup *wayland_popup,
                           int              x,
                           int              y,
                           int              width,
                           int              height,
                           GdkPopupLayout  *layout)
{
  GdkRectangle best_rect;
  GdkRectangle flipped_rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;

  rect_anchor = gdk_popup_layout_get_rect_anchor (layout);
  surface_anchor = gdk_popup_layout_get_surface_anchor (layout);
  anchor_hints = gdk_popup_layout_get_anchor_hints (layout);

  calculate_popup_rect (wayland_popup, layout, &best_rect);

  flipped_rect = best_rect;

  if (x != best_rect.x &&
      anchor_hints & GDK_ANCHOR_FLIP_X)
    {
      GdkRectangle flipped_x_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_surface_anchor;
      GdkPopupLayout *flipped_layout;

      flipped_rect_anchor = gdk_gravity_flip_horizontally (rect_anchor);
      flipped_surface_anchor = gdk_gravity_flip_horizontally (surface_anchor);
      flipped_layout = gdk_popup_layout_copy (layout);
      gdk_popup_layout_set_rect_anchor (flipped_layout,
                                        flipped_rect_anchor);
      gdk_popup_layout_set_surface_anchor (flipped_layout,
                                           flipped_surface_anchor);
      calculate_popup_rect (wayland_popup,
                            flipped_layout,
                            &flipped_x_rect);
      gdk_popup_layout_unref (flipped_layout);

      if (flipped_x_rect.x == x)
        flipped_rect.x = x;
    }
  if (y != best_rect.y &&
      anchor_hints & GDK_ANCHOR_FLIP_Y)
    {
      GdkRectangle flipped_y_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_surface_anchor;
      GdkPopupLayout *flipped_layout;

      flipped_rect_anchor = gdk_gravity_flip_vertically (rect_anchor);
      flipped_surface_anchor = gdk_gravity_flip_vertically (surface_anchor);
      flipped_layout = gdk_popup_layout_copy (layout);
      gdk_popup_layout_set_rect_anchor (flipped_layout,
                                        flipped_rect_anchor);
      gdk_popup_layout_set_surface_anchor (flipped_layout,
                                           flipped_surface_anchor);
      calculate_popup_rect (wayland_popup,
                            flipped_layout,
                            &flipped_y_rect);
      gdk_popup_layout_unref (flipped_layout);

      if (flipped_y_rect.y == y)
        flipped_rect.y = y;
    }

  if (flipped_rect.x != best_rect.x)
    {
      rect_anchor = gdk_gravity_flip_horizontally (rect_anchor);
      surface_anchor = gdk_gravity_flip_horizontally (surface_anchor);
    }
  if (flipped_rect.y != best_rect.y)
    {
      rect_anchor = gdk_gravity_flip_vertically (rect_anchor);
      surface_anchor = gdk_gravity_flip_vertically (surface_anchor);
    }

  GDK_SURFACE (wayland_popup)->popup.rect_anchor = rect_anchor;
  GDK_SURFACE (wayland_popup)->popup.surface_anchor = surface_anchor;
}

static gpointer
create_dynamic_positioner (GdkWaylandPopup *wayland_popup,
                           int              width,
                           int              height,
                           GdkPopupLayout  *layout,
                           gboolean         ack_parent_configure)
{
  GdkSurface *surface = GDK_SURFACE (wayland_popup);
  GdkSurface *parent = surface->parent;
  GdkWaylandSurface *parent_impl = GDK_WAYLAND_SURFACE (parent);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;
  uint32_t constraint_adjustment = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;
  GdkRectangle anchor_rect;
  int rect_anchor_dx;
  int rect_anchor_dy;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  GdkRectangle parent_geometry;
  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);
  geometry = (GdkRectangle) {
    .x = shadow_left,
    .y = shadow_top,
    .width = width - (shadow_left + shadow_right),
    .height = height - (shadow_top + shadow_bottom),
  };

  gdk_wayland_surface_get_window_geometry (surface->parent, &parent_geometry);

  anchor_rect = *gdk_popup_layout_get_anchor_rect (layout);

  /* Wayland protocol requires that the anchor rect is specified
   * wrt. to the parent geometry, and that it is non-empty and
   * contained in the parent geometry.
   */
  if (!gdk_rectangle_intersect (&parent_geometry, &anchor_rect, &anchor_rect))
    {
      anchor_rect.x = 0;
      anchor_rect.y = 0;
      anchor_rect.width = 1;
      anchor_rect.height = 1;
    }
  else
    {
      anchor_rect.x -= parent_geometry.x;
      anchor_rect.y -= parent_geometry.y;
    }

  gdk_popup_layout_get_offset (layout, &rect_anchor_dx, &rect_anchor_dy);

  rect_anchor = gdk_popup_layout_get_rect_anchor (layout);
  surface_anchor = gdk_popup_layout_get_surface_anchor (layout);

  anchor_hints = gdk_popup_layout_get_anchor_hints (layout);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_positioner *positioner;
        enum xdg_positioner_anchor anchor;
        enum xdg_positioner_gravity gravity;

        positioner = xdg_wm_base_create_positioner (display->xdg_wm_base);

        xdg_positioner_set_size (positioner, geometry.width, geometry.height);
        xdg_positioner_set_anchor_rect (positioner,
                                        anchor_rect.x,
                                        anchor_rect.y,
                                        anchor_rect.width,
                                        anchor_rect.height);
        xdg_positioner_set_offset (positioner, rect_anchor_dx, rect_anchor_dy);

        anchor = rect_anchor_to_anchor (rect_anchor);
        xdg_positioner_set_anchor (positioner, anchor);

        gravity = surface_anchor_to_gravity (surface_anchor);
        xdg_positioner_set_gravity (positioner, gravity);

        if (anchor_hints & GDK_ANCHOR_FLIP_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
        if (anchor_hints & GDK_ANCHOR_FLIP_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        if (anchor_hints & GDK_ANCHOR_SLIDE_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        if (anchor_hints & GDK_ANCHOR_SLIDE_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
        if (anchor_hints & GDK_ANCHOR_RESIZE_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        if (anchor_hints & GDK_ANCHOR_RESIZE_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
        xdg_positioner_set_constraint_adjustment (positioner,
                                                  constraint_adjustment);

        if (xdg_positioner_get_version (positioner) >=
            XDG_POSITIONER_SET_REACTIVE_SINCE_VERSION)
          xdg_positioner_set_reactive (positioner);

        if (ack_parent_configure &&
            xdg_positioner_get_version (positioner) >=
            XDG_POSITIONER_SET_PARENT_CONFIGURE_SINCE_VERSION)
          {
            xdg_positioner_set_parent_size (positioner,
                                            parent_geometry.width,
                                            parent_geometry.height);
            xdg_positioner_set_parent_configure (positioner,
                                                 parent_impl->last_configure_serial);
          }

        return positioner;
      }
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_positioner_v6 *positioner;
        enum zxdg_positioner_v6_anchor anchor;
        enum zxdg_positioner_v6_gravity gravity;

        positioner = zxdg_shell_v6_create_positioner (display->zxdg_shell_v6);

        zxdg_positioner_v6_set_size (positioner, geometry.width, geometry.height);
        zxdg_positioner_v6_set_anchor_rect (positioner,
                                            anchor_rect.x,
                                            anchor_rect.y,
                                            anchor_rect.width,
                                            anchor_rect.height);
        zxdg_positioner_v6_set_offset (positioner,
                                       rect_anchor_dx,
                                       rect_anchor_dy);

        anchor = rect_anchor_to_anchor_legacy (rect_anchor);
        zxdg_positioner_v6_set_anchor (positioner, anchor);

        gravity = surface_anchor_to_gravity_legacy (surface_anchor);
        zxdg_positioner_v6_set_gravity (positioner, gravity);

        if (anchor_hints & GDK_ANCHOR_FLIP_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X;
        if (anchor_hints & GDK_ANCHOR_FLIP_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        if (anchor_hints & GDK_ANCHOR_SLIDE_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        if (anchor_hints & GDK_ANCHOR_SLIDE_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
        if (anchor_hints & GDK_ANCHOR_RESIZE_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        if (anchor_hints & GDK_ANCHOR_RESIZE_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
        zxdg_positioner_v6_set_constraint_adjustment (positioner,
                                                      constraint_adjustment);

        return positioner;
      }
    default:
      g_assert_not_reached ();
    }

  g_assert_not_reached ();
}

static gboolean
can_map_grabbing_popup (GdkSurface *surface,
                        GdkSurface *parent)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkSurface *top_most_popup;

  if (!display_wayland->current_grabbing_popups)
    return TRUE;

  top_most_popup = g_list_first (display_wayland->current_grabbing_popups)->data;
  return top_most_popup == parent;
}

static gboolean
gdk_wayland_surface_create_xdg_popup (GdkWaylandPopup *wayland_popup,
                                      GdkSurface      *parent,
                                      GdkWaylandSeat  *grab_input_seat,
                                      int              width,
                                      int              height,
                                      GdkPopupLayout  *layout)
{
  GdkSurface *surface = GDK_SURFACE (wayland_popup);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandSurface *parent_impl = GDK_WAYLAND_SURFACE (parent);
  gpointer positioner;

  if (!impl->display_server.wl_surface)
    return FALSE;

  if (!is_realized_shell_surface (parent_impl))
    return FALSE;

  if (is_realized_popup (impl))
    {
      g_warning ("Can't map popup, already mapped");
      return FALSE;
    }

  if (grab_input_seat &&
      !can_map_grabbing_popup (surface, parent))
    {
      g_warning ("Tried to map a grabbing popup with a non-top most parent");
      return FALSE;
    }

  gdk_surface_freeze_updates (surface);

  positioner = create_dynamic_positioner (wayland_popup, width, height, layout, FALSE);
  gdk_wayland_surface_create_xdg_surface_resources (surface);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      wayland_popup->display_server.xdg_popup =
        xdg_surface_get_popup (impl->display_server.xdg_surface,
                               parent_impl->display_server.xdg_surface,
                               positioner);
      xdg_popup_add_listener (wayland_popup->display_server.xdg_popup,
                              &xdg_popup_listener,
                              wayland_popup);
      xdg_positioner_destroy (positioner);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      wayland_popup->display_server.zxdg_popup_v6 =
        zxdg_surface_v6_get_popup (impl->display_server.zxdg_surface_v6,
                                   parent_impl->display_server.zxdg_surface_v6,
                                   positioner);
      zxdg_popup_v6_add_listener (wayland_popup->display_server.zxdg_popup_v6,
                                  &zxdg_popup_v6_listener,
                                  wayland_popup);
      zxdg_positioner_v6_destroy (positioner);
      break;
    default:
      g_assert_not_reached ();
    }

  wayland_popup->received_reposition_token = 0;
  wayland_popup->reposition_token = 0;

  gdk_popup_layout_get_shadow_width (layout,
                                     &impl->shadow_left,
                                     &impl->shadow_right,
                                     &impl->shadow_top,
                                     &impl->shadow_bottom);

  if (grab_input_seat)
    {
      struct wl_seat *seat;
      guint32 serial;

      seat = gdk_wayland_seat_get_wl_seat (GDK_SEAT (grab_input_seat));
      serial = _gdk_wayland_seat_get_last_implicit_grab_serial (grab_input_seat, NULL);

      switch (display->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_popup_grab (wayland_popup->display_server.xdg_popup, seat, serial);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_popup_v6_grab (wayland_popup->display_server.zxdg_popup_v6, seat, serial);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "Wayland surface commit", NULL);
  wl_surface_commit (impl->display_server.wl_surface);

  if (GDK_IS_POPUP (surface))
    {
      g_assert (wayland_popup->state == POPUP_STATE_IDLE);
      wayland_popup->state = POPUP_STATE_WAITING_FOR_CONFIGURE;
      freeze_popup_toplevel_state (wayland_popup);
    }

  display->current_popups = g_list_append (display->current_popups, surface);
  if (grab_input_seat)
    {
      display->current_grabbing_popups =
        g_list_prepend (display->current_grabbing_popups, surface);
    }

  return TRUE;
}


#define LAST_PROP 1

static void
gdk_wayland_popup_init (GdkWaylandPopup *popup)
{
}

static void
gdk_wayland_popup_constructed (GObject *object)
{
  GdkWaylandPopup *self = GDK_WAYLAND_POPUP (object);
  GdkSurface *surface = GDK_SURFACE (self);

  gdk_surface_set_frame_clock (surface, gdk_surface_get_frame_clock (gdk_popup_get_parent (GDK_POPUP (self))));

  G_OBJECT_CLASS (gdk_wayland_popup_parent_class)->constructed (object);
}

static void
gdk_wayland_popup_get_property (GObject    *object,
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
gdk_wayland_popup_set_property (GObject      *object,
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
gdk_wayland_popup_class_init (GdkWaylandPopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (class);
  GdkWaylandSurfaceClass *wayland_surface_class = GDK_WAYLAND_SURFACE_CLASS (class);

  object_class->constructed = gdk_wayland_popup_constructed;
  object_class->get_property = gdk_wayland_popup_get_property;
  object_class->set_property = gdk_wayland_popup_set_property;

  surface_class->compute_size = gdk_wayland_popup_compute_size;

  wayland_surface_class->handle_configure = gdk_wayland_popup_handle_configure;
  wayland_surface_class->handle_frame = gdk_wayland_popup_handle_frame;
  wayland_surface_class->hide_surface = gdk_wayland_popup_hide_surface;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
is_fallback_relayout_possible (GdkWaylandPopup *wayland_popup)
{
  GList *l;

  for (l = GDK_SURFACE (wayland_popup)->children; l; l = l->next)
    {
      GdkSurface *child = l->data;

      if (GDK_WAYLAND_SURFACE (child)->mapped)
        return FALSE;
    }

  return TRUE;
}

static gboolean gdk_wayland_surface_present_popup (GdkWaylandPopup *wayland_popup,
                                                   int              width,
                                                   int              height,
                                                   GdkPopupLayout  *layout);

static void
queue_relayout_fallback (GdkWaylandPopup *wayland_popup,
                         GdkPopupLayout  *layout)
{
  if (!is_fallback_relayout_possible (wayland_popup))
    return;

  gdk_wayland_surface_hide_surface (GDK_SURFACE (wayland_popup));
  gdk_wayland_surface_present_popup (wayland_popup,
                                     wayland_popup->unconstrained_width,
                                     wayland_popup->unconstrained_height,
                                     layout);
}

static void
do_queue_relayout (GdkWaylandPopup *wayland_popup,
                   int              width,
                   int              height,
                   GdkPopupLayout  *layout)
{
  struct xdg_positioner *positioner;

  g_assert (is_realized_popup (GDK_WAYLAND_SURFACE (wayland_popup)));
  g_assert (wayland_popup->state == POPUP_STATE_IDLE ||
            wayland_popup->state == POPUP_STATE_WAITING_FOR_FRAME);

  g_clear_pointer (&wayland_popup->layout, gdk_popup_layout_unref);
  wayland_popup->layout = gdk_popup_layout_copy (layout);
  wayland_popup->unconstrained_width = width;
  wayland_popup->unconstrained_height = height;

  if (!wayland_popup->display_server.xdg_popup ||
      xdg_popup_get_version (wayland_popup->display_server.xdg_popup) <
      XDG_POPUP_REPOSITION_SINCE_VERSION)
    {
      g_warning_once ("Compositor doesn't support moving popups, "
                      "relying on remapping");
      queue_relayout_fallback (wayland_popup, layout);

      return;
    }

  positioner = create_dynamic_positioner (wayland_popup,
                                          width, height, layout,
                                          TRUE);
  xdg_popup_reposition (wayland_popup->display_server.xdg_popup,
                        positioner,
                        ++wayland_popup->reposition_token);
  xdg_positioner_destroy (positioner);

  gdk_surface_freeze_updates (GDK_SURFACE (wayland_popup));

  switch (wayland_popup->state)
    {
    case POPUP_STATE_IDLE:
      freeze_popup_toplevel_state (wayland_popup);
      break;
    case POPUP_STATE_WAITING_FOR_FRAME:
      break;
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
    default:
      g_assert_not_reached ();
    }

  wayland_popup->state = POPUP_STATE_WAITING_FOR_REPOSITIONED;
}

static gboolean
is_relayout_finished (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->initial_configure_received)
    return FALSE;

  if (GDK_IS_WAYLAND_POPUP (surface))
    {
      GdkWaylandPopup *popup = GDK_WAYLAND_POPUP (surface);
      if (popup->reposition_token != popup->received_reposition_token)
        return FALSE;
    }

  return TRUE;
}

static GdkWaylandSeat *
find_grab_input_seat (GdkSurface *surface,
                      GdkSurface *parent)
{
  GdkWaylandPopup *popup = GDK_WAYLAND_POPUP (surface);
  GdkWaylandPopup *tmp_popup;

  /* Use the device that was used for the grab as the device for
   * the popup surface setup - so this relies on GTK taking the
   * grab before showing the popup surface.
   */
  if (popup->grab_input_seat)
    return GDK_WAYLAND_SEAT (popup->grab_input_seat);

  while (parent)
    {
      if (!GDK_IS_WAYLAND_POPUP (parent))
        break;

      tmp_popup = GDK_WAYLAND_POPUP (parent);

      if (tmp_popup->grab_input_seat)
        return GDK_WAYLAND_SEAT (tmp_popup->grab_input_seat);

      parent = parent->parent;
    }

  return NULL;
}

static void
gdk_wayland_surface_map_popup (GdkWaylandPopup *wayland_popup,
                               int              width,
                               int              height,
                               GdkPopupLayout  *layout)
{
  GdkSurface *surface = GDK_SURFACE (wayland_popup);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_popup);
  GdkSurface *parent;
  GdkWaylandSeat *grab_input_seat;

  parent = surface->parent;
  if (!parent)
    {
      g_warning ("Couldn't map as surface %p as popup because it doesn't have a parent",
                 surface);
      return;
    }

  if (surface->autohide)
    grab_input_seat = find_grab_input_seat (surface, parent);
  else
    grab_input_seat = NULL;

  if (!gdk_wayland_surface_create_xdg_popup (wayland_popup,
                                             parent,
                                             grab_input_seat,
                                             width, height,
                                             layout))
    return;

  wayland_popup->layout = gdk_popup_layout_copy (layout);
  wayland_popup->unconstrained_width = width;
  wayland_popup->unconstrained_height = height;
  wayland_surface->mapped = TRUE;
}

static void
show_popup (GdkWaylandPopup *wayland_popup,
            int              width,
            int              height,
            GdkPopupLayout  *layout)
{
  if (wayland_popup->thaw_upon_show)
    {
      wayland_popup->thaw_upon_show = FALSE;
      gdk_surface_thaw_updates (GDK_SURFACE (wayland_popup));
    }

  gdk_wayland_surface_map_popup (wayland_popup, width, height, layout);
}

typedef struct
{
  int width;
  int height;
  GdkPopupLayout *layout;
} GrabPrepareData;

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  GrabPrepareData *data = user_data;

  g_return_if_fail (GDK_IS_WAYLAND_POPUP (surface));
  GdkWaylandPopup *wayland_popup = GDK_WAYLAND_POPUP (surface);

  show_popup (wayland_popup, data->width, data->height, data->layout);
}

static void
reposition_popup (GdkWaylandPopup *wayland_popup,
                  int              width,
                  int              height,
                  GdkPopupLayout  *layout)
{
  switch (wayland_popup->state)
    {
    case POPUP_STATE_IDLE:
    case POPUP_STATE_WAITING_FOR_FRAME:
      do_queue_relayout (wayland_popup, width, height, layout);
      break;
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
      g_warn_if_reached ();
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gdk_wayland_surface_present_popup (GdkWaylandPopup *wayland_popup,
                                   int              width,
                                   int              height,
                                   GdkPopupLayout  *layout)
{
  GdkSurface *surface = GDK_SURFACE (wayland_popup);
  GdkWaylandSurface *wayland_surface = GDK_WAYLAND_SURFACE (wayland_popup);

  if (!wayland_surface->mapped)
    {
      if (surface->autohide)
        {
          GdkSeat *seat;

          seat = gdk_display_get_default_seat (surface->display);
          if (seat)
            {
              GrabPrepareData data;
              GdkGrabStatus result;

              data = (GrabPrepareData) {
                .width = width,
                .height = height,
                .layout = layout,
              };

              result = gdk_seat_grab (seat,
                                      surface,
                                      GDK_SEAT_CAPABILITY_ALL,
                                      TRUE,
                                      NULL, NULL,
                                      show_grabbing_popup, &data);
              if (result != GDK_GRAB_SUCCESS)
                {
                  const char *grab_status[] = {
                    "success", "already grabbed", "invalid time",
                    "not viewable", "frozen", "failed"
                  };
                  g_warning ("Grab failed: %s", grab_status[result]);
                }
            }
        }
      else
        {
          show_popup (wayland_popup, width, height, layout);
        }
    }
  else
    {
      if (wayland_popup->unconstrained_width == width &&
          wayland_popup->unconstrained_height == height &&
          gdk_popup_layout_equal (wayland_popup->layout, layout))
        return TRUE;

      reposition_popup (wayland_popup, width, height, layout);
    }

  while (wayland_popup->display_server.xdg_popup && !is_relayout_finished (surface))
    {
      gdk_wayland_display_dispatch_queue (surface->display,
                                          wayland_surface->event_queue);
    }

  if (wayland_popup->display_server.xdg_popup)
    {
      gdk_surface_invalidate_rect (surface, NULL);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
gdk_wayland_popup_present (GdkPopup       *popup,
                           int             width,
                           int             height,
                           GdkPopupLayout *layout)
{
  return gdk_wayland_surface_present_popup (GDK_WAYLAND_POPUP (popup), width, height, layout);
}

static GdkGravity
gdk_wayland_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_wayland_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_wayland_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_wayland_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_wayland_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_wayland_popup_present;
  iface->get_surface_anchor = gdk_wayland_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_wayland_popup_get_rect_anchor;
  iface->get_position_x = gdk_wayland_popup_get_position_x;
  iface->get_position_y = gdk_wayland_popup_get_position_y;
}

/* }}} */
/* {{{ Private Popup API */

void
_gdk_wayland_surface_set_grab_seat (GdkSurface *surface,
                                    GdkSeat    *seat)
{
  GdkWaylandPopup *popup;

  g_return_if_fail (surface != NULL);

  popup = GDK_WAYLAND_POPUP (surface);
  popup->grab_input_seat = seat;
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */
