/* graphene-ray.c: A ray emitted from an origin in a given direction
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2015  Emmanuele Bassi
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
 * SECTION:graphene-ray
 * @Title: Ray
 * @Short_Description: A ray emitted from an origin in a given direction
 *
 * #graphene_ray_t is a structure representing a ray emitted by an origin,
 * identified by a point in 3D space, in a given direction, identified
 * by a vector with 3 components.
 *
 * A common use of #graphene_ray_t is ray-casting to find which objects in
 * a 3D scene are under the coordinates of the pointer.
 */

#include "graphene-private.h"

#include "graphene-ray.h"

#include "graphene-alloc-private.h"
#include "graphene-box.h"
#include "graphene-plane.h"
#include "graphene-point3d.h"
#include "graphene-simd4f.h"
#include "graphene-sphere.h"
#include "graphene-vec3.h"
#include "graphene-triangle.h"

#include <math.h>
#include <float.h>

/**
 * graphene_ray_alloc: (constructor)
 *
 * Allocates a new #graphene_ray_t structure.
 *
 * The contents of the returned structure are undefined.
 *
 * Returns: (transfer full): the newly allocated #graphene_ray_t.
 *   Use graphene_ray_free() to free the resources allocated by
 *   this function
 *
 * Since: 1.4
 */
graphene_ray_t *
graphene_ray_alloc (void)
{
  return graphene_aligned_alloc (sizeof (graphene_ray_t), 1, 16);
}

/**
 * graphene_ray_free:
 * @r: a #graphene_ray_t
 *
 * Frees the resources allocated by graphene_ray_alloc().
 *
 * Since: 1.4
 */
void
graphene_ray_free (graphene_ray_t *r)
{
  graphene_aligned_free (r);
}

/**
 * graphene_ray_init:
 * @r: the #graphene_ray_t to initialize
 * @origin: (nullable): the origin of the ray
 * @direction: (nullable): the direction vector
 *
 * Initializes the given #graphene_ray_t using the given @origin
 * and @direction values.
 *
 * Returns: (transfer none): the initialized ray
 *
 * Since: 1.4
 */
graphene_ray_t *
graphene_ray_init (graphene_ray_t           *r,
                   const graphene_point3d_t *origin,
                   const graphene_vec3_t    *direction)
{
  if (origin != NULL)
    graphene_point3d_to_vec3 (origin, &r->origin);
  else
    graphene_vec3_init_from_vec3 (&r->origin, graphene_vec3_zero ());

  if (direction != NULL)
    graphene_vec3_normalize (direction, &r->direction);
  else
    graphene_vec3_init_from_vec3 (&r->direction, graphene_vec3_zero ());

  return r;
}

/**
 * graphene_ray_init_from_ray:
 * @r: the #graphene_ray_t to initialize
 * @src: a #graphene_ray_t
 *
 * Initializes the given #graphene_ray_t using the origin and direction
 * values of another #graphene_ray_t.
 *
 * Returns: (transfer none): the initialized ray
 *
 * Since: 1.4
 */
graphene_ray_t *
graphene_ray_init_from_ray (graphene_ray_t       *r,
                            const graphene_ray_t *src)
{
  return graphene_ray_init_from_vec3 (r, &src->origin, &src->direction);
}

/**
 * graphene_ray_init_from_vec3:
 * @r: the #graphene_ray_t to initialize
 * @origin: (nullable): a #graphene_vec3_t
 * @direction: (nullable): a #graphene_vec3_t
 *
 * Initializes the given #graphene_ray_t using the given vectors.
 *
 * Returns: (transfer none): the initialized ray
 *
 * Since: 1.4
 */
graphene_ray_t *
graphene_ray_init_from_vec3 (graphene_ray_t        *r,
                             const graphene_vec3_t *origin,
                             const graphene_vec3_t *direction)
{
  if (origin != NULL)
    graphene_vec3_init_from_vec3 (&r->origin, origin);
  else
    graphene_vec3_init_from_vec3 (&r->origin, graphene_vec3_zero ());

  if (direction != NULL)
    graphene_vec3_normalize (direction, &r->direction);
  else
    graphene_vec3_init_from_vec3 (&r->direction, graphene_vec3_zero ());

  return r;
}

/**
 * graphene_ray_get_origin:
 * @r: a #graphene_ray_t
 * @origin: (out caller-allocates): return location for the origin
 *
 * Retrieves the origin of the given #graphene_ray_t.
 *
 * Since: 1.4
 */
void
graphene_ray_get_origin (const graphene_ray_t *r,
                         graphene_point3d_t   *origin)
{
  graphene_point3d_init_from_vec3 (origin, &r->origin);
}

/**
 * graphene_ray_get_direction:
 * @r: a #graphene_ray_t
 * @direction: (out caller-allocates): return location for the direction
 *
 * Retrieves the direction of the given #graphene_ray_t.
 *
 * Since: 1.4
 */
void
graphene_ray_get_direction (const graphene_ray_t *r,
                            graphene_vec3_t      *direction)
{
  graphene_vec3_init_from_vec3 (direction, &r->direction);
}

/**
 * graphene_ray_get_position_at:
 * @r: a #graphene_ray_t
 * @t: the distance along the ray
 * @position: (out caller-allocates): return location for the position
 *
 * Retrieves the coordinates of a point at the distance @t along the
 * given #graphene_ray_t.
 *
 * Since: 1.4
 */
void
graphene_ray_get_position_at (const graphene_ray_t *r,
                              float                 t,
                              graphene_point3d_t   *position)
{
  graphene_vec3_t res;

  graphene_vec3_scale (&r->direction, t, &res);
  graphene_vec3_add (&res, &r->origin, &res);

  graphene_point3d_init_from_vec3 (position, &res);
}

/**
 * graphene_ray_get_distance_to_point:
 * @r: a #graphene_ray_t
 * @p: a #graphene_point3d_t
 *
 * Computes the distance of the closest approach between the
 * given #graphene_ray_t @r and the point @p.
 *
 * The closest approach to a ray from a point is the distance
 * between the point and the projection of the point on the
 * ray itself.
 *
 * Returns: the distance of the point
 *
 * Since: 1.4
 */
float
graphene_ray_get_distance_to_point (const graphene_ray_t     *r,
                                    const graphene_point3d_t *p)
{
  graphene_vec3_t point;
  graphene_vec3_t tmp;
  float distance;

  graphene_point3d_to_vec3 (p, &point);

  graphene_vec3_subtract (&point, &r->origin, &tmp);
  distance = graphene_vec3_dot (&tmp, &r->direction);

  /* the point is behind the ray */
  if (distance < 0)
    {
      graphene_vec3_subtract (&r->origin, &point, &tmp);
      return graphene_vec3_length (&tmp);
    }

  /* get the position on the ray at the given distance */
  graphene_vec3_scale (&r->direction, distance, &tmp);
  graphene_vec3_add (&tmp, &r->origin, &tmp);

  /* distance */
  graphene_vec3_subtract (&tmp, &point, &tmp);
  return graphene_vec3_length (&tmp);
}

/**
 * graphene_ray_get_distance_to_plane:
 * @r: a #graphene_ray_t
 * @p: a #graphene_plane_t
 *
 * Computes the distance of the origin of the given #graphene_ray_t from the
 * given plane.
 *
 * If the ray does not intersect the plane, this function returns `INFINITY`.
 *
 * Returns: the distance of the origin of the ray from the plane
 *
 * Since: 1.4
 */
float
graphene_ray_get_distance_to_plane (const graphene_ray_t   *r,
                                    const graphene_plane_t *p)
{
  float denom, t;

  denom = graphene_vec3_dot (&p->normal, &r->direction);
  if (fabsf (denom) < GRAPHENE_FLOAT_EPSILON)
    {
      graphene_point3d_t tmp;

      /* If the ray is coplanar, return 0 */
      graphene_point3d_init_from_vec3 (&tmp, &r->origin);
      if (fabsf (graphene_plane_distance (p, &tmp)) < GRAPHENE_FLOAT_EPSILON)
        return 0.f;

      return INFINITY;
    }

  t = -1.f * (graphene_vec3_dot (&r->origin, &p->normal) + p->constant) / denom;
  if (t >= 0.f)
    return t;

  return INFINITY;
}

static bool
ray_equal (const void *p1,
           const void *p2)
{
  const graphene_ray_t *a = p1;
  const graphene_ray_t *b = p2;

  return graphene_vec3_equal (&a->origin, &b->origin) &&
         graphene_vec3_equal (&a->direction, &b->direction);
}

/**
 * graphene_ray_equal:
 * @a: a #graphene_ray_t
 * @b: a #graphene_ray_t
 *
 * Checks whether the two given #graphene_ray_t are equal.
 *
 * Returns: `true` if the given rays are equal
 *
 * Since: 1.4
 */
bool
graphene_ray_equal (const graphene_ray_t *a,
                    const graphene_ray_t *b)
{
  return graphene_pointer_equal (a, b, ray_equal);
}

/**
 * graphene_ray_get_closest_point_to_point:
 * @r: a #graphene_ray_t
 * @p: a #graphene_point3d_t
 * @res: (out caller-allocates): return location for the closest point3d
 *
 * Computes the point on the given #graphene_ray_t that is closest to the
 * given point @p.
 *
 * Since: 1.4
 */
void
graphene_ray_get_closest_point_to_point (const graphene_ray_t     *r,
                                         const graphene_point3d_t *p,
                                         graphene_point3d_t       *res)
{
  graphene_vec3_t point, result;
  float distance;

  graphene_point3d_to_vec3 (p, &point);
  graphene_vec3_subtract (&point, &r->origin, &result);

  distance = graphene_vec3_dot (&result, &r->direction);
  if (distance < 0)
    graphene_vec3_init_from_vec3 (&result, &r->origin);
  else
    {
      graphene_vec3_scale (&r->direction, distance, &result);
      graphene_vec3_add (&result, &r->origin, &result);
    }

  graphene_point3d_init_from_vec3 (res, &result);
}

/**
 * graphene_ray_intersect_sphere:
 * @r: a #graphene_ray_t
 * @s: a #graphene_sphere_t
 * @t_out: (out): the distance of the point on the ray that intersects the sphere
 *
 * Intersects the given #graphene_ray_t @r with the given
 * #graphene_sphere_t @s.
 *
 * Returns: the type of intersection
 *
 * Since: 1.10
 */
graphene_ray_intersection_kind_t
graphene_ray_intersect_sphere (const graphene_ray_t    *r,
                               const graphene_sphere_t *s,
                               float                   *t_out)
{
  graphene_vec3_t v1;

  graphene_vec3_subtract (&s->center, &r->origin, &v1);

  /* initialize t_out, if set, so we don't have to do that every
   * time we bail out with no intersection
   */
  if (t_out != NULL)
    *t_out = 0.f;

  /* (signed) distance along ray to point nearest sphere center */
  float tca = graphene_vec3_dot (&v1, &r->direction);

  /* square of distance from ray line to sphere center */
  float d2 = graphene_vec3_dot (&v1, &v1) - tca * tca;

  float radius2 = s->radius * s->radius;
  if (d2 > radius2)
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  /* Distance to entry/exit point */
  float thc = sqrtf (radius2 - d2);

  /* t0 = first intersect point - entrance on front of sphere */
  float t0 = tca - thc;

  /* t1 = second intersect point - exit point on back of sphere */
  float t1 = tca + thc;

  // test to see if both t0 and t1 are behind the ray - if so, no intersection
  if (t0 < 0 && t1 < 0)
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  /* test to see if t0 is behind the ray.
   *
   * 1) if it is, the ray is inside the sphere, so return t1,
   * in order to always return an intersect point that is
   * in front of the ray.
   */
  if (t0 < 0)
    {
      if (t_out)
        *t_out = t1;

      return GRAPHENE_RAY_INTERSECTION_KIND_LEAVE;
    }

  /* 2) t0 is in front of the ray, so return t0 */
  if (t_out)
    *t_out = t0;

  return GRAPHENE_RAY_INTERSECTION_KIND_ENTER;
}

/**
 * graphene_ray_intersects_sphere:
 * @r: a #graphene_ray_t
 * @s: a #graphene_sphere_t
 *
 * Checks if the given #graphene_ray_t @r intersects the
 * given #graphene_sphere_t @s.
 *
 * See also: graphene_ray_intersect_sphere()
 *
 * Returns: `true` if the ray intersects the sphere
 *
 * Since: 1.10
 */
