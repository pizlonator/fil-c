/* graphene-box.c: An axis aligned bounding box
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
 * SECTION:graphene-box
 * @Title: Box
 * @Short_Description: Axis-aligned bounding box
 *
 * #graphene_box_t provides a representation of an axis aligned minimum
 * bounding box using the coordinates of its minimum and maximum vertices.
 */

#include "graphene-private.h"

#include "graphene-box.h"

#include "graphene-alloc-private.h"
#include "graphene-point3d.h"
#include "graphene-simd4f.h"
#include "graphene-sphere.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#include <errno.h>
#endif

/**
 * graphene_box_alloc: (constructor)
 *
 * Allocates a new #graphene_box_t.
 *
 * The contents of the returned structure are undefined.
 *
 * Returns: (transfer full): the newly allocated #graphene_box_t structure.
 *   Use graphene_box_free() to free the resources allocated by this function
 *
 * Since: 1.2
 */
graphene_box_t *
graphene_box_alloc (void)
{
  return graphene_aligned_alloc0 (sizeof (graphene_box_t), 1, 16);
}

/**
 * graphene_box_free:
 * @box: a #graphene_box_t
 *
 * Frees the resources allocated by graphene_box_alloc().
 *
 * Since: 1.2
 */
void
graphene_box_free (graphene_box_t *box)
{
  graphene_aligned_free (box);
}

/**
 * graphene_box_init:
 * @box: the #graphene_box_t to initialize
 * @min: (nullable): the coordinates of the minimum vertex
 * @max: (nullable): the coordinates of the maximum vertex
 *
 * Initializes the given #graphene_box_t with two vertices.
 *
 * Returns: (transfer none): the initialized #graphene_box_t
 *
 * Since: 1.2
 */
graphene_box_t *
graphene_box_init (graphene_box_t           *box,
                   const graphene_point3d_t *min,
                   const graphene_point3d_t *max)
{
  if (min != NULL)
    graphene_point3d_to_vec3 (min, &box->min);
  else
    graphene_vec3_init_from_vec3 (&box->min, graphene_vec3_zero ());

  if (max != NULL)
    graphene_point3d_to_vec3 (max, &box->max);
  else
    graphene_vec3_init_from_vec3 (&box->max, graphene_vec3_zero ());

  return box;
}

/**
 * graphene_box_init_from_points:
 * @box: the #graphene_box_t to initialize
 * @n_points: the number #graphene_point3d_t in the @points array
 * @points: (array length=n_points): an array of #graphene_point3d_t
 *
 * Initializes the given #graphene_box_t with the given array
 * of vertices.
 *
 * If @n_points is 0, the returned box is initialized with
 * graphene_box_empty().
 *
 * Returns: (transfer none): the initialized #graphene_box_t
 *
 * Since: 1.2
 */
graphene_box_t *
graphene_box_init_from_points (graphene_box_t           *box,
                               unsigned int              n_points,
                               const graphene_point3d_t *points)
{
  graphene_box_init_from_box (box, graphene_box_empty ());

  for (unsigned int i = 0; i < n_points; i++)
    {
      graphene_vec3_t v;

      graphene_point3d_to_vec3 (&points[i], &v);
      graphene_box_expand_vec3 (box, &v, box);
    }

  return box;
}

/**
 * graphene_box_init_from_vectors:
 * @box: the #graphene_box_t to initialize
 * @n_vectors: the number #graphene_point3d_t in the @vectors array
 * @vectors: (array length=n_vectors): an array of #graphene_vec3_t
 *
 * Initializes the given #graphene_box_t with the given array
 * of vertices.
 *
 * If @n_vectors is 0, the returned box is initialized with
 * graphene_box_empty().
 *
 * Returns: (transfer none): the initialized #graphene_box_t
 *
 * Since: 1.2
 */
graphene_box_t *
graphene_box_init_from_vectors (graphene_box_t        *box,
                                unsigned int           n_vectors,
                                const graphene_vec3_t *vectors)
{
  graphene_box_init_from_box (box, graphene_box_empty ());

  for (unsigned int i = 0; i < n_vectors; i++)
    graphene_box_expand_vec3 (box, &vectors[i], box);

  return box;
}

/**
 * graphene_box_init_from_box:
 * @box: the #graphene_box_t to initialize
 * @src: a #graphene_box_t
 *
 * Initializes the given #graphene_box_t with the vertices of
 * another #graphene_box_t.
 *
 * Returns: (transfer none): the initialized #graphene_box_t
 *
 * Since: 1.2
 */
