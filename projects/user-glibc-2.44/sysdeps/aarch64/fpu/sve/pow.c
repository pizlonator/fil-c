/* Double-precision vector (SVE) pow function

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

/* This version share a similar algorithm as AOR scalar pow.

   The core computation consists in computing pow(x, y) as

     exp (y * log (x)).

   The algorithms for exp and log are very similar to scalar exp and log.
   The log relies on table lookup for 3 variables and an order 8 polynomial.
   It returns a high and a low contribution that are then passed to the exp,
   to minimise the loss of accuracy in both routines.
   The exp is based on 8-bit table lookup for scale and order-4 polynomial.
   The SVE algorithm drops the tail in the exp computation at the price of
   a lower accuracy, slightly above 1ULP.
   The SVE algorithm also drops the special treatement of small (< 2^-65) and
   large (> 2^63) finite values of |y|, as they only affect non-round to
   nearest modes.

   Maximum measured error is 1.04 ULPs:
   SV_NAME_D2 (pow) (0x1.3d2d45bc848acp+63, -0x1.a48a38b40cd43p-12)
     got 0x1.f7116284221fcp-1
    want 0x1.f7116284221fdp-1.  */

#include "math_config.h"
#include "sv_math.h"

#define WANT_SV_POW_SIGN_BIAS 1
#include "sv_pow_inline.h"

static inline double
pow_specialcase (double x, double y)
{
  uint64_t ix = asuint64 (x);
  uint64_t iy = asuint64 (y);
  /* Special cases: |x| or |y| is 0, inf or nan.  */
  if (__glibc_unlikely (zeroinfnan (iy)))
    {
      if (2 * iy == 0)
	return issignaling (x) ? x + y : 1.0;
      if (ix == asuint64 (1.0))
	return issignaling (y) ? x + y : 1.0;
      if (2 * ix > 2 * asuint64 (INFINITY) || 2 * iy > 2 * asuint64 (INFINITY))
	return x + y;
      if (2 * ix == 2 * asuint64 (1.0))
	return 1.0;
      if ((2 * ix < 2 * asuint64 (1.0)) == !(iy >> 63))
	return 0.0; /* |x|<1 && y==inf or |x|>1 && y==-inf.  */
      return y * y;
    }
  if (__glibc_unlikely (zeroinfnan (ix)))
    {
      double x2 = x * x;
      if (ix >> 63 && checkint (iy) == 1)
	x2 = -x2;
      return (iy >> 63) ? 1 / x2 : x2;
    }
  return x;
}

/* Scalar fallback for special case routines with custom signature.  */
static svfloat64_t NOINLINE
sv_pow_specialcase (svfloat64_t x1, svfloat64_t x2, svfloat64_t y,
		    svbool_t cmp)
{
  return sv_call2_f64 (pow_specialcase, x1, x2, y, cmp);
}

svfloat64_t SV_NAME_D2 (pow) (svfloat64_t x, svfloat64_t y, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* This preamble handles special case conditions used in the final scalar
     fallbacks. It also updates ix and sign_bias, that are used in the core
     computation too, i.e., exp( y * log (x) ).  */
  svuint64_t vix0 = svreinterpret_u64 (x);
  svuint64_t viy0 = svreinterpret_u64 (y);

  /* Negative x cases.  */
  svbool_t xisneg = svcmplt (pg, x, 0);

  /* Set sign_bias and ix depending on sign of x and nature of y.  */
  svbool_t yint_or_xpos = pg;
  svuint64_t sign_bias = sv_u64 (0);
  svuint64_t vix = vix0;
  if (__glibc_unlikely (svptest_any (pg, xisneg)))
    {
      /* Determine nature of y.  */
      yint_or_xpos = sv_isint (xisneg, y);
      svbool_t yisodd_xisneg = sv_isodd (xisneg, y);
      /* ix set to abs(ix) if y is integer.  */
      vix = svand_m (yint_or_xpos, vix0, 0x7fffffffffffffff);
      /* Set to SignBias if x is negative and y is odd.  */
      sign_bias = svsel (yisodd_xisneg, sv_u64 (SignBias), sv_u64 (0));
    }

  /* Cases of subnormal x: |x| < 0x1p-1022.  */
  svbool_t x_is_subnormal = svaclt (yint_or_xpos, x, 0x1p-1022);
  if (__glibc_unlikely (svptest_any (yint_or_xpos, x_is_subnormal)))
    {
      /* Normalize subnormal x so exponent becomes negative.  */
      vix = svreinterpret_u64 (svmul_m (x_is_subnormal, x, 0x1p52));
      vix = svand_m (x_is_subnormal, vix, 0x7fffffffffffffff);
      vix = svsub_m (x_is_subnormal, vix, 52ULL << 52);
    }

  /* y_hi = log(ix, &y_lo).  */
  svfloat64_t vlo;
  svfloat64_t vhi = sv_log_inline (yint_or_xpos, vix, &vlo, d);

  /* z = exp(y_hi, y_lo, sign_bias).  */
  svfloat64_t vehi = svmul_x (svptrue_b64 (), y, vhi);
  svfloat64_t vemi = svmls_x (yint_or_xpos, vehi, y, vhi);
  svfloat64_t velo = svnmls_x (yint_or_xpos, vemi, y, vlo);
  svfloat64_t vz = sv_exp_inline (yint_or_xpos, vehi, velo, sign_bias, d);

  /* Cases of finite y and finite negative x.  */
  vz = svsel (yint_or_xpos, vz, sv_f64 (__builtin_nan ("")));

  /* Special cases of x or y: zero, inf and nan.  */
  svbool_t xspecial = sv_zeroinfnan (svptrue_b64 (), vix0);
  svbool_t yspecial = sv_zeroinfnan (svptrue_b64 (), viy0);
  svbool_t special = svorr_z (svptrue_b64 (), xspecial, yspecial);

  /* Cases of zero/inf/nan x or y.  */
  if (__glibc_unlikely (svptest_any (svptrue_b64 (), special)))
    vz = sv_pow_specialcase (x, y, vz, special);

  return vz;
}
