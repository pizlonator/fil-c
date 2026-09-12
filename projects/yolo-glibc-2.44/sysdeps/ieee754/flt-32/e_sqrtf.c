/* e_sqrtf.c -- float version of e_sqrt.c.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* The internal alias to avoid PLT calls interfere with the default
   symbol alias for !LIBM_SVID_COMPAT.  */
#define sqrtf __redirect_sqrtf
#include <math.h>
#undef sqrtf
#include <libm-alias-finite.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include <math-use-builtins.h>
#include "math_config.h"

float
__sqrtf (float x)
{
#if USE_SQRTF_BUILTIN
  if (__glibc_unlikely (isless (x, 0.0f)))
    return __math_invalidf (x);
  return __builtin_sqrtf (x);
#else
  /* Use generic implementation.  */
  float z;
  int32_t sign = (int) 0x80000000;
  int32_t ix, s, q, m, t, i;
  uint32_t r;

  ix = asuint (x);

  /* take care of Inf and NaN */
  if ((ix & 0x7f800000) == 0x7f800000)
    {
      if (ix == 0xff800000)
	return __math_invalidf (0.0f);
      return x * x + x; /* sqrt(NaN)=NaN, sqrt(+inf)=+inf
			   sqrt(-inf)=sNaN */
    }
  /* take care of zero */
  if (ix <= 0)
    {
      if ((ix & (~sign)) == 0)
	return x; /* sqrt(+-0) = +-0 */
      else if (ix < 0)
	return __math_invalidf (0.0f); /* sqrt(-ve) = sNaN */
    }
  /* normalize x */
  m = (ix >> 23);
  if (m == 0)
    { /* subnormal x */
      for (i = 0; (ix & 0x00800000) == 0; i++)
	ix <<= 1;
      m -= i - 1;
    }
  m -= 127; /* unbias exponent */
  ix = (ix & 0x007fffff) | 0x00800000;
  if (m & 1) /* odd m, double x to make it even */
    ix += ix;
  m >>= 1; /* m = [m/2] */

  /* generate sqrt(x) bit by bit */
  ix += ix;
  q = s = 0;	  /* q = sqrt(x) */
  r = 0x01000000; /* r = moving bit from right to left */

  while (r != 0)
    {
      t = s + r;
      if (t <= ix)
	{
	  s = t + r;
	  ix -= t;
	  q += r;
	}
      ix += ix;
      r >>= 1;
    }

  /* use floating add to find out rounding direction */
  if (ix != 0)
    {
      z = 0x1p0 - 0x1.4484cp-100; /* trigger inexact flag.  */
      if (z >= 0x1p0)
	{
	  z = 0x1p0 + 0x1.4484cp-100;
	  if (z > 0x1p0)
	    q += 2;
	  else
	    q += (q & 1);
	}
    }
  ix = (q >> 1) + 0x3f000000;
  ix += (m << 23);
  return asfloat (ix);
#endif /* ! USE_SQRTF_BUILTIN  */
}
libm_alias_finite (__sqrtf, __sqrtf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __sqrtf, sqrtf, GLIBC_2_43);
libm_alias_float_other (__sqrt, sqrt)
#else
libm_alias_float (__sqrt, sqrt)
#endif
