/*
 * Copyright © 2020 Benjamin Otte
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

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gskroundedrect.h>
#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH_BUILDER (gsk_path_builder_get_type ())

GDK_AVAILABLE_IN_4_14
GType                   gsk_path_builder_get_type               (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_14
GskPathBuilder *        gsk_path_builder_new                    (void);
GDK_AVAILABLE_IN_4_14
GskPathBuilder *        gsk_path_builder_ref                    (GskPathBuilder         *self);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_unref                  (GskPathBuilder         *self);
GDK_AVAILABLE_IN_4_14
GskPath *               gsk_path_builder_free_to_path           (GskPathBuilder         *self) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_4_14
GskPath *               gsk_path_builder_to_path                (GskPathBuilder         *self) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_4_14
const graphene_point_t *gsk_path_builder_get_current_point      (GskPathBuilder         *self);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_path               (GskPathBuilder         *self,
                                                                 GskPath                *path);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_reverse_path       (GskPathBuilder         *self,
                                                                 GskPath                *path);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_cairo_path         (GskPathBuilder         *self,
                                                                 const cairo_path_t     *path);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_layout             (GskPathBuilder         *self,
                                                                 PangoLayout            *layout);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_rect               (GskPathBuilder         *self,
                                                                 const graphene_rect_t  *rect);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_rounded_rect       (GskPathBuilder         *self,
                                                                 const GskRoundedRect   *rect);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_circle             (GskPathBuilder         *self,
                                                                 const graphene_point_t *center,
                                                                 float                   radius);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_add_segment            (GskPathBuilder         *self,
                                                                 GskPath                *path,
                                                                 const GskPathPoint     *start,
                                                                 const GskPathPoint     *end);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_move_to                (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_move_to            (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_line_to                (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_line_to            (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_quad_to                (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_quad_to            (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_cubic_to               (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   x3,
                                                                 float                   y3);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_cubic_to           (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   x3,
                                                                 float                   y3);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_conic_to               (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   weight);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_conic_to           (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   weight);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_arc_to                 (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_arc_to             (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_svg_arc_to             (GskPathBuilder         *self,
                                                                 float                   rx,
                                                                 float                   ry,
                                                                 float                   x_axis_rotation,
                                                                 gboolean                large_arc,
                                                                 gboolean                positive_sweep,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_svg_arc_to         (GskPathBuilder         *self,
                                                                 float                   rx,
                                                                 float                   ry,
                                                                 float                   x_axis_rotation,
                                                                 gboolean                large_arc,
                                                                 gboolean                positive_sweep,
                                                                 float                   x,
                                                                 float                   y);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_html_arc_to            (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   radius);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_rel_html_arc_to        (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   radius);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_builder_close                  (GskPathBuilder         *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPathBuilder, gsk_path_builder_unref)

G_END_DECLS
