/* Double-precision AdvSIMD log1p

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

#define WANT_V_LOG1P_K0_SHORTCUT 0
#include "v_log1p_inline.h"

const static struct data
{
  struct v_log1p_data d;
  float64x2_t nan, pinf, minf;
} data = {
  .d = V_LOG1P_CONSTANTS_TABLE,
  .nan = V2 (NAN),
  .pinf = V2 (INFINITY),
  .minf = V2 (-INFINITY),
};

static inline float64x2_t
special_case (float64x2_t x, uint64x2_t cmp, const struct data *d)
{
  float64x2_t y = log1p_inline (x, ptr_barrier (&d->d));
  y = vbslq_f64 (cmp, d->nan, y);
  uint64x2_t ret_pinf = vceqq_f64 (x, d->pinf);
  uint64x2_t ret_minf = vceqq_f64 (x, v_f64 (-1.0));

  y = vbslq_f64 (ret_pinf, d->pinf, y);
  return vbslq_f64 (ret_minf, d->minf, y);
}

/* Vector log1p approximation using polynomial on reduced interval. Routine is
   a modification of the algorithm used in scalar log1p, with no shortcut for
   k=0 and no narrowing for f and k.
   Maximum observed error is 1.95 + 0.5 ULP
   _ZGVnN2v_log1p(0x1.658f7035c4014p+11) got 0x1.fd61d0727429dp+2
					want 0x1.fd61d0727429fp+2 .  */
float64x2_t VPCS_ATTR NOINLINE V_NAME_D1 (log1p) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* Use signed integers here to ensure that negative numbers between 0 and -1
    don't make this expression true.  */
  uint64x2_t is_infnan
      = vcgeq_s64 (vreinterpretq_s64_f64 (x), vreinterpretq_s64_f64 (d->pinf));
  /* The OR-NOT is needed to catch -NaN.  */
  uint64x2_t special_cases = vornq_u64 (is_infnan, vcgtq_f64 (x, v_f64 (-1)));

  if (__glibc_unlikely (v_any_u64 (special_cases)))
    return special_case (x, special_cases, d);

  return log1p_inline (x, &d->d);
}

strong_alias (V_NAME_D1 (log1p), V_NAME_D1 (logp1))
