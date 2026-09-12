/* Copyright (C) 1999-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#define _ERRNO_H	1
#include <bits/errno.h>

/* Clone the calling process, but without copying the whole address space.
   The calling process is suspended until the new process exits or is
   replaced by a call to `execve'.  Return -1 for errors, 0 to the new process,
   and the process ID of the new process to the old process.  */

ENTRY (__vfork)
	ble	0x100(%sr2, %r0)
	ldi	__NR_vfork, %r20

	ldi	-4096, %r1
	comclr,<<	%r1, %ret0, %r0      /* Note: unsigned compare. */
	bv,n	%r0(%rp)

	/* Something bad happened -- no child created -- we need a frame */
	ldo	64(%sp), %sp
	.cfi_def_cfa_offset -64

	/* Set errno */
	bl	__syscall_error, %rp
	sub	%r0, %ret0, %arg0

	/* ret0 is set to -1 by __syscall_error */
	ldw	-84(%sp), %rp
	bv	%r0(%rp)
	ldo	-64(%sp), %sp

PSEUDO_END (__vfork)
libc_hidden_def (__vfork)

weak_alias (__vfork, vfork)
strong_alias (__vfork, __libc_vfork)
