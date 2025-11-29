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

#include "gtkseparator.h"

#include "gtkaccessible.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

/**
 * GtkSeparator:
 *
 * `GtkSeparator` is a horizontal or vertical separator widget.
 *
 * ![An example GtkSeparator](separator.png)
 *
 * A `GtkSeparator` can be used to group the widgets within a window.
 * It displays a line with a shadow to make it appear sunken into the
 * interface.
 *
 * # CSS nodes
 *
 * `GtkSeparator` has a single CSS node with name separator. The node
 * gets one of the .horizontal or .vertical style classes.
 *
 * # Accessibility
 *
 * `GtkSeparator` uses the %GTK_ACCESSIBLE_ROLE_SEPARATOR role.
 */

typedef struct _GtkSeparatorClass GtkSeparatorClass;

struct _GtkSeparator
{
  GtkWidget parent_instance;

  GtkOrientation orientation;
};

struct _GtkSeparatorClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_ORIENTATION
};


G_DEFINE_TYPE_WITH_CODE (GtkSeparator, gtk_separator, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_separator_set_orientation (GtkSeparator   *self,
                               GtkOrientation  orientation)
{
  if (self->orientation != orientation)
    {
      self->orientation = orientation;

      gtk_widget_update_orientation (GTK_WIDGET (self), orientation);
      gtk_widget_queue_resize (GTK_WIDGET (self));

      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_ORIENTATION, orientation,
                                      -1);

      g_object_notify (G_OBJECT (self), "orientation");
    }
}

static void
gtk_separator_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkSeparator *separator = GTK_SEPARATOR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_separator_set_orientation (separator, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkSeparator *separator = GTK_SEPARATOR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, separator->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_init (GtkSeparator *separator)
{
  separator->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_update_orientation (GTK_WIDGET (separator),
                                 separator->orientation);
}

static void
gtk_separator_class_init (GtkSeparatorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_separator_set_property;
  object_class->get_property = gtk_separator_get_property;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  gtk_widget_class_set_css_name (widget_class, I_("separator"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEPARATOR);
}

/**
 * gtk_separator_new:
 * @orientation: the separator’s orientation.
 *
 * Creates a new `GtkSeparator` with the given orientation.
 *
 * Returns: a new `GtkSeparator`.
 */
GtkWidget *
gtk_separator_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_SEPARATOR,
                       "orientation", orientation,
                       NULL);
}
