/* w_sinhf.c -- float version of w_sinh.c.
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
 * wrapper sinhf(x)
 */

#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-float.h>

#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_43)
float
__sinh_compatf (float x)
{
	float z = __ieee754_sinhf (x);
	if (__builtin_expect (!isfinite (z), 0) && isfinite (x)
	    && _LIB_VERSION != _IEEE_)
	    return __kernel_standard_f (x, x, 125); /* sinhf overflow */

	return z;
}
# ifdef NO_COMPAT_NEEDED
strong_alias (__sinh_compatf, __sinhf)
libm_alias_float (__sinh_compat, sinh)
# else
compat_symbol (libm, __sinh_compatf, sinhf, GLIBC_2_0);
# endif
#endif
