/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
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

#pragma once

#include "gtkcssnodeprivate.h"
#include "gtkwidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_WIDGET_NODE           (gtk_css_widget_node_get_type ())
#define GTK_CSS_WIDGET_NODE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_WIDGET_NODE, GtkCssWidgetNode))
#define GTK_CSS_WIDGET_NODE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_WIDGET_NODE, GtkCssWidgetNodeClass))
#define GTK_IS_CSS_WIDGET_NODE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_WIDGET_NODE))
#define GTK_IS_CSS_WIDGET_NODE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_WIDGET_NODE))
#define GTK_CSS_WIDGET_NODE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_WIDGET_NODE, GtkCssWidgetNodeClass))

typedef struct _GtkCssWidgetNode           GtkCssWidgetNode;
typedef struct _GtkCssWidgetNodeClass      GtkCssWidgetNodeClass;

struct _GtkCssWidgetNode
{
  GtkCssNode node;

  GtkWidget *widget;
  guint validate_cb_id;
  GtkCssStyle *last_updated_style;
};

struct _GtkCssWidgetNodeClass
{
  GtkCssNodeClass node_class;
};

GType                   gtk_css_widget_node_get_type            (void) G_GNUC_CONST;

GtkCssNode *            gtk_css_widget_node_new                 (GtkWidget              *widget);

void                    gtk_css_widget_node_widget_destroyed    (GtkCssWidgetNode       *node);

GtkWidget *             gtk_css_widget_node_get_widget          (GtkCssWidgetNode       *node);

G_END_DECLS

