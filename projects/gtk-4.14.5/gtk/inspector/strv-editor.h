/*
 * Copyright (c) 2015 Red Hat, Inc.
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


#define GTK_TYPE_INSPECTOR_STRV_EDITOR            (gtk_inspector_strv_editor_get_type())
#define GTK_INSPECTOR_STRV_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_STRV_EDITOR, GtkInspectorStrvEditor))
#define GTK_INSPECTOR_STRV_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_STRV_EDITOR, GtkInspectorStrvEditorClass))
#define GTK_INSPECTOR_IS_STRV_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_STRV_EDITOR))
#define GTK_INSPECTOR_IS_STRV_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_STRV_EDITOR))
#define GTK_INSPECTOR_STRV_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_STRV_EDITOR, GtkInspectorStrvEditorClass))


typedef struct
{
  GtkBox parent;

  GtkWidget *box;
  GtkWidget *button;

  gboolean blocked;
} GtkInspectorStrvEditor;

typedef struct
{
  GtkBoxClass parent;

  void (* changed) (GtkInspectorStrvEditor *editor);

} GtkInspectorStrvEditorClass;


G_BEGIN_DECLS


GType gtk_inspector_strv_editor_get_type (void);

void    gtk_inspector_strv_editor_set_strv (GtkInspectorStrvEditor  *editor,
                                            char                   **strv);

char **gtk_inspector_strv_editor_get_strv (GtkInspectorStrvEditor  *editor);

G_END_DECLS



// vim: set et:
