/* Cancellable syscall wrapper.  Linux/hppa version.
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
   License along with the GNU C Library.  If not, see
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

	.text
ENTRY(__syscall_cancel_arch)
	stw	%r2,-20(%r30)
	ldo	128(%r30),%r30
	cfi_def_cfa_offset (-128)
	cfi_offset (2, -20)
	ldw	-180(%r30),%r28
	copy	%r26,%r20
	stw	%r28,-108(%r30)
	ldw	-184(%r30),%r28
	copy	%r24,%r26
	stw	%r28,-112(%r30)
	ldw	-188(%r30),%r28
	stw	%r28,-116(%r30)
	ldw	-192(%r30),%r28
	stw	%r4,-104(%r30)
	stw	%r28,-120(%r30)
	copy	%r25,%r28
	copy	%r23,%r25
#ifdef __PIC__
	stw	%r19,-32(%r30)
#endif
	cfi_offset (4, 24)

	.global __syscall_cancel_arch_start
__syscall_cancel_arch_start:
	ldw	0(%r20),%r20
	bb,<	%r20,31-TCB_CANCELED_BIT,1f
	ldw	-120(%r30),%r21
	ldw	-116(%r30),%r22
	ldw	-112(%r30),%r23
	ldw	-108(%r30),%r24
	copy	%r19, %r4
	ble	0x100(%sr2, %r0)

	.global __syscall_cancel_arch_end
__syscall_cancel_arch_end:

	copy	%r28,%r20
	copy	%r4,%r19

	ldw	-148(%r30),%r2
	ldw	-104(%r30),%r4
	bv	%r0(%r2)
	ldo	-128(%r30),%r30
1:
	bl	__syscall_do_cancel,%r2
	nop
	nop

END(__syscall_cancel_arch)
