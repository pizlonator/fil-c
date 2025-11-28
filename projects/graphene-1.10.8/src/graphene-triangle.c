/* graphene-triangle.c: A triangle
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
 * SECTION:graphene-triangle
 * @Title: Triangle
 * @Short_description: A triangle described by 3D points
 *
 * #graphene_triangle_t represents a triangle in 3D space.
 *
 */

#include "graphene-private.h"

#include "graphene-triangle.h"

#include "graphene-alloc-private.h"
#include "graphene-box.h"
#include "graphene-plane.h"
#include "graphene-point3d.h"
#include "graphene-vec2.h"

#include <math.h>

/**
 * graphene_triangle_alloc: (constructor)
 *
 * Allocates a new #graphene_triangle_t.
 *
 * The contents of the returned structure are undefined.
 *
 * Returns: (transfer full): the newly allocated #graphene_triangle_t
 *   structure. Use graphene_triangle_free() to free the resources
 *   allocated by this function
 *
 * Since: 1.2
 */
graphene_triangle_t *
graphene_triangle_alloc (void)
{
  return graphene_aligned_alloc0 (sizeof (graphene_triangle_t), 1, 16);
}

/**
 * graphene_triangle_free:
 * @t: a #graphene_triangle_t
 *
 * Frees the resources allocated by graphene_triangle_alloc().
 *
 * Since: 1.2
 */
void
graphene_triangle_free (graphene_triangle_t *t)
{
  graphene_aligned_free (t);
}

/**
 * graphene_triangle_init_from_point3d:
 * @t: the #graphene_triangle_t to initialize
 * @a: (nullable): a #graphene_point3d_t
 * @b: (nullable): a #graphene_point3d_t
 * @c: (nullable): a #graphene_point3d_t
 *
 * Initializes a #graphene_triangle_t using the three given 3D points.
 *
 * Returns: (transfer none): the initialized #graphene_triangle_t
 *
 * Since: 1.2
 */
graphene_triangle_t *
graphene_triangle_init_from_point3d (graphene_triangle_t      *t,
                                     const graphene_point3d_t *a,
                                     const graphene_point3d_t *b,
                                     const graphene_point3d_t *c)
{
  if (a != NULL)
    graphene_point3d_to_vec3 (a, &t->a);
  else
    graphene_vec3_init_from_vec3 (&t->a, graphene_vec3_zero ());

  if (b != NULL)
    graphene_point3d_to_vec3 (b, &t->b);
  else
    graphene_vec3_init_from_vec3 (&t->b, graphene_vec3_zero ());

  if (c != NULL)
    graphene_point3d_to_vec3 (c, &t->c);
  else
    graphene_vec3_init_from_vec3 (&t->c, graphene_vec3_zero ());

  return t;
}

/**
 * graphene_triangle_init_from_vec3:
 * @t: the #graphene_triangle_t to initialize
 * @a: (nullable): a #graphene_vec3_t
 * @b: (nullable): a #graphene_vec3_t
 * @c: (nullable): a #graphene_vec3_t
 *
 * Initializes a #graphene_triangle_t using the three given vectors.
 *
 * Returns: (transfer none): the initialized #graphene_triangle_t
 *
 * Since: 1.2
 */
graphene_triangle_t *
graphene_triangle_init_from_vec3 (graphene_triangle_t   *t,
                                  const graphene_vec3_t *a,
                                  const graphene_vec3_t *b,
                                  const graphene_vec3_t *c)
{
  if (a != NULL)
    t->a = *a;
  else
    graphene_vec3_init_from_vec3 (&t->a, graphene_vec3_zero ());

  if (b != NULL)
    t->b = *b;
  else
    graphene_vec3_init_from_vec3 (&t->b, graphene_vec3_zero ());

  if (c != NULL)
    t->c = *c;
  else
    graphene_vec3_init_from_vec3 (&t->c, graphene_vec3_zero ());

  return t;
}

/**
 * graphene_triangle_init_from_float:
 * @t: the #graphene_triangle_t to initialize
 * @a: (not nullable) (array fixed-size=3): an array of 3 floating point values
 * @b: (not nullable) (array fixed-size=3): an array of 3 floating point values
 * @c: (not nullable) (array fixed-size=3): an array of 3 floating point values
 *
 * Initializes a #graphene_triangle_t using the three given arrays
 * of floating point values, each representing the coordinates of
 * a point in 3D space.
 *
 * Returns: (transfer none): the initialized #graphene_triangle_t
 *
 * Since: 1.10
 */
graphene_triangle_t *
graphene_triangle_init_from_float (graphene_triangle_t *t,
                                   const float         *a,
                                   const float         *b,
                                   const float         *c)
{
  graphene_vec3_t va, vb, vc;

  return graphene_triangle_init_from_vec3 (t,
                                           graphene_vec3_init_from_float (&va, a),
                                           graphene_vec3_init_from_float (&vb, b),
                                           graphene_vec3_init_from_float (&vc, c));
}

/**
 * graphene_triangle_get_points:
 * @t: a #graphene_triangle_t
 * @a: (out caller-allocates) (optional): return location for the coordinates
 *   of the first vertex
 * @b: (out caller-allocates) (optional): return location for the coordinates
 *   of the second vertex
 * @c: (out caller-allocates) (optional): return location for the coordinates
 *   of the third vertex
 *
 * Retrieves the three vertices of the given #graphene_triangle_t and returns
 * their coordinates as #graphene_point3d_t.
 *
 * Since: 1.2
 */
void
graphene_triangle_get_points (const graphene_triangle_t *t,
                              graphene_point3d_t        *a,
                              graphene_point3d_t        *b,
                              graphene_point3d_t        *c)
{
  if (a != NULL)
    graphene_point3d_init_from_vec3 (a, &t->a);
  if (b != NULL)
    graphene_point3d_init_from_vec3 (b, &t->b);
  if (c != NULL)
    graphene_point3d_init_from_vec3 (c, &t->c);
}

/**
 * graphene_triangle_get_vertices:
 * @t: a #graphene_triangle_t
 * @a: (out caller-allocates) (optional): return location for the first vertex
 * @b: (out caller-allocates) (optional): return location for the second vertex
 * @c: (out caller-allocates) (optional): return location for the third vertex
 *
 * Retrieves the three vertices of the given #graphene_triangle_t.
 *
 * Since: 1.2
 */
void
graphene_triangle_get_vertices (const graphene_triangle_t *t,
                                graphene_vec3_t           *a,
                                graphene_vec3_t           *b,
                                graphene_vec3_t           *c)
{
  if (a != NULL)
    *a = t->a;
  if (b != NULL)
    *b = t->b;
  if (c != NULL)
    *c = t->c;
}

/**
 * graphene_triangle_get_area:
 * @t: a #graphene_triangle_t
 *
 * Computes the area of the given #graphene_triangle_t.
 *
 * Returns: the area of the triangle
 *
 * Since: 1.2
 */
float
graphene_triangle_get_area (const graphene_triangle_t *t)
{
  graphene_vec3_t v1, v2, tmp;

  graphene_vec3_subtract (&t->c, &t->b, &v1);
  graphene_vec3_subtract (&t->a, &t->b, &v2);

  graphene_vec3_cross (&v1, &v2, &tmp);

  return graphene_vec3_length (&tmp) * .5f;
}

/**
 * graphene_triangle_get_midpoint:
 * @t: a #graphene_triangle_t
 * @res: (out caller-allocates): return location for the coordinates of
 *   the midpoint
 *
 * Computes the coordinates of the midpoint of the given #graphene_triangle_t.
 *
 * The midpoint G is the [centroid](https://en.wikipedia.org/wiki/Centroid#Triangle_centroid)
 * of the triangle, i.e. the intersection of its medians.
 *
 * Since: 1.2
 */
void
graphene_triangle_get_midpoint (const graphene_triangle_t *t,
                                graphene_point3d_t        *res)
{
  graphene_vec3_t tmp;

  graphene_vec3_add (&t->a, &t->b, &tmp);
  graphene_vec3_add (&tmp, &t->c, &tmp);
  graphene_vec3_scale (&tmp, (1.f / 3.f), &tmp);

  graphene_point3d_init_from_vec3 (res, &tmp);
}

