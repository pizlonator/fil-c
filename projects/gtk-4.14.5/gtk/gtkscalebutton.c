/* GTK - The GIMP Toolkit
 * Copyright (C) 2005 Ronald S. Bultje
 * Copyright (C) 2006, 2007 Christian Persch
 * Copyright (C) 2006 Jan Arne Petersen
 * Copyright (C) 2005-2007 Red Hat, Inc.
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * Authors:
 * - Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * - Bastien Nocera <bnocera@redhat.com>
 * - Jan Arne Petersen <jpetersen@jpetersen.org>
 * - Christian Persch <chpe@svn.gnome.org>
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
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkscalebutton.h"

#include "gtkaccessiblerange.h"
#include "gtkadjustment.h"
#include "gtkbox.h"
#include "gtkbuttonprivate.h"
#include "gtktogglebutton.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkframe.h"
#include "gtkgesture.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtkrangeprivate.h"
#include "gtkscale.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtknative.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * GtkScaleButton:
 *
 * `GtkScaleButton` provides a button which pops up a scale widget.
 *
 * This kind of widget is commonly used for volume controls in multimedia
 * applications, and GTK provides a [class@Gtk.VolumeButton] subclass that
 * is tailored for this use case.
 *
 * # CSS nodes
 *
 * ```
 * scalebutton.scale
 * ╰── button.toggle
 *     ╰── <icon>
 * ```
 *
 * `GtkScaleButton` has a single CSS node with name scalebutton and `.scale`
 * style class, and contains a `button` node with a `.toggle` style class.
 */


#define SCALE_SIZE 100

enum
{
  VALUE_CHANGED,
  POPUP,
  POPDOWN,

  LAST_SIGNAL
};

enum
{
  PROP_0,

  PROP_ORIENTATION,
  PROP_VALUE,
  PROP_SIZE,
  PROP_ADJUSTMENT,
  PROP_ICONS,
  PROP_ACTIVE,
  PROP_HAS_FRAME
};

typedef struct
{
  GtkWidget *button;

  GtkWidget *plus_button;
  GtkWidget *minus_button;
  GtkWidget *dock;
  GtkWidget *box;
  GtkWidget *scale;
  GtkWidget *active_button;

  GtkOrientation orientation;
  GtkOrientation applied_orientation;

  guint autoscroll_timeout;
  GtkScrollType autoscroll_step;
  gboolean autoscrolling;

  char **icon_list;

  GtkAdjustment *adjustment; /* needed because it must be settable in init() */
} GtkScaleButtonPrivate;

static void     gtk_scale_button_constructed    (GObject             *object);
static void	gtk_scale_button_dispose	(GObject             *object);
static void     gtk_scale_button_finalize       (GObject             *object);
static void	gtk_scale_button_set_property	(GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void	gtk_scale_button_get_property	(GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);
static void     gtk_scale_button_size_allocate  (GtkWidget           *widget,
                                                 int                  width,
                                                 int                  height,
                                                 int                  baseline);
static void     gtk_scale_button_measure        (GtkWidget           *widget,
                                                 GtkOrientation       orientation,
                                                 int                  for_size,
                                                 int                 *minimum,
                                                 int                 *natural,
                                                 int                 *minimum_baseline,
                                                 int                 *natural_baseline);
static void gtk_scale_button_set_orientation_private (GtkScaleButton *button,
                                                      GtkOrientation  orientation);
static void     gtk_scale_button_popup          (GtkWidget           *widget);
static void     gtk_scale_button_popdown        (GtkWidget           *widget);
static void     cb_button_clicked               (GtkWidget           *button,
                                                 gpointer             user_data);
static void     gtk_scale_button_update_icon    (GtkScaleButton      *button);
static void     cb_scale_value_changed          (GtkRange            *range,
                                                 gpointer             user_data);
static void     cb_popup_mapped                 (GtkWidget           *popup,
                                                 gpointer             user_data);

static gboolean gtk_scale_button_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                           double                    dx,
                                                           double                    dy,
                                                           GtkScaleButton           *button);

static void     gtk_scale_button_accessible_range_init    (GtkAccessibleRangeInterface *iface);


G_DEFINE_TYPE_WITH_CODE (GtkScaleButton, gtk_scale_button, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkScaleButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_RANGE,
                                                gtk_scale_button_accessible_range_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static guint signals[LAST_SIGNAL] = { 0, };

static gboolean
accessible_range_set_current_value (GtkAccessibleRange *accessible_range,
                                    double              value)
{
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (accessible_range), value);

  return TRUE;
}

