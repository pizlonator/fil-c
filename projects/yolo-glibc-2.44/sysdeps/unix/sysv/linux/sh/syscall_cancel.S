/* Cancellable syscall wrapper.  Linux/sh version.
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
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <descr-const.h>

/* long int __syscall_cancel_arch (int *cancelhandling,
				   long int nr,
				   long int arg1,
				   long int arg2,
				   long int arg3,
				   long int arg4,
				   long int arg5,
				   long int arg6)  */

ENTRY (__syscall_cancel_arch)

#ifdef SHARED
	mov.l	r12,@-r15
	cfi_def_cfa_offset (4)
	cfi_offset (12, -4)
	mova	L(GT),r0
	mov.l	L(GT),r12
	sts.l	pr,@-r15
	cfi_def_cfa_offset (8)
	cfi_offset (17, -8)
	add	r0,r12
#else
	sts.l	pr,@-r15
	cfi_def_cfa_offset (4)
	cfi_offset (17, -4)
#endif

	.globl __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	/* if (*cancelhandling & CANCELED_BITMASK)
	     __syscall_do_cancel()  */
	mov.l	@r4,r0
	tst	#TCB_CANCELED_BITMASK,r0
	bf/s	1f

	/* Issue a 6 argument syscall.  */
	mov	r5,r3
	mov	r6,r4
	mov	r7,r5
#ifdef SHARED
	mov.l	@(8,r15),r6
	mov.l	@(12,r15),r7
	mov.l	@(16,r15),r0
	mov.l	@(20,r15),r1
#else
	mov.l	@(4,r15),r6
	mov.l	@(8,r15),r7
	mov.l	@(12,r15),r0
	mov.l	@(16,r15),r1
#endif
	trapa	#0x16

	.globl __syscall_cancel_arch_end
__syscall_cancel_arch_end:

	/* The additional or is a workaround for a hardware issue:
	   http://documentation.renesas.com/eng/products/mpumcu/tu/tnsh7456ae.pdf
	 */
	or	r0,r0
	or	r0,r0
	or	r0,r0
	or	r0,r0
	or	r0,r0

	lds.l	@r15+,pr
	cfi_remember_state
	cfi_restore (17)
#ifdef SHARED
	cfi_def_cfa_offset (4)
	rts
	mov.l	@r15+,r12
	cfi_def_cfa_offset (0)
	cfi_restore (12)
	.align 1
1:
	cfi_restore_state
	mov.l	L(SC),r1
	bsrf	r1
L(M):
	nop

	.align 2
L(GT):
	.long	_GLOBAL_OFFSET_TABLE_
L(SC):
	.long	__syscall_do_cancel-(L(M)+2)
#else
	cfi_def_cfa_offset (0)
	rts
	nop

	.align 1
1:
	cfi_restore_state
	mov.l	2f,r1
	jsr	@r1
	nop

	.align 2
2:
	.long	__syscall_do_cancel
#endif

END (__syscall_cancel_arch)
