/* graphene-simd4x4f.c
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
 * SECTION:graphene-simd4x4f
 * @Title: SIMD matrix
 * @short_description: Low level floating point 4 by 4 matrix
 *
 * The #graphene_simd4x4f_t type wraps a platform specific implementation of
 * a four by four matrix of floating point values, using four #graphene_simd4f_t
 * row vectors.
 *
 * Like #graphene_simd4f_t, the #graphene_simd4x4f_t type should be treated
 * as an opaque type; you cannot access its components directly, and you can
 * only operate on all components at the same time.
 */

#include "graphene-private.h"
#include "graphene-simd4x4f.h"

#include <string.h>
#include <math.h>

#if defined(GRAPHENE_USE_SSE) || defined(GRAPHENE_USE_GCC) || defined(GRAPHENE_USE_ARM_NEON)

/**
 * graphene_simd4x4f_transpose_in_place:
 * @s: a #graphene_simd4x4f_t
 *
 * Transposes @s in place.
 *
 * Since: 1.0
 */
void
(graphene_simd4x4f_transpose_in_place) (graphene_simd4x4f_t *s)
{
  graphene_simd4x4f_transpose_in_place (s);
}

#else

void
(graphene_simd4x4f_transpose_in_place) (graphene_simd4x4f_t *s)
{
  graphene_simd4x4f_t m = *s;

  s->x.x = m.x.x;
  s->x.y = m.y.x;
  s->x.z = m.z.x;
  s->x.w = m.w.x;

  s->y.x = m.x.y;
  s->y.y = m.y.y;
  s->y.z = m.z.y;
  s->y.w = m.w.y;

  s->z.x = m.x.z;
  s->z.y = m.y.z;
  s->z.z = m.z.z;
  s->z.w = m.w.z;

  s->w.x = m.x.w;
  s->w.y = m.y.w;
  s->w.z = m.z.w;
  s->w.w = m.w.w;
}

#endif
