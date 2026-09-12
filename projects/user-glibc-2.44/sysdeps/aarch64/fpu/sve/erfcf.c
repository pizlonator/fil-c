/* Single-precision vector (SVE) erfc function

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
  uint32_t off_idx, off_arr;
  float max, shift;
  float third, two_thirds, two_over_fifteen, two_over_five, tenth;
} data = {
  /* Set an offset so the range of the index used for lookup is 644, and it can
     be clamped using a saturated add.  */
  .off_idx = 0xb7fffd7b, /* 0xffffffff - asuint(shift) - 644.  */
  .off_arr = 0xfffffd7b, /* 0xffffffff - 644.  */
  .max = 10.0625f,	 /* 644/64.  */
  .shift = 0x1p17f,
  .third = 0x1.555556p-2f,
  .two_thirds = 0x1.555556p-1f,
  .two_over_fifteen = 0x1.111112p-3f,
  .two_over_five = -0x1.99999ap-2f,
  .tenth = -0x1.99999ap-4f,
};

#define SignMask 0x80000000
#define TableScale 0x28000000 /* 0x1p-47.  */

/* Optimized single-precision vector erfcf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/64.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4

   Values of erfc(r) and scale are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).

   Maximum error: 1.63 ULP (~1.0 ULP for x < 0.0).
   _ZGVsMxv_erfcf(0x1.1dbf7ap+3) got 0x1.f51212p-120
				want 0x1.f51216p-120.  */
svfloat32_t SV_NAME_F1 (erfc) (svfloat32_t x, const svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);

  svfloat32_t a = svabs_x (pg, x);

  /* Clamp input at |x| <= 10.0 + 4/64.  */
  a = svmin_x (pg, a, dat->max);

  /* Reduce x to the nearest multiple of 1/64.  */
  svfloat32_t shift = sv_f32 (dat->shift);
  svfloat32_t z = svadd_x (pg, a, shift);

  /* Saturate index for the NaN case.  */
  svuint32_t i = svqadd (svreinterpret_u32 (z), dat->off_idx);

  /* Lookup erfc(r) and 2/sqrt(pi)*exp(-r^2) in tables.  */
  i = svlsl_x (svptrue_b32 (), i, 1);
  const float32_t *p = &__v_erfcf_data.tab[0].erfc - 2 * dat->off_arr;
  svfloat32_t erfcr = svld1_gather_index (pg, p, i);
  svfloat32_t scale = svld1_gather_index (pg, p + 1, i);

  /* erfc(x) ~ erfc(r) - scale * d * poly(r, d).  */
  svfloat32_t r = svsub_x (pg, z, shift);
  svfloat32_t d = svsub_x (pg, a, r);
  svfloat32_t d2 = svmul_x (svptrue_b32 (), d, d);
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);

  svfloat32_t coeffs = svld1rq (svptrue_b32 (), &dat->third);

  svfloat32_t p1 = r;
  svfloat32_t p2 = svmls_lane (sv_f32 (dat->third), r2, coeffs, 1);
  svfloat32_t p3
      = svmul_x (svptrue_b32 (), r, svmla_lane (sv_f32 (-0.5), r2, coeffs, 0));
  svfloat32_t p4 = svmla_lane (sv_f32 (dat->two_over_five), r2, coeffs, 2);
  p4 = svmls_x (pg, sv_f32 (dat->tenth), r2, p4);

  svfloat32_t y = svmla_x (pg, p3, d, p4);
  y = svmla_x (pg, p2, d, y);
  y = svmla_x (pg, p1, d, y);

  /* Solves the |x| = inf/nan case.  */
  y = svmls_x (pg, erfcr, scale, svmls_x (pg, d, d2, y));

  /* Offset equals 2.0f if sign, else 0.0f.  */
  svuint32_t sign = svand_x (pg, svreinterpret_u32 (x), SignMask);
  svfloat32_t off = svreinterpret_f32 (svlsr_x (pg, sign, 1));
  /* Handle sign and scale back in a single fma.  */
  svfloat32_t fac = svreinterpret_f32 (svorr_x (pg, sign, TableScale));

  return svmla_x (pg, off, fac, y);
}
