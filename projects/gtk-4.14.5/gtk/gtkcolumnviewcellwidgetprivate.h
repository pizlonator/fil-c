/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkcolumnviewcolumn.h"

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW_CELL_WIDGET         (gtk_column_view_cell_widget_get_type ())
#define GTK_COLUMN_VIEW_CELL_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COLUMN_VIEW_CELL_WIDGET, GtkColumnViewCellWidget))
#define GTK_COLUMN_VIEW_CELL_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COLUMN_VIEW_CELL_WIDGET, GtkColumnViewCellWidgetClass))
#define GTK_IS_COLUMN_VIEW_CELL_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COLUMN_VIEW_CELL_WIDGET))
#define GTK_IS_COLUMN_VIEW_CELL_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COLUMN_VIEW_CELL_WIDGET))
#define GTK_COLUMN_VIEW_CELL_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COLUMN_VIEW_CELL_WIDGET, GtkColumnViewCellWidgetClass))

typedef struct _GtkColumnViewCellWidget GtkColumnViewCellWidget;
typedef struct _GtkColumnViewCellWidgetClass GtkColumnViewCellWidgetClass;

GType                           gtk_column_view_cell_widget_get_type           (void) G_GNUC_CONST;

GtkWidget *                     gtk_column_view_cell_widget_new                (GtkColumnViewColumn             *column,
                                                                                gboolean                         inert);

void                            gtk_column_view_cell_widget_set_child          (GtkColumnViewCellWidget         *self,
                                                                                GtkWidget                       *child);

void                            gtk_column_view_cell_widget_remove             (GtkColumnViewCellWidget         *self);

GtkColumnViewCellWidget *       gtk_column_view_cell_widget_get_next           (GtkColumnViewCellWidget         *self);
GtkColumnViewCellWidget *       gtk_column_view_cell_widget_get_prev           (GtkColumnViewCellWidget         *self);
GtkColumnViewColumn *           gtk_column_view_cell_widget_get_column         (GtkColumnViewCellWidget         *self);
void                            gtk_column_view_cell_widget_unset_column       (GtkColumnViewCellWidget         *self);

G_END_DECLS
