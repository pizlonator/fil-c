/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */

/**
 * GtkGesture:
 *
 * `GtkGesture` is the base class for gesture recognition.
 *
 * Although `GtkGesture` is quite generalized to serve as a base for
 * multi-touch gestures, it is suitable to implement single-touch and
 * pointer-based gestures (using the special %NULL `GdkEventSequence`
 * value for these).
 *
 * The number of touches that a `GtkGesture` need to be recognized is
 * controlled by the [property@Gtk.Gesture:n-points] property, if a
 * gesture is keeping track of less or more than that number of sequences,
 * it won't check whether the gesture is recognized.
 *
 * As soon as the gesture has the expected number of touches, it will check
 * regularly if it is recognized, the criteria to consider a gesture as
 * "recognized" is left to `GtkGesture` subclasses.
 *
 * A recognized gesture will then emit the following signals:
 *
 * - [signal@Gtk.Gesture::begin] when the gesture is recognized.
 * - [signal@Gtk.Gesture::update], whenever an input event is processed.
 * - [signal@Gtk.Gesture::end] when the gesture is no longer recognized.
 *
 * ## Event propagation
 *
 * In order to receive events, a gesture needs to set a propagation phase
 * through [method@Gtk.EventController.set_propagation_phase].
 *
 * In the capture phase, events are propagated from the toplevel down
 * to the target widget, and gestures that are attached to containers
 * above the widget get a chance to interact with the event before it
 * reaches the target.
 *
 * In the bubble phase, events are propagated up from the target widget
 * to the toplevel, and gestures that are attached to containers above
 * the widget get a chance to interact with events that have not been
 * handled yet.
 *
 * ## States of a sequence
 *
 * Whenever input interaction happens, a single event may trigger a cascade
 * of `GtkGesture`s, both across the parents of the widget receiving the
 * event and in parallel within an individual widget. It is a responsibility
 * of the widgets using those gestures to set the state of touch sequences
 * accordingly in order to enable cooperation of gestures around the
 * `GdkEventSequence`s triggering those.
 *
 * Within a widget, gestures can be grouped through [method@Gtk.Gesture.group].
 * Grouped gestures synchronize the state of sequences, so calling
 * [method@Gtk.Gesture.set_state] on one will effectively propagate
 * the state throughout the group.
 *
 * By default, all sequences start out in the %GTK_EVENT_SEQUENCE_NONE state,
 * sequences in this state trigger the gesture event handler, but event
 * propagation will continue unstopped by gestures.
 *
 * If a sequence enters into the %GTK_EVENT_SEQUENCE_DENIED state, the gesture
 * group will effectively ignore the sequence, letting events go unstopped
 * through the gesture, but the "slot" will still remain occupied while
 * the touch is active.
 *
 * If a sequence enters in the %GTK_EVENT_SEQUENCE_CLAIMED state, the gesture
 * group will grab all interaction on the sequence, by:
 *
 * - Setting the same sequence to %GTK_EVENT_SEQUENCE_DENIED on every other
 *   gesture group within the widget, and every gesture on parent widgets
 *   in the propagation chain.
 * - Emitting [signal@Gtk.Gesture::cancel] on every gesture in widgets
 *   underneath in the propagation chain.
 * - Stopping event propagation after the gesture group handles the event.
 *
 * Note: if a sequence is set early to %GTK_EVENT_SEQUENCE_CLAIMED on
 * %GDK_TOUCH_BEGIN/%GDK_BUTTON_PRESS (so those events are captured before
 * reaching the event widget, this implies %GTK_PHASE_CAPTURE), one similar
 * event will be emulated if the sequence changes to %GTK_EVENT_SEQUENCE_DENIED.
 * This way event coherence is preserved before event propagation is unstopped
 * again.
 *
 * Sequence states can't be changed freely.
 * See [method@Gtk.Gesture.set_state] to know about the possible
 * lifetimes of a `GdkEventSequence`.
 *
 * ## Touchpad gestures
 *
 * On the platforms that support it, `GtkGesture` will handle transparently
 * touchpad gesture events. The only precautions users of `GtkGesture` should
 * do to enable this support are:
 *
 * - If the gesture has %GTK_PHASE_NONE, ensuring events of type
 *   %GDK_TOUCHPAD_SWIPE and %GDK_TOUCHPAD_PINCH are handled by the `GtkGesture`
 */

#include "config.h"
#include "gtkgesture.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkgestureprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtknative.h"

typedef struct _GtkGesturePrivate GtkGesturePrivate;
typedef struct _PointData PointData;

enum {
  PROP_N_POINTS = 1,
};

enum {
  BEGIN,
  END,
  UPDATE,
  CANCEL,
  SEQUENCE_STATE_CHANGED,
  N_SIGNALS
};

struct _PointData
{
  GdkEvent *event;
  GtkWidget *target;
  double widget_x;
  double widget_y;

  /* Acummulators for touchpad events */
  double accum_dx;
  double accum_dy;

  guint press_handled : 1;
  guint state : 2;
};

struct _GtkGesturePrivate
{
  GHashTable *points;
  GdkEventSequence *last_sequence;
  GdkDevice *device;
  GList *group_link;
  guint n_points;
  guint recognized : 1;
  guint touchpad : 1;
};

static guint signals[N_SIGNALS] = { 0 };

#define BUTTONS_MASK (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)

#define EVENT_IS_TOUCHPAD_GESTURE(e) (gdk_event_get_event_type (e) == GDK_TOUCHPAD_SWIPE || \
                                      gdk_event_get_event_type (e) == GDK_TOUCHPAD_PINCH || \
                                      gdk_event_get_event_type (e) == GDK_TOUCHPAD_HOLD)

GList * _gtk_gesture_get_group_link (GtkGesture *gesture);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkGesture, gtk_gesture, GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_gesture_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  switch (prop_id)
    {
    case PROP_N_POINTS:
      g_value_set_uint (value, priv->n_points);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  switch (prop_id)
    {
    case PROP_N_POINTS:
      priv->n_points = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_finalize (GObject *object)
{
  GtkGesture *gesture = GTK_GESTURE (object);
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);

  gtk_gesture_ungroup (gesture);
  g_list_free (priv->group_link);

  g_hash_table_destroy (priv->points);

  G_OBJECT_CLASS (gtk_gesture_parent_class)->finalize (object);
}

static guint
_gtk_gesture_get_n_touchpad_points (GtkGesture *gesture,
                                    gboolean    only_active)
{
  GtkGesturePrivate *priv;
  PointData *data;
  GdkEventType event_type;
  GdkTouchpadGesturePhase phase = 0;
  guint n_fingers = 0;

  priv = gtk_gesture_get_instance_private (gesture);

  if (!priv->touchpad)
    return 0;

  data = g_hash_table_lookup (priv->points, priv->last_sequence);

  if (!data)
    return 0;

  event_type = gdk_event_get_event_type (data->event);

  if (EVENT_IS_TOUCHPAD_GESTURE (data->event))
    {
      phase = gdk_touchpad_event_get_gesture_phase (data->event);
      n_fingers = gdk_touchpad_event_get_n_fingers (data->event);
    }

  if (only_active &&
      (data->state == GTK_EVENT_SEQUENCE_DENIED ||
       (event_type == GDK_TOUCHPAD_SWIPE && phase == GDK_TOUCHPAD_GESTURE_PHASE_END) ||
       (event_type == GDK_TOUCHPAD_PINCH && phase == GDK_TOUCHPAD_GESTURE_PHASE_END) ||
       (event_type == GDK_TOUCHPAD_HOLD && phase == GDK_TOUCHPAD_GESTURE_PHASE_END)))
    return 0;

  return n_fingers;
}

static guint
_gtk_gesture_get_n_touch_points (GtkGesture *gesture,
                                 gboolean    only_active)
{
  GtkGesturePrivate *priv;
  GHashTableIter iter;
  guint n_points = 0;
  PointData *data;
  GdkEventType event_type;

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      event_type = gdk_event_get_event_type (data->event);

      if (only_active &&
          (data->state == GTK_EVENT_SEQUENCE_DENIED ||
           event_type == GDK_TOUCH_END ||
           event_type == GDK_BUTTON_RELEASE))
        continue;

      n_points++;
    }

  return n_points;
}

static guint
_gtk_gesture_get_n_physical_points (GtkGesture *gesture,
                                    gboolean    only_active)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->touchpad)
    return _gtk_gesture_get_n_touchpad_points (gesture, only_active);
  else
    return _gtk_gesture_get_n_touch_points (gesture, only_active);
}

static gboolean
gtk_gesture_check_impl (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  guint n_points;

  priv = gtk_gesture_get_instance_private (gesture);
  n_points = _gtk_gesture_get_n_physical_points (gesture, TRUE);

  return n_points == priv->n_points;
}

static void
_gtk_gesture_set_recognized (GtkGesture       *gesture,
                             gboolean          recognized,
                             GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->recognized == recognized)
    return;

  priv->recognized = recognized;

  if (recognized)
    g_signal_emit (gesture, signals[BEGIN], 0, sequence);
  else
    g_signal_emit (gesture, signals[END], 0, sequence);
}

static gboolean
_gtk_gesture_do_check (GtkGesture *gesture)
{
  GtkGestureClass *gesture_class;
  gboolean retval = FALSE;

  gesture_class = GTK_GESTURE_GET_CLASS (gesture);

  if (!gesture_class->check)
    return retval;

  retval = gesture_class->check (gesture);
  return retval;
}

static gboolean
_gtk_gesture_has_matching_touchpoints (GtkGesture *gesture)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);
  guint active_n_points, current_n_points;

  current_n_points = _gtk_gesture_get_n_physical_points (gesture, FALSE);
  active_n_points = _gtk_gesture_get_n_physical_points (gesture, TRUE);

  return (active_n_points == priv->n_points &&
          current_n_points == priv->n_points);
}

static gboolean
_gtk_gesture_check_recognized (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);
  gboolean has_matching_touchpoints;

  has_matching_touchpoints = _gtk_gesture_has_matching_touchpoints (gesture);

  if (priv->recognized && !has_matching_touchpoints)
    _gtk_gesture_set_recognized (gesture, FALSE, sequence);
  else if (!priv->recognized && has_matching_touchpoints &&
           _gtk_gesture_do_check (gesture))
    _gtk_gesture_set_recognized (gesture, TRUE, sequence);

  return priv->recognized;
}

static void
_update_touchpad_deltas (PointData *data)
{
  GdkEvent *event = data->event;
  GdkTouchpadGesturePhase phase;
  double dx = 0;
  double dy = 0;

  if (!event)
    return;

  if (EVENT_IS_TOUCHPAD_GESTURE (event))
    {
      phase = gdk_touchpad_event_get_gesture_phase (event);

      if (gdk_event_get_event_type (event) != GDK_TOUCHPAD_HOLD)
        gdk_touchpad_event_get_deltas (event, &dx, &dy);

      if (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)
        data->accum_dx = data->accum_dy = 0;
      else if (phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE)
        {
          data->accum_dx += dx;
          data->accum_dy += dy;
        }
    }
}

