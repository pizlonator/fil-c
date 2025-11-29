/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include "gdkdevice-wayland-private.h"

#include "gdkclipboard-wayland.h"
#include "gdkclipboardprivate.h"
#include "gdkcursorprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicepadprivate.h"
#include "gdkdevicetoolprivate.h"
#include "gdkdropprivate.h"
#include "gdkeventsprivate.h"
#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"
#include "gdkprimary-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkseat-wayland.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktypes.h"
#include "gdkwayland.h"
#include "gdkprivate.h"

#include "pointer-gestures-unstable-v1-client-protocol.h"
#include "tablet-unstable-v2-client-protocol.h"

#include <xkbcommon/xkbcommon.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mman.h>
#if defined(HAVE_DEV_EVDEV_INPUT_H)
#include <dev/evdev/input.h>
#elif defined(HAVE_LINUX_INPUT_H)
#include <linux/input.h>
#endif

/**
 * GdkWaylandDevice:
 *
 * The Wayland implementation of `GdkDevice`.
 *
 * Beyond the regular [class@Gdk.Device] API, the Wayland implementation
 * provides access to Wayland objects such as the `wl_seat` with
 * [method@GdkWayland.WaylandDevice.get_wl_seat], the `wl_keyboard` with
 * [method@GdkWayland.WaylandDevice.get_wl_keyboard] and the `wl_pointer` with
 * [method@GdkWayland.WaylandDevice.get_wl_pointer].
 */

/**
 * GdkWaylandSeat:
 *
 * The Wayland implementation of `GdkSeat`.
 *
 * Beyond the regular [class@Gdk.Seat] API, the Wayland implementation
 * provides access to the Wayland `wl_seat` object with
 * [method@GdkWayland.WaylandSeat.get_wl_seat].
 */

#define BUTTON_BASE (BTN_LEFT - 1) /* Used to translate to 1-indexed buttons */

#ifndef BTN_STYLUS3
#define BTN_STYLUS3 0x149 /* Linux 4.15 */
#endif

#define ALL_BUTTONS_MASK (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | \
                          GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | \
                          GDK_BUTTON5_MASK)

#define GDK_SEAT_DEBUG(seat,type,...) GDK_DISPLAY_DEBUG(gdk_seat_get_display (GDK_SEAT (seat)),type,__VA_ARGS__)

G_DEFINE_TYPE (GdkWaylandSeat, gdk_wayland_seat, GDK_TYPE_SEAT)

static void init_pointer_data (GdkWaylandPointerData *pointer_data,
                               GdkDisplay            *display_wayland,
                               GdkDevice             *logical_device);
static void pointer_surface_update_scale (GdkDevice *device);

#define GDK_SLOT_TO_EVENT_SEQUENCE(s) ((GdkEventSequence *) GUINT_TO_POINTER((s) + 1))
#define GDK_EVENT_SEQUENCE_TO_SLOT(s) (GPOINTER_TO_UINT(s) - 1)

static void deliver_key_event (GdkWaylandSeat       *seat,
                               uint32_t              time_,
                               uint32_t              key,
                               uint32_t              state,
                               gboolean              from_key_repeat);

void
gdk_wayland_seat_stop_cursor_animation (GdkWaylandSeat        *seat,
                                        GdkWaylandPointerData *pointer)
{
  if (pointer->cursor_timeout_id > 0)
    {
      g_source_remove (pointer->cursor_timeout_id);
      pointer->cursor_timeout_id = 0;
      pointer->cursor_image_delay = 0;
    }

  pointer->cursor_image_index = 0;
}

GdkWaylandTabletData *
gdk_wayland_seat_find_tablet (GdkWaylandSeat *seat,
                              GdkDevice      *device)
{
  GList *l;

  for (l = seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      if (tablet->logical_device == device ||
          tablet->stylus_device == device)
        return tablet;
    }

  return NULL;
}

GdkWaylandTabletPadData *
gdk_wayland_seat_find_pad (GdkWaylandSeat *seat,
                           GdkDevice      *device)
{
  GList *l;

  for (l = seat->tablet_pads; l; l = l->next)
    {
      GdkWaylandTabletPadData *pad = l->data;

      if (pad->device == device)
        return pad;
    }

  return NULL;
}

static void
emulate_crossing (GdkSurface       *surface,
                  GdkSurface       *child_surface,
                  GdkDevice       *device,
                  GdkEventType     type,
                  GdkCrossingMode  mode,
                  guint32          time_)
{
  GdkEvent *event;
  GdkModifierType state;
  double x, y;

  gdk_surface_get_device_position (surface, device, &x, &y, &state);
  event = gdk_crossing_event_new (type,
                                  surface,
                                  device,
                                  time_,
                                  state,
                                  x, y,
                                  mode,
                                  GDK_NOTIFY_NONLINEAR);

  _gdk_wayland_display_deliver_event (gdk_surface_get_display (surface), event);
}

static void
emulate_touch_crossing (GdkSurface           *surface,
                        GdkSurface           *child_surface,
                        GdkDevice           *device,
                        GdkDevice           *source,
                        GdkWaylandTouchData *touch,
                        GdkEventType         type,
                        GdkCrossingMode      mode,
                        guint32              time_)
{
  GdkEvent *event;

  event = gdk_crossing_event_new (type,
                                  surface,
                                  device,
                                  time_,
                                  0,
                                  touch->x, touch->y,
                                  mode,
                                  GDK_NOTIFY_NONLINEAR);

  _gdk_wayland_display_deliver_event (gdk_surface_get_display (surface), event);
}

static void
gdk_wayland_seat_discard_pending_offer (GdkWaylandSeat *seat)
{
  if (seat->pending_builder)
    {
      GdkContentFormats *ignore = gdk_content_formats_builder_free_to_formats (seat->pending_builder);
      gdk_content_formats_unref (ignore);
      seat->pending_builder = NULL;
    }
  g_clear_pointer (&seat->pending_offer, wl_data_offer_destroy);
  seat->pending_source_actions = 0;
  seat->pending_action = 0;
}

static inline GdkDragAction
gdk_wayland_actions_to_gdk_actions (uint32_t dnd_actions)
{
  GdkDragAction actions = 0;

  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions |= GDK_ACTION_COPY;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions |= GDK_ACTION_MOVE;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions |= GDK_ACTION_ASK;

  return actions;
}

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *offer,
                  const char           *type)
{
  GdkWaylandSeat *seat = data;

  if (seat->pending_offer != offer)
    {
      GDK_SEAT_DEBUG (seat, EVENTS,
                      "%p: offer for unknown offer %p of %s",
                      seat, offer, type);
      return;
    }

  /* skip magic mime types */
  if (g_str_equal (type, GDK_WAYLAND_LOCAL_DND_MIME_TYPE))
    return;

  gdk_content_formats_builder_add_mime_type (seat->pending_builder, type);
}

static void
data_offer_source_actions (void                 *data,
                           struct wl_data_offer *offer,
                           uint32_t              source_actions)
{
  GdkWaylandSeat *seat = data;

  if (offer == seat->pending_offer)
    {
      seat->pending_source_actions = gdk_wayland_actions_to_gdk_actions (source_actions);
      return;
    }

  if (seat->drop == NULL)
    return;

  gdk_wayland_drop_set_source_actions (seat->drop, source_actions);

  gdk_drop_emit_motion_event (seat->drop,
                              FALSE,
                              seat->pointer_info.surface_x,
                              seat->pointer_info.surface_y,
                              GDK_CURRENT_TIME);
}

static void
data_offer_action (void                 *data,
                   struct wl_data_offer *offer,
                   uint32_t              action)
{
  GdkWaylandSeat *seat = data;

  if (offer == seat->pending_offer)
    {
      seat->pending_action = gdk_wayland_actions_to_gdk_actions (action);
      return;
    }

  if (seat->drop == NULL)
    return;

  gdk_wayland_drop_set_action (seat->drop, action);

  gdk_drop_emit_motion_event (seat->drop,
                              FALSE,
                              seat->pointer_info.surface_x,
                              seat->pointer_info.surface_y,
                              GDK_CURRENT_TIME);
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
  data_offer_source_actions,
  data_offer_action
};

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "data device data offer, data device %p, offer %p",
                  data_device, offer);

  gdk_wayland_seat_discard_pending_offer (seat);

  seat->pending_offer = offer;
  wl_data_offer_add_listener (offer,
                              &data_offer_listener,
                              seat);

  seat->pending_builder = gdk_content_formats_builder_new ();
  seat->pending_source_actions = 0;
  seat->pending_action = 0;
}

static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               serial,
                   struct wl_surface     *surface,
                   wl_fixed_t             x,
                   wl_fixed_t             y,
                   struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;
  GdkSurface *dest_surface;
  GdkContentFormats *formats;
  GdkDevice *device;

  dest_surface = wl_surface_get_user_data (surface);

  if (!GDK_IS_SURFACE (dest_surface))
    return;

  if (offer != seat->pending_offer)
    {
      GDK_SEAT_DEBUG (seat, EVENTS,
                      "%p: enter event for unknown offer %p, expected %p",
                      seat, offer, seat->pending_offer);
      return;
    }

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "data device enter, data device %p serial %u, surface %p, x %f y %f, offer %p",
                  data_device, serial, surface, wl_fixed_to_double (x), wl_fixed_to_double (y), offer);

  /* Update pointer state, so device state queries work during DnD */
  seat->pointer_info.focus = g_object_ref (dest_surface);
  seat->pointer_info.surface_x = wl_fixed_to_double (x);
  seat->pointer_info.surface_y = wl_fixed_to_double (y);

  if (seat->logical_pointer)
    device = seat->logical_pointer;
  else if (seat->logical_touch)
    device = seat->logical_touch;
  else
    {
      g_warning ("No device for DND enter, ignoring.");
      return;
    }

  formats = gdk_content_formats_builder_free_to_formats (seat->pending_builder);
  seat->pending_builder = NULL;
  seat->pending_offer = NULL;

  seat->drop = gdk_wayland_drop_new (device, seat->drag, formats, dest_surface, offer, serial);
  gdk_wayland_drop_set_source_actions (seat->drop, seat->pending_source_actions);
  gdk_wayland_drop_set_action (seat->drop, seat->pending_action);

  gdk_content_formats_unref (formats);

  gdk_wayland_seat_discard_pending_offer (seat);

  gdk_drop_emit_enter_event (seat->drop,
                             FALSE,
                             seat->pointer_info.surface_x,
                             seat->pointer_info.surface_y,
                             GDK_CURRENT_TIME);
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
  GdkWaylandSeat *seat = data;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "data device leave, data device %p", data_device);

  if (seat->drop == NULL)
    return;

  g_object_unref (seat->pointer_info.focus);
  seat->pointer_info.focus = NULL;

  gdk_drop_emit_leave_event (seat->drop,
                             FALSE,
                             GDK_CURRENT_TIME);

  g_clear_object (&seat->drop);
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    wl_fixed_t             x,
                    wl_fixed_t             y)
{
  GdkWaylandSeat *seat = data;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "data device motion, data_device = %p, time = %d, x = %f, y = %f",
                  data_device, time, wl_fixed_to_double (x), wl_fixed_to_double (y));

  if (seat->drop == NULL)
    return;

  /* Update pointer state, so device state queries work during DnD */
  seat->pointer_info.surface_x = wl_fixed_to_double (x);
  seat->pointer_info.surface_y = wl_fixed_to_double (y);

  gdk_drop_emit_motion_event (seat->drop,
                              FALSE,
                              seat->pointer_info.surface_x,
                              seat->pointer_info.surface_y,
                              time);
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
  GdkWaylandSeat *seat = data;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "data device drop, data device %p", data_device);

  gdk_drop_emit_drop_event (seat->drop,
                            FALSE,
                            seat->pointer_info.surface_x,
                            seat->pointer_info.surface_y,
                            GDK_CURRENT_TIME);
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;
  GdkContentFormats *formats;

  if (offer)
    {
      if (offer == seat->pending_offer)
        {
          formats = gdk_content_formats_builder_free_to_formats (seat->pending_builder);
          seat->pending_builder = NULL;
          seat->pending_offer = NULL;
        }
      else
        {
          formats = gdk_content_formats_new (NULL, 0);
          offer = NULL;
        }

      gdk_wayland_seat_discard_pending_offer (seat);
    }
  else
    {
      formats = gdk_content_formats_new (NULL, 0);
    }

  gdk_wayland_clipboard_claim_remote (GDK_WAYLAND_CLIPBOARD (seat->clipboard),
                                      offer,
                                      formats);
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_enter,
  data_device_leave,
  data_device_motion,
  data_device_drop,
  data_device_selection
};

static GdkDevice * get_scroll_device (GdkWaylandSeat              *seat,
                                      enum wl_pointer_axis_source  source);

static void
flush_discrete_scroll_event (GdkWaylandSeat     *seat,
                             gint                value120_x,
                             gint                value120_y)
{
  GdkEvent *event = NULL;
  GdkDevice *source;
  GdkScrollDirection direction;

  if (value120_x > 0)
    direction = GDK_SCROLL_LEFT;
  else if (value120_x < 0)
    direction = GDK_SCROLL_RIGHT;
  else if (value120_y > 0)
    direction = GDK_SCROLL_DOWN;
  else
    direction = GDK_SCROLL_UP;

  source = get_scroll_device (seat, seat->pointer_info.frame.source);

  if (wl_seat_get_version (seat->wl_seat) >= WL_POINTER_AXIS_VALUE120_SINCE_VERSION)
    {
      event = gdk_scroll_event_new_value120 (seat->pointer_info.focus,
                                             source,
                                             NULL,
                                             seat->pointer_info.time,
                                             gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                             direction,
                                             value120_x,
                                             value120_y);
    }
  else
    {
      gint discrete_x = value120_x / 120;
      gint discrete_y = value120_y / 120;

      if (discrete_x != 0 || discrete_y != 0)
        {
          event = gdk_scroll_event_new_discrete (seat->pointer_info.focus,
                                                 source,
                                                 NULL,
                                                 seat->pointer_info.time,
                                                 gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                                 direction);
        }
    }

  if (event)
    _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
flush_smooth_scroll_event (GdkWaylandSeat *seat,
                           double          delta_x,
                           double          delta_y,
                           gboolean        is_stop)
{
  GdkEvent *event;
  GdkDevice *source;

  source = get_scroll_device (seat, seat->pointer_info.frame.source);
  event = gdk_scroll_event_new (seat->pointer_info.focus,
                                source,
                                NULL,
                                seat->pointer_info.time,
                                gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                delta_x, delta_y,
                                is_stop,
                                GDK_SCROLL_UNIT_SURFACE);

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
flush_scroll_event (GdkWaylandSeat             *seat,
                    GdkWaylandPointerFrameData *pointer_frame)
{
  gboolean is_stop = FALSE;

  if (pointer_frame->value120_x || pointer_frame->value120_y)
    {
      flush_discrete_scroll_event (seat,
                                   pointer_frame->value120_x,
                                   pointer_frame->value120_y);
      pointer_frame->value120_x = 0;
      pointer_frame->value120_y = 0;
    }
  else if (pointer_frame->is_scroll_stop ||
           pointer_frame->delta_x != 0 ||
           pointer_frame->delta_y != 0)
    {
      /* Axes can stop independently, if we stop on one axis but have a
       * delta on the other, we don't count it as a stop event.
       */
      if (pointer_frame->is_scroll_stop &&
          pointer_frame->delta_x == 0 &&
          pointer_frame->delta_y == 0)
        is_stop = TRUE;

      flush_smooth_scroll_event (seat,
                                 pointer_frame->delta_x,
                                 pointer_frame->delta_y,
                                 is_stop);
    }

  pointer_frame->value120_x = 0;
  pointer_frame->value120_y = 0;
  pointer_frame->delta_x = 0;
  pointer_frame->delta_y = 0;
  pointer_frame->is_scroll_stop = FALSE;
}

static void
gdk_wayland_seat_flush_frame_event (GdkWaylandSeat *seat)
{
  if (seat->pointer_info.frame.event)
    {
      _gdk_wayland_display_deliver_event (gdk_seat_get_display (GDK_SEAT (seat)),
                                          seat->pointer_info.frame.event);
      seat->pointer_info.frame.event = NULL;
    }
  else
    {
      flush_scroll_event (seat, &seat->pointer_info.frame);
      seat->pointer_info.frame.source = 0;
    }
}

static void
gdk_wayland_seat_set_frame_event (GdkWaylandSeat *seat,
                                  GdkEvent       *event)
{
  if (seat->pointer_info.frame.event &&
      gdk_event_get_event_type (seat->pointer_info.frame.event) != gdk_event_get_event_type (event))
    gdk_wayland_seat_flush_frame_event (seat);

  seat->pointer_info.frame.event = event;
}

static void
pointer_handle_enter (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface,
                      wl_fixed_t         sx,
                      wl_fixed_t         sy)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;

  if (!surface)
    return;

  if (!GDK_IS_SURFACE (wl_surface_get_user_data (surface)))
    return;

  seat->pointer_info.focus = wl_surface_get_user_data (surface);
  g_object_ref (seat->pointer_info.focus);

  seat->pointer_info.button_modifiers = 0;

  seat->pointer_info.surface_x = wl_fixed_to_double (sx);
  seat->pointer_info.surface_y = wl_fixed_to_double (sy);
  seat->pointer_info.enter_serial = serial;

  event = gdk_crossing_event_new (GDK_ENTER_NOTIFY,
                                  seat->pointer_info.focus,
                                  seat->logical_pointer,
                                  0,
                                  0,
                                  seat->pointer_info.surface_x,
                                  seat->pointer_info.surface_y,
                                  GDK_CROSSING_NORMAL,
                                  GDK_NOTIFY_NONLINEAR);
  gdk_wayland_seat_set_frame_event (seat, event);

  gdk_wayland_device_update_surface_cursor (seat->logical_pointer);

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "enter, seat %p surface %p",
                  seat, seat->pointer_info.focus);

  if (wl_seat_get_version (seat->wl_seat) < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_leave (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkDeviceGrabInfo *grab;

  if (!seat->pointer_info.focus)
    return;

  grab = _gdk_display_get_last_device_grab (seat->display,
                                            seat->logical_pointer);

  if (seat->pointer_info.button_modifiers != 0 &&
      grab && grab->implicit)
    {
      gulong display_serial;

      display_serial = _gdk_display_get_next_serial (seat->display);
      _gdk_display_end_device_grab (seat->display, seat->logical_pointer,
                                    display_serial, NULL, TRUE);
      _gdk_display_device_grab_update (seat->display,
                                       seat->logical_pointer,
                                       display_serial);
    }

  event = gdk_crossing_event_new (GDK_LEAVE_NOTIFY,
                                  seat->pointer_info.focus,
                                  seat->logical_pointer,
                                  0,
                                  0,
                                  seat->pointer_info.surface_x,
                                  seat->pointer_info.surface_y,
                                  GDK_CROSSING_NORMAL,
                                  GDK_NOTIFY_NONLINEAR);
  gdk_wayland_seat_set_frame_event (seat, event);

  gdk_wayland_device_update_surface_cursor (seat->logical_pointer);

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "leave, seat %p surface %p",
                  seat, seat->pointer_info.focus);

  g_object_unref (seat->pointer_info.focus);
  seat->pointer_info.focus = NULL;
  if (seat->cursor)
    gdk_wayland_seat_stop_cursor_animation (seat, &seat->pointer_info);

  if (wl_seat_get_version (seat->wl_seat) < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_motion (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           time,
                       wl_fixed_t         sx,
                       wl_fixed_t         sy)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = time;
  seat->pointer_info.surface_x = wl_fixed_to_double (sx);
  seat->pointer_info.surface_y = wl_fixed_to_double (sy);

  event = gdk_motion_event_new (seat->pointer_info.focus,
                                seat->logical_pointer,
                                NULL,
                                time,
                                gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                seat->pointer_info.surface_x,
                                seat->pointer_info.surface_y,
                                NULL);
  gdk_wayland_seat_set_frame_event (seat, event);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double x, y;
      gdk_event_get_position (event, &x, &y);
      g_message ("motion %f %f, seat %p state %d",
                 x, y, seat, gdk_event_get_modifier_state (event));
    }

  if (wl_seat_get_version (seat->wl_seat) < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_button (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           serial,
                       uint32_t           time,
                       uint32_t           button,
                       uint32_t           state)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  uint32_t modifier;
  int gdk_button;

  if (!seat->pointer_info.focus)
    return;

  switch (button)
    {
    case BTN_LEFT:
      gdk_button = GDK_BUTTON_PRIMARY;
      break;
    case BTN_MIDDLE:
      gdk_button = GDK_BUTTON_MIDDLE;
      break;
    case BTN_RIGHT:
      gdk_button = GDK_BUTTON_SECONDARY;
      break;
    default:
       /* For compatibility reasons, all additional buttons go after the old 4-7 scroll ones */
      gdk_button = button - BUTTON_BASE + 4;
      break;
    }

  seat->pointer_info.time = time;
  if (state)
    seat->pointer_info.press_serial = serial;

  event = gdk_button_event_new (state ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE,
                                seat->pointer_info.focus,
                                seat->logical_pointer,
                                NULL,
                                time,
                                gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                gdk_button,
                                seat->pointer_info.surface_x,
                                seat->pointer_info.surface_y,
                                NULL);

  gdk_wayland_seat_set_frame_event (seat, event);

  switch (button)
    {
    case BTN_RIGHT:
      modifier = GDK_BUTTON3_MASK;
      break;
    case BTN_MIDDLE:
      modifier = GDK_BUTTON2_MASK;
      break;
    default:
      modifier = (GDK_BUTTON1_MASK << (button - BUTTON_BASE - 1)) & ALL_BUTTONS_MASK;
      break;
    }

  if (state)
    seat->pointer_info.button_modifiers |= modifier;
  else
    seat->pointer_info.button_modifiers &= ~modifier;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "button %d %s, seat %p state %d",
                  gdk_button_event_get_button (event),
                  state ? "press" : "release",
                  seat,
                  gdk_event_get_modifier_state (event));

  if (wl_seat_get_version (seat->wl_seat) < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static const char *
get_axis_name (uint32_t axis)
{
  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      return "horizontal";
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      return "vertical";
    default:
      return "unknown";
    }
}

static void
pointer_handle_axis (void              *data,
                     struct wl_pointer *pointer,
                     uint32_t           time,
                     uint32_t           axis,
                     wl_fixed_t         value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  /* get the delta and convert it into the expected range */
  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->delta_y = wl_fixed_to_double (value);
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->delta_x = wl_fixed_to_double (value);
      break;
    default:
      g_return_if_reached ();
    }

  seat->pointer_info.time = time;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "scroll, axis %s, value %f, seat %p",
                  get_axis_name (axis), wl_fixed_to_double (value),
                  seat);

  if (wl_seat_get_version (seat->wl_seat) < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_frame (void              *data,
                      struct wl_pointer *pointer)
{
  GdkWaylandSeat *seat = data;

  GDK_SEAT_DEBUG (seat, EVENTS, "frame, seat %p", seat);

  gdk_wayland_seat_flush_frame_event (seat);
}

static const char *
get_axis_source_name (enum wl_pointer_axis_source source)
{
  switch (source)
    {
    case WL_POINTER_AXIS_SOURCE_WHEEL:
      return "wheel";
    case WL_POINTER_AXIS_SOURCE_FINGER:
      return "finger";
    case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
      return "continuous";
    case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
      return "wheel-tilt";
    default:
      return "unknown";
    }
}

static void
pointer_handle_axis_source (void                        *data,
                            struct wl_pointer           *pointer,
                            enum wl_pointer_axis_source  source)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  pointer_frame->source = source;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "axis source %s, seat %p", get_axis_source_name (source), seat);
}

static void
pointer_handle_axis_stop (void              *data,
                          struct wl_pointer *pointer,
                          uint32_t           time,
                          uint32_t           axis)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = time;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->delta_y = 0;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->delta_x = 0;
      break;
    default:
      g_return_if_reached ();
    }

  pointer_frame->is_scroll_stop = TRUE;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "axis %s stop, seat %p", get_axis_name (axis), seat);
}

static void
pointer_handle_axis_discrete (void              *data,
                              struct wl_pointer *pointer,
                              uint32_t           axis,
                              int32_t            value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->value120_y = value * 120;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->value120_x = value * 120;
      break;
    default:
      g_return_if_reached ();
    }

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "discrete scroll, axis %s, value %d, seat %p",
                  get_axis_name (axis), value, seat);
}

static void
pointer_handle_axis_value120 (void              *data,
                              struct wl_pointer *pointer,
                              uint32_t           axis,
                              int32_t            value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->value120_y = value;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->value120_x = value;
      break;
    default:
      g_return_if_reached ();
    }

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "value120 scroll, axis %s, value %d, seat %p",
                  get_axis_name (axis), value, seat);
}

static int
get_active_layout (GdkKeymap *keymap)
{
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);

  for (int i = 0; i < xkb_keymap_num_layouts (xkb_keymap); i++)
    {
      if (xkb_state_layout_index_is_active (xkb_state, i, XKB_STATE_LAYOUT_EFFECTIVE))
        return i;
    }

  return -1;
}

static const char *
get_active_layout_name (GdkKeymap *keymap)
{
  struct xkb_keymap *xkb_keymap;

  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);

  return xkb_keymap_layout_get_name (xkb_keymap, get_active_layout (keymap));
}

static void
keyboard_handle_keymap (void               *data,
                        struct wl_keyboard *keyboard,
                        uint32_t            format,
                        int                 fd,
                        uint32_t            size)
{
  GdkWaylandSeat *seat = data;
  PangoDirection direction;
  gboolean bidi;
  gboolean caps_lock;
  gboolean num_lock;
  gboolean scroll_lock;
  GdkModifierType modifiers;

  direction = gdk_keymap_get_direction (seat->keymap);
  bidi = gdk_keymap_have_bidi_layouts (seat->keymap);
  caps_lock = gdk_keymap_get_caps_lock_state (seat->keymap);
  num_lock = gdk_keymap_get_num_lock_state (seat->keymap);
  scroll_lock = gdk_keymap_get_scroll_lock_state (seat->keymap);
  modifiers = gdk_keymap_get_modifier_state (seat->keymap);

  _gdk_wayland_keymap_update_from_fd (seat->keymap, format, fd, size);

  if (GDK_DISPLAY_DEBUG_CHECK (seat->keymap->display, INPUT))
    {
      GString *s = g_string_new ("");
      struct xkb_keymap *xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (seat->keymap);
      struct xkb_state *xkb_state = _gdk_wayland_keymap_get_xkb_state (seat->keymap);
      for (int i = 0; i < xkb_keymap_num_layouts (xkb_keymap); i++)
        {
          if (s->len > 0)
            g_string_append (s, ", ");
          if (xkb_state_layout_index_is_active (xkb_state, i, XKB_STATE_LAYOUT_EFFECTIVE))
            g_string_append (s, "*");
          g_string_append (s, xkb_keymap_layout_get_name (xkb_keymap, i));
        }
      gdk_debug_message ("layouts: %s", s->str);
      g_string_free (s, TRUE);
    }

  g_signal_emit_by_name (seat->keymap, "keys-changed");
  g_signal_emit_by_name (seat->keymap, "state-changed");
  if (direction != gdk_keymap_get_direction (seat->keymap))
    g_signal_emit_by_name (seat->keymap, "direction-changed");

  if (direction != gdk_keymap_get_direction (seat->keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "direction");
  if (bidi != gdk_keymap_have_bidi_layouts (seat->keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "has-bidi-layouts");
  if (caps_lock != gdk_keymap_get_caps_lock_state (seat->keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "caps-lock-state");
  if (num_lock != gdk_keymap_get_num_lock_state (seat->keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "num-lock-state");
  if (scroll_lock != gdk_keymap_get_scroll_lock_state (seat->keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "scroll-lock-state");
  if (modifiers != gdk_keymap_get_modifier_state (seat->keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "modifier-state");
}

static void
keyboard_handle_enter (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface,
                       struct wl_array    *keys)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;

  if (!surface)
    return;

  if (!GDK_IS_SURFACE (wl_surface_get_user_data (surface)))
    return;

  seat->keyboard_focus = wl_surface_get_user_data (surface);
  g_object_ref (seat->keyboard_focus);
  seat->repeat_key = 0;

  event = gdk_focus_event_new (seat->keyboard_focus,
                               seat->logical_keyboard,
                               TRUE);

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "focus in, seat %p surface %p",
                  seat, seat->keyboard_focus);

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void stop_key_repeat (GdkWaylandSeat *seat);

static void
keyboard_handle_leave (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;

  if (!seat->keyboard_focus)
    return;

  /* gdk_surface_is_destroyed() might already return TRUE for
   * seat->keyboard_focus here, which would happen if we destroyed the
   * surface before losing keyboard focus.
   */
  stop_key_repeat (seat);

  event = gdk_focus_event_new (seat->keyboard_focus,
                               seat->logical_keyboard,
                               FALSE);

  g_object_unref (seat->keyboard_focus);
  seat->keyboard_focus = NULL;
  seat->repeat_key = 0;
  seat->key_modifiers = 0;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "focus out, seat %p surface %p",
                  seat, gdk_event_get_surface (event));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static gboolean keyboard_repeat (gpointer data);

static gboolean
get_key_repeat (GdkWaylandSeat *seat,
                guint          *delay,
                guint          *interval)
{
  gboolean repeat;

  if (seat->have_server_repeat)
    {
      if (seat->server_repeat_rate > 0)
        {
          repeat = TRUE;
          *delay = seat->server_repeat_delay;
          *interval = (1000 / seat->server_repeat_rate);
        }
      else
        {
          repeat = FALSE;
        }
    }
  else
    {
      repeat = TRUE;
      *delay = 400;
      *interval = 80;
    }

  return repeat;
}

static void
stop_key_repeat (GdkWaylandSeat *seat)
{
  if (seat->repeat_timer)
    {
      g_source_remove (seat->repeat_timer);
      seat->repeat_timer = 0;
    }

  g_clear_pointer (&seat->repeat_callback, wl_callback_destroy);
}

static void
deliver_key_event (GdkWaylandSeat *seat,
                   uint32_t        time_,
                   uint32_t        key,
                   uint32_t        state,
                   gboolean        from_key_repeat)
{
  GdkEvent *event;
  struct xkb_state *xkb_state;
  struct xkb_keymap *xkb_keymap;
  GdkKeymap *keymap;
  guint delay, interval, timeout;
  gint64 begin_time, now;
  xkb_mod_mask_t consumed;
  GdkTranslatedKey translated;
  GdkTranslatedKey no_lock;
  xkb_mod_mask_t modifiers;
  xkb_mod_index_t caps_lock;

  begin_time = g_get_monotonic_time ();

  stop_key_repeat (seat);

  keymap = seat->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);

  translated.keyval = xkb_state_key_get_one_sym (xkb_state, key);
  modifiers = xkb_state_serialize_mods (xkb_state, XKB_STATE_MODS_EFFECTIVE);
  consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (xkb_state, key, modifiers);
  translated.consumed = gdk_wayland_keymap_get_gdk_modifiers (keymap, consumed);
  translated.layout = xkb_state_key_get_layout (xkb_state, key);
  translated.level = xkb_state_key_get_level (xkb_state, key, translated.layout);

  if (translated.keyval == XKB_KEY_NoSymbol)
    return;

  seat->pointer_info.time = time_;
  seat->key_modifiers = gdk_keymap_get_modifier_state (keymap);


  modifiers = xkb_state_serialize_mods (xkb_state, XKB_STATE_MODS_EFFECTIVE);
  caps_lock = xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS);
  if (modifiers & (1 << caps_lock))
    {
      struct xkb_state *tmp_state = xkb_state_new (xkb_keymap);
      xkb_layout_index_t layout;

      modifiers &= ~(1 << caps_lock);
      layout = xkb_state_serialize_layout (xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
      xkb_state_update_mask (tmp_state, modifiers, 0, 0, layout, 0, 0);

      no_lock.keyval = xkb_state_key_get_one_sym (tmp_state, key);
      consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (tmp_state, key, modifiers);
      no_lock.consumed = gdk_wayland_keymap_get_gdk_modifiers (keymap, consumed);
      no_lock.layout = xkb_state_key_get_layout (tmp_state, key);
      no_lock.level = xkb_state_key_get_level (tmp_state, key, no_lock.layout);

      xkb_state_unref (tmp_state);
    }
  else
    {
      no_lock = translated;
    }

  event = gdk_key_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE,
                             seat->keyboard_focus,
                             seat->logical_keyboard,
                             time_,
                             key,
                             gdk_wayland_device_get_modifiers (seat->logical_pointer),
                             _gdk_wayland_keymap_key_is_modifier (keymap, key),
                             &translated,
                             &no_lock,
                             NULL);

  _gdk_wayland_display_deliver_event (seat->display, event);

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "keyboard %s event%s, surface %p, code %d, sym %d, "
                  "mods 0x%x, consumed 0x%x, layout %d level %d",
                  (state ? "press" : "release"),
                  (from_key_repeat ? " (repeat)" : ""),
                  gdk_event_get_surface (event),
                  gdk_key_event_get_keycode (event),
                  gdk_key_event_get_keyval (event),
                  gdk_event_get_modifier_state (event),
                  gdk_key_event_get_consumed_modifiers (event),
                  gdk_key_event_get_layout (event),
                  gdk_key_event_get_level (event));

  if (!xkb_keymap_key_repeats (xkb_keymap, key))
    return;

  if (!get_key_repeat (seat, &delay, &interval))
    return;

  if (!from_key_repeat)
    {
      if (state) /* Another key is pressed */
        {
          seat->repeat_key = key;
        }
      else if (seat->repeat_key == key) /* Repeated key is released */
        {
          seat->repeat_key = 0;
        }
    }

  if (!seat->repeat_key)
    return;

  seat->repeat_count++;

  interval *= 1000L;
  delay *= 1000L;

  now = g_get_monotonic_time ();

  if (seat->repeat_count == 1)
    seat->repeat_deadline = begin_time + delay;
  else if (seat->repeat_deadline + interval > now)
    seat->repeat_deadline += interval;
  else
    /* frame delay caused us to miss repeat deadline */
    seat->repeat_deadline = now;

  timeout = (seat->repeat_deadline - now) / 1000L;

  seat->repeat_timer = g_timeout_add (timeout, keyboard_repeat, seat);
  gdk_source_set_static_name_by_id (seat->repeat_timer, "[gtk] keyboard_repeat");
}

static void
sync_after_repeat_callback (void               *data,
                            struct wl_callback *callback,
                            uint32_t            time)
{
  GdkWaylandSeat *seat = data;

  g_clear_pointer (&seat->repeat_callback, wl_callback_destroy);
  deliver_key_event (seat, seat->keyboard_time, seat->repeat_key, 1, TRUE);
}

static const struct wl_callback_listener sync_after_repeat_callback_listener = {
  sync_after_repeat_callback
};

static gboolean
keyboard_repeat (gpointer data)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  /* Ping the server and wait for the timeout.  We won't process
   * key repeat until it responds, since a hung server could lead
   * to a delayed key release event. We don't want to generate
   * repeat events long after the user released the key, just because
   * the server is tardy in telling us the user released the key.
   */
  seat->repeat_callback = wl_display_sync (display->wl_display);

  wl_callback_add_listener (seat->repeat_callback,
                            &sync_after_repeat_callback_listener,
                            seat);

  seat->repeat_timer = 0;
  return G_SOURCE_REMOVE;
}

static void
keyboard_handle_key (void               *data,
                     struct wl_keyboard *keyboard,
                     uint32_t            serial,
                     uint32_t            time,
                     uint32_t            key,
                     uint32_t            state_w)
{
  GdkWaylandSeat *seat = data;

  if (!seat->keyboard_focus)
    return;

  seat->keyboard_time = time;
  seat->keyboard_key_serial = serial;
  seat->repeat_count = 0;
  deliver_key_event (data, time, key + 8, state_w, FALSE);

}

static void
keyboard_handle_modifiers (void               *data,
                           struct wl_keyboard *keyboard,
                           uint32_t            serial,
                           uint32_t            mods_depressed,
                           uint32_t            mods_latched,
                           uint32_t            mods_locked,
                           uint32_t            group)
{
  GdkWaylandSeat *seat = data;
  GdkKeymap *keymap;
  struct xkb_state *xkb_state;
  PangoDirection direction;
  gboolean bidi;
  gboolean caps_lock;
  gboolean num_lock;
  gboolean scroll_lock;
  GdkModifierType modifiers;
  int layout;

  keymap = seat->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);

  direction = gdk_keymap_get_direction (keymap);
  bidi = gdk_keymap_have_bidi_layouts (keymap);
  caps_lock = gdk_keymap_get_caps_lock_state (keymap);
  num_lock = gdk_keymap_get_num_lock_state (keymap);
  scroll_lock = gdk_keymap_get_scroll_lock_state (keymap);
  modifiers = gdk_keymap_get_modifier_state (keymap);
  layout = get_active_layout (keymap);

  /* Note: the docs for xkb_state_update mask state that all parameters
   * must be passed, or we may end up with an 'incoherent' state. But the
   * Wayland modifiers event only includes a single group field, so we
   * can't pass depressed/latched/locked groups.
   *
   * We assume that the compositor is sending us the 'effective' group
   * (the protocol is not clear on that point), and pass it as the depressed
   * group - we are basically pretending that the user holds down a key for
   * this group at all times.
   *
   * This means that our xkb_state would answer a few questions differently
   * from the compositors xkb_state - e.g. if you asked it about the latched
   * group. But nobody is asking it those questions, so it does not really
   * matter. We hope.
   */
  xkb_state_update_mask (xkb_state, mods_depressed, mods_latched, mods_locked, group, 0, 0);

  seat->key_modifiers = gdk_keymap_get_modifier_state (keymap);

  g_signal_emit_by_name (keymap, "state-changed");
  if (layout != get_active_layout (keymap))
    {
      GDK_DISPLAY_DEBUG (keymap->display, INPUT, "active layout now: %s", get_active_layout_name (keymap));

      g_signal_emit_by_name (keymap, "keys-changed");
    }
  if (direction != gdk_keymap_get_direction (keymap))
    {
      g_signal_emit_by_name (keymap, "direction-changed");
      g_object_notify (G_OBJECT (seat->logical_keyboard), "direction");
    }
  if (bidi != gdk_keymap_have_bidi_layouts (keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "has-bidi-layouts");
  if (caps_lock != gdk_keymap_get_caps_lock_state (keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "caps-lock-state");
  if (num_lock != gdk_keymap_get_num_lock_state (keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "num-lock-state");
  if (scroll_lock != gdk_keymap_get_scroll_lock_state (keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "scroll-lock-state");
  if (modifiers != gdk_keymap_get_modifier_state (keymap))
    g_object_notify (G_OBJECT (seat->logical_keyboard), "modifier-state");
}

static void
keyboard_handle_repeat_info (void               *data,
                             struct wl_keyboard *keyboard,
                             int32_t             rate,
                             int32_t             delay)
{
  GdkWaylandSeat *seat = data;

  seat->have_server_repeat = TRUE;
  seat->server_repeat_rate = rate;
  seat->server_repeat_delay = delay;
}

static GdkWaylandTouchData *
gdk_wayland_seat_add_touch (GdkWaylandSeat    *seat,
                            uint32_t           id,
                            struct wl_surface *surface)
{
  GdkWaylandTouchData *touch;

  touch = g_new0 (GdkWaylandTouchData, 1);
  touch->id = id;
  touch->surface = wl_surface_get_user_data (surface);
  touch->initial_touch = (g_hash_table_size (seat->touches) == 0);

  g_hash_table_insert (seat->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

GdkWaylandTouchData *
gdk_wayland_seat_get_touch (GdkWaylandSeat *seat,
                            uint32_t        id)
{
  return g_hash_table_lookup (seat->touches, GUINT_TO_POINTER (id));
}

static void
gdk_wayland_seat_remove_touch (GdkWaylandSeat *seat,
                               uint32_t        id)
{
  g_hash_table_remove (seat->touches, GUINT_TO_POINTER (id));
}

void
gdk_wayland_seat_clear_touchpoints (GdkWaylandSeat *seat,
                                    GdkSurface     *surface)
{
  GHashTableIter iter;
  GdkWaylandTouchData *touch;

  g_hash_table_iter_init (&iter, seat->touches);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      if (touch->surface == surface)
        g_hash_table_iter_remove (&iter);
    }
}

static void
mimic_pointer_emulating_touch_info (GdkDevice           *device,
                                    GdkWaylandTouchData *touch)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;

  pointer = gdk_wayland_device_get_pointer (wayland_device);
  g_set_object (&pointer->focus, touch->surface);
  pointer->press_serial = pointer->enter_serial = touch->touch_down_serial;
  pointer->surface_x = touch->x;
  pointer->surface_y = touch->y;
}

static void
touch_handle_logical_pointer_crossing (GdkWaylandSeat      *seat,
                                       GdkWaylandTouchData *touch,
                                       uint32_t             time)
{
  GdkWaylandPointerData *pointer;

  pointer = gdk_wayland_device_get_pointer (GDK_WAYLAND_DEVICE (seat->logical_touch));

  if (pointer->focus == touch->surface)
    return;

  if (pointer->focus)
    {
      emulate_touch_crossing (pointer->focus, NULL,
                              seat->logical_touch, seat->touch, touch,
                              GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL, time);
    }

  if (touch->surface)
    {
      emulate_touch_crossing (touch->surface, NULL,
                              seat->logical_touch, seat->touch, touch,
                              GDK_ENTER_NOTIFY, GDK_CROSSING_NORMAL, time);
    }
}

static void
touch_handle_down (void              *data,
                   struct wl_touch   *wl_touch,
                   uint32_t           serial,
                   uint32_t           time,
                   struct wl_surface *wl_surface,
                   int32_t            id,
                   wl_fixed_t         x,
                   wl_fixed_t         y)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  if (!wl_surface)
    return;

  touch = gdk_wayland_seat_add_touch (seat, id, wl_surface);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);
  touch->touch_down_serial = serial;
  seat->latest_touch_down_serial = serial;

  event = gdk_touch_event_new (GDK_TOUCH_BEGIN,
                               GDK_SLOT_TO_EVENT_SEQUENCE (touch->id),
                               touch->surface,
                               seat->logical_touch,
                               time,
                               gdk_wayland_device_get_modifiers (seat->logical_touch),
                               touch->x, touch->y,
                               NULL,
                               touch->initial_touch);

  if (touch->initial_touch)
    {
      touch_handle_logical_pointer_crossing (seat, touch, time);
      gdk_wayland_device_set_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch),
                                              touch);
      mimic_pointer_emulating_touch_info (seat->logical_touch, touch);
    }

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double xx, yy;
      gdk_event_get_position (event, &xx, &yy);
      g_message ("touch begin %f %f", xx, yy);
    }

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
touch_handle_up (void            *data,
                 struct wl_touch *wl_touch,
                 uint32_t         serial,
                 uint32_t         time,
                 int32_t          id)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = gdk_wayland_seat_get_touch (seat, id);
  if (!touch)
    return;

  event = gdk_touch_event_new (GDK_TOUCH_END,
                               GDK_SLOT_TO_EVENT_SEQUENCE (touch->id),
                               touch->surface,
                               seat->logical_touch,
                               time,
                               gdk_wayland_device_get_modifiers (seat->logical_touch),
                               touch->x, touch->y,
                               NULL,
                               touch->initial_touch);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double x, y;
      gdk_event_get_position (event, &x, &y);
      g_message ("touch end %f %f", x, y);
    }

  _gdk_wayland_display_deliver_event (seat->display, event);

  if (touch->initial_touch)
    gdk_wayland_device_set_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch),
                                            NULL);

  gdk_wayland_seat_remove_touch (seat, id);
}

static void
touch_handle_motion (void            *data,
                     struct wl_touch *wl_touch,
                     uint32_t         time,
                     int32_t          id,
                     wl_fixed_t       x,
                     wl_fixed_t       y)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = gdk_wayland_seat_get_touch (seat, id);
  if (!touch)
    return;

  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);

  if (touch->initial_touch)
    mimic_pointer_emulating_touch_info (seat->logical_touch, touch);

  event = gdk_touch_event_new (GDK_TOUCH_UPDATE,
                               GDK_SLOT_TO_EVENT_SEQUENCE (touch->id),
                               touch->surface,
                               seat->logical_touch,
                               time,
                               gdk_wayland_device_get_modifiers (seat->logical_touch),
                               touch->x, touch->y,
                               NULL,
                               touch->initial_touch);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double xx, yy;
      gdk_event_get_position (event, &xx, &yy);
      g_message ("touch update %f %f", xx, yy);
    }

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
touch_handle_frame (void            *data,
                    struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel (void            *data,
                     struct wl_touch *wl_touch)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GdkEvent *event;

  gdk_wayland_device_set_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch),
                                          NULL);

  g_hash_table_iter_init (&iter, seat->touches);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      event = gdk_touch_event_new (GDK_TOUCH_CANCEL,
                                   GDK_SLOT_TO_EVENT_SEQUENCE (touch->id),
                                   touch->surface,
                                   seat->logical_touch,
                                   GDK_CURRENT_TIME,
                                   gdk_wayland_device_get_modifiers (seat->logical_touch),
                                   touch->x, touch->y,
                                   NULL,
                                   touch->initial_touch);
      _gdk_wayland_display_deliver_event (seat->display, event);
      g_hash_table_iter_remove (&iter);
    }

  GDK_SEAT_DEBUG (seat, EVENTS, "touch cancel");
}

static void
touch_handle_shape (void            *data,
                    struct wl_touch *touch,
                    int32_t         id,
                    wl_fixed_t      major,
                    wl_fixed_t      minor)
{
}

static void
touch_handle_orientation (void            *data,
                          struct wl_touch *touch,
                          int32_t         id,
                          wl_fixed_t      orientation)
{
}

static void
emit_gesture_swipe_event (GdkWaylandSeat          *seat,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint32                  n_fingers,
                          double                   dx,
                          double                   dy)
{
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = _time;

  if (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)
    seat->pointer_info.touchpad_event_sequence++;

  event = gdk_touchpad_event_new_swipe (seat->pointer_info.focus,
                                        GDK_SLOT_TO_EVENT_SEQUENCE (seat->pointer_info.touchpad_event_sequence),
                                        seat->logical_pointer,
                                        _time,
                                        gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                        phase,
                                        seat->pointer_info.surface_x,
                                        seat->pointer_info.surface_y,
                                        n_fingers,
                                        dx, dy);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double x, y;
      gdk_event_get_position (event, &x, &y);
      g_message ("swipe event %d, coords: %f %f, seat %p state %d",
                 gdk_event_get_event_type (event), x, y, seat,
                 gdk_event_get_modifier_state (event));
    }

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_swipe_begin (void                                *data,
                     struct zwp_pointer_gesture_swipe_v1 *swipe,
                     uint32_t                             serial,
                     uint32_t                             time,
                     struct wl_surface                   *surface,
                     uint32_t                             fingers)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_swipe_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_swipe_update (void                                *data,
                      struct zwp_pointer_gesture_swipe_v1 *swipe,
                      uint32_t                             time,
                      wl_fixed_t                           dx,
                      wl_fixed_t                           dy)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_swipe_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE,
                            time,
                            seat->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy));
}

static void
gesture_swipe_end (void                                *data,
                   struct zwp_pointer_gesture_swipe_v1 *swipe,
                   uint32_t                             serial,
                   uint32_t                             time,
                   int32_t                              cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkTouchpadGesturePhase phase;

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_swipe_event (seat, phase, time,
                            seat->gesture_n_fingers, 0, 0);
}

static void
emit_gesture_pinch_event (GdkWaylandSeat          *seat,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint                    n_fingers,
                          double                   dx,
                          double                   dy,
                          double                   scale,
                          double                   angle_delta)
{
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = _time;

  if (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)
    seat->pointer_info.touchpad_event_sequence++;

  event = gdk_touchpad_event_new_pinch (seat->pointer_info.focus,
                                        GDK_SLOT_TO_EVENT_SEQUENCE (seat->pointer_info.touchpad_event_sequence),
                                        seat->logical_pointer,
                                        _time,
                                        gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                        phase,
                                        seat->pointer_info.surface_x,
                                        seat->pointer_info.surface_y,
                                        n_fingers,
                                        dx, dy,
                                        scale, angle_delta * G_PI / 180);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double x, y;
      gdk_event_get_position (event, &x, &y);
      g_message ("pinch event %d, coords: %f %f, seat %p state %d",
                       gdk_event_get_event_type (event),
                       x, y, seat,
                       gdk_event_get_modifier_state (event));
    }

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_pinch_begin (void                                *data,
                     struct zwp_pointer_gesture_pinch_v1 *pinch,
                     uint32_t                             serial,
                     uint32_t                             time,
                     struct wl_surface                   *surface,
                     uint32_t                             fingers)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_pinch_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0, 1, 0);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_pinch_update (void                                *data,
                      struct zwp_pointer_gesture_pinch_v1 *pinch,
                      uint32_t                             time,
                      wl_fixed_t                           dx,
                      wl_fixed_t                           dy,
                      wl_fixed_t                           scale,
                      wl_fixed_t                           rotation)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_pinch_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE, time,
                            seat->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy),
                            wl_fixed_to_double (scale),
                            wl_fixed_to_double (rotation));
}

static void
gesture_pinch_end (void                                *data,
                   struct zwp_pointer_gesture_pinch_v1 *pinch,
                   uint32_t                             serial,
                   uint32_t                             time,
                   int32_t                              cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkTouchpadGesturePhase phase;

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_pinch_event (seat, phase,
                            time, seat->gesture_n_fingers,
                            0, 0, 1, 0);
}

static void
emit_gesture_hold_event (GdkWaylandSeat          *seat,
                         GdkTouchpadGesturePhase  phase,
                         guint32                  _time,
                         guint32                  n_fingers)
{
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = _time;

  if (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)
    seat->pointer_info.touchpad_event_sequence++;

  event = gdk_touchpad_event_new_hold (seat->pointer_info.focus,
                                       GDK_SLOT_TO_EVENT_SEQUENCE (seat->pointer_info.touchpad_event_sequence),
                                       seat->logical_pointer,
                                       _time,
                                       gdk_wayland_device_get_modifiers (seat->logical_pointer),
                                       phase,
                                       seat->pointer_info.surface_x,
                                       seat->pointer_info.surface_y,
                                       n_fingers);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_seat_get_display (GDK_SEAT (seat)), EVENTS))
    {
      double x, y;
      gdk_event_get_position (event, &x, &y);
      g_message ("hold event %d, coords: %f %f, seat %p state %d",
                 gdk_event_get_event_type (event),
                 x, y, seat,
                 gdk_event_get_modifier_state (event));
    }

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_hold_begin (void                               *data,
                    struct zwp_pointer_gesture_hold_v1 *hold,
                    uint32_t                            serial,
                    uint32_t                            time,
                    struct wl_surface                  *surface,
                    uint32_t                            fingers)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_hold_event (seat,
                           GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                           time, fingers);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_hold_end (void                               *data,
                  struct zwp_pointer_gesture_hold_v1 *hold,
                  uint32_t                            serial,
                  uint32_t                            time,
                  int32_t                             cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkTouchpadGesturePhase phase;

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_hold_event (seat, phase, time,
                           seat->gesture_n_fingers);
}

static void
_gdk_wayland_seat_remove_tool (GdkWaylandSeat           *seat,
                               GdkWaylandTabletToolData *tool)
{
  seat->tablet_tools = g_list_remove (seat->tablet_tools, tool);

  gdk_seat_tool_removed (GDK_SEAT (seat), tool->tool);

  zwp_tablet_tool_v2_destroy (tool->wp_tablet_tool);
  g_object_unref (tool->tool);
  g_free (tool);
}

static void
_gdk_wayland_seat_remove_tablet (GdkWaylandSeat       *seat,
                                 GdkWaylandTabletData *tablet)
{
  seat->tablets = g_list_remove (seat->tablets, tablet);

  gdk_seat_device_removed (GDK_SEAT (seat), tablet->stylus_device);
  gdk_seat_device_removed (GDK_SEAT (seat), tablet->logical_device);

  while (tablet->pads)
    {
      GdkWaylandTabletPadData *pad = tablet->pads->data;

      pad->current_tablet = NULL;
      tablet->pads = g_list_remove (tablet->pads, pad);
    }

  zwp_tablet_v2_destroy (tablet->wp_tablet);

  _gdk_device_set_associated_device (tablet->logical_device, NULL);
  _gdk_device_set_associated_device (tablet->stylus_device, NULL);

  if (tablet->pointer_info.focus)
    g_object_unref (tablet->pointer_info.focus);

  wl_surface_destroy (tablet->pointer_info.pointer_surface);
  g_object_unref (tablet->logical_device);
  g_object_unref (tablet->stylus_device);
  g_free (tablet);
}

static void
_gdk_wayland_seat_remove_tablet_pad (GdkWaylandSeat          *seat,
                                     GdkWaylandTabletPadData *pad)
{
  seat->tablet_pads = g_list_remove (seat->tablet_pads, pad);

  if (pad->device)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), pad->device);
      _gdk_device_set_associated_device (pad->device, NULL);
      g_object_unref (pad->device);
    }

  g_free (pad);
}

static GdkWaylandTabletPadGroupData *
tablet_pad_lookup_button_group (GdkWaylandTabletPadData *pad,
                                uint32_t                 button)
{
  GdkWaylandTabletPadGroupData *group;
  GList *l;

  for (l = pad->mode_groups; l; l = l->next)
    {
      group = l->data;

      if (g_list_find (group->buttons, GUINT_TO_POINTER (button)))
        return group;
    }

  return NULL;
}

static void
tablet_handle_name (void                 *data,
                    struct zwp_tablet_v2 *wp_tablet,
                    const char           *name)
{
  GdkWaylandTabletData *tablet = data;

  tablet->name = g_strdup (name);
}

static void
tablet_handle_id (void                 *data,
                  struct zwp_tablet_v2 *wp_tablet,
                  uint32_t              vid,
                  uint32_t              pid)
{
  GdkWaylandTabletData *tablet = data;

  tablet->vid = vid;
  tablet->pid = pid;
}

static void
tablet_handle_path (void                 *data,
                    struct zwp_tablet_v2 *wp_tablet,
                    const char           *path)
{
  GdkWaylandTabletData *tablet = data;

  tablet->path = g_strdup (path);
}

static void
tablet_handle_done (void                 *data,
                    struct zwp_tablet_v2 *wp_tablet)
{
  GdkWaylandTabletData *tablet = data;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tablet->seat);
  GdkDisplay *display = gdk_seat_get_display (GDK_SEAT (seat));
  GdkDevice *logical_device, *stylus_device;
  char *logical_name;
  char *vid, *pid;

  vid = g_strdup_printf ("%.4x", tablet->vid);
  pid = g_strdup_printf ("%.4x", tablet->pid);

  logical_name = g_strdup_printf ("Logical pointer for %s", tablet->name);
  logical_device = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                 "name", logical_name,
                                 "source", GDK_SOURCE_MOUSE,
                                 "has-cursor", TRUE,
                                 "display", display,
                                 "seat", seat,
                                 NULL);

  gdk_wayland_device_set_pointer (GDK_WAYLAND_DEVICE (logical_device),
                                  &tablet->pointer_info);

  stylus_device = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                "name", tablet->name,
                                "source", GDK_SOURCE_PEN,
                                "has-cursor", FALSE,
                                "display", display,
                                "seat", seat,
                                "vendor-id", vid,
                                "product-id", pid,
                                NULL);

  tablet->logical_device = logical_device;
  init_pointer_data (&tablet->pointer_info, display, tablet->logical_device);

  tablet->stylus_device = stylus_device;

  _gdk_device_set_associated_device (logical_device, seat->logical_keyboard);
  _gdk_device_set_associated_device (stylus_device, logical_device);

  gdk_seat_device_added (GDK_SEAT (seat), logical_device);
  gdk_seat_device_added (GDK_SEAT (seat), stylus_device);

  g_free (logical_name);
  g_free (vid);
  g_free (pid);
}

static void
tablet_handle_removed (void                 *data,
                       struct zwp_tablet_v2 *wp_tablet)
{
  GdkWaylandTabletData *tablet = data;

  _gdk_wayland_seat_remove_tablet (GDK_WAYLAND_SEAT (tablet->seat), tablet);
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
  pointer_handle_frame,
  pointer_handle_axis_source,
  pointer_handle_axis_stop,
  pointer_handle_axis_discrete,
  pointer_handle_axis_value120,
};

static const struct wl_keyboard_listener keyboard_listener = {
  keyboard_handle_keymap,
  keyboard_handle_enter,
  keyboard_handle_leave,
  keyboard_handle_key,
  keyboard_handle_modifiers,
  keyboard_handle_repeat_info,
};

static const struct wl_touch_listener touch_listener = {
  touch_handle_down,
  touch_handle_up,
  touch_handle_motion,
  touch_handle_frame,
  touch_handle_cancel,
  touch_handle_shape,
  touch_handle_orientation,
};

static const struct zwp_pointer_gesture_swipe_v1_listener gesture_swipe_listener = {
  gesture_swipe_begin,
  gesture_swipe_update,
  gesture_swipe_end
};

static const struct zwp_pointer_gesture_pinch_v1_listener gesture_pinch_listener = {
  gesture_pinch_begin,
  gesture_pinch_update,
  gesture_pinch_end
};

static const struct zwp_pointer_gesture_hold_v1_listener gesture_hold_listener = {
  gesture_hold_begin,
  gesture_hold_end
};

static const struct zwp_tablet_v2_listener tablet_listener = {
  tablet_handle_name,
  tablet_handle_id,
  tablet_handle_path,
  tablet_handle_done,
  tablet_handle_removed,
};

static void
seat_handle_capabilities (void                    *data,
                          struct wl_seat          *wl_seat,
                          enum wl_seat_capability  caps)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);

  GDK_SEAT_DEBUG (seat, MISC,
                  "seat %p with %s%s%s", wl_seat,
                  (caps & WL_SEAT_CAPABILITY_POINTER) ? " pointer, " : "",
                  (caps & WL_SEAT_CAPABILITY_KEYBOARD) ? " keyboard, " : "",
                  (caps & WL_SEAT_CAPABILITY_TOUCH) ? " touch" : "");

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !seat->wl_pointer)
    {
      seat->wl_pointer = wl_seat_get_pointer (wl_seat);
      wl_pointer_set_user_data (seat->wl_pointer, seat);
      wl_pointer_add_listener (seat->wl_pointer, &pointer_listener, seat);

      seat->pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                    "name", "Wayland Pointer",
                                    "source", GDK_SOURCE_MOUSE,
                                    "has-cursor", TRUE,
                                    "display", seat->display,
                                    "seat", seat,
                                    NULL);
      _gdk_device_set_associated_device (seat->pointer, seat->logical_pointer);
      gdk_seat_device_added (GDK_SEAT (seat), seat->pointer);

      if (display_wayland->pointer_gestures)
        {
          seat->wp_pointer_gesture_swipe =
            zwp_pointer_gestures_v1_get_swipe_gesture (display_wayland->pointer_gestures,
                                                       seat->wl_pointer);
          zwp_pointer_gesture_swipe_v1_set_user_data (seat->wp_pointer_gesture_swipe,
                                                      seat);
          zwp_pointer_gesture_swipe_v1_add_listener (seat->wp_pointer_gesture_swipe,
                                                     &gesture_swipe_listener, seat);

          seat->wp_pointer_gesture_pinch =
            zwp_pointer_gestures_v1_get_pinch_gesture (display_wayland->pointer_gestures,
                                                       seat->wl_pointer);
          zwp_pointer_gesture_pinch_v1_set_user_data (seat->wp_pointer_gesture_pinch,
                                                      seat);
          zwp_pointer_gesture_pinch_v1_add_listener (seat->wp_pointer_gesture_pinch,
                                                     &gesture_pinch_listener, seat);

          if (zwp_pointer_gestures_v1_get_version (display_wayland->pointer_gestures) >= ZWP_POINTER_GESTURES_V1_GET_HOLD_GESTURE_SINCE_VERSION)
            {
              seat->wp_pointer_gesture_hold =
                  zwp_pointer_gestures_v1_get_hold_gesture (display_wayland->pointer_gestures,
                                                            seat->wl_pointer);
              zwp_pointer_gesture_hold_v1_set_user_data (seat->wp_pointer_gesture_hold,
                                                         seat);
              zwp_pointer_gesture_hold_v1_add_listener (seat->wp_pointer_gesture_hold,
                                                        &gesture_hold_listener, seat);
            }
        }
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && seat->wl_pointer)
    {
      g_clear_pointer (&seat->wp_pointer_gesture_swipe,
                       zwp_pointer_gesture_swipe_v1_destroy);
      g_clear_pointer (&seat->wp_pointer_gesture_pinch,
                       zwp_pointer_gesture_pinch_v1_destroy);

      wl_pointer_release (seat->wl_pointer);
      seat->wl_pointer = NULL;
      gdk_seat_device_removed (GDK_SEAT (seat), seat->pointer);
      _gdk_device_set_associated_device (seat->pointer, NULL);

      g_clear_object (&seat->pointer);

      if (seat->wheel_scrolling)
        {
          gdk_seat_device_removed (GDK_SEAT (seat), seat->wheel_scrolling);
          _gdk_device_set_associated_device (seat->wheel_scrolling, NULL);

          g_clear_object (&seat->wheel_scrolling);
        }

      if (seat->finger_scrolling)
        {
          gdk_seat_device_removed (GDK_SEAT (seat), seat->finger_scrolling);
          _gdk_device_set_associated_device (seat->finger_scrolling, NULL);

          g_clear_object (&seat->finger_scrolling);
        }

      if (seat->continuous_scrolling)
        {
          gdk_seat_device_removed (GDK_SEAT (seat), seat->continuous_scrolling);
          _gdk_device_set_associated_device (seat->continuous_scrolling, NULL);

          g_clear_object (&seat->continuous_scrolling);
        }
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !seat->wl_keyboard)
    {
      seat->wl_keyboard = wl_seat_get_keyboard (wl_seat);
      wl_keyboard_set_user_data (seat->wl_keyboard, seat);
      wl_keyboard_add_listener (seat->wl_keyboard, &keyboard_listener, seat);

      seat->keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                     "name", "Wayland Keyboard",
                                     "source", GDK_SOURCE_KEYBOARD,
                                     "has-cursor", FALSE,
                                     "display", seat->display,
                                     "seat", seat,
                                     NULL);
      _gdk_device_reset_axes (seat->keyboard);
      _gdk_device_set_associated_device (seat->keyboard, seat->logical_keyboard);
      gdk_seat_device_added (GDK_SEAT (seat), seat->keyboard);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && seat->wl_keyboard)
    {
      wl_keyboard_release (seat->wl_keyboard);
      seat->wl_keyboard = NULL;
      gdk_seat_device_removed (GDK_SEAT (seat), seat->keyboard);
      _gdk_device_set_associated_device (seat->keyboard, NULL);

      g_clear_object (&seat->keyboard);
    }

  if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !seat->wl_touch)
    {
      seat->wl_touch = wl_seat_get_touch (wl_seat);
      wl_touch_set_user_data (seat->wl_touch, seat);
      wl_touch_add_listener (seat->wl_touch, &touch_listener, seat);

      seat->logical_touch = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                         "name", "Wayland Touch Logical Pointer",
                                         "source", GDK_SOURCE_TOUCHSCREEN,
                                         "has-cursor", TRUE,
                                         "display", seat->display,
                                         "seat", seat,
                                         NULL);

      gdk_wayland_device_set_pointer (GDK_WAYLAND_DEVICE (seat->logical_touch),
                                      &seat->touch_info);
      _gdk_device_set_associated_device (seat->logical_touch, seat->logical_keyboard);
      gdk_seat_device_added (GDK_SEAT (seat), seat->logical_touch);

      seat->touch = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                  "name", "Wayland Touch",
                                  "source", GDK_SOURCE_TOUCHSCREEN,
                                  "has-cursor", FALSE,
                                  "display", seat->display,
                                  "seat", seat,
                                  NULL);
      _gdk_device_set_associated_device (seat->touch, seat->logical_touch);
      gdk_seat_device_added (GDK_SEAT (seat), seat->touch);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && seat->wl_touch)
    {
      wl_touch_release (seat->wl_touch);
      seat->wl_touch = NULL;
      gdk_seat_device_removed (GDK_SEAT (seat), seat->touch);
      gdk_seat_device_removed (GDK_SEAT (seat), seat->logical_touch);
      _gdk_device_set_associated_device (seat->logical_touch, NULL);
      _gdk_device_set_associated_device (seat->touch, NULL);

      g_clear_object (&seat->logical_touch);
      g_clear_object (&seat->touch);
    }
}

static GdkDevice *
get_scroll_device (GdkWaylandSeat              *seat,
                   enum wl_pointer_axis_source  source)
{
  if (!seat->pointer)
    return NULL;

  switch (source)
    {
    case WL_POINTER_AXIS_SOURCE_WHEEL:
      if (seat->wheel_scrolling == NULL)
        {
          seat->wheel_scrolling = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                                "name", "Wayland Wheel Scrolling",
                                                "source", GDK_SOURCE_MOUSE,
                                                "has-cursor", TRUE,
                                                "display", seat->display,
                                                "seat", seat,
                                                NULL);
         gdk_seat_device_added (GDK_SEAT (seat), seat->wheel_scrolling);
        }
      return seat->wheel_scrolling;

    case WL_POINTER_AXIS_SOURCE_FINGER:
      if (seat->finger_scrolling == NULL)
        {
          seat->finger_scrolling = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                                 "name", "Wayland Finger Scrolling",
                                                 "source", GDK_SOURCE_TOUCHPAD,
                                                 "has-cursor", TRUE,
                                                 "display", seat->display,
                                                 "seat", seat,
                                                 NULL);
         gdk_seat_device_added (GDK_SEAT (seat), seat->finger_scrolling);
        }
      return seat->finger_scrolling;

    case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
      if (seat->continuous_scrolling == NULL)
        {
          seat->continuous_scrolling = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                                     "name", "Wayland Continuous Scrolling",
                                                     "source", GDK_SOURCE_TRACKPOINT,
                                                     "has-cursor", TRUE,
                                                     "display", seat->display,
                                                     "seat", seat,
                                                     NULL);
         gdk_seat_device_added (GDK_SEAT (seat), seat->continuous_scrolling);
        }
      return seat->continuous_scrolling;

    case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
    default:
      return seat->pointer;
    }
}

static void
seat_handle_name (void           *data,
                  struct wl_seat *seat,
                  const char     *name)
{
  /* We don't care about the name. */
  GDK_SEAT_DEBUG (GDK_WAYLAND_SEAT (data), MISC,
                  "seat %p name %s", seat, name);
}

static const struct wl_seat_listener seat_listener = {
  seat_handle_capabilities,
  seat_handle_name,
};

static void
tablet_tool_handle_type (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool,
                         uint32_t                   tool_type)
{
  GdkWaylandTabletToolData *tool = data;

  switch (tool_type)
    {
    case ZWP_TABLET_TOOL_V2_TYPE_PEN:
      tool->type = GDK_DEVICE_TOOL_TYPE_PEN;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_BRUSH:
      tool->type = GDK_DEVICE_TOOL_TYPE_BRUSH;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH:
      tool->type = GDK_DEVICE_TOOL_TYPE_AIRBRUSH;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_PENCIL:
      tool->type = GDK_DEVICE_TOOL_TYPE_PENCIL;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_ERASER:
      tool->type = GDK_DEVICE_TOOL_TYPE_ERASER;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_MOUSE:
      tool->type = GDK_DEVICE_TOOL_TYPE_MOUSE;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_LENS:
      tool->type = GDK_DEVICE_TOOL_TYPE_LENS;
      break;
    default:
      tool->type = GDK_DEVICE_TOOL_TYPE_UNKNOWN;
      break;
    };
}

static void
tablet_tool_handle_hardware_serial (void                      *data,
                                    struct zwp_tablet_tool_v2 *wp_tablet_tool,
                                    uint32_t                   serial_hi,
                                    uint32_t                   serial_lo)
{
  GdkWaylandTabletToolData *tool = data;

  tool->hardware_serial = ((guint64) serial_hi) << 32 | serial_lo;
}

static void
tablet_tool_handle_hardware_id_wacom (void                      *data,
                                      struct zwp_tablet_tool_v2 *wp_tablet_tool,
                                      uint32_t                   id_hi,
                                      uint32_t                   id_lo)
{
  GdkWaylandTabletToolData *tool = data;

  tool->hardware_id_wacom = ((guint64) id_hi) << 32 | id_lo;
}

static void
tablet_tool_handle_capability (void                      *data,
                               struct zwp_tablet_tool_v2 *wp_tablet_tool,
                               uint32_t                   capability)
{
  GdkWaylandTabletToolData *tool = data;

  switch (capability)
    {
    case ZWP_TABLET_TOOL_V2_CAPABILITY_TILT:
      tool->axes |= GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE:
      tool->axes |= GDK_AXIS_FLAG_PRESSURE;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE:
      tool->axes |= GDK_AXIS_FLAG_DISTANCE;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION:
      tool->axes |= GDK_AXIS_FLAG_ROTATION;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER:
      tool->axes |= GDK_AXIS_FLAG_SLIDER;
      break;
    default:
      break;
    }
}

static void
tablet_tool_handle_done (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;

  tool->tool = gdk_device_tool_new (tool->hardware_serial,
                                    tool->hardware_id_wacom,
                                    tool->type, tool->axes);
  gdk_seat_tool_added (tool->seat, tool->tool);
}

static void
tablet_tool_handle_removed (void                      *data,
                            struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;

  _gdk_wayland_seat_remove_tool (GDK_WAYLAND_SEAT (tool->seat), tool);
}

static void
gdk_wayland_tablet_flush_frame_event (GdkWaylandTabletData *tablet,
                                      guint32               time)
{
  GdkEvent *event;
  GdkEventType type;

  event = tablet->pointer_info.frame.event;
  tablet->pointer_info.frame.event = NULL;

  if (!event)
    return;

  gdk_event_ref (event);

  type = gdk_event_get_event_type (event);

  if (type == GDK_PROXIMITY_OUT)
    emulate_crossing (gdk_event_get_surface (event), NULL,
                      tablet->logical_device, GDK_LEAVE_NOTIFY,
                      GDK_CROSSING_NORMAL, time);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (tablet->seat),
                                      event);

  if (type == GDK_PROXIMITY_IN)
    emulate_crossing (gdk_event_get_surface (event), NULL,
                      tablet->logical_device, GDK_ENTER_NOTIFY,
                      GDK_CROSSING_NORMAL, time);

  gdk_event_unref (event);
}

static void
gdk_wayland_tablet_set_frame_event (GdkWaylandTabletData *tablet,
                                    GdkEvent             *event)
{
  if (tablet->pointer_info.frame.event &&
      gdk_event_get_event_type (tablet->pointer_info.frame.event) != gdk_event_get_event_type (event))
    gdk_wayland_tablet_flush_frame_event (tablet, GDK_CURRENT_TIME);

  tablet->pointer_info.frame.event = event;
}

static void
gdk_wayland_device_tablet_clone_tool_axes (GdkWaylandTabletData *tablet,
                                           GdkDeviceTool        *tool)
{
  int axis_pos;

  g_object_freeze_notify (G_OBJECT (tablet->stylus_device));
  _gdk_device_reset_axes (tablet->stylus_device);

  _gdk_device_add_axis (tablet->stylus_device, GDK_AXIS_X, 0, 0, 0);
  _gdk_device_add_axis (tablet->stylus_device, GDK_AXIS_Y, 0, 0, 0);

  if (tool->tool_axes & (GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT))
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_XTILT, -90, 90, 0);
      tablet->axis_indices[GDK_AXIS_XTILT] = axis_pos;

      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_YTILT, -90, 90, 0);
      tablet->axis_indices[GDK_AXIS_YTILT] = axis_pos;
    }
  if (tool->tool_axes & GDK_AXIS_FLAG_DISTANCE)
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_DISTANCE, 0, 65535, 0);
      tablet->axis_indices[GDK_AXIS_DISTANCE] = axis_pos;
    }
  if (tool->tool_axes & GDK_AXIS_FLAG_PRESSURE)
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_PRESSURE, 0, 65535, 0);
      tablet->axis_indices[GDK_AXIS_PRESSURE] = axis_pos;
    }

  if (tool->tool_axes & GDK_AXIS_FLAG_ROTATION)
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_ROTATION, 0, 360, 0);
      tablet->axis_indices[GDK_AXIS_ROTATION] = axis_pos;
    }

  if (tool->tool_axes & GDK_AXIS_FLAG_SLIDER)
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_SLIDER, -65535, 65535, 0);
      tablet->axis_indices[GDK_AXIS_SLIDER] = axis_pos;
    }

  g_object_thaw_notify (G_OBJECT (tablet->stylus_device));
}

static void
gdk_wayland_mimic_device_axes (GdkDevice *logical,
                               GdkDevice *physical)
{
  double axis_min, axis_max, axis_resolution;
  GdkAxisUse axis_use;
  int axis_count;
  int i;

  g_object_freeze_notify (G_OBJECT (logical));
  _gdk_device_reset_axes (logical);
  axis_count = gdk_device_get_n_axes (physical);

  for (i = 0; i < axis_count; i++)
    {
      _gdk_device_get_axis_info (physical, i, &axis_use, &axis_min,
                                 &axis_max, &axis_resolution);
      _gdk_device_add_axis (logical, axis_use, axis_min,
                            axis_max, axis_resolution);
    }

  g_object_thaw_notify (G_OBJECT (logical));
}

static void
tablet_tool_handle_proximity_in (void                      *data,
                                 struct zwp_tablet_tool_v2 *wp_tablet_tool,
                                 uint32_t                   serial,
                                 struct zwp_tablet_v2      *wp_tablet,
                                 struct wl_surface         *wsurface)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = zwp_tablet_v2_get_user_data (wp_tablet);
  GdkSurface *surface;
  GdkEvent *event;

  if (!wsurface)
    return;

  surface = wl_surface_get_user_data (wsurface);

  if (!surface)
      return;
  if (!GDK_IS_SURFACE (surface))
      return;

  tool->current_tablet = tablet;
  tablet->current_tool = tool;

  tablet->pointer_info.enter_serial = serial;

  tablet->pointer_info.focus = g_object_ref (surface);

  gdk_device_update_tool (tablet->stylus_device, tool->tool);
  gdk_wayland_device_tablet_clone_tool_axes (tablet, tool->tool);
  gdk_wayland_mimic_device_axes (tablet->logical_device, tablet->stylus_device);

  event = gdk_proximity_event_new (GDK_PROXIMITY_IN,
                                   tablet->pointer_info.focus,
                                   tablet->logical_device,
                                   tool->tool,
                                   tablet->pointer_info.time);
  gdk_wayland_tablet_set_frame_event (tablet, event);

  tablet->pointer_info.pointer_surface_outputs =
    g_slist_append (tablet->pointer_info.pointer_surface_outputs,
                    gdk_wayland_surface_get_wl_output (surface));
  pointer_surface_update_scale (tablet->logical_device);

  GDK_SEAT_DEBUG (tablet->seat, EVENTS,
                  "proximity in, seat %p surface %p tool %d",
                  tablet->seat, tablet->pointer_info.focus,
                  gdk_device_tool_get_tool_type (tool->tool));
}

