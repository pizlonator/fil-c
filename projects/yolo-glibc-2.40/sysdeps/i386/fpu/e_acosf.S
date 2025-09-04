/*
 * Public domain.
 */

#include <machine/asm.h>
#include <libm-alias-finite.h>

RCSID("$NetBSD: $")

/* acos = atan (sqrt(1 - x^2) / x) */
ENTRY(__ieee754_acosf)
	flds	4(%esp)			/* x */
	fld	%st
	fmul	%st(0)			/* x^2 */
	fld1
	fsubp				/* 1 - x^2 */
	fsqrt				/* sqrt (1 - x^2) */
	fabs
	fxch	%st(1)
	fpatan
	ret
END (__ieee754_acosf)
libm_alias_finite (__ieee754_acosf, __acosf)