static GtkEventSequenceState
gtk_gesture_get_group_state (GtkGesture       *gesture,
                             GdkEventSequence *sequence)
{
  GtkEventSequenceState state = GTK_EVENT_SEQUENCE_NONE;
  GList *group_elem;

  group_elem = g_list_first (_gtk_gesture_get_group_link (gesture));

  for (; group_elem; group_elem = group_elem->next)
    {
      if (group_elem->data == gesture)
        continue;
      if (!gtk_gesture_handles_sequence (group_elem->data, sequence))
        continue;

      state = gtk_gesture_get_sequence_state (group_elem->data, sequence);
      break;
    }

  return state;
}

static gboolean
_gtk_gesture_update_point (GtkGesture     *gesture,
                           GdkEvent       *event,
                           GtkWidget      *target,
                           double          x,
                           double          y,
                           gboolean        add)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *device;
  gboolean existed, touchpad;
  PointData *data;

  device = gdk_event_get_device (event);

  if (!device)
    return FALSE;

  priv = gtk_gesture_get_instance_private (gesture);
  touchpad = EVENT_IS_TOUCHPAD_GESTURE (event);

  if (add)
    {
      /* If the event happens with the wrong device, or
       * on the wrong window, ignore.
       */
      if (priv->device && priv->device != device)
        return FALSE;

      /* Make touchpad and touchscreen gestures mutually exclusive */
      if (touchpad && g_hash_table_size (priv->points) > 0)
        return FALSE;
      else if (!touchpad && priv->touchpad)
        return FALSE;
    }
  else if (!priv->device)
    return FALSE;

  sequence = gdk_event_get_event_sequence (event);
  existed = g_hash_table_lookup_extended (priv->points, sequence,
                                          NULL, (gpointer *) &data);
  if (!existed)
    {
      if (!add)
        return FALSE;

      if (g_hash_table_size (priv->points) == 0)
        {
          priv->device = device;
          priv->touchpad = touchpad;
        }

      data = g_new0 (PointData, 1);
      g_hash_table_insert (priv->points, sequence, data);
    }

  if (data->event)
    gdk_event_unref (data->event);

  data->event = gdk_event_ref ((GdkEvent *)event);
  g_set_object (&data->target, target);
  _update_touchpad_deltas (data);
  data->widget_x = x + data->accum_dx;
  data->widget_y = y + data->accum_dy;

  if (!existed)
    {
      GtkEventSequenceState state;

      /* Deny the sequence right away if the expected
       * number of points is exceeded, so this sequence
       * can be tracked with gtk_gesture_handles_sequence().
       *
       * Otherwise, make the sequence inherit the same state
       * from other gestures in the same group.
       */
      if (_gtk_gesture_get_n_physical_points (gesture, FALSE) > priv->n_points)
        state = GTK_EVENT_SEQUENCE_DENIED;
      else
        state = gtk_gesture_get_group_state (gesture, sequence);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_gesture_set_sequence_state (gesture, sequence, state);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  return TRUE;
}

static void
_gtk_gesture_check_empty (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (g_hash_table_size (priv->points) == 0)
    {
      priv->device = NULL;
      priv->touchpad = FALSE;
    }
}

static void
_gtk_gesture_remove_point (GtkGesture     *gesture,
                           GdkEvent       *event)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *device;

  sequence = gdk_event_get_event_sequence (event);
  device = gdk_event_get_device (event);
  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->device != device)
    return;

  g_hash_table_remove (priv->points, sequence);
  _gtk_gesture_check_empty (gesture);
}

static void
_gtk_gesture_cancel_all (GtkGesture *gesture)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GHashTableIter iter;

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer*) &sequence, NULL))
    {
      g_signal_emit (gesture, signals[CANCEL], 0, sequence);
      g_hash_table_iter_remove (&iter);
      _gtk_gesture_check_recognized (gesture, sequence);
    }

  _gtk_gesture_check_empty (gesture);
}

static gboolean
gesture_within_surface (GtkGesture *gesture,
                        GdkSurface  *surface)
{
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  return surface == gtk_native_get_surface (gtk_widget_get_native (widget));
}

static gboolean
gtk_gesture_filter_event (GtkEventController *controller,
                          GdkEvent           *event)
{
  /* Even though GtkGesture handles these events, we want
   * touchpad gestures disabled by default, it will be
   * subclasses which punch the holes in for the events
   * they can possibly handle.
   */
  if (EVENT_IS_TOUCHPAD_GESTURE (event))
    return FALSE;

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_parent_class)->filter_event (controller, event);
}

