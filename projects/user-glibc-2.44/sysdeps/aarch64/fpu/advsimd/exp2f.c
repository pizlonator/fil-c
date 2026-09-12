/* Single-precision vector (AdvSIMD) exp2 function

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
#include "v_expf_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 0x1.fap+6 /* = 126.50.  */

static const struct data
{
  struct v_expf_special_data special_data;
  float32x4_t c1, c3;
  uint32x4_t exponent_bias;
  float32x4_t special_bound;
  float c0, c2, c4, zero;
} data = {
  .special_data = V_EXPF_SPECIAL_DATA,
  .c0 = 0x1.59977ap-10f,
  .c1 = V4 (0x1.3ce9e4p-7f),
  .c2 = 0x1.c6bd32p-5f,
  .c3 = V4 (0x1.ebf9bcp-3f),
  .c4 = 0x1.62e422p-1f,
  .exponent_bias = V4 (0x3f800000),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp2f's true overflow bound
     `log2(FLT_MAX) ≈ 128.0` or underflow bound `log2(FLT_TRUE_MIN) = -149.0`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V4 (SpecialBound),
};

/* Single-precision vector exp2f routine.
   The maximum error is 1.47 +0.5 ULP:
   _ZGVnN4v_exp2f(0x1.7fdccep+0) got 0x1.69e764p+1
				want 0x1.69e768p+1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (exp2) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = n + r, with r in [-1/2, 1/2].  */
  float32x4_t n = vrndaq_f32 (x);
  float32x4_t r = vsubq_f32 (x, n);
  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (x)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

  uint32x4_t cmp = vcageq_f32 (x, d->special_bound);

  float32x4_t c024 = vld1q_f32 (&d->c0);
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p = vfmaq_laneq_f32 (d->c1, r, c024, 0);
  float32x4_t q = vfmaq_laneq_f32 (d->c3, r, c024, 1);
  q = vfmaq_f32 (q, p, r2);
  p = vmulq_laneq_f32 (r, c024, 2);
  float32x4_t poly = vfmaq_f32 (p, q, r2);

  if (__glibc_unlikely (v_any_u32 (cmp)))
    return expf_special (poly, n, e, cmp, scale, &d->special_data);

  return vfmaq_f32 (scale, poly, scale);
}
libmvec_hidden_def (V_NAME_F1 (exp2))
HALF_WIDTH_ALIAS_F1 (exp2)
