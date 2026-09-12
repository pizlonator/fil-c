/* Single-precision vector log10p1 function

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include "sv_math.h"

static const struct data
{
  uint32_t four;
  int32_t three_quarters;
  float32_t c2, c4, c6, c8, c10, c12;
  float32_t c1, c3, c5, c7, c9, c11, small_log10_2;
} data = {
  /* Polynomial generated using FPMinimax in [-0.25, 0.5].  */
  .c1 = 0x1.bcb7b2p-2,
  .c2 = -0x1.bcb79ep-3,
  .c3 = 0x1.287984p-3,
  .c4 = -0x1.bcc0d4p-4,
  .c5 = 0x1.642986p-4,
  .c6 = -0x1.2815bcp-4,
  .c7 = 0x1.ec8a4p-5,
  .c8 = -0x1.ac4418p-5,
  .c9 = 0x1.f155a8p-5,
  .c10 = -0x1.34b422p-4,
  .c11 = 0x1.bc5418p-5,
  .c12 = -0x1.911e6ep-7,
  .four = 0x40800000,
  .three_quarters = 0x3f400000,
  /* 2^-23 * log10(2) used for scaling.  */
  .small_log10_2 = 0x1.344136p-25f,
};

#define SignedExpMask sv_s32 (0xff800000)

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, const svbool_t pg,
	      svbool_t special)
{
  y = svsel (special, sv_f32 (NAN), y);
  svbool_t ret_pinf = svcmpeq_f32 (pg, x, sv_f32 (INFINITY));
  svbool_t ret_minf = svcmpeq_f32 (pg, x, sv_f32 (-1.0));
  y = svsel (ret_pinf, sv_f32 (INFINITY), y);
  return svsel (ret_minf, sv_f32 (-INFINITY), y);
}

/* Vector log10p1f approximation using polynomial on reduced interval.
   Worst-case error is 3.40 ULP:
   _ZGVsMxv_log10p1f(0x1.8bfff6p+6) got 0x1.000002p+1
				   want 0x1.fffffep+0.  */
svfloat32_t SV_NAME_F1 (log10p1) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* With x + 1 = t * 2^k (where t = m + 1 and k is chosen such that m
      is in [-0.25, 0.5]):
    log10p1(x) = log10(t) + log(2^k) = log10p1(m) + k * log10(2).

    We approximate log10p1(m) with a polynomial, then scale by
    k. Instead of doing this directly, we use an intermediate
    scale factor s = 4*k to ensure the scale is representable
    as a normalised fp32 number.  */

  svfloat32_t m = svsub_x (svptrue_b32 (), x, -1.0f);

  /* Choose k to scale x to the range [-1/4, 1/2].  */
  svint32_t k = svand_x (
      pg, svsub_x (svptrue_b32 (), svreinterpret_s32 (m), d->three_quarters),
      SignedExpMask);

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number, and scale m down accordingly.  */
  svfloat32_t s = svreinterpret_f32 (svsubr_x (pg, k, d->four));

  /* Scale x by exponent manipulation.  */
  svfloat32_t m_scale = svreinterpret_f32_u32 (
      svsub_x (svptrue_b32 (), svreinterpret_u32 (x), svreinterpret_u32 (k)));
  m_scale = svadd_x (svptrue_b32 (), m_scale,
		     svmla_x (pg, sv_f32 (-1.0f), sv_f32 (0.25f), s));

  svfloat32_t scale_back
      = svmul_x (svptrue_b32 (), svcvt_f32_x (svptrue_b32 (), k),
		 sv_f32 (d->small_log10_2));
  svfloat32_t m2 = svmul_x (svptrue_b32 (), m_scale, m_scale);

  /* Order-12 pairwise Horner.  */
  svfloat32_t c1357 = svld1rq (svptrue_b32 (), &d->c1);
  svfloat32_t c911 = svld1rq (svptrue_b32 (), &d->c9);
  svfloat32_t p23 = svmla_lane (sv_f32 (d->c2), m_scale, c1357, 1);
  svfloat32_t p45 = svmla_lane (sv_f32 (d->c4), m_scale, c1357, 2);
  svfloat32_t p67 = svmla_lane (sv_f32 (d->c6), m_scale, c1357, 3);
  svfloat32_t p89 = svmla_lane (sv_f32 (d->c8), m_scale, c911, 0);
  svfloat32_t p1011 = svmla_lane (sv_f32 (d->c10), m_scale, c911, 1);

  svfloat32_t p = svmla_x (pg, p1011, m2, d->c12);
  p = svmla_x (pg, p89, m2, p);
  p = svmla_x (pg, p67, m2, p);
  p = svmla_x (pg, p45, m2, p);
  p = svmla_x (pg, p23, m2, p);

  /* Scaling factor m_scale is multiplied with c1 coeff, then added to p.  */
  svfloat32_t scaled_c1 = svmla_lane (scale_back, m_scale, c1357, 0);

  /* Special cases: x <= -1, x == inf, x == nan.  */
  svbool_t special_cases
      = svorn_z (pg, svcmple (svptrue_b32 (), x, sv_f32 (-1.0)),
		 svcmplt (pg, x, sv_f32 (INFINITY)));
  if (__glibc_unlikely (svptest_any (pg, special_cases)))
    return special_case (x, svmla_x (pg, scaled_c1, m2, p), pg, special_cases);
  return svmla_x (pg, scaled_c1, m2, p);
}
