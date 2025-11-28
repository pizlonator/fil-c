/* graphene-simd4f.c
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
 * SECTION:graphene-simd4f
 * @Title: SIMD vector
 * @short_description: Low level floating point 4-sized vector
 *
 * The #graphene_simd4f_t type wraps a platform specific implementation of
 * a vector of four floating point values.
 *
 * Graphene can be compiled to use different implementations of the SIMD
 * types, and will generally prefer the faster hardware-backed implementation
 * if one is available.
 *
 * The #graphene_simd4f_t should be treated as an opaque, integral type;
 * you cannot access its components directly, and you can only operate on
 * all components at the same time.
 */

#include "graphene-private.h"
#include "graphene-simd4f.h"

#include <string.h>
#include <math.h>

/**
 * graphene_simd4f_t:
 *
 * A vector type containing four floating point values.
 *
 * The contents of the #graphene_simd4f_t type are private and
 * cannot be directly accessed; use the provided API instead.
 *
 * Since: 1.0
 */

/* fast paths are all defined in the graphene-simd4f.h header */
#if defined(GRAPHENE_USE_SSE) || defined(GRAPHENE_USE_GCC) || defined(GRAPHENE_USE_ARM_NEON)

/**
 * graphene_simd4f_init:
 * @x: the first component of the vector
 * @y: the second component of the vector
 * @z: the third component of the vector
 * @w: the fourth component of the vector
 *
 * Initializes a #graphene_simd4f_t with the given values.
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_init) (float x,
                        float y,
                        float z,
                        float w)
{
  return graphene_simd4f_init (x, y, z, w);
}

/**
 * graphene_simd4f_init_zero:
 *
 * Initializes a #graphene_simd4f_t with 0 in all components.
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_init_zero) (void)
{
  return graphene_simd4f_init_zero ();
}

/**
 * graphene_simd4f_init_4f:
 * @v: (array fixed-size=4): an array of at least 4 floating
 *   point values
 *
 * Initializes a #graphene_simd4f_t using an array of 4 floating
 * point values.
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_init_4f) (const float *v)
{
  return graphene_simd4f_init_4f (v);
}

/**
 * graphene_simd4f_init_3f:
 * @v: (array fixed-size=3): an array of at least 3 floating
 *   point values
 *
 * Initializes a #graphene_simd4f_t using an array of 3 floating
 * point values.
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_init_3f) (const float *v)
{
  return graphene_simd4f_init_3f (v);
}

/**
 * graphene_simd4f_init_2f:
 * @v: (array fixed-size=2): an array of at least 2 floating
 * point values
 *
 * Initializes a #graphene_simd4f_t using an array of 2 floating
 * point values.
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_init_2f) (const float *v)
{
  return graphene_simd4f_init_2f (v);
}

/**
 * graphene_simd4f_dup_4f:
 * @s: a #graphene_simd4f_t
 * @v: (out) (array fixed-size=4): return location for an
 *   array of at least 4 floating point values
 *
 * Copies the contents of a #graphene_simd4f_t into an
 * array of floating points.
 *
 * Since: 1.0
 */
void
(graphene_simd4f_dup_4f) (const graphene_simd4f_t  s,
                          float                   *v)
{
  graphene_simd4f_dup_4f (s, v);
}

/**
 * graphene_simd4f_dup_3f:
 * @s: a #graphene_simd4f_t
 * @v: (out) (array fixed-size=3): return location for an
 *   array of at least 3 floating point values
 *
 * Copies the contents of a #graphene_simd4f_t into an
 * array of floating points.
 *
 * Since: 1.0
 */
void
(graphene_simd4f_dup_3f) (const graphene_simd4f_t  s,
                          float                   *v)
{
  graphene_simd4f_dup_3f (s, v);
}

/**
 * graphene_simd4f_dup_2f:
 * @s: a #graphene_simd4f_t
 * @v: (out) (array fixed-size=2): return location for an
 *   array of at least 2 floating point values
 *
 * Copies the contents of a #graphene_simd4f_t into an
 * array of floating points.
 *
 * Since: 1.0
 */
void
(graphene_simd4f_dup_2f) (const graphene_simd4f_t  s,
                          float                   *v)
{
  graphene_simd4f_dup_2f (s, v);
}

