/*
 * Copyright © 2018 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include "gtklistfactorywidgetprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_LIST_ITEM_WIDGET         (gtk_list_item_widget_get_type ())
#define GTK_LIST_ITEM_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_LIST_ITEM_WIDGET, GtkListItemWidget))
#define GTK_LIST_ITEM_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_LIST_ITEM_WIDGET, GtkListItemWidgetClass))
#define GTK_IS_LIST_ITEM_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_LIST_ITEM_WIDGET))
#define GTK_IS_LIST_ITEM_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_LIST_ITEM_WIDGET))
#define GTK_LIST_ITEM_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_LIST_ITEM_WIDGET, GtkListItemWidgetClass))

typedef struct _GtkListItemWidget GtkListItemWidget;
typedef struct _GtkListItemWidgetClass GtkListItemWidgetClass;

struct _GtkListItemWidget
{
  GtkListFactoryWidget parent_instance;
};

struct _GtkListItemWidgetClass
{
  GtkListFactoryWidgetClass parent_class;
};

GType                   gtk_list_item_widget_get_type           (void) G_GNUC_CONST;

GtkWidget *             gtk_list_item_widget_new                (GtkListItemFactory     *factory,
                                                                 const char             *css_name,
                                                                 GtkAccessibleRole       role);

void                    gtk_list_item_widget_set_child          (GtkListItemWidget      *self,
                                                                 GtkWidget              *child);

G_END_DECLS

