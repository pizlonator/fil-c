/* Single-precision (Advanced SIMD) exp10m1 function

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
#define SpecialBound 0x1.330cf3ce9955ap+5 /* ≈ 38.3813.  */

static const struct data
{
  struct v_expf_special_data special_data;
  uint32x4_t exponent_bias;
  float log10_lo, c2, c4, c6;
  float log10_2_high, log10_2_low;
  float32x4_t log10_hi, c1, c3, c5, c7, c8;
  float32x4_t inv_log10_2, special_bound;
} data = {
  .special_data = V_EXPF_SPECIAL_DATA,
  /* Coefficients generated using Remez algorithm with minimisation of relative
  error.  */
  .inv_log10_2 = V4 (0x1.a934fp+1),
  .log10_2_high = 0x1.344136p-2,
  .log10_2_low = 0x1.ec10cp-27,
  .log10_hi = V4 (0x1.26bb1b8000000p+1),
  .log10_lo = 0x1.daaa8b0000000p-26,
  .c1 = V4 (0x1.53524ep1),
  .c2 = 0x1.046fc8p1,
  .c3 = V4 (0x1.2bd376p0),
  .c4 = 0x1.156f8p-1,
  .c5 = V4 (0x1.b28c0ep-3),
  .c6 = -0x1.05e38ep-4,
  .c7 = V4 (-0x1.c79f4ap-4),
  .c8 = V4 (0x1.2d6f34p1),
  .exponent_bias = V4 (0x3f800000),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp10m1f's
     true overflow bound `log10(FLT_MAX) ≈ 38.53` or
     underflow bound `log10(FLT_TRUE_MIN) ≈ -44.85`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V4 (SpecialBound),
};

/* Fast vector implementation of single-precision exp10m1.
   Algorithm is accurate to 1.70 + 0.5 ULP.
   _ZGVnN4v_exp10m1f(0x1.36f94cp-3) got 0x1.ac96acp-2
				   want 0x1.ac96bp-2.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (exp10m1) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* exp10(x) = 2^n * 10^r = 2^n * (1 + poly (r)),
     with 1 + poly(r) in [1/sqrt(2), sqrt(2)] and
     x = r + n * log10 (2), with r in [-log10(2)/2, log10(2)/2].  */
  float32x4_t n = vrndaq_f32 (vmulq_f32 (x, d->inv_log10_2));
  float32x4_t log10_2 = vld1q_f32 (&d->log10_2_high);
  float32x4_t r = vfmsq_laneq_f32 (x, n, log10_2, 0);
  r = vfmaq_laneq_f32 (r, n, log10_2, 1);

  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (n)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

  uint32x4_t cmp = vcageq_f32 (x, d->special_bound);

  /* Pairwise Horner scheme.  */
  float32x4_t log10lo_c246 = vld1q_f32 (&d->log10_lo);
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p78 = vfmaq_f32 (d->c7, r, d->c8);
  float32x4_t p56 = vfmaq_laneq_f32 (d->c5, r, log10lo_c246, 3);
  float32x4_t p34 = vfmaq_laneq_f32 (d->c3, r, log10lo_c246, 2);
  float32x4_t p12 = vfmaq_laneq_f32 (d->c1, r, log10lo_c246, 1);
  float32x4_t p58 = vfmaq_f32 (p56, r2, p78);
  float32x4_t p36 = vfmaq_f32 (p34, r2, p58);
  float32x4_t p16 = vfmaq_f32 (p12, r2, p36);

  float32x4_t poly
      = vfmaq_laneq_f32 (vmulq_f32 (d->log10_hi, r), r, log10lo_c246, 0);
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
libmvec_hidden_def (V_NAME_F1 (exp10m1))
HALF_WIDTH_ALIAS_F1 (exp10m1)
