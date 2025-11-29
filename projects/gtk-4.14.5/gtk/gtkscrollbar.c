/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

#include "gtkscrollbar.h"
#include "gtkrange.h"

#include "gtkaccessiblerange.h"
#include "gtkadjustment.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkboxlayout.h"


/**
 * GtkScrollbar:
 *
 * The `GtkScrollbar` widget is a horizontal or vertical scrollbar.
 *
 * ![An example GtkScrollbar](scrollbar.png)
 *
 * Its position and movement are controlled by the adjustment that is passed to
 * or created by [ctor@Gtk.Scrollbar.new]. See [class@Gtk.Adjustment] for more
 * details. The [property@Gtk.Adjustment:value] field sets the position of the
 * thumb and must be between [property@Gtk.Adjustment:lower] and
 * [property@Gtk.Adjustment:upper] - [property@Gtk.Adjustment:page-size].
 * The [property@Gtk.Adjustment:page-size] represents the size of the visible
 * scrollable area.
 *
 * The fields [property@Gtk.Adjustment:step-increment] and
 * [property@Gtk.Adjustment:page-increment] fields are added to or subtracted
 * from the [property@Gtk.Adjustment:value] when the user asks to move by a step
 * (using e.g. the cursor arrow keys) or by a page (using e.g. the Page Down/Up
 * keys).
 *
 * # CSS nodes
 *
 * ```
 * scrollbar
 * ╰── range[.fine-tune]
 *     ╰── trough
 *         ╰── slider
 * ```
 *
 * `GtkScrollbar` has a main CSS node with name scrollbar and a subnode for its
 * contents. The main node gets the .horizontal or .vertical style classes applied,
 * depending on the scrollbar's orientation.
 *
 * The range node gets the style class .fine-tune added when the scrollbar is
 * in 'fine-tuning' mode.
 *
 * Other style classes that may be added to scrollbars inside
 * [class@Gtk.ScrolledWindow] include the positional classes (.left, .right,
 * .top, .bottom) and style classes related to overlay scrolling (.overlay-indicator,
 * .dragging, .hovering).
 *
 * # Accessibility
 *
 * `GtkScrollbar` uses the %GTK_ACCESSIBLE_ROLE_SCROLLBAR role.
 */

typedef struct _GtkScrollbarClass   GtkScrollbarClass;

struct _GtkScrollbar
{
  GtkWidget parent_instance;
};

struct _GtkScrollbarClass
{
  GtkWidgetClass parent_class;
};

typedef struct {
  GtkOrientation orientation;
  GtkWidget *range;
} GtkScrollbarPrivate;

enum {
  PROP_0,
  PROP_ADJUSTMENT,

  PROP_ORIENTATION,
  LAST_PROP = PROP_ORIENTATION
};

static void gtk_scrollbar_accessible_range_init (GtkAccessibleRangeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkScrollbar, gtk_scrollbar, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkScrollbar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_RANGE, 
                         gtk_scrollbar_accessible_range_init))

static GParamSpec *props[LAST_PROP] = { NULL, };

static gboolean
accessible_range_set_current_value (GtkAccessibleRange *range,
                                    double              value)
{
  GtkScrollbar *self = GTK_SCROLLBAR (range);
  GtkAdjustment *adjustment = gtk_scrollbar_get_adjustment (self);

  if (adjustment)
    {
      gtk_adjustment_set_value (adjustment, value);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_scrollbar_accessible_range_init (GtkAccessibleRangeInterface *iface)
{
  iface->set_current_value = accessible_range_set_current_value;
}

static void
gtk_scrollbar_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkScrollbar *self = GTK_SCROLLBAR (object);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  switch (property_id)
   {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, gtk_scrollbar_get_adjustment (self));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_scrollbar_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkScrollbar *self = GTK_SCROLLBAR (object);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  switch (property_id)
    {
    case PROP_ADJUSTMENT:
      gtk_scrollbar_set_adjustment (self, g_value_get_object (value));
      break;
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);

        if (orientation != priv->orientation)
          {
            GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
            gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), orientation);
            gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->range), orientation);
            priv->orientation = orientation;
            gtk_widget_update_orientation (GTK_WIDGET (self), priv->orientation);
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify_by_pspec (object, pspec);
            gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                            GTK_ACCESSIBLE_PROPERTY_ORIENTATION, orientation,
                                            -1);
          }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void gtk_scrollbar_adjustment_changed       (GtkAdjustment *adjustment,
                                                    gpointer       data);
static void gtk_scrollbar_adjustment_value_changed (GtkAdjustment *adjustment,
                                                    gpointer       data);

static void
gtk_scrollbar_dispose (GObject *object)
{
  GtkScrollbar *self = GTK_SCROLLBAR (object);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);
  GtkAdjustment *adj;

  adj = gtk_range_get_adjustment (GTK_RANGE (priv->range));
  if (adj)
    {
      g_signal_handlers_disconnect_by_func (adj, gtk_scrollbar_adjustment_changed, self);
      g_signal_handlers_disconnect_by_func (adj, gtk_scrollbar_adjustment_value_changed, self);
    }

  g_clear_pointer (&priv->range, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_scrollbar_parent_class)->dispose (object);
}

static void
gtk_scrollbar_class_init (GtkScrollbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_scrollbar_get_property;
  object_class->set_property = gtk_scrollbar_set_property;
  object_class->dispose = gtk_scrollbar_dispose;

  /**
   * GtkScrollbar:adjustment: (attributes org.gtk.Property.get=gtk_scrollbar_get_adjustment org.gtk.Property.set=gtk_scrollbar_set_adjustment)
   *
   * The `GtkAdjustment` controlled by this scrollbar.
   */
  props[PROP_ADJUSTMENT] =
      g_param_spec_object ("adjustment", NULL, NULL,
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  gtk_widget_class_set_css_name (widget_class, I_("scrollbar"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SCROLLBAR);
}

static void
gtk_scrollbar_init (GtkScrollbar *self)
{
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  priv->range = g_object_new (GTK_TYPE_RANGE, NULL);
  gtk_widget_set_hexpand (priv->range, TRUE);
  gtk_widget_set_vexpand (priv->range, TRUE);
  gtk_widget_set_parent (priv->range, GTK_WIDGET (self));
  gtk_widget_update_orientation (GTK_WIDGET (self), priv->orientation);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_ORIENTATION, priv->orientation,
                                  -1);
}

/**
 * gtk_scrollbar_new:
 * @orientation: the scrollbar’s orientation.
 * @adjustment: (nullable): the [class@Gtk.Adjustment] to use, or %NULL
 *   to create a new adjustment.
 *
 * Creates a new scrollbar with the given orientation.
 *
 * Returns:  the new `GtkScrollbar`.
 */
GtkWidget *
gtk_scrollbar_new (GtkOrientation  orientation,
                   GtkAdjustment  *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_SCROLLBAR,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}

static void
gtk_scrollbar_adjustment_changed (GtkAdjustment *adjustment,
                                  gpointer       data)
{
  GtkScrollbar *self = data;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, gtk_adjustment_get_upper (adjustment),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, gtk_adjustment_get_lower (adjustment),
                                  -1);
}

static void
gtk_scrollbar_adjustment_value_changed (GtkAdjustment *adjustment,
                                        gpointer       data)
{
  GtkScrollbar *self = data;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, gtk_adjustment_get_value (adjustment),
                                  -1);
}

/**
 * gtk_scrollbar_set_adjustment: (attributes org.gtk.Method.set_property=adjustment)
 * @self: a `GtkScrollbar`
 * @adjustment: (nullable): the adjustment to set
 *
 * Makes the scrollbar use the given adjustment.
 */
void
gtk_scrollbar_set_adjustment (GtkScrollbar  *self,
                              GtkAdjustment *adjustment)
{
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);
  GtkAdjustment *adj;

  g_return_if_fail (GTK_IS_SCROLLBAR (self));
  g_return_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment));

  adj = gtk_range_get_adjustment (GTK_RANGE (priv->range));
  if (adj == adjustment)
    return;

  if (adj)
    {
      g_signal_handlers_disconnect_by_func (adj, gtk_scrollbar_adjustment_changed, self);
      g_signal_handlers_disconnect_by_func (adj, gtk_scrollbar_adjustment_value_changed, self);
    }

  gtk_range_set_adjustment (GTK_RANGE (priv->range), adjustment);

  if (adjustment)
    {
      g_signal_connect (adjustment, "changed",
                        G_CALLBACK (gtk_scrollbar_adjustment_changed), self);
      g_signal_connect (adjustment, "value-changed",
                        G_CALLBACK (gtk_scrollbar_adjustment_value_changed), self);

      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, gtk_adjustment_get_upper (adjustment),
                                      GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, gtk_adjustment_get_lower (adjustment),
                                      GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, gtk_adjustment_get_value (adjustment),
                                      -1);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ADJUSTMENT]);
}

/**
 * gtk_scrollbar_get_adjustment: (attributes org.gtk.Method.get_property=adjustment)
 * @self: a `GtkScrollbar`
 *
 * Returns the scrollbar's adjustment.
 *
 * Returns: (transfer none): the scrollbar's adjustment
 */
GtkAdjustment *
gtk_scrollbar_get_adjustment (GtkScrollbar  *self)
{
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_SCROLLBAR (self), NULL);

  if (priv->range)
    return gtk_range_get_adjustment (GTK_RANGE (priv->range));

  return NULL;
}
