/* The clone3 syscall wrapper.  Linux/aarch64 version.
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
   x0: cl_args
   x1: size
   x2: func
   x3: arg  */

        .text
ENTRY(__clone3)
	/* Save args for the child.  */
	mov	x10, x0		/* cl_args  */
	mov	x11, x2		/* func	 */
	mov	x12, x3		/* args  */

	/* Sanity check args.  */
	mov	x0, #-EINVAL
	cbz	x10, .Lsyscall_error	/* No NULL cl_args pointer.  */
	cbz	x2, .Lsyscall_error	/* No NULL function pointer.  */

    /* Clear ZA state of SME.  */
	CALL_LIBC_ARM_ZA_DISABLE

	/* Do the system call, the kernel expects:
	   x8: system call number
	   x0: cl_args
	   x1: size  */
	mov	x0, x10
	mov	x8, #SYS_ify(clone3)
	svc	0x0

	cmp	x0, #0
	beq	thread_start
	blt	.Lsyscall_error
	RET
PSEUDO_END (__clone3)

	.align 4
	.type thread_start, %function
thread_start:
	cfi_startproc
	cfi_undefined (x30)
	mov	x29, 0

	/* Pick the function arg and execute.  */
	mov	x0, x12
	blr	x11

	/* We are done, pass the return value through x0.  */
	mov	x8, #SYS_ify(exit)
	svc	0x0
	cfi_endproc
	.size thread_start, .-thread_start

libc_hidden_def (__clone3)