/**
 * graphene_simd4f_get:
 * @s: a #graphene_simd4f_t
 * @i: the index of the component to retrieve
 *
 * Retrieves the given component of a #graphene_simd4f_t.
 *
 * Since: 1.2
 */
float
(graphene_simd4f_get) (const graphene_simd4f_t s,
                       unsigned int            i)
{
  switch (i)
    {
    case 0:
      return graphene_simd4f_get (s, 0);

    case 1:
      return graphene_simd4f_get (s, 1);

    case 2:
      return graphene_simd4f_get (s, 2);

    case 3:
      return graphene_simd4f_get (s, 3);

    default:
      return 0.f;
  }
}

/**
 * graphene_simd4f_get_x:
 * @s: a #graphene_simd4f_t
 *
 * Retrieves the first component of @s.
 *
 * Returns: the first component of a #graphene_simd4f_t
 *
 * Since: 1.0
 */
float
(graphene_simd4f_get_x) (const graphene_simd4f_t s)
{
  return graphene_simd4f_get_x (s);
}

/**
 * graphene_simd4f_get_y:
 * @s: a #graphene_simd4f_t
 *
 * Retrieves the second component of @s.
 *
 * Returns: the second component of a #graphene_simd4f_t
 *
 * Since: 1.0
 */
float
(graphene_simd4f_get_y) (const graphene_simd4f_t s)
{
  return graphene_simd4f_get_y (s);
}

/**
 * graphene_simd4f_get_z:
 * @s: a #graphene_simd4f_t
 *
 * Retrieves the third component of @s.
 *
 * Returns: the third component of a #graphene_simd4f_t
 *
 * Since: 1.0
 */
float
(graphene_simd4f_get_z) (const graphene_simd4f_t s)
{
  return graphene_simd4f_get_z (s);
}

/**
 * graphene_simd4f_get_w:
 * @s: a #graphene_simd4f_t
 *
 * Retrieves the fourth component of @s.
 *
 * Returns: the fourth component of a #graphene_simd4f_t
 *
 * Since: 1.0
 */
float
(graphene_simd4f_get_w) (const graphene_simd4f_t s)
{
  return graphene_simd4f_get_w (s);
}