bool
graphene_ray_intersects_sphere (const graphene_ray_t    *r,
                                const graphene_sphere_t *s)
{
  return graphene_ray_intersect_sphere (r, s, NULL) != GRAPHENE_RAY_INTERSECTION_KIND_NONE;
}

/**
 * graphene_ray_intersect_box:
 * @r: a #graphene_ray_t
 * @b: a #graphene_box_t
 * @t_out: (out): the distance of the point on the ray that intersects the box
 *
 * Intersects the given #graphene_ray_t @r with the given
 * #graphene_box_t @b.
 *
 * Returns: the type of intersection
 *
 * Since: 1.10
 */
graphene_ray_intersection_kind_t
graphene_ray_intersect_box (const graphene_ray_t *r,
                            const graphene_box_t *b,
                            float                *t_out)
{
  graphene_vec3_t inv_dir;

  /* FIXME: Needs a graphene_vec3_reciprocal() */
  inv_dir.value = graphene_simd4f_reciprocal (r->direction.value);

  graphene_vec3_t inv_min;
  graphene_vec3_subtract (&(b->min), &r->origin, &inv_min);
  graphene_vec3_multiply (&inv_min, &inv_dir, &inv_min);

  graphene_vec3_t inv_max;
  graphene_vec3_subtract (&(b->max), &r->origin, &inv_max);
  graphene_vec3_multiply (&inv_max, &inv_dir, &inv_max);

  float tx_min, tx_max;
  if (graphene_vec3_get_x (&inv_dir) >= 0.f)
    {
      tx_min = graphene_vec3_get_x (&inv_min);
      tx_max = graphene_vec3_get_x (&inv_max);
    }
  else
    {
      tx_min = graphene_vec3_get_x (&inv_max);
      tx_max = graphene_vec3_get_x (&inv_min);
    }

  float ty_min, ty_max;
  if (graphene_vec3_get_y (&inv_dir) >= 0.f)
    {
      ty_min = graphene_vec3_get_y (&inv_min);
      ty_max = graphene_vec3_get_y (&inv_max);
    }
  else
    {
      ty_min = graphene_vec3_get_y (&inv_max);
      ty_max = graphene_vec3_get_y (&inv_min);
    }

  if (t_out != NULL)
    *t_out = 0.f;

  if ((tx_min > ty_max) || (ty_min > tx_max))
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  /* These lines also handle the case where tx_min or tx_max is NaN
   * (result of 0 * INFINITY): NaN != NaN
   */
  if (ty_min > tx_min || graphene_isnan (tx_min))
    tx_min = ty_min;
  if (ty_max < tx_max || graphene_isnan (tx_max))
    tx_max = ty_max;

  float tz_min, tz_max;
  if (graphene_vec3_get_z (&inv_dir) >= 0.f)
    {
      tz_min = graphene_vec3_get_z (&inv_min);
      tz_max = graphene_vec3_get_z (&inv_max);
    }
  else
    {
      tz_min = graphene_vec3_get_z (&inv_max);
      tz_max = graphene_vec3_get_z (&inv_min);
    }

  if ((tx_min > tz_max) || (tz_min > tx_max))
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  if (tz_min > tx_min || graphene_isnan (tx_min))
    tx_min = tz_min;
  if (tz_max < tx_max || graphene_isnan (tx_max))
    tx_max = tz_max;

  /* return the point closest to the ray (positive side) */
  if (tx_max < 0)
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  if (tx_min >= 0)
    {
      if (t_out)
        *t_out = tx_min;

      return GRAPHENE_RAY_INTERSECTION_KIND_ENTER;
    }

  if (t_out)
    *t_out = tx_max;

  return GRAPHENE_RAY_INTERSECTION_KIND_LEAVE;
}

/**
 * graphene_ray_intersects_box:
 * @r: a #graphene_ray_t
 * @b: a #graphene_box_t
 *
 * Checks whether the given #graphene_ray_t @r intersects the
 * given #graphene_box_t @b.
 *
 * See also: graphene_ray_intersect_box()
 *
 * Returns: `true` if the ray intersects the box
 *
 * Since: 1.10
 */