graphene_box_t *
graphene_box_init_from_box (graphene_box_t       *box,
                            const graphene_box_t *src)
{
  box->min = src->min;
  box->max = src->max;

  return box;
}

/**
 * graphene_box_init_from_vec3:
 * @box: the #graphene_box_t to initialize
 * @min: (nullable): the coordinates of the minimum vertex
 * @max: (nullable): the coordinates of the maximum vertex
 *
 * Initializes the given #graphene_box_t with two vertices
 * stored inside #graphene_vec3_t.
 *
 * Returns: (transfer none): the initialized #graphene_box_t
 *
 * Since: 1.2
 */
graphene_box_t *
graphene_box_init_from_vec3 (graphene_box_t        *box,
                             const graphene_vec3_t *min,
                             const graphene_vec3_t *max)
{
  if (min != NULL)
    box->min = *min;
  else
    graphene_vec3_init_from_vec3 (&box->min, graphene_vec3_zero ());

  if (max != NULL)
    box->max = *max;
  else
    graphene_vec3_init_from_vec3 (&box->max, graphene_vec3_zero ());

  return box;
}

static inline graphene_box_t *
graphene_box_init_from_simd4f (graphene_box_t          *box,
                               const graphene_simd4f_t  min,
                               const graphene_simd4f_t  max)
{
  box->min.value = min;
  box->max.value = max;

  return box;
}

static inline void
graphene_box_expand_simd4f (const graphene_box_t    *box,
                            const graphene_simd4f_t  v,
                            graphene_box_t          *res)
{
  res->min.value = graphene_simd4f_min (box->min.value, v);
  res->max.value = graphene_simd4f_max (box->max.value, v);
}

/**
 * graphene_box_expand_vec3:
 * @box: a #graphene_box_t
 * @vec: the coordinates of the point to include, as a #graphene_vec3_t
 * @res: (out caller-allocates): return location for the expanded box
 *
 * Expands the dimensions of @box to include the coordinates of the
 * given vector.
 *
 * Since: 1.2
 */
void
graphene_box_expand_vec3 (const graphene_box_t  *box,
                          const graphene_vec3_t *vec,
                          graphene_box_t        *res)
{
  graphene_box_expand_simd4f (box, vec->value, res);
}

/**
 * graphene_box_expand:
 * @box: a #graphene_box_t
 * @point: the coordinates of the point to include
 * @res: (out caller-allocates): return location for the expanded box
 *
 * Expands the dimensions of @box to include the coordinates at @point.
 *
 * Since: 1.2
 */
void
graphene_box_expand (const graphene_box_t     *box,
                     const graphene_point3d_t *point,
                     graphene_box_t           *res)
{
  graphene_simd4f_t v = graphene_simd4f_init (point->x, point->y, point->z, 0.f);

  graphene_box_expand_simd4f (box, v, res);
}

/**
 * graphene_box_expand_scalar:
 * @box: a #graphene_box_t
 * @scalar: a scalar value
 * @res: (out caller-allocates): return location for the expanded box
 *
 * Expands the dimensions of @box by the given @scalar value.
 *
 * If @scalar is positive, the #graphene_box_t will grow; if @scalar is
 * negative, the #graphene_box_t will shrink.
 *
 * Since: 1.2
 */
void
graphene_box_expand_scalar (const graphene_box_t *box,
                            float                 scalar,
                            graphene_box_t       *res)
{
  float min = scalar * -1.f;
  float max = scalar;

  res->min.value = graphene_simd4f_add (box->min.value, graphene_simd4f_splat (min));
  res->max.value = graphene_simd4f_add (box->max.value, graphene_simd4f_splat (max));
}

/**
 * graphene_box_union:
 * @a: a #graphene_box_t
 * @b: the box to union to @a
 * @res: (out caller-allocates): return location for the result
 *
 * Unions the two given #graphene_box_t.
 *
 * Since: 1.2
 */
void
graphene_box_union (const graphene_box_t *a,
                    const graphene_box_t *b,
                    graphene_box_t       *res)
{
  res->min.value = graphene_simd4f_min (a->min.value, b->min.value);
  res->max.value = graphene_simd4f_max (a->max.value, b->max.value);
}

/**
 * graphene_box_intersection:
 * @a: a #graphene_box_t
 * @b: a #graphene_box_t
 * @res: (out caller-allocates) (optional): return location for the result
 *
 * Intersects the two given #graphene_box_t.
 *
 * If the two boxes do not intersect, @res will contain a degenerate box
 * initialized with graphene_box_empty().
 *
 * Returns: true if the two boxes intersect
 *
 * Since: 1.2
 */
bool
graphene_box_intersection (const graphene_box_t *a,
                           const graphene_box_t *b,
                           graphene_box_t       *res)
{
  graphene_simd4f_t min, max;

  min = graphene_simd4f_max (a->min.value, b->min.value);
  max = graphene_simd4f_min (a->max.value, b->max.value);

  if (!graphene_simd4f_cmp_le (min, max))
    {
      if (res != NULL)
        graphene_box_init_from_box (res, graphene_box_empty ());

      return false;
    }

  if (res != NULL)
    graphene_box_init_from_simd4f (res, min, max);

  return true;
}

/**
 * graphene_box_get_width:
 * @box: a #graphene_box_t
 *
 * Retrieves the size of the @box on the X axis.
 *
 * Returns: the width of the box
 *
 * Since: 1.2
 */
float
graphene_box_get_width (const graphene_box_t *box)
{
  float res = graphene_simd4f_get_x (graphene_simd4f_sub (box->max.value, box->min.value));

  return fabsf (res);
}

/**
 * graphene_box_get_height:
 * @box: a #graphene_box_t
 *
 * Retrieves the size of the @box on the Y axis.
 *
 * Returns: the height of the box
 *
 * Since: 1.2
 */
float
graphene_box_get_height (const graphene_box_t *box)
{
  float res = graphene_simd4f_get_y (graphene_simd4f_sub (box->max.value, box->min.value));

  return fabsf (res);
}

/**
 * graphene_box_get_depth:
 * @box: a #graphene_box_t
 *
 * Retrieves the size of the @box on the Z axis.
 *
 * Returns: the depth of the box
 *
 * Since: 1.2
 */
float
graphene_box_get_depth (const graphene_box_t *box)
{
  float res = graphene_simd4f_get_z (graphene_simd4f_sub (box->max.value, box->min.value));

  return fabsf (res);
}

static inline bool
graphene_box_is_empty (const graphene_box_t *box)
{
#ifdef HAVE_ISINFF
  float vmin[3], vmax[3];

  graphene_simd4f_dup_3f (box->min.value, vmin);
  graphene_simd4f_dup_3f (box->max.value, vmax);

  return (isinff (vmin[0]) == 1 && isinff (vmin[1]) == 1 && isinff (vmin[2]) == 1) &&
         (isinff (vmax[0]) == -1 && isinff (vmax[1]) == -1 && isinff (vmax[2]) == -1);
#else
  graphene_simd4f_t neg_inf = graphene_simd4f_init (-INFINITY, -INFINITY, -INFINITY, 0.f);
  graphene_simd4f_t pos_inf = graphene_simd4f_init (INFINITY, INFINITY, INFINITY, 0.f);

  /* This is only every going to be valid for boxes that we have
   * initialized ourselves, because we use the same values; the
   * bitwise comparison will not hold for infinities generated by
   * other operations
   */
  int min_cmp = memcmp (&box->min.value, &pos_inf, sizeof (graphene_simd4f_t));
  int max_cmp = memcmp (&box->max.value, &neg_inf, sizeof (graphene_simd4f_t));

  return min_cmp == 0 && max_cmp == 0;
#endif
}

static inline bool
graphene_box_is_infinity (const graphene_box_t *box)
{
#ifdef HAVE_ISINFF
  float vmin[3], vmax[3];

  graphene_simd4f_dup_3f (box->min.value, vmin);
  graphene_simd4f_dup_3f (box->max.value, vmax);

  return (isinff (vmin[0]) == -1 && isinff (vmin[1]) == -1 && isinff (vmin[2]) == -1) &&
         (isinff (vmax[0]) == 1 && isinff (vmax[1]) == 1 && isinff (vmax[2]) == 1);
#else
  graphene_simd4f_t neg_inf = graphene_simd4f_init (-INFINITY, -INFINITY, -INFINITY, 0.f);
  graphene_simd4f_t pos_inf = graphene_simd4f_init (INFINITY, INFINITY, INFINITY, 0.f);

  /* This is only every going to be valid for boxes that we have
   * initialized ourselves, because we use the same values; the
   * bitwise comparison will not hold for infinities generated by
   * other operations
   */
  int min_cmp = memcmp (&box->min.value, &neg_inf, sizeof (graphene_simd4f_t));
  int max_cmp = memcmp (&box->max.value, &pos_inf, sizeof (graphene_simd4f_t));

  return min_cmp == 0 && max_cmp == 0;
#endif
}

/**
 * graphene_box_get_size:
 * @box: a #graphene_box_t
 * @size: (out caller-allocates): return location for the size
 *
 * Retrieves the size of the box on all three axes, and stores
 * it into the given @size vector.
 *
 * Since: 1.2
 */
void
graphene_box_get_size (const graphene_box_t *box,
                       graphene_vec3_t      *size)
{
  if (graphene_box_is_empty (box))
    size->value = graphene_simd4f_init_zero ();
  else if (graphene_box_is_infinity (box))
    size->value = graphene_simd4f_init (INFINITY, INFINITY, INFINITY, 0.f);
  else
    size->value = graphene_simd4f_sub (box->max.value, box->min.value);
}

/**
 * graphene_box_get_center:
 * @box: a #graphene_box_t
 * @center: (out caller-allocates): return location for the coordinates of
 *   the center
 *
 * Retrieves the coordinates of the center of a #graphene_box_t.
 *
 * Since: 1.2
 */
void
graphene_box_get_center (const graphene_box_t *box,
                         graphene_point3d_t   *center)
{
  graphene_vec3_t res;

  if (graphene_box_is_empty (box) || graphene_box_is_infinity (box))
    {
      graphene_point3d_init (center, 0.f, 0.f, 0.f);
      return;
    }

  graphene_vec3_add (&box->min, &box->max, &res);
  graphene_vec3_scale (&res, 0.5f, &res);

  graphene_point3d_init_from_vec3 (center, &res);
}

/**
 * graphene_box_get_min:
 * @box: a #graphene_box_t
 * @min: (out caller-allocates): return location for the minimum point
 *
 * Retrieves the coordinates of the minimum point of the given
 * #graphene_box_t.
 *
 * Since: 1.2
 */
void
graphene_box_get_min (const graphene_box_t *box,
                      graphene_point3d_t   *min)
{
  graphene_point3d_init_from_vec3 (min, &box->min);
}

/**
 * graphene_box_get_max:
 * @box: a #graphene_box_t
 * @max: (out caller-allocates): return location for the maximum point
 *
 * Retrieves the coordinates of the maximum point of the given
 * #graphene_box_t.
 *
 * Since: 1.2
 */
void
graphene_box_get_max (const graphene_box_t *box,
                      graphene_point3d_t   *max)
{
  graphene_point3d_init_from_vec3 (max, &box->max);
}

/**
 * graphene_box_get_vertices:
 * @box: a #graphene_box_t
 * @vertices: (out) (array fixed-size=8): return location for an array
 *   of 8 #graphene_vec3_t
 *
 * Computes the vertices of the given #graphene_box_t.
 *
 * Since: 1.2
 */
void
graphene_box_get_vertices (const graphene_box_t *box,
                           graphene_vec3_t       vertices[])
{
  graphene_point3d_t min, max;

  graphene_box_get_min (box, &min);
  graphene_box_get_max (box, &max);

  graphene_vec3_init (&vertices[0], min.x, min.y, min.z);
  graphene_vec3_init (&vertices[1], min.x, min.y, max.z);
  graphene_vec3_init (&vertices[2], min.x, max.y, min.z);
  graphene_vec3_init (&vertices[3], min.x, max.y, max.z);
  graphene_vec3_init (&vertices[4], max.x, min.y, min.z);
  graphene_vec3_init (&vertices[5], max.x, min.y, max.z);
  graphene_vec3_init (&vertices[6], max.x, max.y, min.z);
  graphene_vec3_init (&vertices[7], max.x, max.y, max.z);
}

/**
 * graphene_box_contains_point:
 * @box: a #graphene_box_t
 * @point: the coordinates to check
 *
 * Checks whether @box contains the given @point.
 *
 * Returns: `true` if the point is contained in the given box
 *
 * Since: 1.2
 */
