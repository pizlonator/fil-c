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

	/* Clear ZA state of SME.  */
	CALL_LIBC_ARM_ZA_DISABLE

	mov	x0, #0x4111	/* CLONE_VM | CLONE_VFORK | SIGCHLD */
	mov	x1, sp
	DO_CALL (clone, 2)

	cmn	x0, #4095
	b.cs    .Lsyscall_error
	cbz	x0, L(child)
	RET
L(child):
	/* Return with indirect branch in the child to support GCS.
	   Compilers insert BTI instruction after vfork() to make
	   sure return via BR works on systems with BTI.  */
	br	x30
PSEUDO_END (__vfork)
libc_hidden_def (__vfork)

weak_alias (__vfork, vfork)
strong_alias (__vfork, __libc_vfork)