static void
tablet_tool_handle_proximity_out (void                      *data,
                                  struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEvent *event;

  if (!tablet)
    return;

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "proximity out, seat %p, tool %d", tool->seat,
                  gdk_device_tool_get_tool_type (tool->tool));

  event = gdk_proximity_event_new (GDK_PROXIMITY_OUT,
                                   tablet->pointer_info.focus,
                                   tablet->logical_device,
                                   tool->tool,
                                   tablet->pointer_info.time);
  gdk_wayland_tablet_set_frame_event (tablet, event);

  gdk_wayland_seat_stop_cursor_animation (GDK_WAYLAND_SEAT (tool->seat),
                                          &tablet->pointer_info);

  tablet->pointer_info.pointer_surface_outputs =
    g_slist_remove (tablet->pointer_info.pointer_surface_outputs,
                    gdk_wayland_surface_get_wl_output (tablet->pointer_info.focus));
  pointer_surface_update_scale (tablet->logical_device);

  g_object_unref (tablet->pointer_info.focus);
  tablet->pointer_info.focus = NULL;

  tablet->pointer_info.button_modifiers &=
    ~(GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK |
      GDK_BUTTON4_MASK | GDK_BUTTON5_MASK);

  gdk_device_update_tool (tablet->stylus_device, NULL);
  g_clear_object (&tablet->pointer_info.cursor);
  tablet->pointer_info.cursor_is_default = FALSE;
}

static double *
tablet_copy_axes (GdkWaylandTabletData *tablet)
{
  return g_memdup2 (tablet->axes, sizeof (double) * GDK_AXIS_LAST);
}

static void
tablet_create_button_event_frame (GdkWaylandTabletData *tablet,
                                  GdkEventType          evtype,
                                  guint                 button)
{
  GdkEvent *event;

  event = gdk_button_event_new (evtype,
                                tablet->pointer_info.focus,
                                tablet->logical_device,
                                tablet->current_tool->tool,
                                tablet->pointer_info.time,
                                gdk_wayland_device_get_modifiers (tablet->logical_device),
                                button,
                                tablet->pointer_info.surface_x,
                                tablet->pointer_info.surface_y,
                                tablet_copy_axes (tablet));
  gdk_wayland_tablet_set_frame_event (tablet, event);
}

static void
tablet_tool_handle_down (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool,
                         uint32_t                   serial)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;

  if (!tablet || !tablet->pointer_info.focus)
    return;

  tablet->pointer_info.press_serial = serial;

  tablet_create_button_event_frame (tablet, GDK_BUTTON_PRESS, GDK_BUTTON_PRIMARY);
  tablet->pointer_info.button_modifiers |= GDK_BUTTON1_MASK;
}

static void
tablet_tool_handle_up (void                      *data,
                       struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;

  if (!tablet || !tablet->pointer_info.focus)
    return;

  tablet_create_button_event_frame (tablet, GDK_BUTTON_RELEASE, GDK_BUTTON_PRIMARY);
  tablet->pointer_info.button_modifiers &= ~GDK_BUTTON1_MASK;
}

static void
tablet_tool_handle_motion (void                      *data,
                           struct zwp_tablet_tool_v2 *wp_tablet_tool,
                           wl_fixed_t                 sx,
                           wl_fixed_t                 sy)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEvent *event;

  if (!tablet)
    return;

  tablet->pointer_info.surface_x = wl_fixed_to_double (sx);
  tablet->pointer_info.surface_y = wl_fixed_to_double (sy);

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "tablet motion %f %f",
                  tablet->pointer_info.surface_x,
                  tablet->pointer_info.surface_y);

  event = gdk_motion_event_new (tablet->pointer_info.focus,
                                tablet->logical_device,
                                tool->tool,
                                tablet->pointer_info.time,
                                gdk_wayland_device_get_modifiers (tablet->logical_device),
                                tablet->pointer_info.surface_x,
                                tablet->pointer_info.surface_y,
                                tablet_copy_axes (tablet));

  gdk_wayland_tablet_set_frame_event (tablet, event);
}

static void
tablet_tool_handle_pressure (void                      *data,
                             struct zwp_tablet_tool_v2 *wp_tablet_tool,
                             uint32_t                   pressure)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  int axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_PRESSURE];

  _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                              pressure, &tablet->axes[GDK_AXIS_PRESSURE]);

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "tablet tool %d pressure %d",
                  gdk_device_tool_get_tool_type (tool->tool), pressure);
}

static void
tablet_tool_handle_distance (void                      *data,
                             struct zwp_tablet_tool_v2 *wp_tablet_tool,
                             uint32_t                   distance)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  int axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_DISTANCE];

  _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                              distance, &tablet->axes[GDK_AXIS_DISTANCE]);

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "tablet tool %d distance %d",
                  gdk_device_tool_get_tool_type (tool->tool), distance);
}

static void
tablet_tool_handle_tilt (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool,
                         wl_fixed_t                 xtilt,
                         wl_fixed_t                 ytilt)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  int xtilt_axis_index;
  int ytilt_axis_index;

  if (!tablet)
    return;

  xtilt_axis_index = tablet->axis_indices[GDK_AXIS_XTILT];
  ytilt_axis_index = tablet->axis_indices[GDK_AXIS_YTILT];

  _gdk_device_translate_axis (tablet->stylus_device, xtilt_axis_index,
                              wl_fixed_to_double (xtilt),
                              &tablet->axes[GDK_AXIS_XTILT]);
  _gdk_device_translate_axis (tablet->stylus_device, ytilt_axis_index,
                              wl_fixed_to_double (ytilt),
                              &tablet->axes[GDK_AXIS_YTILT]);

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "tablet tool %d tilt %f/%f",
                  gdk_device_tool_get_tool_type (tool->tool),
                  wl_fixed_to_double (xtilt), wl_fixed_to_double (ytilt));
}

static void
tablet_tool_handle_button (void                      *data,
                           struct zwp_tablet_tool_v2 *wp_tablet_tool,
                           uint32_t                   serial,
                           uint32_t                   button,
                           uint32_t                   state)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEventType evtype;
  guint n_button;

  if (!tablet || !tablet->pointer_info.focus)
    return;

  tablet->pointer_info.press_serial = serial;

  if (button == BTN_STYLUS)
    n_button = GDK_BUTTON_MIDDLE;
  else if (button == BTN_STYLUS2)
    n_button = GDK_BUTTON_SECONDARY;
  else if (button == BTN_STYLUS3)
    n_button = 8; /* Back */
  else
    return;

  if (state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED)
    evtype = GDK_BUTTON_PRESS;
  else if (state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_RELEASED)
    evtype = GDK_BUTTON_RELEASE;
  else
    return;

  tablet_create_button_event_frame (tablet, evtype, n_button);
}

static void
tablet_tool_handle_rotation (void                      *data,
                             struct zwp_tablet_tool_v2 *wp_tablet_tool,
                             wl_fixed_t                 degrees)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  int axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_ROTATION];

  _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                              wl_fixed_to_double (degrees),
                              &tablet->axes[GDK_AXIS_ROTATION]);

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "tablet tool %d rotation %f",
                  gdk_device_tool_get_tool_type (tool->tool),
                  wl_fixed_to_double (degrees));
}

static void
tablet_tool_handle_slider (void                      *data,
                           struct zwp_tablet_tool_v2 *wp_tablet_tool,
                           int32_t                    position)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  int axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_SLIDER];

  _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                              position, &tablet->axes[GDK_AXIS_SLIDER]);

  GDK_SEAT_DEBUG (tool->seat, EVENTS,
                  "tablet tool %d slider %d",
                  gdk_device_tool_get_tool_type (tool->tool), position);
}

static void
tablet_tool_handle_wheel (void                      *data,
                          struct zwp_tablet_tool_v2 *wp_tablet_tool,
                          int32_t                    degrees,
                          int32_t                    clicks)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkWaylandSeat *seat;
  GdkEvent *event;

  if (!tablet)
    return;

  seat = GDK_WAYLAND_SEAT (tablet->seat);

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "tablet tool %d wheel %d/%d",
                  gdk_device_tool_get_tool_type (tool->tool), degrees, clicks);

  if (clicks == 0)
    return;

  /* Send smooth event */
  event = gdk_scroll_event_new (tablet->pointer_info.focus,
                                tablet->logical_device,
                                tablet->current_tool->tool,
                                tablet->pointer_info.time,
                                gdk_wayland_device_get_modifiers (tablet->logical_device),
                                0, clicks,
                                FALSE,
                                GDK_SCROLL_UNIT_WHEEL);

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
tablet_tool_handle_frame (void                      *data,
                          struct zwp_tablet_tool_v2 *wl_tablet_tool,
                          uint32_t                   time)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEvent *frame_event;

  if (!tablet)
    return;

  GDK_SEAT_DEBUG (tablet->seat, EVENTS,
                  "tablet frame, time %d", time);

  frame_event = tablet->pointer_info.frame.event;

  if (frame_event && gdk_event_get_event_type (frame_event) == GDK_PROXIMITY_OUT)
    {
      tool->current_tablet = NULL;
      tablet->current_tool = NULL;
    }

  tablet->pointer_info.time = time;
  gdk_wayland_tablet_flush_frame_event (tablet, time);
}

static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
  tablet_tool_handle_type,
  tablet_tool_handle_hardware_serial,
  tablet_tool_handle_hardware_id_wacom,
  tablet_tool_handle_capability,
  tablet_tool_handle_done,
  tablet_tool_handle_removed,
  tablet_tool_handle_proximity_in,
  tablet_tool_handle_proximity_out,
  tablet_tool_handle_down,
  tablet_tool_handle_up,
  tablet_tool_handle_motion,
  tablet_tool_handle_pressure,
  tablet_tool_handle_distance,
  tablet_tool_handle_tilt,
  tablet_tool_handle_rotation,
  tablet_tool_handle_slider,
  tablet_tool_handle_wheel,
  tablet_tool_handle_button,
  tablet_tool_handle_frame,
};

static void
tablet_pad_ring_handle_source (void                          *data,
                               struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring,
                               uint32_t                       source)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad ring handle source, ring = %p source = %d",
                  wp_tablet_pad_ring, source);

  group->axis_tmp_info.source = source;
}

static void
tablet_pad_ring_handle_angle (void                          *data,
                              struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring,
                              wl_fixed_t                     angle)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad ring handle angle, ring = %p angle = %f",
                  wp_tablet_pad_ring, wl_fixed_to_double (angle));

  group->axis_tmp_info.value = wl_fixed_to_double (angle);
}

static void
tablet_pad_ring_handle_stop (void                          *data,
                             struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad ring handle stop, ring = %p", wp_tablet_pad_ring);

  group->axis_tmp_info.is_stop = TRUE;
}

static void
tablet_pad_ring_handle_frame (void                          *data,
                              struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring,
                              uint32_t                       time)
{
  GdkWaylandTabletPadGroupData *group = data;
  GdkWaylandTabletPadData *pad = group->pad;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "tablet pad ring handle frame, ring = %p", wp_tablet_pad_ring);

  event = gdk_pad_event_new_ring (seat->keyboard_focus,
                                  pad->device,
                                  time,
                                  g_list_index (pad->mode_groups, group),
                                  g_list_index (pad->rings, wp_tablet_pad_ring),
                                  group->current_mode,
                                  group->axis_tmp_info.value);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static const struct zwp_tablet_pad_ring_v2_listener tablet_pad_ring_listener = {
  tablet_pad_ring_handle_source,
  tablet_pad_ring_handle_angle,
  tablet_pad_ring_handle_stop,
  tablet_pad_ring_handle_frame,
};

static void
tablet_pad_strip_handle_source (void                           *data,
                                struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip,
                                uint32_t                        source)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad strip handle source, strip = %p source = %d",
                  wp_tablet_pad_strip, source);

  group->axis_tmp_info.source = source;
}

static void
tablet_pad_strip_handle_position (void                           *data,
                                  struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip,
                                  uint32_t                        position)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad strip handle position, strip = %p position = %d",
                  wp_tablet_pad_strip, position);

  group->axis_tmp_info.value = (double) position / 65535;
}

static void
tablet_pad_strip_handle_stop (void                           *data,
                              struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad strip handle stop, strip = %p",
                  wp_tablet_pad_strip);

  group->axis_tmp_info.is_stop = TRUE;
}

static void
tablet_pad_strip_handle_frame (void                           *data,
                               struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip,
                               uint32_t                        time)
{
  GdkWaylandTabletPadGroupData *group = data;
  GdkWaylandTabletPadData *pad = group->pad;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "tablet pad strip handle frame, strip = %p",
                  wp_tablet_pad_strip);

  event = gdk_pad_event_new_strip (seat->keyboard_focus,
                                   pad->device,
                                   time,
                                   g_list_index (pad->mode_groups, group),
                                   g_list_index (pad->strips, wp_tablet_pad_strip),
                                   group->current_mode,
                                   group->axis_tmp_info.value);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static const struct zwp_tablet_pad_strip_v2_listener tablet_pad_strip_listener = {
  tablet_pad_strip_handle_source,
  tablet_pad_strip_handle_position,
  tablet_pad_strip_handle_stop,
  tablet_pad_strip_handle_frame,
};

static void
tablet_pad_group_handle_buttons (void                           *data,
                                 struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                                 struct wl_array                *buttons)
{
  GdkWaylandTabletPadGroupData *group = data;
  uint32_t *p;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad group handle buttons, pad group = %p, n_buttons = %" G_GSIZE_FORMAT,
                  wp_tablet_pad_group, buttons->size);

  wl_array_for_each (p, buttons)
    {
      group->buttons = g_list_prepend (group->buttons, GUINT_TO_POINTER (*p));
    }

  group->buttons = g_list_reverse (group->buttons);
}

static void
tablet_pad_group_handle_ring (void                           *data,
                              struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                              struct zwp_tablet_pad_ring_v2  *wp_tablet_pad_ring)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad group handle ring, pad group = %p, ring = %p",
                  wp_tablet_pad_group, wp_tablet_pad_ring);

  zwp_tablet_pad_ring_v2_add_listener (wp_tablet_pad_ring,
                                       &tablet_pad_ring_listener, group);
  zwp_tablet_pad_ring_v2_set_user_data (wp_tablet_pad_ring, group);

  group->rings = g_list_append (group->rings, wp_tablet_pad_ring);
  group->pad->rings = g_list_append (group->pad->rings, wp_tablet_pad_ring);
}

static void
tablet_pad_group_handle_strip (void                           *data,
                               struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                               struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad group handle strip, pad group = %p, strip = %p",
                  wp_tablet_pad_group, wp_tablet_pad_strip);

  zwp_tablet_pad_strip_v2_add_listener (wp_tablet_pad_strip,
                                       &tablet_pad_strip_listener, group);
  zwp_tablet_pad_strip_v2_set_user_data (wp_tablet_pad_strip, group);

  group->strips = g_list_append (group->strips, wp_tablet_pad_strip);
  group->pad->strips = g_list_append (group->pad->strips, wp_tablet_pad_strip);
}

static void
tablet_pad_group_handle_modes (void                           *data,
                               struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                               uint32_t                        modes)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad group handle modes, pad group = %p, n_modes = %d",
                  wp_tablet_pad_group, modes);

  group->n_modes = modes;
}