static void
gtk_scale_button_accessible_range_init (GtkAccessibleRangeInterface *iface)
{
  iface->set_current_value = accessible_range_set_current_value;
}

static void
gtk_scale_button_class_init (GtkScaleButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructed = gtk_scale_button_constructed;
  gobject_class->finalize = gtk_scale_button_finalize;
  gobject_class->dispose = gtk_scale_button_dispose;
  gobject_class->set_property = gtk_scale_button_set_property;
  gobject_class->get_property = gtk_scale_button_get_property;

  widget_class->measure = gtk_scale_button_measure;
  widget_class->size_allocate = gtk_scale_button_size_allocate;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->grab_focus = gtk_widget_grab_focus_child;


  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkScaleButton:value: (attributes org.gtk.Property.get=gtk_scale_button_get_value org.gtk.Property.set=gtk_scale_button_set_value)
   *
   * The value of the scale.
   */
  g_object_class_install_property (gobject_class,
				   PROP_VALUE,
				   g_param_spec_double ("value", NULL, NULL,
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0,
							GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkScaleButton:adjustment: (attributes org.gtk.Property.get=gtk_scale_button_get_adjustment org.gtk.Property.set=gtk_scale_button_set_adjustment)
   *
   * The `GtkAdjustment` that is used as the model.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment", NULL, NULL,
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkScaleButton:icons: (attributes org.gtk.Property.set=gtk_scale_button_set_icons)
   *
   * The names of the icons to be used by the scale button.
   *
   * The first item in the array will be used in the button
   * when the current value is the lowest value, the second
   * item for the highest value. All the subsequent icons will
   * be used for all the other values, spread evenly over the
   * range of values.
   *
   * If there's only one icon name in the @icons array, it will
   * be used for all the values. If only two icon names are in
   * the @icons array, the first one will be used for the bottom
   * 50% of the scale, and the second one for the top 50%.
   *
   * It is recommended to use at least 3 icons so that the
   * `GtkScaleButton` reflects the current value of the scale
   * better for the users.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ICONS,
                                   g_param_spec_boxed ("icons", NULL, NULL,
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkScaleButton:active: (attributes org.gtk.Property.get=gtk_scale_button_get_active)
   *
   * If the scale button should be pressed in.
   *
   * Since: 4.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READABLE));

  /**
   * GtkScaleButton:has-frame: (attributes org.gtk.Property.get=gtk_scale_button_get_has_frame org.gtk.Property.set=gtk_scale_button_set_has_frame)
   *
   * If the scale button has a frame.
   *
   * Since: 4.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_FRAME,
                                   g_param_spec_boolean ("has-frame", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkScaleButton::value-changed:
   * @button: the object which received the signal
   * @value: the new value
   *
   * Emitted when the value field has changed.
   */
  signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkScaleButtonClass, value_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /**
   * GtkScaleButton::popup:
   * @button: the object which received the signal
   *
   * Emitted to popup the scale widget.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are <kbd>Space</kbd>,
   * <kbd>Enter</kbd> and <kbd>Return</kbd>.
   */
  signals[POPUP] =
    g_signal_new_class_handler (I_("popup"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_scale_button_popup),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  /**
   * GtkScaleButton::popdown:
   * @button: the object which received the signal
   *
   * Emitted to dismiss the popup.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>Escape</kbd>.
   */
  signals[POPDOWN] =
    g_signal_new_class_handler (I_("popdown"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_scale_button_popdown),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  /* Key bindings */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_space, 0,
				       "popup",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Space, 0,
				       "popup",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Return, 0,
				       "popup",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_ISO_Enter, 0,
				       "popup",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Enter, 0,
				       "popup",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Escape, 0,
				       "popdown",
                                       NULL);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkscalebutton.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, button);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkScaleButton, plus_button);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkScaleButton, minus_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, dock);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, scale);

  gtk_widget_class_bind_template_callback (widget_class, cb_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, cb_scale_value_changed);
  gtk_widget_class_bind_template_callback (widget_class, cb_popup_mapped);

  gtk_widget_class_set_css_name (widget_class, I_("scalebutton"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static gboolean
start_autoscroll (gpointer data)
{
  GtkScaleButton *button = data;
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  gtk_range_start_autoscroll (GTK_RANGE (priv->scale), priv->autoscroll_step);

  priv->autoscrolling = TRUE;
  priv->autoscroll_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
button_pressed_cb (GtkGesture     *gesture,
                   int             n_press,
                   double          x,
                   double          y,
                   GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  if (widget == priv->plus_button)
    priv->autoscroll_step = GTK_SCROLL_PAGE_FORWARD;
  else
    priv->autoscroll_step = GTK_SCROLL_PAGE_BACKWARD;
  priv->autoscroll_timeout = g_timeout_add (200, start_autoscroll, button);
}

static void
gtk_scale_button_toggled (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  gboolean active;

  g_object_notify (G_OBJECT (button), "active");

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button));

  if (active)
    gtk_popover_popup (GTK_POPOVER (priv->dock));
  else
    gtk_popover_popdown (GTK_POPOVER (priv->dock));
}

static void
gtk_scale_button_closed (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button), FALSE);
}

