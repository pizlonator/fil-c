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
 * GtkGestureRotate:
 *
 * `GtkGestureRotate` is a `GtkGesture` for 2-finger rotations.
 *
 * Whenever the angle between both handled sequences changes, the
 * [signal@Gtk.GestureRotate::angle-changed] signal is emitted.
 */

#include "config.h"
#include <math.h>
#include "gtkgesturerotate.h"
#include "gtkgesturerotateprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

typedef struct _GtkGestureRotatePrivate GtkGestureRotatePrivate;

enum {
  ANGLE_CHANGED,
  LAST_SIGNAL
};

struct _GtkGestureRotatePrivate
{
  double initial_angle;
  double accum_touchpad_angle;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureRotate, gtk_gesture_rotate, GTK_TYPE_GESTURE)

static void
gtk_gesture_rotate_init (GtkGestureRotate *gesture)
{
}

static GObject *
gtk_gesture_rotate_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
  GObject *object;

  object = G_OBJECT_CLASS (gtk_gesture_rotate_parent_class)->constructor (type,
                                                                          n_construct_properties,
                                                                          construct_properties);
  g_object_set (object, "n-points", 2, NULL);

  return object;
}

static gboolean
_gtk_gesture_rotate_get_angle (GtkGestureRotate *rotate,
                               double           *angle)
{
  GtkGestureRotatePrivate *priv;
  GdkEvent *last_event;
  double x1, y1, x2, y2;
  GtkGesture *gesture;
  double dx, dy;
  GList *sequences = NULL;
  GdkTouchpadGesturePhase phase;
  gboolean retval = FALSE;

  gesture = GTK_GESTURE (rotate);
  priv = gtk_gesture_rotate_get_instance_private (rotate);

  if (!gtk_gesture_is_recognized (gesture))
    goto out;

  sequences = gtk_gesture_get_sequences (gesture);
  if (!sequences)
    goto out;

  last_event = gtk_gesture_get_last_event (gesture, sequences->data);

  if (gdk_event_get_event_type (last_event) == GDK_TOUCHPAD_PINCH)
    {
      phase = gdk_touchpad_event_get_gesture_phase (last_event);
      if (phase == GDK_TOUCHPAD_GESTURE_PHASE_CANCEL)
        goto out;

      *angle = priv->accum_touchpad_angle;
    }
  else
    {
      if (!sequences->next)
        goto out;

      gtk_gesture_get_point (gesture, sequences->data, &x1, &y1);
      gtk_gesture_get_point (gesture, sequences->next->data, &x2, &y2);

      dx = x1 - x2;
      dy = y1 - y2;

      *angle = atan2 (dx, dy);

      /* Invert angle */
      *angle = (2 * G_PI) - *angle;

      /* And constraint it to 0°-360° */
      *angle = fmod (*angle, 2 * G_PI);
    }

  retval = TRUE;

 out:
  g_list_free (sequences);
  return retval;
}

static gboolean
_gtk_gesture_rotate_check_emit (GtkGestureRotate *gesture)
{
  GtkGestureRotatePrivate *priv;
  double angle, delta;

  if (!_gtk_gesture_rotate_get_angle (gesture, &angle))
    return FALSE;

  priv = gtk_gesture_rotate_get_instance_private (gesture);
  delta = angle - priv->initial_angle;

  if (delta < 0)
    delta += 2 * G_PI;

  g_signal_emit (gesture, signals[ANGLE_CHANGED], 0, angle, delta);
  return TRUE;
}

static void
gtk_gesture_rotate_begin (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  GtkGestureRotate *rotate = GTK_GESTURE_ROTATE (gesture);
  GtkGestureRotatePrivate *priv;

  priv = gtk_gesture_rotate_get_instance_private (rotate);
  _gtk_gesture_rotate_get_angle (rotate, &priv->initial_angle);
}

static void
gtk_gesture_rotate_update (GtkGesture       *gesture,
                           GdkEventSequence *sequence)
{
  _gtk_gesture_rotate_check_emit (GTK_GESTURE_ROTATE (gesture));
}

static gboolean
gtk_gesture_rotate_filter_event (GtkEventController *controller,
                                 GdkEvent           *event)
{
  /* Let 2-finger touchpad pinch and hold events go through */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_PINCH)
    {
      guint n_fingers;

      n_fingers = gdk_touchpad_event_get_n_fingers (event);

      if (n_fingers == 2)
        return FALSE;
      else
        return TRUE;
    }
  else if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_HOLD)
    return TRUE;

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_rotate_parent_class)->filter_event (controller, event);
}

static gboolean
gtk_gesture_rotate_handle_event (GtkEventController *controller,
                                 GdkEvent           *event,
                                 double              x,
                                 double              y)
{
  GtkGestureRotate *rotate = GTK_GESTURE_ROTATE (controller);
  GtkGestureRotatePrivate *priv;
  GdkTouchpadGesturePhase phase;
  double delta;

  priv = gtk_gesture_rotate_get_instance_private (rotate);

  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_PINCH)
    {
      phase = gdk_touchpad_event_get_gesture_phase (event);
      delta = gdk_touchpad_event_get_pinch_angle_delta (event);
      if (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN ||
          phase == GDK_TOUCHPAD_GESTURE_PHASE_END)
        priv->accum_touchpad_angle = 0;
      else if (phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE)
        priv->accum_touchpad_angle += delta;
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_rotate_parent_class)->handle_event (controller, event, x, y);
}

static void
gtk_gesture_rotate_class_init (GtkGestureRotateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->constructor = gtk_gesture_rotate_constructor;

  event_controller_class->filter_event = gtk_gesture_rotate_filter_event;
  event_controller_class->handle_event = gtk_gesture_rotate_handle_event;

  gesture_class->begin = gtk_gesture_rotate_begin;
  gesture_class->update = gtk_gesture_rotate_update;

  /**
   * GtkGestureRotate::angle-changed:
   * @gesture: the object on which the signal is emitted
   * @angle: Current angle in radians
   * @angle_delta: Difference with the starting angle, in radians
   *
   * Emitted when the angle between both tracked points changes.
   */
  signals[ANGLE_CHANGED] =
    g_signal_new (I_("angle-changed"),
                  GTK_TYPE_GESTURE_ROTATE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkGestureRotateClass, angle_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[ANGLE_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

/**
 * gtk_gesture_rotate_new:
 *
 * Returns a newly created `GtkGesture` that recognizes 2-touch
 * rotation gestures.
 *
 * Returns: a newly created `GtkGestureRotate`
 **/
GtkGesture *
gtk_gesture_rotate_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_ROTATE,
                       NULL);
}

/**
 * gtk_gesture_rotate_get_angle_delta:
 * @gesture: a `GtkGestureRotate`
 *
 * Gets the angle delta in radians.
 *
 * If @gesture is active, this function returns the angle difference
 * in radians since the gesture was first recognized. If @gesture is
 * not active, 0 is returned.
 *
 * Returns: the angle delta in radians
 */
double
gtk_gesture_rotate_get_angle_delta (GtkGestureRotate *gesture)
{
  GtkGestureRotatePrivate *priv;
  double angle;

  g_return_val_if_fail (GTK_IS_GESTURE_ROTATE (gesture), 0.0);

  if (!_gtk_gesture_rotate_get_angle (gesture, &angle))
    return 0.0;

  priv = gtk_gesture_rotate_get_instance_private (gesture);

  return angle - priv->initial_angle;
}
