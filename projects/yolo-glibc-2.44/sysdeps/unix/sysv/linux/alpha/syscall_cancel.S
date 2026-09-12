/* Cancellable syscall wrapper.  Linux/alpha version.
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

/* long int __syscall_cancel_arch (int *ch,
				   __syscall_arg_t nr,
				   __syscall_arg_t arg1,
				   __syscall_arg_t arg2,
				   __syscall_arg_t arg3,
				   __syscall_arg_t arg4,
				   __syscall_arg_t arg5,
				   __syscall_arg_t arg6)  */

	.set noreorder
	.set noat
	.set nomacro
ENTRY (__syscall_cancel_arch)
	.frame	sp, 16, ra, 0
	.mask	0x4000000,-16
	cfi_startproc
	ldah	gp, 0(t12)
	lda	gp, 0(gp)
	lda	sp, -16(sp)
	cfi_def_cfa_offset (16)
	mov	a1, v0
	stq	ra, 0(sp)
	cfi_offset (26, -16)
	.prologue 1

	.global	__syscall_cancel_arch_start
__syscall_cancel_arch_start:
	ldl	t0, 0(a0)
	addl	zero, t0, t0
	/* if (*ch & CANCELED_BITMASK)  */
	and	t0, TCB_CANCELED_BITMASK, t0
	bne	t0, 1f
	mov	a2, a0
	mov	a3, a1
	mov	a4, a2
	ldq	a4, 16(sp)
	mov	a5, a3
	ldq	a5, 24(sp)
	.set	macro
	callsys
	.set	nomacro

	.global __syscall_cancel_arch_end
__syscall_cancel_arch_end:
	subq	zero, v0, t0
	ldq	ra, 0(sp)
	cmovne	a3, t0, v0
	lda	sp, 16(sp)
	cfi_remember_state
	cfi_restore (26)
	cfi_def_cfa_offset (0)
	ret	zero, (ra), 1
	.align 4
1:
	cfi_restore_state
	ldq 	t12, __syscall_do_cancel(gp)		!literal!2
	jsr 	ra, (t12), __syscall_do_cancel		!lituse_jsr!2
	cfi_endproc
END (__syscall_cancel_arch)
