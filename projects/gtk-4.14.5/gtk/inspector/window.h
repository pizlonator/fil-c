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


#include <gtk/gtkwindow.h>

#include "inspectoroverlay.h"

#define GTK_TYPE_INSPECTOR_WINDOW            (gtk_inspector_window_get_type())
#define GTK_INSPECTOR_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_WINDOW, GtkInspectorWindow))
#define GTK_INSPECTOR_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_WINDOW, GtkInspectorWindowClass))
#define GTK_INSPECTOR_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_WINDOW))
#define GTK_INSPECTOR_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_WINDOW))
#define GTK_INSPECTOR_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_WINDOW, GtkInspectorWindowClass))


#define TREE_TEXT_SCALE 0.8
#define TREE_CHECKBOX_SIZE (int)(0.8 * 13)

typedef struct
{
  GtkWindow parent;

  GtkWidget *top_stack;
  GtkWidget *object_stack;
  GtkWidget *button_stack;
  GtkWidget *object_tree;
  GtkWidget *object_id;
  GtkWidget *object_details;
  GtkWidget *object_buttons;
  GtkWidget *object_details_button;
  GtkWidget *select_object;
  GtkWidget *object_start_stack;
  GtkWidget *object_center_stack;
  GtkWidget *object_title;
  GtkWidget *prop_list;
  GtkWidget *layout_prop_list;
  GtkWidget *selector;
  GtkWidget *signals_list;
  GtkWidget *classes_list;
  GtkWidget *widget_css_node_tree;
  GtkWidget *widget_recorder;
  GtkWidget *object_hierarchy;
  GtkWidget *size_groups;
  GtkWidget *tree_data;
  GtkWidget *list_data;
  GtkWidget *actions;
  GtkWidget *shortcuts;
  GtkWidget *menu;
  GtkWidget *misc_info;
  GtkWidget *controllers;
  GtkWidget *magnifier;
  GtkWidget *a11y;
  GtkWidget *sidebar_revealer;
  GtkWidget *css_editor;
  GtkWidget *visual;
  GtkWidget *clipboard;
  GtkWidget *general;
  GtkWidget *logs;

  GtkWidget *go_up_button;
  GtkWidget *go_down_button;
  GtkWidget *go_previous_button;
  GtkWidget *list_position_label;
  GtkWidget *go_next_button;

  GList *extra_pages;

  GdkSeat *grab_seat;

  GtkInspectorOverlay *flash_overlay;
  int flash_count;
  int flash_cnx;

  GArray *objects;

  GList *overlays;

  GdkDisplay *inspected_display;

} GtkInspectorWindow;

typedef struct
{
  GtkWindowClass parent;
} GtkInspectorWindowClass;


G_BEGIN_DECLS

GType      gtk_inspector_window_get_type    (void);
GtkWidget *gtk_inspector_window_get         (GdkDisplay *display);

void       gtk_inspector_flash_widget       (GtkInspectorWindow *iw,
                                             GtkWidget          *widget);

void       gtk_inspector_on_inspect         (GtkWidget          *widget,
                                             GtkInspectorWindow *iw);

void                    gtk_inspector_window_add_overlay                        (GtkInspectorWindow     *iw,
                                                                                 GtkInspectorOverlay    *overlay);
void                    gtk_inspector_window_remove_overlay                     (GtkInspectorWindow     *iw,
                                                                                 GtkInspectorOverlay    *overlay);

void                    gtk_inspector_window_select_widget_under_pointer        (GtkInspectorWindow     *iw);
GdkDisplay *            gtk_inspector_window_get_inspected_display              (GtkInspectorWindow     *iw);

typedef enum
{
  CHILD_KIND_WIDGET,
  CHILD_KIND_CONTROLLER,
  CHILD_KIND_PROPERTY,
  CHILD_KIND_LISTITEM,
  CHILD_KIND_OTHER
} ChildKind;

void                    gtk_inspector_window_push_object     (GtkInspectorWindow *iw,
                                                              GObject            *object,
                                                              ChildKind           kind,
                                                              guint               position);
void                    gtk_inspector_window_pop_object      (GtkInspectorWindow *iw);
void                    gtk_inspector_window_set_object      (GtkInspectorWindow *iw,
                                                              GObject            *object,
                                                              ChildKind           kind,
                                                              guint               position);
void                    gtk_inspector_window_replace_object  (GtkInspectorWindow *iw,
                                                              GObject            *object,
                                                              ChildKind           kind,
                                                              guint               position);

gboolean                gtk_inspector_is_recording                              (GtkWidget              *widget);
GskRenderNode *         gtk_inspector_prepare_render                            (GtkWidget              *widget,
                                                                                 GskRenderer            *renderer,
                                                                                 GdkSurface             *surface,
                                                                                 const cairo_region_t   *region,
                                                                                 GskRenderNode          *root,
                                                                                 GskRenderNode          *widget_node);
gboolean                gtk_inspector_handle_event                              (GdkEvent               *event);

G_END_DECLS



// vim: set et sw=2 ts=2:
