/* Single-precision vector (Advanced SIMD) erf function

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
  float32x4_t max, shift, third;
} data = {
  .max = V4 (3.9375), /* 4 - 8/128.  */
  .shift = V4 (0x1p16f),
  .third = V4 (0x1.555556p-2f), /* 1/3.  */
};

#define AbsMask 0x7fffffff

struct entry
{
  float32x4_t erf;
  float32x4_t scale;
};

static inline struct entry
lookup (uint32x4_t i)
{
  struct entry e;
  float32x2_t t0 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 0)].erf);
  float32x2_t t1 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 1)].erf);
  float32x2_t t2 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 2)].erf);
  float32x2_t t3 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 3)].erf);
  float32x4_t e1 = vcombine_f32 (t0, t1);
  float32x4_t e2 = vcombine_f32 (t2, t3);
  e.erf = vuzp1q_f32 (e1, e2);
  e.scale = vuzp2q_f32 (e1, e2);
  return e;
}

/* Single-precision implementation of vector erf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r) + scale * d * [1 - r * d - 1/3 * d^2]

   Values of erf(r) and scale are read from lookup tables.
   For |x| > 3.9375, erf(|x|) rounds to 1.0f.

   Maximum error: 1.93 ULP
     _ZGVnN4v_erff(0x1.c373e6p-9) got 0x1.fd686cp-9
				 want 0x1.fd6868p-9.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (erf) (float32x4_t x)
{
  const struct data *dat = ptr_barrier (&data);

  float32x4_t a = vabsq_f32 (x);
  uint32x4_t a_gt_max = vcgtq_f32 (a, dat->max);

  /* Lookup erf(r) and scale(r) in tables, e.g. set erf(r) to 0 and scale to
     2/sqrt(pi), when x reduced to r = 0.  */
  float32x4_t shift = dat->shift;
  float32x4_t z = vaddq_f32 (a, shift);

  uint32x4_t i
      = vsubq_u32 (vreinterpretq_u32_f32 (z), vreinterpretq_u32_f32 (shift));
  i = vminq_u32 (i, v_u32 (512));
  struct entry e = lookup (i);

  float32x4_t r = vsubq_f32 (z, shift);

  /* erf(x) ~ erf(r) + scale * d * (1 - r * d - 1/3 * d^2).  */
  float32x4_t d = vsubq_f32 (a, r);
  float32x4_t d2 = vmulq_f32 (d, d);
  float32x4_t y = vfmaq_f32 (r, dat->third, d);
  y = vfmaq_f32 (e.erf, e.scale, vfmsq_f32 (d, d2, y));

  /* Solves the |x| = inf case.  */
  y = vbslq_f32 (a_gt_max, v_f32 (1.0f), y);

  /* Copy sign.  */
  return vbslq_f32 (v_u32 (AbsMask), y, x);
}
libmvec_hidden_def (V_NAME_F1 (erf))
HALF_WIDTH_ALIAS_F1 (erf)
