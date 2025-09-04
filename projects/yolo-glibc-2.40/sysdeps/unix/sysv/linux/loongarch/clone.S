/* The clone syscall wrapper.
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

/* clone() is even more special than fork() as it mucks with stacks
   and invokes a function in the right context after its all over.  */

#include <sys/asm.h>
#include <sysdep.h>
#define _ERRNO_H  1
#include <bits/errno.h>
#include <tls.h>
#include "tcb-offsets.h"

/* int clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg,
   void *parent_tidptr, void *tls, void *child_tidptr) */

ENTRY (__clone)

	/* Align stack to 16.  */
	BSTRINS		a1, zero, 3, 0

	/* Sanity check arguments.  */
	beqz		a0, L (invalid) /* No NULL function pointers.  */
	beqz		a1, L (invalid) /* No NULL stack pointers.  */

	ADDI 		a1, a1, -16 /* Reserve argument save space.  */
	REG_S		a0, a1, 0   /* Save function pointer.  */
	REG_S		a3, a1, SZREG   /* Save argument pointer.  */

	/* The syscall expects the args to be in different slots.  */
	or		a0, a2, zero
	or		a2, a4, zero
	or		a3, a6, zero
	or		a4, a5, zero

	/* Do the system call.  */
	LI		a7,__NR_clone
	syscall		0

	blt		a0, zero ,L (error)
	beqz		a0,L (thread_start)

	/* Successful return from the parent.  */
	ret

L (invalid):
	LI		a0, -EINVAL

	/* Something bad happened -- no child created.  */
L (error):
	b		__syscall_error

END (__clone)

/* Load up the arguments to the function.  Put this block of code in
   its own function so that we can terminate the stack trace with our
   debug info.  */
ENTRY (__thread_start)
L (thread_start):

/* Terminate call stack by noting ra is undefined.  Use a dummy
   .cfi_label to force starting the FDE.  */
	.cfi_label .Ldummy
	cfi_undefined (1)

	/* Restore the arg for user's function.  */
	REG_L		a1, sp, 0   /* Function pointer.  */
	REG_L		a0, sp, SZREG   /* Argument pointer.  */

	/* Call the user's function.  */
	jirl		ra, a1, 0

	/* Call exit with the function's return value.  */
	LI		a7, __NR_exit
	syscall		0

	END (__thread_start)

libc_hidden_def (__clone)
weak_alias (__clone, clone)
