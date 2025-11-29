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
#include "gdkprivate-wayland.h"
#include "gdkcursorprivate.h"
#include "gdkeventsprivate.h"

#define GDK_SLOT_TO_EVENT_SEQUENCE(s) ((GdkEventSequence *) GUINT_TO_POINTER((s) + 1))
#define GDK_EVENT_SEQUENCE_TO_SLOT(s) (GPOINTER_TO_UINT(s) - 1)

typedef struct
{
  GdkWaylandTouchData *emulating_touch; /* Only used on wd->logical_touch */
  GdkWaylandPointerData *pointer;
} GdkWaylandDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdkWaylandDevice, gdk_wayland_device, GDK_TYPE_DEVICE)

static void
gdk_wayland_device_set_surface_cursor (GdkDevice  *device,
                                       GdkSurface *surface,
                                       GdkCursor  *cursor)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);

  if (device == seat->logical_touch)
    return;

  if (seat->grab_cursor)
    cursor = seat->grab_cursor;

  if (pointer->cursor != NULL &&
      cursor != NULL &&
      gdk_cursor_equal (cursor, pointer->cursor))
    return;

  if (cursor == NULL)
    {
      if (!pointer->cursor_is_default)
        {
          g_clear_object (&pointer->cursor);
          pointer->cursor = gdk_cursor_new_from_name ("default", NULL);
          pointer->cursor_is_default = TRUE;

          gdk_wayland_seat_stop_cursor_animation (seat, pointer);
          gdk_wayland_device_update_surface_cursor (device);
        }
      else
        {
          /* Nothing to do, we'already using the default cursor */
        }
    }
  else
    {
      g_set_object (&pointer->cursor, cursor);
      pointer->cursor_is_default = FALSE;

      gdk_wayland_seat_stop_cursor_animation (seat, pointer);
      gdk_wayland_device_update_surface_cursor (device);
    }
}

static GdkGrabStatus
gdk_wayland_device_grab (GdkDevice    *device,
                         GdkSurface   *surface,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkSurface   *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);

  if (GDK_IS_DRAG_SURFACE (surface) &&
      gdk_surface_get_mapped (surface))
    {
      g_warning ("Surface %p is already mapped at the time of grabbing. "
                 "gdk_seat_grab() should be used to simultaneously grab input "
                 "and show this popup. You may find oddities ahead.",
                 surface);
    }

  gdk_wayland_device_maybe_emit_grab_crossing (device, surface, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      gdk_wayland_surface_inhibit_shortcuts (surface,
                                             gdk_device_get_seat (device));
      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */
      if (pointer->grab_surface != NULL &&
          time_ != 0 && pointer->grab_time > time_)
        {
          return GDK_GRAB_ALREADY_GRABBED;
        }

      if (time_ == 0)
        time_ = pointer->time;

      pointer->grab_surface = surface;
      pointer->grab_time = time_;
      _gdk_wayland_surface_set_grab_seat (surface, GDK_SEAT (wayland_seat));

      g_clear_object (&wayland_seat->cursor);

      if (cursor)
        wayland_seat->cursor = g_object_ref (cursor);

      gdk_wayland_device_update_surface_cursor (device);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_device_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);
  GdkSurface *prev_focus;

  prev_focus = gdk_wayland_device_maybe_emit_ungrab_crossing (device, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      if (prev_focus)
        gdk_wayland_surface_restore_shortcuts (prev_focus,
                                              gdk_device_get_seat (device));
    }
  else
    {
      /* Device is a pointer */
      gdk_wayland_device_update_surface_cursor (device);

      if (pointer->grab_surface)
        _gdk_wayland_surface_set_grab_seat (pointer->grab_surface,
                                           NULL);
    }
}

static GdkSurface *
gdk_wayland_device_surface_at_position (GdkDevice       *device,
                                        double          *win_x,
                                        double          *win_y,
                                        GdkModifierType *mask)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;

  pointer = gdk_wayland_device_get_pointer (wayland_device);

  if (!pointer)
    return NULL;

  if (win_x)
    *win_x = pointer->surface_x;
  if (win_y)
    *win_y = pointer->surface_y;
  if (mask)
    *mask = gdk_wayland_device_get_modifiers (device);

  return pointer->focus;
}

static void
gdk_wayland_device_class_init (GdkWaylandDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_wayland_device_set_surface_cursor;
  device_class->grab = gdk_wayland_device_grab;
  device_class->ungrab = gdk_wayland_device_ungrab;
  device_class->surface_at_position = gdk_wayland_device_surface_at_position;
}

static void
gdk_wayland_device_init (GdkWaylandDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_AXIS_Y, 0, 0, 1);
}

GdkWaylandPointerData *
gdk_wayland_device_get_pointer (GdkWaylandDevice *wayland_device)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  return priv->pointer;
}

void
gdk_wayland_device_set_pointer (GdkWaylandDevice      *wayland_device,
                                GdkWaylandPointerData *pointer)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  priv->pointer = pointer;
}

