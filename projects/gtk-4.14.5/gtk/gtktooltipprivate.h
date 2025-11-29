/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
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


#include <gtk/gtktooltip.h>
#include <gtk/gtknative.h>


G_BEGIN_DECLS

void _gtk_tooltip_handle_event           (GtkWidget          *target,
                                          GdkEvent           *event);
void _gtk_tooltip_hide                   (GtkWidget          *widget);
void gtk_tooltip_trigger_tooltip_query   (GtkWidget          *widget);

GtkWidget * _gtk_widget_find_at_coords   (GdkSurface         *surface,
                                          int                 surface_x,
                                          int                 surface_y,
                                          int                *widget_x,
                                          int                *widget_y);

void gtk_tooltip_maybe_allocate          (GtkNative          *native);
void gtk_tooltip_unset_surface           (GtkNative          *native);

G_END_DECLS


