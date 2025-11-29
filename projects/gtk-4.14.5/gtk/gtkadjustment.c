/* GTK - The GIMP Toolkit
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

#include "config.h"
#include "gtkadjustment.h"
#include "gtkadjustmentprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"


/**
 * GtkAdjustment:
 *
 * `GtkAdjustment` is a model for a numeric value.
 *
 * The `GtkAdjustment` has an associated lower and upper bound.
 * It also contains step and page increments, and a page size.
 *
 * Adjustments are used within several GTK widgets, including
 * [class@Gtk.SpinButton], [class@Gtk.Viewport], [class@Gtk.Scrollbar]
 * and [class@Gtk.Scale].
 *
 * The `GtkAdjustment` object does not update the value itself. Instead
 * it is left up to the owner of the `GtkAdjustment` to control the value.
 */


struct _GtkAdjustmentPrivate {
  double lower;
  double upper;
  double value;
  double step_increment;
  double page_increment;
  double page_size;

  double source;
  double target;

  guint duration;
  guint tick_id;
  gint64 start_time;
  gint64 end_time;
  GdkFrameClock *clock;
};
typedef struct _GtkAdjustmentPrivate GtkAdjustmentPrivate;

enum
{
  PROP_0,
  PROP_VALUE,
  PROP_LOWER,
  PROP_UPPER,
  PROP_STEP_INCREMENT,
  PROP_PAGE_INCREMENT,
  PROP_PAGE_SIZE,
  NUM_PROPERTIES
};

enum
{
  CHANGED,
  VALUE_CHANGED,
  LAST_SIGNAL
};


static void gtk_adjustment_get_property                (GObject      *object,
                                                        guint         prop_id,
                                                        GValue       *value,
                                                        GParamSpec   *pspec);
static void gtk_adjustment_set_property                (GObject      *object,
                                                        guint         prop_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void gtk_adjustment_dispatch_properties_changed (GObject      *object,
                                                        guint         n_pspecs,
                                                        GParamSpec  **pspecs);

static guint adjustment_signals[LAST_SIGNAL] = { 0 };

static GParamSpec *adjustment_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkAdjustment, gtk_adjustment, G_TYPE_INITIALLY_UNOWNED)

static void
gtk_adjustment_finalize (GObject *object)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  if (priv->tick_id)
    g_signal_handler_disconnect (priv->clock, priv->tick_id);
  if (priv->clock)
    g_object_unref (priv->clock);

  G_OBJECT_CLASS (gtk_adjustment_parent_class)->finalize (object);
}

static void
gtk_adjustment_class_init (GtkAdjustmentClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize                    = gtk_adjustment_finalize;
  gobject_class->set_property                = gtk_adjustment_set_property;
  gobject_class->get_property                = gtk_adjustment_get_property;
  gobject_class->dispatch_properties_changed = gtk_adjustment_dispatch_properties_changed;

  class->changed = NULL;
  class->value_changed = NULL;

  /**
   * GtkAdjustment:value: (attributes org.gtk.Property.get=gtk_adjustment_get_value org.gtk.Property.set=gtk_adjustment_set_value)
   *
   * The value of the adjustment.
   */
  adjustment_props[PROP_VALUE] =
      g_param_spec_double ("value", NULL, NULL,
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  /**
   * GtkAdjustment:lower: (attributes org.gtk.Property.get=gtk_adjustment_get_lower org.gtk.Property.set=gtk_adjustment_set_lower)
   *
   * The minimum value of the adjustment.
   */
  adjustment_props[PROP_LOWER] =
      g_param_spec_double ("lower", NULL, NULL,
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  /**
   * GtkAdjustment:upper: (attributes org.gtk.Property.get=gtk_adjustment_get_upper org.gtk.Property.set=gtk_adjustment_set_upper)
   *
   * The maximum value of the adjustment.
   *
   * Note that values will be restricted by `upper - page-size` if the page-size
   * property is nonzero.
   */
  adjustment_props[PROP_UPPER] =
      g_param_spec_double ("upper", NULL, NULL,
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  /**
   * GtkAdjustment:step-increment: (attributes org.gtk.Property.get=gtk_adjustment_get_step_increment org.gtk.Property.set=gtk_adjustment_set_step_increment)
   *
   * The step increment of the adjustment.
   */
  adjustment_props[PROP_STEP_INCREMENT] =
      g_param_spec_double ("step-increment", NULL, NULL,
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  /**
   * GtkAdjustment:page-increment: (attributes org.gtk.Property.get=gtk_adjustment_get_page_increment org.gtk.Property.set=gtk_adjustment_set_page_increment)
   *
   * The page increment of the adjustment.
   */
  adjustment_props[PROP_PAGE_INCREMENT] =
      g_param_spec_double ("page-increment", NULL, NULL,
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  /**
   * GtkAdjustment:page-size: (attributes org.gtk.Property.get=gtk_adjustment_get_page_size org.gtk.Property.set=gtk_adjustment_set_page_size)
   *
   * The page size of the adjustment.
   *
   * Note that the page-size is irrelevant and should be set to zero
   * if the adjustment is used for a simple scalar value, e.g. in a
   * `GtkSpinButton`.
   */
  adjustment_props[PROP_PAGE_SIZE] =
      g_param_spec_double ("page-size", NULL, NULL,
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, adjustment_props);

  /**
   * GtkAdjustment::changed:
   * @adjustment: the object which received the signal
   *
   * Emitted when one or more of the `GtkAdjustment` properties have been
   * changed.
   *
   * Note that the [property@Gtk.Adjustment:value] property is
   * covered by the [signal@Gtk.Adjustment::value-changed] signal.
   */
  adjustment_signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkAdjustment::value-changed:
   * @adjustment: the object which received the signal
   *
   * Emitted when the value has been changed.
   */
  adjustment_signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, value_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
}

static void
gtk_adjustment_init (GtkAdjustment *adjustment)
{
}

static void
gtk_adjustment_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_double (value, priv->value);
      break;
    case PROP_LOWER:
      g_value_set_double (value, priv->lower);
      break;
    case PROP_UPPER:
      g_value_set_double (value, priv->upper);
      break;
    case PROP_STEP_INCREMENT:
      g_value_set_double (value, priv->step_increment);
      break;
    case PROP_PAGE_INCREMENT:
      g_value_set_double (value, priv->page_increment);
      break;
    case PROP_PAGE_SIZE:
      g_value_set_double (value, priv->page_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_adjustment_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  double double_value = g_value_get_double (value);

  switch (prop_id)
    {
    case PROP_VALUE:
      gtk_adjustment_set_value (adjustment, double_value);
      break;
    case PROP_LOWER:
      gtk_adjustment_set_lower (adjustment, double_value);
      break;
    case PROP_UPPER:
      gtk_adjustment_set_upper (adjustment, double_value);
      break;
    case PROP_STEP_INCREMENT:
      gtk_adjustment_set_step_increment (adjustment, double_value);
      break;
    case PROP_PAGE_INCREMENT:
      gtk_adjustment_set_page_increment (adjustment, double_value);
      break;
    case PROP_PAGE_SIZE:
      gtk_adjustment_set_page_size (adjustment, double_value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static inline void
emit_value_changed (GtkAdjustment *adjustment)
{
  g_signal_emit (adjustment, adjustment_signals[VALUE_CHANGED], 0);
  g_object_notify_by_pspec (G_OBJECT (adjustment), adjustment_props[PROP_VALUE]);
}

static void
gtk_adjustment_dispatch_properties_changed (GObject     *object,
                                            guint        n_pspecs,
                                            GParamSpec **pspecs)
{
  gboolean changed = FALSE;
  int i;

  G_OBJECT_CLASS (gtk_adjustment_parent_class)->dispatch_properties_changed (object, n_pspecs, pspecs);

  for (i = 0; i < n_pspecs; i++)
    switch (pspecs[i]->param_id)
      {
      case PROP_LOWER:
      case PROP_UPPER:
      case PROP_STEP_INCREMENT:
      case PROP_PAGE_INCREMENT:
      case PROP_PAGE_SIZE:
        changed = TRUE;
        break;
      default:
        break;
      }

  if (changed)
    {
      g_signal_emit (object, adjustment_signals[CHANGED], 0);
    }
}

static double
gtk_adjustment_sanitize_value (GtkAdjustment *self,
                               double         value)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (self);

  /* don't use CLAMP() so we don't end up below lower if upper - page_size
   * is smaller than lower
   */
  value = MIN (value, priv->upper - priv->page_size);
  value = MAX (value, priv->lower);

  return value;
}

/**
 * gtk_adjustment_new:
 * @value: the initial value
 * @lower: the minimum value
 * @upper: the maximum value
 * @step_increment: the step increment
 * @page_increment: the page increment
 * @page_size: the page size
 *
 * Creates a new `GtkAdjustment`.
 *
 * Returns: a new `GtkAdjustment`
 */
GtkAdjustment *
gtk_adjustment_new (double value,
		    double lower,
		    double upper,
		    double step_increment,
		    double page_increment,
		    double page_size)
{
  return g_object_new (GTK_TYPE_ADJUSTMENT,
		       "lower", lower,
		       "upper", upper,
		       "step-increment", step_increment,
		       "page-increment", page_increment,
		       "page-size", page_size,
		       "value", value,
		       NULL);
}

/**
 * gtk_adjustment_get_value: (attributes org.gtk.Method.get_property=value)
 * @adjustment: a `GtkAdjustment`
 *
 * Gets the current value of the adjustment.
 *
 * Returns: The current value of the adjustment
 */
double
gtk_adjustment_get_value (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return priv->value;
}

double
gtk_adjustment_get_target_value (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  if (priv->tick_id)
    return priv->target;
  else
    return priv->value;
}

static void
adjustment_set_value (GtkAdjustment *adjustment,
                      double         value)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  value = gtk_adjustment_sanitize_value (adjustment, value);

  if (priv->value != value)
    {
      priv->value = value;
      emit_value_changed (adjustment);
    }
}

static void gtk_adjustment_on_frame_clock_update (GdkFrameClock *clock,
                                                  GtkAdjustment *adjustment);

static void
gtk_adjustment_begin_updating (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  if (priv->tick_id == 0)
    {
      priv->tick_id = g_signal_connect (priv->clock, "update",
                                        G_CALLBACK (gtk_adjustment_on_frame_clock_update), adjustment);
      gdk_frame_clock_begin_updating (priv->clock);
    }
}

static void
gtk_adjustment_end_updating (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  if (priv->tick_id != 0)
    {
      g_signal_handler_disconnect (priv->clock, priv->tick_id);
      priv->tick_id = 0;
      gdk_frame_clock_end_updating (priv->clock);
    }
}

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static double
ease_out_cubic (double t)
{
  double p = t - 1;

  return p * p * p + 1;
}

static void
gtk_adjustment_on_frame_clock_update (GdkFrameClock *clock,
                                      GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);
  gint64 now;

  now = gdk_frame_clock_get_frame_time (clock);

  if (now < priv->end_time)
    {
      double t;

      t = (now - priv->start_time) / (double) (priv->end_time - priv->start_time);
      t = ease_out_cubic (t);
      adjustment_set_value (adjustment, priv->source + t * (priv->target - priv->source));
    }
  else
    {
      adjustment_set_value (adjustment, priv->target);
      gtk_adjustment_end_updating (adjustment);
    }
}

static void
gtk_adjustment_set_value_internal (GtkAdjustment *adjustment,
                                   double         value,
                                   gboolean       animate)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  value = gtk_adjustment_sanitize_value (adjustment, value);

  if (animate && priv->duration != 0 && priv->clock != NULL)
    {
      if (priv->tick_id && priv->target == value)
        return;

      priv->source = priv->value;
      priv->target = value;
      priv->start_time = gdk_frame_clock_get_frame_time (priv->clock);
      priv->end_time = priv->start_time + 1000 * priv->duration;
      gtk_adjustment_begin_updating (adjustment);
    }
  else
    {
      gtk_adjustment_end_updating (adjustment);
      adjustment_set_value (adjustment, value);
    }
}

/**
 * gtk_adjustment_set_value: (attributes org.gtk.Method.set_property=value)
 * @adjustment: a `GtkAdjustment`
 * @value: the new value
 *
 * Sets the `GtkAdjustment` value.
 *
 * The value is clamped to lie between [property@Gtk.Adjustment:lower]
 * and [property@Gtk.Adjustment:upper].
 *
 * Note that for adjustments which are used in a `GtkScrollbar`,
 * the effective range of allowed values goes from
 * [property@Gtk.Adjustment:lower] to
 * [property@Gtk.Adjustment:upper] - [property@Gtk.Adjustment:page-size].
 */
void
gtk_adjustment_set_value (GtkAdjustment *adjustment,
			  double         value)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  gtk_adjustment_set_value_internal (adjustment, value, FALSE);
}

void
gtk_adjustment_animate_to_value (GtkAdjustment *adjustment,
			         double         value)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  gtk_adjustment_set_value_internal (adjustment, value, TRUE);
}

/**
 * gtk_adjustment_get_lower: (attributes org.gtk.Method.get_property=lower)
 * @adjustment: a `GtkAdjustment`
 *
 * Retrieves the minimum value of the adjustment.
 *
 * Returns: The current minimum value of the adjustment
 **/
double
gtk_adjustment_get_lower (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return priv->lower;
}

/**
 * gtk_adjustment_set_lower: (attributes org.gtk.Method.set_property=lower)
 * @adjustment: a `GtkAdjustment`
 * @lower: the new minimum value
 *
 * Sets the minimum value of the adjustment.
 *
 * When setting multiple adjustment properties via their individual
 * setters, multiple [signal@Gtk.Adjustment::changed] signals will
 * be emitted. However, since the emission of the
 * [signal@Gtk.Adjustment::changed] signal is tied to the emission
 * of the ::notify signals of the changed properties, it’s possible
 * to compress the [signal@Gtk.Adjustment::changed] signals into one
 * by calling g_object_freeze_notify() and g_object_thaw_notify()
 * around the calls to the individual setters.
 *
 * Alternatively, using a single g_object_set() for all the properties
 * to change, or using [method@Gtk.Adjustment.configure] has the same effect.
 */
void
gtk_adjustment_set_lower (GtkAdjustment *adjustment,
                          double         lower)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (lower != priv->lower)
    {
      priv->lower = lower;
      g_object_notify_by_pspec (G_OBJECT (adjustment), adjustment_props[PROP_LOWER]);
    }
}

/**
 * gtk_adjustment_get_upper: (attributes org.gtk.Method.get_property=upper)
 * @adjustment: a `GtkAdjustment`
 *
 * Retrieves the maximum value of the adjustment.
 *
 * Returns: The current maximum value of the adjustment
 */
double
gtk_adjustment_get_upper (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return priv->upper;
}

/**
 * gtk_adjustment_set_upper: (attributes org.gtk.Method.set_property=upper)
 * @adjustment: a `GtkAdjustment`
 * @upper: the new maximum value
 *
 * Sets the maximum value of the adjustment.
 *
 * Note that values will be restricted by `upper - page-size`
 * if the page-size property is nonzero.
 *
 * See [method@Gtk.Adjustment.set_lower] about how to compress
 * multiple emissions of the [signal@Gtk.Adjustment::changed]
 * signal when setting multiple adjustment properties.
 */
void
gtk_adjustment_set_upper (GtkAdjustment *adjustment,
                          double         upper)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (upper != priv->upper)
    {
      priv->upper = upper;
      g_object_notify_by_pspec (G_OBJECT (adjustment), adjustment_props[PROP_UPPER]);
    }
}

/**
 * gtk_adjustment_get_step_increment: (attributes org.gtk.Method.get_property=step-increment)
 * @adjustment: a `GtkAdjustment`
 *
 * Retrieves the step increment of the adjustment.
 *
 * Returns: The current step increment of the adjustment.
 */
double
gtk_adjustment_get_step_increment (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return priv->step_increment;
}

/**
 * gtk_adjustment_set_step_increment: (attributes org.gtk.Method.set_property=step-increment)
 * @adjustment: a `GtkAdjustment`
 * @step_increment: the new step increment
 *
 * Sets the step increment of the adjustment.
 *
 * See [method@Gtk.Adjustment.set_lower] about how to compress
 * multiple emissions of the [signal@Gtk.Adjustment::changed]
 * signal when setting multiple adjustment properties.
 */
void
gtk_adjustment_set_step_increment (GtkAdjustment *adjustment,
                                   double         step_increment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (step_increment != priv->step_increment)
    {
      priv->step_increment = step_increment;
      g_object_notify_by_pspec (G_OBJECT (adjustment), adjustment_props[PROP_STEP_INCREMENT]);
    }
}

/**
 * gtk_adjustment_get_page_increment: (attributes org.gtk.Method.get_property=page-increment)
 * @adjustment: a `GtkAdjustment`
 *
 * Retrieves the page increment of the adjustment.
 *
 * Returns: The current page increment of the adjustment
 **/
double
gtk_adjustment_get_page_increment (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return priv->page_increment;
}

/**
 * gtk_adjustment_set_page_increment: (attributes org.gtk.Method.set_property=page-increment)
 * @adjustment: a `GtkAdjustment`
 * @page_increment: the new page increment
 *
 * Sets the page increment of the adjustment.
 *
 * See [method@Gtk.Adjustment.set_lower] about how to compress
 * multiple emissions of the [signal@Gtk.Adjustment::changed]
 * signal when setting multiple adjustment properties.
 */
void
gtk_adjustment_set_page_increment (GtkAdjustment *adjustment,
                                   double         page_increment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (page_increment != priv->page_increment)
    {
      priv->page_increment = page_increment;
      g_object_notify_by_pspec (G_OBJECT (adjustment), adjustment_props[PROP_PAGE_INCREMENT]);
    }
}

/**
 * gtk_adjustment_get_page_size: (attributes org.gtk.Method.get_property=page-size)
 * @adjustment: a `GtkAdjustment`
 *
 * Retrieves the page size of the adjustment.
 *
 * Returns: The current page size of the adjustment
 **/
double
gtk_adjustment_get_page_size (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return priv->page_size;
}

/**
 * gtk_adjustment_set_page_size: (attributes org.gtk.Method.set_property=page-size)
 * @adjustment: a `GtkAdjustment`
 * @page_size: the new page size
 *
 * Sets the page size of the adjustment.
 *
 * See [method@Gtk.Adjustment.set_lower] about how to compress
 * multiple emissions of the [signal@Gtk.Adjustment::changed]
 * signal when setting multiple adjustment properties.
 */
void
gtk_adjustment_set_page_size (GtkAdjustment *adjustment,
                              double         page_size)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (page_size != priv->page_size)
    {
      priv->page_size = page_size;
      g_object_notify_by_pspec (G_OBJECT (adjustment), adjustment_props[PROP_PAGE_SIZE]);
    }
}

/**
 * gtk_adjustment_configure:
 * @adjustment: a `GtkAdjustment`
 * @value: the new value
 * @lower: the new minimum value
 * @upper: the new maximum value
 * @step_increment: the new step increment
 * @page_increment: the new page increment
 * @page_size: the new page size
 *
 * Sets all properties of the adjustment at once.
 *
 * Use this function to avoid multiple emissions of the
 * [signal@Gtk.Adjustment::changed] signal. See
 * [method@Gtk.Adjustment.set_lower] for an alternative
 * way of compressing multiple emissions of
 * [signal@Gtk.Adjustment::changed] into one.
 */
void
gtk_adjustment_configure (GtkAdjustment *adjustment,
                          double         value,
                          double         lower,
                          double         upper,
                          double         step_increment,
                          double         page_increment,
                          double         page_size)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);
  gboolean value_changed = FALSE;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  g_object_freeze_notify (G_OBJECT (adjustment));

  gtk_adjustment_set_lower (adjustment, lower);
  gtk_adjustment_set_upper (adjustment, upper);
  gtk_adjustment_set_step_increment (adjustment, step_increment);
  gtk_adjustment_set_page_increment (adjustment, page_increment);
  gtk_adjustment_set_page_size (adjustment, page_size);

  value = gtk_adjustment_sanitize_value (adjustment, value);

  if (value != priv->value)
    {
      /* set value manually to make sure "changed" is emitted with the
       * new value in place and is emitted before "value-changed"
       */
      priv->value = value;
      value_changed = TRUE;
    }

  /* The dispatch_properties_changed implementation will emit ::changed! */
  g_object_thaw_notify (G_OBJECT (adjustment));

  if (value_changed)
    emit_value_changed (adjustment);
}

