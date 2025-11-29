/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
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

#define GTK_TYPE_INSPECTOR_PROP_LIST            (gtk_inspector_prop_list_get_type())
#define GTK_INSPECTOR_PROP_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_PROP_LIST, GtkInspectorPropList))
#define GTK_INSPECTOR_PROP_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_PROP_LIST, GtkInspectorPropListClass))
#define GTK_INSPECTOR_IS_PROP_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_PROP_LIST))
#define GTK_INSPECTOR_IS_PROP_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_PROP_LIST))
#define GTK_INSPECTOR_PROP_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_PROP_LIST, GtkInspectorPropListClass))


typedef struct _GtkInspectorPropListPrivate GtkInspectorPropListPrivate;

typedef struct _GtkInspectorPropList
{
  GtkBox parent;
  GtkInspectorPropListPrivate *priv;
} GtkInspectorPropList;

typedef struct _GtkInspectorPropListClass
{
  GtkBoxClass parent;
} GtkInspectorPropListClass;


G_BEGIN_DECLS

GType      gtk_inspector_prop_list_get_type   (void);
gboolean   gtk_inspector_prop_list_set_object (GtkInspectorPropList *pl,
                                               GObject              *object);
void gtk_inspector_prop_list_set_layout_child (GtkInspectorPropList *pl,
                                               GObject              *object);

void strdup_value_contents (const GValue  *value,
                            char         **contents,
                            char         **type);



G_END_DECLS


// vim: set et sw=2 ts=2:
