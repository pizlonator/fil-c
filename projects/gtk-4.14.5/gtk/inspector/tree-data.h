/*
 * Copyright (c) 2014, 2020 Red Hat, Inc.
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

#define GTK_TYPE_INSPECTOR_TREE_DATA    (gtk_inspector_tree_data_get_type ())
#define GTK_INSPECTOR_TREE_DATA(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_TREE_DATA, GtkInspectorTreeData))
#define GTK_INSPECTOR_IS_TREE_DATA(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_TREE_DATA))

typedef struct _GtkInspectorTreeData GtkInspectorTreeData;

G_BEGIN_DECLS

GType      gtk_inspector_tree_data_get_type   (void) G_GNUC_CONST;
void       gtk_inspector_tree_data_set_object (GtkInspectorTreeData *sl,
                                               GObject              *object);

G_END_DECLS


// vim: set et sw=2 ts=2:
