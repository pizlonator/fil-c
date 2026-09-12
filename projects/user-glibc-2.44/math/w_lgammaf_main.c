/* w_lgammaf.c -- float version of w_lgamma.c.
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

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-float.h>

#include <lgamma-compat.h>

#if BUILD_LGAMMA
float
LGFUNC (__lgammaf) (float x)
{
	float y = CALL_LGAMMA (float, __ieee754_lgammaf_r, x);
	if(__builtin_expect(!isfinite(y), 0)
	   && isfinite(x) && _LIB_VERSION != _IEEE_)
		return __kernel_standard_f(x, x,
					   floorf(x)==x&&x<=0.0f
					   ? 115 /* lgamma pole */
					   : 114); /* lgamma overflow */

	return y;
}
compat_symbol (libm, LGFUNC (__lgammaf), lgammaf, LGAMMA_OLD_VER);
# if GAMMA_ALIAS
strong_alias (LGFUNC (__lgammaf), __gammaf)
weak_alias (__gammaf, gammaf)
# endif
#endif
