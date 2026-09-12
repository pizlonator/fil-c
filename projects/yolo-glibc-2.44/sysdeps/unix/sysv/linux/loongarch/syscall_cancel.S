/* Cancellable syscall wrapper.  Linux/loongarch version.
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

ENTRY (__syscall_cancel_arch)

	.global __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	/* if (*cancelhandling & CANCELED_BITMASK)
	     __syscall_do_cancel()  */
	ld.w	t0, a0, 0
	andi	t0, t0, TCB_CANCELED_BITMASK
	bne	t0, zero, 1f

	/* Issue a 6 argument syscall.  */
	move	t1, a1
	move	a0, a2
	move	a1, a3
	move	a2, a4
	move	a3, a5
	move	a4, a6
	move	a5, a7
	move	a7, t1
	syscall 0

	.global __syscall_cancel_arch_end
__syscall_cancel_arch_end:
	jr	ra
1:
	b	__syscall_do_cancel

END (__syscall_cancel_arch)
