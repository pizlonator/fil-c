/* The clone3 syscall wrapper.  Linux/arc version.
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
#define _ERRNO_H	1
#include <bits/errno.h>

/* The userland implementation is:
   int clone3 (struct clone_args *cl_args, size_t size,
               int (*func)(void *arg), void *arg);

   the kernel entry is:
   int clone3 (struct clone_args *cl_args, size_t size);

   The parameters are passed in registers from userland:
   r0: cl_args
   r1: size
   r2: func
   r3: arg  */

ENTRY(__clone3)

	/* Save args for the child.  */
	mov	r10, r0		/* cl_args  */
	mov	r11, r2		/* func	 */
	mov	r12, r3		/* args  */

	/* Sanity check args.  */
	breq	r10, 0, L (__sys_err)	/* No NULL cl_args pointer.  */
	breq	r11, 0, L (__sys_err)	/* No NULL function pointer.  */

	/* Do the system call, the kernel expects:
	   r8: system call number
	   r0: cl_args
	   r1: size  */
	mov	r0, r10
	mov	r8, __NR_clone3
	ARC_TRAP_INSN

	cmp	r0, 0
	beq	thread_start_clone3	/* Child returns.  */
	blt	L (__sys_err2)
	j	[blink]			/* Parent returns.  */

L (__sys_err):
	mov	r0, -EINVAL
L (__sys_err2):
	b	__syscall_error
PSEUDO_END (__clone3)


	.align 4
	.type thread_start_clone3, %function
thread_start_clone3:
	cfi_startproc
	/* Terminate call stack by noting ra is undefined.  */
	cfi_undefined (blink)

	/* Child jumps off to @fn with @arg as argument.  */
	jl.d	[r11]
	mov	r0, r12

	/* exit() with result from @fn (already in r0).  */
	mov	r8, __NR_exit
	ARC_TRAP_INSN

	/* In case it ever came back.  */
	flag	1

	cfi_endproc
	.size thread_start_clone3, .-thread_start_clone3

libc_hidden_def (__clone3)
