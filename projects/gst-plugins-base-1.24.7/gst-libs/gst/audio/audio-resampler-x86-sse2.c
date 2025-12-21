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

#include "audio-resampler-x86-sse2.h"

#if defined (HAVE_EMMINTRIN_H) && defined(__SSE2__)
#include <emmintrin.h>

static inline void
inner_product_gint16_full_1_sse2 (gint16 * o, const gint16 * a,
    const gint16 * b, gint len, const gint16 * icoeff, gint bstride)
{
  gint i;
  __m128i sum, t;

  sum = _mm_setzero_si128 ();

  for (i = 0; i < len; i += 16) {
    t = _mm_loadu_si128 ((__m128i *) (a + i));
    sum =
        _mm_add_epi32 (sum, _mm_madd_epi16 (t,
            _mm_load_si128 ((__m128i *) (b + i + 0))));

    t = _mm_loadu_si128 ((__m128i *) (a + i + 8));
    sum =
        _mm_add_epi32 (sum, _mm_madd_epi16 (t,
            _mm_load_si128 ((__m128i *) (b + i + 8))));
  }
  sum = _mm_add_epi32 (sum, _mm_shuffle_epi32 (sum, _MM_SHUFFLE (2, 3, 2, 3)));
  sum = _mm_add_epi32 (sum, _mm_shuffle_epi32 (sum, _MM_SHUFFLE (1, 1, 1, 1)));

  sum = _mm_add_epi32 (sum, _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));
  sum = _mm_srai_epi32 (sum, PRECISION_S16);
  sum = _mm_packs_epi32 (sum, sum);
  *o = _mm_extract_epi16 (sum, 0);
}

static inline void
inner_product_gint16_linear_1_sse2 (gint16 * o, const gint16 * a,
    const gint16 * b, gint len, const gint16 * icoeff, gint bstride)
{
  gint i = 0;
  __m128i sum[2], t;
  __m128i f = _mm_set_epi64x (0, *((gint64 *) icoeff));
  const gint16 *c[2] = { (gint16 *) ((gint8 *) b + 0 * bstride),
    (gint16 *) ((gint8 *) b + 1 * bstride)
  };

  sum[0] = sum[1] = _mm_setzero_si128 ();
  f = _mm_unpacklo_epi16 (f, sum[0]);

  for (; i < len; i += 16) {
    t = _mm_loadu_si128 ((__m128i *) (a + i + 0));
    sum[0] =
        _mm_add_epi32 (sum[0], _mm_madd_epi16 (t,
            _mm_load_si128 ((__m128i *) (c[0] + i + 0))));
    sum[1] =
        _mm_add_epi32 (sum[1], _mm_madd_epi16 (t,
            _mm_load_si128 ((__m128i *) (c[1] + i + 0))));

    t = _mm_loadu_si128 ((__m128i *) (a + i + 8));
    sum[0] =
        _mm_add_epi32 (sum[0], _mm_madd_epi16 (t,
            _mm_load_si128 ((__m128i *) (c[0] + i + 8))));
    sum[1] =
        _mm_add_epi32 (sum[1], _mm_madd_epi16 (t,
            _mm_load_si128 ((__m128i *) (c[1] + i + 8))));
  }
  sum[0] = _mm_srai_epi32 (sum[0], PRECISION_S16);
  sum[1] = _mm_srai_epi32 (sum[1], PRECISION_S16);

  sum[0] =
      _mm_madd_epi16 (sum[0], _mm_shuffle_epi32 (f, _MM_SHUFFLE (0, 0, 0, 0)));
  sum[1] =
      _mm_madd_epi16 (sum[1], _mm_shuffle_epi32 (f, _MM_SHUFFLE (1, 1, 1, 1)));
  sum[0] = _mm_add_epi32 (sum[0], sum[1]);

  sum[0] =
      _mm_add_epi32 (sum[0], _mm_shuffle_epi32 (sum[0], _MM_SHUFFLE (2, 3, 2,
              3)));
  sum[0] =
      _mm_add_epi32 (sum[0], _mm_shuffle_epi32 (sum[0], _MM_SHUFFLE (1, 1, 1,
              1)));

  sum[0] = _mm_add_epi32 (sum[0], _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));
  sum[0] = _mm_srai_epi32 (sum[0], PRECISION_S16);
  sum[0] = _mm_packs_epi32 (sum[0], sum[0]);
  *o = _mm_extract_epi16 (sum[0], 0);
}

