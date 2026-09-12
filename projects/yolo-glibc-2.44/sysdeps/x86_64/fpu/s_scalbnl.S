/*
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(__scalbnl)
	movl	%edi,-4(%rsp)
	fildl	-4(%rsp)
	fldt	8(%rsp)
	fscale
	fstp	%st(1)
	ret
END (__scalbnl)
