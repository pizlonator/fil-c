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

#include "gtklistfactorywidgetprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW_ROW_WIDGET         (gtk_column_view_row_widget_get_type ())
#define GTK_COLUMN_VIEW_ROW_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COLUMN_VIEW_ROW_WIDGET, GtkColumnViewRowWidget))
#define GTK_COLUMN_VIEW_ROW_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COLUMN_VIEW_ROW_WIDGET, GtkColumnViewRowWidgetClass))
#define GTK_IS_COLUMN_VIEW_ROW_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COLUMN_VIEW_ROW_WIDGET))
#define GTK_IS_COLUMN_VIEW_ROW_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COLUMN_VIEW_ROW_WIDGET))
#define GTK_COLUMN_VIEW_ROW_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COLUMN_VIEW_ROW_WIDGET, GtkColumnViewRowWidgetClass))

typedef struct _GtkColumnViewRowWidget GtkColumnViewRowWidget;
typedef struct _GtkColumnViewRowWidgetClass GtkColumnViewRowWidgetClass;

struct _GtkColumnViewRowWidget
{
  GtkListFactoryWidget parent_instance;
};

struct _GtkColumnViewRowWidgetClass
{
  GtkListFactoryWidgetClass parent_class;
};

GType                   gtk_column_view_row_widget_get_type             (void) G_GNUC_CONST;

GtkWidget *             gtk_column_view_row_widget_new                  (GtkListItemFactory     *factory,
                                                                         gboolean                is_header);

void                    gtk_column_view_row_widget_add_child            (GtkColumnViewRowWidget *self,
                                                                         GtkWidget              *child);
void                    gtk_column_view_row_widget_reorder_child        (GtkColumnViewRowWidget *self,
                                                                         GtkWidget              *child,
                                                                         guint                   position);
void                    gtk_column_view_row_widget_remove_child         (GtkColumnViewRowWidget *self,
                                                                         GtkWidget              *child);

G_END_DECLS

