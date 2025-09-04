/* s_fabsl.c -- long double version of s_fabs.c.
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

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: $";
#endif

/*
 * fabsl(x) returns the absolute value of x.
 */

#include <math.h>
#include <math_private.h>
#include <libm-alias-ldouble.h>

_Float128
__fabsl (_Float128 x)
{
  return __builtin_fabsl (x);
}
libm_alias_ldouble (__fabs, fabs)