bool
graphene_box_contains_point (const graphene_box_t     *box,
                             const graphene_point3d_t *point)
{
  if (graphene_box_is_empty (box))
    return false;

  if (graphene_box_is_infinity (box))
    return true;

  graphene_simd4f_t p = graphene_simd4f_init (point->x, point->y, point->z, 0.f);

  if (graphene_simd4f_cmp_ge (p, box->min.value) &&
      graphene_simd4f_cmp_le (p, box->max.value))
    return true;

  return false;
}

/**
 * graphene_box_contains_box:
 * @a: a #graphene_box_t
 * @b: a #graphene_box_t
 *
 * Checks whether the #graphene_box_t @a contains the given
 * #graphene_box_t @b.
 *
 * Returns: `true` if the box is contained in the given box
 *
 * Since: 1.2
 */
bool
graphene_box_contains_box (const graphene_box_t *a,
                           const graphene_box_t *b)
{
  if (graphene_box_is_empty (a) || graphene_box_is_infinity (b))
    return false;

  if (graphene_box_is_infinity (a) || graphene_box_is_empty (b))
    return true;

  /* we cheat a bit and access the SIMD directly */
  if (graphene_simd4f_cmp_ge (b->min.value, a->min.value) &&
      graphene_simd4f_cmp_le (b->max.value, a->max.value))
    return true;

  return false;
}

static bool
box_equal (const void *p1,
           const void *p2)
{
  const graphene_box_t *a = p1;
  const graphene_box_t *b = p2;

  if (graphene_box_is_empty (a) && graphene_box_is_empty (b))
    return true;
  else if (graphene_box_is_empty (a) || graphene_box_is_empty (b))
    return false;

  if (graphene_box_is_infinity (a) && graphene_box_is_infinity (b))
    return true;
  else if (graphene_box_is_infinity (a) || graphene_box_is_infinity (b))
    return false;

  return graphene_vec3_equal (&a->min, &b->min) &&
         graphene_vec3_equal (&a->max, &b->max);
}

/**
 * graphene_box_equal:
 * @a: a #graphene_box_t
 * @b: a #graphene_box_t
 *
 * Checks whether the two given boxes are equal.
 *
 * Returns: `true` if the boxes are equal
 *
 * Since: 1.2
 */
bool
graphene_box_equal (const graphene_box_t *a,
                    const graphene_box_t *b)
{
  return graphene_pointer_equal (a, b, box_equal);
}

/**
 * graphene_box_get_bounding_sphere:
 * @box: a #graphene_box_t
 * @sphere: (out caller-allocates): return location for the bounding sphere
 *
 * Computes the bounding #graphene_sphere_t capable of containing the given
 * #graphene_box_t.
 *
 * Since: 1.2
 */
void
graphene_box_get_bounding_sphere (const graphene_box_t *box,
                                  graphene_sphere_t    *sphere)
{
  graphene_point3d_t center;
  graphene_vec3_t size;

  graphene_box_get_center (box, &center);

  graphene_box_get_size (box, &size);
  float radius = graphene_vec3_length (&size) * 0.5f;

  graphene_sphere_init (sphere, &center, radius);
}

enum {
  BOX_ZERO = 0,
  BOX_ONE,
  BOX_MINUS_ONE,
  BOX_ONE_MINUS_ONE,
  BOX_INFINITY,
  BOX_EMPTY,

  N_STATIC_BOX
};

static graphene_box_t static_box[N_STATIC_BOX];

static void
init_static_box_once (void)
{
  static_box[BOX_ZERO].min.value = graphene_simd4f_init_zero ();
  static_box[BOX_ZERO].max.value = graphene_simd4f_init_zero ();

  static_box[BOX_ONE].min.value = graphene_simd4f_init_zero ();
  static_box[BOX_ONE].max.value = graphene_simd4f_init (1.f, 1.f, 1.f, 0.f);

  static_box[BOX_MINUS_ONE].min.value = graphene_simd4f_init (-1.f, -1.f, -1.f, 0.f);
  static_box[BOX_MINUS_ONE].max.value = graphene_simd4f_init_zero ();

  static_box[BOX_ONE_MINUS_ONE].min.value = graphene_simd4f_init (-1.f, -1.f, -1.f, 0.f);
  static_box[BOX_ONE_MINUS_ONE].max.value = graphene_simd4f_init (1.f, 1.f, 1.f, 0.f);

  static_box[BOX_INFINITY].min.value = graphene_simd4f_init (-INFINITY, -INFINITY, -INFINITY, 0.f);
  static_box[BOX_INFINITY].max.value = graphene_simd4f_init (INFINITY, INFINITY, INFINITY, 0.f);

  static_box[BOX_EMPTY].min.value = graphene_simd4f_init (INFINITY, INFINITY, INFINITY, 0.f);
  static_box[BOX_EMPTY].max.value = graphene_simd4f_init (-INFINITY, -INFINITY, -INFINITY, 0.f);
}

