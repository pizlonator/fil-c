/* The clone3 syscall wrapper.  Linux/arm version.
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

        .text
ENTRY(__clone3)
	/* Sanity check args.  */
	cmp	r0, #0
	ite	ne
	cmpne	r1, #0
	moveq	r0, #-EINVAL
	beq	PLTJMP(syscall_error)

	/* Do the syscall, the kernel expects:
	   r7: system call number:
	   r0: cl_args
	   r1: size  */
	push    { r7 }
	cfi_adjust_cfa_offset (4)
	cfi_rel_offset (r7, 0)
	ldr     r7, =SYS_ify(clone3)
	swi	0x0
	cfi_endproc

	cmp	r0, #0
	beq	1f
	pop     {r7}
	blt	PLTJMP(C_SYMBOL_NAME(__syscall_error))
	RETINSTR(, lr)

	cfi_startproc
PSEUDO_END (__clone3)

1:
	.fnstart
	.cantunwind
	mov	r0, r3
	mov	ip, r2
	BLX (ip)

	/* And we are done, passing the return value through r0.  */
	ldr	r7, =SYS_ify(exit)
	swi	0x0

	.fnend

libc_hidden_def (__clone3)