/**
 * graphene_simd4f_splat:
 * @v: a floating point value
 *
 * Sets all the components of a new #graphene_simd4f_t to the
 * same value @v:
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = v,
 *     .y = v,
 *     .z = v,
 *     .w = v
 *   };
 * ]|
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_splat) (float v)
{
  return graphene_simd4f_splat (v);
}

/**
 * graphene_simd4f_splat_x:
 * @s: a #graphene_simd4f_t
 *
 * Sets all the components of a new #graphene_simd4f_t to the
 * same value of the first component of the passed vector:
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = s.x,
 *     .y = s.x,
 *     .z = s.x,
 *     .w = s.x
 *   }
 * ]|
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_splat_x) (const graphene_simd4f_t s)
{
  return graphene_simd4f_splat_x (s);
}

/**
 * graphene_simd4f_splat_y:
 * @s: a #graphene_simd4f_t
 *
 * Sets all the components of a new #graphene_simd4f_t to the
 * same value of the second component of the passed vector:
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = s.y,
 *     .y = s.y,
 *     .z = s.y,
 *     .w = s.y
 *   }
 * ]|
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_splat_y) (const graphene_simd4f_t s)
{
  return graphene_simd4f_splat_y (s);
}

/**
 * graphene_simd4f_splat_z:
 * @s: a #graphene_simd4f_t
 *
 * Sets all the components of a #graphene_simd4f_t to the
 * same value of the third component of the passed vector:
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = s.z,
 *     .y = s.z,
 *     .z = s.z,
 *     .w = s.z
 *   }
 * ]|
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_splat_z) (const graphene_simd4f_t s)
{
  return graphene_simd4f_splat_z (s);
}

/**
 * graphene_simd4f_splat_w:
 * @s: a #graphene_simd4f_t
 *
 * Sets all the components of a #graphene_simd4f_t to the
 * same value of the fourth component of the passed vector:
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = s.w,
 *     .y = s.w,
 *     .z = s.w,
 *     .w = s.w
 *   }
 * ]|
 *
 * Returns: the initialized #graphene_simd4f_t
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_splat_w) (const graphene_simd4f_t s)
{
  return graphene_simd4f_splat_w (s);
}

/**
 * graphene_simd4f_reciprocal:
 * @s: a #graphene_simd4f_t
 *
 * Computes the reciprocal of every component of @s.
 * The reciprocals of positive and negative 0 are defined
 * as positive and negative infinity, respectively.
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = 1.0 / s.x,
 *     .y = 1.0 / s.y,
 *     .z = 1.0 / s.z,
 *     .w = 1.0 / s.w
 *   }
 * ]|
 *
 * Returns: a vector containing the reciprocal of the
 *   passed vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_reciprocal) (const graphene_simd4f_t s)
{
  return graphene_simd4f_reciprocal (s);
}

/**
 * graphene_simd4f_sqrt:
 * @s: a #graphene_simd4f_t
 *
 * Computes the square root of every component of @s.
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = sqrt (s.x),
 *     .y = sqrt (s.y),
 *     .z = sqrt (s.z),
 *     .w = sqrt (s.w)
 *   }
 * ]|
 *
 * Returns: a vector containing the square root of the
 *   passed vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_sqrt) (const graphene_simd4f_t s)
{
  return graphene_simd4f_sqrt (s);
}

/**
 * graphene_simd4f_rsqrt:
 * @s: a #graphene_simd4f_t
 *
 * Computes the reciprocal square root of every component
 * of @s.
 *
 * |[<!-- language="plain" -->
 *   {
 *     .x = 1.0 / sqrt (s.x),
 *     .y = 1.0 / sqrt (s.y),
 *     .z = 1.0 / sqrt (s.z),
 *     .w = 1.0 / sqrt (s.w)
 *   }
 * ]|
 *
 * Returns: a vector containing the reciprocal square root
 *   of the passed vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_rsqrt) (const graphene_simd4f_t s)
{
  return graphene_simd4f_rsqrt (s);
}

/**
 * graphene_simd4f_add:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t vector where each
 * component is the sum of the respective components
 * in @a and @b.
 *
 * |[<!-- lanugage="plain" -->
 *   {
 *     .x = a.x + b.x,
 *     .y = a.y + b.y,
 *     .z = a.z + b.z,
 *     .w = a.w + b.w
 *   }
 * ]|
 *
 * Returns: the sum vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_add) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_add (a, b);
}

/**
 * graphene_simd4f_sub:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t vector where each
 * component is the subtraction of the respective components
 * in @a and @b.
 *
 * |[<!-- lanugage="plain" -->
 *   {
 *     .x = a.x - b.x,
 *     .y = a.y - b.y,
 *     .z = a.z - b.z,
 *     .w = a.w - b.w
 *   }
 * ]|
 *
 * Returns: the subtraction vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_sub) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_sub (a, b);
}

/**
 * graphene_simd4f_mul:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t vector where each
 * component is the multiplication of the respective components
 * in @a and @b.
 *
 * |[<!-- lanugage="plain" -->
 *   {
 *     .x = a.x * b.x,
 *     .y = a.y * b.y,
 *     .z = a.z * b.z,
 *     .w = a.w * b.w
 *   }
 * ]|
 *
 * Returns: the multiplication vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_mul) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_mul (a, b);
}

/**
 * graphene_simd4f_div:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t vector where each
 * component is the division of the respective components
 * in @a and @b.
 *
 * |[<!-- lanugage="plain" -->
 *   {
 *     .x = a.x / b.x,
 *     .y = a.y / b.y,
 *     .z = a.z / b.z,
 *     .w = a.w / b.w
 *   }
 * ]|
 *
 * Returns: the division vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_div) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_div (a, b);
}

/**
 * graphene_simd4f_cross3:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t vector where each
 * component contains the 3-way cross product of the
 * given @a and @b vectors.
 *
 * Returns: the cross3 vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_cross3) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_cross3 (a, b);
}

/**
 * graphene_simd4f_dot3:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Computes the dot product of the first three components of the
 * two given #graphene_simd4f_t.
 *
 * Returns: a vector whose components are all set to the
 *   dot product of the components of the two operands
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_dot3) (const graphene_simd4f_t a,
                        const graphene_simd4f_t b)
{
  return graphene_simd4f_dot3 (a, b);
}

/**
 * graphene_simd4f_dot3_scalar:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Computes the dot product of the first three components of the
 * two given #graphene_simd4f_t.
 *
 * Returns: the dot product of the two vectors, as a scalar value.
 *
 * Since: 1.4
 */
