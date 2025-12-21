/* GStreamer
 * Copyright (C) <2016> Wim Taymans <wim.taymans@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "audio-resampler-x86-sse.h"

#if defined (HAVE_XMMINTRIN_H) && defined(__SSE__)
#include <xmmintrin.h>

static inline void
inner_product_gfloat_full_1_sse (gfloat * o, const gfloat * a,
    const gfloat * b, gint len, const gfloat * icoeff, gint bstride)
{
  gint i = 0;
  __m128 sum = _mm_setzero_ps ();

  for (; i < len; i += 8) {
    sum =
        _mm_add_ps (sum, _mm_mul_ps (_mm_loadu_ps (a + i + 0),
            _mm_load_ps (b + i + 0)));
    sum =
        _mm_add_ps (sum, _mm_mul_ps (_mm_loadu_ps (a + i + 4),
            _mm_load_ps (b + i + 4)));
  }
  sum = _mm_add_ps (sum, _mm_movehl_ps (sum, sum));
  sum = _mm_add_ss (sum, _mm_shuffle_ps (sum, sum, 0x55));
  _mm_store_ss (o, sum);
}

static inline void
inner_product_gfloat_linear_1_sse (gfloat * o, const gfloat * a,
    const gfloat * b, gint len, const gfloat * icoeff, gint bstride)
{
  gint i = 0;
  __m128 sum[2], t;
  const gfloat *c[2] = { (gfloat *) ((gint8 *) b + 0 * bstride),
    (gfloat *) ((gint8 *) b + 1 * bstride)
  };

  sum[0] = sum[1] = _mm_setzero_ps ();

  for (; i < len; i += 8) {
    t = _mm_loadu_ps (a + i + 0);
    sum[0] = _mm_add_ps (sum[0], _mm_mul_ps (t, _mm_load_ps (c[0] + i + 0)));
    sum[1] = _mm_add_ps (sum[1], _mm_mul_ps (t, _mm_load_ps (c[1] + i + 0)));
    t = _mm_loadu_ps (a + i + 4);
    sum[0] = _mm_add_ps (sum[0], _mm_mul_ps (t, _mm_load_ps (c[0] + i + 4)));
    sum[1] = _mm_add_ps (sum[1], _mm_mul_ps (t, _mm_load_ps (c[1] + i + 4)));
  }
  sum[0] = _mm_mul_ps (_mm_sub_ps (sum[0], sum[1]), _mm_load1_ps (icoeff));
  sum[0] = _mm_add_ps (sum[0], sum[1]);
  sum[0] = _mm_add_ps (sum[0], _mm_movehl_ps (sum[0], sum[0]));
  sum[0] = _mm_add_ss (sum[0], _mm_shuffle_ps (sum[0], sum[0], 0x55));
  _mm_store_ss (o, sum[0]);
}

static inline void
inner_product_gfloat_cubic_1_sse (gfloat * o, const gfloat * a,
    const gfloat * b, gint len, const gfloat * icoeff, gint bstride)
{
  gint i = 0;
  __m128 sum[4];
  __m128 t, f = _mm_loadu_ps (icoeff);
  const gfloat *c[4] = { (gfloat *) ((gint8 *) b + 0 * bstride),
    (gfloat *) ((gint8 *) b + 1 * bstride),
    (gfloat *) ((gint8 *) b + 2 * bstride),
    (gfloat *) ((gint8 *) b + 3 * bstride)
  };

  sum[0] = sum[1] = sum[2] = sum[3] = _mm_setzero_ps ();

  for (; i < len; i += 4) {
    t = _mm_loadu_ps (a + i);
    sum[0] = _mm_add_ps (sum[0], _mm_mul_ps (t, _mm_load_ps (c[0] + i)));
    sum[1] = _mm_add_ps (sum[1], _mm_mul_ps (t, _mm_load_ps (c[1] + i)));
    sum[2] = _mm_add_ps (sum[2], _mm_mul_ps (t, _mm_load_ps (c[2] + i)));
    sum[3] = _mm_add_ps (sum[3], _mm_mul_ps (t, _mm_load_ps (c[3] + i)));
  }
  sum[0] = _mm_mul_ps (sum[0], _mm_shuffle_ps (f, f, 0x00));
  sum[1] = _mm_mul_ps (sum[1], _mm_shuffle_ps (f, f, 0x55));
  sum[2] = _mm_mul_ps (sum[2], _mm_shuffle_ps (f, f, 0xaa));
  sum[3] = _mm_mul_ps (sum[3], _mm_shuffle_ps (f, f, 0xff));
  sum[0] = _mm_add_ps (sum[0], sum[1]);
  sum[2] = _mm_add_ps (sum[2], sum[3]);
  sum[0] = _mm_add_ps (sum[0], sum[2]);
  sum[0] = _mm_add_ps (sum[0], _mm_movehl_ps (sum[0], sum[0]));
  sum[0] = _mm_add_ss (sum[0], _mm_shuffle_ps (sum[0], sum[0], 0x55));
  _mm_store_ss (o, sum[0]);
}

MAKE_RESAMPLE_FUNC (gfloat, full, 1, sse);
MAKE_RESAMPLE_FUNC (gfloat, linear, 1, sse);
MAKE_RESAMPLE_FUNC (gfloat, cubic, 1, sse);

void
interpolate_gfloat_linear_sse (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
  gint i;
  gfloat *o = op, *a = ap, *ic = icp;
  __m128 f[2], t1, t2;
  const gfloat *c[2] = { (gfloat *) ((gint8 *) a + 0 * astride),
    (gfloat *) ((gint8 *) a + 1 * astride)
  };

  f[0] = _mm_load1_ps (ic + 0);
  f[1] = _mm_load1_ps (ic + 1);

  for (i = 0; i < len; i += 8) {
    t1 = _mm_mul_ps (_mm_load_ps (c[0] + i + 0), f[0]);
    t2 = _mm_mul_ps (_mm_load_ps (c[1] + i + 0), f[1]);
    _mm_store_ps (o + i + 0, _mm_add_ps (t1, t2));

    t1 = _mm_mul_ps (_mm_load_ps (c[0] + i + 4), f[0]);
    t2 = _mm_mul_ps (_mm_load_ps (c[1] + i + 4), f[1]);
    _mm_store_ps (o + i + 4, _mm_add_ps (t1, t2));
  }
}

void
interpolate_gfloat_cubic_sse (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
  gint i;
  gfloat *o = op, *a = ap, *ic = icp;
  __m128 f[4], t[4];
  const gfloat *c[4] = { (gfloat *) ((gint8 *) a + 0 * astride),
    (gfloat *) ((gint8 *) a + 1 * astride),
    (gfloat *) ((gint8 *) a + 2 * astride),
    (gfloat *) ((gint8 *) a + 3 * astride)
  };

  f[0] = _mm_load1_ps (ic + 0);
  f[1] = _mm_load1_ps (ic + 1);
  f[2] = _mm_load1_ps (ic + 2);
  f[3] = _mm_load1_ps (ic + 3);

  for (i = 0; i < len; i += 4) {
    t[0] = _mm_mul_ps (_mm_load_ps (c[0] + i + 0), f[0]);
    t[1] = _mm_mul_ps (_mm_load_ps (c[1] + i + 0), f[1]);
    t[2] = _mm_mul_ps (_mm_load_ps (c[2] + i + 0), f[2]);
    t[3] = _mm_mul_ps (_mm_load_ps (c[3] + i + 0), f[3]);
    t[0] = _mm_add_ps (t[0], t[1]);
    t[2] = _mm_add_ps (t[2], t[3]);
    _mm_store_ps (o + i + 0, _mm_add_ps (t[0], t[2]));
  }
}

#endif
