/* GTK - The GIMP Toolkit
 * Copyright (C) 2017-2018, Red Hat, Inc.
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
 * GtkGestureStylus:
 *
 * `GtkGestureStylus` is a `GtkGesture` specific to stylus input.
 *
 * The provided signals just relay the basic information of the
 * stylus events.
 */

#include "config.h"
#include "gtkgesturestylus.h"
#include "gtkgesturestylusprivate.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtknative.h"

typedef struct {
  gboolean stylus_only;
} GtkGestureStylusPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureStylus, gtk_gesture_stylus, GTK_TYPE_GESTURE_SINGLE)

enum {
  PROP_STYLUS_ONLY = 1,
  N_PROPERTIES
};

enum {
  PROXIMITY,
  DOWN,
  MOTION,
  UP,
  N_SIGNALS
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
static guint signals[N_SIGNALS] = { 0, };

static void gtk_gesture_stylus_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
	GtkGestureStylus *gesture = GTK_GESTURE_STYLUS (object);

    switch (prop_id)
      {
      case PROP_STYLUS_ONLY:
        g_value_set_boolean (value, gtk_gesture_stylus_get_stylus_only (gesture));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
}

static void gtk_gesture_stylus_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
    GtkGestureStylus *gesture = GTK_GESTURE_STYLUS (object);

    switch (prop_id)
      {
      case PROP_STYLUS_ONLY:
        gtk_gesture_stylus_set_stylus_only (gesture, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
}

static gboolean
gtk_gesture_stylus_handle_event (GtkEventController *controller,
                                 GdkEvent           *event,
                                 double              x,
                                 double              y)
{
  GtkGestureStylusPrivate* priv;
  GdkModifierType modifiers;
  guint n_signal;

  priv = gtk_gesture_stylus_get_instance_private (GTK_GESTURE_STYLUS (controller));
  GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_stylus_parent_class)->handle_event (controller, event, x, y);

  if (priv->stylus_only && !gdk_event_get_device_tool (event))
    return FALSE;

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_BUTTON_PRESS:
      n_signal = DOWN;
      break;
    case GDK_BUTTON_RELEASE:
      n_signal = UP;
      break;
    case GDK_MOTION_NOTIFY:
      modifiers = gdk_event_get_modifier_state (event);

      if (modifiers & GDK_BUTTON1_MASK)
        n_signal = MOTION;
      else
        n_signal = PROXIMITY;
      break;
    default:
      return FALSE;
    }

  g_signal_emit (controller, signals[n_signal], 0, x, y);

  return TRUE;
}

