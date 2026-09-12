/* Double-precision vector (SVE) cos function.

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

#include "sv_math.h"
#include "sv_trig_fallback.h"

static const struct data
{
  double inv_pio2, pio2_1, pio2_2, pio2_3, shift, range_val;
} data = {
  /* Polynomial coefficients are hardwired in FTMAD instructions.  */
  .inv_pio2 = 0x1.45f306dc9c882p-1,
  .pio2_1 = 0x1.921fb54442d18p+0,
  .pio2_2 = 0x1.1a62633145c07p-54,
  .pio2_3 = -0x1.f1976b7ed8fbcp-110,
  /* Original shift used in AdvSIMD cos,
     plus a contribution to set the bit #0 of q
     as expected by trigonometric instructions.  */
  .shift = 0x1.8000000000001p52,
  .range_val = 0x1p23,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  svfloat64x2_t r = sv_large_range_reduction (x);

  /* Unpack return struct.  */
  svfloat64_t remainder = svget2 (r, 0);
  svuint64_t quadrant = svreinterpret_u64 (svget2 (r, 1));

  svfloat64x2_t eval = sv_sincos_eval (remainder);
  svfloat64x2_t lookup = sv_sin_cos_lookup (quadrant);

  svfloat64_t sin_r = svget2 (eval, 0);
  svfloat64_t cosm1_r = svget2 (eval, 1);
  svfloat64_t sin_k = svget2 (lookup, 0);
  svfloat64_t cos_k = svget2 (lookup, 1);

  /* Construct cos(x) from k and r, using angle addition formula, with
    approximations of sin(r) and cos(r) - 1 to reduce rounding errors.
    cos(x) = cos(k + r)
      = cos(k)*cos(r) - sin(k)*sin(r)
      = cos(k)*cosm1(r) - sin(k)*sin(r) + cos(k).  */

  svfloat64_t large_cos = svmla_x (svptrue_b64 (), cos_k, cosm1_r, cos_k);
  large_cos = svmls_x (svptrue_b64 (), large_cos, sin_k, sin_r);

  /* Inf cases are handled correctly by the fast path, and incorrectly
    by the slow path. However, it's less costly to the fast path to
    handle them separately. So we do want to branch here for inf cases,
    but then use the fast path value anyway.  */
  special = svaclt (special, x, sv_f64 (INFINITY));
  return svsel (special, large_cos, y);
}

/* Vector version of cos.
   The maximum observed error is 1.53 + 0.5 ULP when |x| < 0x1p23.
   _ZGVsMxv_cos (0x1.166b1063318b8p+19)
    got 0x1.fff1e92b6c31ap-4
   want 0x1.fff1e92b6c318p-4
   The special domain has a higher maximum error than the fast path:
   Maximum observed error is 2.44 + 0.5ULP when |x| >= 0x1p23.
   _ZGVsMxv_cos (0x1.aac6f8bffec82p+206)
    got -0x1.98ecd0b3020bfp-7
   want -0x1.98ecd0b3020bcp-7.  */
svfloat64_t SV_NAME_D1 (cos) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svfloat64_t inv_pio2 = svld1rq (svptrue_b64 (), &d->inv_pio2);
  svfloat64_t pio2_23 = svld1rq (svptrue_b64 (), &d->pio2_2);

  /* n = rint(x/(pi/2)).  */
  svfloat64_t q = svmla_lane (sv_f64 (d->shift), x, inv_pio2, 0);
  svfloat64_t n = svsub_x (pg, q, sv_f64 (d->shift));

  /* r = x - n*(pi/2)  (range reduction into -pi/4 .. pi/4).  */
  svfloat64_t r = x;
  r = svmls_lane (r, n, inv_pio2, 1);
  r = svmls_lane (r, n, pio2_23, 0);
  r = svmls_lane (r, n, pio2_23, 1);

  /* sin(r) or cos(r) poly approx, selected by the quadrant bits in q.  */
  svfloat64_t r2 = svtsmul (r, svreinterpret_u64 (q));
  svfloat64_t y = sv_f64 (0.0);
  y = svtmad (y, r2, 7);
  y = svtmad (y, r2, 6);
  y = svtmad (y, r2, 5);
  y = svtmad (y, r2, 4);
  y = svtmad (y, r2, 3);
  y = svtmad (y, r2, 2);
  y = svtmad (y, r2, 1);
  y = svtmad (y, r2, 0);

  /* Final multiplicative factor: 1.0 or x depending on bit #0 of q.  */
  svfloat64_t f = svtssel (r, svreinterpret_u64 (q));

  svbool_t special = svacge (pg, x, d->range_val);
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svmul_x (svptrue_b64 (), f, y), special);

  /* Apply factor.  */
  return svmul_x (svptrue_b64 (), f, y);
}
