/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#pragma once

#include <gdk/gdktypes.h>
#include <gdk/gdkdrag.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkdevicetool.h>

G_BEGIN_DECLS

#define GDK_EVENT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_EVENT, GdkEventClass))

typedef struct _GdkEventClass   GdkEventClass;

/*< private >
 * GdkEvent:
 * @ref_count: the reference count of the event
 * @event_type: the specialized event type
 * @surface: the surface of the event
 * @device: the device of the event
 * @time: a serial identifier of the event that can be used to order
 *   two events
 * @flags: event flags
 *
 * The abstract type for all windowing system events.
 */
struct _GdkEvent
{
  GTypeInstance parent_instance;

  grefcount ref_count;

  /* Specialised event type */
  GdkEventType event_type;

  /* The surface of the event */
  GdkSurface *surface;

  /* The devices associated to the event */
  GdkDevice *device;

  guint32 time;
  guint16 flags;
};

/*< private >
 * GdkEventClass:
 * @finalize: a function called when the last reference held on an event is
 *   released; implementations of GdkEvent must chain up to the parent class
 *
 * The base class for events.
 */
struct _GdkEventClass
{
  GTypeClass parent_class;

  void                  (* finalize)            (GdkEvent *event);

  GdkModifierType       (* get_state)           (GdkEvent *event);
  gboolean              (* get_position)        (GdkEvent *event,
                                                 double   *x,
                                                 double   *y);
  GdkEventSequence *    (* get_sequence)        (GdkEvent *event);
  GdkDeviceTool *       (* get_tool)            (GdkEvent *event);
  gboolean              (* get_axes)            (GdkEvent *event,
                                                 double  **axes,
                                                 guint    *n_axes);
};

/*
 * GdkDeleteEvent:
 *
 * Generated when a surface is deleted.
 */
struct _GdkDeleteEvent
{
  GdkEvent parent_instance;
};

/*
 * GdkMotionEvent:
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) set during the motion
 *   event. See [flags@Gdk.ModifierType]
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @axes: @x, @y translated to the axes of @device, or %NULL if @device is
 *   the mouse.
 * @history: (element-type GdkTimeCoord): a list of time and coordinates
 *   for other motion events that were compressed before delivering the
 *   current event
 *
 * Generated when the pointer moves.
 */
struct _GdkMotionEvent
{
  GdkEvent parent_instance;

  GdkModifierType state;
  double x;
  double y;
  double *axes;
  GdkDeviceTool *tool;
  GArray *history; /* <GdkTimeCoord> */
};

/*
 * GdkButtonEvent:
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See [flags@Gdk.ModifierType]
 * @button: the button which was pressed or released, numbered from 1 to 5.
 *   Normally button 1 is the left mouse button, 2 is the middle button,
 *   and 3 is the right button. On 2-button mice, the middle button can
 *   often be simulated by pressing both mouse buttons together.
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @axes: @x, @y translated to the axes of @device, or %NULL if @device is
 *   the mouse.
 * @tool: a `GdkDeviceTool`
 *
 * Used for button press and button release events. The
 * @type field will be one of %GDK_BUTTON_PRESS or %GDK_BUTTON_RELEASE,
 */
struct _GdkButtonEvent
{
  GdkEvent parent_instance;

  GdkModifierType state;
  guint button;
  double x;
  double y;
  double *axes;
  GdkDeviceTool *tool;
};

/*
 * GdkTouchEvent:
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See [flags@Gdk.ModifierType]
 * @x: the x coordinate of the pointer relative to the surface
 * @y: the y coordinate of the pointer relative to the surface
 * @axes: @x, @y translated to the axes of the event's device, or %NULL
 *   if @device is the mouse
 * @sequence: the event sequence that the event belongs to
 * @emulated: whether the event is the result of a pointer emulation
 *
 * Used for touch events.
 * @type field will be one of %GDK_TOUCH_BEGIN, %GDK_TOUCH_UPDATE,
 * %GDK_TOUCH_END or %GDK_TOUCH_CANCEL.
 *
 * Touch events are grouped into sequences by means of the @sequence
 * field, which can also be obtained with gdk_event_get_event_sequence().
 * Each sequence begins with a %GDK_TOUCH_BEGIN event, followed by
 * any number of %GDK_TOUCH_UPDATE events, and ends with a %GDK_TOUCH_END
 * (or %GDK_TOUCH_CANCEL) event. With multitouch devices, there may be
 * several active sequences at the same time.
 */
struct _GdkTouchEvent
{
  GdkEvent parent_instance;

  GdkModifierType state;
  double x;
  double y;
  double *axes;
  GdkEventSequence *sequence;
  gboolean touch_emulating;
  gboolean pointer_emulated;
};

/*
 * GdkScrollEvent:
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See [flags@Gdk.ModifierType]
 * @direction: the direction to scroll to (one of %GDK_SCROLL_UP,
 *   %GDK_SCROLL_DOWN, %GDK_SCROLL_LEFT, %GDK_SCROLL_RIGHT or
 *   %GDK_SCROLL_SMOOTH).
 * @delta_x: the x coordinate of the scroll delta
 * @delta_y: the y coordinate of the scroll delta
 * @tool: a `GdkDeviceTool`
 * @history: (element-type GdkTimeCoord): array of times and deltas
 *   for other scroll events that were compressed before delivering the
 *   current event
 * @unit: The scroll unit in which delta_x and delta_y are represented.
 *
 * Generated from button presses for the buttons 4 to 7. Wheel mice are
 * usually configured to generate button press events for buttons 4 and 5
 * when the wheel is turned.
 *
 * Some GDK backends can also generate “smooth” scroll events, which
 * can be recognized by the %GDK_SCROLL_SMOOTH scroll direction. For
 * these, the scroll deltas can be obtained with
 * gdk_event_get_scroll_deltas().
 */
struct _GdkScrollEvent
{
  GdkEvent parent_instance;

  GdkModifierType state;
  GdkScrollDirection direction;
  double delta_x;
  double delta_y;
  gboolean is_stop;
  GdkDeviceTool *tool;
  GArray *history; /* <GdkTimeCoord> */
  GdkScrollUnit unit;
};

/*
 * GdkTranslatedKey:
 * @keyval: the translated key symbol
 * @consumed: the consumed modifiers
 * @layout: the keyboard layout
 * @level: the layout level
 *
 * Describes a translated key code.
 */
typedef struct {
  guint keyval;
  GdkModifierType consumed;
  guint layout;
  guint level;
} GdkTranslatedKey;

/*
 * GdkKeyEvent:
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See [flags@Gdk.ModifierType]
 * @keycode: the raw code of the key that was pressed or released.
 * @translated: the result of translating @keycode. First with the full
 *   @state, then while ignoring Caps Lock.
 * @compose_sequence: optional string for use by selected IM modules.
 *   Contains either partial compose sequences or the final composed
 *   string of the keystroke sequence.
 *
 * Describes a key press or key release event.
 */
struct _GdkKeyEvent
{
  GdkEvent parent_instance;

  GdkModifierType state;
  guint32 keycode;
  gboolean key_is_modifier;
  GdkTranslatedKey translated[2];
  char *compose_sequence;
};

/*
 * GdkCrossingEvent:
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See [flags@Gdk.ModifierType]
 * @mode: the crossing mode (%GDK_CROSSING_NORMAL, %GDK_CROSSING_GRAB,
 *  %GDK_CROSSING_UNGRAB, %GDK_CROSSING_GTK_GRAB, %GDK_CROSSING_GTK_UNGRAB or
 *  %GDK_CROSSING_STATE_CHANGED).  %GDK_CROSSING_GTK_GRAB, %GDK_CROSSING_GTK_UNGRAB,
 *  and %GDK_CROSSING_STATE_CHANGED were added in 2.14 and are always synthesized,
 *  never native.
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @detail: the kind of crossing that happened (%GDK_NOTIFY_INFERIOR,
 *  %GDK_NOTIFY_ANCESTOR, %GDK_NOTIFY_VIRTUAL, %GDK_NOTIFY_NONLINEAR or
 *  %GDK_NOTIFY_NONLINEAR_VIRTUAL).
 * @focus: %TRUE if @surface is the focus surface or an inferior.
 * @child_surface: the surface that was entered or left.
 *
 * Generated when the pointer enters or leaves a surface.
 */
struct _GdkCrossingEvent
{
  GdkEvent parent_instance;

  GdkModifierType state;
  GdkCrossingMode mode;
  double x;
  double y;
  GdkNotifyType detail;
  gboolean focus;
  GdkSurface *child_surface;
};

/*
 * GdkFocusEvent:
 * @in: %TRUE if the surface has gained the keyboard focus, %FALSE if
 *   it has lost the focus.
 * @mode: the crossing mode
 * @detail: the kind of crossing that happened
 *
 * Describes a change of keyboard focus.
 */
struct _GdkFocusEvent
{
  GdkEvent parent_instance;

  gboolean focus_in;
};

/*
 * GdkProximityEvent:
 * @tool: the `GdkDeviceTool` associated to the event
 *
 * A proximity event indicates that a tool of a graphic tablet, or similar
 * devices that report proximity, has moved in or out of contact with the
 * tablet, or perhaps that the user’s finger has moved in or out of contact
 * with a touch screen.
 */
struct _GdkProximityEvent
{
  GdkEvent parent_instance;

  GdkDeviceTool *tool;
};

/*
 * GdkGrabBrokenEvent:
 * @keyboard: %TRUE if a keyboard grab was broken, %FALSE if a pointer
 *   grab was broken
 * @implicit: %TRUE if the broken grab was implicit
 * @grab_surface: If this event is caused by another grab in the same
 *   application, @grab_surface contains the new grab surface. Otherwise
 *   @grab_surface is %NULL.
 *
 * Generated when a pointer or keyboard grab is broken. On X11, this happens
 * when the grab surface becomes unviewable (i.e. it or one of its ancestors
 * is unmapped), or if the same application grabs the pointer or keyboard
 * again. Note that implicit grabs (which are initiated by button presses)
 * can also cause `GdkGrabBrokenEvent` events.
 */
struct _GdkGrabBrokenEvent
{
  GdkEvent parent_instance;

  gboolean keyboard;
  gboolean implicit;
  GdkSurface *grab_surface;
};

/*
 * GdkDNDEvent:
 * @drop: the `GdkDrop` for the current DND operation.
 * @x: the X coordinate of the pointer
 * @y: the Y coordinate of the pointer
 *
 * Generated during DND operations.
 */
struct _GdkDNDEvent
{
  GdkEvent parent_instance;

  GdkDrop *drop;
  double x;
  double y;
};

/*
 * GdkTouchpadEvent:
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See [flags@Gdk.ModifierType]
 * @phase: (type GdkTouchpadGesturePhase): the current phase of the gesture
 * @n_fingers: The number of fingers triggering the pinch
 * @time: the time of the event in milliseconds
 * @x: The X coordinate of the pointer
 * @y: The Y coordinate of the pointer
 * @dx: Movement delta in the X axis of the swipe focal point
 * @dy: Movement delta in the Y axis of the swipe focal point
 * @angle_delta: For pinch events, the angle change in radians, negative angles
 *   denote counter-clockwise movements
 * @scale: For pinch events, the current scale, relative to that at the time of
 *   the corresponding %GDK_TOUCHPAD_GESTURE_PHASE_BEGIN event
 *
 * Generated during touchpad gestures.
 */
struct _GdkTouchpadEvent
{
  GdkEvent parent_instance;

  GdkEventSequence *sequence;
  GdkModifierType state;
  gint8 phase;
  gint8 n_fingers;
  double x;
  double y;
  double dx;
  double dy;
  double angle_delta;
  double scale;
};

struct _GdkPadEvent
{
  GdkEvent parent_instance;

  guint group;
  guint mode;
  guint button;
  guint index;
  double value;
};

void gdk_event_init_types (void);

GdkEvent * gdk_button_event_new         (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDeviceTool   *tool,
                                         guint32          time,
                                         GdkModifierType  state,
                                         guint            button,
                                         double           x,
                                         double           y,
                                         double          *axes);

GdkEvent * gdk_motion_event_new         (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDeviceTool   *tool,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           x,
                                         double           y,
                                         double          *axes);

GdkEvent * gdk_crossing_event_new       (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           x,
                                         double           y,
                                         GdkCrossingMode  mode,
                                         GdkNotifyType    notify);

GdkEvent * gdk_proximity_event_new      (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDeviceTool   *tool,
                                         guint32          time);

GdkEvent * gdk_key_event_new            (GdkEventType      type,
                                         GdkSurface       *surface,
                                         GdkDevice        *device,
                                         guint32           time,
                                         guint             keycode,
                                         GdkModifierType   modifiers,
                                         gboolean          is_modifier,
                                         GdkTranslatedKey *translated,
                                         GdkTranslatedKey *no_lock,
                                         char             *compose_sequence);

GdkEvent * gdk_focus_event_new          (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         gboolean         focus_in);

GdkEvent * gdk_delete_event_new         (GdkSurface      *surface);

