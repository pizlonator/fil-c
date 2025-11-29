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

#include "gtk/gtkcolumnviewcolumn.h"

#include "gtk/gtkcolumnviewcellwidgetprivate.h"


void                    gtk_column_view_column_set_column_view          (GtkColumnViewColumn    *self,
                                                                         GtkColumnView          *view);

void                    gtk_column_view_column_set_position             (GtkColumnViewColumn    *self,
                                                                         guint                   position);

void                    gtk_column_view_column_add_cell                 (GtkColumnViewColumn    *self,
                                                                         GtkColumnViewCellWidget      *cell);
void                    gtk_column_view_column_remove_cell              (GtkColumnViewColumn    *self,
                                                                         GtkColumnViewCellWidget      *cell);
GtkColumnViewCellWidget *     gtk_column_view_column_get_first_cell           (GtkColumnViewColumn    *self);
GtkWidget *             gtk_column_view_column_get_header               (GtkColumnViewColumn    *self);

void                    gtk_column_view_column_update_factory           (GtkColumnViewColumn    *self,
                                                                         gboolean                inert);
void                    gtk_column_view_column_queue_resize             (GtkColumnViewColumn    *self);
void                    gtk_column_view_column_measure                  (GtkColumnViewColumn    *self,
                                                                         int                    *minimum,
                                                                         int                    *natural);
void                    gtk_column_view_column_allocate                 (GtkColumnViewColumn    *self,
                                                                         int                     offset,
                                                                         int                     size);
void                    gtk_column_view_column_get_allocation           (GtkColumnViewColumn    *self,
                                                                         int                    *offset,
                                                                         int                    *size);

void                    gtk_column_view_column_notify_sort              (GtkColumnViewColumn    *self);

void                    gtk_column_view_column_set_header_position      (GtkColumnViewColumn    *self,
                                                                         int                     offset);
void                    gtk_column_view_column_get_header_allocation    (GtkColumnViewColumn    *self,
                                                                         int                    *offset,
                                                                         int                    *size);

