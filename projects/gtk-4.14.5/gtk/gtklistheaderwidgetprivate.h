/*
 * Copyright © 2023 Benjamin Otte
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

#include "gtklistheaderbaseprivate.h"

#include "gtklistitemfactory.h"

G_BEGIN_DECLS

#define GTK_TYPE_LIST_HEADER_WIDGET         (gtk_list_header_widget_get_type ())
#define GTK_LIST_HEADER_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_LIST_HEADER_WIDGET, GtkListHeaderWidget))
#define GTK_LIST_HEADER_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_LIST_HEADER_WIDGET, GtkListHeaderWidgetClass))
#define GTK_IS_LIST_HEADER_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_LIST_HEADER_WIDGET))
#define GTK_IS_LIST_HEADER_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_LIST_HEADER_WIDGET))
#define GTK_LIST_HEADER_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_LIST_HEADER_WIDGET, GtkListHeaderWidgetClass))

typedef struct _GtkListHeaderWidget GtkListHeaderWidget;
typedef struct _GtkListHeaderWidgetClass GtkListHeaderWidgetClass;

struct _GtkListHeaderWidget
{
  GtkListHeaderBase parent_instance;
};

struct _GtkListHeaderWidgetClass
{
  GtkListHeaderBaseClass parent_class;
};

GType                   gtk_list_header_widget_get_type         (void) G_GNUC_CONST;

GtkWidget *             gtk_list_header_widget_new              (GtkListItemFactory     *factory);

void                    gtk_list_header_widget_set_factory      (GtkListHeaderWidget    *self,
                                                                 GtkListItemFactory     *factory);
GtkListItemFactory *    gtk_list_header_widget_get_factory      (GtkListHeaderWidget    *self);

void                    gtk_list_header_widget_set_child        (GtkListHeaderWidget    *self,
                                                                 GtkWidget              *child);


G_END_DECLS