/**
 * gtk_adjustment_clamp_page:
 * @adjustment: a `GtkAdjustment`
 * @lower: the lower value
 * @upper: the upper value
 *
 * Updates the value property to ensure that the range
 * between @lower and @upper is in the current page.
 *
 * The current page goes from `value` to `value` + `page-size`.
 * If the range is larger than the page size, then only the
 * start of it will be in the current page.
 *
 * A [signal@Gtk.Adjustment::value-changed] signal will be emitted
 * if the value is changed.
 */
void
gtk_adjustment_clamp_page (GtkAdjustment *adjustment,
			   double         lower,
			   double         upper)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);
  gboolean need_emission;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  lower = CLAMP (lower, priv->lower, priv->upper);
  upper = CLAMP (upper, priv->lower, priv->upper);

  need_emission = FALSE;

  if (priv->value + priv->page_size < upper)
    {
      priv->value = upper - priv->page_size;
      need_emission = TRUE;
    }
  if (priv->value > lower)
    {
      priv->value = lower;
      need_emission = TRUE;
    }

  if (need_emission)
    emit_value_changed (adjustment);
}

/**
 * gtk_adjustment_get_minimum_increment:
 * @adjustment: a `GtkAdjustment`
 *
 * Gets the smaller of step increment and page increment.
 *
 * Returns: the minimum increment of @adjustment
 */
double
gtk_adjustment_get_minimum_increment (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);
  double minimum_increment;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0);

    if (priv->step_increment != 0 && priv->page_increment != 0)
    {
      if (ABS (priv->step_increment) < ABS (priv->page_increment))
        minimum_increment = priv->step_increment;
      else
        minimum_increment = priv->page_increment;
    }
  else if (priv->step_increment == 0 && priv->page_increment == 0)
    {
      minimum_increment = 0;
    }
  else if (priv->step_increment == 0)
    {
      minimum_increment = priv->page_increment;
    }
  else
    {
      minimum_increment = priv->step_increment;
    }

  return minimum_increment;
}

void
gtk_adjustment_enable_animation (GtkAdjustment *adjustment,
                                 GdkFrameClock *clock,
                                 guint          duration)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  if (priv->clock != clock)
    {
      if (priv->tick_id)
        {
          adjustment_set_value (adjustment, priv->target);

          g_signal_handler_disconnect (priv->clock, priv->tick_id);
          priv->tick_id = 0;
          gdk_frame_clock_end_updating (priv->clock);
        }

      if (priv->clock)
        g_object_unref (priv->clock);

      priv->clock = clock;

      if (priv->clock)
        g_object_ref (priv->clock);
    }

  priv->duration = duration;
}

guint
gtk_adjustment_get_animation_duration (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  return priv->duration;
}

gboolean
gtk_adjustment_is_animating (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = gtk_adjustment_get_instance_private (adjustment);

  return priv->tick_id != 0;
}
