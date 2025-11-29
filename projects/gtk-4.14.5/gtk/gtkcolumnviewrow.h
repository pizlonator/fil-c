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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW_ROW (gtk_column_view_row_get_type ())
GDK_AVAILABLE_IN_4_12
GDK_DECLARE_INTERNAL_TYPE(GtkColumnViewRow, gtk_column_view_row, GTK, COLUMN_VIEW_ROW, GObject)

GDK_AVAILABLE_IN_4_12
gpointer        gtk_column_view_row_get_item                    (GtkColumnViewRow       *self);
GDK_AVAILABLE_IN_4_12
guint           gtk_column_view_row_get_position                (GtkColumnViewRow       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_12
gboolean        gtk_column_view_row_get_selected                (GtkColumnViewRow       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_12
gboolean        gtk_column_view_row_get_selectable              (GtkColumnViewRow       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_12
void            gtk_column_view_row_set_selectable              (GtkColumnViewRow       *self,
                                                                 gboolean                selectable);
GDK_AVAILABLE_IN_4_12
gboolean        gtk_column_view_row_get_activatable             (GtkColumnViewRow       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_12
void            gtk_column_view_row_set_activatable             (GtkColumnViewRow       *self,
                                                                 gboolean                activatable);
GDK_AVAILABLE_IN_4_12
gboolean        gtk_column_view_row_get_focusable               (GtkColumnViewRow       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_12
void            gtk_column_view_row_set_focusable               (GtkColumnViewRow       *self,
                                                                 gboolean                focusable);
GDK_AVAILABLE_IN_4_12
const char *    gtk_column_view_row_get_accessible_description  (GtkColumnViewRow       *self);
GDK_AVAILABLE_IN_4_12
void            gtk_column_view_row_set_accessible_description  (GtkColumnViewRow       *self,
                                                                 const char             *description);
GDK_AVAILABLE_IN_4_12
const char *    gtk_column_view_row_get_accessible_label        (GtkColumnViewRow       *self);
GDK_AVAILABLE_IN_4_12
void            gtk_column_view_row_set_accessible_label        (GtkColumnViewRow       *self,
                                                                 const char             *label);

G_END_DECLS

