/*
 * Copyright © 2018 Benjamin Otte
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

#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GTK_TYPE_RENDER_NODE_PAINTABLE (gtk_render_node_paintable_get_type ())

G_DECLARE_FINAL_TYPE (GtkRenderNodePaintable, gtk_render_node_paintable, GTK, RENDER_NODE_PAINTABLE, GObject)

GdkPaintable *  gtk_render_node_paintable_new   (GskRenderNode         *node,
                                                 const graphene_rect_t *bounds);

GskRenderNode * gtk_render_node_paintable_get_render_node       (GtkRenderNodePaintable *self);

G_END_DECLS

