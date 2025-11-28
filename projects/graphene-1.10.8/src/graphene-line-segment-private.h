/* graphene-line-segment-private.h: Segments
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

#pragma once

#include "graphene-point.h"

typedef struct {
  graphene_point_t start;
  graphene_point_t end;
} graphene_line_segment_t;

static inline graphene_line_segment_t
graphene_line_segment_init (const graphene_point_t *start,
                            const graphene_point_t *end)
{
  graphene_line_segment_t l;
  l.start = *start;
  l.end = *end;

  return l;
}

static inline bool
graphene_line_segment_points_on_same_side (graphene_line_segment_t  s,
                                           const graphene_point_t  *a,
                                           const graphene_point_t  *b)
{
  const float delta_x = (s.end.x - s.start.x);
  const float delta_y = (s.end.y - s.start.y);

  const float one = delta_x * (a->y - s.start.y) - delta_y * (a->x - s.start.x);
  const float two = delta_x * (b->y - s.start.y) - delta_y * (b->x - s.start.x);

  if ((one >= 0.f && two >= 0.f) || (one <= 0.f && two <= 0.f))
    return true;

  return false;
}
