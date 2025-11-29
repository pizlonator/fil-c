/*
 * Copyright 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright 1998-2002 Tor Lillqvist
 * Copyright 2005-2008 Imendio AB
 * Copyright 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#import "GdkMacosWindow.h"
#import "GdkMacosBaseView.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacoskeymap-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacosseat-private.h"

#include "gdk/gdkeventsprivate.h"

#define GDK_MOD2_MASK (1 << 4)
#define GRIP_WIDTH 15
#define GRIP_HEIGHT 15
#define GDK_LION_RESIZE 5

static gboolean
test_resize (NSEvent         *event,
             GdkMacosSurface *surface,
             int              x,
             int              y)
{
  NSWindow *window;

  g_assert (event != NULL);
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  window = _gdk_macos_surface_get_native (surface);

  /* Resizing from the resize indicator only begins if an NSLeftMouseButton
   * event is received in the resizing area.
   */
  if ([event type] == NSEventTypeLeftMouseDown &&
      [window showsResizeIndicator])
    {
      NSRect frame;

      /* If the resize indicator is visible and the event is in the lower
       * right 15x15 corner, we leave these events to Cocoa as to be
       * handled as resize events.  Applications may have widgets in this
       * area.  These will most likely be larger than 15x15 and for scroll
       * bars there are also other means to move the scroll bar.  Since
       * the resize indicator is the only way of resizing windows on Mac
       * OS, it is too important to not make functional.
       */
      frame = [[window contentView] bounds];
      if (x > frame.size.width - GRIP_WIDTH &&
          x < frame.size.width &&
          y > frame.size.height - GRIP_HEIGHT &&
          y < frame.size.height)
        return TRUE;
     }

  /* If we're on Lion and within 5 pixels of an edge, then assume that the
   * user wants to resize, and return NULL to let Quartz get on with it.
   * We check the selector isRestorable to see if we're on 10.7.  This
   * extra check is in case the user starts dragging before GDK recognizes
   * the grab.
   *
   * We perform this check for a button press of all buttons, because we
   * do receive, for instance, a right mouse down event for a GDK surface
   * for x-coordinate range [-3, 0], but we do not want to forward this
   * into GDK. Forwarding such events into GDK will confuse the pointer
   * window finding code, because there are no GdkSurfaces present in
   * the range [-3, 0].
   */
  if (([event type] == NSEventTypeLeftMouseDown ||
       [event type] == NSEventTypeRightMouseDown ||
       [event type] == NSEventTypeOtherMouseDown))
    {
      if (x < GDK_LION_RESIZE ||
          x > GDK_SURFACE (surface)->width - GDK_LION_RESIZE ||
          y > GDK_SURFACE (surface)->height - GDK_LION_RESIZE)
        return TRUE;
    }

  return FALSE;
}

static guint32
get_time_from_ns_event (NSEvent *event)
{
  double time = [event timestamp];
  /* cast via double->uint64 conversion to make sure that it is
   * wrapped on 32-bit machines when it overflows
   */
  return (guint32) (guint64) (time * 1000.0);
}

static int
get_mouse_button_from_ns_event (NSEvent *event)
{
  NSInteger button = [event buttonNumber];

  switch (button)
    {
    case 0:
      return 1;
    case 1:
      return 3;
    case 2:
      return 2;
    default:
      return button + 1;
    }
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_buttons (NSUInteger nsbuttons)
{
  GdkModifierType modifiers = 0;

  if (nsbuttons & (1 << 0))
    modifiers |= GDK_BUTTON1_MASK;
  if (nsbuttons & (1 << 1))
    modifiers |= GDK_BUTTON3_MASK;
  if (nsbuttons & (1 << 2))
    modifiers |= GDK_BUTTON2_MASK;
  if (nsbuttons & (1 << 3))
    modifiers |= GDK_BUTTON4_MASK;
  if (nsbuttons & (1 << 4))
    modifiers |= GDK_BUTTON5_MASK;

  return modifiers;
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_event (NSEvent *event)
{
  GdkModifierType state = 0;
  int button;

  /* This maps buttons 1 to 5 to GDK_BUTTON[1-5]_MASK */
  button = get_mouse_button_from_ns_event (event);
  if (button >= 1 && button <= 5)
    state = (1 << (button + 7));

  return state;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_flags (NSUInteger nsflags)
{
  GdkModifierType modifiers = 0;

  if (nsflags & NSEventModifierFlagCapsLock)
    modifiers |= GDK_LOCK_MASK;
  if (nsflags & NSEventModifierFlagShift)
    modifiers |= GDK_SHIFT_MASK;
  if (nsflags & NSEventModifierFlagControl)
    modifiers |= GDK_CONTROL_MASK;
  if (nsflags & NSEventModifierFlagOption)
    modifiers |= GDK_ALT_MASK;
  if (nsflags & NSEventModifierFlagCommand)
    modifiers |= GDK_MOD2_MASK;

  return modifiers;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_event (NSEvent *nsevent)
{
  return get_keyboard_modifiers_from_ns_flags ([nsevent modifierFlags]);
}

GdkModifierType
_gdk_macos_display_get_current_mouse_modifiers (GdkMacosDisplay *self)
{
  return get_mouse_button_modifiers_from_ns_buttons ([NSEvent pressedMouseButtons]);
}

GdkModifierType
_gdk_macos_display_get_current_keyboard_modifiers (GdkMacosDisplay *self)
{
  return get_keyboard_modifiers_from_ns_flags ([NSEvent modifierFlags]);
}

static GdkEvent *
fill_button_event (GdkMacosDisplay *display,
                   GdkMacosSurface *surface,
                   NSEvent         *nsevent,
                   int              x,
                   int              y)
{
  GdkSeat *seat;
  GdkEventType type;
  GdkModifierType state;
  GdkDevice *pointer = NULL;
  GdkDeviceTool *tool = NULL;
  double *axes = NULL;
  cairo_region_t *input_region;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_macos_display_get_current_mouse_modifiers (display);
  input_region = GDK_SURFACE (surface)->input_region;

  switch ((int)[nsevent type])
    {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
      type = GDK_BUTTON_PRESS;
      state &= ~get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
      type = GDK_BUTTON_RELEASE;
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    default:
      g_assert_not_reached ();
    }

  /* Ignore button press events outside the window coords but
   * allow for button release which can happen during grabs.
   */
  if (type == GDK_BUTTON_PRESS &&
      (x < 0 || x > GDK_SURFACE (surface)->width ||
       y < 0 || y > GDK_SURFACE (surface)->height ||
       (input_region && !cairo_region_contains_point (input_region, x, y))))
    return NULL;

  if (([nsevent subtype] == NSEventSubtypeTabletPoint) &&
      _gdk_macos_seat_get_tablet (GDK_MACOS_SEAT (seat), &pointer, &tool))
    axes = _gdk_macos_seat_get_tablet_axes_from_nsevent (GDK_MACOS_SEAT (seat), nsevent);
  else
    pointer = gdk_seat_get_pointer (seat);

  return gdk_button_event_new (type,
                               GDK_SURFACE (surface),
                               pointer,
                               tool,
                               get_time_from_ns_event (nsevent),
                               state,
                               get_mouse_button_from_ns_event (nsevent),
                               x,
                               y,
                               axes);
}

static GdkEvent *
synthesize_crossing_event (GdkMacosDisplay *display,
                           GdkMacosSurface *surface,
                           NSEvent         *nsevent,
                           int              x,
                           int              y)
{
  GdkEventType event_type;
  GdkModifierType state;
  GdkSeat *seat;

  switch ((int)[nsevent type])
    {
    case NSEventTypeMouseEntered:
      event_type = GDK_ENTER_NOTIFY;
      break;

    case NSEventTypeMouseExited:
      event_type = GDK_LEAVE_NOTIFY;
      break;

    default:
      g_return_val_if_reached (NULL);
    }

  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_macos_display_get_current_mouse_modifiers (display);
  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));

  return gdk_crossing_event_new (event_type,
                                 GDK_SURFACE (surface),
                                 gdk_seat_get_pointer (seat),
                                 get_time_from_ns_event (nsevent),
                                 state,
                                 x,
                                 y,
                                 GDK_CROSSING_NORMAL,
                                 GDK_NOTIFY_NONLINEAR);
}

static inline guint
get_group_from_ns_event (NSEvent *nsevent)
{
  return ([nsevent modifierFlags] & NSEventModifierFlagOption) ? 1 : 0;
}

static void
add_virtual_modifiers (GdkModifierType *state)
{
  if (*state & GDK_MOD2_MASK)
    *state |= GDK_META_MASK;
}

static GdkEvent *
fill_key_event (GdkMacosDisplay *display,
                GdkMacosSurface *surface,
                NSEvent         *nsevent,
                GdkEventType     type)
{
  GdkTranslatedKey translated = {0};
  GdkTranslatedKey no_lock = {0};
  GdkModifierType consumed;
  GdkModifierType state;
  GdkKeymap *keymap;
  gboolean is_modifier;
  GdkSeat *seat;
  guint keycode;
  guint keyval;
  guint group;
  int layout;
  int level;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (nsevent != NULL);

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
  keymap = gdk_display_get_keymap (GDK_DISPLAY (display));
  keycode = [nsevent keyCode];
  keyval = GDK_KEY_VoidSymbol;
  state = get_keyboard_modifiers_from_ns_event (nsevent);
  group = get_group_from_ns_event (nsevent);
  is_modifier = _gdk_macos_keymap_is_modifier (keycode);

  gdk_keymap_translate_keyboard_state (keymap, keycode, state, group,
                                       &keyval, &layout, &level, &consumed);

  /* If the key press is a modifier, the state should include the mask for
   * that modifier but only for releases, not presses. This matches the
   * X11 backend behavior.
   */
  if (is_modifier)
    {
      guint mask = 0;

      switch (keyval)
        {
        case GDK_KEY_Meta_R:
        case GDK_KEY_Meta_L:
          mask = GDK_MOD2_MASK;
          break;
        case GDK_KEY_Shift_R:
        case GDK_KEY_Shift_L:
          mask = GDK_SHIFT_MASK;
          break;
        case GDK_KEY_Caps_Lock:
          mask = GDK_LOCK_MASK;
          break;
        case GDK_KEY_Alt_R:
        case GDK_KEY_Alt_L:
          mask = GDK_ALT_MASK;
          break;
        case GDK_KEY_Control_R:
        case GDK_KEY_Control_L:
          mask = GDK_CONTROL_MASK;
          break;
        default:
          mask = 0;
        }

      if (type == GDK_KEY_PRESS)
        state &= ~mask;
      else if (type == GDK_KEY_RELEASE)
        state |= mask;
    }

  state |= _gdk_macos_display_get_current_mouse_modifiers (display);
  add_virtual_modifiers (&state);

  translated.keyval = keyval;
  translated.consumed = consumed;
  translated.layout = layout;
  translated.level = level;

  if (state & GDK_LOCK_MASK)
    {
      gdk_keymap_translate_keyboard_state (keymap,
                                           keycode,
                                           state & ~GDK_LOCK_MASK,
                                           group,
                                           &keyval,
                                           &layout,
                                           &level,
                                           &consumed);

      no_lock.keyval = keycode;
      no_lock.consumed = consumed;
      no_lock.layout = layout;
      no_lock.level = level;
    }
  else
    {
      no_lock = translated;
    }

  return gdk_key_event_new (type,
                            GDK_SURFACE (surface),
                            gdk_seat_get_keyboard (seat),
                            get_time_from_ns_event (nsevent),
                            [nsevent keyCode],
                            state,
                            is_modifier,
                            &translated,
                            &no_lock,
                            NULL);
}

static GdkEvent *
fill_pinch_event (GdkMacosDisplay *display,
                  GdkMacosSurface *surface,
                  NSEvent         *nsevent,
                  int              x,
                  int              y)
{
  static double last_scale = 1.0;
  static enum {
    FP_STATE_IDLE,
    FP_STATE_UPDATE
  } last_state = FP_STATE_IDLE;
  GdkSeat *seat;
  GdkTouchpadGesturePhase phase;
  double angle_delta = 0.0;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  /* fill_pinch_event handles the conversion from the two OSX gesture events
   * NSEventTypeMagnfiy and NSEventTypeRotate to the GDK_TOUCHPAD_PINCH event.
   * The normal behavior of the OSX events is that they produce as sequence of
   *   1 x NSEventPhaseBegan,
   *   n x NSEventPhaseChanged,
   *   1 x NSEventPhaseEnded
   * This can happen for both the Magnify and the Rotate events independently.
   * As both events are summarized in one GDK_TOUCHPAD_PINCH event sequence, a
   * little state machine handles the case of two NSEventPhaseBegan events in
   * a sequence, e.g. Magnify(Began), Magnify(Changed)..., Rotate(Began)...
   * such that PINCH(STARTED), PINCH(UPDATE).... will not show a second
   * PINCH(STARTED) event.
   */

  switch ((int)[nsevent phase])
    {
    case NSEventPhaseBegan:
      switch (last_state)
        {
        case FP_STATE_IDLE:
          phase = GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;
          last_state = FP_STATE_UPDATE;
          last_scale = 1.0;
          break;
        case FP_STATE_UPDATE:
          /* We have already received a PhaseBegan event but no PhaseEnded
             event. This can happen, e.g. Magnify(Began), Magnify(Change)...
             Rotate(Began), Rotate (Change),...., Magnify(End) Rotate(End)
          */
          phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
          break;
        }
      break;

    case NSEventPhaseChanged:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
      break;

    case NSEventPhaseEnded:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_END;
      switch (last_state)
        {
        case FP_STATE_IDLE:
          /* We are idle but have received a second PhaseEnded event.
             This can happen because we have Magnify and Rotate OSX
             event sequences. We just send a second end GDK_PHASE_END.
          */
          break;
        case FP_STATE_UPDATE:
          last_state = FP_STATE_IDLE;
          break;
        }
      break;

    case NSEventPhaseCancelled:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      last_state = FP_STATE_IDLE;
      break;

    case NSEventPhaseMayBegin:
    case NSEventPhaseStationary:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  switch ((int)[nsevent type])
    {
    case NSEventTypeMagnify:
      last_scale *= [nsevent magnification] + 1.0;
      angle_delta = 0.0;
      break;

    case NSEventTypeRotate:
      angle_delta = - [nsevent rotation] * G_PI / 180.0;
      break;

    default:
      g_assert_not_reached ();
    }

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));

  return gdk_touchpad_event_new_pinch (GDK_SURFACE (surface),
                                       NULL, /* FIXME make up sequences */
                                       gdk_seat_get_pointer (seat),
                                       get_time_from_ns_event (nsevent),
                                       get_keyboard_modifiers_from_ns_event (nsevent),
                                       phase,
                                       x,
                                       y,
                                       2,
                                       0.0,
                                       0.0,
                                       last_scale,
                                       angle_delta);

}

static gboolean
is_motion_event (NSEventType event_type)
{
  return (event_type == NSEventTypeLeftMouseDragged ||
          event_type == NSEventTypeRightMouseDragged ||
          event_type == NSEventTypeOtherMouseDragged ||
          event_type == NSEventTypeMouseMoved);
}

static GdkEvent *
fill_motion_event (GdkMacosDisplay *display,
                   GdkMacosSurface *surface,
                   NSEvent         *nsevent,
                   int              x,
                   int              y)
{
  GdkSeat *seat;
  GdkModifierType state;
  GdkDevice *pointer = NULL;
  GdkDeviceTool *tool = NULL;
  double *axes = NULL;

  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (nsevent != NULL);
  g_assert (is_motion_event ([nsevent type]));

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
  state = get_keyboard_modifiers_from_ns_event (nsevent) |
          _gdk_macos_display_get_current_mouse_modifiers (display);

  if (([nsevent subtype] == NSEventSubtypeTabletPoint) &&
      _gdk_macos_seat_get_tablet (GDK_MACOS_SEAT (seat), &pointer, &tool))
    axes = _gdk_macos_seat_get_tablet_axes_from_nsevent (GDK_MACOS_SEAT (seat), nsevent);
  else
    pointer = gdk_seat_get_pointer (seat);

  return gdk_motion_event_new (GDK_SURFACE (surface),
                               pointer,
                               tool,
                               get_time_from_ns_event (nsevent),
                               state,
                               x,
                               y,
                               axes);
}

static GdkEvent *
fill_scroll_event (GdkMacosDisplay *self,
                   GdkMacosSurface *surface,
                   NSEvent         *nsevent,
                   int              x,
                   int              y)
{
  GdkScrollDirection direction = 0;
  GdkModifierType state;
  GdkDevice *pointer;
  GdkEvent *ret = NULL;
  NSEventPhase phase;
  NSEventPhase momentumPhase;
  GdkSeat *seat;
  double dx;
  double dy;

  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (nsevent != NULL);

  phase = [nsevent phase];
  momentumPhase = [nsevent momentumPhase];

  /* Ignore kinetic scroll events from the display server as we already
   * handle those internally.
   */
  if (phase == 0 && momentumPhase != 0)
    return GDK_MACOS_EVENT_DROP;

  seat = gdk_display_get_default_seat (GDK_DISPLAY (self));
  pointer = gdk_seat_get_pointer (seat);
  state = _gdk_macos_display_get_current_mouse_modifiers (self) |
          _gdk_macos_display_get_current_keyboard_modifiers (self);

  /* If we are starting a new phase, send a stop so any previous
   * scrolling immediately stops.
   */
  if (phase == NSEventPhaseMayBegin)
    return gdk_scroll_event_new (GDK_SURFACE (surface),
                                 pointer,
                                 NULL,
                                 get_time_from_ns_event (nsevent),
                                 state,
                                 0.0, 0.0, TRUE,
                                 GDK_SCROLL_UNIT_SURFACE);

  dx = [nsevent deltaX];
  dy = [nsevent deltaY];

  if ([nsevent hasPreciseScrollingDeltas])
    {
      double sx;
      double sy;

      sx = [nsevent scrollingDeltaX];
      sy = [nsevent scrollingDeltaY];

      if (sx != 0.0 || sy != 0.0)
        ret = gdk_scroll_event_new (GDK_SURFACE (surface),
                                    pointer,
                                    NULL,
                                    get_time_from_ns_event (nsevent),
                                    state,
                                    -sx,
                                    -sy,
                                    FALSE,
                                    GDK_SCROLL_UNIT_SURFACE);

      /* Fall through for scroll emulation */
    }

  if (dy != 0.0)
    {
      if (dy < 0.0)
        direction = GDK_SCROLL_DOWN;
      else
        direction = GDK_SCROLL_UP;

      dx = 0.0;
    }
  else if (dx != 0.0)
    {
      if (dx < 0.0)
        direction = GDK_SCROLL_RIGHT;
      else
        direction = GDK_SCROLL_LEFT;

      dy = 0.0;
    }

  if ((dx != 0.0 || dy != 0.0) && ![nsevent hasPreciseScrollingDeltas])
    {
      g_assert (ret == NULL);

      ret = gdk_scroll_event_new_discrete (GDK_SURFACE (surface),
                                           pointer,
                                           NULL,
                                           get_time_from_ns_event (nsevent),
                                           state,
                                           direction);
    }

  if (phase == NSEventPhaseEnded || phase == NSEventPhaseCancelled)
    {
      /* The user must have released their fingers in a touchpad
       * scroll, so try to send a scroll is_stop event.
       */
      if (ret != NULL)
        _gdk_event_queue_append (GDK_DISPLAY (self), g_steal_pointer (&ret));
      ret = gdk_scroll_event_new (GDK_SURFACE (surface),
                                  pointer,
                                  NULL,
                                  get_time_from_ns_event (nsevent),
                                  state,
                                  0.0, 0.0, TRUE,
                                  GDK_SCROLL_UNIT_SURFACE);
    }

  return g_steal_pointer (&ret);
}


static GdkEvent *
fill_event (GdkMacosDisplay *self,
            GdkMacosWindow  *window,
            NSEvent         *nsevent,
            int              x,
            int              y)
{
  GdkMacosSurface *surface = [window gdkSurface];
  NSEventType event_type = [nsevent type];
  GdkEvent *ret = NULL;

  switch ((int)event_type)
    {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
      ret = fill_button_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved:
      ret = fill_motion_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      ret = fill_pinch_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeMouseExited:
    case NSEventTypeMouseEntered:
      {
        GdkSeat *seat = gdk_display_get_default_seat (GDK_DISPLAY (self));
        GdkDevice *pointer = gdk_seat_get_pointer (seat);
        GdkDeviceGrabInfo *grab = _gdk_display_get_last_device_grab (GDK_DISPLAY (self), pointer);

        if ([(GdkMacosWindow *)window isInManualResizeOrMove])
          {
            ret = GDK_MACOS_EVENT_DROP;
          }
        else if (grab == NULL || grab->owner_events)
          {
            if (event_type == NSEventTypeMouseExited)
              [[NSCursor arrowCursor] set];

            ret = synthesize_crossing_event (self, surface, nsevent, x, y);
          }
      }

      break;

    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged: {
      GdkEventType type = _gdk_macos_keymap_get_event_type (nsevent);

      if (type)
        ret = fill_key_event (self, surface, nsevent, type);

      break;
    }

    case NSEventTypeScrollWheel:
      ret = fill_scroll_event (self, surface, nsevent, x, y);
      break;

    default:
      break;
    }

  return ret;
}

static gboolean
is_mouse_button_press_event (NSEventType type)
{
  switch ((int)type)
    {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
      return TRUE;

    default:
      return FALSE;
    }
}

static void
get_surface_point_from_screen_point (GdkSurface *surface,
                                     NSPoint     screen_point,
                                     int        *x,
                                     int        *y)
{
  NSWindow *nswindow;
  NSPoint point;

  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));
  point = convert_nspoint_from_screen (nswindow, screen_point);

  *x = point.x;
  *y = surface->height - point.y;
}

static GdkSurface *
find_surface_under_pointer (GdkMacosDisplay *self,
                            NSPoint          screen_point,
                            int             *x,
                            int             *y)
{
  GdkMacosSurface *surface;
  int x_tmp, y_tmp;

  surface = _gdk_macos_display_get_surface_at_display_coords (self,
                                                              screen_point.x,
                                                              screen_point.y,
                                                              &x_tmp,
                                                              &y_tmp);

  if (surface != NULL)
    {
      _gdk_macos_display_from_display_coords (self,
                                              screen_point.x,
                                              screen_point.y,
                                              &x_tmp, &y_tmp);
      *x = x_tmp - surface->root_x;
      *y = y_tmp - surface->root_y;
      /* If the coordinates are out of window bounds, this surface is not
       * under the pointer and we thus return NULL. This can occur when
       * surface under pointer has not yet been updated due to a very recent
       * window resize. Alternatively, we should no longer be relying on
       * the surface_under_pointer value which is maintained in gdkwindow.c.
       */
      if (*x < 0 ||
          *y < 0 ||
          *x >= GDK_SURFACE (surface)->width ||
          *y >= GDK_SURFACE (surface)->height)
        surface = NULL;
    }

  return GDK_SURFACE (surface);
}

static GdkSurface *
get_surface_from_ns_event (GdkMacosDisplay *self,
                           NSEvent         *nsevent,
                           NSPoint         *screen_point,
                           int             *x,
                           int             *y)
{
  GdkSurface *surface = NULL;
  NSWindow *nswindow = [nsevent window];

  if (GDK_IS_MACOS_WINDOW (nswindow))
    {
      GdkMacosBaseView *view;
      NSPoint point, view_point;
      NSRect view_frame;

      view = (GdkMacosBaseView *)[nswindow contentView];
      if (!GDK_IS_MACOS_BASE_VIEW (view))
        goto find_under_pointer;

      surface = GDK_SURFACE ([view gdkSurface]);

      point = [nsevent locationInWindow];
      view_point = [view convertPoint:point fromView:nil];
      view_frame = [view frame];

      /* NSEvents come in with a window set, but with window coordinates
       * out of window bounds. For e.g. moved events this is fine, we use
       * this information to properly handle enter/leave notify and motion
       * events. For mouse button press/release, we want to avoid forwarding
       * these events however, because the window they relate to is not the
       * window set in the event. This situation appears to occur when button
       * presses come in just before (or just after?) a window is resized and
       * also when a button press occurs on the OS X window titlebar.
       *
       * By setting surface to NULL, we do another attempt to get the right
       * surface window below.
       */
      if (is_mouse_button_press_event ([nsevent type]) &&
          (view_point.x < view_frame.origin.x ||
           view_point.x >= view_frame.origin.x + view_frame.size.width ||
           view_point.y < view_frame.origin.y ||
           view_point.y >= view_frame.origin.y + view_frame.size.height))
        {
          NSRect windowRect = [nswindow frame];

          surface = NULL;

          /* This is a hack for button presses to break all grabs. E.g. if
           * a menu is open and one clicks on the title bar (or anywhere
           * out of window bounds), we really want to pop down the menu (by
           * breaking the grabs) before OS X handles the action of the title
           * bar button.
           *
           * Because we cannot ingest this event into GDK, we have to do it
           * here, not very nice.
           */
          _gdk_macos_display_break_all_grabs (self, get_time_from_ns_event (nsevent));

          /* If the X,Y is on the frame itself, then we don't want to discover
           * the surface under the pointer at all so that we let OS X handle
           * it instead. We add padding to include resize operations too.
           */
          windowRect.origin.x = -GDK_LION_RESIZE;
          windowRect.origin.y = -GDK_LION_RESIZE;
          windowRect.size.width += (2 * GDK_LION_RESIZE);
          windowRect.size.height += (2 * GDK_LION_RESIZE);
          if (NSPointInRect (point, windowRect))
            return NULL;
        }
      else
        {
          *screen_point = convert_nspoint_to_screen ([nsevent window], point);
          *x = point.x;
          *y = surface->height - point.y;
        }
    }

find_under_pointer:

  if (surface == NULL && nswindow == NULL)
    {
      /* Fallback used when no NSWindow set. This happens e.g. when
       * we allow motion events without a window set in gdk_event_translate()
       * that occur immediately after the main menu bar was clicked/used.
       * This fallback will not return coordinates contained in a window's
       * titlebar.
       */
      *screen_point = [NSEvent mouseLocation];
      surface = find_surface_under_pointer (self, *screen_point, x, y);
    }

  return surface;
}

