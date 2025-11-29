/*
 * Copyright © 2019 Benjamin Otte
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

#include "gsktransform.h"

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef struct _GskTransformClass GskTransformClass;

struct _GskTransform
{
  const GskTransformClass *transform_class;

  GskTransformCategory category;
  GskTransform *next;
};

gboolean                gsk_transform_parser_parse              (GtkCssParser           *parser,
                                                                 GskTransform          **out_transform);

void gsk_matrix_transform_point   (const graphene_matrix_t  *m,
                                   const graphene_point_t   *p,
                                   graphene_point_t         *res);
void gsk_matrix_transform_point3d (const graphene_matrix_t  *m,
                                   const graphene_point3d_t *p,
                                   graphene_point3d_t       *res);
void gsk_matrix_transform_bounds  (const graphene_matrix_t  *m,
                                   const graphene_rect_t    *r,
                                   graphene_rect_t          *res);
void gsk_matrix_transform_rect    (const graphene_matrix_t  *m,
                                   const graphene_rect_t    *r,
                                   graphene_quad_t          *res);

#define gsk_transform_get_category(t) ((t) ? (t)->category : GSK_TRANSFORM_CATEGORY_IDENTITY)

G_END_DECLS