float
(graphene_simd4f_dot3_scalar) (const graphene_simd4f_t a,
                               const graphene_simd4f_t b)
{
  return graphene_simd4f_dot3_scalar (a, b);
}

/**
 * graphene_simd4f_min:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * minimum value of each component of @a and @b.
 *
 * Returns: the new minimum vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_min) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_min (a, b);
}

/**
 * graphene_simd4f_max:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * maximum value of each component of @a and @b.
 *
 * Returns: the new maximum vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_max) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_max (a, b);
}

/**
 * graphene_simd4f_shuffle_wxyz:
 * @s: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * re-ordered values of the W, X, Y, and Z components
 * of @s.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_shuffle_wxyz) (const graphene_simd4f_t s)
{
  return graphene_simd4f_shuffle_wxyz (s);
}

/**
 * graphene_simd4f_shuffle_zwxy:
 * @s: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * re-ordered values of the Z, W, X, and Y components
 * of @s.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_shuffle_zwxy) (const graphene_simd4f_t s)
{
  return graphene_simd4f_shuffle_zwxy (s);
}

/**
 * graphene_simd4f_shuffle_yzwx:
 * @s: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * re-ordered values of the Y, Z, W, and X components
 * of @s.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_shuffle_yzwx) (const graphene_simd4f_t s)
{
  return graphene_simd4f_shuffle_yzwx (s);
}

/**
 * graphene_simd4f_zero_w:
 * @s: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * same values of the given @s vector, except for the
 * W component, which is set to 0.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_zero_w) (const graphene_simd4f_t s)
{
  return graphene_simd4f_zero_w (s);
}

/**
 * graphene_simd4f_zero_zw:
 * @s: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * same values of the given @s vector, except for the
 * Z and W components, which are set to 0.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_zero_zw) (const graphene_simd4f_t s)
{
  return graphene_simd4f_zero_zw (s);
}

/**
 * graphene_simd4f_merge_w:
 * @s: a #graphene_simd4f_t
 * @v: the new value of the W component
 *
 * Creates a new #graphene_simd4f_t that contains the
 * same values of the given @s vector, except for the
 * W component, which is set to @v.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_merge_w) (const graphene_simd4f_t s,
                           float                   v)
{
  return graphene_simd4f_merge_w (s, v);
}

/**
 * graphene_simd4f_merge_high:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * last two components of the vector @a and the last
 * two components of the vector @b.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_merge_high) (const graphene_simd4f_t a,
                              const graphene_simd4f_t b)
{
  return graphene_simd4f_merge_high (a, b);
}

/**
 * graphene_simd4f_merge_low:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Creates a new #graphene_simd4f_t that contains the
 * first two components of the vector @a and the first
 * two components of the vector @b.
 *
 * Returns: the new vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_merge_low) (const graphene_simd4f_t a,
                             const graphene_simd4f_t b)
{
  return graphene_simd4f_merge_low (a, b);
}

/**
 * graphene_simd4f_flip_sign_0101:
 * @s: a #graphene_simd4f_t
 *
 * Flips the signs of the second and fourth components of
 * the given vector @s.
 *
 * Returns: the new vector, with the changed signs
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_flip_sign_0101) (const graphene_simd4f_t s)
{
  return graphene_simd4f_flip_sign_0101 (s);
}

/**
 * graphene_simd4f_flip_sign_1010:
 * @s: a #graphene_simd4f_t
 *
 * Flips the signs of the first and third components of
 * the given vector @s.
 *
 * Returns: the new vector, with the changed signs
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_flip_sign_1010) (const graphene_simd4f_t s)
{
  return graphene_simd4f_flip_sign_1010 (s);
}

/**
 * graphene_simd4f_cmp_eq:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Checks if the two given #graphene_simd4f_t are equal.
 *
 * Returns: `true` if the values of the vectors are equal
 *
 * Since: 1.0
 */