static gboolean
gtk_gesture_handle_event (GtkEventController *controller,
                          GdkEvent           *event,
                          double              x,
                          double              y)
{
  GtkGesture *gesture = GTK_GESTURE (controller);
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *source_device;
  gboolean was_recognized;
  GdkEventType event_type;
  GdkTouchpadGesturePhase phase = 0;
  GdkModifierType state;
  GtkWidget *target;

  source_device = gdk_event_get_device (event);

  if (!source_device)
    return FALSE;

  priv = gtk_gesture_get_instance_private (gesture);
  sequence = gdk_event_get_event_sequence (event);
  was_recognized = gtk_gesture_is_recognized (gesture);
  event_type = gdk_event_get_event_type (event);
  state = gdk_event_get_modifier_state (event);
  if (EVENT_IS_TOUCHPAD_GESTURE (event))
    phase = gdk_touchpad_event_get_gesture_phase (event);

  target = gtk_event_controller_get_target (controller);

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_DENIED)
    priv->last_sequence = sequence;

  if (event_type == GDK_BUTTON_PRESS ||
      event_type == GDK_TOUCH_BEGIN ||
      (EVENT_IS_TOUCHPAD_GESTURE (event) &&
       phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN &&
       gdk_touchpad_event_get_n_fingers (event) == priv->n_points))
    {
      if (_gtk_gesture_update_point (gesture, event, target, x, y, TRUE))
        {
          gboolean triggered_recognition;

          triggered_recognition =
            !was_recognized && _gtk_gesture_has_matching_touchpoints (gesture);

          if (_gtk_gesture_check_recognized (gesture, sequence))
            {
              PointData *data;

              data = g_hash_table_lookup (priv->points, sequence);

              /* If the sequence was claimed early, the press event will be consumed */
              if (gtk_gesture_get_sequence_state (gesture, sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
                data->press_handled = TRUE;
            }
          else if (triggered_recognition && g_hash_table_size (priv->points) == 0)
            {
              /* Recognition was triggered, but the gesture reset during
               * ::begin emission. Still, recognition was strictly triggered,
               * so the event should be consumed.
               */
              return TRUE;
            }
        }
    }
  else if (event_type == GDK_BUTTON_RELEASE ||
           event_type == GDK_TOUCH_END ||
           (EVENT_IS_TOUCHPAD_GESTURE (event) &&
            phase == GDK_TOUCHPAD_GESTURE_PHASE_END &&
            gdk_touchpad_event_get_n_fingers (event) == priv->n_points))
    {
      gboolean was_claimed = FALSE;

      if (_gtk_gesture_update_point (gesture, event, target, x, y, FALSE))
        {
          if (was_recognized &&
              _gtk_gesture_check_recognized (gesture, sequence))
            g_signal_emit (gesture, signals[UPDATE], 0, sequence);

          was_claimed =
            gtk_gesture_get_sequence_state (gesture, sequence) == GTK_EVENT_SEQUENCE_CLAIMED;

          _gtk_gesture_remove_point (gesture, event);
        }

      return was_claimed && was_recognized;
    }
  else if (event_type == GDK_MOTION_NOTIFY ||
           event_type == GDK_TOUCH_UPDATE ||
           (EVENT_IS_TOUCHPAD_GESTURE (event) &&
            phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE &&
            gdk_touchpad_event_get_n_fingers (event) == priv->n_points))
    {
      if (event_type == GDK_MOTION_NOTIFY)
        {
          if ((state & BUTTONS_MASK) == 0)
            return FALSE;
        }

      if (_gtk_gesture_update_point (gesture, event, target, x, y, FALSE) &&
          _gtk_gesture_check_recognized (gesture, sequence))
        g_signal_emit (gesture, signals[UPDATE], 0, sequence);
    }
  else if (event_type == GDK_TOUCH_CANCEL)
    {
      if (!priv->touchpad)
        _gtk_gesture_cancel_sequence (gesture, sequence);
    }
  else if (EVENT_IS_TOUCHPAD_GESTURE (event) &&
           phase == GDK_TOUCHPAD_GESTURE_PHASE_CANCEL &&
           gdk_touchpad_event_get_n_fingers (event) == priv->n_points)
    {
      if (priv->touchpad)
        _gtk_gesture_cancel_sequence (gesture, sequence);
    }
  else if (event_type == GDK_GRAB_BROKEN)
    {
      GdkSurface *surface;

      surface = gdk_grab_broken_event_get_grab_surface (event);
      if (!surface || !gesture_within_surface (gesture, surface))
        _gtk_gesture_cancel_all (gesture);

      return FALSE;
    }
  else
    {
      /* Unhandled event */
      return FALSE;
    }

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_CLAIMED)
    return FALSE;

  return priv->recognized;
}

static void
gtk_gesture_reset (GtkEventController *controller)
{
  _gtk_gesture_cancel_all (GTK_GESTURE (controller));
}

static void
gtk_gesture_class_init (GtkGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->get_property = gtk_gesture_get_property;
  object_class->set_property = gtk_gesture_set_property;
  object_class->finalize = gtk_gesture_finalize;

  controller_class->filter_event = gtk_gesture_filter_event;
  controller_class->handle_event = gtk_gesture_handle_event;
  controller_class->reset = gtk_gesture_reset;

  klass->check = gtk_gesture_check_impl;

  /**
   * GtkGesture:n-points:
   *
   * The number of touch points that trigger
   * recognition on this gesture.
   */
  g_object_class_install_property (object_class,
                                   PROP_N_POINTS,
                                   g_param_spec_uint ("n-points", NULL, NULL,
                                                      1, G_MAXUINT, 1,
                                                      GTK_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY));
  /**
   * GtkGesture::begin:
   * @gesture: the object which received the signal
   * @sequence: (nullable): the `GdkEventSequence` that made the gesture
   *   to be recognized
   *
   * Emitted when the gesture is recognized.
   *
   * This means the number of touch sequences matches
   * [property@Gtk.Gesture:n-points].
   *
   * Note: These conditions may also happen when an extra touch
   * (eg. a third touch on a 2-touches gesture) is lifted, in that
   * situation @sequence won't pertain to the current set of active
   * touches, so don't rely on this being true.
   */
  signals[BEGIN] =
    g_signal_new (I_("begin"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, begin),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);

  /**
   * GtkGesture::end:
   * @gesture: the object which received the signal
   * @sequence: (nullable): the `GdkEventSequence` that made gesture
   *   recognition to finish
   *
   * Emitted when @gesture either stopped recognizing the event
   * sequences as something to be handled, or the number of touch
   * sequences became higher or lower than [property@Gtk.Gesture:n-points].
   *
   * Note: @sequence might not pertain to the group of sequences that
   * were previously triggering recognition on @gesture (ie. a just
   * pressed touch sequence that exceeds [property@Gtk.Gesture:n-points]).
   * This situation may be detected by checking through
   * [method@Gtk.Gesture.handles_sequence].
   */
  signals[END] =
    g_signal_new (I_("end"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, end),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);

  /**
   * GtkGesture::update:
   * @gesture: the object which received the signal
   * @sequence: (nullable): the `GdkEventSequence` that was updated
   *
   * Emitted whenever an event is handled while the gesture is recognized.
   *
   * @sequence is guaranteed to pertain to the set of active touches.
   */
  signals[UPDATE] =
    g_signal_new (I_("update"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, update),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);

  /**
   * GtkGesture::cancel:
   * @gesture: the object which received the signal
   * @sequence: (nullable): the `GdkEventSequence` that was cancelled
   *
   * Emitted whenever a sequence is cancelled.
   *
   * This usually happens on active touches when
   * [method@Gtk.EventController.reset] is called on @gesture
   * (manually, due to grabs...), or the individual @sequence
   * was claimed by parent widgets' controllers (see
   * [method@Gtk.Gesture.set_sequence_state]).
   *
   * @gesture must forget everything about @sequence as in
   * response to this signal.
   */
  signals[CANCEL] =
    g_signal_new (I_("cancel"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, cancel),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);

  /**
   * GtkGesture::sequence-state-changed:
   * @gesture: the object which received the signal
   * @sequence: (nullable): the `GdkEventSequence` that was cancelled
   * @state: the new sequence state
   *
   * Emitted whenever a sequence state changes.
   *
   * See [method@Gtk.Gesture.set_sequence_state] to know
   * more about the expectable sequence lifetimes.
   */
  signals[SEQUENCE_STATE_CHANGED] =
    g_signal_new (I_("sequence-state-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, sequence_state_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_ENUM,
                  G_TYPE_NONE, 2, GDK_TYPE_EVENT_SEQUENCE,
                  GTK_TYPE_EVENT_SEQUENCE_STATE);
  g_signal_set_va_marshaller (signals[SEQUENCE_STATE_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXED_ENUMv);
}

static void
free_point_data (gpointer data)
{
  PointData *point = data;

  if (point->event)
    gdk_event_unref (point->event);

  if (point->target)
    g_object_unref (point->target);

  g_free (point);
}

static void
gtk_gesture_init (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);
  priv->points = g_hash_table_new_full (NULL, NULL, NULL,
                                        (GDestroyNotify) free_point_data);
  priv->group_link = g_list_prepend (NULL, gesture);
}

/**
 * gtk_gesture_get_device:
 * @gesture: a `GtkGesture`
 *
 * Returns the logical `GdkDevice` that is currently operating
 * on @gesture.
 *
 * This returns %NULL if the gesture is not being interacted.
 *
 * Returns: (nullable) (transfer none): a `GdkDevice`
 */
GdkDevice *
gtk_gesture_get_device (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->device;
}

/**
 * gtk_gesture_get_sequence_state:
 * @gesture: a `GtkGesture`
 * @sequence: a `GdkEventSequence`
 *
 * Returns the @sequence state, as seen by @gesture.
 *
 * Returns: The sequence state in @gesture
 */
GtkEventSequenceState
gtk_gesture_get_sequence_state (GtkGesture       *gesture,
                                GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture),
                        GTK_EVENT_SEQUENCE_NONE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return GTK_EVENT_SEQUENCE_NONE;

  return data->state;
}

/**
 * gtk_gesture_set_sequence_state:
 * @gesture: a `GtkGesture`
 * @sequence: a `GdkEventSequence`
 * @state: the sequence state
 *
 * Sets the state of @sequence in @gesture.
 *
 * Sequences start in state %GTK_EVENT_SEQUENCE_NONE, and whenever
 * they change state, they can never go back to that state. Likewise,
 * sequences in state %GTK_EVENT_SEQUENCE_DENIED cannot turn back to
 * a not denied state. With these rules, the lifetime of an event
 * sequence is constrained to the next four:
 *
 * * None
 * * None → Denied
 * * None → Claimed
 * * None → Claimed → Denied
 *
 * Note: Due to event handling ordering, it may be unsafe to set the
 * state on another gesture within a [signal@Gtk.Gesture::begin] signal
 * handler, as the callback might be executed before the other gesture
 * knows about the sequence. A safe way to perform this could be:
 *
 * ```c
 * static void
 * first_gesture_begin_cb (GtkGesture       *first_gesture,
 *                         GdkEventSequence *sequence,
 *                         gpointer          user_data)
 * {
 *   gtk_gesture_set_sequence_state (first_gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);
 *   gtk_gesture_set_sequence_state (second_gesture, sequence, GTK_EVENT_SEQUENCE_DENIED);
 * }
 *
 * static void
 * second_gesture_begin_cb (GtkGesture       *second_gesture,
 *                          GdkEventSequence *sequence,
 *                          gpointer          user_data)
 * {
 *   if (gtk_gesture_get_sequence_state (first_gesture, sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
 *     gtk_gesture_set_sequence_state (second_gesture, sequence, GTK_EVENT_SEQUENCE_DENIED);
 * }
 * ```
 *
 * If both gestures are in the same group, just set the state on
 * the gesture emitting the event, the sequence will be already
 * be initialized to the group's global state when the second
 * gesture processes the event.
 *
 * Returns: %TRUE if @sequence is handled by @gesture,
 *   and the state is changed successfully
 *
 * Deprecated: 4.10. Use [method@Gtk.Gesture.set_state]
 */
gboolean
gtk_gesture_set_sequence_state (GtkGesture            *gesture,
                                GdkEventSequence      *sequence,
                                GtkEventSequenceState  state)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (state >= GTK_EVENT_SEQUENCE_NONE &&
                        state <= GTK_EVENT_SEQUENCE_DENIED, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  if (data->state == state)
    return FALSE;

  /* denied sequences remain denied */
  if (data->state == GTK_EVENT_SEQUENCE_DENIED)
    return FALSE;

  /* Sequences can't go from claimed/denied to none */
  if (state == GTK_EVENT_SEQUENCE_NONE &&
      data->state != GTK_EVENT_SEQUENCE_NONE)
    return FALSE;

  data->state = state;

  gtk_widget_cancel_event_sequence (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)),
                                    gesture, sequence, state);
  g_signal_emit (gesture, signals[SEQUENCE_STATE_CHANGED], 0,
                 sequence, state);

  if (state == GTK_EVENT_SEQUENCE_DENIED)
    _gtk_gesture_check_recognized (gesture, sequence);

  return TRUE;
}

/**
 * gtk_gesture_set_state:
 * @gesture: a `GtkGesture`
 * @state: the sequence state
 *
 * Sets the state of all sequences that @gesture is currently
 * interacting with.
 *
 * Sequences start in state %GTK_EVENT_SEQUENCE_NONE, and whenever
 * they change state, they can never go back to that state. Likewise,
 * sequences in state %GTK_EVENT_SEQUENCE_DENIED cannot turn back to
 * a not denied state. With these rules, the lifetime of an event
 * sequence is constrained to the next four:
 *
 * * None
 * * None → Denied
 * * None → Claimed
 * * None → Claimed → Denied
 *
 * Note: Due to event handling ordering, it may be unsafe to set the
 * state on another gesture within a [signal@Gtk.Gesture::begin] signal
 * handler, as the callback might be executed before the other gesture
 * knows about the sequence. A safe way to perform this could be:
 *
 * ```c
 * static void
 * first_gesture_begin_cb (GtkGesture       *first_gesture,
 *                         GdkEventSequence *sequence,
 *                         gpointer          user_data)
 * {
 *   gtk_gesture_set_state (first_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
 *   gtk_gesture_set_state (second_gesture, GTK_EVENT_SEQUENCE_DENIED);
 * }
 *
 * static void
 * second_gesture_begin_cb (GtkGesture       *second_gesture,
 *                          GdkEventSequence *sequence,
 *                          gpointer          user_data)
 * {
 *   if (gtk_gesture_get_sequence_state (first_gesture, sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
 *     gtk_gesture_set_state (second_gesture, GTK_EVENT_SEQUENCE_DENIED);
 * }
 * ```
 *
 * If both gestures are in the same group, just set the state on
 * the gesture emitting the event, the sequence will be already
 * be initialized to the group's global state when the second
 * gesture processes the event.
 *
 * Returns: %TRUE if the state of at least one sequence
 *   was changed successfully
 */
gboolean
gtk_gesture_set_state (GtkGesture            *gesture,
                       GtkEventSequenceState  state)
{
  gboolean handled = FALSE;
  GtkGesturePrivate *priv;
  GList *sequences, *l;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (state >= GTK_EVENT_SEQUENCE_NONE &&
                        state <= GTK_EVENT_SEQUENCE_DENIED, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  sequences = g_hash_table_get_keys (priv->points);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  for (l = sequences; l; l = l->next)
    handled |= gtk_gesture_set_sequence_state (gesture, l->data, state);
G_GNUC_END_IGNORE_DEPRECATIONS

  g_list_free (sequences);

  return handled;
}

/**
 * gtk_gesture_get_sequences:
 * @gesture: a `GtkGesture`
 *
 * Returns the list of `GdkEventSequences` currently being interpreted
 * by @gesture.
 *
 * Returns: (transfer container) (element-type GdkEventSequence): A list
 *   of `GdkEventSequence`, the list elements are owned by GTK and must
 *   not be freed or modified, the list itself must be deleted
 *   through g_list_free()
 */
GList *
gtk_gesture_get_sequences (GtkGesture *gesture)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GList *sequences = NULL;
  GHashTableIter iter;
  PointData *data;
  GdkEventType event_type;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer *) &sequence, (gpointer *) &data))
    {
      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;

      event_type = gdk_event_get_event_type (data->event);

      if (event_type == GDK_TOUCH_END ||
          event_type == GDK_BUTTON_RELEASE)
        continue;

      sequences = g_list_prepend (sequences, sequence);
    }

  return sequences;
}

/**
 * gtk_gesture_get_last_updated_sequence:
 * @gesture: a `GtkGesture`
 *
 * Returns the `GdkEventSequence` that was last updated on @gesture.
 *
 * Returns: (transfer none) (nullable): The last updated sequence
 */
GdkEventSequence *
gtk_gesture_get_last_updated_sequence (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->last_sequence;
}

