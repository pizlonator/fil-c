/*
 * Copyright © 2022 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#pragma once

#include <gtk/gtkwidget.h>
#include <gtk/gtkexpression.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_CHOOSER_CELL (gtk_file_chooser_cell_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkFileChooserCell, gtk_file_chooser_cell, GTK, FILE_CHOOSER_CELL, GtkWidget)

GtkFileChooserCell * gtk_file_chooser_cell_new (void);

G_END_DECLS

