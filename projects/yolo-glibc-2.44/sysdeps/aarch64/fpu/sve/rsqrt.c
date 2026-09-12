/* Double-precision vector (SVE) rsqrt function

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

#include "sv_math.h"

static const struct data
{
  float64_t special_bound;
  int64_t scale_up, scale_down;
} data = {
  /* When x < 0x1p-1021, estimate becomes infinity.
    x is scaled up by 0x1p54, so the estimate does not reach infinity.
    Then the result is multiplied by 0x1p27.
    The difference between the lowest power possible (-1074) and the special
    bound (-1021) is 54, so 2^54 is used as the scaling value.  */
  .special_bound = 0x1p-1021,
  .scale_up = 54,
  .scale_down = 27,
};

static inline svfloat64_t
inline_rsqrt (svfloat64_t x)
{
  /* Do estimate instruction.  */
  svfloat64_t estimate = svrsqrte_f64 (x);

  /* Do first step instruction.  */
  svfloat64_t estimate_squared = svmul_x (svptrue_b64 (), estimate, estimate);
  svfloat64_t step = svrsqrts_f64 (x, estimate_squared);
  estimate = svmul_x (svptrue_b64 (), estimate, step);

  /* Do second step instruction.  */
  estimate_squared = svmul_x (svptrue_b64 (), estimate, estimate);
  step = svrsqrts_f64 (x, estimate_squared);
  estimate = svmul_x (svptrue_b64 (), estimate, step);

  /* Do third step instruction.
     This is required to achieve < 3.0 ULP.  */
  estimate_squared = svmul_x (svptrue_b64 (), estimate, estimate);
  step = svrsqrts_f64 (x, estimate_squared);
  estimate = svmul_x (svptrue_b64 (), estimate, step);
  return estimate;
}

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svbool_t special, const struct data *d)
{
  x = svscale_f64_m (special, x, sv_s64 (d->scale_up));
  svfloat64_t estimate = inline_rsqrt (x);
  return svscale_f64_m (special, estimate, sv_s64 (d->scale_down));
}

/* Double-precision SVE implementation of rsqrt(x).
  Maximum observed error: 1.45 + 0.5
  _ZGVnN2v_rsqrt(0x1.d13fb41254643p+1023) got 0x1.0c8dee1b29dfap-512
					 want 0x1.0c8dee1b29df8p-512.  */
svfloat64_t SV_NAME_D1 (rsqrt) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t special = svcmplt_n_f64 (pg, x, d->special_bound);
  if (__glibc_unlikely (svptest_any (pg, special)))
    {
      return special_case (x, special, d);
    }
  return inline_rsqrt (x);
}