GdkEvent * gdk_scroll_event_new         (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDeviceTool   *tool,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           delta_x,
                                         double           delta_y,
                                         gboolean         is_stop,
                                         GdkScrollUnit    unit);

GdkEvent * gdk_scroll_event_new_discrete (GdkSurface         *surface,
                                          GdkDevice          *device,
                                          GdkDeviceTool      *tool,
                                          guint32             time,
                                          GdkModifierType     state,
                                          GdkScrollDirection  direction);

GdkEvent * gdk_scroll_event_new_value120 (GdkSurface         *surface,
                                          GdkDevice          *device,
                                          GdkDeviceTool      *tool,
                                          guint32             time,
                                          GdkModifierType     state,
                                          GdkScrollDirection  direction,
                                          double              delta_x,
                                          double              delta_y);

GdkEvent * gdk_touch_event_new          (GdkEventType      type,
                                         GdkEventSequence *sequence,
                                         GdkSurface       *surface,
                                         GdkDevice        *device,
                                         guint32           time,
                                         GdkModifierType   state,
                                         double            x,
                                         double            y,
                                         double           *axes,
                                         gboolean          emulating);

GdkEvent * gdk_touchpad_event_new_swipe (GdkSurface              *surface,
                                         GdkEventSequence        *sequence,
                                         GdkDevice               *device,
                                         guint32                  time,
                                         GdkModifierType          state,
                                         GdkTouchpadGesturePhase  phase,
                                         double                   x,
                                         double                   y,
                                         int                      n_fingers,
                                         double                   dx,
                                         double                   dy);

GdkEvent * gdk_touchpad_event_new_pinch (GdkSurface              *surface,
                                         GdkEventSequence        *sequence,
                                         GdkDevice               *device,
                                         guint32                  time,
                                         GdkModifierType          state,
                                         GdkTouchpadGesturePhase  phase,
                                         double                   x,
                                         double                   y,
                                         int                      n_fingers,
                                         double                   dx,
                                         double                   dy,
                                         double                   scale,
                                         double                   angle_delta);

GdkEvent * gdk_touchpad_event_new_hold  (GdkSurface              *surface,
                                         GdkEventSequence        *sequence,
                                         GdkDevice               *device,
                                         guint32                  time,
                                         GdkModifierType          state,
                                         GdkTouchpadGesturePhase  phase,
                                         double                   x,
                                         double                   y,
                                         int                      n_fingers);

GdkEvent * gdk_pad_event_new_ring       (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         guint32          time,
                                         guint            group,
                                         guint            index,
                                         guint            mode,
                                         double           value);

GdkEvent * gdk_pad_event_new_strip      (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         guint32          time,
                                         guint            group,
                                         guint            index,
                                         guint            mode,
                                         double           value);

GdkEvent * gdk_pad_event_new_button     (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         guint32          time,
                                         guint            group,
                                         guint            button,
                                         guint            mode);

GdkEvent * gdk_pad_event_new_group_mode (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         guint32          time,
                                         guint            group,
                                         guint            mode);

GdkEvent * gdk_dnd_event_new            (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDrop         *drop,
                                         guint32          time,
                                         double           x,
                                         double           y);

GdkEvent * gdk_grab_broken_event_new    (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkSurface      *grab_surface,
                                         gboolean         implicit);

GdkTranslatedKey *      gdk_key_event_get_translated_key        (GdkEvent *event,
                                                                 gboolean  no_lock);

char *                  gdk_key_event_get_compose_sequence      (GdkEvent *event);

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0,

  /* When we are ready to draw a frame, we pause event delivery,
   * mark all events in the queue with this flag, and deliver
   * only those events until we finish the frame.
   */
  GDK_EVENT_FLUSHED = 1 << 2
} GdkEventFlags;

GdkEvent* _gdk_event_unqueue (GdkDisplay *display);

gboolean _gdk_event_emit              (GdkEvent   *event);
GList*   _gdk_event_queue_find_first  (GdkDisplay *display);
void     _gdk_event_queue_remove_link (GdkDisplay *display,
                                       GList      *node);
GList*   _gdk_event_queue_append      (GdkDisplay *display,
                                       GdkEvent   *event);

void     _gdk_event_queue_handle_motion_compression (GdkDisplay *display);
void     gdk_event_queue_handle_scroll_compression  (GdkDisplay *display);
void     _gdk_event_queue_flush                     (GdkDisplay       *display);

double * gdk_event_dup_axes (GdkEvent *event);

G_END_DECLS

