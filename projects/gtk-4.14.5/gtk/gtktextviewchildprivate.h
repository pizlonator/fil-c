/* GTK - The GIMP Toolkit
 * gtktextviewchild-private.h Copyright (C) 2019 Red Hat, Inc.
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

#include <gtk/gtkwidget.h>
#include <gtk/gtktextview.h>

#define GTK_TYPE_TEXT_VIEW_CHILD (gtk_text_view_child_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkTextViewChild, gtk_text_view_child, GTK, TEXT_VIEW_CHILD, GtkWidget)

G_GNUC_INTERNAL
GtkWidget         *gtk_text_view_child_new             (GtkTextWindowType  window_type);
G_GNUC_INTERNAL
GtkTextWindowType  gtk_text_view_child_get_window_type (GtkTextViewChild  *self);

G_GNUC_INTERNAL
void               gtk_text_view_child_add             (GtkTextViewChild  *self,
                                                        GtkWidget         *widget);
G_GNUC_INTERNAL
void               gtk_text_view_child_remove          (GtkTextViewChild  *self,
                                                        GtkWidget         *widget);
G_GNUC_INTERNAL
void               gtk_text_view_child_add_overlay     (GtkTextViewChild  *self,
                                                        GtkWidget         *widget,
                                                        int                xpos,
                                                        int                ypos);
G_GNUC_INTERNAL
void               gtk_text_view_child_move_overlay    (GtkTextViewChild  *self,
                                                        GtkWidget         *widget,
                                                        int                xpos,
                                                        int                ypos);
G_GNUC_INTERNAL
void               gtk_text_view_child_set_offset      (GtkTextViewChild  *child,
                                                        int                xoffset,
                                                        int                yoffset);

