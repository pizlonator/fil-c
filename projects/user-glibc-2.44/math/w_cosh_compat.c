/* Optimizations by Ulrich Drepper <drepper@gmail.com>, 2011 */
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

/*
 * wrapper cosh(x)
 */

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-double.h>

#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_44)
double
__cosh_compat (double x)
{
	double z = __ieee754_cosh (x);
	if (__builtin_expect (!isfinite (z), 0) && isfinite (x)
	    && _LIB_VERSION != _IEEE_)
		return __kernel_standard (x, x, 5); /* cosh overflow */

	return z;
}
# ifdef NO_COMPAT_NEEDED
strong_alias (__cosh_compat, __cosh)
libm_alias_double (__cosh_compat, cosh)
# else
compat_symbol (libm, __cosh_compat, cosh, GLIBC_2_0);
#  ifdef NO_LONG_DOUBLE
weak_alias (__cosh_compat, coshl)
#  endif
#  ifdef LONG_DOUBLE_COMPAT
LONG_DOUBLE_COMPAT_CHOOSE_libm_coshl (
  compat_symbol (libm, __cosh_compat, coshl, FIRST_VERSION_libm_coshl), );
#  endif
# endif
#endif