static void
gtk_scale_button_init (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  GtkEventController *controller;

  priv->orientation = GTK_ORIENTATION_VERTICAL;
  priv->applied_orientation = GTK_ORIENTATION_VERTICAL;

  gtk_widget_init_template (GTK_WIDGET (button));
  gtk_widget_set_parent (priv->dock, GTK_WIDGET (button));

  /* Need a local reference to the adjustment */
  priv->adjustment = gtk_adjustment_new (0, 0, 100, 2, 20, 0);
  g_object_ref_sink (priv->adjustment);
  gtk_range_set_adjustment (GTK_RANGE (priv->scale), priv->adjustment);

  gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, gtk_adjustment_get_upper (priv->adjustment),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, gtk_adjustment_get_lower (priv->adjustment),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, gtk_adjustment_get_value (priv->adjustment),
                                  -1);

  gtk_widget_add_css_class (GTK_WIDGET (button), "scale");

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_scale_button_scroll_controller_scroll),
                    button);
  gtk_widget_add_controller (GTK_WIDGET (button), controller);

  g_signal_connect_swapped (priv->dock, "closed",
                            G_CALLBACK (gtk_scale_button_closed), button);
  g_signal_connect_swapped (priv->button, "toggled",
                            G_CALLBACK (gtk_scale_button_toggled), button);

  g_signal_connect (gtk_button_get_gesture (GTK_BUTTON (priv->plus_button)),
                    "pressed", G_CALLBACK (button_pressed_cb), button);
  g_signal_connect (gtk_button_get_gesture (GTK_BUTTON (priv->minus_button)),
                    "pressed", G_CALLBACK (button_pressed_cb), button);
}

static void
gtk_scale_button_constructed (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->constructed (object);

  /* set button text and size */
  gtk_scale_button_update_icon (button);
}

