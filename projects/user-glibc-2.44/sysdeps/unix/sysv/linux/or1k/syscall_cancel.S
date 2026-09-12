/* Cancellable syscall wrapper.  Linux/or1k version.
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
	l.addi	r1, r1, -4
	cfi_def_cfa_offset (4)
	l.sw	0(r1), r9
	cfi_offset (9, -4)

	.global __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	/* if (*cancelhandling & CANCELED_BITMASK)
	     __syscall_do_cancel()  */
	l.movhi	r19, hi(0)
	l.lwz	r17, 0(r3)
	l.andi	r17, r17, 8
	l.sfeq	r17, r19
	l.bnf	1f

	/* Issue a 6 argument syscall.  */
	l.or	r11, r4, r4
	l.or	r3, r5, r5
	l.or	r4, r6, r6
	l.or	r5, r7, r7
	l.or	r6, r8, r8
	l.lwz	r7, 4(r1)
	l.lwz	r8, 8(r1)
	l.sys	1
	 l.nop

	.global __syscall_cancel_arch_end
__syscall_cancel_arch_end:

	l.lwz	r9, 0(r1)
	l.jr	r9
	l.addi	r1, r1, 4
	cfi_remember_state
	cfi_def_cfa_offset (0)
	cfi_restore (9)
1:
	cfi_restore_state
	l.jal	__syscall_do_cancel
	 l.nop
END (__syscall_cancel_arch)
