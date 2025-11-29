/*
 * Copyright (c) 2020 Red Hat, Inc.
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

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_SHORTCUTS (gtk_inspector_shortcuts_get_type ())

G_DECLARE_FINAL_TYPE (GtkInspectorShortcuts, gtk_inspector_shortcuts, GTK, INSPECTOR_SHORTCUTS, GtkWidget)


void gtk_inspector_shortcuts_set_object (GtkInspectorShortcuts *sl,
                                         GObject               *object);

