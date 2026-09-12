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

static const struct data
{
  float32x4_t c0, c2, c4, c6, c8;
  uint32x4_t off, offset_lower_bound;
  uint32x4_t special_bound;
  uint16x8_t special_bound_u16;
  uint32x4_t mantissa_mask;
  float c1, c3, c5, c7;
  float32x4_t pinf, minf, nan;
} data = {
  /* Coefficients generated using Remez algorithm approximate
     log2(1+r)/r for r in [ -1/3, 1/3 ].
     rel error: 0x1.c4c4b0cp-26.  */
  .c0 = V4 (0x1.715476p0f), /* (float)(1 / ln(2)).  */
  .c1 = -0x1.715458p-1f,
  .c2 = V4 (0x1.ec701cp-2f),
  .c3 = -0x1.7171a4p-2f,
  .c4 = V4 (0x1.27a0b8p-2f),
  .c5 = -0x1.e5143ep-3f,
  .c6 = V4 (0x1.9d8ecap-3f),
  .c7 = -0x1.c675bp-3f,
  .c8 = V4 (0x1.9e495p-3f),
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
inline_log2f (uint32x4_t u_off, float32x4_t n, const struct data *d)
{
  uint32x4_t u = vaddq_u32 (vandq_u32 (u_off, d->mantissa_mask), d->off);
  float32x4_t r = vsubq_f32 (vreinterpretq_f32_u32 (u), v_f32 (1.0f));

  /* y = log2(1+r) + n.  */
  float32x4_t r2 = vmulq_f32 (r, r);

  float32x4_t c1357 = vld1q_f32 (&d->c1);
  float32x4_t c01 = vfmaq_laneq_f32 (d->c0, r, c1357, 0);
  float32x4_t c23 = vfmaq_laneq_f32 (d->c2, r, c1357, 1);
  float32x4_t c45 = vfmaq_laneq_f32 (d->c4, r, c1357, 2);
  float32x4_t c67 = vfmaq_laneq_f32 (d->c6, r, c1357, 3);
  float32x4_t p68 = vfmaq_f32 (c67, r2, d->c8);
  float32x4_t p48 = vfmaq_f32 (c45, r2, p68);
  float32x4_t p28 = vfmaq_f32 (c23, r2, p48);
  float32x4_t p = vfmaq_f32 (c01, r2, p28);

  return vfmaq_f32 (n, p, r);
}

static float32x4_t VPCS_ATTR NOINLINE
special_case (uint32x4_t u_off, const struct data *d)
{
  float32x4_t x = vreinterpretq_f32_u32 (vaddq_u32 (u_off, d->off));
  uint32x4_t special
      = vcgeq_u32 (vsubq_u32 (u_off, d->offset_lower_bound), d->special_bound);
  float32x4_t x_sqrt = vbslq_f32 (special, vsqrtq_f32 (x), x);

  u_off = vsubq_u32 (vreinterpretq_u32_f32 (x_sqrt), d->off);
  float32x4_t n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u_off), 23)); /* signextend.  */

  float32x4_t y = inline_log2f (u_off, n, d);

  /* Scale down by multiplying output by two.
	  Because log(x) = 2log(sqrt(x)).  */
  y = vbslq_f32 (special, vmulq_f32 (y, v_f32 (2.0f)), y);

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

/* Fast implementation for single precision AdvSIMD log2,
   relies on same argument reduction as AdvSIMD logf.
   Maximum error: 1.99 + 0.5
   _ZGVnN4v_log2f(0x1.558174p+0) got 0x1.a9be84p-2
				want 0x1.a9be8p-2.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log2) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* To avoid having to mov x out of the way, keep u after offset has been
     applied, and recover x by adding the offset back in the special-case
     handler.  */
  uint32x4_t u_off = vsubq_u32 (vreinterpretq_u32_f32 (x), d->off);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  float32x4_t n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u_off), 23)); /* signextend.  */

  uint16x4_t special_u16 = vcge_u16 (vsubhn_u32 (u_off, d->offset_lower_bound),
				     vget_low_u16 (d->special_bound_u16));

  if (__glibc_unlikely (v_any_u16h (special_u16)))
    return special_case (u_off, d);
  return inline_log2f (u_off, n, d);
}

libmvec_hidden_def (V_NAME_F1 (log2))
HALF_WIDTH_ALIAS_F1 (log2)
