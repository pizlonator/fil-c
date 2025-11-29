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

#include "gskpathprivate.h"
#include "gskpathpoint.h"
#include "gskpathopprivate.h"
#include "gskboundingboxprivate.h"

G_BEGIN_DECLS

GskContour *            gsk_standard_contour_new                (GskPathFlags            flags,
                                                                 const graphene_point_t *points,
                                                                 gsize                   n_points,
                                                                 const gskpathop        *ops,
                                                                 gsize                   n_ops,
                                                                 gssize                  offset);

GskContour *            gsk_circle_contour_new                  (const graphene_point_t *center,
                                                                 float                   radius);
GskContour *            gsk_rect_contour_new                    (const graphene_rect_t  *rect);
GskContour *            gsk_rounded_rect_contour_new            (const GskRoundedRect   *rounded_rect);

const char *            gsk_contour_get_type_name               (const GskContour       *self);
void                    gsk_contour_copy                        (GskContour *            dest,
                                                                 const GskContour       *src);
GskContour *            gsk_contour_dup                         (const GskContour       *src);
GskContour *            gsk_contour_reverse                     (const GskContour       *src);

gsize                   gsk_contour_get_size                    (const GskContour       *self);
GskPathFlags            gsk_contour_get_flags                   (const GskContour       *self);
void                    gsk_contour_print                       (const GskContour       *self,
                                                                 GString                *string);
gboolean                gsk_contour_get_bounds                  (const GskContour       *self,
                                                                 GskBoundingBox         *bounds);
gboolean                gsk_contour_get_stroke_bounds           (const GskContour       *self,
                                                                 const GskStroke        *stroke,
                                                                 GskBoundingBox         *bounds);
gboolean                gsk_contour_foreach                     (const GskContour       *self,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);
int                     gsk_contour_get_winding                 (const GskContour       *self,
                                                                 const graphene_point_t *point);
gsize                   gsk_contour_get_n_ops                   (const GskContour       *self);
gboolean                gsk_contour_get_closest_point           (const GskContour       *self,
                                                                 const graphene_point_t *point,
                                                                 float                   threshold,
                                                                 GskPathPoint           *result,
                                                                 float                  *out_dist);
void                    gsk_contour_get_position                (const GskContour       *self,
                                                                 const GskPathPoint     *point,
                                                                 graphene_point_t       *pos);
void                    gsk_contour_get_tangent                 (const GskContour       *self,
                                                                 const GskPathPoint     *point,
                                                                 GskPathDirection        direction,
                                                                 graphene_vec2_t        *tangent);
float                   gsk_contour_get_curvature               (const GskContour       *self,
                                                                 const GskPathPoint     *point,
                                                                 GskPathDirection        direction,
                                                                 graphene_point_t       *center);
void                    gsk_contour_add_segment                 (const GskContour       *self,
                                                                 GskPathBuilder         *builder,
                                                                 gboolean                emit_move_to,
                                                                 const GskPathPoint     *start,
                                                                 const GskPathPoint     *end);

gpointer                gsk_contour_init_measure                (const GskContour       *self,
                                                                 float                   tolerance,
                                                                 float                  *out_length);
void                    gsk_contour_free_measure                (const GskContour       *self,
                                                                 gpointer                data);
void                    gsk_contour_get_point                   (const GskContour       *self,
                                                                 gpointer                measure_data,
                                                                 float                   distance,
                                                                 GskPathPoint           *result);
float                   gsk_contour_get_distance                (const GskContour       *self,
                                                                 const GskPathPoint     *point,
                                                                 gpointer                measure_data);

G_END_DECLS
