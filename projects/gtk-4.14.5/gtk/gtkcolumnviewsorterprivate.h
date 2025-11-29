/*
 * Copyright © 2019 Matthias Clasen
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcolumnviewsorter.h>

G_BEGIN_DECLS

GtkColumnViewSorter *   gtk_column_view_sorter_new              (void);

gboolean                gtk_column_view_sorter_add_column       (GtkColumnViewSorter    *self,
                                                                 GtkColumnViewColumn    *column);
gboolean                gtk_column_view_sorter_remove_column    (GtkColumnViewSorter    *self,
                                                                 GtkColumnViewColumn    *column);

void                    gtk_column_view_sorter_clear            (GtkColumnViewSorter    *self);

GtkColumnViewColumn *   gtk_column_view_sorter_get_sort_column  (GtkColumnViewSorter    *self,
                                                                 gboolean               *inverted);

gboolean                gtk_column_view_sorter_set_column       (GtkColumnViewSorter    *self,
                                                                 GtkColumnViewColumn    *column,
                                                                 gboolean                inverted);


G_END_DECLS


