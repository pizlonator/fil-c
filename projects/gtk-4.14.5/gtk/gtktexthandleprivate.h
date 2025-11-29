/* GTK - The GIMP Toolkit
 * Copyright © 2012 Carlos Garnacho <carlosg@gnome.org>
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

G_BEGIN_DECLS

#define GTK_TYPE_TEXT_HANDLE (gtk_text_handle_get_type ())
G_DECLARE_FINAL_TYPE (GtkTextHandle, gtk_text_handle,
		      GTK, TEXT_HANDLE, GtkWidget)

typedef enum
{
  GTK_TEXT_HANDLE_ROLE_CURSOR,
  GTK_TEXT_HANDLE_ROLE_SELECTION_START,
  GTK_TEXT_HANDLE_ROLE_SELECTION_END,
} GtkTextHandleRole;

GtkTextHandle *    gtk_text_handle_new          (GtkWidget             *parent);

void               gtk_text_handle_present      (GtkTextHandle         *handle);

void               gtk_text_handle_set_role (GtkTextHandle     *handle,
					     GtkTextHandleRole  role);
GtkTextHandleRole  gtk_text_handle_get_role (GtkTextHandle     *handle);

void               gtk_text_handle_set_position (GtkTextHandle      *handle,
						 const GdkRectangle *rect);

gboolean           gtk_text_handle_get_is_dragged (GtkTextHandle *handle);

G_END_DECLS

