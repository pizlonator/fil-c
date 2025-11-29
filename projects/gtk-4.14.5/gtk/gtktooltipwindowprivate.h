/* GTK - The GIMP Toolkit
 * Copyright 2015  Emmanuele Bassi 
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_TOOLTIP_WINDOW (gtk_tooltip_window_get_type ())

G_DECLARE_FINAL_TYPE (GtkTooltipWindow, gtk_tooltip_window, GTK, TOOLTIP_WINDOW, GtkWidget)

GtkWidget *     gtk_tooltip_window_new                          (void);

void            gtk_tooltip_window_present                      (GtkTooltipWindow *window);

void            gtk_tooltip_window_set_label_markup             (GtkTooltipWindow *window,
                                                                 const char       *markup);
void            gtk_tooltip_window_set_label_text               (GtkTooltipWindow *window,
                                                                 const char       *text);
void            gtk_tooltip_window_set_image_icon               (GtkTooltipWindow *window,
                                                                 GdkPaintable     *paintable);
void            gtk_tooltip_window_set_image_icon_from_name     (GtkTooltipWindow *window,
                                                                 const char       *icon_name);
void            gtk_tooltip_window_set_image_icon_from_gicon    (GtkTooltipWindow *window,
                                                                 GIcon            *gicon);
void            gtk_tooltip_window_set_custom_widget            (GtkTooltipWindow *window,
                                                                 GtkWidget        *custom_widget);
void            gtk_tooltip_window_set_relative_to              (GtkTooltipWindow *window,
                                                                 GtkWidget        *relative_to);
void            gtk_tooltip_window_position                     (GtkTooltipWindow *window,
                                                                 GdkRectangle     *rect,
                                                                 GdkGravity        rect_anchor,
                                                                 GdkGravity        surface_anchor,
                                                                 GdkAnchorHints    anchor_hints,
                                                                 int               dx,
                                                                 int               dy);

G_END_DECLS

