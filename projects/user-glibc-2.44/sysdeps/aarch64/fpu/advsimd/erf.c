/* Double-precision vector (Advanced SIMD) erf function

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

#include "v_math.h"

static const struct data
{
  float64x2_t third;
  float64x2_t tenth, two_over_five, two_over_nine;
  double two_over_fifteen;
  float64x2_t max, shift;
  uint64x2_t max_idx;
} data = {
  .max_idx = V2 (768),
  .third = V2 (0x1.5555555555556p-2), /* used to compute 2/3 and 1/6 too.  */
  .two_over_fifteen = 0x1.1111111111111p-3,
  .tenth = V2 (-0x1.999999999999ap-4),
  .two_over_five = V2 (-0x1.999999999999ap-2),
  .two_over_nine = V2 (-0x1.c71c71c71c71cp-3),
  .max = V2 (5.9921875), /* 6 - 1/128.  */
  .shift = V2 (0x1p45),
};

#define AbsMask 0x7fffffffffffffff

struct entry
{
  float64x2_t erf;
  float64x2_t scale;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  float64x2_t e1 = vld1q_f64 (&__v_erf_data.tab[vgetq_lane_u64 (i, 0)].erf),
	      e2 = vld1q_f64 (&__v_erf_data.tab[vgetq_lane_u64 (i, 1)].erf);
  e.erf = vuzp1q_f64 (e1, e2);
  e.scale = vuzp2q_f64 (e1, e2);
  return e;
}

/* Double-precision implementation of vector erf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r) + scale * d * [
       + 1
       - r d
       + 1/3 (2 r^2 - 1) d^2
       - 1/6 (r (2 r^2 - 3)) d^3
       + 1/30 (4 r^4 - 12 r^2 + 3) d^4
       - 1/90 (4 r^4 - 20 r^2 + 15) d^5
     ]

   Maximum measure error: 2.29 ULP
   V_NAME_D1 (erf)(-0x1.00003c924e5d1p-8) got -0x1.20dd59132ebadp-8
					 want -0x1.20dd59132ebafp-8.  */
float64x2_t VPCS_ATTR V_NAME_D1 (erf) (float64x2_t x)
{
  const struct data *dat = ptr_barrier (&data);

  float64x2_t a = vabsq_f64 (x);
  /* Reciprocal conditions that do not catch NaNs so they can be used in BSLs
     to return expected results.  */
  uint64x2_t a_le_max = vcaleq_f64 (x, dat->max);
  uint64x2_t a_gt_max = vcagtq_f64 (x, dat->max);

  /* Set r to multiple of 1/128 nearest to |x|.  */
  float64x2_t shift = dat->shift;
  float64x2_t z = vaddq_f64 (a, shift);

  /* Lookup erf(r) and scale(r) in table, without shortcut for small values,
     but with saturated indices for large values and NaNs in order to avoid
     segfault.  */
  uint64x2_t i
      = vsubq_u64 (vreinterpretq_u64_f64 (z), vreinterpretq_u64_f64 (shift));
  i = vbslq_u64 (a_le_max, i, dat->max_idx);
  struct entry e = lookup (i);

  float64x2_t r = vsubq_f64 (z, shift);

  /* erf(x) ~ erf(r) + scale * d * poly (r, d).  */
  float64x2_t d = vsubq_f64 (a, r);
  float64x2_t d2 = vmulq_f64 (d, d);
  float64x2_t r2 = vmulq_f64 (r, r);

  float64x2_t two_over_fifteen_and_fortyfive
      = vld1q_f64 (&dat->two_over_fifteen);

  /* poly (d, r) = 1 + p1(r) * d + p2(r) * d^2 + ... + p5(r) * d^5.  */
  float64x2_t p1 = r;
  float64x2_t p2
      = vfmsq_f64 (dat->third, r2, vaddq_f64 (dat->third, dat->third));
  float64x2_t p3 = vmulq_f64 (r, vfmaq_f64 (v_f64 (-0.5), r2, dat->third));
  float64x2_t p4 = vfmaq_laneq_f64 (dat->two_over_five, r2,
				    two_over_fifteen_and_fortyfive, 0);
  p4 = vfmsq_f64 (dat->tenth, r2, p4);
  float64x2_t p5 = vfmaq_laneq_f64 (dat->two_over_nine, r2,
				    two_over_fifteen_and_fortyfive, 1);
  p5 = vmulq_f64 (r, vfmaq_f64 (vmulq_f64 (v_f64 (0.5), dat->third), r2, p5));

  float64x2_t p34 = vfmaq_f64 (p3, d, p4);
  float64x2_t p12 = vfmaq_f64 (p1, d, p2);
  float64x2_t y = vfmaq_f64 (p34, d2, p5);
  y = vfmaq_f64 (p12, d2, y);

  y = vfmaq_f64 (e.erf, e.scale, vfmsq_f64 (d, d2, y));

  /* Solves the |x| = inf and NaN cases.  */
  y = vbslq_f64 (a_gt_max, v_f64 (1.0), y);

  /* Copy sign.  */
  return vbslq_f64 (v_u64 (AbsMask), y, x);
}