static void
gtk_gesture_stylus_class_init (GtkGestureStylusClass *klass)
{
  GObjectClass* object_class;
  GtkEventControllerClass *event_controller_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = gtk_gesture_stylus_get_property;
  object_class->set_property = gtk_gesture_stylus_set_property;

  /**
   * GtkGestureStylus:stylus-only: (attributes org.gtk.Property.get=gtk_gesture_stylus_get_stylus_only org.gtk.Property.set=gtk_gesture_stylus_set_stylus_only)
   *
   * If this gesture should exclusively react to stylus input devices.
   *
   * Since: 4.10
   */
  obj_properties[PROP_STYLUS_ONLY] = g_param_spec_boolean ("stylus-only", NULL, NULL,
                                                     TRUE,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_EXPLICIT_NOTIFY |
                                                     G_PARAM_CONSTRUCT);
  g_object_class_install_properties (object_class,  N_PROPERTIES, obj_properties);

  event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  event_controller_class->handle_event = gtk_gesture_stylus_handle_event;

  /**
   * GtkGestureStylus::proximity:
   * @gesture: the `GtkGestureStylus` that emitted the signal
   * @x: the X coordinate of the stylus event
   * @y: the Y coordinate of the stylus event
   *
   * Emitted when the stylus is in proximity of the device.
   */
  signals[PROXIMITY] =
    g_signal_new (I_("proximity"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, proximity),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[PROXIMITY],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  /**
   * GtkGestureStylus::down:
   * @gesture: the `GtkGestureStylus` that emitted the signal
   * @x: the X coordinate of the stylus event
   * @y: the Y coordinate of the stylus event
   *
   * Emitted when the stylus touches the device.
   */
  signals[DOWN] =
    g_signal_new (I_("down"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, down),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[DOWN],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  /**
   * GtkGestureStylus::motion:
   * @gesture: the `GtkGestureStylus` that emitted the signal
   * @x: the X coordinate of the stylus event
   * @y: the Y coordinate of the stylus event
   *
   * Emitted when the stylus moves while touching the device.
   */
  signals[MOTION] =
    g_signal_new (I_("motion"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, motion),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[MOTION],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  /**
   * GtkGestureStylus::up:
   * @gesture: the `GtkGestureStylus` that emitted the signal
   * @x: the X coordinate of the stylus event
   * @y: the Y coordinate of the stylus event
   *
   * Emitted when the stylus no longer touches the device.
   */
  signals[UP] =
    g_signal_new (I_("up"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, up),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[UP],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_gesture_stylus_init (GtkGestureStylus *gesture)
{
  GtkGestureStylusPrivate* priv = gtk_gesture_stylus_get_instance_private(gesture);

  priv->stylus_only = TRUE;
}

/**
 * gtk_gesture_stylus_new:
 *
 * Creates a new `GtkGestureStylus`.
 *
 * Returns: a newly created stylus gesture
 **/
GtkGesture *
gtk_gesture_stylus_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_STYLUS,
                       NULL);
}

/**
 * gtk_gesture_stylus_get_stylus_only: (attributes org.gtk.Method.get_property=stylus-only)
 * @gesture: the gesture
 *
 * Checks whether the gesture is for styluses only.
 *
 * Stylus-only gestures will signal events exclusively from stylus
 * input devices.
 *
 * Returns: %TRUE if the gesture is only for stylus events
 *
 * Since: 4.10
 */
gboolean
gtk_gesture_stylus_get_stylus_only (GtkGestureStylus *gesture)
{
  GtkGestureStylusPrivate *priv = gtk_gesture_stylus_get_instance_private (gesture);

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);

  return priv->stylus_only;
}

/**
 * gtk_gesture_stylus_set_stylus_only: (attributes org.gtk.Method.set_property=stylus-only)
 * @gesture: the gesture
 * @stylus_only: whether the gesture is used exclusively for stylus events
 *
 * Sets the state of stylus-only
 *
 * If true, the gesture will exclusively handle events from stylus input devices,
 * otherwise it'll handle events from any pointing device.
 *
 * Since: 4.10
 */
void
gtk_gesture_stylus_set_stylus_only (GtkGestureStylus *gesture, gboolean stylus_only)
{
  GtkGestureStylusPrivate *priv = gtk_gesture_stylus_get_instance_private (gesture);

  g_return_if_fail (GTK_IS_GESTURE_STYLUS (gesture));

  if (priv->stylus_only == stylus_only)
    return;

  priv->stylus_only = stylus_only;

  g_object_notify_by_pspec (G_OBJECT (gesture), obj_properties[PROP_STYLUS_ONLY]);
}

/**
 * gtk_gesture_stylus_get_axis:
 * @gesture: a `GtkGestureStylus`
 * @axis: requested device axis
 * @value: (out): return location for the axis value
 *
 * Returns the current value for the requested @axis.
 *
 * This function must be called from the handler of one of the
 * [signal@Gtk.GestureStylus::down], [signal@Gtk.GestureStylus::motion],
 * [signal@Gtk.GestureStylus::up] or [signal@Gtk.GestureStylus::proximity]
 * signals.
 *
 * Returns: %TRUE if there is a current value for the axis
 **/
gboolean
gtk_gesture_stylus_get_axis (GtkGestureStylus *gesture,
			     GdkAxisUse        axis,
			     double           *value)
{
  GdkEvent *event;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);
  g_return_val_if_fail (axis < GDK_AXIS_LAST, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));
  if (!event)
    return FALSE;

  return gdk_event_get_axis (event, axis, value);
}

/**
 * gtk_gesture_stylus_get_axes:
 * @gesture: a `GtkGestureStylus`
 * @axes: (array): array of requested axes, terminated with %GDK_AXIS_IGNORE
 * @values: (out) (array): return location for the axis values
 *
 * Returns the current values for the requested @axes.
 *
 * This function must be called from the handler of one of the
 * [signal@Gtk.GestureStylus::down], [signal@Gtk.GestureStylus::motion],
 * [signal@Gtk.GestureStylus::up] or [signal@Gtk.GestureStylus::proximity]
 * signals.
 *
 * Returns: %TRUE if there is a current value for the axes
 */
