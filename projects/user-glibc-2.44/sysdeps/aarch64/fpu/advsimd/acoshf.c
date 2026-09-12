/* Single-precision vector (Advanced SIMD) acosh function

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

#include "v_log1pf_inline.h"

const static struct data
{
  struct v_log1pf_data log1pf_consts;
  uint32x4_t one;
  uint16x4_t special_bound_u16;
  float32x4_t inf, nan;
} data = {
  .log1pf_consts = V_LOG1PF_CONSTANTS_TABLE,
  .one = V4 (0x3f800000),
  /* asuint(sqrt(FLT_MAX)) - asuint(1).  */
  .special_bound_u16 = V4 (0x2000),
  .inf = V4 (INFINITY),
  .nan = V4 (NAN),
};

static inline float32x4_t VPCS_ATTR
inline_acoshf (float32x4_t x, const struct data *d)
{
  /* acosh(x) = ln(x + sqrt[x^2 -1]).
    So acosh(x) = log1p (x + sqrt[x^2 - 1] - 1).  */
  float32x4_t xm1 = vsubq_f32 (x, vreinterpretq_f32_u32 (d->one));
  float32x4_t u
      = vmulq_f32 (xm1, vaddq_f32 (x, vreinterpretq_f32_u32 (d->one)));

  float32x4_t y = vaddq_f32 (xm1, vsqrtq_f32 (u));

  return log1pf_inline (y, &d->log1pf_consts);
}

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, uint16x4_t special_u16, const struct data *d)
{
  float32x4_t one = v_f32 (1.0);
  float32x4_t ln2 = d->log1pf_consts.ln2;

  /* acosh(x) = ln(x + sqrt[x^2 -1]).
    So acosh(x) = log1p (x + sqrt[x^2 - 1] - 1).  */
  float32x4_t xm1 = vsubq_f32 (x, one);
  float32x4_t u = vmulq_f32 (xm1, vaddq_f32 (x, one));

  float32x4_t y = vaddq_f32 (xm1, vsqrtq_f32 (u));

  /* special_u16 will be 0x0000ffff for true lanes, which doesn't work with bit
     selects therefore to make it a 32 bit predicate, we have to add special
     << 16.  */
  uint32x4_t special = vmovl_u16 (special_u16);
  special = vaddq_u32 (special, vshlq_n_u32 (special, 16));
  float32x4_t xy = vbslq_f32 (special, x, y);

  /* For large inputs, acosh(x) ≈ log(x) + ln(2).
     We use log1pf-inline log implementation and add ln(2).  */
  float32x4_t log_xy = log1pf_inline (xy, &d->log1pf_consts);

  /* For acoshf there are three special cases that need considering. Infinity
     and NaNs, which are also returned unchanged and for cases of x < 1 we'll
     return all NaNs since acoshf is defined on (1, inf).  */
  uint32x4_t is_lower_one = vcltq_f32 (x, one);
  uint32x4_t is_finite = vcltq_f32 (x, d->inf);

  /* This dependency chain of selects can run in parallel with y and log_x
     being calculated before the last addition.  */
  float32x4_t ln2_inf_nan = vbslq_f32 (is_finite, ln2, x);
  ln2_inf_nan = vbslq_f32 (is_lower_one, d->nan, ln2_inf_nan);
  float32x4_t ln2_inf_nan_zero = vbslq_f32 (special, ln2_inf_nan, v_f32 (0));

  return vaddq_f32 (log_xy, ln2_inf_nan_zero);
}

/* Vector approximation for single-precision acosh, based on log1p.
   The largest observed error is 3.22 ULP:
   _ZGVnN4v_acoshf(0x1.007ef2p+0) got 0x1.fdcdccp-5
				 want 0x1.fdcdd2p-5.  */

float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (acosh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  /* Inputs greater than or equal to special_bound will cause the output to
    overflow. This is because there is a square operation in log1pf_inline.
    This also captures inf, nan and any input less than or equal to 1.  */
  uint16x4_t special
      = vcge_u16 (vsubhn_u32 (ix, d->one), d->special_bound_u16);

  if (__glibc_unlikely (v_any_u16h (special)))
    return special_case (x, special, d);
  return inline_acoshf (x, d);
}

libmvec_hidden_def (V_NAME_F1 (acosh))
HALF_WIDTH_ALIAS_F1 (acosh)
