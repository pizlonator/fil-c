/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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


#include <gtk/gtkrange.h>


G_BEGIN_DECLS

void               _gtk_range_set_has_origin               (GtkRange      *range,
                                                            gboolean       has_origin);
gboolean           _gtk_range_get_has_origin               (GtkRange      *range);
void               _gtk_range_set_stop_values              (GtkRange      *range,
                                                            double        *values,
                                                            int            n_values);
int                _gtk_range_get_stop_positions           (GtkRange      *range,
                                                            int          **values);

GtkWidget         *gtk_range_get_slider_widget             (GtkRange *range);
GtkWidget         *gtk_range_get_trough_widget             (GtkRange *range);


void               gtk_range_start_autoscroll              (GtkRange      *range,
                                                            GtkScrollType  scroll_type);
void               gtk_range_stop_autoscroll               (GtkRange      *range);

G_END_DECLS