gboolean
gtk_gesture_stylus_get_axes (GtkGestureStylus  *gesture,
			     GdkAxisUse         axes[],
			     double           **values)
{
  GdkEvent *event;
  GArray *array;
  int i = 0;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);
  g_return_val_if_fail (values != NULL, FALSE);

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));
  if (!event)
    return FALSE;

  array = g_array_new (TRUE, FALSE, sizeof (double));

  while (axes[i] != GDK_AXIS_IGNORE)
    {
      double value;

      if (axes[i] >= GDK_AXIS_LAST)
        {
          g_warning ("Requesting unknown axis %d, did you "
                     "forget to add a last GDK_AXIS_IGNORE axis?",
                     axes[i]);
          g_array_free (array, TRUE);
          return FALSE;
        }

      gdk_event_get_axis (event, axes[i], &value);
      g_array_append_val (array, value);
      i++;
    }

  *values = (double *) g_array_free (array, FALSE);
  return TRUE;
}

/**
 * gtk_gesture_stylus_get_backlog:
 * @gesture: a `GtkGestureStylus`
 * @backlog: (out) (array length=n_elems): coordinates and times for the backlog events
 * @n_elems: (out): return location for the number of elements
 *
 * Returns the accumulated backlog of tracking information.
 *
 * By default, GTK will limit rate of input events. On stylus input
 * where accuracy of strokes is paramount, this function returns the
 * accumulated coordinate/timing state before the emission of the
 * current [Gtk.GestureStylus::motion] signal.
 *
 * This function may only be called within a [signal@Gtk.GestureStylus::motion]
 * signal handler, the state given in this signal and obtainable through
 * [method@Gtk.GestureStylus.get_axis] express the latest (most up-to-date)
 * state in motion history.
 *
 * The @backlog is provided in chronological order.
 *
 * Returns: %TRUE if there is a backlog to unfold in the current state.
 */
gboolean
gtk_gesture_stylus_get_backlog (GtkGestureStylus  *gesture,
                                GdkTimeCoord     **backlog,
                                guint             *n_elems)
{
  GdkEvent *event;
  GArray *backlog_array;
  GdkTimeCoord *history = NULL;
  guint n_coords = 0, i;
  double surf_x, surf_y;
  GtkNative *native;
  GtkWidget *event_widget;
  GtkWidget *controller_widget;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);
  g_return_val_if_fail (backlog != NULL && n_elems != NULL, FALSE);

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));

  if (event && GDK_IS_EVENT_TYPE (event, GDK_MOTION_NOTIFY))
    history = gdk_event_get_history (event, &n_coords);

  if (!history)
    return FALSE;

  native = gtk_widget_get_native (gtk_get_event_widget (event));
  gtk_native_get_surface_transform (native, &surf_x, &surf_y);

  backlog_array = g_array_new (FALSE, FALSE, sizeof (GdkTimeCoord));
  event_widget = gtk_get_event_widget (event);
  controller_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  for (i = 0; i < n_coords; i++)
    {
      const GdkTimeCoord *time_coord = &history[i];
      graphene_point_t p;

      if (gtk_widget_compute_point (event_widget, controller_widget,
                                    &GRAPHENE_POINT_INIT (time_coord->axes[GDK_AXIS_X] - surf_x,
                                                          time_coord->axes[GDK_AXIS_Y] - surf_y),
                                    &p))
        {
          GdkTimeCoord translated_coord = *time_coord;

          translated_coord.axes[GDK_AXIS_X] = p.x;
          translated_coord.axes[GDK_AXIS_Y] = p.y;

          g_array_append_val (backlog_array, translated_coord);
        }
      }

  *n_elems = backlog_array->len;
  *backlog = (GdkTimeCoord *) g_array_free (backlog_array, FALSE);
  g_free (history);

  return TRUE;
}

/**
 * gtk_gesture_stylus_get_device_tool:
 * @gesture: a `GtkGestureStylus`
 *
 * Returns the `GdkDeviceTool` currently driving input through this gesture.
 *
 * This function must be called from the handler of one of the
 * [signal@Gtk.GestureStylus::down], [signal@Gtk.GestureStylus::motion],
 * [signal@Gtk.GestureStylus::up] or [signal@Gtk.GestureStylus::proximity]
 * signals.
 *
 * Returns: (nullable) (transfer none): The current stylus tool
 */
GdkDeviceTool *
gtk_gesture_stylus_get_device_tool (GtkGestureStylus *gesture)
{
  GdkEvent *event;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));
  if (!event)
    return NULL;

  return gdk_event_get_device_tool (event);
}
