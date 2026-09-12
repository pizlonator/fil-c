/* @(#)w_sinh.c 5.1 93/09/24 */
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
 * wrapper sinh(x)
 */

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-double.h>

#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_44)
double
__sinh_compat (double x)
{
	double z = __ieee754_sinh (x);
	if (__builtin_expect (!isfinite (z), 0) && isfinite (x)
	    && _LIB_VERSION != _IEEE_)
		return __kernel_standard (x, x, 25); /* sinh overflow */

	return z;
}
# ifdef NO_COMPAT_NEEDED
strong_alias (__sinh_compat, __sinh)
libm_alias_double (__sinh_compat, sinh)
# else
compat_symbol (libm, __sinh_compat, sinh, GLIBC_2_0);
#  ifdef NO_LONG_DOUBLE
weak_alias (__sinh_compat, sinhl)
#  endif
#  ifdef LONG_DOUBLE_COMPAT
LONG_DOUBLE_COMPAT_CHOOSE_libm_sinhl (
  compat_symbol (libm, __sinh_compat, sinhl, FIRST_VERSION_libm_sinhl), );
#  endif
# endif
#endif