bool
(graphene_simd4f_cmp_eq) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_cmp_eq (a, b);
}

/**
 * graphene_simd4f_cmp_neq:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Checks if the two given #graphene_simd4f_t are not equal.
 *
 * Returns: `true` if the values of the vectors are not equal
 *
 * Since: 1.0
 */
bool
(graphene_simd4f_cmp_neq) (const graphene_simd4f_t a,
                           const graphene_simd4f_t b)
{
  return graphene_simd4f_cmp_neq (a, b);
}

/**
 * graphene_simd4f_cmp_lt:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Compares two #graphene_simd4f_t and checks if all components
 * of the vector @a are less than the respective components of
 * the vector @b.
 *
 * Returns: `true` if vector @a is less than @b
 *
 * Since: 1.2
 */
bool
(graphene_simd4f_cmp_lt) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_cmp_lt (a, b);
}

/**
 * graphene_simd4f_cmp_le:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Compares two #graphene_simd4f_t and checks if all components
 * of the vector @a are less than or equal to the respective components
 * of the vector @b.
 *
 * Returns: `true` if vector @a is less than or equal to @b
 *
 * Since: 1.2
 */
bool
(graphene_simd4f_cmp_le) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_cmp_le (a, b);
}

/**
 * graphene_simd4f_cmp_ge:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Compares two #graphene_simd4f_t and checks if all components
 * of the vector @a are greater than or equal to the respective
 * components of the vector @b.
 *
 * Returns: `true` if vector @a is greater than or equal to @b
 *
 * Since: 1.0
 */
bool
(graphene_simd4f_cmp_ge) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_cmp_ge (a, b);
}

/**
 * graphene_simd4f_cmp_gt:
 * @a: a #graphene_simd4f_t
 * @b: a #graphene_simd4f_t
 *
 * Compares two #graphene_simd4f_t and checks if all components
 * of the vector @a are greater than the respective components of
 * the vector @b.
 *
 * Returns: `true` if vector @a is greater than @b
 *
 * Since: 1.0
 */
bool
(graphene_simd4f_cmp_gt) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_cmp_gt (a, b);
}

/**
 * graphene_simd4f_neg:
 * @s: a #graphene_simd4f_t
 *
 * Negates the values of @s.
 *
 * Returns: the negated vector
 *
 * Since: 1.0
 */
graphene_simd4f_t
(graphene_simd4f_neg) (const graphene_simd4f_t s)
{
  return graphene_simd4f_neg (s);
}

#else /* GRAPHENE_USE_SCALAR */

graphene_simd4f_t
(graphene_simd4f_init) (float x,
                        float y,
                        float z,
                        float w)
{
  graphene_simd4f_t s = { x, y, z, w };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_init_zero) (void)
{
  return graphene_simd4f_init (0.f, 0.f, 0.f, 0.f);
}

graphene_simd4f_t
(graphene_simd4f_init_4f) (const float *v)
{
  return graphene_simd4f_init (v[0], v[1], v[2], v[3]);
}

graphene_simd4f_t
(graphene_simd4f_init_3f) (const float *v)
{
  return graphene_simd4f_init (v[0], v[1], v[2], 0.f);
}

graphene_simd4f_t
(graphene_simd4f_init_2f) (const float *v)
{
  return graphene_simd4f_init (v[0], v[1], 0.f, 0.f);
}

void
(graphene_simd4f_dup_4f) (const graphene_simd4f_t  s,
                          float                   *v)
{
  memcpy (v, &s, sizeof (float) * 4);
}

void
(graphene_simd4f_dup_3f) (const graphene_simd4f_t  s,
                          float                   *v)
{
  memcpy (v, &s, sizeof (float) * 3);
}

void
(graphene_simd4f_dup_2f) (const graphene_simd4f_t  s,
                        float                   *v)
{
  memcpy (v, &s, sizeof (float) * 2);
}

float
(graphene_simd4f_get) (const graphene_simd4f_t s,
                       unsigned int            i)
{
  switch (i)
    {
    case 0:
      return s.x;
    case 1:
      return s.y;
    case 2:
      return s.z;
    case 3:
      return s.w;
    }

  return 0;
}

