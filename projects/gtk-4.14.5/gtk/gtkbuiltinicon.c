
/*
 * Copyright © 2015 Endless Mobile, Inc.
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
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#include "config.h"

#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"

/* GtkBuiltinIcon was a minimal widget wrapped around a GtkBuiltinIcon gadget,
 * It should be used whenever builtin-icon functionality is desired
 * but a widget is needed for other reasons.
 */

struct _GtkBuiltinIcon
{
  GtkWidget parent;
};

G_DEFINE_TYPE (GtkBuiltinIcon, gtk_builtin_icon, GTK_TYPE_WIDGET)

static void
gtk_builtin_icon_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (width > 0 && height > 0)
    gtk_css_style_snapshot_icon (style, snapshot, width, height);
}

static void
gtk_builtin_icon_css_changed (GtkWidget         *widget,
                              GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (gtk_builtin_icon_parent_class)->css_changed (widget, change);

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

static void
gtk_builtin_icon_measure (GtkWidget      *widget,
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
gtk_builtin_icon_class_init (GtkBuiltinIconClass *klass)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  wclass->snapshot = gtk_builtin_icon_snapshot;
  wclass->measure = gtk_builtin_icon_measure;
  wclass->css_changed = gtk_builtin_icon_css_changed;
}

static void
gtk_builtin_icon_init (GtkBuiltinIcon *self)
{
}

GtkWidget *
gtk_builtin_icon_new (const char *css_name)
{
  return g_object_new (GTK_TYPE_BUILTIN_ICON,
                       "css-name", css_name,
                       NULL);
}

void
gtk_builtin_icon_set_css_name (GtkBuiltinIcon *self,
                               const char     *css_name)
{
  gtk_css_node_set_name (gtk_widget_get_css_node (GTK_WIDGET (self)),
                         g_quark_from_string (css_name));
}
