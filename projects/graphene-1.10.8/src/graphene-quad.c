/* graphene-quad.c: Quad
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * SECTION:graphene-quad
 * @title: Quad
 * @short_description: Four-vertex quadrilateral
 *
 * A #graphene_quad_t represents a coplanar, four vertex quadrilateral shape.
 */

#include "graphene-private.h"

#include "graphene-quad.h"

#include "graphene-line-segment-private.h"
#include "graphene-rect.h"
#include "graphene-simd4f.h"

#include <string.h>

/**
 * graphene_quad_alloc: (constructor)
 *
 * Allocates a new #graphene_quad_t instance.
 *
 * The contents of the returned instance are undefined.
 *
 * Returns: (transfer full): the newly created #graphene_quad_t instance
 *
 * Since: 1.0
 */
graphene_quad_t *
graphene_quad_alloc (void)
{
  return calloc (1, sizeof (graphene_quad_t));
}

/**
 * graphene_quad_free:
 * @q: a #graphene_quad_t
 *
 * Frees the resources allocated by graphene_quad_alloc()
 *
 * Since: 1.0
 */
void
graphene_quad_free (graphene_quad_t *q)
{
  free (q);
}

/**
 * graphene_quad_init:
 * @q: the #graphene_quad_t to initialize
 * @p1: the first point of the quadrilateral
 * @p2: the second point of the quadrilateral
 * @p3: the third point of the quadrilateral
 * @p4: the fourth point of the quadrilateral
 *
 * Initializes a #graphene_quad_t with the given points.
 *
 * Returns: (transfer none): the initialized #graphene_quad_t
 *
 * Since: 1.0
 */
graphene_quad_t *
graphene_quad_init (graphene_quad_t        *q,
                    const graphene_point_t *p1,
                    const graphene_point_t *p2,
                    const graphene_point_t *p3,
                    const graphene_point_t *p4)
{
  q->points[0] = *p1;
  q->points[1] = *p2;
  q->points[2] = *p3;
  q->points[3] = *p4;

  return q;
}

/**
 * graphene_quad_init_from_rect:
 * @q: the #graphene_quad_t to initialize
 * @r: a #graphene_rect_t
 *
 * Initializes a #graphene_quad_t using the four corners of the
 * given #graphene_rect_t.
 *
 * Returns: (transfer none): the initialized #graphene_quad_t
 *
 * Since: 1.0
 */
graphene_quad_t *
graphene_quad_init_from_rect (graphene_quad_t       *q,
                              const graphene_rect_t *r)
{
  graphene_rect_get_top_left (r, &(q->points[0]));
  graphene_rect_get_top_right (r, &(q->points[1]));
  graphene_rect_get_bottom_right (r, &(q->points[2]));
  graphene_rect_get_bottom_left (r, &(q->points[3]));

  return q;
}

/**
 * graphene_quad_init_from_points:
 * @q: the #graphene_quad_t to initialize
 * @points: (array fixed-size=4): an array of 4 #graphene_point_t
 *
 * Initializes a #graphene_quad_t using an array of points.
 *
 * Returns: (transfer none): the initialized #graphene_quad_t
 *
 * Since: 1.2
 */
graphene_quad_t *
graphene_quad_init_from_points (graphene_quad_t        *q,
                                const graphene_point_t  points[])
{
  memcpy (q->points, points, sizeof (graphene_point_t) * 4);

  return q;
}

/**
 * graphene_quad_contains:
 * @q: a #graphene_quad_t
 * @p: a #graphene_point_t
 *
 * Checks if the given #graphene_quad_t contains the given #graphene_point_t.
 *
 * Returns: `true` if the point is inside the #graphene_quad_t
 *
 * Since: 1.0
 */
bool
graphene_quad_contains (const graphene_quad_t  *q,
                        const graphene_point_t *p)
{
  graphene_line_segment_t l1, l2, l3, l4;

  l1 = graphene_line_segment_init (&q->points[0], &q->points[1]);
  l2 = graphene_line_segment_init (&q->points[1], &q->points[2]);
  l3 = graphene_line_segment_init (&q->points[2], &q->points[3]);
  l4 = graphene_line_segment_init (&q->points[3], &q->points[0]);

  return graphene_line_segment_points_on_same_side (l1, p, &(q->points[2])) &&
         graphene_line_segment_points_on_same_side (l2, p, &(q->points[3])) &&
         graphene_line_segment_points_on_same_side (l3, p, &(q->points[0])) &&
         graphene_line_segment_points_on_same_side (l4, p, &(q->points[1]));
}

/**
 * graphene_quad_bounds:
 * @q: a #graphene_quad_t
 * @r: (out caller-allocates): return location for a #graphene_rect_t
 *
 * Computes the bounding rectangle of @q and places it into @r.
 *
 * Since: 1.0
 */
void
graphene_quad_bounds (const graphene_quad_t *q,
                      graphene_rect_t       *r)
{
  graphene_simd4f_t vx, vy;
  float min_x, max_x;
  float min_y, max_y;

  vx = graphene_simd4f_init (q->points[0].x, q->points[1].x, q->points[2].x, q->points[3].x);
  vy = graphene_simd4f_init (q->points[0].y, q->points[1].y, q->points[2].y, q->points[3].y);

  min_x = graphene_simd4f_get_x (graphene_simd4f_min_val (vx));
  min_y = graphene_simd4f_get_x (graphene_simd4f_min_val (vy));

  max_x = graphene_simd4f_get_x (graphene_simd4f_max_val (vx));
  max_y = graphene_simd4f_get_x (graphene_simd4f_max_val (vy));

  graphene_rect_init (r, min_x, min_y, max_x - min_x, max_y - min_y);
}

/**
 * graphene_quad_get_point:
 * @q: a #graphene_quad_t
 * @index_: the index of the point to retrieve
 *
 * Retrieves the point of a #graphene_quad_t at the given index.
 *
 * Returns: (transfer none): a #graphene_point_t
 *
 * Since: 1.0
 */
const graphene_point_t *
graphene_quad_get_point (const graphene_quad_t *q,
                         unsigned int           index_)
{
  return &q->points[index_];
}
