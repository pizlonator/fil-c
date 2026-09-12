/* The clone3 syscall wrapper.  Linux/RISC-V version.
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

#include <asm/errno.h>
#include <sys/asm.h>
#include <sysdep.h>

/* The userland implementation is:
   int clone3 (struct clone_args *cl_args, size_t size,
               int (*func)(void *arg), void *arg);

   the kernel entry is:
   int clone3 (struct clone_args *cl_args, size_t size);

   The parameters are passed in registers from userland:
   a0: cl_args
   a1: size
   a2: func
   a3: arg  */

        .text
ENTRY(__clone3)
	/* Sanity check args.  */
	beqz	a0, L(invalid)	/* No NULL cl_args pointer.  */
	beqz	a2, L(invalid)  /* No NULL function pointer.  */

	/* Do the system call, the kernel expects:
	   a7: system call number
	   a0: cl_args
	   a1: size  */
	li	a7, __NR_clone3
	scall

	bltz	a0, L(error)
	beqz	a0, L(thread_start)

	ret

L(invalid):
	li	a0, -EINVAL
L(error):
	tail	__syscall_error
END (__clone3)

ENTRY(__thread_start_clone3)
L(thread_start):
	/* Terminate call stack by noting ra is undefined.  Use a dummy
	   .cfi_label to force starting the FDE.  */
	.cfi_label .Ldummy
	cfi_undefined (ra)

	/* Restore the arg for user's function and call the user's
	   function.  */
	mv		a0, a3	/* Argument pointer.  */
	jalr		a2

	/* Call exit with the function's return value.  */
	li		a7, __NR_exit
	scall
END(__thread_start_clone3)

libc_hidden_def (__clone3)
