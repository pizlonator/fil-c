/* Double-precision (Advanced SIMD) cospi function

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
#include "poly_advsimd_f64.h"

static const struct data
{
  float64x2_t poly[10];
  float64x2_t range_val;
} data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see sinpi.sollya for details.  */
  .poly = { V2 (0x1.921fb54442d184p1), V2 (-0x1.4abbce625be53p2),
	    V2 (0x1.466bc6775ab16p1), V2 (-0x1.32d2cce62dc33p-1),
	    V2 (0x1.507834891188ep-4), V2 (-0x1.e30750a28c88ep-8),
	    V2 (0x1.e8f48308acda4p-12), V2 (-0x1.6fc0032b3c29fp-16),
	    V2 (0x1.af86ae521260bp-21), V2 (-0x1.012a9870eeb7dp-25) },
  .range_val = V2 (0x1p63),
};

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t odd, uint64x2_t cmp)
{
  /* Fall back to scalar code.  */
  y = vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
  return v_call_f64 (cospi, x, y, cmp);
}

/* Approximation for vector double-precision cospi(x).
   Maximum Error 3.06 ULP:
  _ZGVnN2v_cospi(0x1.7dd4c0b03cc66p-5) got 0x1.fa854babfb6bep-1
				      want 0x1.fa854babfb6c1p-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (cospi) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint64x2_t cmp = vcageq_f64 (x, d->range_val);
  uint64x2_t odd
      = vshlq_n_u64 (vreinterpretq_u64_s64 (vcvtaq_s64_f64 (x)), 63);

  float64x2_t r = vsubq_f64 (x, vrndaq_f64 (x));

  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  r = vsubq_f64 (v_f64 (0.5), vabsq_f64 (r));

  /* y = sin(r).  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t r4 = vmulq_f64 (r2, r2);
  float64x2_t y = vmulq_f64 (v_pw_horner_9_f64 (r2, r4, d->poly), r);

  /* Fallback to scalar.  */
  if (__glibc_unlikely (v_any_u64 (cmp)))
    return special_case (x, y, odd, cmp);

  /* Reintroduce the sign bit for inputs which round to odd.  */
  return vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
}