static void
gtk_scale_button_set_property (GObject       *object,
			       guint          prop_id,
			       const GValue  *value,
			       GParamSpec    *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_scale_button_set_orientation_private (button, g_value_get_enum (value));
      break;
    case PROP_VALUE:
      gtk_scale_button_set_value (button, g_value_get_double (value));
      break;
    case PROP_ADJUSTMENT:
      gtk_scale_button_set_adjustment (button, g_value_get_object (value));
      break;
    case PROP_ICONS:
      gtk_scale_button_set_icons (button,
                                  (const char **)g_value_get_boxed (value));
      break;
    case PROP_HAS_FRAME:
      gtk_scale_button_set_has_frame (button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_button_get_property (GObject     *object,
			       guint        prop_id,
			       GValue      *value,
			       GParamSpec  *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_VALUE:
      g_value_set_double (value, gtk_scale_button_get_value (button));
      break;
    case PROP_ADJUSTMENT:
      g_value_set_object (value, gtk_scale_button_get_adjustment (button));
      break;
    case PROP_ICONS:
      g_value_set_boxed (value, priv->icon_list);
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, gtk_scale_button_get_active (button));
      break;
    case PROP_HAS_FRAME:
      g_value_set_boolean (value, gtk_scale_button_get_has_frame (button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_button_finalize (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_clear_pointer (&priv->icon_list, g_strfreev);
  g_clear_object (&priv->adjustment);
  g_clear_handle_id (&priv->autoscroll_timeout, g_source_remove);

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->finalize (object);
}

static void
gtk_scale_button_dispose (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_clear_pointer (&priv->dock, gtk_widget_unparent);
  g_clear_pointer (&priv->button, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->dispose (object);
}

/**
 * gtk_scale_button_new:
 * @min: the minimum value of the scale (usually 0)
 * @max: the maximum value of the scale (usually 100)
 * @step: the stepping of value when a scroll-wheel event,
 *   or up/down arrow event occurs (usually 2)
 * @icons: (nullable) (array zero-terminated=1): a %NULL-terminated
 *   array of icon names, or %NULL if you want to set the list
 *   later with gtk_scale_button_set_icons()
 *
 * Creates a `GtkScaleButton`.
 *
 * The new scale button has a range between @min and @max,
 * with a stepping of @step.
 *
 * Returns: a new `GtkScaleButton`
 */
GtkWidget *
gtk_scale_button_new (double        min,
		      double        max,
		      double        step,
		      const char **icons)
{
  GtkScaleButton *button;
  GtkAdjustment *adjustment;

  adjustment = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  button = g_object_new (GTK_TYPE_SCALE_BUTTON,
                         "adjustment", adjustment,
                         "icons", icons,
                         NULL);

  return GTK_WIDGET (button);
}

/**
 * gtk_scale_button_get_value: (attributes org.gtk.Method.get_property=value)
 * @button: a `GtkScaleButton`
 *
 * Gets the current value of the scale button.
 *
 * Returns: current value of the scale button
 */
double
gtk_scale_button_get_value (GtkScaleButton * button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), 0);

  return gtk_adjustment_get_value (priv->adjustment);
}

/**
 * gtk_scale_button_set_value: (attributes org.gtk.Method.set_property=value)
 * @button: a `GtkScaleButton`
 * @value: new value of the scale button
 *
 * Sets the current value of the scale.
 *
 * If the value is outside the minimum or maximum range values,
 * it will be clamped to fit inside them.
 *
 * The scale button emits the [signal@Gtk.ScaleButton::value-changed]
 * signal if the value changes.
 */
void
gtk_scale_button_set_value (GtkScaleButton *button,
			    double          value)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  gtk_range_set_value (GTK_RANGE (priv->scale), value);
  g_object_notify (G_OBJECT (button), "value");
}

/**
 * gtk_scale_button_set_icons: (attributes org.gtk.Method.set_property=icons)
 * @button: a `GtkScaleButton`
 * @icons: (array zero-terminated=1): a %NULL-terminated array of icon names
 *
 * Sets the icons to be used by the scale button.
 */
void
gtk_scale_button_set_icons (GtkScaleButton  *button,
			    const char     **icons)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  char **tmp;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  tmp = priv->icon_list;
  priv->icon_list = g_strdupv ((char **) icons);
  g_strfreev (tmp);
  gtk_scale_button_update_icon (button);

  g_object_notify (G_OBJECT (button), "icons");
}

/**
 * gtk_scale_button_get_adjustment: (attributes org.gtk.Method.get_property=adjustment)
 * @button: a `GtkScaleButton`
 *
 * Gets the `GtkAdjustment` associated with the `GtkScaleButton`’s scale.
 *
 * See [method@Gtk.Range.get_adjustment] for details.
 *
 * Returns: (transfer none): the adjustment associated with the scale
 */
GtkAdjustment*
gtk_scale_button_get_adjustment	(GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return priv->adjustment;
}

/**
 * gtk_scale_button_set_adjustment: (attributes org.gtk.Method.set_property=adjustment)
 * @button: a `GtkScaleButton`
 * @adjustment: a `GtkAdjustment`
 *
 * Sets the `GtkAdjustment` to be used as a model
 * for the `GtkScaleButton`’s scale.
 *
 * See [method@Gtk.Range.set_adjustment] for details.
 */
void
gtk_scale_button_set_adjustment	(GtkScaleButton *button,
				 GtkAdjustment  *adjustment)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (priv->adjustment != adjustment)
    {
      if (priv->adjustment)
        g_object_unref (priv->adjustment);
      priv->adjustment = g_object_ref_sink (adjustment);

      if (priv->scale)
        gtk_range_set_adjustment (GTK_RANGE (priv->scale), adjustment);

      g_object_notify (G_OBJECT (button), "adjustment");

      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, gtk_adjustment_get_upper (adjustment),
                                      GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, gtk_adjustment_get_lower (adjustment),
                                      GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, gtk_adjustment_get_value (adjustment),
                                      -1);

    }
}

/**
 * gtk_scale_button_get_plus_button:
 * @button: a `GtkScaleButton`
 *
 * Retrieves the plus button of the `GtkScaleButton.`
 *
 * Returns: (transfer none) (type Gtk.Button): the plus button
 *   of the `GtkScaleButton`
 */