static inline void
inner_product_gint16_cubic_1_sse2 (gint16 * o, const gint16 * a,
    const gint16 * b, gint len, const gint16 * icoeff, gint bstride)
{
  gint i = 0;
  __m128i sum[4], t[4];
  __m128i f = _mm_set_epi64x (0, *((long long *) icoeff));
  const gint16 *c[4] = { (gint16 *) ((gint8 *) b + 0 * bstride),
    (gint16 *) ((gint8 *) b + 1 * bstride),
    (gint16 *) ((gint8 *) b + 2 * bstride),
    (gint16 *) ((gint8 *) b + 3 * bstride)
  };

  sum[0] = sum[1] = sum[2] = sum[3] = _mm_setzero_si128 ();
  f = _mm_unpacklo_epi16 (f, sum[0]);

  for (; i < len; i += 8) {
    t[0] = _mm_loadu_si128 ((__m128i *) (a + i));
    sum[0] =
        _mm_add_epi32 (sum[0], _mm_madd_epi16 (t[0],
            _mm_load_si128 ((__m128i *) (c[0] + i))));
    sum[1] =
        _mm_add_epi32 (sum[1], _mm_madd_epi16 (t[0],
            _mm_load_si128 ((__m128i *) (c[1] + i))));
    sum[2] =
        _mm_add_epi32 (sum[2], _mm_madd_epi16 (t[0],
            _mm_load_si128 ((__m128i *) (c[2] + i))));
    sum[3] =
        _mm_add_epi32 (sum[3], _mm_madd_epi16 (t[0],
            _mm_load_si128 ((__m128i *) (c[3] + i))));
  }
  t[0] = _mm_unpacklo_epi32 (sum[0], sum[1]);
  t[1] = _mm_unpacklo_epi32 (sum[2], sum[3]);
  t[2] = _mm_unpackhi_epi32 (sum[0], sum[1]);
  t[3] = _mm_unpackhi_epi32 (sum[2], sum[3]);

  sum[0] =
      _mm_add_epi32 (_mm_unpacklo_epi64 (t[0], t[1]), _mm_unpackhi_epi64 (t[0],
          t[1]));
  sum[2] =
      _mm_add_epi32 (_mm_unpacklo_epi64 (t[2], t[3]), _mm_unpackhi_epi64 (t[2],
          t[3]));
  sum[0] = _mm_add_epi32 (sum[0], sum[2]);

  sum[0] = _mm_srai_epi32 (sum[0], PRECISION_S16);
  sum[0] = _mm_madd_epi16 (sum[0], f);

  sum[0] =
      _mm_add_epi32 (sum[0], _mm_shuffle_epi32 (sum[0], _MM_SHUFFLE (2, 3, 2,
              3)));
  sum[0] =
      _mm_add_epi32 (sum[0], _mm_shuffle_epi32 (sum[0], _MM_SHUFFLE (1, 1, 1,
              1)));

  sum[0] = _mm_add_epi32 (sum[0], _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));
  sum[0] = _mm_srai_epi32 (sum[0], PRECISION_S16);
  sum[0] = _mm_packs_epi32 (sum[0], sum[0]);
  *o = _mm_extract_epi16 (sum[0], 0);
}

static inline void
inner_product_gdouble_full_1_sse2 (gdouble * o, const gdouble * a,
    const gdouble * b, gint len, const gdouble * icoeff, gint bstride)
{
  gint i = 0;
  __m128d sum = _mm_setzero_pd ();

  for (; i < len; i += 8) {
    sum =
        _mm_add_pd (sum, _mm_mul_pd (_mm_loadu_pd (a + i + 0),
            _mm_load_pd (b + i + 0)));
    sum =
        _mm_add_pd (sum, _mm_mul_pd (_mm_loadu_pd (a + i + 2),
            _mm_load_pd (b + i + 2)));
    sum =
        _mm_add_pd (sum, _mm_mul_pd (_mm_loadu_pd (a + i + 4),
            _mm_load_pd (b + i + 4)));
    sum =
        _mm_add_pd (sum, _mm_mul_pd (_mm_loadu_pd (a + i + 6),
            _mm_load_pd (b + i + 6)));
  }
  sum = _mm_add_sd (sum, _mm_unpackhi_pd (sum, sum));
  _mm_store_sd (o, sum);
}

static inline void
inner_product_gdouble_linear_1_sse2 (gdouble * o, const gdouble * a,
    const gdouble * b, gint len, const gdouble * icoeff, gint bstride)
{
  gint i = 0;
  __m128d sum[2], t;
  const gdouble *c[2] = { (gdouble *) ((gint8 *) b + 0 * bstride),
    (gdouble *) ((gint8 *) b + 1 * bstride)
  };

  sum[0] = sum[1] = _mm_setzero_pd ();

  for (; i < len; i += 4) {
    t = _mm_loadu_pd (a + i + 0);
    sum[0] = _mm_add_pd (sum[0], _mm_mul_pd (t, _mm_load_pd (c[0] + i + 0)));
    sum[1] = _mm_add_pd (sum[1], _mm_mul_pd (t, _mm_load_pd (c[1] + i + 0)));
    t = _mm_loadu_pd (a + i + 2);
    sum[0] = _mm_add_pd (sum[0], _mm_mul_pd (t, _mm_load_pd (c[0] + i + 2)));
    sum[1] = _mm_add_pd (sum[1], _mm_mul_pd (t, _mm_load_pd (c[1] + i + 2)));
  }
  sum[0] = _mm_mul_pd (_mm_sub_pd (sum[0], sum[1]), _mm_load1_pd (icoeff));
  sum[0] = _mm_add_pd (sum[0], sum[1]);
  sum[0] = _mm_add_sd (sum[0], _mm_unpackhi_pd (sum[0], sum[0]));
  _mm_store_sd (o, sum[0]);
}

static inline void
inner_product_gdouble_cubic_1_sse2 (gdouble * o, const gdouble * a,
    const gdouble * b, gint len, const gdouble * icoeff, gint bstride)
{
  gint i;
  __m128d f[2], sum[4], t;
  const gdouble *c[4] = { (gdouble *) ((gint8 *) b + 0 * bstride),
    (gdouble *) ((gint8 *) b + 1 * bstride),
    (gdouble *) ((gint8 *) b + 2 * bstride),
    (gdouble *) ((gint8 *) b + 3 * bstride)
  };

  f[0] = _mm_loadu_pd (icoeff + 0);
  f[1] = _mm_loadu_pd (icoeff + 2);
  sum[0] = sum[1] = sum[2] = sum[3] = _mm_setzero_pd ();

  for (i = 0; i < len; i += 2) {
    t = _mm_loadu_pd (a + i + 0);
    sum[0] = _mm_add_pd (sum[0], _mm_mul_pd (t, _mm_load_pd (c[0] + i)));
    sum[1] = _mm_add_pd (sum[1], _mm_mul_pd (t, _mm_load_pd (c[1] + i)));
    sum[2] = _mm_add_pd (sum[2], _mm_mul_pd (t, _mm_load_pd (c[2] + i)));
    sum[3] = _mm_add_pd (sum[3], _mm_mul_pd (t, _mm_load_pd (c[3] + i)));
  }
  sum[0] =
      _mm_mul_pd (sum[0], _mm_shuffle_pd (f[0], f[0], _MM_SHUFFLE2 (0, 0)));
  sum[1] =
      _mm_mul_pd (sum[1], _mm_shuffle_pd (f[0], f[0], _MM_SHUFFLE2 (1, 1)));
  sum[2] =
      _mm_mul_pd (sum[2], _mm_shuffle_pd (f[1], f[1], _MM_SHUFFLE2 (0, 0)));
  sum[3] =
      _mm_mul_pd (sum[3], _mm_shuffle_pd (f[1], f[1], _MM_SHUFFLE2 (1, 1)));
  sum[0] = _mm_add_pd (sum[0], sum[1]);
  sum[2] = _mm_add_pd (sum[2], sum[3]);
  sum[0] = _mm_add_pd (sum[0], sum[2]);
  sum[0] = _mm_add_sd (sum[0], _mm_unpackhi_pd (sum[0], sum[0]));
  _mm_store_sd (o, sum[0]);
}

MAKE_RESAMPLE_FUNC (gint16, full, 1, sse2);
MAKE_RESAMPLE_FUNC (gint16, linear, 1, sse2);
MAKE_RESAMPLE_FUNC (gint16, cubic, 1, sse2);

MAKE_RESAMPLE_FUNC (gdouble, full, 1, sse2);
MAKE_RESAMPLE_FUNC (gdouble, linear, 1, sse2);
MAKE_RESAMPLE_FUNC (gdouble, cubic, 1, sse2);

void
interpolate_gint16_linear_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
  gint i = 0;
  gint16 *o = op, *a = ap, *ic = icp;
  __m128i ta, tb, t1, t2;
  __m128i f = _mm_set_epi64x (0, *((gint64 *) ic));
  const gint16 *c[2] = { (gint16 *) ((gint8 *) a + 0 * astride),
    (gint16 *) ((gint8 *) a + 1 * astride)
  };

  f = _mm_unpacklo_epi32 (f, f);
  f = _mm_unpacklo_epi64 (f, f);

  for (; i < len; i += 8) {
    ta = _mm_load_si128 ((__m128i *) (c[0] + i));
    tb = _mm_load_si128 ((__m128i *) (c[1] + i));

    t1 = _mm_madd_epi16 (_mm_unpacklo_epi16 (ta, tb), f);
    t2 = _mm_madd_epi16 (_mm_unpackhi_epi16 (ta, tb), f);

    t1 = _mm_add_epi32 (t1, _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));
    t2 = _mm_add_epi32 (t2, _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));

    t1 = _mm_srai_epi32 (t1, PRECISION_S16);
    t2 = _mm_srai_epi32 (t2, PRECISION_S16);

    t1 = _mm_packs_epi32 (t1, t2);
    _mm_store_si128 ((__m128i *) (o + i), t1);
  }
}

void
interpolate_gint16_cubic_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
  gint i = 0;
  gint16 *o = op, *a = ap, *ic = icp;
  __m128i ta, tb, tl1, tl2, th1, th2;
  __m128i f[2];
  const gint16 *c[4] = { (gint16 *) ((gint8 *) a + 0 * astride),
    (gint16 *) ((gint8 *) a + 1 * astride),
    (gint16 *) ((gint8 *) a + 2 * astride),
    (gint16 *) ((gint8 *) a + 3 * astride)
  };

  f[0] = _mm_set_epi16 (ic[1], ic[0], ic[1], ic[0], ic[1], ic[0], ic[1], ic[0]);
  f[1] = _mm_set_epi16 (ic[3], ic[2], ic[3], ic[2], ic[3], ic[2], ic[3], ic[2]);

  for (; i < len; i += 8) {
    ta = _mm_load_si128 ((__m128i *) (c[0] + i));
    tb = _mm_load_si128 ((__m128i *) (c[1] + i));

    tl1 = _mm_madd_epi16 (_mm_unpacklo_epi16 (ta, tb), f[0]);
    th1 = _mm_madd_epi16 (_mm_unpackhi_epi16 (ta, tb), f[0]);

    ta = _mm_load_si128 ((__m128i *) (c[2] + i));
    tb = _mm_load_si128 ((__m128i *) (c[3] + i));

    tl2 = _mm_madd_epi16 (_mm_unpacklo_epi16 (ta, tb), f[1]);
    th2 = _mm_madd_epi16 (_mm_unpackhi_epi16 (ta, tb), f[1]);

    tl1 = _mm_add_epi32 (tl1, tl2);
    th1 = _mm_add_epi32 (th1, th2);

    tl1 = _mm_add_epi32 (tl1, _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));
    th1 = _mm_add_epi32 (th1, _mm_set1_epi32 (1 << (PRECISION_S16 - 1)));

    tl1 = _mm_srai_epi32 (tl1, PRECISION_S16);
    th1 = _mm_srai_epi32 (th1, PRECISION_S16);

    tl1 = _mm_packs_epi32 (tl1, th1);
    _mm_store_si128 ((__m128i *) (o + i), tl1);
  }
}

void
interpolate_gdouble_linear_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
  gint i;
  gdouble *o = op, *a = ap, *ic = icp;
  __m128d f[2], t1, t2;
  const gdouble *c[2] = { (gdouble *) ((gint8 *) a + 0 * astride),
    (gdouble *) ((gint8 *) a + 1 * astride)
  };

  f[0] = _mm_load1_pd (ic + 0);
  f[1] = _mm_load1_pd (ic + 1);

  for (i = 0; i < len; i += 4) {
    t1 = _mm_mul_pd (_mm_load_pd (c[0] + i + 0), f[0]);
    t2 = _mm_mul_pd (_mm_load_pd (c[1] + i + 0), f[1]);
    _mm_store_pd (o + i + 0, _mm_add_pd (t1, t2));

    t1 = _mm_mul_pd (_mm_load_pd (c[0] + i + 2), f[0]);
    t2 = _mm_mul_pd (_mm_load_pd (c[1] + i + 2), f[1]);
    _mm_store_pd (o + i + 2, _mm_add_pd (t1, t2));
  }
}

void
interpolate_gdouble_cubic_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
  gint i;
  gdouble *o = op, *a = ap, *ic = icp;
  __m128d f[4], t[4];
  const gdouble *c[4] = { (gdouble *) ((gint8 *) a + 0 * astride),
    (gdouble *) ((gint8 *) a + 1 * astride),
    (gdouble *) ((gint8 *) a + 2 * astride),
    (gdouble *) ((gint8 *) a + 3 * astride)
  };

  f[0] = _mm_load1_pd (ic + 0);
  f[1] = _mm_load1_pd (ic + 1);
  f[2] = _mm_load1_pd (ic + 2);
  f[3] = _mm_load1_pd (ic + 3);

  for (i = 0; i < len; i += 2) {
    t[0] = _mm_mul_pd (_mm_load_pd (c[0] + i + 0), f[0]);
    t[1] = _mm_mul_pd (_mm_load_pd (c[1] + i + 0), f[1]);
    t[2] = _mm_mul_pd (_mm_load_pd (c[2] + i + 0), f[2]);
    t[3] = _mm_mul_pd (_mm_load_pd (c[3] + i + 0), f[3]);
    t[0] = _mm_add_pd (t[0], t[1]);
    t[2] = _mm_add_pd (t[2], t[3]);
    _mm_store_pd (o + i + 0, _mm_add_pd (t[0], t[2]));
  }
}

#endif
