/* Double-precision vector (AdvSIMD) exp10 function.

   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include "v_exp_special_case_inline.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 0x1.33a6fa2f05a71p+8 /* log10(2^1022) ~ 307.66.  */

/* Value of n above which scale overflows even with special treatment.  */
#define ScaleBound 163840.0 /* 1280.0 * N.  */

const static struct data
{
  struct v_exp_special_data special_data;
  double c1, c3;
  double log2_10_hi, log2_10_lo;
  float64x2_t c0, c2, log10_2, shift;
  float64x2_t special_bound, scale_thresh;
} data = {
  .special_data = V_EXP_SPECIAL_DATA,
  /* Coefficients generated using Remez algorithm.
     rel error: 0x1.5ddf8f28p-54
     abs error: 0x1.5ed266c8p-54 in [ -log10(2)/256, log10(2)/256 ].  */
  .c0 = V2 (0x1.26bb1bbb5524p1),
  .c1 = 0x1.53524c73cecdap1,
  .c2 = V2 (0x1.047060efb781cp1),
  .c3 = 0x1.2bd76040f0d16p0,
  .log10_2 = V2 (0x1.a934f0979a371p8), /* N/log2(10).  */
  .log2_10_hi = 0x1.34413509f79ffp-9,  /* log2(10)/N.  */
  .log2_10_lo = -0x1.9dc1da994fd21p-66,
  .shift = V2 (0x1.8p+52),
  .scale_thresh = V2 (ScaleBound),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp10's overflow bound
     `log10(DBL_MAX) ≈ 308.25` or underflow bound
     `log10(DBL_TRUE_MIN) ≈ -323.31`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V2 (SpecialBound),
};

#define N (1 << V_EXP_TABLE_BITS)
#define IndexMask (N - 1)

static inline uint64x2_t
lookup_sbits (uint64x2_t i)
{
  return (uint64x2_t){ __v_exp_data[i[0] & IndexMask],
		       __v_exp_data[i[1] & IndexMask] };
}

/* Fast vector implementation of exp10.
   Maximum measured error is 1.64 ulp.
   _ZGVnN2v_exp10(0x1.ccd1c9d82cc8cp+0) got 0x1.f8dab6d7fed0cp+5
				       want 0x1.f8dab6d7fed0ap+5.  */
float64x2_t VPCS_ATTR V_NAME_D1 (exp10) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t cmp = vcagtq_f64 (x, d->special_bound);

  /* n = round(x/(log10(2)/N)).  */
  float64x2_t z = vfmaq_f64 (d->shift, x, d->log10_2);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* r = x - n*log10(2)/N.  */
  float64x2_t log2_10_hl = vld1q_f64 (&d->log2_10_hi);
  float64x2_t r = x;
  r = vfmsq_laneq_f64 (r, n, log2_10_hl, 0);
  r = vfmsq_laneq_f64 (r, n, log2_10_hl, 1);

  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TABLE_BITS);

  /* poly = exp10(r) - 1 ~= C0 r + C1 r^2 + C2 r^3 + C3 r^4.  */
  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t p = vfmaq_laneq_f64 (d->c0, r, c13, 0);
  float64x2_t poly = vfmaq_laneq_f64 (d->c2, r, c13, 1);
  float64x2_t r2 = vmulq_f64 (r, r);
  p = vfmaq_f64 (p, poly, r2);
  poly = vmulq_f64 (r, p);

  /* scale = 2^(n/N).  */
  u = lookup_sbits (u);
  float64x2_t scale = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

  if (__glibc_unlikely (v_any_u64 (cmp)))
    return exp_special (poly, n, scale, d->scale_thresh, &d->special_data);

  return vfmaq_f64 (scale, poly, scale);
}