#ifdef HAVE_PTHREAD
static pthread_once_t static_box_once = PTHREAD_ONCE_INIT;

static inline void
init_static_box (void)
{
  int status = pthread_once (&static_box_once, init_static_box_once);

  if (status < 0)
    {
      int saved_errno = errno;

      fprintf (stderr, "pthread_once failed: %s (errno:%d)\n",
               strerror (saved_errno),
               saved_errno);
    }
}

#elif defined(HAVE_INIT_ONCE)
static INIT_ONCE static_box_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
InitBoxFunc (PINIT_ONCE InitOnce,
             PVOID      param,
             PVOID     *ctx)
{
  init_static_box_once ();
  return TRUE;
}

static inline void
init_static_box (void)
{
  BOOL bStatus = InitOnceExecuteOnce (&static_box_once,
                                      InitBoxFunc,
                                      NULL,
                                      NULL);

  if (!bStatus)
    fprintf (stderr, "InitOnceExecuteOnce failed\n");
}

#else /* !HAVE_PTHREAD */
static bool static_box_init = false;

static inline void
init_static_box (void)
{
  if (static_box_init)
    return;

  init_static_box_once ();
  static_box_init = true;
}

#endif /* HAVE_PTHREAD */

/**
 * graphene_box_zero:
 *
 * A #graphene_box_t with both the minimum and maximum vertices set at (0, 0, 0).
 *
 * The returned value is owned by Graphene and should not be modified or freed.
 *
 * Returns: (transfer none): a #graphene_box_t
 *
 * Since: 1.2
 */
const graphene_box_t *
graphene_box_zero (void)
{
  init_static_box ();

  return &(static_box[BOX_ZERO]);
}

/**
 * graphene_box_one:
 *
 * A #graphene_box_t with the minimum vertex set at (0, 0, 0) and the
 * maximum vertex set at (1, 1, 1).
 *
 * The returned value is owned by Graphene and should not be modified or freed.
 *
 * Returns: (transfer none): a #graphene_box_t
 *
 * Since: 1.2
 */
const graphene_box_t *
graphene_box_one (void)
{
  init_static_box ();

  return &(static_box[BOX_ONE]);
}

/**
 * graphene_box_minus_one:
 *
 * A #graphene_box_t with the minimum vertex set at (-1, -1, -1) and the
 * maximum vertex set at (0, 0, 0).
 *
 * The returned value is owned by Graphene and should not be modified or freed.
 *
 * Returns: (transfer none): a #graphene_box_t
 *
 * Since: 1.2
 */
const graphene_box_t *
graphene_box_minus_one (void)
{
  init_static_box ();

  return &(static_box[BOX_MINUS_ONE]);
}

/**
 * graphene_box_one_minus_one:
 *
 * A #graphene_box_t with the minimum vertex set at (-1, -1, -1) and the
 * maximum vertex set at (1, 1, 1).
 *
 * The returned value is owned by Graphene and should not be modified or freed.
 *
 * Returns: (transfer none): a #graphene_box_t
 *
 * Since: 1.2
 */
const graphene_box_t *
graphene_box_one_minus_one (void)
{
  init_static_box ();

  return &(static_box[BOX_ONE_MINUS_ONE]);
}

/**
 * graphene_box_infinite:
 *
 * A degenerate #graphene_box_t that cannot be expanded.
 *
 * The returned value is owned by Graphene and should not be modified or freed.
 *
 * Returns: (transfer none): a #graphene_box_t
 *
 * Since: 1.2
 */
const graphene_box_t *
graphene_box_infinite (void)
{
  init_static_box ();

  return &(static_box[BOX_INFINITY]);
}

/**
 * graphene_box_empty:
 *
 * A degenerate #graphene_box_t that can only be expanded.
 *
 * The returned value is owned by Graphene and should not be modified or freed.
 *
 * Returns: (transfer none): a #graphene_box_t
 *
 * Since: 1.2
 */
const graphene_box_t *
graphene_box_empty (void)
{
  init_static_box ();

  return &(static_box[BOX_EMPTY]);
}
