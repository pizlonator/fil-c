/* Single-precision vector (Advanced SIMD) erfc function

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
  uint32x4_t offset, table_scale;
  float32x4_t max, shift;
  float coeffs[4];
  float32x4_t third, two_over_five, tenth;
} data = {
  /* Set an offset so the range of the index used for lookup is 644, and it can
     be clamped using a saturated add.  */
  .offset = V4 (0xb7fffd7b),	       /* 0xffffffff - asuint(shift) - 644.  */
  .table_scale = V4 (0x28000000 << 1), /* asuint (2^-47) << 1.  */
  .max = V4 (10.0625f),		       /* 10 + 1/16 = 644/64.  */
  .shift = V4 (0x1p17f),
  /* Store 1/3, 2/3 and 2/15 in a single register for use with indexed muls and
     fmas.  */
  .coeffs = { 0x1.555556p-2f, 0x1.555556p-1f, 0x1.111112p-3f, 0 },
  .third = V4 (0x1.555556p-2f),
  .two_over_five = V4 (-0x1.99999ap-2f),
  .tenth = V4 (-0x1.99999ap-4f),
};

#define Off 0xfffffd7b	     /* 0xffffffff - 644.  */

struct entry
{
  float32x4_t erfc;
  float32x4_t scale;
};

static inline struct entry
lookup (uint32x4_t i)
{
  struct entry e;
  float32x2_t t0
      = vld1_f32 (&__v_erfcf_data.tab[vgetq_lane_u32 (i, 0) - Off].erfc);
  float32x2_t t1
      = vld1_f32 (&__v_erfcf_data.tab[vgetq_lane_u32 (i, 1) - Off].erfc);
  float32x2_t t2
      = vld1_f32 (&__v_erfcf_data.tab[vgetq_lane_u32 (i, 2) - Off].erfc);
  float32x2_t t3
      = vld1_f32 (&__v_erfcf_data.tab[vgetq_lane_u32 (i, 3) - Off].erfc);
  float32x4_t e1 = vcombine_f32 (t0, t1);
  float32x4_t e2 = vcombine_f32 (t2, t3);
  e.erfc = vuzp1q_f32 (e1, e2);
  e.scale = vuzp2q_f32 (e1, e2);
  return e;
}

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
   _ZGVnN4v_erfcf(0x1.1dbf7ap+3) got 0x1.f51212p-120
				want 0x1.f51216p-120.  */
VPCS_ATTR
float32x4_t NOINLINE V_NAME_F1 (erfc) (float32x4_t x)
{
  const struct data *dat = ptr_barrier (&data);

  float32x4_t a = vabsq_f32 (x);
  a = vminq_f32 (a, dat->max);

  /* Lookup erfc(r) and scale(r) in tables, e.g. set erfc(r) to 0 and scale to
     2/sqrt(pi), when x reduced to r = 0.  */
  float32x4_t shift = dat->shift;
  float32x4_t z = vaddq_f32 (a, shift);

  /* Clamp index to a range of 644. A naive approach would use a subtract and
     min. Instead we offset the table address and the index, then use a
     saturating add.  */
  uint32x4_t i = vqaddq_u32 (vreinterpretq_u32_f32 (z), dat->offset);

  struct entry e = lookup (i);

  /* erfc(x) ~ erfc(r) - scale * d * poly(r, d).  */
  float32x4_t r = vsubq_f32 (z, shift);
  float32x4_t d = vsubq_f32 (a, r);
  float32x4_t d2 = vmulq_f32 (d, d);
  float32x4_t r2 = vmulq_f32 (r, r);

  float32x4_t p1 = r;
  float32x4_t coeffs = vld1q_f32 (dat->coeffs);
  float32x4_t p2 = vfmsq_laneq_f32 (dat->third, r2, coeffs, 1);
  float32x4_t p3
      = vmulq_f32 (r, vfmaq_laneq_f32 (v_f32 (-0.5), r2, coeffs, 0));
  float32x4_t p4 = vfmaq_laneq_f32 (dat->two_over_five, r2, coeffs, 2);
  p4 = vfmsq_f32 (dat->tenth, r2, p4);

  float32x4_t y = vfmaq_f32 (p3, d, p4);
  y = vfmaq_f32 (p2, d, y);
  y = vfmaq_f32 (p1, d, y);
  y = vfmsq_f32 (e.erfc, e.scale, vfmsq_f32 (d, d2, y));

  /* Offset equals 2.0f if sign, else 0.0f.  */
  uint32x4_t sign = vshrq_n_u32 (vreinterpretq_u32_f32 (x), 31);
  float32x4_t off = vreinterpretq_f32_u32 (vshlq_n_u32 (sign, 30));
  /* Copy sign and scale back in a single fma. Since the bit patterns do not
     overlap, then logical or and addition are equivalent here.  */
  float32x4_t fac = vreinterpretq_f32_u32 (
      vsraq_n_u32 (vshlq_n_u32 (sign, 31), dat->table_scale, 1));

  return vfmaq_f32 (off, fac, y);
}
libmvec_hidden_def (V_NAME_F1 (erfc))
HALF_WIDTH_ALIAS_F1 (erfc)
