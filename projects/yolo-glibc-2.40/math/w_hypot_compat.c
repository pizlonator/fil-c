/* @(#)w_hypot.c 5.1 93/09/24 */
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
 * wrapper hypot(x,y)
 */

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-double.h>


#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_35)
double
__hypot_compat (double x, double y)
{
	double z = __ieee754_hypot(x,y);
	if(__builtin_expect(!isfinite(z), 0)
	   && isfinite(x) && isfinite(y) && _LIB_VERSION != _IEEE_)
	    return __kernel_standard(x, y, 4); /* hypot overflow */

	return z;
}
compat_symbol (libm, __hypot_compat, hypot, GLIBC_2_0);
# ifdef NO_LONG_DOUBLE
weak_alias (__hypot_compat, hypotl)
# endif
# ifdef LONG_DOUBLE_COMPAT
LONG_DOUBLE_COMPAT_CHOOSE_libm_hypotl (
  compat_symbol (libm, __hypot_compat, hypotl, FIRST_VERSION_libm_hypotl), );
# endif
#endif