bool
graphene_ray_intersects_box (const graphene_ray_t *r,
                             const graphene_box_t *b)
{
  return graphene_ray_intersect_box (r, b, NULL) != GRAPHENE_RAY_INTERSECTION_KIND_NONE;
}

/**
 * graphene_ray_intersect_triangle:
 * @r: a #graphene_ray_t
 * @t: a #graphene_triangle_t
 * @t_out: (out): the distance of the point on the ray that intersects the triangle
 *
 * Intersects the given #graphene_ray_t @r with the given
 * #graphene_triangle_t @t.
 *
 * Returns: the type of intersection
 *
 * Since: 1.10
 */
graphene_ray_intersection_kind_t
graphene_ray_intersect_triangle (const graphene_ray_t      *r,
                                 const graphene_triangle_t *t,
                                 float                     *t_out)
{
  graphene_vec3_t diff, edge1, edge2, normal;
  graphene_ray_intersection_kind_t kind;

  /* from http://www.geometrictools.com/GTEngine/Include/Mathematics/GteIntrRay3Triangle3.h */

  /* Compute the offset origin, edges, and normal */
  graphene_vec3_subtract (&t->b, &t->a, &edge1);
  graphene_vec3_subtract (&t->c, &t->a, &edge2);
  graphene_vec3_cross (&edge1, &edge2, &normal);

  /* Solve:
   *
   *   Q + t * D = b1 * E1 + b2 * E2
   *
   * Where:
   *  * Q = kDiff
   *  * D = ray direction
   *  * E1 = kEdge1
   *  * E2 = kEdge2
   *  * N = Cross(E1,E2)
   *
   * by:
   *
   *  |Dot(D,N)| * b1 = sign(Dot(D,N)) * Dot(D,Cross(Q,E2))
   *  |Dot(D,N)| * b2 = sign(Dot(D,N)) * Dot(D,Cross(E1,Q))
   *  |Dot(D,N)| * t = -sign(Dot(D,N)) * Dot(Q,N)
   */
  float DdN = graphene_vec3_dot (&r->direction, &normal);
  float sign;

  /* Ray and triangle are parallel, call it a "no intersection"
   * even if the ray does intersect
   */
  if (graphene_approx_val (DdN, 0.f))
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;
  else if (DdN > 0)
    {
      kind = GRAPHENE_RAY_INTERSECTION_KIND_LEAVE;
      sign = 1.f;

    }
  else
    {
      kind = GRAPHENE_RAY_INTERSECTION_KIND_ENTER;
      sign = -1.f;
      DdN = -DdN;
    }

  graphene_vec3_subtract (&r->origin, &t->a, &diff);
  graphene_vec3_cross (&diff, &edge2, &edge2);
  float DdQxE2 = sign * graphene_vec3_dot (&r->direction, &edge2);

  /* b1 < 0, no intersection */
  if (DdQxE2 < 0)
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  graphene_vec3_cross (&edge1, &diff, &edge1);

  float DdE1xQ = sign * graphene_vec3_dot (&r->direction, &edge1);

  /* b2 < 0, no intersection */
  if (DdE1xQ < 0)
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  /* b1 + b2 > 1, no intersection */
  if (DdQxE2 + DdE1xQ > DdN)
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  /* Line intersects triangle, check if ray does */
  float QdN = -sign * graphene_vec3_dot (&diff,
                                         &normal);

  /* t < 0, no intersection */
  if ( QdN < 0 )
    return GRAPHENE_RAY_INTERSECTION_KIND_NONE;

  if (t_out != NULL)
    *t_out = QdN / DdN;

  return kind;
}

/**
 * graphene_ray_intersects_triangle:
 * @r: a #graphene_ray_t
 * @t: a #graphene_triangle_t
 *
 * Checks whether the given #graphene_ray_t @r intersects the
 * given #graphene_triangle_t @b.
 *
 * See also: graphene_ray_intersect_triangle()
 *
 * Returns: `true` if the ray intersects the triangle
 *
 * Since: 1.10
 */
bool
graphene_ray_intersects_triangle (const graphene_ray_t      *r,
                                  const graphene_triangle_t *t)
{
  return graphene_ray_intersect_triangle (r, t, NULL) != GRAPHENE_RAY_INTERSECTION_KIND_NONE;
}