float
(graphene_simd4f_get_x) (const graphene_simd4f_t s)
{
  return s.x;
}

float
(graphene_simd4f_get_y) (const graphene_simd4f_t s)
{
  return s.y;
}

float
(graphene_simd4f_get_z) (const graphene_simd4f_t s)
{
  return s.z;
}

float
(graphene_simd4f_get_w) (const graphene_simd4f_t s)
{
  return s.w;
}

graphene_simd4f_t
(graphene_simd4f_splat) (float v)
{
  graphene_simd4f_t s = { v, v, v, v };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_splat_x) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = { v.x, v.x, v.x, v.x };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_splat_y) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = { v.y, v.y, v.y, v.y };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_splat_z) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = { v.z, v.z, v.z, v.z };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_splat_w) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = { v.w, v.w, v.w, v.w };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_reciprocal) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = {
#if defined (HAVE_IEEE_754_FLOAT_DIVISION)
    1.0f / v.x,
    1.0f / v.y,
    1.0f / v.z,
    1.0f / v.w
#else
    fabsf (v.x) > FLT_EPSILON ? 1.0f / v.x : copysignf (INFINITY, v.x),
    fabsf (v.y) > FLT_EPSILON ? 1.0f / v.y : copysignf (INFINITY, v.y),
    fabsf (v.z) > FLT_EPSILON ? 1.0f / v.z : copysignf (INFINITY, v.z),
    fabsf (v.w) > FLT_EPSILON ? 1.0f / v.w : copysignf (INFINITY, v.w)
#endif
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_sqrt) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = {
    sqrtf (v.x),
    sqrtf (v.y),
    sqrtf (v.z),
    sqrtf (v.w)
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_rsqrt) (graphene_simd4f_t v)
{
  graphene_simd4f_t s = {
    fabsf (v.x) > FLT_EPSILON ? 1.0f / sqrtf (v.x) : 0.f,
    fabsf (v.y) > FLT_EPSILON ? 1.0f / sqrtf (v.y) : 0.f,
    fabsf (v.z) > FLT_EPSILON ? 1.0f / sqrtf (v.z) : 0.f,
    fabsf (v.w) > FLT_EPSILON ? 1.0f / sqrtf (v.w) : 0.f
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_add) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  graphene_simd4f_t s = {
    a.x + b.x,
    a.y + b.y,
    a.z + b.z,
    a.w + b.w
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_sub) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  graphene_simd4f_t s = {
    a.x - b.x,
    a.y - b.y,
    a.z - b.z,
    a.w - b.w
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_mul) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  graphene_simd4f_t s = {
    a.x * b.x,
    a.y * b.y,
    a.z * b.z,
    a.w * b.w
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_div) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  graphene_simd4f_t s = {
    fabsf (b.x) > FLT_EPSILON ? a.x / b.x : 0.f,
    fabsf (b.y) > FLT_EPSILON ? a.y / b.y : 0.f,
    fabsf (b.z) > FLT_EPSILON ? a.z / b.z : 0.f,
    fabsf (b.w) > FLT_EPSILON ? a.w / b.w : 0.f
  };
  return s;
}

graphene_simd4f_t
(graphene_simd4f_cross3) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return graphene_simd4f_init (a.y * b.z - a.z * b.y,
                               a.z * b.x - a.x * b.z,
                               a.x * b.y - a.y * b.x,
                               0.f);
}

graphene_simd4f_t
(graphene_simd4f_dot3) (const graphene_simd4f_t a,
                        const graphene_simd4f_t b)
{
  return graphene_simd4f_splat (graphene_simd4f_dot3_scalar (a, b));
}

float
(graphene_simd4f_dot3_scalar) (const graphene_simd4f_t a,
                               const graphene_simd4f_t b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

graphene_simd4f_t
(graphene_simd4f_min) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_init (a.x < b.x ? a.x : b.x,
                               a.y < b.y ? a.y : b.y,
                               a.z < b.z ? a.z : b.z,
                               a.w < b.w ? a.w : b.w);
}

