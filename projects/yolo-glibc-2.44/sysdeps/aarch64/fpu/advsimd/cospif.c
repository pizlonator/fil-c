/* Single-precision (Advanced SIMD) cospi function

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
#include "poly_advsimd_f32.h"

static const struct data
{
  float32x4_t poly[6];
  float32x4_t range_val;
} data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { V4 (0x1.921fb6p1f), V4 (-0x1.4abbcep2f), V4 (0x1.466bc6p1f),
	    V4 (-0x1.32d2ccp-1f), V4 (0x1.50783p-4f), V4 (-0x1.e30750p-8f) },
  .range_val = V4 (0x1p31f),
};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t odd, uint32x4_t cmp)
{
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
  return v_call_f32 (cospif, x, y, cmp);
}

/* Approximation for vector single-precision cospi(x)
    Maximum Error: 3.17 ULP:
    _ZGVnN4v_cospif(0x1.d341a8p-5) got 0x1.f7cd56p-1
				  want 0x1.f7cd5p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (cospi) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t cmp = vcageq_f32 (x, d->range_val);
  uint32x4_t odd
      = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (x)), 31);

  /* r = x - rint(x).  */
  float32x4_t r = vsubq_f32 (x, vrndaq_f32 (x));

  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  r = vsubq_f32 (v_f32 (0.5f), vabsq_f32 (r));

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t r4 = vmulq_f32 (r2, r2);
  float32x4_t y = vmulq_f32 (v_pw_horner_5_f32 (r2, r4, d->poly), r);

  /* Fallback to scalar.  */
  if (__glibc_unlikely (v_any_u32 (cmp)))
    return special_case (x, y, odd, cmp);

  /* Reintroduce the sign bit for inputs which round to odd.  */
  return vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
}
libmvec_hidden_def (V_NAME_F1 (cospi))
HALF_WIDTH_ALIAS_F1 (cospi)
