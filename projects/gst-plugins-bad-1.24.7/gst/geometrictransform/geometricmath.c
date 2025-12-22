/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Thanks to Jerry Huxtable <http://www.jhlabs.com> work on its java
 * image editor and filters. The algorithms here were extracted from
 * his code.
 */

#include "geometricmath.h"
#include <math.h>

#define N  0x1000
#define B  0x100
#define BM 0xff

struct _GstGMNoise
{
  gdouble p[2 * B + 2];
  gdouble g2[2 * B + 2][2];
};

static void
normalize_2 (gdouble * v)
{
  gdouble s = sqrt (v[0] * v[0] + v[1] * v[1]);

  v[0] = v[0] / s;
  v[1] = v[1] / s;
}

GstGMNoise *
gst_gm_noise_new (void)
{
  GstGMNoise *noise = g_new0 (GstGMNoise, 1);
  gint i, j, k;

  for (i = 0; i < B; i++) {
    noise->p[i] = i;
    for (j = 0; j < 2; j++) {
      noise->g2[i][j] = ((g_random_int () % (2 * B))
          - B) / (gdouble) B;
    }
    normalize_2 (noise->g2[i]);
  }

  for (i = B - 1; i >= 0; i--) {
    k = noise->p[i];
    j = g_random_int () % B;
    noise->p[i] = noise->p[j];
    noise->p[j] = k;
  }

  for (i = 0; i < B + 2; i++) {
    noise->p[B + i] = noise->p[i];
    for (j = 0; j < 2; j++) {
      noise->g2[B + i][j] = noise->g2[i][j];
    }
  }

  return noise;
}

void
gst_gm_noise_free (GstGMNoise * noise)
{
  g_free (noise);
}

static gdouble
s_curve (gdouble x)
{
  return x * x * (3.0 - 2.0 * x);
}

static gdouble
lerp (gdouble t, gdouble a, gdouble b)
{
  return a + t * (b - a);
}

gdouble
gst_gm_noise_2 (GstGMNoise * noise, gdouble x, gdouble y)
{
  gint bx0, bx1, by0, by1, b00, b10, b01, b11;
  gdouble rx0, rx1, ry0, ry1, sx, sy, a, b, t, u, v;
  gdouble *q;
  gint i, j;

  t = x + N;
  bx0 = ((gint) t) & BM;
  bx1 = (bx0 + 1) & BM;
  rx0 = t - (gint) t;
  rx1 = rx0 - 1.0;

  t = y + N;
  by0 = ((gint) t) & BM;
  by1 = (by0 + 1) & BM;
  ry0 = t - (gint) t;
  ry1 = ry0 - 1.0;

  i = noise->p[bx0];
  j = noise->p[bx1];

  b00 = noise->p[i + by0];
  b10 = noise->p[j + by0];
  b01 = noise->p[i + by1];
  b11 = noise->p[j + by1];

  sx = s_curve (rx0);
  sy = s_curve (ry0);

  q = noise->g2[b00];
  u = rx0 * q[0] + ry0 * q[1];
  q = noise->g2[b10];
  v = rx1 * q[0] + ry0 * q[1];
  a = lerp (sx, u, v);

  q = noise->g2[b01];
  u = rx0 * q[0] + ry1 * q[1];
  q = noise->g2[b11];
  v = rx1 * q[0] + ry1 * q[1];
  b = lerp (sx, u, v);

  return 1.5 * lerp (sy, a, b);
}

/*
 * This differs from the % operator with respect to negative numbers
 */
gdouble
gst_gm_mod_float (gdouble a, gdouble b)
{
  gint n = (gint) (a / b);

  a -= n * b;
  if (a < 0)
    return a + b;
  return a;
}

/*
 * Returns a repeating triangle shape in the range 0..1 with wavelength 1.0
 */
gdouble
gst_gm_triangle (gdouble x)
{
  gdouble r = gst_gm_mod_float (x, 1.0);

  return 2.0 * (r < 0.5 ? r : 1 - r);
}

/*
 * Hermite interpolation
 */
gdouble
gst_gm_smoothstep (gdouble edge0, gdouble edge1, gdouble x)
{
  gdouble t = CLAMP ((x - edge0) / (edge1 - edge0), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}
