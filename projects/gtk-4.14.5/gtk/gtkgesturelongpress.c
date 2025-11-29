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
 * GtkGestureLongPress:
 *
 * `GtkGestureLongPress` is a `GtkGesture` for long presses.
 *
 * This gesture is also known as “Press and Hold”.
 *
 * When the timeout is exceeded, the gesture is triggering the
 * [signal@Gtk.GestureLongPress::pressed] signal.
 *
 * If the touchpoint is lifted before the timeout passes, or if
 * it drifts too far of the initial press point, the
 * [signal@Gtk.GestureLongPress::cancelled] signal will be emitted.
 *
 * How long the timeout is before the ::pressed signal gets emitted is
 * determined by the [property@Gtk.Settings:gtk-long-press-time] setting.
 * It can be modified by the [property@Gtk.GestureLongPress:delay-factor]
 * property.
 */

#include "config.h"
#include "gtkgesturelongpress.h"
#include "gtkgesturelongpressprivate.h"
#include "gtkgestureprivate.h"
#include "gtkmarshalers.h"
#include "gtkdragsourceprivate.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"

typedef struct _GtkGestureLongPressPrivate GtkGestureLongPressPrivate;

enum {
  PRESSED,
  CANCELLED,
  N_SIGNALS
};

enum {
  PROP_DELAY_FACTOR = 1,
  LAST_PROP
};

struct _GtkGestureLongPressPrivate
{
  double initial_x;
  double initial_y;

  double delay_factor;
  guint timeout_id;
  guint delay;
  guint cancelled : 1;
  guint triggered : 1;
};

static guint signals[N_SIGNALS] = { 0 };
static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureLongPress, gtk_gesture_long_press, GTK_TYPE_GESTURE_SINGLE)

static void
gtk_gesture_long_press_init (GtkGestureLongPress *gesture)
{
  GtkGestureLongPressPrivate *priv = gtk_gesture_long_press_get_instance_private (gesture);
  priv->delay_factor = 1.0;
}

static gboolean
gtk_gesture_long_press_check (GtkGesture *gesture)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));

  if (priv->cancelled)
    return FALSE;

  return GTK_GESTURE_CLASS (gtk_gesture_long_press_parent_class)->check (gesture);
}

static gboolean
_gtk_gesture_long_press_timeout (gpointer user_data)
{
  GtkGestureLongPress *gesture = user_data;
  GtkGestureLongPressPrivate *priv;
  GdkEventSequence *sequence;
  double x, y;

  priv = gtk_gesture_long_press_get_instance_private (gesture);
  sequence = gtk_gesture_get_last_updated_sequence (GTK_GESTURE (gesture));
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &x, &y);

  priv->timeout_id = 0;
  priv->triggered = TRUE;
  g_signal_emit (gesture, signals[PRESSED], 0, x, y);

  return G_SOURCE_REMOVE;
}

static void
gtk_gesture_long_press_begin (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGestureLongPressPrivate *priv;
  GdkEvent *event;
  GdkEventType event_type;
  GtkWidget *widget;
  int delay;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (gesture, sequence);

  if (!event)
    return;

  event_type = gdk_event_get_event_type (event);

  if (event_type != GDK_BUTTON_PRESS &&
      event_type != GDK_TOUCH_BEGIN)
    return;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  g_object_get (gtk_widget_get_settings (widget),
                "gtk-long-press-time", &delay,
                NULL);

  delay = (int)(priv->delay_factor * delay);

  gtk_gesture_get_point (gesture, sequence,
                         &priv->initial_x, &priv->initial_y);
  priv->timeout_id = g_timeout_add (delay, _gtk_gesture_long_press_timeout, gesture);
  gdk_source_set_static_name_by_id (priv->timeout_id, "[gtk] _gtk_gesture_long_press_timeout");
}

static void
gtk_gesture_long_press_update (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  GtkGestureLongPressPrivate *priv;
  GtkWidget *widget;
  double x, y;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));
  gtk_gesture_get_point (gesture, sequence, &x, &y);

  if (gtk_drag_check_threshold_double (widget, priv->initial_x, priv->initial_y, x, y))
    {
      if (priv->timeout_id)
        {
          g_source_remove (priv->timeout_id);
          priv->timeout_id = 0;
          g_signal_emit (gesture, signals[CANCELLED], 0);
        }

      priv->cancelled = TRUE;
      _gtk_gesture_check (gesture);
    }
}

static void
gtk_gesture_long_press_end (GtkGesture       *gesture,
                            GdkEventSequence *sequence)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));

  if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
      g_signal_emit (gesture, signals[CANCELLED], 0);
    }

  priv->cancelled = priv->triggered = FALSE;
}

static void
gtk_gesture_long_press_cancel (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  gtk_gesture_long_press_end (gesture, sequence);
  GTK_GESTURE_CLASS (gtk_gesture_long_press_parent_class)->cancel (gesture, sequence);
}

static void
gtk_gesture_long_press_sequence_state_changed (GtkGesture            *gesture,
                                               GdkEventSequence      *sequence,
                                               GtkEventSequenceState  state)
{
  if (state == GTK_EVENT_SEQUENCE_DENIED)
    gtk_gesture_long_press_end (gesture, sequence);
}

static void
gtk_gesture_long_press_finalize (GObject *object)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (object));

  if (priv->timeout_id)
    g_source_remove (priv->timeout_id);

  G_OBJECT_CLASS (gtk_gesture_long_press_parent_class)->finalize (object);
}

static void
gtk_gesture_long_press_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  switch (property_id)
    {
    case PROP_DELAY_FACTOR:
      g_value_set_double (value, gtk_gesture_long_press_get_delay_factor (GTK_GESTURE_LONG_PRESS (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_gesture_long_press_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_DELAY_FACTOR:
      gtk_gesture_long_press_set_delay_factor (GTK_GESTURE_LONG_PRESS (object),
                                               g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_gesture_long_press_class_init (GtkGestureLongPressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->finalize = gtk_gesture_long_press_finalize;
  object_class->get_property = gtk_gesture_long_press_get_property;
  object_class->set_property = gtk_gesture_long_press_set_property;

  gesture_class->check = gtk_gesture_long_press_check;
  gesture_class->begin = gtk_gesture_long_press_begin;
  gesture_class->update = gtk_gesture_long_press_update;
  gesture_class->end = gtk_gesture_long_press_end;
  gesture_class->cancel = gtk_gesture_long_press_cancel;
  gesture_class->sequence_state_changed = gtk_gesture_long_press_sequence_state_changed;

  /**
   * GtkGestureLongPress:delay-factor: (attributes org.gtk.Property.get=gtk_gesture_long_press_get_delay_factor org.gtk.Property.set=gtk_gesture_long_press_set_delay_factor)
   *
   * Factor by which to modify the default timeout.
   */
  props[PROP_DELAY_FACTOR] =
    g_param_spec_double ("delay-factor", NULL, NULL,
                         0.5, 2.0, 1.0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /**
   * GtkGestureLongPress::pressed:
   * @gesture: the object which received the signal
   * @x: the X coordinate where the press happened, relative to the widget allocation
   * @y: the Y coordinate where the press happened, relative to the widget allocation
   *
   * Emitted whenever a press goes unmoved/unreleased longer than
   * what the GTK defaults tell.
   */
  signals[PRESSED] =
    g_signal_new (I_("pressed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureLongPressClass, pressed),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[PRESSED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  /**
   * GtkGestureLongPress::cancelled:
   * @gesture: the object which received the signal
   *
   * Emitted whenever a press moved too far, or was released
   * before [signal@Gtk.GestureLongPress::pressed] happened.
   */
  signals[CANCELLED] =
    g_signal_new (I_("cancelled"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureLongPressClass, cancelled),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

/**
 * gtk_gesture_long_press_new:
 *
 * Returns a newly created `GtkGesture` that recognizes long presses.
 *
 * Returns: a newly created `GtkGestureLongPress`.
 */
GtkGesture *
gtk_gesture_long_press_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_LONG_PRESS,
                       NULL);
}

/**
 * gtk_gesture_long_press_set_delay_factor: (attributes org.gtk.Method.set_property=delay-factor)
 * @gesture: A `GtkGestureLongPress`
 * @delay_factor: The delay factor to apply
 *
 * Applies the given delay factor.
 *
 * The default long press time will be multiplied by this value.
 * Valid values are in the range [0.5..2.0].
 */
void
gtk_gesture_long_press_set_delay_factor (GtkGestureLongPress *gesture,
                                         double               delay_factor)
{
  GtkGestureLongPressPrivate *priv = gtk_gesture_long_press_get_instance_private (gesture);

  g_return_if_fail (GTK_IS_GESTURE_LONG_PRESS (gesture));
  g_return_if_fail (delay_factor >= 0.5);
  g_return_if_fail (delay_factor <= 2.0);

  if (delay_factor == priv->delay_factor)
    return;

  priv->delay_factor = delay_factor;

  g_object_notify_by_pspec (G_OBJECT (gesture), props[PROP_DELAY_FACTOR]);
}

/**
 * gtk_gesture_long_press_get_delay_factor: (attributes org.gtk.Method.get_property=delay-factor)
 * @gesture: A `GtkGestureLongPress`
 *
 * Returns the delay factor.
 *
 * Returns: the delay factor
 */
double
gtk_gesture_long_press_get_delay_factor (GtkGestureLongPress *gesture)
{
  GtkGestureLongPressPrivate *priv = gtk_gesture_long_press_get_instance_private (gesture);

  g_return_val_if_fail (GTK_IS_GESTURE_LONG_PRESS (gesture), 0);

  return priv->delay_factor;
}
