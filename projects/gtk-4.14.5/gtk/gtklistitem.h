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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_LIST_ITEM (gtk_list_item_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_INTERNAL_TYPE (GtkListItem, gtk_list_item, GTK, LIST_ITEM, GObject)

GDK_AVAILABLE_IN_ALL
gpointer        gtk_list_item_get_item                          (GtkListItem            *self);
GDK_AVAILABLE_IN_ALL
guint           gtk_list_item_get_position                      (GtkListItem            *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
gboolean        gtk_list_item_get_selected                      (GtkListItem            *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
gboolean        gtk_list_item_get_selectable                    (GtkListItem            *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
void            gtk_list_item_set_selectable                    (GtkListItem            *self,
                                                                 gboolean                selectable);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_list_item_get_activatable                   (GtkListItem            *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
void            gtk_list_item_set_activatable                   (GtkListItem            *self,
                                                                 gboolean                activatable);
GDK_AVAILABLE_IN_4_12
gboolean        gtk_list_item_get_focusable                     (GtkListItem            *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_12
void            gtk_list_item_set_focusable                     (GtkListItem            *self,
                                                                 gboolean                focusable);

GDK_AVAILABLE_IN_ALL
void            gtk_list_item_set_child                         (GtkListItem            *self,
                                                                 GtkWidget              *child);
GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_list_item_get_child                         (GtkListItem            *self);

GDK_AVAILABLE_IN_4_12
void            gtk_list_item_set_accessible_description        (GtkListItem            *self,
                                                                 const char             *description);
GDK_AVAILABLE_IN_4_12
const char *    gtk_list_item_get_accessible_description        (GtkListItem            *self);

GDK_AVAILABLE_IN_4_12
void            gtk_list_item_set_accessible_label              (GtkListItem            *self,
                                                                 const char             *label);
GDK_AVAILABLE_IN_4_12
const char *    gtk_list_item_get_accessible_label              (GtkListItem            *self);

G_END_DECLS

