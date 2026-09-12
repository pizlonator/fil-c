/* Single-precision vector (Advanced SIMD) asinh function

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
#include "v_log1pf_inline.h"

const static struct data
{
  struct v_log1pf_data log1pf_consts;
  float32x4_t one;
  uint32x4_t square_lim;
  float32x4_t inf, nan;
} data = {
  .one = V4 (1),
  .log1pf_consts = V_LOG1PF_CONSTANTS_TABLE,
  .square_lim = V4 (0x5f800000), /* asuint(sqrt(FLT_MAX)).  */
  .inf = V4 (INFINITY),
  .nan = V4 (NAN),
};

static inline float32x4_t VPCS_ATTR
inline_asinhf (float32x4_t ax, uint32x4_t sign, const struct data *d)
{
  /* Consider the identity asinh(x) = log(x + sqrt(x^2 + 1)).
    Then, for x>0, asinh(x) = log1p(x + x^2 / (1 + sqrt(x^2 + 1))).  */
  float32x4_t t
      = vaddq_f32 (v_f32 (1.0f), vsqrtq_f32 (vfmaq_f32 (d->one, ax, ax)));
  float32x4_t y = vaddq_f32 (ax, vdivq_f32 (vmulq_f32 (ax, ax), t));

  return vreinterpretq_f32_u32 (veorq_u32 (
      sign, vreinterpretq_u32_f32 (log1pf_inline (y, &d->log1pf_consts))));
}

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t ax, uint32x4_t sign, uint32x4_t special,
	      const struct data *d)
{
  float32x4_t t
      = vaddq_f32 (v_f32 (1.0f), vsqrtq_f32 (vfmaq_f32 (d->one, ax, ax)));
  float32x4_t y = vaddq_f32 (ax, vdivq_f32 (vmulq_f32 (ax, ax), t));

  /* For large inputs (x > 2^64), asinh(x) ≈ ln(2x).
     1 becomes negligible in sqrt(x^2+1), so we compute
     asinh(x) as ln(x) + ln(2).  */
  float32x4_t xy = vbslq_f32 (special, ax, y);
  float32x4_t log_xy = log1pf_inline (xy, &d->log1pf_consts);

  /* Infinity and NaNs are the only other special cases that need checking
     before we return the values. 0 is handled by inline_asinhf as it returns
     0. Below we implememt the logic that returns infinity when infinity is
     passed and NaNs when NaNs are passed. Sometimes due to intrinsics like
     vdivq_f32, infinity can change into NaNs, so we want to make sure the
     right result is returned.

     Since these steps run in parallel with the log, we select between the
     results we'll want to add to log_x in time, since addition with infinity
     and NaNs doesn't make a difference. When we get to the adition step,
     everything is already in the right place.  */
  float32x4_t ln2 = d->log1pf_consts.ln2;
  uint32x4_t is_finite = vcltq_f32 (ax, d->inf);
  float32x4_t ln2_inf_nan = vbslq_f32 (is_finite, ln2, ax);

  /* Before returning the result, the right sign will be assinged to the
     absolute result. This is because we pass an absoulte x to the function.
   */

  float32x4_t asinhf
      = vbslq_f32 (special, vaddq_f32 (log_xy, ln2_inf_nan), log_xy);
  return vreinterpretq_f32_u32 (
      veorq_u32 (sign, vreinterpretq_u32_f32 (asinhf)));
}

/* Single-precision implementation of vector asinh(x), using vector log1p.
   Worst-case error is 2.59 ULP:
   _ZGVnN4v_asinhf(0x1.d86124p-3) got 0x1.d449bep-3
				 want 0x1.d449c4p-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (asinh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  float32x4_t ax = vabsq_f32 (x);
  uint32x4_t iax = vreinterpretq_u32_f32 (ax);
  uint32x4_t sign = veorq_u32 (vreinterpretq_u32_f32 (x), iax);

  /* Inputs greater than or equal to square_lim will cause the output to
    overflow. This is because there is a square operation in the log1pf_inline
    call. Also captures inf and nan. Does not capture negative numbers as we
    separate the sign bit from the rest of the input.  */
  uint32x4_t special = vcgeq_u32 (iax, d->square_lim);

  if (__glibc_unlikely (v_any_u32 (special)))
    return special_case (ax, sign, special, d);
  return inline_asinhf (ax, sign, d);
}

libmvec_hidden_def (V_NAME_F1 (asinh))
HALF_WIDTH_ALIAS_F1 (asinh)
