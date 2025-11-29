/*
 * Copyright © 2021 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_MEASURE_GRAPH (gtk_inspector_measure_graph_get_type ())

G_DECLARE_FINAL_TYPE (GtkInspectorMeasureGraph, gtk_inspector_measure_graph, GTK, INSPECTOR_MEASURE_GRAPH, GObject)

GtkInspectorMeasureGraph * gtk_inspector_measure_graph_new      (void);

void                    gtk_inspector_measure_graph_clear       (GtkInspectorMeasureGraph       *self);
void                    gtk_inspector_measure_graph_measure     (GtkInspectorMeasureGraph       *self,
                                                                 GtkWidget                      *widget);
GdkTexture *            gtk_inspector_measure_graph_get_texture (GtkInspectorMeasureGraph       *self);

G_END_DECLS