/**
 * gtk_gesture_get_last_event:
 * @gesture: a `GtkGesture`
 * @sequence: (nullable): a `GdkEventSequence`
 *
 * Returns the last event that was processed for @sequence.
 *
 * Note that the returned pointer is only valid as long as the
 * @sequence is still interpreted by the @gesture. If in doubt,
 * you should make a copy of the event.
 *
 * Returns: (transfer none) (nullable): The last event from @sequence
 */
GdkEvent *
gtk_gesture_get_last_event (GtkGesture       *gesture,
                            GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return NULL;

  return data->event;
}

/*
 * gtk_gesture_get_last_target:
 * @gesture: a `GtkGesture`
 * @sequence: event sequence
 *
 * Returns the widget that the last event was targeted at.
 *
 * See [method@Gtk.Gesture.get_last_event].
 *
 * Returns: (transfer none) (nullable): The target of the last event
 */
GtkWidget *
gtk_gesture_get_last_target (GtkGesture        *gesture,
                             GdkEventSequence  *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return NULL;

  return data->target;
}

/**
 * gtk_gesture_get_point:
 * @gesture: a `GtkGesture`
 * @sequence: (nullable): a `GdkEventSequence`, or %NULL for pointer events
 * @x: (out) (optional): return location for X axis of the sequence coordinates
 * @y: (out) (optional): return location for Y axis of the sequence coordinates
 *
 * If @sequence is currently being interpreted by @gesture,
 * returns %TRUE and fills in @x and @y with the last coordinates
 * stored for that event sequence.
 *
 * The coordinates are always relative to the widget allocation.
 *
 * Returns: %TRUE if @sequence is currently interpreted
 */
gboolean
gtk_gesture_get_point (GtkGesture       *gesture,
                       GdkEventSequence *sequence,
                       double           *x,
                       double           *y)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return FALSE;

  if (x)
    *x = data->widget_x;
  if (y)
    *y = data->widget_y;

  return TRUE;
}

gboolean
_gtk_gesture_get_last_update_time (GtkGesture       *gesture,
                                   GdkEventSequence *sequence,
                                   guint32          *evtime)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return FALSE;

  if (evtime)
    *evtime = gdk_event_get_time (data->event);

  return TRUE;
};

/**
 * gtk_gesture_get_bounding_box:
 * @gesture: a `GtkGesture`
 * @rect: (out): bounding box containing all active touches.
 *
 * If there are touch sequences being currently handled by @gesture,
 * returns %TRUE and fills in @rect with the bounding box containing
 * all active touches.
 *
 * Otherwise, %FALSE will be returned.
 *
 * Note: This function will yield unexpected results on touchpad
 * gestures. Since there is no correlation between physical and
 * pixel distances, these will look as if constrained in an
 * infinitely small area, @rect width and height will thus be 0
 * regardless of the number of touchpoints.
 *
 * Returns: %TRUE if there are active touches, %FALSE otherwise
 */
gboolean
gtk_gesture_get_bounding_box (GtkGesture   *gesture,
                              GdkRectangle *rect)
{
  GtkGesturePrivate *priv;
  double x1, y1, x2, y2;
  GHashTableIter iter;
  guint n_points = 0;
  PointData *data;
  GdkEventType event_type;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  x1 = y1 = G_MAXDOUBLE;
  x2 = y2 = -G_MAXDOUBLE;

  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;

      event_type = gdk_event_get_event_type (data->event);

      if (event_type == GDK_TOUCH_END ||
          event_type == GDK_BUTTON_RELEASE)
        continue;

      n_points++;
      x1 = MIN (x1, data->widget_x);
      y1 = MIN (y1, data->widget_y);
      x2 = MAX (x2, data->widget_x);
      y2 = MAX (y2, data->widget_y);
    }

  if (n_points == 0)
    return FALSE;

  rect->x = x1;
  rect->y = y1;
  rect->width = x2 - x1;
  rect->height = y2 - y1;

  return TRUE;
}


/**
 * gtk_gesture_get_bounding_box_center:
 * @gesture: a `GtkGesture`
 * @x: (out): X coordinate for the bounding box center
 * @y: (out): Y coordinate for the bounding box center
 *
 * If there are touch sequences being currently handled by @gesture,
 * returns %TRUE and fills in @x and @y with the center of the bounding
 * box containing all active touches.
 *
 * Otherwise, %FALSE will be returned.
 *
 * Returns: %FALSE if no active touches are present, %TRUE otherwise
 */
gboolean
gtk_gesture_get_bounding_box_center (GtkGesture *gesture,
                                     double     *x,
                                     double     *y)
{
  GdkEvent *last_event;
  GdkRectangle rect;
  GdkEventSequence *sequence;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (x != NULL && y != NULL, FALSE);

  sequence = gtk_gesture_get_last_updated_sequence (gesture);
  last_event = gtk_gesture_get_last_event (gesture, sequence);

  if (EVENT_IS_TOUCHPAD_GESTURE (last_event))
    return gtk_gesture_get_point (gesture, sequence, x, y);
  else if (!gtk_gesture_get_bounding_box (gesture, &rect))
    return FALSE;

  *x = rect.x + rect.width / 2;
  *y = rect.y + rect.height / 2;
  return TRUE;
}

/**
 * gtk_gesture_is_active:
 * @gesture: a `GtkGesture`
 *
 * Returns %TRUE if the gesture is currently active.
 *
 * A gesture is active while there are touch sequences
 * interacting with it.
 *
 * Returns: %TRUE if gesture is active
 */
gboolean
gtk_gesture_is_active (GtkGesture *gesture)
{
  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  return _gtk_gesture_get_n_physical_points (gesture, TRUE) != 0;
}

