/* Double-precision vector (AdvSIMD) exp2 function

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
#include "poly_advsimd_f64.h"
#include "v_exp_special_case_inline.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 0x1.ffp+9 /* log2(2^1022) = 1022.00.  */

/* Value of n above which scale overflows even with special treatment.  */
#define ScaleBound 0x1.4p+10 /* 1280.0.  */

#define N (1 << V_EXP_TABLE_BITS)
#define IndexMask (N - 1)

static const struct data
{
  struct v_exp_special_data special_data;
  float64x2_t poly[4];
  float64x2_t shift, special_bound, scale_thresh;
} data = {
  .special_data = V_EXP_SPECIAL_DATA,
  /* Coefficients are computed using Remez algorithm with
     minimisation of the absolute error.  */
  .poly = { V2 (0x1.62e42fefa3686p-1), V2 (0x1.ebfbdff82c241p-3),
	    V2 (0x1.c6b09b16de99ap-5), V2 (0x1.3b2abf5571ad8p-7) },
  .shift = V2 (0x1.8p52 / N),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp2's overflow bound
     `log2(DBL_MAX) ≈ 1024.0` or underflow bound
     `log2(DBL_TRUE_MIN) = -1074.0`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V2 (SpecialBound),
  .scale_thresh = V2 (ScaleBound),
};

static inline uint64x2_t
lookup_sbits (uint64x2_t i)
{
  return (uint64x2_t){ __v_exp_data[i[0] & IndexMask],
		       __v_exp_data[i[1] & IndexMask] };
}

/* Fast vector implementation of exp2.
   Maximum measured error is 1.65 ulp.
   _ZGVnN2v_exp2(-0x1.4c264ab5b559bp-6) got 0x1.f8db0d4df721fp-1
				       want 0x1.f8db0d4df721dp-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (exp2) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t cmp = vcagtq_f64 (x, d->special_bound);

  /* n = round(x/N).  */
  float64x2_t z = vaddq_f64 (d->shift, x);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* r = x - n/N.  */
  float64x2_t r = vsubq_f64 (x, n);

  /* scale = 2^(n/N).  */
  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TABLE_BITS);
  u = lookup_sbits (u);
  float64x2_t scale = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

  /* poly ~ exp2(r) - 1.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t poly = v_pairwise_poly_3_f64 (r, r2, d->poly);
  poly = vmulq_f64 (r, poly);

  if (__glibc_unlikely (v_any_u64 (cmp)))
    return exp_special (poly, n, scale, d->scale_thresh, &d->special_data);
  return vfmaq_f64 (scale, scale, poly);
}
