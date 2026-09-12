/* The clone3 syscall wrapper.  Linux/powerpc64 version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#define _ERRNO_H        1
#include <bits/errno.h>

/* The userland implementation is:
   int clone3 (struct clone_args *cl_args, size_t size,
               int (*func)(void *arg), void *arg);

   the kernel entry is:
   int clone3 (struct clone_args *cl_args, size_t size);

   The parameters are passed in registers from userland:
   r3: cl_args
   r4: size
   r5: func
   r6: arg  */

#ifdef USE_PPC_SCV
/* This is equivalent to DO_CALL_SCV, but we cannot use the macro here
   because it uses CFI directives and we just called cfi_endproc.  */
# define DO_CLONE3_SVC_CALL(jumpfalse)				\
	CHECK_SCV_SUPPORT r28 jumpfalse;			\
	mflr 	r31;						\
	.machine "push";					\
	.machine "power9";					\
	scv 	0;						\
	.machine "pop";						\
	mtlr 	r31;						\
	/* With scv an, an error is a value -4095 <= x < 0.  */	\
	cmpdi	cr1, r3, 0;					\
	b	1f;
#else
# define DO_CLONE3_SVC_CALL(fail_branch)
#endif

        .text
ENTRY(__clone3)
	CALL_MCOUNT 4

	/* Sanity checks args.  */
	cmpdi	cr0, r3, 0
	cmpdi	cr1, r5, 0
	cror	cr0*4+eq, cr1*4+eq, cr0*4+eq
	beq	cr0,L(badargs)

	/* Save some regs in the "red zone".  */
#ifdef USE_PPC_SCV
	std	r28, -32(r1)
	cfi_offset (r28, -32)
#endif
	std	r29, -24(r1)
	std	r30, -16(r1)
	std	r31, -8(r1)
	cfi_offset (r29, -24)
	cfi_offset (r30, -16)
	cfi_offset (r31, -8)

	/* Save func and arg across syscall.  */
	mr	r30, r5		/* Function in r30.  */
	mr	r29, r6		/* Argument in r29.  */

	/* End FDE now, because in the child the unwind info will be
	   wrong.  */
	cfi_endproc

	/* Setup a minimum stack frame for child.  It needs to first calculate
	   the effective stack address, write the start empty backchain pointer,
	   and update the frame size in the input cl_args.  */
	ld	r7, 40(r3)	/* Load stack value.  */
	ld	r8, 48(r3)	/* Load stack_size value.  */
	li	r10, 0
	add	r7, r7, r8	/* Calculate effective stack address.  */
	std	r10, -FRAME_MIN_SIZE_PARM(r7)
	addi	r8, r8, -FRAME_MIN_SIZE_PARM
	std	r8, 48(r3)

	/* Do the system call, the kernel expects:
	   r0: system call numer
	   r3: cl_args
	   r4: size  */
	li	r0, SYS_ify(clone3)
	DO_CLONE3_SVC_CALL (0f)
0:      DO_CALL_SC

	/* With sc, error is indicated by cr0.SO.  */
	cmpdi	cr1, r3, 0
	crandc	cr1*4+eq, cr1*4+eq, cr0*4+so

1:	bne-	cr1,L(parent)

	/* Child, load the function and arguments.  */
	std	r2, FRAME_TOC_SAVE(r1)
	PPC64_LOAD_FUNCPTR r30
	mr	r3, r29
	bctrl
	ld	r2, FRAME_TOC_SAVE(r1)

	li	r0, SYS_ify(exit)
	DO_CLONE3_SVC_CALL (2f)
2:	DO_CALL_SC
	/* We won't ever get here but provide a nop so that the linker
	   will insert a toc adjusting stub if necessary.  */
	nop

L(badargs):
	cfi_startproc
	li	r3, EINVAL
	TAIL_CALL_SYSCALL_ERROR

L(parent):
	/* Check if scv is available.  */
	cmpdi cr1, r28, 0

	/* Parent.  Restore registers & return.  */
#ifdef USE_PPC_SCV
	cfi_offset (r28, -32)
	ld	r28, -32(r1)
	cfi_restore (r28)
#endif
	cfi_offset (r29,-24)
	cfi_offset (r30,-16)
	cfi_offset (r31,-8)
	ld	r29, -24(r1)
	ld	r30, -16(r1)
	ld	r31, -8(r1)
	cfi_restore (r29)
	cfi_restore (r30)
	cfi_restore (r31)

#ifdef USE_PPC_SCV
	beq	cr1, 0f
	RET_SCV
	b	1f
#endif
0:	RET_SC
1:	TAIL_CALL_SYSCALL_ERROR

PSEUDO_END (__clone3)

libc_hidden_def (__clone3)
