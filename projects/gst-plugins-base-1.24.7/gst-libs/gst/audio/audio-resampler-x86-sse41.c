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

#include "audio-resampler-x86-sse41.h"

#if 0
#define __SSE4_1__
#pragma GCC target("sse4.1")
#endif

#if defined (__x86_64__) && \
    defined (HAVE_SMMINTRIN_H) && defined (HAVE_EMMINTRIN_H) && \
    defined (__SSE4_1__)

#include <emmintrin.h>
#include <smmintrin.h>

static inline void
inner_product_gint32_full_1_sse41 (gint32 * o, const gint32 * a,
    const gint32 * b, gint len, const gint32 * icoeff, gint bstride)
{
  gint i = 0;
  __m128i sum, ta, tb;
  gint64 res;

  sum = _mm_setzero_si128 ();

  for (; i < len; i += 8) {
    ta = _mm_loadu_si128 ((__m128i *) (a + i));
    tb = _mm_load_si128 ((__m128i *) (b + i));

    sum =
        _mm_add_epi64 (sum, _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum =
        _mm_add_epi64 (sum, _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));

    ta = _mm_loadu_si128 ((__m128i *) (a + i + 4));
    tb = _mm_load_si128 ((__m128i *) (b + i + 4));

    sum =
        _mm_add_epi64 (sum, _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum =
        _mm_add_epi64 (sum, _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));
  }
  sum = _mm_add_epi64 (sum, _mm_unpackhi_epi64 (sum, sum));
  res = _mm_cvtsi128_si64 (sum);

  res = (res + (1 << (PRECISION_S32 - 1))) >> PRECISION_S32;
  *o = CLAMP (res, G_MININT32, G_MAXINT32);
}

static inline void
inner_product_gint32_linear_1_sse41 (gint32 * o, const gint32 * a,
    const gint32 * b, gint len, const gint32 * icoeff, gint bstride)
{
  gint i = 0;
  gint64 res;
  __m128i sum[2], ta, tb;
  __m128i f = _mm_loadu_si128 ((__m128i *) icoeff);
  const gint32 *c[2] = { (gint32 *) ((gint8 *) b + 0 * bstride),
    (gint32 *) ((gint8 *) b + 1 * bstride)
  };

  sum[0] = sum[1] = _mm_setzero_si128 ();

  for (; i < len; i += 4) {
    ta = _mm_loadu_si128 ((__m128i *) (a + i));

    tb = _mm_load_si128 ((__m128i *) (c[0] + i));
    sum[0] = _mm_add_epi64 (sum[0], _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum[0] = _mm_add_epi64 (sum[0], _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));

    tb = _mm_load_si128 ((__m128i *) (c[1] + i));
    sum[1] = _mm_add_epi64 (sum[1], _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum[1] = _mm_add_epi64 (sum[1], _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));
  }
  sum[0] = _mm_srli_epi64 (sum[0], PRECISION_S32);
  sum[1] = _mm_srli_epi64 (sum[1], PRECISION_S32);
  sum[0] =
      _mm_mul_epi32 (sum[0], _mm_shuffle_epi32 (f, _MM_SHUFFLE (0, 0, 0, 0)));
  sum[1] =
      _mm_mul_epi32 (sum[1], _mm_shuffle_epi32 (f, _MM_SHUFFLE (1, 1, 1, 1)));
  sum[0] = _mm_add_epi64 (sum[0], sum[1]);
  sum[0] = _mm_add_epi64 (sum[0], _mm_unpackhi_epi64 (sum[0], sum[0]));
  res = _mm_cvtsi128_si64 (sum[0]);

  res = (res + (1 << (PRECISION_S32 - 1))) >> PRECISION_S32;
  *o = CLAMP (res, G_MININT32, G_MAXINT32);
}

static inline void
inner_product_gint32_cubic_1_sse41 (gint32 * o, const gint32 * a,
    const gint32 * b, gint len, const gint32 * icoeff, gint bstride)
{
  gint i = 0;
  gint64 res;
  __m128i sum[4], ta, tb;
  __m128i f = _mm_loadu_si128 ((__m128i *) icoeff);
  const gint32 *c[4] = { (gint32 *) ((gint8 *) b + 0 * bstride),
    (gint32 *) ((gint8 *) b + 1 * bstride),
    (gint32 *) ((gint8 *) b + 2 * bstride),
    (gint32 *) ((gint8 *) b + 3 * bstride)
  };

  sum[0] = sum[1] = sum[2] = sum[3] = _mm_setzero_si128 ();

  for (; i < len; i += 4) {
    ta = _mm_loadu_si128 ((__m128i *) (a + i));

    tb = _mm_load_si128 ((__m128i *) (c[0] + i));
    sum[0] = _mm_add_epi64 (sum[0], _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum[0] = _mm_add_epi64 (sum[0], _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));

    tb = _mm_load_si128 ((__m128i *) (c[1] + i));
    sum[1] = _mm_add_epi64 (sum[1], _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum[1] = _mm_add_epi64 (sum[1], _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));

    tb = _mm_load_si128 ((__m128i *) (c[2] + i));
    sum[2] = _mm_add_epi64 (sum[2], _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum[2] = _mm_add_epi64 (sum[2], _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));

    tb = _mm_load_si128 ((__m128i *) (c[3] + i));
    sum[3] = _mm_add_epi64 (sum[3], _mm_mul_epi32 (_mm_unpacklo_epi32 (ta, ta),
            _mm_unpacklo_epi32 (tb, tb)));
    sum[3] = _mm_add_epi64 (sum[3], _mm_mul_epi32 (_mm_unpackhi_epi32 (ta, ta),
            _mm_unpackhi_epi32 (tb, tb)));
  }
  sum[0] = _mm_srli_epi64 (sum[0], PRECISION_S32);
  sum[1] = _mm_srli_epi64 (sum[1], PRECISION_S32);
  sum[2] = _mm_srli_epi64 (sum[2], PRECISION_S32);
  sum[3] = _mm_srli_epi64 (sum[3], PRECISION_S32);
  sum[0] =
      _mm_mul_epi32 (sum[0], _mm_shuffle_epi32 (f, _MM_SHUFFLE (0, 0, 0, 0)));
  sum[1] =
      _mm_mul_epi32 (sum[1], _mm_shuffle_epi32 (f, _MM_SHUFFLE (1, 1, 1, 1)));
  sum[2] =
      _mm_mul_epi32 (sum[2], _mm_shuffle_epi32 (f, _MM_SHUFFLE (2, 2, 2, 2)));
  sum[3] =
      _mm_mul_epi32 (sum[3], _mm_shuffle_epi32 (f, _MM_SHUFFLE (3, 3, 3, 3)));
  sum[0] = _mm_add_epi64 (sum[0], sum[1]);
  sum[2] = _mm_add_epi64 (sum[2], sum[3]);
  sum[0] = _mm_add_epi64 (sum[0], sum[2]);
  sum[0] = _mm_add_epi64 (sum[0], _mm_unpackhi_epi64 (sum[0], sum[0]));
  res = _mm_cvtsi128_si64 (sum[0]);

  res = (res + (1 << (PRECISION_S32 - 1))) >> PRECISION_S32;
  *o = CLAMP (res, G_MININT32, G_MAXINT32);
}

MAKE_RESAMPLE_FUNC (gint32, full, 1, sse41);
MAKE_RESAMPLE_FUNC (gint32, linear, 1, sse41);
MAKE_RESAMPLE_FUNC (gint32, cubic, 1, sse41);

#endif