static void
tablet_pad_group_handle_done (void                           *data,
                              struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_SEAT_DEBUG (group->pad->seat, EVENTS,
                  "tablet pad group handle done, pad group = %p",
                  wp_tablet_pad_group);
}

static void
tablet_pad_group_handle_mode (void                           *data,
                              struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                              uint32_t                        time,
                              uint32_t                        serial,
                              uint32_t                        mode)
{
  GdkWaylandTabletPadGroupData *group = data;
  GdkWaylandTabletPadData *pad = group->pad;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;
  guint n_group;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "tablet pad group handle mode, pad group = %p, mode = %d",
                  wp_tablet_pad_group, mode);

  group->mode_switch_serial = serial;
  group->current_mode = mode;
  n_group = g_list_index (pad->mode_groups, group);

  event = gdk_pad_event_new_group_mode (seat->keyboard_focus,
                                        pad->device,
                                        time,
                                        n_group,
                                        mode);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static const struct zwp_tablet_pad_group_v2_listener tablet_pad_group_listener = {
  tablet_pad_group_handle_buttons,
  tablet_pad_group_handle_ring,
  tablet_pad_group_handle_strip,
  tablet_pad_group_handle_modes,
  tablet_pad_group_handle_done,
  tablet_pad_group_handle_mode,
};

static void
tablet_pad_handle_group (void                           *data,
                         struct zwp_tablet_pad_v2       *wp_tablet_pad,
                         struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group)
{
  GdkWaylandTabletPadData *pad = data;
  GdkWaylandTabletPadGroupData *group;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle group, pad group = %p, group = %p",
                  wp_tablet_pad_group, wp_tablet_pad_group);

  group = g_new0 (GdkWaylandTabletPadGroupData, 1);
  group->wp_tablet_pad_group = wp_tablet_pad_group;
  group->pad = pad;

  zwp_tablet_pad_group_v2_add_listener (wp_tablet_pad_group,
                                        &tablet_pad_group_listener, group);
  zwp_tablet_pad_group_v2_set_user_data (wp_tablet_pad_group, group);
  pad->mode_groups = g_list_append (pad->mode_groups, group);
}

static void
tablet_pad_handle_path (void                     *data,
                        struct zwp_tablet_pad_v2 *wp_tablet_pad,
                        const char               *path)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle path, pad = %p, path = %s",
                  wp_tablet_pad, path);

  pad->path = g_strdup (path);
}

static void
tablet_pad_handle_buttons (void                     *data,
                           struct zwp_tablet_pad_v2 *wp_tablet_pad,
                           uint32_t                  buttons)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle buttons, pad = %p, n_buttons = %d",
                  wp_tablet_pad, buttons);

  pad->n_buttons = buttons;
}

static void
tablet_pad_handle_done (void                     *data,
                        struct zwp_tablet_pad_v2 *wp_tablet_pad)
{
  G_GNUC_UNUSED GdkWaylandTabletPadData *pad = data;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle done, pad = %p", wp_tablet_pad);
}

static void
tablet_pad_handle_button (void                     *data,
                          struct zwp_tablet_pad_v2 *wp_tablet_pad,
                          uint32_t                  time,
                          uint32_t                  button,
                          uint32_t                  state)
{
  GdkWaylandTabletPadData *pad = data;
  GdkWaylandTabletPadGroupData *group;
  GdkEvent *event;
  int n_group;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle button, pad = %p, button = %d, state = %d",
                  wp_tablet_pad, button, state);

  group = tablet_pad_lookup_button_group (pad, button);

#ifdef G_DISABLE_ASSERT
  if (group == NULL)
    return;
#else
  g_assert (group != NULL);
#endif

  n_group = g_list_index (pad->mode_groups, group);

  event = gdk_pad_event_new_button (state == ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED
                                       ? GDK_PAD_BUTTON_PRESS
                                       : GDK_PAD_BUTTON_RELEASE,
                                    GDK_WAYLAND_SEAT (pad->seat)->keyboard_focus,
                                    pad->device,
                                    time,
                                    n_group,
                                    button,
                                    group->current_mode);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat), event);
}

static void
tablet_pad_handle_enter (void                     *data,
                         struct zwp_tablet_pad_v2 *wp_tablet_pad,
                         uint32_t                  serial,
                         struct zwp_tablet_v2     *wp_tablet,
                         struct wl_surface        *surface)
{
  GdkWaylandTabletPadData *pad = data;
  GdkWaylandTabletData *tablet = zwp_tablet_v2_get_user_data (wp_tablet);

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle enter, pad = %p, tablet = %p surface = %p",
                  wp_tablet_pad, wp_tablet, surface);

  if (pad->device && pad->current_tablet != tablet)
    {
      gdk_seat_device_removed (GDK_SEAT (pad->seat), pad->device);
      _gdk_device_set_associated_device (pad->device, NULL);
      g_clear_object (&pad->device);
    }

  /* Relate pad and tablet */
  tablet->pads = g_list_prepend (tablet->pads, pad);
  pad->current_tablet = tablet;

  if (!pad->device)
    {
      gchar *name, *vid, *pid;

      name = g_strdup_printf ("%s Pad %d",
                              tablet->name,
                              g_list_index (tablet->pads, pad) + 1);
      vid = g_strdup_printf ("%.4x", tablet->vid);
      pid = g_strdup_printf ("%.4x", tablet->pid);

      pad->device =
        g_object_new (GDK_TYPE_WAYLAND_DEVICE_PAD,
                      "name", name,
                      "vendor-id", vid,
                      "product-id", pid,
                      "source", GDK_SOURCE_TABLET_PAD,
                      "display", gdk_seat_get_display (pad->seat),
                      "seat", pad->seat,
                      NULL);

      _gdk_device_set_associated_device (pad->device, GDK_WAYLAND_SEAT (pad->seat)->logical_keyboard);
      gdk_seat_device_added (GDK_SEAT (pad->seat), pad->device);

      g_free (name);
      g_free (vid);
      g_free (pid);
    }
}

static void
tablet_pad_handle_leave (void                     *data,
                         struct zwp_tablet_pad_v2 *wp_tablet_pad,
                         uint32_t                  serial,
                         struct wl_surface        *surface)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle leave, pad = %p, surface = %p",
                  wp_tablet_pad, surface);

  if (pad->current_tablet)
    {
      pad->current_tablet->pads = g_list_remove (pad->current_tablet->pads, pad);
      pad->current_tablet = NULL;
    }
}

static void
tablet_pad_handle_removed (void                     *data,
                           struct zwp_tablet_pad_v2 *wp_tablet_pad)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_SEAT_DEBUG (pad->seat, EVENTS,
                  "tablet pad handle removed, pad = %p", wp_tablet_pad);

  /* Remove from the current tablet */
  if (pad->current_tablet)
    {
      pad->current_tablet->pads = g_list_remove (pad->current_tablet->pads, pad);
      pad->current_tablet = NULL;
    }

  _gdk_wayland_seat_remove_tablet_pad (GDK_WAYLAND_SEAT (pad->seat), pad);
}

static const struct zwp_tablet_pad_v2_listener tablet_pad_listener = {
  tablet_pad_handle_group,
  tablet_pad_handle_path,
  tablet_pad_handle_buttons,
  tablet_pad_handle_done,
  tablet_pad_handle_button,
  tablet_pad_handle_enter,
  tablet_pad_handle_leave,
  tablet_pad_handle_removed,
};

static void
tablet_seat_handle_tablet_added (void                      *data,
                                 struct zwp_tablet_seat_v2 *wp_tablet_seat,
                                 struct zwp_tablet_v2      *wp_tablet)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTabletData *tablet;

  tablet = g_new0 (GdkWaylandTabletData, 1);
  tablet->seat = GDK_SEAT (seat);

  tablet->wp_tablet = wp_tablet;

  seat->tablets = g_list_prepend (seat->tablets, tablet);

  zwp_tablet_v2_add_listener (wp_tablet, &tablet_listener, tablet);
  zwp_tablet_v2_set_user_data (wp_tablet, tablet);
}

static void
tablet_seat_handle_tool_added (void                      *data,
                               struct zwp_tablet_seat_v2 *wp_tablet_seat,
                               struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTabletToolData *tool;

  tool = g_new0 (GdkWaylandTabletToolData, 1);
  tool->wp_tablet_tool = wp_tablet_tool;
  tool->seat = GDK_SEAT (seat);

  zwp_tablet_tool_v2_add_listener (wp_tablet_tool, &tablet_tool_listener, tool);
  zwp_tablet_tool_v2_set_user_data (wp_tablet_tool, tool);

  seat->tablet_tools = g_list_prepend (seat->tablet_tools, tool);
}

static void
tablet_seat_handle_pad_added (void                      *data,
                              struct zwp_tablet_seat_v2 *wp_tablet_seat,
                              struct zwp_tablet_pad_v2  *wp_tablet_pad)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTabletPadData *pad;

  pad = g_new0 (GdkWaylandTabletPadData, 1);
  pad->wp_tablet_pad = wp_tablet_pad;
  pad->seat = GDK_SEAT (seat);

  zwp_tablet_pad_v2_add_listener (wp_tablet_pad, &tablet_pad_listener, pad);
  zwp_tablet_pad_v2_set_user_data (wp_tablet_pad, pad);

  seat->tablet_pads = g_list_prepend (seat->tablet_pads, pad);
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
  tablet_seat_handle_tablet_added,
  tablet_seat_handle_tool_added,
  tablet_seat_handle_pad_added,
};

static void
init_devices (GdkWaylandSeat *seat)
{
  /* pointer */
  seat->logical_pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                       "name", "Core Pointer",
                                       "source", GDK_SOURCE_MOUSE,
                                       "has-cursor", TRUE,
                                       "display", seat->display,
                                       "seat", seat,
                                       NULL);

  gdk_wayland_device_set_pointer (GDK_WAYLAND_DEVICE (seat->logical_pointer),
                                  &seat->pointer_info);

  /* keyboard */
  seat->logical_keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                        "name", "Core Keyboard",
                                        "source", GDK_SOURCE_KEYBOARD,
                                        "has-cursor", FALSE,
                                        "display", seat->display,
                                        "seat", seat,
                                        NULL);
  _gdk_device_reset_axes (seat->logical_keyboard);

  /* link both */
  _gdk_device_set_associated_device (seat->logical_pointer, seat->logical_keyboard);
  _gdk_device_set_associated_device (seat->logical_keyboard, seat->logical_pointer);

  gdk_seat_device_added (GDK_SEAT (seat), seat->logical_pointer);
  gdk_seat_device_added (GDK_SEAT (seat), seat->logical_keyboard);
}

static void
pointer_surface_update_scale (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);
  guint32 scale;
  GSList *l;

  if (wl_surface_get_version (pointer->pointer_surface) < WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    {
      /* We can't set the scale on this surface */
      return;
    }

  if (!pointer->pointer_surface_outputs)
    return;

  scale = 1;
  for (l = pointer->pointer_surface_outputs; l != NULL; l = l->next)
    {
      guint32 output_scale = gdk_wayland_display_get_output_scale (display_wayland, l->data);
      scale = MAX (scale, output_scale);
    }

  if (pointer->current_output_scale == scale)
    return;
  pointer->current_output_scale = scale;

  gdk_wayland_device_update_surface_cursor (device);
}

void
gdk_wayland_seat_update_cursor_scale (GdkWaylandSeat *seat)
{
  GList *l;

  pointer_surface_update_scale (seat->logical_pointer);

  for (l = seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;
      pointer_surface_update_scale (tablet->logical_device);
    }
}

static void
pointer_surface_enter (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)

{
  GdkDevice *device = data;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandTabletData *tablet;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "pointer surface of seat %p entered output %p",
                  seat, output);

  tablet = gdk_wayland_seat_find_tablet (seat, device);

  if (tablet)
    {
      tablet->pointer_info.pointer_surface_outputs =
        g_slist_append (tablet->pointer_info.pointer_surface_outputs, output);
    }
  else
    {
      seat->pointer_info.pointer_surface_outputs =
        g_slist_append (seat->pointer_info.pointer_surface_outputs, output);
    }

  pointer_surface_update_scale (device);
}

static void
pointer_surface_leave (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)
{
  GdkDevice *device = data;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandTabletData *tablet;

  GDK_SEAT_DEBUG (seat, EVENTS,
                  "pointer surface of seat %p left output %p",
                  seat, output);

  tablet = gdk_wayland_seat_find_tablet (seat, device);

  if (tablet)
    {
      tablet->pointer_info.pointer_surface_outputs =
        g_slist_remove (tablet->pointer_info.pointer_surface_outputs, output);
    }
  else
    {
      seat->pointer_info.pointer_surface_outputs =
        g_slist_remove (seat->pointer_info.pointer_surface_outputs, output);
    }

  pointer_surface_update_scale (device);
}

static void
pointer_surface_preferred_buffer_scale (void              *data,
                                        struct wl_surface *wl_surface,
                                        int32_t            factor)
{
}

static void
pointer_surface_preferred_buffer_transform (void              *data,
                                            struct wl_surface *wl_surface,
                                            uint32_t           transform)
{
}

static const struct wl_surface_listener pointer_surface_listener = {
  pointer_surface_enter,
  pointer_surface_leave,
  pointer_surface_preferred_buffer_scale,
  pointer_surface_preferred_buffer_transform,
};

static void
gdk_wayland_pointer_data_finalize (GdkWaylandPointerData *pointer)
{
  g_clear_object (&pointer->focus);
  g_clear_object (&pointer->cursor);
  wl_surface_destroy (pointer->pointer_surface);
  g_slist_free (pointer->pointer_surface_outputs);
}

static void
gdk_wayland_seat_dispose (GObject *object)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (object);

  g_clear_pointer (&seat->wl_seat, wl_seat_destroy);
  g_clear_pointer (&seat->wl_pointer, wl_pointer_destroy);
  g_clear_pointer (&seat->wl_keyboard, wl_keyboard_destroy);
  g_clear_pointer (&seat->wl_touch, wl_touch_destroy);
  g_clear_pointer (&seat->wp_pointer_gesture_swipe, zwp_pointer_gesture_swipe_v1_destroy);
  g_clear_pointer (&seat->wp_pointer_gesture_pinch, zwp_pointer_gesture_pinch_v1_destroy);
  g_clear_pointer (&seat->wp_pointer_gesture_hold, zwp_pointer_gesture_hold_v1_destroy);
  g_clear_pointer (&seat->wp_tablet_seat, zwp_tablet_seat_v2_destroy);

  G_OBJECT_CLASS (gdk_wayland_seat_parent_class)->dispose (object);
}

static void
gdk_wayland_seat_finalize (GObject *object)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (object);
  GList *l;

  for (l = seat->tablet_tools; l != NULL; l = l->next)
    _gdk_wayland_seat_remove_tool (seat, l->data);

  for (l = seat->tablet_pads; l != NULL; l = l->next)
    _gdk_wayland_seat_remove_tablet_pad (seat, l->data);

  for (l = seat->tablets; l != NULL; l = l->next)
    _gdk_wayland_seat_remove_tablet (seat, l->data);

  seat_handle_capabilities (seat, seat->wl_seat, 0);
  g_object_unref (seat->keymap);
  gdk_wayland_pointer_data_finalize (&seat->pointer_info);
  /* FIXME: destroy data_device */
  g_clear_object (&seat->drag);
  g_clear_object (&seat->drop);
  g_clear_object (&seat->clipboard);
  g_clear_object (&seat->primary_clipboard);
  g_hash_table_destroy (seat->touches);
  zwp_tablet_seat_v2_destroy (seat->wp_tablet_seat);
  stop_key_repeat (seat);

  G_OBJECT_CLASS (gdk_wayland_seat_parent_class)->finalize (object);
}

