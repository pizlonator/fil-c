/* Single-precision vector (AdvSIMD) log10 function

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

static const struct data
{
  float32x4_t c0, c2, c4, c6, inv_ln10, ln2;
  uint32x4_t off, offset_lower_bound;
  uint32x4_t special_bound;
  uint16x8_t special_bound_u16;
  uint32x4_t mantissa_mask;
  float c1, c3, c5, c7;
  float32x4_t pinf, minf, nan;
} data = {
  /* Use order 9 for log10(1+x), i.e. order 8 for log10(1+x)/x, with x in
      [-1/3, 1/3] (offset=2/3). Max. relative error: 0x1.068ee468p-25.  */
  .c0 = V4 (-0x1.bcb79cp-3f),
  .c1 = 0x1.2879c8p-3f,
  .c2 = V4 (-0x1.bcd472p-4f),
  .c3 = 0x1.6408f8p-4f,
  .c4 = V4 (-0x1.246f8p-4f),
  .c5 = 0x1.f0e514p-5f,
  .c6 = V4 (-0x1.0fc92cp-4f),
  .c7 = 0x1.f5f76ap-5f,
  .ln2 = V4 (0x1.62e43p-1f),
  .inv_ln10 = V4 (0x1.bcb7b2p-2f),
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .off = V4 (0x3f2aaaab), /* 0.666667.  */
  .offset_lower_bound = V4 (0x00800000 - 0x3f2aaaab),
  .special_bound = V4 (0x7f000000), /* asuint32(inf) - 0x00800000.  */
  .special_bound_u16 = V8 (0x7f00),
  .mantissa_mask = V4 (0x007fffff),
  .pinf = V4 (INFINITY),
  .minf = V4 (-INFINITY),
  .nan = V4 (NAN),
};

static inline float32x4_t VPCS_ATTR
inline_log10f (uint32x4_t u_off, float32x4_t n, const struct data *d)
{
  float32x4_t c1357 = vld1q_f32 (&d->c1);

  uint32x4_t u = vaddq_u32 (vandq_u32 (u_off, d->mantissa_mask), d->off);
  float32x4_t r = vsubq_f32 (vreinterpretq_f32_u32 (u), v_f32 (1.0f));

  /* y = log10(1+r) + n * log10(2).  */
  float32x4_t r2 = vmulq_f32 (r, r);

  float32x4_t c01 = vfmaq_laneq_f32 (d->c0, r, c1357, 0);
  float32x4_t c23 = vfmaq_laneq_f32 (d->c2, r, c1357, 1);
  float32x4_t c45 = vfmaq_laneq_f32 (d->c4, r, c1357, 2);
  float32x4_t c67 = vfmaq_laneq_f32 (d->c6, r, c1357, 3);

  float32x4_t p47 = vfmaq_f32 (c45, r2, c67);
  float32x4_t p27 = vfmaq_f32 (c23, r2, p47);
  float32x4_t poly = vfmaq_f32 (c01, r2, p27);

  /* y = Log10(2) * n + poly * InvLn(10).  */
  float32x4_t y = vfmaq_f32 (r, d->ln2, n);
  y = vmulq_f32 (y, d->inv_ln10);

  return vfmaq_f32 (y, poly, r2);
}

static inline float32x4_t VPCS_ATTR
special_case (float32x4_t x, const struct data *d)
{
  float32x4_t x_sqrt = vsqrtq_f32 (x);

  uint32x4_t u_off = vsubq_u32 (vreinterpretq_u32_f32 (x_sqrt), d->off);
  float32x4_t n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u_off), 23)); /* signextend.  */

  float32x4_t y = inline_log10f (u_off, n, d);

  /* Scale down by multiplying output by two.
	  Because log(x) = 2log(sqrt(x)).  */
  y = vmulq_f32 (y, v_f32 (2.0f));

  /* Is true for +/- inf, +/- nan as well as all negative numbers.  */
  uint32x4_t is_infnan
      = vcgeq_u32 (vreinterpretq_u32_f32 (x), vreinterpretq_u32_f32 (d->pinf));
  uint32x4_t infnan_or_zero = vorrq_u32 (is_infnan, vceqzq_f32 (x));

  y = vbslq_f32 (infnan_or_zero, d->nan, y);
  uint32x4_t ret_pinf = vceqq_f32 (x, d->pinf);
  uint32x4_t ret_minf = vceqzq_f32 (x);
  y = vbslq_f32 (ret_pinf, d->pinf, y);
  y = vbslq_f32 (ret_minf, d->minf, y);
  return y;
}

/* Fast implementation of AdvSIMD log10f,
   uses a similar approach as AdvSIMD logf with the same offset (i.e., 2/3) and
   an order 9 polynomial.
   Maximum error: 2.81 + 0.5
   _ZGVnN4v_log10f(0x1.555c16p+0) got 0x1.ffe2fap-4
				 want 0x1.ffe2f4p-4.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log10) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* To avoid having to mov x out of the way, keep u after offset has been
     applied, and recover x by adding the offset back in the special-case
     handler.  */
  uint32x4_t u_off = vsubq_u32 (vreinterpretq_u32_f32 (x), d->off);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  float32x4_t n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u_off), 23)); /* signextend.  */

  uint32x4_t special
      = vcgeq_u32 (vsubq_u32 (u_off, d->offset_lower_bound), d->special_bound);
  uint16x4_t special_u16 = vcge_u16 (vsubhn_u32 (u_off, d->offset_lower_bound),
				     vget_low_u16 (d->special_bound_u16));

  if (__glibc_unlikely (v_any_u16h (special_u16)))
    return vbslq_f32 (special, special_case (x, d),
		      inline_log10f (u_off, n, d));
  return inline_log10f (u_off, n, d);
}

libmvec_hidden_def (V_NAME_F1 (log10))
HALF_WIDTH_ALIAS_F1 (log10)
