/* Cancellable syscall wrapper.  Linux/AArch64 version.
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

/* long int [x0] __syscall_cancel_arch (int *cancelhandling [x0],
					long int nr   [x1],
					long int arg1 [x2],
					long int arg2 [x3],
					long int arg3 [x4],
					long int arg4 [x5],
					long int arg5 [x6],
					long int arg6 [x7])  */

ENTRY (__syscall_cancel_arch)

	.globl __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	/* if (*cancelhandling & CANCELED_BITMASK)
	     __syscall_do_cancel()  */
	ldr	w0, [x0]
	tbnz    w0, TCB_CANCELED_BIT, 1f

	/* Issue a 6 argument syscall, the nr [x1] being the syscall
	   number.  */
	mov	x8, x1
	mov	x0, x2
	mov	x1, x3
	mov	x2, x4
	mov	x3, x5
	mov	x4, x6
	mov	x5, x7
	svc	0x0

	.globl __syscall_cancel_arch_end
__syscall_cancel_arch_end:
	ret

1:
	b	__syscall_do_cancel

END (__syscall_cancel_arch)
