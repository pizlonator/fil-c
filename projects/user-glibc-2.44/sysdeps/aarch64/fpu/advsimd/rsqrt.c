/* Double-precision vector (Advanced SIMD) rsqrt function

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

static const struct data
{
  float64x2_t special_bound;
  float64x2_t scale_up, scale_down;
} data = {
  /* When x < 0x1p-1021, estimate becomes infinity.
    x is scaled up by 0x1p54, so the estimate does not reach infinity.
    Then the result is multiplied by 0x1p27.
    The difference between the lowest power possible (-1074) and the special
    bound (-1021) is 54, so 2^54 is used as the scaling value.  */
  .special_bound = V2 (0x1p-1021),
  .scale_up = V2 (0x1p54),
  .scale_down = V2 (0x1p27),
};

static inline float64x2_t VPCS_ATTR
inline_rsqrt (float64x2_t x)
{
  /* Do estimate instruction.  */
  float64x2_t estimate = vrsqrteq_f64 (x);

  /* Do first step instruction.  */
  float64x2_t estimate_squared = vmulq_f64 (estimate, estimate);
  float64x2_t step = vrsqrtsq_f64 (x, estimate_squared);
  estimate = vmulq_f64 (estimate, step);

  /* Do second step instruction.  */
  estimate_squared = vmulq_f64 (estimate, estimate);
  step = vrsqrtsq_f64 (x, estimate_squared);
  estimate = vmulq_f64 (estimate, step);

  /* Do third step instruction.
     This is required to achieve < 3.0 ULP.  */
  estimate_squared = vmulq_f64 (estimate, estimate);
  step = vrsqrtsq_f64 (x, estimate_squared);
  estimate = vmulq_f64 (estimate, step);
  return estimate;
}

static float64x2_t NOINLINE
special_case (float64x2_t x, uint64x2_t special, const struct data *d)
{
  x = vbslq_f64 (special, vmulq_f64 (x, d->scale_up), x);
  float64x2_t estimate = inline_rsqrt (x);
  return vbslq_f64 (special, vmulq_f64 (estimate, d->scale_down), estimate);
}

/* Double-precision implementation of vector rsqrt(x).
  Maximum observed error: 1.45 + 0.5
  _ZGVnN2v_rsqrt(0x1.d13fb41254643p+1023) got 0x1.0c8dee1b29dfap-512
					 want 0x1.0c8dee1b29df8p-512.  */
float64x2_t VPCS_ATTR V_NAME_D1 (rsqrt) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* Special case: x < special_bound.  */
  uint64x2_t special = vcgtq_f64 (d->special_bound, x);
  if (__glibc_unlikely (v_any_u64 (special)))
    {
      return special_case (x, special, d);
    }
  return inline_rsqrt (x);
}
