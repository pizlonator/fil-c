/* GTK - The GIMP Toolkit
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
 * GtkGestureDrag:
 *
 * `GtkGestureDrag` is a `GtkGesture` implementation for drags.
 *
 * The drag operation itself can be tracked throughout the
 * [signal@Gtk.GestureDrag::drag-begin],
 * [signal@Gtk.GestureDrag::drag-update] and
 * [signal@Gtk.GestureDrag::drag-end] signals, and the relevant
 * coordinates can be extracted through
 * [method@Gtk.GestureDrag.get_offset] and
 * [method@Gtk.GestureDrag.get_start_point].
 */
#include "config.h"
#include "gtkgesturedrag.h"
#include "gtkgesturedragprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

typedef struct _GtkGestureDragPrivate GtkGestureDragPrivate;
typedef struct _EventData EventData;

struct _GtkGestureDragPrivate
{
  double start_x;
  double start_y;
  double last_x;
  double last_y;
};

enum {
  DRAG_BEGIN,
  DRAG_UPDATE,
  DRAG_END,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureDrag, gtk_gesture_drag, GTK_TYPE_GESTURE_SINGLE)

static gboolean
gtk_gesture_drag_filter_event (GtkEventController *controller,
                               GdkEvent           *event)
{
  /* Let touchpad swipe events go through, only if they match n-points  */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE)
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

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_drag_parent_class)->filter_event (controller, event);
}

static void
gtk_gesture_drag_begin (GtkGesture       *gesture,
                        GdkEventSequence *sequence)
{
  GtkGestureDragPrivate *priv;
  GdkEventSequence *current;

  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  priv = gtk_gesture_drag_get_instance_private (GTK_GESTURE_DRAG (gesture));
  gtk_gesture_get_point (gesture, current, &priv->start_x, &priv->start_y);
  priv->last_x = priv->start_x;
  priv->last_y = priv->start_y;

  g_signal_emit (gesture, signals[DRAG_BEGIN], 0, priv->start_x, priv->start_y);
}

static void
gtk_gesture_drag_update (GtkGesture       *gesture,
                         GdkEventSequence *sequence)
{
  GtkGestureDragPrivate *priv;
  double x, y;

  priv = gtk_gesture_drag_get_instance_private (GTK_GESTURE_DRAG (gesture));
  gtk_gesture_get_point (gesture, sequence, &priv->last_x, &priv->last_y);
  x = priv->last_x - priv->start_x;
  y = priv->last_y - priv->start_y;

  g_signal_emit (gesture, signals[DRAG_UPDATE], 0, x, y);
}

static void
gtk_gesture_drag_end (GtkGesture       *gesture,
                      GdkEventSequence *sequence)
{
  GtkGestureDragPrivate *priv;
  GdkEventSequence *current;
  double x, y;

  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  priv = gtk_gesture_drag_get_instance_private (GTK_GESTURE_DRAG (gesture));
  gtk_gesture_get_point (gesture, current, &priv->last_x, &priv->last_y);
  x = priv->last_x - priv->start_x;
  y = priv->last_y - priv->start_y;

  g_signal_emit (gesture, signals[DRAG_END], 0, x, y);
}

static void
gtk_gesture_drag_class_init (GtkGestureDragClass *klass)
{
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);
  GtkEventControllerClass *event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  event_controller_class->filter_event = gtk_gesture_drag_filter_event;

  gesture_class->begin = gtk_gesture_drag_begin;
  gesture_class->update = gtk_gesture_drag_update;
  gesture_class->end = gtk_gesture_drag_end;

  /**
   * GtkGestureDrag::drag-begin:
   * @gesture: the object which received the signal
   * @start_x: X coordinate, relative to the widget allocation
   * @start_y: Y coordinate, relative to the widget allocation
   *
   * Emitted whenever dragging starts.
   */
  signals[DRAG_BEGIN] =
    g_signal_new (I_("drag-begin"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureDragClass, drag_begin),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[DRAG_BEGIN],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
  /**
   * GtkGestureDrag::drag-update:
   * @gesture: the object which received the signal
   * @offset_x: X offset, relative to the start point
   * @offset_y: Y offset, relative to the start point
   *
   * Emitted whenever the dragging point moves.
   */
  signals[DRAG_UPDATE] =
    g_signal_new (I_("drag-update"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureDragClass, drag_update),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[DRAG_UPDATE],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
  /**
   * GtkGestureDrag::drag-end:
   * @gesture: the object which received the signal
   * @offset_x: X offset, relative to the start point
   * @offset_y: Y offset, relative to the start point
   *
   * Emitted whenever the dragging is finished.
   */
  signals[DRAG_END] =
    g_signal_new (I_("drag-end"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureDragClass, drag_end),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[DRAG_END],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_gesture_drag_init (GtkGestureDrag *gesture)
{
}

/**
 * gtk_gesture_drag_new:
 *
 * Returns a newly created `GtkGesture` that recognizes drags.
 *
 * Returns: a newly created `GtkGestureDrag`
 **/
GtkGesture *
gtk_gesture_drag_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_DRAG,
                       NULL);
}

/**
 * gtk_gesture_drag_get_start_point:
 * @gesture: a `GtkGesture`
 * @x: (out) (optional): X coordinate for the drag start point
 * @y: (out) (optional): Y coordinate for the drag start point
 *
 * Gets the point where the drag started.
 *
 * If the @gesture is active, this function returns %TRUE
 * and fills in @x and @y with the drag start coordinates,
 * in widget-relative coordinates.
 *
 * Returns: %TRUE if the gesture is active
 */
gboolean
gtk_gesture_drag_get_start_point (GtkGestureDrag *gesture,
                                  double         *x,
                                  double         *y)
{
  GtkGestureDragPrivate *priv;
  GdkEventSequence *sequence;

  g_return_val_if_fail (GTK_IS_GESTURE_DRAG (gesture), FALSE);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return FALSE;

  priv = gtk_gesture_drag_get_instance_private (gesture);

  if (x)
    *x = priv->start_x;

  if (y)
    *y = priv->start_y;

  return TRUE;
}

/**
 * gtk_gesture_drag_get_offset:
 * @gesture: a `GtkGesture`
 * @x: (out) (optional): X offset for the current point
 * @y: (out) (optional): Y offset for the current point
 *
 * Gets the offset from the start point.
 *
 * If the @gesture is active, this function returns %TRUE and
 * fills in @x and @y with the coordinates of the current point,
 * as an offset to the starting drag point.
 *
 * Returns: %TRUE if the gesture is active
 */
gboolean
gtk_gesture_drag_get_offset (GtkGestureDrag *gesture,
                             double         *x,
                             double         *y)
{
  GtkGestureDragPrivate *priv;
  GdkEventSequence *sequence;

  g_return_val_if_fail (GTK_IS_GESTURE_DRAG (gesture), FALSE);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return FALSE;

  priv = gtk_gesture_drag_get_instance_private (gesture);

  if (x)
    *x = priv->last_x - priv->start_x;

  if (y)
    *y = priv->last_y - priv->start_y;

  return TRUE;
}
