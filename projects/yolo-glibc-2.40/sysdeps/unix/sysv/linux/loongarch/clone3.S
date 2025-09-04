/* The clone3 syscall wrapper.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.

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
   <https://www.gnu.org/licenses/>.  */

/* clone3() is even more special than fork() as it mucks with stacks
   and invokes a function in the right context after its all over.  */

#include <sys/asm.h>
#include <sysdep.h>
#define _ERRNO_H  1
#include <bits/errno.h>
#include <tls.h>
#include "tcb-offsets.h"

/* int clone3(struct clone_args *cl_args, size_t size,
   int (*func)(void *arg), void *arg); */

ENTRY (__clone3)

	/* Sanity check arguments.  */
	beqz		a0, L (invalid)	/* No NULL cl_args pointer.  */
	beqz		a2, L (invalid)	/* No NULL function pointer.  */

	/* Do the system call.  */
	LI		a7, __NR_clone3
	syscall		0

	blt		a0, zero ,L (error)
	beqz		a0, L (thread_start3)

	/* Successful return from the parent.  */
	ret

L (invalid):
	LI		a0, -EINVAL

	/* Something bad happened -- no child created.  */
L (error):
	b		__syscall_error

END (__clone3)

/* Load up the arguments to the function.  Put this block of code in
   its own function so that we can terminate the stack trace with our
   debug info.  */
ENTRY (__thread_start3)
L (thread_start3):

/* Terminate call stack by noting ra is undefined.  Use a dummy
   .cfi_label to force starting the FDE.  */
	.cfi_label .Ldummy
	cfi_undefined (1)

	/* Align stack to 16.  */
	BSTRINS		sp, zero, 3, 0

	/* Set up arguments for the function call.  */
	move		a0, a3		/* Argument.  */
	jirl		ra, a2, 0	/* Call function.  */

	/* Call exit with the function's return value.  */
	LI		a7, __NR_exit
	syscall		0

	END (__thread_start3)

libc_hidden_def (__clone3)
