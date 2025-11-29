/*
 * Copyright (c) 2014 Benjamin Otte <ottte@gnome.org>
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

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GraphData        GraphData;

G_DECLARE_FINAL_TYPE (GraphData, graph_data, GRAPH, DATA, GObject)

GraphData       *graph_data_new             (guint        n_values);

guint            graph_data_get_n_values    (GraphData   *data);
double           graph_data_get_value       (GraphData   *data,
                                             guint        i);
double           graph_data_get_minimum     (GraphData   *data);
double           graph_data_get_maximum     (GraphData   *data);

void             graph_data_prepend_value   (GraphData   *data,
                                             double       value);

G_END_DECLS

