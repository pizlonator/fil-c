/* graphene-sphere.c: A sphere
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
 * SECTION:graphene-sphere
 * @Title: Sphere
 * @Short_Description: A sphere
 *
 * #graphene_sphere_t provides a representation of a sphere using its center
 * and radius.
 */

#include "graphene-private.h"

#include "graphene-sphere.h"

#include "graphene-alloc-private.h"
#include "graphene-box.h"
#include "graphene-point3d.h"
#include "graphene-simd4f.h"

#include <math.h>

/**
 * graphene_sphere_alloc: (constructor)
 *
 * Allocates a new #graphene_sphere_t.
 *
 * The contents of the newly allocated structure are undefined.
 *
 * Returns: (transfer full): the newly allocated #graphene_sphere_t. Use
 *   graphene_sphere_free() to free the resources allocated by this function
 *
 * Since: 1.2
 */
graphene_sphere_t *
graphene_sphere_alloc (void)
{
  return graphene_aligned_alloc0 (sizeof (graphene_sphere_t), 1, 16);
}

/**
 * graphene_sphere_free:
 * @s: a #graphene_sphere_t
 *
 * Frees the resources allocated by graphene_sphere_alloc().
 *
 * Since: 1.2
 */
void
graphene_sphere_free (graphene_sphere_t *s)
{
  graphene_aligned_free (s);
}

/**
 * graphene_sphere_init:
 * @s: the #graphene_sphere_t to initialize
 * @center: (nullable): the coordinates of the center of the sphere, or %NULL
 *   for a center in (0, 0, 0)
 * @radius: the radius of the sphere
 *
 * Initializes the given #graphene_sphere_t with the given @center and @radius.
 *
 * Returns: (transfer none): the initialized #graphene_sphere_t
 *
 * Since: 1.2
 */
graphene_sphere_t *
graphene_sphere_init (graphene_sphere_t        *s,
                      const graphene_point3d_t *center,
                      float                     radius)
{
  if (center != NULL)
    graphene_point3d_to_vec3 (center, &s->center);
  else
    graphene_vec3_init_from_vec3 (&s->center, graphene_vec3_zero ());

  s->radius = radius;

  return s;
}

static float
distance_sq (const graphene_vec3_t *p1,
             const graphene_vec3_t *p2)
{
  graphene_vec3_t delta;

  graphene_vec3_subtract (p1, p2, &delta);
  return graphene_vec3_dot (&delta, &delta);
}

/**
 * graphene_sphere_init_from_points:
 * @s: the #graphene_sphere_t to initialize
 * @n_points: the number of #graphene_point3d_t in the @points array
 * @points: (array length=n_points): an array of #graphene_point3d_t
 * @center: (nullable): the center of the sphere
 *
 * Initializes the given #graphene_sphere_t using the given array
 * of 3D coordinates so that the sphere includes them.
 *
 * The center of the sphere can either be specified, or will be center
 * of the 3D volume that encompasses all @points.
 *
 * Returns: (transfer none): the initialized #graphene_sphere_t
 *
 * Since: 1.2
 */
graphene_sphere_t *
graphene_sphere_init_from_points (graphene_sphere_t        *s,
                                  unsigned int              n_points,
                                  const graphene_point3d_t *points,
                                  const graphene_point3d_t *center)
{
  float max_radius_sq = 0.f;

  if (center != NULL)
    graphene_point3d_to_vec3 (center, &s->center);
  else
    {
      graphene_box_t box;
      graphene_point3d_t c;

      graphene_box_init_from_points (&box, n_points, points);
      graphene_box_get_center (&box, &c);

      graphene_point3d_to_vec3 (&c, &s->center);
    }

  for (unsigned int i = 0; i < n_points; i++)
    {
      graphene_vec3_t p;

      graphene_point3d_to_vec3 (&points[i], &p);

      max_radius_sq = fmaxf (max_radius_sq, distance_sq (&s->center, &p));
    }

  s->radius = sqrtf (max_radius_sq);

  return s;
}

/**
 * graphene_sphere_init_from_vectors:
 * @s: the #graphene_sphere_t to initialize
 * @n_vectors: the number of #graphene_vec3_t in the @vectors array
 * @vectors: (array length=n_vectors): an array of #graphene_vec3_t
 * @center: (nullable): the center of the sphere
 *
 * Initializes the given #graphene_sphere_t using the given array
 * of 3D coordinates so that the sphere includes them.
 *
 * The center of the sphere can either be specified, or will be center
 * of the 3D volume that encompasses all @vectors.
 *
 * Returns: (transfer none): the initialized #graphene_sphere_t
 *
 * Since: 1.2
 */
graphene_sphere_t *
graphene_sphere_init_from_vectors (graphene_sphere_t        *s,
                                   unsigned int              n_vectors,
                                   const graphene_vec3_t    *vectors,
                                   const graphene_point3d_t *center)
{
  float max_radius_sq = 0.f;

  if (center != NULL)
    graphene_point3d_to_vec3 (center, &s->center);
  else
    {
      graphene_box_t box;
      graphene_point3d_t c;

      graphene_box_init_from_vectors (&box, n_vectors, vectors);
      graphene_box_get_center (&box, &c);

      graphene_point3d_to_vec3 (&c, &s->center);
    }

  for (unsigned int i = 0; i < n_vectors; i++)
    max_radius_sq = fmaxf (max_radius_sq, distance_sq (&s->center, &vectors[i]));

  s->radius = sqrtf (max_radius_sq);

  return s;
}

/**
 * graphene_sphere_get_center:
 * @s: a #graphene_sphere_t
 * @center: (out caller-allocates): return location for the coordinates of
 *   the center
 *
 * Retrieves the coordinates of the center of a #graphene_sphere_t.
 *
 * Since: 1.2
 */
void
graphene_sphere_get_center (const graphene_sphere_t *s,
                            graphene_point3d_t      *center)
{
  graphene_point3d_init_from_vec3 (center, &s->center);
}

/**
 * graphene_sphere_get_radius:
 * @s: a #graphene_sphere_t
 *
 * Retrieves the radius of a #graphene_sphere_t.
 *
 * Since: 1.2
 */
float
graphene_sphere_get_radius (const graphene_sphere_t *s)
{
  return s->radius;
}

/**
 * graphene_sphere_is_empty:
 * @s: a #graphene_sphere_t
 *
 * Checks whether the sphere has a zero radius.
 *
 * Returns: `true` if the sphere is empty
 *
 * Since: 1.2
 */
bool
graphene_sphere_is_empty (const graphene_sphere_t *s)
{
  return s != NULL && s->radius <= 0;
}

/**
 * graphene_sphere_contains_point:
 * @s: a #graphene_sphere_t
 * @point: a #graphene_point3d_t
 *
 * Checks whether the given @point is contained in the volume
 * of a #graphene_sphere_t.
 *
 * Returns: `true` if the sphere contains the point
 *
 * Since: 1.2
 */
bool
graphene_sphere_contains_point (const graphene_sphere_t  *s,
                                const graphene_point3d_t *point)
{
  graphene_vec3_t tmp;
  float radius_sq;

  graphene_point3d_to_vec3 (point, &tmp);
  radius_sq = s->radius * s->radius;

  if (distance_sq (&s->center, &tmp) <= radius_sq)
    return true;

  return false;
}

/**
 * graphene_sphere_distance:
 * @s: a #graphene_sphere_t
 * @point: a #graphene_point3d_t
 *
 * Computes the distance of the given @point from the surface of
 * a #graphene_sphere_t.
 *
 * Returns: the distance of the point
 *
 * Since: 1.2
 */
float
graphene_sphere_distance (const graphene_sphere_t  *s,
                          const graphene_point3d_t *point)
{
  graphene_vec3_t tmp;

  graphene_point3d_to_vec3 (point, &tmp);

  return sqrtf (distance_sq (&s->center, &tmp)) - s->radius;
}

/**
 * graphene_sphere_get_bounding_box:
 * @s: a #graphene_sphere_t
 * @box: (out caller-allocates): return location for the bounding box
 *
 * Computes the bounding box capable of containing the
 * given #graphene_sphere_t.
 *
 * Since: 1.2
 */
void
graphene_sphere_get_bounding_box (const graphene_sphere_t *s,
                                  graphene_box_t          *box)
{
  graphene_box_init_from_vec3 (box, &s->center, &s->center);
  graphene_box_expand_scalar (box, s->radius, box);
}

/**
 * graphene_sphere_translate:
 * @s: a #graphene_sphere_t
 * @point: the coordinates of the translation
 * @res: (out caller-allocates): return location for the translated sphere
 *
 * Translates the center of the given #graphene_sphere_t using the @point
 * coordinates as the delta of the translation.
 *
 * Since: 1.2
 */
void
graphene_sphere_translate (const graphene_sphere_t  *s,
                           const graphene_point3d_t *point,
                           graphene_sphere_t        *res)
{
  graphene_vec3_t tmp;

  graphene_point3d_to_vec3 (point, &tmp);
  graphene_vec3_add (&s->center, &tmp, &res->center);
}

static bool
sphere_equal (const void *p1,
              const void *p2)
{
  const graphene_sphere_t *a = p1;
  const graphene_sphere_t *b = p2;

  return graphene_vec3_equal (&a->center, &b->center) &&
         graphene_approx_val (a->radius, b->radius);
}

/**
 * graphene_sphere_equal:
 * @a: a #graphene_sphere_t
 * @b: a #graphene_sphere_t
 *
 * Checks whether two #graphene_sphere_t are equal.
 *
 * Returns: `true` if the spheres are equal
 *
 * Since: 1.2
 */
bool
graphene_sphere_equal (const graphene_sphere_t *a,
                       const graphene_sphere_t *b)
{
  return graphene_pointer_equal (a, b, sphere_equal);
}
