/* The clone3 syscall wrapper.  Linux/s390x version.
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
   r2: cl_args
   r3: size
   r4: func
   r5: arg  */

        .text
ENTRY(__clone3)
	/* Sanity check args.  */
	ltgr	%r2, %r2
	je	error
	ltgr	%r4, %r4
	je	error

	/* Do the system call, the kernel expects:
	   r1: system call number
	   r2: cl_args
	   r3: size  */
	lghi	%r1, SYS_ify(clone3)
	svc	0
	ltgr	%r2,%r2			/* check return code */
	jz	thread_start
	jgm	SYSCALL_ERROR_LABEL
	br	%r14
error:
	lghi	%r2,-EINVAL
	jg	SYSCALL_ERROR_LABEL
PSEUDO_END (__clone3)

	.align 16
	.type thread_start, %function
thread_start:
	cfi_startproc
	/* Mark r14 as undefined in order to stop unwinding here.  */
	cfi_undefined (r14)

	/* func is in gpr 4, arg in gpr 5.  */
	lgr	%r2, %r5
	aghi	%r15, -160
	xc	0(8,%r15),0(%r15)
	basr	%r14, %r4

	DO_CALL (exit, 1)
	cfi_endproc
	ASM_SIZE_DIRECTIVE (thread_start)

libc_hidden_def (__clone3)