static GdkSeatCapabilities
gdk_wayland_seat_get_capabilities (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkSeatCapabilities caps = 0;

  if (wayland_seat->logical_pointer)
    caps |= GDK_SEAT_CAPABILITY_POINTER;
  if (wayland_seat->logical_keyboard)
    caps |= GDK_SEAT_CAPABILITY_KEYBOARD;
  if (wayland_seat->logical_touch)
    caps |= GDK_SEAT_CAPABILITY_TOUCH;

  return caps;
}

static void
gdk_wayland_seat_set_grab_surface (GdkWaylandSeat *seat,
                                  GdkSurface      *surface)
{
  if (seat->grab_surface)
    {
      _gdk_wayland_surface_set_grab_seat (seat->grab_surface, NULL);
      g_object_remove_weak_pointer (G_OBJECT (seat->grab_surface),
                                    (gpointer *) &seat->grab_surface);
      seat->grab_surface = NULL;
    }

  if (surface)
    {
      seat->grab_surface = surface;
      g_object_add_weak_pointer (G_OBJECT (surface),
                                 (gpointer *) &seat->grab_surface);
      _gdk_wayland_surface_set_grab_seat (surface, GDK_SEAT (seat));
    }
}

static GdkGrabStatus
gdk_wayland_seat_grab (GdkSeat                *seat,
                       GdkSurface             *surface,
                       GdkSeatCapabilities     capabilities,
                       gboolean                owner_events,
                       GdkCursor              *cursor,
                       GdkEvent               *event,
                       GdkSeatGrabPrepareFunc  prepare_func,
                       gpointer                prepare_func_data)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
  GdkDisplay *display = gdk_seat_get_display (seat);
  GList *l;

  if (surface == NULL || GDK_SURFACE_DESTROYED (surface))
    return GDK_GRAB_NOT_VIEWABLE;

  gdk_wayland_seat_set_grab_surface (wayland_seat, surface);
  wayland_seat->grab_time = evtime;

  if (prepare_func)
    (prepare_func) (seat, surface, prepare_func_data);

  if (!gdk_wayland_surface_has_surface (surface))
    {
      gdk_wayland_seat_set_grab_surface (wayland_seat, NULL);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  if (wayland_seat->logical_pointer &&
      capabilities & GDK_SEAT_CAPABILITY_POINTER)
    {
      gdk_wayland_device_maybe_emit_grab_crossing (wayland_seat->logical_pointer,
                                                   surface, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->logical_pointer,
                                    surface,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);

      gdk_wayland_seat_set_global_cursor (seat, cursor);
      g_set_object (&wayland_seat->cursor, cursor);
      gdk_wayland_device_update_surface_cursor (wayland_seat->logical_pointer);
    }

  if (wayland_seat->logical_touch &&
      capabilities & GDK_SEAT_CAPABILITY_TOUCH)
    {
      gdk_wayland_device_maybe_emit_grab_crossing (wayland_seat->logical_touch,
                                                   surface, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->logical_touch,
                                    surface,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);
    }

  if (wayland_seat->logical_keyboard &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      gdk_wayland_device_maybe_emit_grab_crossing (wayland_seat->logical_keyboard,
                                                   surface, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->logical_keyboard,
                                    surface,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);

      /* Inhibit shortcuts if the seat grab is for the keyboard only */
      if (capabilities == GDK_SEAT_CAPABILITY_KEYBOARD)
        gdk_wayland_surface_inhibit_shortcuts (surface, seat);
    }

  if (wayland_seat->tablets &&
      capabilities & GDK_SEAT_CAPABILITY_TABLET_STYLUS)
    {
      for (l = wayland_seat->tablets; l; l = l->next)
        {
          GdkWaylandTabletData *tablet = l->data;

          if (tablet->current_tool)
            {
              gdk_wayland_device_maybe_emit_grab_crossing (tablet->logical_device,
                                                           surface,
                                                           evtime);
            }

          _gdk_display_add_device_grab (display,
                                        tablet->logical_device,
                                        surface,
                                        owner_events,
                                        GDK_ALL_EVENTS_MASK,
                                        _gdk_display_get_next_serial (display),
                                        evtime,
                                        FALSE);

          gdk_wayland_device_update_surface_cursor (tablet->logical_device);
        }
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_seat_ungrab (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkDisplay *display = gdk_seat_get_display (seat);
  GdkDeviceGrabInfo *grab;
  GList *l;

  g_clear_object (&wayland_seat->grab_cursor);

  gdk_wayland_seat_set_grab_surface (wayland_seat, NULL);

  if (wayland_seat->logical_pointer)
    {
      gdk_wayland_device_maybe_emit_ungrab_crossing (wayland_seat->logical_pointer,
                                                     GDK_CURRENT_TIME);

      gdk_wayland_device_update_surface_cursor (wayland_seat->logical_pointer);
    }

  if (wayland_seat->logical_keyboard)
    {
      GdkSurface *prev_focus;

      prev_focus = gdk_wayland_device_maybe_emit_ungrab_crossing (wayland_seat->logical_keyboard,
                                                                  GDK_CURRENT_TIME);
      if (prev_focus)
        gdk_wayland_surface_restore_shortcuts (prev_focus, seat);
    }

  if (wayland_seat->logical_touch)
    {
      grab = _gdk_display_get_last_device_grab (display, wayland_seat->logical_touch);

      if (grab)
        grab->serial_end = grab->serial_start;
    }

  for (l = wayland_seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      grab = _gdk_display_get_last_device_grab (display, tablet->logical_device);

      if (grab)
        grab->serial_end = grab->serial_start;
    }
}

static GdkDevice *
gdk_wayland_seat_get_logical_device (GdkSeat             *seat,
                                     GdkSeatCapabilities  capabilities)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);

  if (capabilities == GDK_SEAT_CAPABILITY_POINTER)
    return wayland_seat->logical_pointer;
  else if (capabilities == GDK_SEAT_CAPABILITY_KEYBOARD)
    return wayland_seat->logical_keyboard;
  else if (capabilities == GDK_SEAT_CAPABILITY_TOUCH)
    return wayland_seat->logical_touch;

  return NULL;
}

static GList *
gdk_wayland_seat_get_devices (GdkSeat             *seat,
                              GdkSeatCapabilities  capabilities)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GList *physical_devices = NULL;

  if (wayland_seat->finger_scrolling && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    physical_devices = g_list_prepend (physical_devices, wayland_seat->finger_scrolling);
  if (wayland_seat->continuous_scrolling && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    physical_devices = g_list_prepend (physical_devices, wayland_seat->continuous_scrolling);
  if (wayland_seat->wheel_scrolling && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    physical_devices = g_list_prepend (physical_devices, wayland_seat->wheel_scrolling);
  if (wayland_seat->pointer && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    physical_devices = g_list_prepend (physical_devices, wayland_seat->pointer);
  if (wayland_seat->keyboard && (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD))
    physical_devices = g_list_prepend (physical_devices, wayland_seat->keyboard);
  if (wayland_seat->touch && (capabilities & GDK_SEAT_CAPABILITY_TOUCH))
    physical_devices = g_list_prepend (physical_devices, wayland_seat->touch);

  if (wayland_seat->tablets && (capabilities & GDK_SEAT_CAPABILITY_TABLET_STYLUS))
    {
      GList *l;

      for (l = wayland_seat->tablets; l; l = l->next)
        {
          GdkWaylandTabletData *tablet = l->data;

          physical_devices = g_list_prepend (physical_devices, tablet->stylus_device);
        }
    }

  if (wayland_seat->tablet_pads && (capabilities & GDK_SEAT_CAPABILITY_TABLET_PAD))
    {
      GList *l;

      for (l = wayland_seat->tablet_pads; l; l = l->next)
        {
          GdkWaylandTabletPadData *data = l->data;
          physical_devices = g_list_prepend (physical_devices, data->device);
        }
    }

  return physical_devices;
}

static GList *
gdk_wayland_seat_get_tools (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GList *tools = NULL, *l;

  for (l = wayland_seat->tablet_tools; l; l = l->next)
    {
      GdkWaylandTabletToolData *tool = l->data;

      tools = g_list_prepend (tools, tool->tool);
    }

  return tools;
}

static void
gdk_wayland_seat_class_init (GdkWaylandSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->finalize = gdk_wayland_seat_finalize;
  object_class->dispose = gdk_wayland_seat_dispose;

  seat_class->get_capabilities = gdk_wayland_seat_get_capabilities;
  seat_class->grab = gdk_wayland_seat_grab;
  seat_class->ungrab = gdk_wayland_seat_ungrab;
  seat_class->get_logical_device = gdk_wayland_seat_get_logical_device;
  seat_class->get_devices = gdk_wayland_seat_get_devices;
  seat_class->get_tools = gdk_wayland_seat_get_tools;
}

static void
gdk_wayland_seat_init (GdkWaylandSeat *seat)
{
}

static void
init_pointer_data (GdkWaylandPointerData *pointer_data,
                   GdkDisplay            *display,
                   GdkDevice             *logical_device)
{
  GdkWaylandDisplay *display_wayland;

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  pointer_data->current_output_scale = 1;
  pointer_data->pointer_surface =
    wl_compositor_create_surface (display_wayland->compositor);
  wl_surface_add_listener (pointer_data->pointer_surface,
                           &pointer_surface_listener,
                           logical_device);
}

void
_gdk_wayland_display_create_seat (GdkWaylandDisplay *display_wayland,
                                  guint32            id,
				  struct wl_seat    *wl_seat)
{
  GdkDisplay *display = GDK_DISPLAY (display_wayland);
  GdkWaylandSeat *seat;

  seat = g_object_new (GDK_TYPE_WAYLAND_SEAT,
                       "display", display_wayland,
                       NULL);
  seat->id = id;
  seat->keymap = _gdk_wayland_keymap_new (display);
  seat->display = display;
  seat->touches = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_free);
  seat->wl_seat = wl_seat;

  wl_seat_add_listener (seat->wl_seat, &seat_listener, seat);
  wl_seat_set_user_data (seat->wl_seat, seat);

  if (display_wayland->primary_selection_manager)
    {
      seat->primary_clipboard = gdk_wayland_primary_new (seat);
    }
  else
    {
      /* If the compositor doesn't support primary clipboard,
       * just do it local-only */
      seat->primary_clipboard = gdk_clipboard_new (display);
    }

  seat->data_device =
    wl_data_device_manager_get_data_device (display_wayland->data_device_manager,
                                            seat->wl_seat);
  seat->clipboard = gdk_wayland_clipboard_new (display);
  wl_data_device_add_listener (seat->data_device,
                               &data_device_listener, seat);

  init_devices (seat);
  init_pointer_data (&seat->pointer_info, display, seat->logical_pointer);

  if (display_wayland->tablet_manager)
    {
      seat->wp_tablet_seat =
        zwp_tablet_manager_v2_get_tablet_seat (display_wayland->tablet_manager,
                                               wl_seat);
      zwp_tablet_seat_v2_add_listener (seat->wp_tablet_seat, &tablet_seat_listener,
                                       seat);
    }

  if (display->clipboard == NULL)
    display->clipboard = g_object_ref (seat->clipboard);
  if (display->primary_clipboard == NULL)
    display->primary_clipboard = g_object_ref (seat->primary_clipboard);

  gdk_display_add_seat (display, GDK_SEAT (seat));
}

void
_gdk_wayland_display_remove_seat (GdkWaylandDisplay *display_wayland,
                                  guint32            id)
{
  GdkDisplay *display = GDK_DISPLAY (display_wayland);
  GList *l, *seats;

  seats = gdk_display_list_seats (display);

  for (l = seats; l != NULL; l = l->next)
    {
      GdkWaylandSeat *seat = l->data;

      if (seat->id != id)
        continue;

      gdk_display_remove_seat (display, GDK_SEAT (seat));
      break;
    }

  g_list_free (seats);
}

uint32_t
_gdk_wayland_seat_get_implicit_grab_serial (GdkSeat          *seat,
                                            GdkDevice        *device,
                                            GdkEventSequence *sequence)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GList *l;

  if (sequence)
    {
      GdkWaylandTouchData *touch = NULL;

      touch = gdk_wayland_seat_get_touch (GDK_WAYLAND_SEAT (seat),
                                          GDK_EVENT_SEQUENCE_TO_SLOT (sequence));
      if (touch)
        return touch->touch_down_serial;
    }
  else if (device == wayland_seat->logical_touch)
    {
      GdkWaylandTouchData *touch;
      GHashTableIter iter;

      /* Pick the first sequence */
      g_hash_table_iter_init (&iter, wayland_seat->touches);
      g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch);
      return touch->touch_down_serial;
    }

  for (l = wayland_seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      if (tablet->logical_device == device)
        return tablet->pointer_info.press_serial;
    }

  return GDK_WAYLAND_SEAT (seat)->pointer_info.press_serial;
}

uint32_t
_gdk_wayland_seat_get_last_implicit_grab_serial (GdkWaylandSeat    *seat,
                                                 GdkEventSequence **sequence)
{
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GList *l;
  uint32_t serial;

  g_hash_table_iter_init (&iter, seat->touches);

  if (sequence)
    *sequence = NULL;

  serial = seat->keyboard_key_serial;

  if (seat->pointer_info.press_serial > serial)
    serial = seat->pointer_info.press_serial;

  for (l = seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      if (tablet->pointer_info.press_serial > serial)
        serial = tablet->pointer_info.press_serial;
    }

  if (g_hash_table_size (seat->touches) > 0)
    {
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
        {
          if (touch->touch_down_serial > serial)
            {
              if (sequence)
                *sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);
              serial = touch->touch_down_serial;

            }
        }
    }
  else
    {
      if (seat->latest_touch_down_serial > serial)
        serial = seat->latest_touch_down_serial;
    }

  return serial;
}

void
gdk_wayland_seat_set_global_cursor (GdkSeat   *seat,
                                    GdkCursor *cursor)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkDevice *pointer;

  pointer = gdk_seat_get_pointer (seat);

  g_set_object (&wayland_seat->grab_cursor, cursor);
  GDK_DEVICE_GET_CLASS (pointer)->set_surface_cursor (pointer,
                                                      gdk_wayland_device_get_focus (pointer),
                                                      NULL);
}

void
gdk_wayland_seat_set_drag (GdkSeat *seat,
                           GdkDrag *drag)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);

  g_set_object (&wayland_seat->drag, drag);
}

/**
 * gdk_wayland_seat_get_wl_seat: (skip)
 * @seat: (type GdkWaylandSeat): a `GdkSeat`
 *
 * Returns the Wayland `wl_seat` of a `GdkSeat`.
 *
 * Returns: (transfer none): a Wayland `wl_seat`
 */
struct wl_seat *
gdk_wayland_seat_get_wl_seat (GdkSeat *seat)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SEAT (seat), NULL);

  return GDK_WAYLAND_SEAT (seat)->wl_seat;
}
