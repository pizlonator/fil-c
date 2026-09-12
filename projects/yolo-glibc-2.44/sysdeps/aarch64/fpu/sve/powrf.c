/* Single-precision vector (SVE) powr function

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

#include "flt-32/math_config.h"
#include "sv_math.h"

#define WANT_SV_POWF_SIGN_BIAS 0
#include "sv_powf_inline.h"

/* A scalar subroutine used to fix main powrf special cases.  */
static inline float
powrf_specialcase (float x, float y)
{
  uint32_t ix = asuint (x);
  uint32_t iy = asuint (y);
  /* |y| is 0, Inf or NaN.  */
  if (__glibc_unlikely (zeroinfnan (iy)))
    {
      /* |x| or |y| is NaN.  */
      if (2 * ix > 2u * 0x7f800000 || 2 * iy > 2u * 0x7f800000)
	return __builtin_nanf ("");
      /* |y| = 0.  */
      if (2 * iy == 0)
	{
	  /* |x| = 0 or Inf.  */
	  if ((2 * ix == 0) || (2 * ix == 2u * 0x7f800000))
	    return __builtin_nanf ("");
	  /* x is finite.  */
	  return 1.0f;
	}
      /* |y| = Inf and x = 1.0.  */
      if (ix == 0x3f800000)
	return __builtin_nanf ("");
      /* |x| < 1 and y = Inf or |x| > 1 and y = -Inf.  */
      if ((2 * ix < 2 * 0x3f800000) == !(iy & 0x80000000))
	return 0.0f;
      /* |y| = Inf and previous conditions not met.  */
      return y * y;
    }
  /* x is 0, Inf or NaN. Negative x are handled in the core.  */
  if (__glibc_unlikely (zeroinfnan (ix)))
    {
      float x2 = x * x;
      return iy & 0x80000000 ? 1 / x2 : x2;
    }
  /* Return x for convenience, but make sure result is never used.  */
  return x;
}

/* Scalar fallback for special case routines with custom signature.  */
static svfloat32_t NOINLINE
sv_call_powrf_sc (svfloat32_t x1, svfloat32_t x2, svfloat32_t y, svbool_t cmp)
{
  return sv_call2_f32 (powrf_specialcase, x1, x2, y, cmp);
}

/* Implementation of SVE powrf.

   Provides the same accuracy as AdvSIMD powf and powrf, since it relies on the
   same algorithm.

   Maximum measured error is 2.57 ULPs:
   SV_NAME_F2 (powr) (0x1.031706p+0, 0x1.ce2ec2p+12)
     got 0x1.fff868p+127
    want 0x1.fff862p+127.  */
svfloat32_t SV_NAME_F2 (powr) (svfloat32_t x, svfloat32_t y, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t vix = svreinterpret_u32 (x);
  svuint32_t viy = svreinterpret_u32 (y);

  svbool_t xpos = svcmpge (pg, x, sv_f32 (0.0f));

  /* Special cases of x or y: zero, inf and nan.  */
  svbool_t xspecial = sv_zeroinfnan (xpos, vix);
  svbool_t yspecial = sv_zeroinfnan (xpos, viy);
  svbool_t cmp = svorr_z (xpos, xspecial, yspecial);

  /* Cases of subnormal x: |x| < 0x1p-126.  */
  svbool_t x_is_subnormal = svaclt (xpos, x, d->small_bound);
  if (__glibc_unlikely (svptest_any (xpos, x_is_subnormal)))
    {
      /* Normalize subnormal x so exponent becomes negative.  */
      vix = svreinterpret_u32 (svmul_m (x_is_subnormal, x, 0x1p23f));
      vix = svsub_m (x_is_subnormal, vix, d->subnormal_bias);
    }

  /* Part of core computation carried in working precision.  */
  svuint32_t tmp = svsub_x (xpos, vix, d->off);
  svuint32_t i
      = svand_x (xpos, svlsr_x (xpos, tmp, (23 - V_POWF_LOG2_TABLE_BITS)),
		 V_POWF_LOG2_N - 1);
  svuint32_t top = svand_x (xpos, tmp, 0xff800000);
  svuint32_t iz = svsub_x (xpos, vix, top);
  svint32_t k
      = svasr_x (xpos, svreinterpret_s32 (top), (23 - V_POWF_EXP2_TABLE_BITS));

  /* Compute core in extended precision and return intermediate ylogx results
     to handle cases of underflow and underflow in exp.  */
  svfloat32_t ylogx;
  /* Pass a dummy sign_bias so we can re-use powf core.
     The core is simplified by setting WANT_SV_POWF_SIGN_BIAS = 0.  */
  svfloat32_t ret = sv_powf_core (xpos, i, iz, k, y, sv_u32 (0), &ylogx, d);

  /* Handle exp special cases of underflow and overflow.  */
  svbool_t no_uflow = svcmpgt (xpos, ylogx, d->uflow_bound);
  svbool_t oflow = svcmpgt (xpos, ylogx, d->oflow_bound);
  svfloat32_t ret_flow = svdup_n_f32_z (no_uflow, INFINITY);
  ret = svsel (svorn_z (xpos, oflow, no_uflow), ret_flow, ret);

  /* Cases of negative x.  */
  ret = svsel (xpos, ret, sv_f32 (__builtin_nanf ("")));

  if (__glibc_unlikely (svptest_any (cmp, cmp)))
    return sv_call_powrf_sc (x, y, ret, cmp);

  return ret;
}
