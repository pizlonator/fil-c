/* Cancellable syscall wrapper.  Linux/sparc32 version.
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
	save	%sp, -96, %sp

	cfi_window_save
	cfi_register (%o7, %i7)
	cfi_def_cfa_register (%fp)

	.globl __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	/* if (*cancelhandling & CANCELED_BITMASK)
	     __syscall_do_cancel()  */
	ld	[%i0], %g2
	andcc	%g2, TCB_CANCELED_BITMASK, %g0
#ifdef __sparcv9
	bne,pn	%icc, 2f
#else
	bne	2f
#endif
	/* Issue a 6 argument syscall.  */
	 mov	%i2, %o0
	mov	%i3, %o1
	mov	%i4, %o2
	mov	%i5, %o3
	ld	[%fp+92], %o4
	ld	[%fp+96], %o5
	 mov	%i1, %g1
	ta	0x10

	.globl __syscall_cancel_arch_end
__syscall_cancel_arch_end:
	bcc	1f
	 nop
	sub	%g0, %o0, %o0
1:
	mov	%o0, %i0
#ifdef __sparcv9
	return	%i7+8
	 nop
#else
	jmp	%i7+8
	 restore
#endif

2:
	call	__syscall_do_cancel, 0
	 nop
	nop

END (__syscall_cancel_arch)