GtkWidget *
gtk_scale_button_get_plus_button (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return priv->plus_button;
}

/**
 * gtk_scale_button_get_minus_button:
 * @button: a `GtkScaleButton`
 *
 * Retrieves the minus button of the `GtkScaleButton`.
 *
 * Returns: (transfer none) (type Gtk.Button): the minus button
 *   of the `GtkScaleButton`
 */
GtkWidget *
gtk_scale_button_get_minus_button (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return priv->minus_button;
}

/**
 * gtk_scale_button_get_popup:
 * @button: a `GtkScaleButton`
 *
 * Retrieves the popup of the `GtkScaleButton`.
 *
 * Returns: (transfer none): the popup of the `GtkScaleButton`
 */
GtkWidget *
gtk_scale_button_get_popup (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return priv->dock;
}

/**
 * gtk_scale_button_get_active: (attributes org.gtk.Method.get_property=active)
 * @button: a `GtkScaleButton`
 *
 * Queries a `GtkScaleButton` and returns its current state.
 *
 * Returns %TRUE if the scale button is pressed in and %FALSE
 * if it is raised.
 *
 * Returns: whether the button is pressed
 *
 * Since: 4.10
 */
gboolean
gtk_scale_button_get_active (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button));
}

/**
 * gtk_scale_button_get_has_frame: (attributes org.gtk.Method.get_property=has-frame)
 * @button: a `GtkScaleButton`
 *
 * Returns whether the button has a frame.
 *
 * Returns: %TRUE if the button has a frame
 *
 * Since: 4.14
 */
gboolean
gtk_scale_button_get_has_frame (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), TRUE);

  return gtk_button_get_has_frame (GTK_BUTTON (priv->button));
}

/**
 * gtk_scale_button_set_has_frame: (attributes org.gtk.Method.set_property=has-frame)
 * @button: a `GtkScaleButton`
 * @has_frame: whether the button should have a visible frame
 *
 * Sets the style of the button.
 *
 * Since: 4.14
 */
void
gtk_scale_button_set_has_frame (GtkScaleButton *button,
                                gboolean        has_frame)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  if (gtk_button_get_has_frame (GTK_BUTTON (priv->button)) == has_frame)
    return;

  gtk_button_set_has_frame (GTK_BUTTON (priv->button), has_frame);
  g_object_notify (G_OBJECT (button), "has-frame");
}

static void
apply_orientation (GtkScaleButton *button,
                   GtkOrientation  orientation)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  if (priv->applied_orientation != orientation)
    {
      priv->applied_orientation = orientation;

      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->box), orientation);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->scale), orientation);

      if (orientation == GTK_ORIENTATION_VERTICAL)
        {
          gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->scale,
                                                            priv->plus_button);
          gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->minus_button,
                                                            priv->scale);
          gtk_widget_set_size_request (GTK_WIDGET (priv->scale), -1, SCALE_SIZE);
          gtk_range_set_inverted (GTK_RANGE (priv->scale), TRUE);
        }
      else
        {
          gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->scale,
                                                            priv->minus_button);
          gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->plus_button,
                                                            priv->scale);
          gtk_widget_set_size_request (GTK_WIDGET (priv->scale), SCALE_SIZE, -1);
          gtk_range_set_inverted (GTK_RANGE (priv->scale), FALSE);
        }
    }
}

static void
gtk_scale_button_set_orientation_private (GtkScaleButton *button,
                                          GtkOrientation  orientation)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;

      apply_orientation (button, priv->orientation);

      g_object_notify (G_OBJECT (button), "orientation");
    }
}

static gboolean
gtk_scale_button_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                           double                    dx,
                                           double                    dy,
                                           GtkScaleButton           *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  GtkAdjustment *adjustment;
  double d;

  adjustment = priv->adjustment;

  d = CLAMP (gtk_scale_button_get_value (button) -
             (dy * gtk_adjustment_get_step_increment (adjustment)),
             gtk_adjustment_get_lower (adjustment),
             gtk_adjustment_get_upper (adjustment));

  gtk_scale_button_set_value (button, d);

  return GDK_EVENT_STOP;
}

/*
 * button callbacks.
 */

static void
gtk_scale_popup (GtkWidget *widget)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (widget);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  apply_orientation (button, priv->orientation);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button), TRUE);
}