GdkWaylandTouchData *
gdk_wayland_device_get_emulating_touch (GdkWaylandDevice *wayland_device)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  return priv->emulating_touch;
}

void
gdk_wayland_device_set_emulating_touch (GdkWaylandDevice    *wayland_device,
                                        GdkWaylandTouchData *touch)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  priv->emulating_touch = touch;
}

gboolean
gdk_wayland_device_update_surface_cursor (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);
  struct wl_buffer *buffer;
  int x, y, w, h, scale;
  guint next_image_index, next_image_delay;
  gboolean retval = G_SOURCE_REMOVE;
  GdkWaylandTabletData *tablet;

  tablet = gdk_wayland_seat_find_tablet (seat, device);

  if (pointer->cursor)
    {
      buffer = _gdk_wayland_cursor_get_buffer (GDK_WAYLAND_DISPLAY (seat->display),
                                               pointer->cursor,
                                               pointer->current_output_scale,
                                               pointer->cursor_image_index,
                                               &x, &y, &w, &h, &scale);
    }
  else
    {
      pointer->cursor_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (tablet)
    {
      if (!tablet->current_tool)
        {
          pointer->cursor_timeout_id = 0;
          return G_SOURCE_REMOVE;
        }

      zwp_tablet_tool_v2_set_cursor (tablet->current_tool->wp_tablet_tool,
                                     pointer->enter_serial,
                                     pointer->pointer_surface,
                                     x, y);
    }
  else if (seat->wl_pointer)
    {
      wl_pointer_set_cursor (seat->wl_pointer,
                             pointer->enter_serial,
                             pointer->pointer_surface,
                             x, y);
    }
  else
    {
      pointer->cursor_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (buffer)
    {
      wl_surface_attach (pointer->pointer_surface, buffer, 0, 0);
      if (wl_surface_get_version (pointer->pointer_surface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
        wl_surface_set_buffer_scale (pointer->pointer_surface, scale);
      wl_surface_damage (pointer->pointer_surface,  0, 0, w, h);
      wl_surface_commit (pointer->pointer_surface);
    }
  else
    {
      wl_surface_attach (pointer->pointer_surface, NULL, 0, 0);
      wl_surface_commit (pointer->pointer_surface);
    }

  next_image_index =
    _gdk_wayland_cursor_get_next_image_index (GDK_WAYLAND_DISPLAY (seat->display),
                                              pointer->cursor,
                                              pointer->current_output_scale,
                                              pointer->cursor_image_index,
                                              &next_image_delay);

  if (next_image_index != pointer->cursor_image_index)
    {
      if (next_image_delay != pointer->cursor_image_delay ||
          pointer->cursor_timeout_id == 0)
        {
          guint id;
          GSource *source;

          gdk_wayland_seat_stop_cursor_animation (seat, pointer);

          /* Queue timeout for next frame */
          id = g_timeout_add (next_image_delay,
                              (GSourceFunc) gdk_wayland_device_update_surface_cursor,
                              device);
          source = g_main_context_find_source_by_id (NULL, id);
          g_source_set_static_name (source, "[gtk] gdk_wayland_device_update_surface_cursor");
          pointer->cursor_timeout_id = id;
        }
      else
        retval = G_SOURCE_CONTINUE;

      pointer->cursor_image_index = next_image_index;
      pointer->cursor_image_delay = next_image_delay;
    }
  else
    gdk_wayland_seat_stop_cursor_animation (seat, pointer);

  return retval;
}

GdkModifierType
gdk_wayland_device_get_modifiers (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);
  GdkModifierType mask;

  mask = seat->key_modifiers;

  if (pointer)
    mask |= pointer->button_modifiers;

  return mask;
}

void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkSurface       *surface,
                                double           *win_x,
                                double           *win_y,
                                GdkModifierType  *mask)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;
  double x, y;

  if (mask)
    *mask = gdk_wayland_device_get_modifiers (device);

  pointer = gdk_wayland_device_get_pointer (wayland_device);

  if (pointer->focus == surface)
    {
      x = pointer->surface_x;
      y = pointer->surface_y;
    }
  else
    {
      x = y = -1;
    }

  if (win_x)
    *win_x = x;
  if (win_y)
    *win_y = y;
}

GdkSurface *
gdk_wayland_device_get_focus (GdkDevice *device)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;

  if (device == wayland_seat->logical_keyboard)
    return wayland_seat->keyboard_focus;
  else
    {
      pointer = gdk_wayland_device_get_pointer (wayland_device);

      if (pointer)
        return pointer->focus;
    }

  return NULL;
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

void
gdk_wayland_device_unset_touch_grab (GdkDevice        *gdk_device,
                                     GdkEventSequence *sequence)
{
  GdkWaylandSeat *seat;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  touch = gdk_wayland_seat_get_touch (seat,
                                      GDK_EVENT_SEQUENCE_TO_SLOT (sequence));

  if (touch ==
      gdk_wayland_device_get_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch)))
    {
      gdk_wayland_device_set_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch),
                                              NULL);
      emulate_touch_crossing (touch->surface, NULL,
                              seat->logical_touch, seat->touch,
                              touch, GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL,
                              GDK_CURRENT_TIME);
    }

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
}