static GdkMacosSurface *
find_surface_for_keyboard_event (NSEvent *nsevent)
{
  NSView *nsview = [[nsevent window] contentView];

  if (GDK_IS_MACOS_BASE_VIEW (nsview))
    {
      GdkMacosBaseView *view = (GdkMacosBaseView *)nsview;
      GdkSurface *surface = GDK_SURFACE ([view gdkSurface]);
      GdkDisplay *display = gdk_surface_get_display (surface);
      GdkSeat *seat = gdk_display_get_default_seat (display);
      GdkDevice *device = gdk_seat_get_keyboard (seat);
      GdkDeviceGrabInfo *grab = _gdk_display_get_last_device_grab (display, device);

      if (grab && grab->surface && !grab->owner_events)
        return GDK_MACOS_SURFACE (grab->surface);

      return GDK_MACOS_SURFACE (surface);
    }

  return NULL;
}

static GdkMacosSurface *
find_surface_for_mouse_event (GdkMacosDisplay *self,
                              NSEvent         *nsevent,
                              int             *x,
                              int             *y)
{
  NSPoint point;
  NSEventType event_type;
  GdkSurface *surface;
  GdkDisplay *display;
  GdkDevice *pointer;
  GdkDeviceGrabInfo *grab;
  GdkSeat *seat;

  /* Even if we had a surface window, it might be for something outside
   * the input region (shadow) which we might want to ignore. This is
   * handled for us deeper in the event unwrapping.
   */
  if (!(surface = get_surface_from_ns_event (self, nsevent, &point, x, y)))
    return NULL;

  display = gdk_surface_get_display (surface);
  seat = gdk_display_get_default_seat (GDK_DISPLAY (self));
  pointer = gdk_seat_get_pointer (seat);

  event_type = [nsevent type];

  /* From the docs for XGrabPointer:
   *
   * If owner_events is True and if a generated pointer event
   * would normally be reported to this client, it is reported
   * as usual. Otherwise, the event is reported with respect to
   * the grab_window and is reported only if selected by
   * event_mask. For either value of owner_events, unreported
   * events are discarded.
   */
  if ((grab = _gdk_display_get_last_device_grab (display, pointer)))
    {
      if (grab->owner_events)
        {
          /* For owner events, we need to use the surface under the
           * pointer, not the window from the NSEvent, since that is
           * reported with respect to the key window, which could be
           * wrong.
           */
          GdkSurface *surface_under_pointer;
          int x_tmp, y_tmp;

          surface_under_pointer = find_surface_under_pointer (self, point, &x_tmp, &y_tmp);
          if (surface_under_pointer)
            {
              surface = surface_under_pointer;
              *x = x_tmp;
              *y = y_tmp;
            }

          return GDK_MACOS_SURFACE (surface);
        }
      else
        {
          /* Finally check the grab window. */
          GdkSurface *grab_surface = grab->surface;
          get_surface_point_from_screen_point (grab_surface, point, x, y);
          return GDK_MACOS_SURFACE (grab_surface);
        }

      return NULL;
    }
  else
    {
      /* The non-grabbed case. */
      GdkSurface *surface_under_pointer;
      int x_tmp, y_tmp;

      /* Ignore all events but mouse moved that might be on the title
       * bar (above the content view). The reason is that otherwise
       * gdk gets confused about getting e.g. button presses with no
       * window (the title bar is not known to it).
       */
      if (event_type != NSEventTypeMouseMoved)
        {
          if (*y < 0)
            return NULL;
        }

      /* As for owner events, we need to use the surface under the
       * pointer, not the window from the NSEvent.
       */
      surface_under_pointer = find_surface_under_pointer (self, point, &x_tmp, &y_tmp);
      if (surface_under_pointer != NULL)
        {
          surface = surface_under_pointer;
          *x = x_tmp;
          *y = y_tmp;
        }

      return GDK_MACOS_SURFACE (surface);
    }

  return NULL;
}

