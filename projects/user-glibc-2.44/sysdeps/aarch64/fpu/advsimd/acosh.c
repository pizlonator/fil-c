/* Double-precision vector (Advanced SIMD) acosh function

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

#define WANT_V_LOG1P_K0_SHORTCUT 1
#include "v_log1p_inline.h"

const static struct data
{
  struct v_log1p_data log1p_consts;
  uint64x2_t one, thresh;
} data = {
  .log1p_consts = V_LOG1P_CONSTANTS_TABLE,
  .one = V2 (0x3ff0000000000000),
  .thresh = V2 (0x1ff0000000000000) /* asuint64(0x1p511) - asuint64(1).  */
};

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t special,
	      const struct v_log1p_data *d)
{
  /* Acosh is define on [1, inf). Its formula can be re-written knowing that 1
    becomes negligible when x is a very large number. So for special numbers,
    where x >= 2^511, acosh ~= ln(2x). But, ln(2x) = ln(2) + ln(x) and below we
    calculate ln(x) and then add ln(2) to the result.

    Right before returning we check if x is infinity or if x is lower than 1,
    in which case we return infinity or NaN.  */
  float64x2_t one = v_f64 (1.0);
  float64x2_t t = log1p_inline (vbslq_f64 (special, x, y), d);
  t = vbslq_f64 (special, vaddq_f64 (t, v_f64 (d->ln2[0])), t);
  t = vbslq_f64 (vceqq_f64 (x, v_f64 (INFINITY)), v_f64 (INFINITY), t);
  return vbslq_f64 (vcltq_f64 (x, one), v_f64 (NAN), t);
}

/* Vector approximation for double-precision acosh, based on log1p.
   The largest observed error is 3.02 ULP in the region where the
   argument to log1p falls in the k=0 interval, i.e. x close to 1:
   _ZGVnN2v_acosh(0x1.00798aaf80739p+0) got 0x1.f2d6d823bc9dfp-5
				       want 0x1.f2d6d823bc9e2p-5.  */
VPCS_ATTR float64x2_t V_NAME_D1 (acosh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t special
      = vcgeq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (x), d->one), d->thresh);

  float64x2_t xm1 = vsubq_f64 (x, v_f64 (1.0));
  float64x2_t y = vaddq_f64 (x, v_f64 (1.0));
  y = vmulq_f64 (y, xm1);
  y = vsqrtq_f64 (y);
  y = vaddq_f64 (xm1, y);

  if (__glibc_unlikely (v_any_u64 (special)))
    return special_case (x, y, special, &d->log1p_consts);
  return log1p_inline (y, &d->log1p_consts);
}