static void
gtk_scale_button_popdown (GtkWidget *widget)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (widget);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button), FALSE);
}

static void
gtk_scale_button_popup (GtkWidget *widget)
{
  gtk_scale_popup (widget);
}

/*
 * +/- button callbacks.
 */
static gboolean
button_click (GtkScaleButton *button,
              GtkWidget      *active)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  GtkAdjustment *adjustment = priv->adjustment;
  gboolean can_continue = TRUE;
  double val;

  val = gtk_scale_button_get_value (button);

  if (active == priv->plus_button)
    val += gtk_adjustment_get_page_increment (adjustment);
  else
    val -= gtk_adjustment_get_page_increment (adjustment);

  if (val <= gtk_adjustment_get_lower (adjustment))
    {
      can_continue = FALSE;
      val = gtk_adjustment_get_lower (adjustment);
    }
  else if (val > gtk_adjustment_get_upper (adjustment))
    {
      can_continue = FALSE;
      val = gtk_adjustment_get_upper (adjustment);
    }

  gtk_scale_button_set_value (button, val);

  return can_continue;
}

static void
cb_button_clicked (GtkWidget *widget,
                   gpointer   user_data)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (user_data);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  if (priv->autoscroll_timeout)
    {
      g_source_remove (priv->autoscroll_timeout);
      priv->autoscroll_timeout = 0;
    }

  if (priv->autoscrolling)
    {
      gtk_range_stop_autoscroll (GTK_RANGE (priv->scale));
      priv->autoscrolling = FALSE;
      return;
    }

  button_click (button, widget);
}

static void
gtk_scale_button_update_icon (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  GtkAdjustment *adjustment;
  double value;
  const char *name;
  guint num_icons;

  if (!priv->icon_list || !priv->icon_list[0] || priv->icon_list[0][0] == '\0')
    {
      gtk_button_set_icon_name (GTK_BUTTON (priv->button), "image-missing");
      return;
    }

  num_icons = g_strv_length (priv->icon_list);

  /* The 1-icon special case */
  if (num_icons == 1)
    {
      gtk_button_set_icon_name (GTK_BUTTON (priv->button), priv->icon_list[0]);
      return;
    }

  adjustment = priv->adjustment;
  value = gtk_scale_button_get_value (button);

  /* The 2-icons special case */
  if (num_icons == 2)
    {
      double limit;

      limit = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) / 2 + gtk_adjustment_get_lower (adjustment);
      if (value < limit)
        name = priv->icon_list[0];
      else
        name = priv->icon_list[1];

      gtk_button_set_icon_name (GTK_BUTTON (priv->button), name);
      return;
    }

  /* With 3 or more icons */
  if (value == gtk_adjustment_get_lower (adjustment))
    {
      name = priv->icon_list[0];
    }
  else if (value == gtk_adjustment_get_upper (adjustment))
    {
      name = priv->icon_list[1];
    }
  else
    {
      double step;
      guint i;

      step = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) / (num_icons - 2); i = (guint) ((value - gtk_adjustment_get_lower (adjustment)) / step) + 2;
      g_assert (i < num_icons);
      name = priv->icon_list[i];
    }

  gtk_button_set_icon_name (GTK_BUTTON (priv->button), name);
}

static void
cb_scale_value_changed (GtkRange *range,
                        gpointer  user_data)
{
  GtkScaleButton *button = user_data;
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);
  double value;
  double upper, lower;

  value = gtk_range_get_value (range);
  upper = gtk_adjustment_get_upper (priv->adjustment);
  lower = gtk_adjustment_get_lower (priv->adjustment);

  gtk_scale_button_update_icon (button);

  gtk_widget_set_sensitive (priv->plus_button, value < upper);
  gtk_widget_set_sensitive (priv->minus_button, lower < value);

  g_signal_emit (button, signals[VALUE_CHANGED], 0, value);
  g_object_notify (G_OBJECT (button), "value");

  gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, value,
                                  -1);
}

static void
cb_popup_mapped (GtkWidget *popup,
                 gpointer   user_data)
{
  GtkScaleButton *button = user_data;
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  gtk_widget_grab_focus (priv->scale);
}

static void
gtk_scale_button_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (widget);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  gtk_widget_measure (priv->button,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

}

static void
gtk_scale_button_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (widget);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (button);

  gtk_widget_size_allocate (priv->button,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);

  gtk_popover_present (GTK_POPOVER (priv->dock));
}
