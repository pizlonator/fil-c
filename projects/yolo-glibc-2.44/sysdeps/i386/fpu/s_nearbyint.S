/*
 * Public domain.
 */

#include <machine/asm.h>
#include <libm-alias-double.h>

ENTRY(__nearbyint)
	fldl	4(%esp)
	subl	$32, %esp
	cfi_adjust_cfa_offset (32)
	fnstenv	4(%esp)
	frndint
	fldenv	4(%esp)
	addl	$32, %esp
	cfi_adjust_cfa_offset (-32)
	ret
END (__nearbyint)
libm_alias_double (__nearbyint, nearbyint)
