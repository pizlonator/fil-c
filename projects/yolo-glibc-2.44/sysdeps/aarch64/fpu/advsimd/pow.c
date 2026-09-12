/* Double-precision vector (AdvSIMD) pow function

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

#include "v_pow_inline.h"

static double NOINLINE
pow_scalar_special_case (double x, double y)
{
  uint32_t sign_bias = 0;
  uint64_t ix, iy;
  uint32_t topx, topy;

  ix = asuint64 (x);
  iy = asuint64 (y);
  topx = top12 (x);
  topy = top12 (y);
  /* Special cases: (x < 0x1p-126 or inf or nan) or
     (|y| < 0x1p-65 or |y| >= 0x1p63 or nan).  */
  if (__glibc_unlikely (topx - SmallPowX >= ThresPowX
		|| (topy & 0x7ff) - SmallPowY >= ThresPowY))
    {
      /* |y| is 0, Inf or NaN.  */
      if (__glibc_unlikely (zeroinfnan (iy)))
	{
	  if (2 * iy == 0)
	    return __issignaling (x) ? x + y : 1.0;
	  if (ix == asuint64 (1.0))
	    return __issignaling (y) ? x + y : 1.0;
	  if (2 * ix > 2 * asuint64 (INFINITY)
	      || 2 * iy > 2 * asuint64 (INFINITY))
	    return x + y;
	  if (2 * ix == 2 * asuint64 (1.0))
	    return 1.0;
	  if ((2 * ix < 2 * asuint64 (1.0)) == !(iy >> 63))
	    return 0.0; /* |x|<1 && y==inf or |x|>1 && y==-inf.  */
	  return y * y;
	}
      /* |x| is 0, Inf or NaN.  */
      if (__glibc_unlikely (zeroinfnan (ix)))
	{
	  double x2 = x * x;
	  if (ix >> 63 && checkint (iy) == 1)
	    {
	      x2 = -x2;
	      sign_bias = 1;
	    }
	  return iy >> 63 ? 1 / x2 : x2;
	}
      /* Here x and y are non-zero finite.  */
      /* Finite negative x returns NaN unless y is integer.  */
      if (ix >> 63)
	{
	  /* Finite x < 0.  */
	  int yint = checkint (iy);
	  if (yint == 0)
	    return __builtin_nan ("");
	  if (yint == 1)
	    sign_bias = SignBias;
	  ix &= 0x7fffffffffffffff;
	  topx &= 0x7ff;
	}
      /* Note: if |y| > 1075 * ln2 * 2^53 ~= 0x1.749p62 then pow(x,y) = inf/0
	 and if |y| < 2^-54 / 1075 ~= 0x1.e7b6p-65 then pow(x,y) = +-1.  */
      if ((topy & 0x7ff) - SmallPowY >= ThresPowY)
	{
	  /* Note: sign_bias == 0 here because y is not odd.  */
	  if (ix == asuint64 (1.0))
	    return 1.0;
	  /* |y| < 2^-65, x^y ~= 1 + y*log(x).  */
	  if ((topy & 0x7ff) < SmallPowY)
	    return 1.0;
	  return (ix > asuint64 (1.0)) == (topy < 0x800) ? INFINITY : 0;
	}
      if (topx == 0)
	{
	  /* Normalize subnormal x so exponent becomes negative.  */
	  ix = asuint64 (x * 0x1p52);
	  ix &= 0x7fffffffffffffff;
	  ix -= 52ULL << 52;
	}
    }

  /* Core computation of exp (y * log (x)).  */
  double lo;
  double hi = log_inline (ix, &lo);
  double ehi = y * hi;
  double elo = y * lo + fma (y, hi, -ehi);
  return exp_inline (ehi, elo, sign_bias);
}

static float64x2_t VPCS_ATTR NOINLINE
scalar_fallback (float64x2_t x, float64x2_t y)
{
  return (float64x2_t){ pow_scalar_special_case (x[0], y[0]),
			pow_scalar_special_case (x[1], y[1]) };
}

/* Implementation of AdvSIMD pow.
   Maximum measured error is 1.04 ULPs:
   _ZGVnN2vv_pow(0x1.024a3e56b3c3p-136, 0x1.87910248b58acp-13)
     got 0x1.f71162f473251p-1
    want 0x1.f71162f473252p-1.  */
float64x2_t VPCS_ATTR V_NAME_D2 (pow) (float64x2_t x, float64x2_t y)
{
  const struct data *d = ptr_barrier (&data);

  /* Case of x <= 0 is too complicated to be vectorised efficiently here,
     fallback to scalar pow for all lanes if any x < 0 detected.  */
  if (v_any_u64 (vclezq_s64 (vreinterpretq_s64_f64 (x))))
    return scalar_fallback (x, y);

  uint64x2_t vix = vreinterpretq_u64_f64 (x);
  uint64x2_t viy = vreinterpretq_u64_f64 (y);

  /* Special cases of x or y.
     The case y==0 does not trigger a special case, since in this case it is
     necessary to fix the result only if x is a signalling nan, which already
     triggers a special case. We test y==0 directly in the scalar fallback.  */
  uint64x2_t x_is_inf_or_nan = vcgeq_u64 (vandq_u64 (vix, d->inf), d->inf);
  uint64x2_t y_is_inf_or_nan = vcgeq_u64 (vandq_u64 (viy, d->inf), d->inf);
  uint64x2_t special = vorrq_u64 (x_is_inf_or_nan, y_is_inf_or_nan);

  /* Fallback to scalar on all lanes if any lane is inf or nan.  */
  if (__glibc_unlikely (v_any_u64 (special)))
    return scalar_fallback (x, y);

  /* Cases of subnormal x: |x| < 0x1p-1022.  */
  uint64x2_t x_is_subnormal = vcaltq_f64 (x, d->subnormal_bound);
  if (__glibc_unlikely (v_any_u64 (x_is_subnormal)))
    {
      /* Normalize subnormal x so exponent becomes negative.  */
      uint64x2_t vix_norm = vreinterpretq_u64_f64 (
	  vabsq_f64 (vmulq_f64 (x, d->subnormal_scale)));
      vix_norm = vsubq_u64 (vix_norm, d->subnormal_bias);
      x = vbslq_f64 (x_is_subnormal, vreinterpretq_f64_u64 (vix_norm), x);
    }

  /* Core computation of exp (y * log (x)).  */
  return v_pow_inline (x, y, d);
}
