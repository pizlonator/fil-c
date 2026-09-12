/* w_hypotf.c -- float version of w_hypot.c.
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

/*
 * wrapper hypotf(x,y)
 */

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-float.h>


#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_35)
float
__hypotf_compat (float x, float y)
{
	float z = __ieee754_hypotf(x,y);
	if(__builtin_expect(!isfinite(z), 0)
	   && isfinite(x) && isfinite(y) && _LIB_VERSION != _IEEE_)
	    /* hypot overflow */
	    return __kernel_standard_f(x, y, 104);

	return z;
}
compat_symbol (libm, __hypotf_compat, hypotf, GLIBC_2_0);
#endif
