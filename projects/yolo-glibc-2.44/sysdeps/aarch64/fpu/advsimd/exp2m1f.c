/* Single-precision (Advanced SIMD) exp2m1 function

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

#include "v_math.h"
#include "v_expf_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 0x1.fep+6 /* ≈ 127.5.  */

static const struct data
{
  struct v_expf_special_data special_data;
  float log2_lo, c2, c4, c6;
  uint32x4_t exponent_bias;
  float32x4_t log2_hi, c1, c3, c5;
  float32x4_t special_bound;
} data = {
  .special_data = V_EXPF_SPECIAL_DATA,
  /* Coefficients generated using remez's algorithm for exp2m1f(x).  */
  .log2_lo = -0x1.05c610p-29,
  .c2 = 0x1.c6b06ep-5,
  .c4 = 0x1.5da59ep-10,
  .c6 = 0x1.e081d6p-17,
  .exponent_bias = V4 (0x3f800000),
  .log2_hi = V4 (0x1.62e43p-1),
  .c1 = V4 (0x1.ebfbep-3),
  .c3 = V4 (0x1.3b2a5cp-7),
  .c5 = V4 (0x1.440dccp-13),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp2m1f's true overflow bound
     `log2(FLT_MAX) ≈ 128.0` or underflow bound `log2(FLT_TRUE_MIN) = -149.0`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V4 (SpecialBound),
};

/* Single-precision vector exp2(x) - 1 function.
   The maximum error is  1.76 + 0.5 ULP.
   _ZGVnN4v_exp2m1f (0x1.018af8p-1) got 0x1.ab2ebcp-2
				   want 0x1.ab2ecp-2.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (exp2m1) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = n + r, with r in [-1/2, 1/2].  */
  float32x4_t n = vrndaq_f32 (x);
  float32x4_t r = vsubq_f32 (x, n);

  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (n)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

  uint32x4_t cmp = vcageq_f32 (x, d->special_bound);

  /* Pairwise horner scheme.  */
  float32x4_t log2lo_c246 = vld1q_f32 (&d->log2_lo);
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p12 = vfmaq_laneq_f32 (d->c1, r, log2lo_c246, 1);
  float32x4_t p34 = vfmaq_laneq_f32 (d->c3, r, log2lo_c246, 2);
  float32x4_t p56 = vfmaq_laneq_f32 (d->c5, r, log2lo_c246, 3);
  float32x4_t p36 = vfmaq_f32 (p34, r2, p56);
  float32x4_t p16 = vfmaq_f32 (p12, r2, p36);
  float32x4_t poly
      = vfmaq_laneq_f32 (vmulq_f32 (d->log2_hi, r), r, log2lo_c246, 0);
  poly = vfmaq_f32 (poly, p16, r2);

  float32x4_t y = vfmaq_f32 (vsubq_f32 (scale, v_f32 (1.0f)), poly, scale);

  /* Fallback to special case for lanes with overflow.  */
  if (__glibc_unlikely (v_any_u32 (cmp)))
    {
      float32x4_t special
	  = (expf_special (poly, n, e, cmp, scale, &d->special_data));
      float32x4_t specialm1 = vsubq_f32 (special, v_f32 (1.0f));
      return vbslq_f32 (cmp, specialm1, y);
    }
  return y;
}
libmvec_hidden_def (V_NAME_F1 (exp2m1))
HALF_WIDTH_ALIAS_F1 (exp2m1)
