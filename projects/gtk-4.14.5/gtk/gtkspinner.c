/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2007 John Stowers, Neil Jagdish Patel.
 * Copyright (C) 2009 Bastien Nocera, David Zeuthen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Code adapted from egg-spinner
 * by Christian Hergert <christian.hergert@gmail.com>
 */

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkspinner.h"

#include "gtkimage.h"
#include "gtkprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkrendericonprivate.h"


/**
 * GtkSpinner:
 *
 * A `GtkSpinner` widget displays an icon-size spinning animation.
 *
 * It is often used as an alternative to a [class@Gtk.ProgressBar]
 * for displaying indefinite activity, instead of actual progress.
 *
 * ![An example GtkSpinner](spinner.png)
 *
 * To start the animation, use [method@Gtk.Spinner.start], to stop it
 * use [method@Gtk.Spinner.stop].
 *
 * # CSS nodes
 *
 * `GtkSpinner` has a single CSS node with the name spinner.
 * When the animation is active, the :checked pseudoclass is
 * added to this node.
 */

typedef struct _GtkSpinnerClass GtkSpinnerClass;

struct _GtkSpinner
{
  GtkWidget parent;

  guint spinning : 1;
};

struct _GtkSpinnerClass
{
  GtkWidgetClass parent_class;
};


enum {
  PROP_0,
  PROP_SPINNING
};

G_DEFINE_TYPE (GtkSpinner, gtk_spinner, GTK_TYPE_WIDGET)

#define DEFAULT_SIZE 16

static void
update_state_flags (GtkSpinner *spinner)
{
  if (spinner->spinning && gtk_widget_get_mapped (GTK_WIDGET (spinner)))
    gtk_widget_set_state_flags (GTK_WIDGET (spinner),
                                GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (spinner),
                                  GTK_STATE_FLAG_CHECKED);
}

static void
gtk_spinner_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GtkCssStyle *style;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  *minimum = *natural = _gtk_css_number_value_get (style->icon->icon_size, 100);
}

static void
gtk_spinner_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  gtk_css_style_snapshot_icon (style,
                               snapshot,
                               gtk_widget_get_width (widget),
                               gtk_widget_get_height (widget));
}

static void
gtk_spinner_map (GtkWidget *widget)
{
  GtkSpinner *spinner = GTK_SPINNER (widget);

  GTK_WIDGET_CLASS (gtk_spinner_parent_class)->map (widget);

  update_state_flags (spinner);
}

static void
gtk_spinner_unmap (GtkWidget *widget)
{
  GtkSpinner *spinner = GTK_SPINNER (widget);

  GTK_WIDGET_CLASS (gtk_spinner_parent_class)->unmap (widget);

  update_state_flags (spinner);
}

static void
gtk_spinner_css_changed (GtkWidget         *widget,
                         GtkCssStyleChange *change)
{ 
  GTK_WIDGET_CLASS (gtk_spinner_parent_class)->css_changed (widget, change);
  
  if (change == NULL ||
      gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON_SIZE))
    {
      gtk_widget_queue_resize (widget);
    }
  else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON_TEXTURE |
                                                 GTK_CSS_AFFECTS_ICON_REDRAW))
    {
      gtk_widget_queue_draw (widget);
    }
}

/**
 * gtk_spinner_get_spinning: (attributes org.gtk.Method.get_property=spinning)
 * @spinner: a `GtkSpinner`
 *
 * Returns whether the spinner is spinning.
 *
 * Returns: %TRUE if the spinner is active
 */
gboolean
gtk_spinner_get_spinning (GtkSpinner *spinner)
{
  g_return_val_if_fail (GTK_IS_SPINNER (spinner), FALSE);

  return spinner->spinning;
}

/**
 * gtk_spinner_set_spinning: (attributes org.gtk.Method.set_property=spinning)
 * @spinner: a `GtkSpinner`
 * @spinning: whether the spinner should be spinning
 *
 * Sets the activity of the spinner.
 */
void
gtk_spinner_set_spinning (GtkSpinner *spinner,
                          gboolean    spinning)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  spinning = !!spinning;

  if (spinning == spinner->spinning)
    return;

  spinner->spinning = spinning;

  update_state_flags (spinner);

  g_object_notify (G_OBJECT (spinner), "spinning");
}

static void
gtk_spinner_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (param_id)
    {
      case PROP_SPINNING:
        g_value_set_boolean (value, gtk_spinner_get_spinning (GTK_SPINNER (object)));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (param_id)
    {
      case PROP_SPINNING:
        gtk_spinner_set_spinning (GTK_SPINNER (object), g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_class_init (GtkSpinnerClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->get_property = gtk_spinner_get_property;
  gobject_class->set_property = gtk_spinner_set_property;

  widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->snapshot = gtk_spinner_snapshot;
  widget_class->measure = gtk_spinner_measure;
  widget_class->map = gtk_spinner_map;
  widget_class->unmap = gtk_spinner_unmap;
  widget_class->css_changed = gtk_spinner_css_changed;

  /**
   * GtkSpinner:spinning: (attributes org.gtk.Property.get=gtk_spinner_get_spinning org.gtk.Property.set=gtk_spinner_set_spinning)
   *
   * Whether the spinner is spinning
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SPINNING,
                                   g_param_spec_boolean ("spinning", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, I_("spinner"));
}

static void
gtk_spinner_init (GtkSpinner *spinner)
{
}

/**
 * gtk_spinner_new:
 *
 * Returns a new spinner widget. Not yet started.
 *
 * Returns: a new `GtkSpinner`
 */
GtkWidget *
gtk_spinner_new (void)
{
  return g_object_new (GTK_TYPE_SPINNER, NULL);
}

/**
 * gtk_spinner_start:
 * @spinner: a `GtkSpinner`
 *
 * Starts the animation of the spinner.
 */
void
gtk_spinner_start (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_spinning (spinner, TRUE);
}

/**
 * gtk_spinner_stop:
 * @spinner: a `GtkSpinner`
 *
 * Stops the animation of the spinner.
 */
void
gtk_spinner_stop (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_spinning (spinner, FALSE);
}