/**
 * gtk_gesture_is_recognized:
 * @gesture: a `GtkGesture`
 *
 * Returns %TRUE if the gesture is currently recognized.
 *
 * A gesture is recognized if there are as many interacting
 * touch sequences as required by @gesture.
 *
 * Returns: %TRUE if gesture is recognized
 */
gboolean
gtk_gesture_is_recognized (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->recognized;
}

gboolean
_gtk_gesture_check (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  return _gtk_gesture_check_recognized (gesture, priv->last_sequence);
}

/**
 * gtk_gesture_handles_sequence:
 * @gesture: a `GtkGesture`
 * @sequence: (nullable): a `GdkEventSequence`
 *
 * Returns %TRUE if @gesture is currently handling events
 * corresponding to @sequence.
 *
 * Returns: %TRUE if @gesture is handling @sequence, %FALSE otherwise
 */
gboolean
gtk_gesture_handles_sequence (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  if (data->state == GTK_EVENT_SEQUENCE_DENIED)
    return FALSE;

  return TRUE;
}

gboolean
_gtk_gesture_cancel_sequence (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  g_signal_emit (gesture, signals[CANCEL], 0, sequence);
  _gtk_gesture_remove_point (gesture, data->event);
  _gtk_gesture_check_recognized (gesture, sequence);

  return TRUE;
}

GList *
_gtk_gesture_get_group_link (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->group_link;
}

/**
 * gtk_gesture_group:
 * @gesture: a `GtkGesture`
 * @group_gesture: `GtkGesture` to group @gesture with
 *
 * Adds @gesture to the same group than @group_gesture.
 *
 * Gestures are by default isolated in their own groups.
 *
 * Both gestures must have been added to the same widget before
 * they can be grouped.
 *
 * When gestures are grouped, the state of `GdkEventSequences`
 * is kept in sync for all of those, so calling
 * [method@Gtk.Gesture.set_sequence_state], on one will transfer
 * the same value to the others.
 *
 * Groups also perform an "implicit grabbing" of sequences, if a
 * `GdkEventSequence` state is set to %GTK_EVENT_SEQUENCE_CLAIMED
 * on one group, every other gesture group attached to the same
 * `GtkWidget` will switch the state for that sequence to
 * %GTK_EVENT_SEQUENCE_DENIED.
 */
void
gtk_gesture_group (GtkGesture *gesture,
                   GtkGesture *group_gesture)
{
  GList *link, *group_link, *next;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (GTK_IS_GESTURE (group_gesture));
  g_return_if_fail (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (group_gesture)) ==
                    gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  link = _gtk_gesture_get_group_link (gesture);

  if (link->prev || link->next)
    {
      if (gtk_gesture_is_grouped_with (gesture, group_gesture))
        return;
      gtk_gesture_ungroup (gesture);
    }

  group_link = _gtk_gesture_get_group_link (group_gesture);
  next = group_link->next;

  /* Rewire link so it's inserted after the group_gesture elem */
  link->prev = group_link;
  link->next = next;
  group_link->next = link;
  if (next)
    next->prev = link;
}

/**
 * gtk_gesture_ungroup:
 * @gesture: a `GtkGesture`
 *
 * Separates @gesture into an isolated group.
 */
void
gtk_gesture_ungroup (GtkGesture *gesture)
{
  GList *link, *prev, *next;

  g_return_if_fail (GTK_IS_GESTURE (gesture));

  link = _gtk_gesture_get_group_link (gesture);
  prev = link->prev;
  next = link->next;

  /* Detach link from the group chain */
  if (prev)
    prev->next = next;
  if (next)
    next->prev = prev;

  link->next = link->prev = NULL;
}

/**
 * gtk_gesture_get_group:
 * @gesture: a `GtkGesture`
 *
 * Returns all gestures in the group of @gesture
 *
 * Returns: (element-type GtkGesture) (transfer container): The list
 *   of `GtkGesture`s, free with g_list_free()
 */
GList *
gtk_gesture_get_group (GtkGesture *gesture)
{
  GList *link;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  link = _gtk_gesture_get_group_link (gesture);

  return g_list_copy (g_list_first (link));
}

/**
 * gtk_gesture_is_grouped_with:
 * @gesture: a `GtkGesture`
 * @other: another `GtkGesture`
 *
 * Returns %TRUE if both gestures pertain to the same group.
 *
 * Returns: whether the gestures are grouped
 */
gboolean
gtk_gesture_is_grouped_with (GtkGesture *gesture,
                             GtkGesture *other)
{
  GList *link;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (GTK_IS_GESTURE (other), FALSE);

  link = _gtk_gesture_get_group_link (gesture);
  link = g_list_first (link);

  return g_list_find (link, other) != NULL;
}

gboolean
_gtk_gesture_handled_sequence_press (GtkGesture       *gesture,
                                     GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  return data->press_handled;
}

gboolean
_gtk_gesture_get_pointer_emulating_sequence (GtkGesture        *gesture,
                                             GdkEventSequence **sequence)
{
  GtkGesturePrivate *priv;
  GdkEventSequence *seq;
  GHashTableIter iter;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer*) &seq, (gpointer*) &data))
    {
      switch ((guint) gdk_event_get_event_type (data->event))
        {
        case GDK_TOUCH_BEGIN:
        case GDK_TOUCH_UPDATE:
        case GDK_TOUCH_END:
          if (!gdk_touch_event_get_emulating_pointer (data->event))
            continue;
          G_GNUC_FALLTHROUGH;
        case GDK_BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
        case GDK_MOTION_NOTIFY:
          *sequence = seq;
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}