/**
 * gdk_wayland_device_get_wl_seat: (skip)
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_seat` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_seat`
 */
struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_seat;
}

/**
 * gdk_wayland_device_get_wl_pointer: (skip)
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_pointer` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_pointer`
 */
struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_pointer;
}

/**
 * gdk_wayland_device_get_wl_keyboard: (skip)
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_keyboard` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_keyboard`
 */
struct wl_keyboard *
gdk_wayland_device_get_wl_keyboard (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_keyboard;
}

/**
 * gdk_wayland_device_get_xkb_keymap:
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the `xkb_keymap` of a `GdkDevice`.
 *
 * Returns: (transfer none): a `struct xkb_keymap`
 *
 * Since: 4.4
 */
struct xkb_keymap *
gdk_wayland_device_get_xkb_keymap (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return _gdk_wayland_keymap_get_xkb_keymap (seat->keymap);
}

GdkKeymap *
_gdk_wayland_device_get_keymap (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->keymap;
}

/**
 * gdk_wayland_device_get_data_device: (skip)
 * @gdk_device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_data_device` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_data_device`
 */
struct wl_data_device *
gdk_wayland_device_get_data_device (GdkDevice *gdk_device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  return seat->data_device;
}

/**
 * gdk_wayland_device_set_selection: (skip)
 * @gdk_device: (type GdkWaylandDevice): a `GdkDevice`
 * @source: the data source for the selection
 *
 * Sets the selection of the `GdkDevice.
 *
 * This is calling wl_data_device_set_selection() on
 * the `wl_data_device` of @gdk_device.
 */
void
gdk_wayland_device_set_selection (GdkDevice             *gdk_device,
                                  struct wl_data_source *source)
{
  GdkWaylandSeat *seat;
  guint32 serial;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL);
  wl_data_device_set_selection (seat->data_device, source, serial);
}

/**
 * gdk_wayland_device_get_node_path:
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the `/dev/input/event*` path of this device.
 *
 * For `GdkDevice`s that possibly coalesce multiple hardware
 * devices (eg. mouse, keyboard, touch,...), this function
 * will return %NULL.
 *
 * This is most notably implemented for devices of type
 * %GDK_SOURCE_PEN, %GDK_SOURCE_TABLET_PAD.
 *
 * Returns: (nullable) (transfer none): the `/dev/input/event*`
 *   path of this device
 */
const char *
gdk_wayland_device_get_node_path (GdkDevice *device)
{
  GdkWaylandTabletData *tablet;
  GdkWaylandTabletPadData *pad;

  GdkSeat *seat;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  seat = gdk_device_get_seat (device);
  tablet = gdk_wayland_seat_find_tablet (GDK_WAYLAND_SEAT (seat), device);
  if (tablet)
    return tablet->path;

  pad = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat), device);
  if (pad)
    return pad->path;

  return NULL;
}

static void
emulate_focus (GdkSurface *surface,
               GdkDevice *device,
               gboolean   focus_in,
               guint32    time_)
{
  GdkEvent *event = gdk_focus_event_new (surface, device, focus_in);

  _gdk_wayland_display_deliver_event (gdk_surface_get_display (surface), event);
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
device_emit_grab_crossing (GdkDevice       *device,
                           GdkSurface       *from,
                           GdkSurface       *to,
                           GdkCrossingMode  mode,
                           guint32          time_)
{
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      if (from)
        emulate_focus (from, device, FALSE, time_);
      if (to)
        emulate_focus (to, device, TRUE, time_);
    }
  else
    {
      if (from)
        emulate_crossing (from, to, device, GDK_LEAVE_NOTIFY, mode, time_);
      if (to)
        emulate_crossing (to, from, device, GDK_ENTER_NOTIFY, mode, time_);
    }
}

void
gdk_wayland_device_maybe_emit_grab_crossing (GdkDevice  *device,
                                             GdkSurface *window,
                                             guint32     time)
{
  GdkSurface *surface = gdk_wayland_device_get_focus (device);
  GdkSurface *focus = window;

  if (focus != surface)
    device_emit_grab_crossing (device, focus, window, GDK_CROSSING_GRAB, time);
}

GdkSurface*
gdk_wayland_device_maybe_emit_ungrab_crossing (GdkDevice *device,
                                               guint32    time_)
{
  GdkDeviceGrabInfo *grab;
  GdkSurface *focus = NULL;
  GdkSurface *surface = NULL;
  GdkSurface *prev_focus = NULL;

  focus = gdk_wayland_device_get_focus (device);
  grab = _gdk_display_get_last_device_grab (gdk_device_get_display (device), device);

  if (grab)
    {
      prev_focus = grab->surface;
      surface = grab->surface;
    }

  if (focus != surface)
    device_emit_grab_crossing (device, prev_focus, focus, GDK_CROSSING_UNGRAB, time_);

  return prev_focus;
}
