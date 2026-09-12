/* Double-precision vector (AdvSIMD) cosh function

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
  double c2, inv_ln2;
  float64x2_t c0, c1;
  float64x2_t shift, inf_bound, cosh_9, special_bound;
  uint64x2_t index_mask;
  double ln2_hi_lo[2];
} data = {
  .c0 = V2 (0x1.fffffffffffd4p-2),
  .c1 = V2 (0x1.5555571d6b68cp-3),
  .c2 = 0x1.5555576a59599p-5,
  .inv_ln2 = 0x1.71547652b82fep8, /* N/ln2.  */
  /* -ln2/N.  */
  .ln2_hi_lo = { -0x1.62e42fefa39efp-9, -0x1.abc9e3b39803f3p-64 },
  .shift = V2 (0x1.8p+52),
  .index_mask = V2 (0xff),
  /* ln(2^1023). expm1 helper overflows for large input.  */
  .special_bound = V2 (0x1.6232e147ae148p+9), /* 708.40.  */
  /* Bound past which function returns inf.  */
  .inf_bound = V2 (0x1.634p+9), /* 710.5.  */
  /* Cosh(9) slightly shifted for accuracy.  */
  .cosh_9 = V2 (0x1.fa7157c470f82p+11),
};

/* Helper for approximating exp(x). Copied from v_exp_tail, with no
   special-case handling or tail.  */
static inline float64x2_t
exp_inline (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t c2_inv_ln2 = vld1q_f64 (&d->c2);
  /* n = round(x/(ln2/N)).  */
  float64x2_t z = vfmaq_laneq_f64 (d->shift, x, c2_inv_ln2, 1);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* r = x - n*ln2/N.  */
  float64x2_t ln2_hi_lo = vld1q_f64 (d->ln2_hi_lo);
  float64x2_t r = vfmaq_laneq_f64 (x, n, ln2_hi_lo, 0);
  r = vfmaq_laneq_f64 (r, n, ln2_hi_lo, 1);

  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TAIL_TABLE_BITS);
  uint64x2_t i = vandq_u64 (u, d->index_mask);

  /* y = tail + exp(r) - 1 ~= r + C1 r^2 + C2 r^3 + C3 r^4.  */
  float64x2_t poly = vfmaq_laneq_f64 (d->c1, r, c2_inv_ln2, 0);
  poly = vfmaq_f64 (d->c0, poly, r);
  poly = vmulq_f64 (vfmaq_f64 (v_f64 (1), poly, r), r);

  /* s = 2^(n/N).  */
  u = v_lookup_u64 (__v_exp_tail_data, i);
  float64x2_t scale = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

  return vfmaq_f64 (scale, poly, scale);
}

/* Uses the compound angle formula to adjust x back into an approximable range:
   cosh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding cosh == sinh,
   this can be simplified into: cosh (A + B) = cosh(A) * e^B.  */
static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t t, uint64x2_t special)
{
  const struct data *d = ptr_barrier (&data);

  /* Complete fast path computation.  */
  float64x2_t half_t = vmulq_n_f64 (t, 0.5);
  float64x2_t half_over_t = vdivq_f64 (v_f64 (0.5), t);
  float64x2_t y = vaddq_f64 (half_t, half_over_t);

  /* Absolute x so we can subtract 9.0.  */
  float64x2_t ax = vabsq_f64 (x);
  /* Subtract 9.0 from x as a reduction to prevent early overflow.  */
  float64x2_t sx = vsubq_f64 (ax, v_f64 (9.0));
  float64x2_t s = exp_inline (sx);

  /* Multiply the result by cosh(9) slightly shifted for accuracy.  */
  float64x2_t r = vmulq_f64 (s, d->cosh_9);

  /* Check for overflowing lanes and return inf.  */
  uint64x2_t cmp = vcagtq_f64 (ax, d->inf_bound);

  /* Set overflowing lines to inf and set none over flowing to result.  */
  r = vbslq_f64 (cmp, v_f64 (INFINITY), r);

  /* Return r for special lanes and y for none special lanes.  */
  return vbslq_f64 (special, r, y);
}

/* Approximation for vector double-precision cosh(x) using exp_inline.
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The greatest observed error in the non-special region is 2.12 + 0.5 ULP:
   _ZGVnN2v_cosh (-0x1.6241387f982f3p+1) got 0x1.ff784e05bad75p+2
					want 0x1.ff784e05bad72p+2.  */
float64x2_t VPCS_ATTR V_NAME_D1 (cosh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* Up to the point that exp overflows, we can use it to calculate cosh by
     exp(|x|) / 2 + 1 / (2 * exp(|x|)).  */
  float64x2_t t = exp_inline (x);

  /* Check for special cases.  */
  uint64x2_t special = vcagtq_f64 (x, d->special_bound);
  /* Fall back to vectorised special case for any lanes which would cause
     exp to overflow.  */
  if (__glibc_unlikely (v_any_u64 (special)))
    return special_case (x, t, special);

  /* Complete fast path if no special lanes.  */
  float64x2_t half_t = vmulq_n_f64 (t, 0.5);
  float64x2_t half_over_t = vdivq_f64 (v_f64 (0.5), t);
  return vaddq_f64 (half_t, half_over_t);
}
