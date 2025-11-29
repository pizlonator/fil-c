/*
 * Copyright (c) 2014 Benjamin Otte <otte@gnome.org>
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
#include "graphdata.h"

G_BEGIN_DECLS

typedef struct _GraphRenderer GraphRenderer;

G_DECLARE_FINAL_TYPE (GraphRenderer, graph_renderer, GRAPH, RENDERER, GtkWidget);

GraphRenderer *graph_renderer_new      (void);

void           graph_renderer_set_data (GraphRenderer *self,
                                        GraphData     *data);

G_END_DECLS

