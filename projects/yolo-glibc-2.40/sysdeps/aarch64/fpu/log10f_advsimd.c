/* Single-precision vector (AdvSIMD) log10 function

   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
  uint32x4_t min_norm;
  uint16x8_t special_bound;
  float32x4_t poly[8];
  float32x4_t inv_ln10, ln2;
  uint32x4_t off, mantissa_mask;
} data = {
  /* Use order 9 for log10(1+x), i.e. order 8 for log10(1+x)/x, with x in
      [-1/3, 1/3] (offset=2/3). Max. relative error: 0x1.068ee468p-25.  */
  .poly = { V4 (-0x1.bcb79cp-3f), V4 (0x1.2879c8p-3f), V4 (-0x1.bcd472p-4f),
	    V4 (0x1.6408f8p-4f), V4 (-0x1.246f8p-4f), V4 (0x1.f0e514p-5f),
	    V4 (-0x1.0fc92cp-4f), V4 (0x1.f5f76ap-5f) },
  .ln2 = V4 (0x1.62e43p-1f),
  .inv_ln10 = V4 (0x1.bcb7b2p-2f),
  .min_norm = V4 (0x00800000),
  .special_bound = V8 (0x7f00), /* asuint32(inf) - min_norm.  */
  .off = V4 (0x3f2aaaab),	/* 0.666667.  */
  .mantissa_mask = V4 (0x007fffff),
};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, float32x4_t p, float32x4_t r2,
	      uint16x4_t cmp)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (log10f, x, vfmaq_f32 (y, p, r2), vmovl_u16 (cmp));
}

/* Fast implementation of AdvSIMD log10f,
   uses a similar approach as AdvSIMD logf with the same offset (i.e., 2/3) and
   an order 9 polynomial.
   Maximum error: 3.305ulps (nearest rounding.)
   _ZGVnN4v_log10f(0x1.555c16p+0) got 0x1.ffe2fap-4
				 want 0x1.ffe2f4p-4.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log10) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t u = vreinterpretq_u32_f32 (x);
  uint16x4_t special = vcge_u16 (vsubhn_u32 (u, d->min_norm),
				 vget_low_u16 (d->special_bound));

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = vsubq_u32 (u, d->off);
  float32x4_t n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u), 23)); /* signextend.  */
  u = vaddq_u32 (vandq_u32 (u, d->mantissa_mask), d->off);
  float32x4_t r = vsubq_f32 (vreinterpretq_f32_u32 (u), v_f32 (1.0f));

  /* y = log10(1+r) + n * log10(2).  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t poly = v_pw_horner_7_f32 (r, r2, d->poly);
  /* y = Log10(2) * n + poly * InvLn(10).  */
  float32x4_t y = vfmaq_f32 (r, d->ln2, n);
  y = vmulq_f32 (y, d->inv_ln10);

  if (__glibc_unlikely (v_any_u16h (special)))
    return special_case (x, y, poly, r2, special);
  return vfmaq_f32 (y, poly, r2);
}
libmvec_hidden_def (V_NAME_F1 (log10))
HALF_WIDTH_ALIAS_F1 (log10)
