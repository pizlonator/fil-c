/* Compute x * y + z as ternary operation.
   Copyright (C) 2010-2026 Free Software Foundation, Inc.
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

#define NO_MATH_REDIRECT
#include <float.h>
#define dfmal __hide_dfmal
#define f32xfmaf64 __hide_f32xfmaf64
#include <math.h>
#undef dfmal
#undef f32xfmaf64
#include <libm-alias-double.h>
#include <math-narrow-alias.h>
#include <math-use-builtins.h>


#if !USE_FMA_BUILTIN
# include <stdbit.h>
# include "math_config.h"
# include <math_uint128.h>

# define ZEROINFNAN (0x7ff - EXPONENT_BIAS - MANTISSA_WIDTH - 1)

struct num
{
  uint64_t m;
  int e;
  int sign;
};

static inline struct num normalize (double x)
{
  uint64_t ix = asuint64 (x);
  int e = ix >> MANTISSA_WIDTH;
  int sign = e & 0x800;
  e &= 0x7ff;
  if (!e)
    {
      ix = asuint64 (x * 0x1p63);
      e = ix >> MANTISSA_WIDTH & 0x7ff;
      e = e ? e-63 : 0x800;
    }
  ix &= (UINT64_C(1) << MANTISSA_WIDTH) - 1;
  ix |= UINT64_C(1) << MANTISSA_WIDTH;
  ix <<= 1;
  e -= EXPONENT_BIAS + MANTISSA_WIDTH + 1;
  return (struct num){ix,e,sign};
}

static void mul (uint64_t *hi, uint64_t *lo, uint64_t x, uint64_t y)
{
  u128 r = u128_mul (u128_from_u64 (x), u128_from_u64 (y));
  *hi = u128_high (r);
  *lo = u128_low (r);
}
#endif

double
__fma (double x, double y, double z)
{
#if USE_FMA_BUILTIN
  return __builtin_fma (x, y, z);
#else
  /* Normalize so top 10bits and last bit are 0  */
  struct num nx, ny, nz;
  nx = normalize (x);
  ny = normalize (y);
  nz = normalize (z);

  if (nx.e >= ZEROINFNAN || ny.e >= ZEROINFNAN)
    return x * y + z;
  if (nz.e >= ZEROINFNAN)
    {
      if (nz.e > ZEROINFNAN)	/* z==0 */
	return x * y;
      else if (isnan (z))
	return __builtin_nan ("");
      return z;
    }

  /* mul: r = x*y  */
  uint64_t rhi, rlo, zhi, zlo;
  mul (&rhi, &rlo, nx.m, ny.m);
  /* Either top 20 or 21 bits of rhi and last 2 bits of rlo are 0  */

  /* align exponents */
  int e = nx.e + ny.e;
  int d = nz.e - e;
  /* Shift bits z<<=kz, r>>=kr, so kz+kr == d, set e = e+kr (== ez-kz).  */
  if (d > 0)
    {
      if (d < 64)
	{
	  zlo = nz.m << d;
	  zhi = nz.m >> (64 - d);
	}
      else
	{
	  zlo = 0;
	  zhi = nz.m;
	  e = nz.e - 64;
	  d -= 64;
	  if (d == 0)
	    /* No further shift of r needed; rhi/rlo are unchanged.  */
	    ;
	  else if (d < 64)
	    {
	      rlo = rhi << (64 - d) | rlo >> d | !!(rlo << (64 - d));
	      rhi = rhi >> d;
	    }
	  else
	    {
	      rlo = 1;
	      rhi = 0;
	    }
	}
    }
  else
    {
      zhi = 0;
      d = -d;
      if (d == 0)
	zlo = nz.m;
      else if (d < 64)
	zlo = nz.m >> d | !!(nz.m << (64 - d));
      else
	zlo = 1;
    }

  /* add  */
  int sign = nx.sign ^ ny.sign;
  bool samesign = !(sign ^ nz.sign);
  bool nonzero = true;
  if (samesign)
    {
      /* r += z  */
      rlo += zlo;
      rhi += zhi + (rlo < zlo);
    }
  else
    {
      /* r -= z  */
      uint64_t t = rlo;
      rlo -= zlo;
      rhi = rhi - zhi - (t < rlo);
      if (rhi >> 63)
	{
	  rlo = -rlo;
	  rhi = -rhi - !!rlo;
	  sign = !sign;
	}
      nonzero = !!rhi;
    }

  /* Set rhi to top 63bit of the result (last bit is sticky).  */
  if (nonzero)
    {
      e += 64;
      d = stdc_leading_zeros (rhi) - 1;
      /* note: d > 0 */
      rhi = rhi << d | rlo >> (64 - d) | !!(rlo << d);
    }
  else if (rlo)
    {
      d = stdc_leading_zeros (rlo) - 1;
      if (d < 0)
	rhi = rlo >> 1 | (rlo & 1);
      else
	rhi = rlo << d;
    }
  else
    {
      /* Exact +-0  */
      return x * y + z;
    }
  e -= d;

  /* Convert to double.  */
  int64_t i = rhi;		   /* i is in [1<<62,(1<<63)-1] */
  if (sign)
    i = -i;
  double r = convertfromint64 (i); /* |r| is in [0x1p62,0x1p63] */

  if (__glibc_likely (e >= -1084 && e <= 960))
    /* Fast-path for normal numbers.  */
    return asdouble (asuint64 (r) + ((int64_t) e << MANTISSA_WIDTH));
  else if (e < -1022 - 62)
    {
      /* Result is subnormal before rounding.  */
      if (e == -1022 - 63)
	{
	  double c = 0x1p63;
	  if (sign)
	    c = -c;
	  if (r == c)
	    {
	      /* Min normal after rounding, underflow depends
	         on arch behaviour which can be imitated by
	         a double to float conversion.  */
	      float fltmin = 0x0.ffffff8p-63 * FLT_MIN * r;
	      return DBL_MIN / FLT_MIN * fltmin;
	    }
	  /* One bit is lost when scaled, add another top bit to
	     only round once at conversion if it is inexact.  */
	  if (rhi << 53)
	    {
	      i = rhi >> 1 | (rhi & 1) | 1ull << 62;
	      if (sign)
		i = -i;
	      r = convertfromint64 (i);
	      r = 2 * r - c;	/* remove top bit */

	      /* Raise underflow portably, such that it
	         cannot be optimized away.  */
	      {
		double_t tiny = DBL_MIN / FLT_MIN * r;
		r += (double) (tiny * tiny) * (r - r);
	      }
	    }
	}
      else
	{
	  /* Only round once when scaled.  */
	  d = 10;
	  i = (rhi >> d | !!(rhi << (64 - d))) << d;
	  if (sign)
	    i = -i;
	  r = convertfromint64 (i);
	}
    }
  return __scalbn (r, e);
#endif /* ! USE_FMA_BUILTIN  */
}

#ifndef __fma
libm_alias_double (__fma, fma)
libm_alias_double_narrow (__fma, fma)
#endif