graphene_simd4f_t
(graphene_simd4f_max) (const graphene_simd4f_t a,
                       const graphene_simd4f_t b)
{
  return graphene_simd4f_init (a.x > b.x ? a.x : b.x,
                               a.y > b.y ? a.y : b.y,
                               a.z > b.z ? a.z : b.z,
                               a.w > b.w ? a.w : b.w);
}

graphene_simd4f_t
(graphene_simd4f_shuffle_wxyz) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (s.w, s.x, s.y, s.z);
}

graphene_simd4f_t
(graphene_simd4f_shuffle_zwxy) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (s.z, s.w, s.x, s.y);
}

graphene_simd4f_t
(graphene_simd4f_shuffle_yzwx) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (s.y, s.z, s.w, s.x);
}

graphene_simd4f_t
(graphene_simd4f_zero_w) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (s.x, s.y, s.z, 0.0f);
}

graphene_simd4f_t
(graphene_simd4f_zero_zw) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (s.x, s.y, 0.0f, 0.0f);
}

graphene_simd4f_t
(graphene_simd4f_merge_w) (const graphene_simd4f_t s,
                           float                   v)
{
  return graphene_simd4f_init (s.x, s.y, s.z, v);
}

graphene_simd4f_t
(graphene_simd4f_merge_high) (const graphene_simd4f_t a,
                              const graphene_simd4f_t b)
{
  return graphene_simd4f_init (a.z, a.w, b.z, b.w);
}

graphene_simd4f_t
(graphene_simd4f_merge_low) (const graphene_simd4f_t a,
                             const graphene_simd4f_t b)
{
  return graphene_simd4f_init (a.x, a.y, b.x, b.y);
}

graphene_simd4f_t
(graphene_simd4f_flip_sign_0101) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (s.x, -s.y, s.z, -s.w);
}

graphene_simd4f_t
(graphene_simd4f_flip_sign_1010) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (-s.x, s.y, -s.z, s.w);
}

static inline bool
approx_equal (float a,
              float b,
              float epsilon)
{
#ifdef HAVE_ISINFF
  if (isinff (a) && isinff (b))
    return true;
#else
  if (fpclassify (a) == FP_INFINITE && fpclassify (b) == FP_INFINITE)
    return true;
#endif

  float diff = fabsf (a - b);
  if (isnan (diff))
    return true;

  if (diff <= epsilon)
    return true;

  float abs_a = fabsf (a);
  float abs_b = fabsf (b);

  float largest = abs_b > abs_a ? abs_b : abs_a;

  return diff <= largest * epsilon;
}

bool
(graphene_simd4f_cmp_eq) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return approx_equal (a.x, b.x, FLT_EPSILON) &&
         approx_equal (a.y, b.y, FLT_EPSILON) &&
         approx_equal (a.z, b.z, FLT_EPSILON) &&
         approx_equal (a.w, b.w, FLT_EPSILON);
}

bool
(graphene_simd4f_cmp_neq) (const graphene_simd4f_t a,
                           const graphene_simd4f_t b)
{
  return !approx_equal (a.x, b.x, FLT_EPSILON) ||
         !approx_equal (a.y, b.y, FLT_EPSILON) ||
         !approx_equal (a.z, b.z, FLT_EPSILON) ||
         !approx_equal (a.w, b.w, FLT_EPSILON);
}

bool
(graphene_simd4f_cmp_lt) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return a.x < b.x &&
         a.y < b.y &&
         a.z < b.z &&
         a.w < b.w;
}

bool
(graphene_simd4f_cmp_le) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return a.x <= b.x &&
         a.y <= b.y &&
         a.z <= b.z &&
         a.w <= b.w;
}

bool
(graphene_simd4f_cmp_ge) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return a.x >= b.x &&
         a.y >= b.y &&
         a.z >= b.z &&
         a.w >= b.w;
}

bool
(graphene_simd4f_cmp_gt) (const graphene_simd4f_t a,
                          const graphene_simd4f_t b)
{
  return a.x > b.x &&
         a.y > b.y &&
         a.z > b.z &&
         a.w > b.w;
}

graphene_simd4f_t
(graphene_simd4f_neg) (const graphene_simd4f_t s)
{
  return graphene_simd4f_init (-s.x, -s.y, -s.z, -s.w);
}

#endif /* GRAPHENE_USE_SCALAR */
