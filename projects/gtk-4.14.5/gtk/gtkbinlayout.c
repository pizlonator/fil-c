/* gtkbinlayout.c: Layout manager for bin-like widgets
 * Copyright 2019  GNOME Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkBinLayout:
 *
 * `GtkBinLayout` is a `GtkLayoutManager` subclass useful for create "bins" of
 * widgets.
 *
 * `GtkBinLayout` will stack each child of a widget on top of each other,
 * using the [property@Gtk.Widget:hexpand], [property@Gtk.Widget:vexpand],
 * [property@Gtk.Widget:halign], and [property@Gtk.Widget:valign] properties
 * of each child to determine where they should be positioned.
 */

#include "config.h"

#include "gtkbinlayout.h"

#include "gtkwidgetprivate.h"

struct _GtkBinLayout
{
  GtkLayoutManager parent_instance;
};

G_DEFINE_TYPE (GtkBinLayout, gtk_bin_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_bin_layout_measure (GtkLayoutManager *layout_manager,
                        GtkWidget        *widget,
                        GtkOrientation    orientation,
                        int               for_size,
                        int              *minimum,
                        int              *natural,
                        int              *minimum_baseline,
                        int              *natural_baseline)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_should_layout (child))
        {
          int child_min = 0;
          int child_nat = 0;
          int child_min_baseline = -1;
          int child_nat_baseline = -1;

          gtk_widget_measure (child, orientation, for_size,
                              &child_min, &child_nat,
                              &child_min_baseline, &child_nat_baseline);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);

          if (child_min_baseline > -1)
            *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
          if (child_nat_baseline > -1)
            *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
        }
    }
}

static void
gtk_bin_layout_allocate (GtkLayoutManager *layout_manager,
                         GtkWidget        *widget,
                         int               width,
                         int               height,
                         int               baseline)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (child && gtk_widget_should_layout (child))
        gtk_widget_allocate (child, width, height, baseline, NULL);
    }
}
static void
gtk_bin_layout_class_init (GtkBinLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_manager_class->measure = gtk_bin_layout_measure;
  layout_manager_class->allocate = gtk_bin_layout_allocate;
}

static void
gtk_bin_layout_init (GtkBinLayout *self)
{
}

/**
 * gtk_bin_layout_new:
 *
 * Creates a new `GtkBinLayout` instance.
 *
 * Returns: the newly created `GtkBinLayout`
 */
GtkLayoutManager *
gtk_bin_layout_new (void)
{
  return g_object_new (GTK_TYPE_BIN_LAYOUT, NULL);
}
