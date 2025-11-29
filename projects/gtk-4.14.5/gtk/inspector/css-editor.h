/*
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_CSS_EDITOR            (gtk_inspector_css_editor_get_type())
#define GTK_INSPECTOR_CSS_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_CSS_EDITOR, GtkInspectorCssEditor))
#define GTK_INSPECTOR_CSS_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_CSS_EDITOR, GtkInspectorCssEditorClass))
#define GTK_INSPECTOR_IS_CSS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_CSS_EDITOR))
#define GTK_INSPECTOR_IS_CSS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_CSS_EDITOR))
#define GTK_INSPECTOR_CSS_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_CSS_EDITOR, GtkInspectorCssEditorClass))


typedef struct _GtkInspectorCssEditorPrivate GtkInspectorCssEditorPrivate;

typedef struct _GtkInspectorCssEditor
{
  GtkBox parent;
  GtkInspectorCssEditorPrivate *priv;
} GtkInspectorCssEditor;

typedef struct _GtkInspectorCssEditorClass
{
  GtkBoxClass parent;
} GtkInspectorCssEditorClass;

G_BEGIN_DECLS

GType      gtk_inspector_css_editor_get_type   (void);
void       gtk_inspector_css_editor_set_display (GtkInspectorCssEditor *ce,
                                                 GdkDisplay            *display);

G_END_DECLS


// vim: set et sw=2 ts=2:
