/* Single-precision vector (AdvSIMD) exp10 function.

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
#define SpecialBound 0x1.30a46f1561911p+5 /* ≈ 38.09.  */

static const struct data
{
  struct v_expf_special_data special_data;
  float log10_2_high, log10_2_low, c2, c4;
  float32x4_t c0, c1, c3;
  float32x4_t inv_log10_2, special_bound;
  uint32x4_t exponent_bias;
} data = {
  .special_data = V_EXPF_SPECIAL_DATA,
  /* Coefficients generated using Remez algorithm with minimisation of relative
     error.
     rel error: 0x1.89dafa3p-24
     abs error: 0x1.167d55p-23 in [-log10(2)/2, log10(2)/2].  */
  .c0 = V4 (0x1.26bb16p+1f),
  .c1 = V4 (0x1.5350d2p+1f),
  .c2 = 0x1.04744ap+1f,
  .c3 = V4 (0x1.2d8176p+0f),
  .c4 = 0x1.12b41ap-1f,
  .inv_log10_2 = V4 (0x1.a934fp+1),
  .log10_2_high = 0x1.344136p-2,
  .log10_2_low = 0x1.ec10cp-27,
  .special_bound = V4 (SpecialBound),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp10f's
     true overflow bound `log10(FLT_MAX) ≈ 38.53` or
     underflow bound `log10(FLT_TRUE_MIN) ≈ -44.85`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .exponent_bias = V4 (0x3f800000),
};

/* Single-precision vector exp10f routine.
   The maximum error is 1.86 +0.5 ULP:
   _ZGVnN4v_exp10f(0x1.be2b36p+1) got 0x1.7e79c4p+11
				 want 0x1.7e79cp+11.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (exp10) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* exp10(x) = 2^n * 10^r = 2^n * (1 + poly (r)),
     with poly(r) in [1/sqrt(2), sqrt(2)] and
     x = r + n * log10 (2), with r in [-log10(2)/2, log10(2)/2].  */
  float32x4_t log10_2_c24 = vld1q_f32 (&d->log10_2_high);
  float32x4_t n = vrndaq_f32 (vmulq_f32 (x, d->inv_log10_2));
  float32x4_t r = vfmsq_laneq_f32 (x, n, log10_2_c24, 0);
  r = vfmaq_laneq_f32 (r, n, log10_2_c24, 1);
  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (n)), 23);

  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

  uint32x4_t cmp = vcageq_f32 (x, d->special_bound);

  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p12 = vfmaq_laneq_f32 (d->c1, r, log10_2_c24, 2);
  float32x4_t p34 = vfmaq_laneq_f32 (d->c3, r, log10_2_c24, 3);
  float32x4_t p14 = vfmaq_f32 (p12, r2, p34);
  float32x4_t poly = vfmaq_f32 (vmulq_f32 (r, d->c0), p14, r2);

  if (__glibc_unlikely (v_any_u32 (cmp)))
    return expf_special (poly, n, e, cmp, scale, &d->special_data);
  return vfmaq_f32 (scale, poly, scale);
}
libmvec_hidden_def (V_NAME_F1 (exp10))
HALF_WIDTH_ALIAS_F1 (exp10)
