/* Double-precision vector (Advanced SIMD) tanh function

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
  float64x2_t c2, c4, c6, c8;
  float64x2_t two_over_ln2;
  int64x2_t exponent_bias;
  double c1, c3, c5, c7, c9, c10;
  double ln2_hi_lo[2];
  float64x2_t special_bound;
} data = {
  .c1 = 0x1.5555555555559p-3,
  .c2 = V2 (0x1.555555555554bp-5),
  .c3 = 0x1.111111110f663p-7,
  .c4 = V2 (0x1.6c16c16c1b5f3p-10),
  .c5 = 0x1.a01a01affa35dp-13,
  .c6 = V2 (0x1.a01a018b4ecbbp-16),
  .c7 = 0x1.71ddf82db5bb4p-19,
  .c8 = V2 (0x1.27e517fc0d54bp-22),
  .c9 = 0x1.af5eedae67435p-26,
  .c10 = 0x1.1f143d060a28ap-29,
  .ln2_hi_lo = { 0x1.62e42fefa39efp-1, 0x1.abc9e3b39803fp-56 },
  .two_over_ln2 = V2 (0x1.71547652b82fep1),
  .exponent_bias = V2 (0x3ff0000000000000),
  /* Bound past which function returns signed 1 as the result.  */
  .special_bound = V2 (0x1.2cccccccccccdp+4), /* 18.80.  */
};

/* e^2x - 1 inline helper.  */
static inline float64x2_t
e2xm1_inline (float64x2_t x, const struct data *d)
{
  float64x2_t ln2_hi_lo = vld1q_f64 (&d->ln2_hi_lo[0]);

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float64x2_t n = vrndaq_f64 (vmulq_f64 (x, d->two_over_ln2));
  int64x2_t i = vcvtq_s64_f64 (n);
  float64x2_t f = vaddq_f64 (x, x);
  f = vfmsq_laneq_f64 (f, n, ln2_hi_lo, 0);
  f = vfmsq_laneq_f64 (f, n, ln2_hi_lo, 1);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t f4 = vmulq_f64 (f2, f2);
  float64x2_t lane_consts_13 = vld1q_f64 (&d->c1);
  float64x2_t lane_consts_57 = vld1q_f64 (&d->c5);
  float64x2_t lane_consts_910 = vld1q_f64 (&d->c9);
  float64x2_t p01 = vfmaq_laneq_f64 (v_f64 (0.5), f, lane_consts_13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, f, lane_consts_13, 1);
  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, f, lane_consts_57, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, f, lane_consts_57, 1);
  float64x2_t p03 = vfmaq_f64 (p01, f2, p23);
  float64x2_t p47 = vfmaq_f64 (p45, f2, p67);
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, f, lane_consts_910, 0);
  float64x2_t p = vfmaq_laneq_f64 (p89, f2, lane_consts_910, 1);
  p = vfmaq_f64 (p47, f4, p);
  p = vfmaq_f64 (p03, f4, p);

  p = vfmaq_f64 (f, f2, p);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  int64x2_t u = vaddq_s64 (vshlq_n_s64 (i, 52), d->exponent_bias);
  float64x2_t t = vreinterpretq_f64_s64 (u);

  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f64 (vsubq_f64 (t, v_f64 (1.0)), p, t);
}

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t q, uint64x2_t special)
{
  const struct data *d = ptr_barrier (&data);
  /* Complete fast path.  */
  float64x2_t y = vdivq_f64 (q, (vaddq_f64 (q, v_f64 (2.0))));

  uint64x2_t ix = vreinterpretq_u64_f64 (x);

  /* expm1 exponent bias is +1.0f.  */
  uint64x2_t one_bits = vreinterpretq_u64_s64 (d->exponent_bias);

  /* Mask selecting only the sign bit in each lane.  */
  uint64x2_t sign_mask = vdupq_n_u64 (0x8000000000000000ULL);

  /* Produce signed 1 for return of special cases:
    sign bit taken from ix
    all other bits taken from +1.0f (one_bits).  */
  uint64x2_t special_bits = vbslq_u64 (sign_mask, ix, one_bits);
  float64x2_t special_y = vreinterpretq_f64_u64 (special_bits);

  /* Select between special case or regular case and return value.  */
  return vbslq_f64 (special, special_y, y);
}

/* Vector approximation for double-precision tanh(x), using a simplified
   version of expm1. The greatest observed error is 2.70 ULP:
   _ZGVnN2v_tanh(-0x1.c59aa220cb177p-3) got -0x1.be5452a6459fep-3
				       want -0x1.be5452a6459fbp-3.  */
float64x2_t VPCS_ATTR V_NAME_D1 (tanh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  float64x2_t q = e2xm1_inline (x, d);

  /* Check for special cases.  */
  uint64x2_t special = vcagtq_f64 (x, d->special_bound);
  /* For sufficiently high inputs, the result of tanh(|x|) is 1 when correctly
     rounded, at this point we can return 1 directly, with sign correction.
     This will also act as a guard against our approximation overflowing.
     Kept as a special case to avoid slow down in fast path.  */
  if (__glibc_unlikely (v_any_u64 (special)))
    return special_case (x, q, special);

  /* Complete fast path if no special lanes.  */
  return vdivq_f64 (q, (vaddq_f64 (q, v_f64 (2.0))));
}
