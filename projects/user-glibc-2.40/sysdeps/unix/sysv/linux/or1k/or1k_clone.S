/* clone helper __or1k_clone for OpenRISC.
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
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <sysdep.h>
#include <tls.h>
#define __ASSEMBLY__
#include <linux/sched.h>

	.text
ENTRY (__or1k_clone)

	/* To handle GCC varargs we need to use our __clone wrapper to pop
	   everything from the stack for us.
	   Now everything is placed in the registers which saves us a lot
	   of trouble.

	   The userland implementation is:

	     int clone (int (*fn)(void *), void *child_stack,
			int flags, void *arg, pid_t *ptid,
			struct user_desc *tls, pid_t *ctid);
	   The kernel entry is:

	     int clone (long flags, void *child_stack, int *parent_tid,
			int *child_tid, struct void *tls)

	     NB: tls isn't really an argument, it is read from r7 directly.  */

	/* First, align the stack to 4 bytes.  */
	l.xori	r11, r0, -4
	l.and	r4, r4, r11

	/* Put 'fn', 'arg' and 'flags' on the child stack.  */
	l.addi	r4, r4, -12
	l.sw	8(r4), r3
	l.sw	4(r4), r6
	l.sw	0(r4), r5

	l.ori	r3, r5, 0
	/* The child_stack is already in r4.  */
	l.ori	r5, r7, 0
	l.lwz	r6, 0(r1)
	l.ori	r7, r8, 0

	DO_CALL (clone)

	l.sfgeui r11, 0xf001
	l.bf	L(error)
	 l.nop

	/* If we are not the child, return the pid.  */
	l.sfeqi	r11, 0
	l.bf	L(thread_start)
	 l.nop

	l.jr	r9
	 l.nop

L(thread_start):
	/* Load function from stack.  */
	l.lwz	r11, 8(r1)
	l.jalr	r11
	 l.lwz	r3, 4(r1)

	/* Exit the child thread.  */
	l.ori	r3, r11, 0
	DO_CALL (exit)

L(error):
	l.j	SYSCALL_ERROR_NAME
	 l.ori	r3, r11, 0

END (__or1k_clone)
