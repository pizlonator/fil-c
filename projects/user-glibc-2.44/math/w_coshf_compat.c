/* w_coshf.c -- float version of w_cosh.c.
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
 * wrapper coshf(x)
 */

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-float.h>
#include <shlib-compat.h>

#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_43)
float
__cosh_compatf (float x)
{
	float z = __ieee754_coshf (x);
	if (__builtin_expect (!isfinite (z), 0) && isfinite (x)
	    && _LIB_VERSION != _IEEE_)
		return __kernel_standard_f (x, x, 105); /* cosh overflow */

	return z;
}
# ifdef NO_COMPAT_NEEDED
strong_alias (__cosh_compatf, __coshf)
libm_alias_float (__cosh_compat, cosh)
# else
compat_symbol (libm, __cosh_compatf, coshf, GLIBC_2_0);
# endif
#endif
