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
#include <gtk/gtksizegroup.h>


#define GTK_TYPE_INSPECTOR_ACTION_EDITOR            (gtk_inspector_action_editor_get_type())
#define GTK_INSPECTOR_ACTION_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_ACTION_EDITOR, GtkInspectorActionEditor))
#define GTK_INSPECTOR_IS_ACTION_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_ACTION_EDITOR))


typedef struct _GtkInspectorActionEditor GtkInspectorActionEditor;

G_BEGIN_DECLS

GType      gtk_inspector_action_editor_get_type (void);
GtkWidget *gtk_inspector_action_editor_new      (void);
void       gtk_inspector_action_editor_set      (GtkInspectorActionEditor *self,
                                                 GObject                  *owner,
                                                 const char               *name);
void       gtk_inspector_action_editor_update   (GtkInspectorActionEditor *self);

G_END_DECLS



// vim: set et:
