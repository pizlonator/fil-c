/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#define GTK_TYPE_INSPECTOR_RESOURCE_LIST            (gtk_inspector_resource_list_get_type())
#define GTK_INSPECTOR_RESOURCE_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_RESOURCE_LIST, GtkInspectorResourceList))
#define GTK_INSPECTOR_IS_RESOURCE_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_RESOURCE_LIST))


typedef struct _GtkInspectorResourceList GtkInspectorResourceList;

G_BEGIN_DECLS

GType      gtk_inspector_resource_list_get_type   (void);

G_END_DECLS


// vim: set et sw=2 ts=2:
