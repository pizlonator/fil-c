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

#include "gtk/gtkcolumnview.h"
#include "gtk/gtklistview.h"
#include "gtk/gtksizerequest.h"

#include "gtk/gtkcolumnviewsorterprivate.h"
#include "gtk/gtkcolumnviewrowwidgetprivate.h"

gboolean                gtk_column_view_is_inert                (GtkColumnView          *self);

GtkColumnViewRowWidget *gtk_column_view_get_header_widget       (GtkColumnView          *self);
GtkListView *           gtk_column_view_get_list_view           (GtkColumnView          *self);

void                    gtk_column_view_measure_across          (GtkColumnView          *self,
                                                                 int                    *minimum,
                                                                 int                    *natural);

void                    gtk_column_view_distribute_width        (GtkColumnView          *self,
                                                                 int                     width,
                                                                 GtkRequestedSize       *sizes);

void                    gtk_column_view_set_focus_column        (GtkColumnView          *self,
                                                                 GtkColumnViewColumn    *focus_column,
                                                                 gboolean                scroll);
GtkColumnViewColumn *   gtk_column_view_get_focus_column        (GtkColumnView          *self);