/* This function finds the correct window to send an event to, taking
 * into account grabs, event propagation, and event masks.
 */
static GdkMacosSurface *
find_surface_for_ns_event (GdkMacosDisplay *self,
                           NSEvent         *nsevent,
                           int             *x,
                           int             *y)
{
  GdkMacosBaseView *view;
  GdkSurface *surface;
  NSPoint point;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (nsevent != NULL);
  g_assert (x != NULL);
  g_assert (y != NULL);

  switch ((int)[nsevent type])
    {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved:
    case NSEventTypeScrollWheel:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      return find_surface_for_mouse_event (self, nsevent, x, y);

    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
      /* Only handle our own entered/exited events, not the ones for the
       * titlebar buttons.
       */
      if ((surface = get_surface_from_ns_event (self, nsevent, &point, x, y)) &&
          (view = (GdkMacosBaseView *)[GDK_MACOS_SURFACE (surface)->window contentView]) &&
          ([nsevent trackingArea] == [view trackingArea]))
        return GDK_MACOS_SURFACE (surface);
      else
        return NULL;

    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged:
      return find_surface_for_keyboard_event (nsevent);

    default:
      /* Ignore everything else. */
      return NULL;
    }
}

GdkEvent *
_gdk_macos_display_translate (GdkMacosDisplay *self,
                              NSEvent         *nsevent)
{
  GdkMacosSurface *surface;
  GdkMacosWindow *window;
  NSEventType event_type;
  NSWindow *event_window;
  int x;
  int y;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);
  g_return_val_if_fail (nsevent != NULL, NULL);

  /* There is no support for real desktop wide grabs, so we break
   * grabs when the application loses focus (gets deactivated).
   */
  event_type = [nsevent type];
  if (event_type == NSEventTypeAppKitDefined)
    {
      if ([nsevent subtype] == NSEventSubtypeApplicationDeactivated)
        _gdk_macos_display_break_all_grabs (self, get_time_from_ns_event (nsevent));

      /* This could potentially be used to break grabs when clicking
       * on the title. The subtype 20 is undocumented so it's probably
       * not a good idea: else if (subtype == 20) break_all_grabs ();
       */

      /* Leave all AppKit events to AppKit. */
      return NULL;
    }

  /* We need to register the proximity event from any point on the screen
   * to properly register the devices
   * FIXME: is there a better way to detect if a tablet has been plugged?
   */
  if (event_type == NSEventTypeTabletProximity)
    {
      GdkSeat *seat = gdk_display_get_default_seat (GDK_DISPLAY (self));

      _gdk_macos_seat_handle_tablet_tool_event (GDK_MACOS_SEAT (seat), nsevent);

      /* FIXME: we might want to cache this proximity event and propagate it
       * but proximity events in gdk work at a window level while on macos
       * works at a screen level. For now we just skip them.
       */
      return NULL;
    }

  /* If the event was delivered to NSWindow that is foreign (or rather,
   * Cocoa native), then we should pass the event along to that window.
   */
  if ((event_window = [nsevent window]) && !GDK_IS_MACOS_WINDOW (event_window))
    return NULL;

  /* If we can't find a GdkSurface to deliver the event to, then we
   * should pass it along to the NSApp.
   */
  if (!(surface = find_surface_for_ns_event (self, nsevent, &x, &y)))
    return NULL;

  if (!(window = (GdkMacosWindow *)_gdk_macos_surface_get_native (surface)) ||
      !GDK_IS_MACOS_WINDOW (window))
    return NULL;

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([window isInMove])
    {
      _gdk_macos_display_break_all_grabs (self, get_time_from_ns_event (nsevent));
      return NULL;
    }

  /* Also when in a manual resize or move , we ignore events so that
   * these are pushed to GdkMacosNSWindow's sendEvent handler.
   */
  if ([window isInManualResizeOrMove])
    return NULL;

  /* Make sure we have a GdkSurface */
  if (!(surface = [window gdkSurface]))
    return NULL;

  /* Quartz handles resizing on its own, so stay out of the way. */
  if (test_resize (nsevent, surface, x, y))
    return NULL;

  if (event_type == NSEventTypeRightMouseDown ||
      event_type == NSEventTypeOtherMouseDown ||
      event_type == NSEventTypeLeftMouseDown)
    {
      if (![NSApp isActive])
        [NSApp activateIgnoringOtherApps:YES];

      if (![window isKeyWindow])
        {
          NSWindow *orig_window = [nsevent window];

          /* To get NSApp to supress activating the window we might
           * have clicked through the shadow of, we need to dispatch
           * the event and handle it in GdkMacosView:mouseDown to call
           * [NSApp preventWindowOrdering]. Calling it here will not
           * do anything as the event is not registered.
           */
          if (orig_window &&
              GDK_IS_MACOS_WINDOW (orig_window) &&
              [(GdkMacosWindow *)orig_window needsMouseDownQuirk])
            [NSApp sendEvent:nsevent];

          [window showAndMakeKey:YES];
          _gdk_macos_display_clear_sorting (self);
        }
    }
  else if (is_motion_event(event_type))
    {
      NSWindow *orig_window = [nsevent window];

      if (orig_window && GDK_IS_MACOS_WINDOW (orig_window))
        [NSApp sendEvent:nsevent];
    }

  return fill_event (self, window, nsevent, x, y);
}

void
_gdk_macos_display_send_event (GdkMacosDisplay *self,
                               NSEvent         *nsevent)
{
  GdkMacosSurface *surface;
  GdkMacosWindow *window;
  GdkEvent *event;
  int x;
  int y;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (nsevent != NULL);

  if ((surface = find_surface_for_ns_event (self, nsevent, &x, &y)) &&
      (window = (GdkMacosWindow *)_gdk_macos_surface_get_native (surface)) &&
      (event = fill_event (self, window, nsevent, x, y)))
    _gdk_windowing_got_event (GDK_DISPLAY (self),
                              _gdk_event_queue_append (GDK_DISPLAY (self), event),
                              event,
                              _gdk_display_get_next_serial (GDK_DISPLAY (self)));
}
