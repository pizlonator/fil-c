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
 * GtkGestureSwipe:
 *
 * `GtkGestureSwipe` is a `GtkGesture` for swipe gestures.
 *
 * After a press/move/.../move/release sequence happens, the
 * [signal@Gtk.GestureSwipe::swipe] signal will be emitted,
 * providing the velocity and directionality of the sequence
 * at the time it was lifted.
 *
 * If the velocity is desired in intermediate points,
 * [method@Gtk.GestureSwipe.get_velocity] can be called in a
 * [signal@Gtk.Gesture::update] handler.
 *
 * All velocities are reported in pixels/sec units.
 */

#include "config.h"
#include "gtkgestureswipe.h"
#include "gtkgestureswipeprivate.h"
#include "gtkgestureprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

#define CAPTURE_THRESHOLD_MS 150

typedef struct _GtkGestureSwipePrivate GtkGestureSwipePrivate;
typedef struct _EventData EventData;

struct _EventData
{
  guint32 evtime;
  int x;
  int y;
};

struct _GtkGestureSwipePrivate
{
  GArray *events;
};

enum {
  SWIPE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureSwipe, gtk_gesture_swipe, GTK_TYPE_GESTURE_SINGLE)

static void
gtk_gesture_swipe_finalize (GObject *object)
{
  GtkGestureSwipePrivate *priv;

  priv = gtk_gesture_swipe_get_instance_private (GTK_GESTURE_SWIPE (object));
  g_array_free (priv->events, TRUE);

  G_OBJECT_CLASS (gtk_gesture_swipe_parent_class)->finalize (object);
}

static gboolean
gtk_gesture_swipe_filter_event (GtkEventController *controller,
                                GdkEvent           *event)
{
  /* Let touchpad swipe and hold events go through, only if they match n-points */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE ||
      gdk_event_get_event_type (event) == GDK_TOUCHPAD_HOLD)
    {
      guint n_points;
      guint n_fingers;

      g_object_get (G_OBJECT (controller), "n-points", &n_points, NULL);

      n_fingers = gdk_touchpad_event_get_n_fingers (event);

      if (n_fingers == n_points)
        return FALSE;
      else
        return TRUE;
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_swipe_parent_class)->filter_event (controller, event);
}

static void
_gtk_gesture_swipe_clear_backlog (GtkGestureSwipe *gesture,
                                  guint32          evtime)
{
  GtkGestureSwipePrivate *priv;
  int i, length = 0;

  priv = gtk_gesture_swipe_get_instance_private (gesture);

  for (i = 0; i < (int) priv->events->len; i++)
    {
      EventData *data;

      data = &g_array_index (priv->events, EventData, i);

      if (data->evtime >= evtime - CAPTURE_THRESHOLD_MS)
        {
          length = i - 1;
          break;
        }
    }

  if (length > 0)
    g_array_remove_range (priv->events, 0, length);
}

static void
gtk_gesture_swipe_append_event (GtkGestureSwipe  *swipe,
                                GdkEventSequence *sequence)
{
  GtkGestureSwipePrivate *priv;
  EventData new;
  double x, y;

  priv = gtk_gesture_swipe_get_instance_private (swipe);
  _gtk_gesture_get_last_update_time (GTK_GESTURE (swipe), sequence, &new.evtime);
  gtk_gesture_get_point (GTK_GESTURE (swipe), sequence, &x, &y);

  new.x = x;
  new.y = y;

  _gtk_gesture_swipe_clear_backlog (swipe, new.evtime);
  g_array_append_val (priv->events, new);
}

static void
gtk_gesture_swipe_update (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  GtkGestureSwipe *swipe = GTK_GESTURE_SWIPE (gesture);

  gtk_gesture_swipe_append_event (swipe, sequence);
}

static void
_gtk_gesture_swipe_calculate_velocity (GtkGestureSwipe *gesture,
                                       double          *velocity_x,
                                       double          *velocity_y)
{
  GtkGestureSwipePrivate *priv;
  GdkEventSequence *sequence;
  guint32 evtime, diff_time;
  EventData *start, *end;
  double diff_x, diff_y;

  priv = gtk_gesture_swipe_get_instance_private (gesture);
  *velocity_x = *velocity_y = 0;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  _gtk_gesture_get_last_update_time (GTK_GESTURE (gesture), sequence, &evtime);
  _gtk_gesture_swipe_clear_backlog (gesture, evtime);

  if (priv->events->len == 0)
    return;

  start = &g_array_index (priv->events, EventData, 0);
  end = &g_array_index (priv->events, EventData, priv->events->len - 1);

  diff_time = end->evtime - start->evtime;
  diff_x = end->x - start->x;
  diff_y = end->y - start->y;

  if (diff_time == 0)
    return;

  /* Velocity in pixels/sec */
  *velocity_x = diff_x * 1000 / diff_time;
  *velocity_y = diff_y * 1000 / diff_time;
}

static void
gtk_gesture_swipe_end (GtkGesture       *gesture,
                       GdkEventSequence *sequence)
{
  GtkGestureSwipe *swipe = GTK_GESTURE_SWIPE (gesture);
  GtkGestureSwipePrivate *priv;
  double velocity_x, velocity_y;
  GdkEventSequence *seq;

  seq = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (gtk_gesture_get_sequence_state (gesture, seq) == GTK_EVENT_SEQUENCE_DENIED)
    return;

  if (gtk_gesture_is_active (gesture))
    return;

  gtk_gesture_swipe_append_event (swipe, sequence);

  priv = gtk_gesture_swipe_get_instance_private (swipe);
  _gtk_gesture_swipe_calculate_velocity (swipe, &velocity_x, &velocity_y);
  g_signal_emit (gesture, signals[SWIPE], 0, velocity_x, velocity_y);

  if (priv->events->len > 0)
    g_array_remove_range (priv->events, 0, priv->events->len);
}

static void
gtk_gesture_swipe_class_init (GtkGestureSwipeClass *klass)
{
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);
  GtkEventControllerClass *event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_gesture_swipe_finalize;

  event_controller_class->filter_event = gtk_gesture_swipe_filter_event;

  gesture_class->update = gtk_gesture_swipe_update;
  gesture_class->end = gtk_gesture_swipe_end;

  /**
   * GtkGestureSwipe::swipe:
   * @gesture: object which received the signal
   * @velocity_x: velocity in the X axis, in pixels/sec
   * @velocity_y: velocity in the Y axis, in pixels/sec
   *
   * Emitted when the recognized gesture is finished.
   *
   * Velocity and direction are a product of previously recorded events.
   */
  signals[SWIPE] =
    g_signal_new (I_("swipe"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureSwipeClass, swipe),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[SWIPE],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_gesture_swipe_init (GtkGestureSwipe *gesture)
{
  GtkGestureSwipePrivate *priv;

  priv = gtk_gesture_swipe_get_instance_private (gesture);
  priv->events = g_array_new (FALSE, FALSE, sizeof (EventData));
}

/**
 * gtk_gesture_swipe_new:
 *
 * Returns a newly created `GtkGesture` that recognizes swipes.
 *
 * Returns: a newly created `GtkGestureSwipe`
 */
GtkGesture *
gtk_gesture_swipe_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_SWIPE,
                       NULL);
}

/**
 * gtk_gesture_swipe_get_velocity:
 * @gesture: a `GtkGestureSwipe`
 * @velocity_x: (out): return value for the velocity in the X axis, in pixels/sec
 * @velocity_y: (out): return value for the velocity in the Y axis, in pixels/sec
 *
 * Gets the current velocity.
 *
 * If the gesture is recognized, this function returns %TRUE and fills
 * in @velocity_x and @velocity_y with the recorded velocity, as per the
 * last events processed.
 *
 * Returns: whether velocity could be calculated
 */
gboolean
gtk_gesture_swipe_get_velocity (GtkGestureSwipe *gesture,
                                double          *velocity_x,
                                double          *velocity_y)
{
  double vel_x, vel_y;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  if (!gtk_gesture_is_recognized (GTK_GESTURE (gesture)))
    return FALSE;

  _gtk_gesture_swipe_calculate_velocity (gesture, &vel_x, &vel_y);

  if (velocity_x)
    *velocity_x = vel_x;
  if (velocity_y)
    *velocity_y = vel_y;

  return TRUE;
}
