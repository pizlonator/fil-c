/*
 * Public domain.
 */

#include <machine/asm.h>
#include <libm-alias-finite.h>

ENTRY(__ieee754_fmodf)
	flds	8(%esp)
	flds	4(%esp)
1:	fprem
	fstsw	%ax
	sahf
	jp	1b
	fstp	%st(1)
	ret
END(__ieee754_fmodf)
libm_alias_finite (__ieee754_fmodf, __fmodf)
