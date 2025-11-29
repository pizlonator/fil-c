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


#define GTK_TYPE_INSPECTOR_VARIANT_EDITOR            (gtk_inspector_variant_editor_get_type())
#define GTK_INSPECTOR_VARIANT_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_VARIANT_EDITOR, GtkInspectorVariantEditor))
#define GTK_INSPECTOR_IS_VARIANT_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_VARIANT_EDITOR))


typedef struct _GtkInspectorVariantEditor GtkInspectorVariantEditor;

G_BEGIN_DECLS

typedef void (* GtkInspectorVariantEditorChanged) (GtkWidget *editor,
                                                   gpointer   data);

GType      gtk_inspector_variant_editor_get_type (void);
GtkWidget *gtk_inspector_variant_editor_new      (const GVariantType               *type,
                                                  GtkInspectorVariantEditorChanged  callback,
                                                  gpointer                          data);
void       gtk_inspector_variant_editor_set_type (GtkWidget                        *editor,
                                                  const GVariantType               *type);
void       gtk_inspector_variant_editor_set_value (GtkWidget                       *editor,
                                                   GVariant                        *value);
GVariant * gtk_inspector_variant_editor_get_value (GtkWidget                       *editor);


G_END_DECLS



// vim: set et:
