/* Single-precision vector (SVE) log2p1 function

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
  float32_t c1, c3, c5, c7, c9, c11;
} data = {
  /* Polynomial generated using FPMinimax in [-0.25, 0.5].  */
  .c1 = 0x1.715476p0,	 .c2 = -0x1.71548p-1,	      .c3 = 0x1.ec718p-2,
  .c4 = -0x1.714fecp-2,	 .c5 = 0x1.27498ep-2,	      .c6 = -0x1.ecd864p-3,
  .c7 = 0x1.ace5b4p-3,	 .c8 = -0x1.7800fcp-3,	      .c9 = 0x1.226c92p-3,
  .c10 = -0x1.92cbb2p-4, .c11 = 0x1.624cb2p-4,	      .c12 = -0x1.bb0f1p-5,
  .four = 0x40800000,	 .three_quarters = 0x3f400000
};

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

/* Vector log2p1f approximation using polynomial on reduced interval.
   Worst-case error is 1.90 ULP:
   _ZGVsMxv_log2p1f(0x1.8789fcp-2) got 0x1.de58d4p-2
				  want 0x1.de58d8p-2.  */
svfloat32_t SV_NAME_F1 (log2p1) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* With x + 1 = t * 2^k (where t = m + 1 and k is chosen such that m
      is in [-0.25, 0.5]):
    log2p1(x) = log2(t) + log(2^k) = log2p1(m) + k.

    We approximate log2p1(m) with a polynomial, then scale by
    k. Instead of doing this directly, we use an intermediate
    scale factor s = 4*k to ensure the scale is representable
    as a normalised fp32 number.  */

  svfloat32_t m = svadd_x (svptrue_b32 (), x, 1);

  /* Choose k to scale x to the range [-1/4, 1/2].  */
  svint32_t k = svand_x (
      pg, svsub_x (svptrue_b32 (), svreinterpret_s32 (m), d->three_quarters),
      sv_s32 (0xff800000));

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number, and scale m down accordingly.  */
  svfloat32_t s = svreinterpret_f32 (svsubr_x (pg, k, d->four));

  /* Scale x by exponent manipulation.  */
  svfloat32_t m_scale = svreinterpret_f32_u32 (
      svsub_x (svptrue_b32 (), svreinterpret_u32 (x), svreinterpret_u32 (k)));
  m_scale = svadd_x (svptrue_b32 (), m_scale,
		     svmla_x (pg, sv_f32 (-1.0f), sv_f32 (0.25f), s));

  svfloat32_t scale_back = svmul_x (
      svptrue_b32 (), svcvt_f32_x (svptrue_b32 (), k), sv_f32 (0x1.0p-23f));
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

  /* Scaling factor m_scale is muld with c1 coeff, then added to p.  */
  svfloat32_t scaled_c1 = svmla_lane (scale_back, m_scale, c1357, 0);

  /* Special cases: x <= -1, x == inf, x == nan.  */
  svbool_t special_cases
      = svorn_z (pg, svcmple (svptrue_b32 (), x, sv_f32 (-1.0)),
		 svcmplt (pg, x, sv_f32 (INFINITY)));
  if (__glibc_unlikely (svptest_any (pg, special_cases)))
    return special_case (x, svmla_x (pg, scaled_c1, m2, p), pg, special_cases);
  return svmla_x (pg, scaled_c1, m2, p);
}
