/* Double-precision vector (SVE) powr function

   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include "math_config.h"
#include "pow_common.h"
#include "sv_math.h"

#define WANT_SV_POW_SIGN_BIAS 0
#include "sv_pow_inline.h"

/* A scalar subroutine used to fix main powr special cases.  */
static inline double
powr_specialcase (double x, double y)
{
  uint64_t ix = asuint64 (x);
  uint64_t iy = asuint64 (y);
  /* |y| is 0, Inf or NaN.  */
  if (__glibc_unlikely (zeroinfnan (iy)))
    {
      /* |x| or |y| is NaN.  */
      if (2 * ix > 2 * asuint64 (INFINITY) || 2 * iy > 2 * asuint64 (INFINITY))
	return __builtin_nan ("");
      /* |y| is 0.0.  */
      if (2 * iy == 0)
	{
	  /* |x| = 0 or Inf.  */
	  if ((2 * ix == 0) || (2 * ix == 2 * asuint64 (INFINITY)))
	    return __builtin_nan ("");
	  /* x is finite.  */
	  return 1.0;
	}
      /* x is 1.0.  */
      if (ix == asuint64 (1.0))
	return __builtin_nan ("");
      /* |x| < 1 and y = Inf or |x| > 1 and y = -Inf.  */
      if ((2 * ix < 2 * asuint64 (1.0)) == !(iy >> 63))
	return 0.0;
      /* |y| = Inf and previous conditions not met.  */
      return y * y;
    }
  /* x is 0, Inf or NaN. Negative x are handled in the core.  */
  if (__glibc_unlikely (zeroinfnan (ix)))
    {
      double x2 = x * x;
      return (iy >> 63) ? 1 / x2 : x2;
    }
  /* Return x for convenience, but make sure result is never used.  */
  return x;
}

/* Scalar fallback for special case routines with custom signature.  */
static svfloat64_t NOINLINE
sv_powr_specialcase (svfloat64_t x1, svfloat64_t x2, svfloat64_t y,
		     svbool_t cmp)
{
  return sv_call2_f64 (powr_specialcase, x1, x2, y, cmp);
}

/* Implementation of SVE powr.

   Provides the same accuracy as AdvSIMD pow and powr, since it relies on the
   same algorithm.

   Maximum measured error is 1.04 ULPs:
   SV_NAME_D2 (powr) (0x1.3d2d45bc848acp+63, -0x1.a48a38b40cd43p-12)
     got 0x1.f7116284221fcp-1
    want 0x1.f7116284221fdp-1.  */
svfloat64_t SV_NAME_D2 (powr) (svfloat64_t x, svfloat64_t y, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t vix = svreinterpret_u64 (x);
  svuint64_t viy = svreinterpret_u64 (y);

  svbool_t xpos = svcmpge (pg, x, sv_f64 (0.0));

  /* Special cases of x or y: zero, inf and nan.  */
  svbool_t xspecial = sv_zeroinfnan (xpos, vix);
  svbool_t yspecial = sv_zeroinfnan (xpos, viy);
  svbool_t cmp = svorr_z (xpos, xspecial, yspecial);

  /* Cases of positive subnormal x: 0 < x < 0x1p-1022.  */
  svbool_t x_is_subnormal = svaclt (xpos, x, 0x1p-1022);
  if (__glibc_unlikely (svptest_any (xpos, x_is_subnormal)))
    {
      /* Normalize subnormal x so exponent becomes negative.  */
      svuint64_t vix_norm
	  = svreinterpret_u64 (svmul_m (x_is_subnormal, x, 0x1p52));
      vix = svsub_m (x_is_subnormal, vix_norm, 52ULL << 52);
    }

  svfloat64_t vlo;
  svfloat64_t vhi = sv_log_inline (xpos, vix, &vlo, d);

  svfloat64_t vehi = svmul_x (svptrue_b64 (), y, vhi);
  svfloat64_t vemi = svmls_x (xpos, vehi, y, vhi);
  svfloat64_t velo = svnmls_x (xpos, vemi, y, vlo);
  svfloat64_t vz = sv_exp_inline (xpos, vehi, velo, sv_u64 (0), d);

  /* Cases of negative x.  */
  vz = svsel (xpos, vz, sv_f64 (__builtin_nan ("")));

  if (__glibc_unlikely (svptest_any (cmp, cmp)))
    return sv_powr_specialcase (x, y, vz, cmp);

  return vz;
}