/**
 * graphene_triangle_get_normal:
 * @t: a #graphene_triangle_t
 * @res: (out caller-allocates): return location for the normal vector
 *
 * Computes the normal vector of the given #graphene_triangle_t.
 *
 * Since: 1.2
 */
void
graphene_triangle_get_normal (const graphene_triangle_t *t,
                              graphene_vec3_t           *res)
{
  graphene_vec3_t v1, v2, tmp;
  float length_sq;

  graphene_vec3_subtract (&t->c, &t->b, &v1);
  graphene_vec3_subtract (&t->a, &t->b, &v2);

  graphene_vec3_cross (&v1, &v2, &tmp);

  length_sq = graphene_vec3_dot (&tmp, &tmp);
  if (length_sq > 0)
    graphene_vec3_scale (&tmp, 1.f / sqrtf (length_sq), res);
  else
    graphene_vec3_init_from_vec3 (res, graphene_vec3_zero ());
}

/**
 * graphene_triangle_get_plane:
 * @t: a #graphene_triangle_t
 * @res: (out caller-allocates): return location for the plane
 *
 * Computes the plane based on the vertices of the given #graphene_triangle_t.
 *
 * Since: 1.2
 */
void
graphene_triangle_get_plane (const graphene_triangle_t *t,
                             graphene_plane_t          *res)
{
  graphene_point3d_t a, b, c;

  graphene_point3d_init_from_vec3 (&a, &t->a);
  graphene_point3d_init_from_vec3 (&b, &t->b);
  graphene_point3d_init_from_vec3 (&c, &t->c);

  graphene_plane_init_from_points (res, &a, &b, &c);
}

/**
 * graphene_triangle_get_bounding_box:
 * @t: a #graphene_triangle_t
 * @res: (out caller-allocates): return location for the box
 *
 * Computes the bounding box of the given #graphene_triangle_t.
 *
 * Since: 1.2
 */
void
graphene_triangle_get_bounding_box (const graphene_triangle_t *t,
                                    graphene_box_t            *res)
{
  graphene_box_init_from_box (res, graphene_box_empty ());
  graphene_box_expand_vec3 (res, &t->a, res);
  graphene_box_expand_vec3 (res, &t->b, res);
  graphene_box_expand_vec3 (res, &t->c, res);
}

/**
 * graphene_triangle_get_uv:
 * @t: a #graphene_triangle_t
 * @p: (nullable): a #graphene_point3d_t
 * @uv_a: the UV coordinates of the first point
 * @uv_b: the UV coordinates of the second point
 * @uv_c: the UV coordinates of the third point
 * @res: (out caller-allocates): a vector containing the UV coordinates
 *   of the given point @p
 *
 * Computes the UV coordinates of the given point @p.
 *
 * The point @p must lie on the same plane as the triangle @t; if the point
 * is not coplanar, the result of this function is undefined. If @p is %NULL,
 * the point will be set in (0, 0, 0).
 *
 * The UV coordinates will be placed in the @res vector:
 *
 *  - `res.x = u`
 *  - `res.y = v`
 *
 * See also: graphene_triangle_get_barycoords()
 *
 * Returns: `true` if the coordinates are valid
 *
 * Since: 1.10
 */
bool
graphene_triangle_get_uv (const graphene_triangle_t *t,
                          const graphene_point3d_t  *p,
                          const graphene_vec2_t     *uv_a,
                          const graphene_vec2_t     *uv_b,
                          const graphene_vec2_t     *uv_c,
                          graphene_vec2_t           *res)
{
  graphene_vec2_t bc;

  graphene_vec2_init (res, 0.f, 0.f);

  if (!graphene_triangle_get_barycoords (t, p, &bc))
    return false;

  float u = graphene_vec2_get_x (&bc);
  float v = graphene_vec2_get_y (&bc);

  /* We have the barycentric coordinates of three points,
   * but barycentric coordinates must always sum to 1, so
   * we can easily derive the third factor
   */
  graphene_point3d_t barycoord =
    GRAPHENE_POINT3D_INIT (1.f - u - v, v, u);

  graphene_vec2_t tmp;

  graphene_vec2_scale (uv_a, barycoord.x, &tmp);
  graphene_vec2_add (res, &tmp, res);

  graphene_vec2_scale (uv_b, barycoord.y, &tmp);
  graphene_vec2_add (res, &tmp, res);

  graphene_vec2_scale (uv_c, barycoord.z, &tmp);
  graphene_vec2_add (res, &tmp, res);

  return true;
}

/**
 * graphene_triangle_get_barycoords:
 * @t: a #graphene_triangle_t
 * @p: (nullable): a #graphene_point3d_t
 * @res: (out caller-allocates): return location for the vector
 *   with the barycentric coordinates
 *
 * Computes the [barycentric coordinates](http://en.wikipedia.org/wiki/Barycentric_coordinate_system)
 * of the given point @p.
 *
 * The point @p must lie on the same plane as the triangle @t; if the
 * point is not coplanar, the result of this function is undefined.
 *
 * If we place the origin in the coordinates of the triangle's A point,
 * the barycentric coordinates are `u`, which is on the AC vector; and `v`
 * which is on the AB vector:
 *
 * ![](triangle-barycentric.png)
 *
 * The returned #graphene_vec2_t contains the following values, in order:
 *
 *  - `res.x = u`
 *  - `res.y = v`
 *
 * Returns: `true` if the barycentric coordinates are valid
 *
 * Since: 1.2
 */
bool
graphene_triangle_get_barycoords (const graphene_triangle_t *t,
                                  const graphene_point3d_t  *p,
                                  graphene_vec2_t           *res)
{
  graphene_vec3_t point;

  if (p == NULL)
    graphene_vec3_init (&point, 0.f, 0.f, 0.f);
  else
    graphene_point3d_to_vec3 (p, &point);

  graphene_vec3_t v0, v1, v2;
  float dot00, dot01, dot02;
  float dot11, dot12;
  float denom, inv_denom;

  graphene_vec3_subtract (&t->c, &t->a, &v0);
  graphene_vec3_subtract (&t->b, &t->a, &v1);
  graphene_vec3_subtract (&point, &t->a, &v2);

  dot00 = graphene_vec3_dot (&v0, &v0);
  dot01 = graphene_vec3_dot (&v0, &v1);
  dot02 = graphene_vec3_dot (&v0, &v2);
  dot11 = graphene_vec3_dot (&v1, &v1);
  dot12 = graphene_vec3_dot (&v1, &v2);

  denom = dot00 * dot11 - dot01 * dot01;
  if (fabsf (denom) <= FLT_EPSILON)
    return false;

  inv_denom = 1.f / denom;

  float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
  float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

  graphene_vec2_init (res, u, v);

  return true;
}

/**
 * graphene_triangle_contains_point:
 * @t: a #graphene_triangle_t
 * @p: a #graphene_point3d_t
 *
 * Checks whether the given triangle @t contains the point @p.
 *
 * Returns: `true` if the point is inside the triangle
 *
 * Since: 1.2
 */
bool
graphene_triangle_contains_point (const graphene_triangle_t *t,
                                  const graphene_point3d_t  *p)
{
  graphene_vec2_t res;

  /* we use the barycoordinates from the given point to check
   * if the point is inside the triangle.
   *
   * see: http://www.blackpawn.com/texts/pointinpoly/default.html
   */
  if (!graphene_triangle_get_barycoords (t, p, &res))
    return false;

  float u = graphene_vec2_get_x (&res);
  float v = graphene_vec2_get_y (&res);

  return (u >= 0.f) && (v >= 0.f) && (u + v < 1.f);
}

static bool
triangle_equal (const void *p1,
                const void *p2)
{
  const graphene_triangle_t *a = p1;
  const graphene_triangle_t *b = p2;

  return graphene_vec3_equal (&a->a, &b->a) &&
         graphene_vec3_equal (&a->b, &b->b) &&
         graphene_vec3_equal (&a->c, &b->c);
}

/**
 * graphene_triangle_equal:
 * @a: a #graphene_triangle_t
 * @b: a #graphene_triangle_t
 *
 * Checks whether the two given #graphene_triangle_t are equal.
 *
 * Returns: `true` if the triangles are equal
 *
 * Since: 1.2
 */
bool
graphene_triangle_equal (const graphene_triangle_t *a,
                         const graphene_triangle_t *b)
{
  return graphene_pointer_equal (a, b, triangle_equal);
}
