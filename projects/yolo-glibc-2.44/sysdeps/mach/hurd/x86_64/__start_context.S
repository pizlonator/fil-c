/* Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

/* This is the helper code which gets called if a function which is
   registered with 'makecontext' returns.  In this case we have to
   install the context listed in the uc_link element of the context
   'makecontext' manipulated at the time of the 'makecontext' call.
   If the pointer is NULL the process must terminate.  */


ENTRY(__start_context)
	/* This removes the parameters passed to the function given to
	   'makecontext' from the stack.  RBX contains the address
	   on the stack pointer for the next context.  */
	movq	%rbx, %rsp

	/* Don't use pop here so that stack is aligned to 16 bytes.  */
	movq	(%rsp), %rdi		/* This is the next context.  */
	testq	%rdi, %rdi
	je	2f			/* If it is zero exit.  */

	call	__setcontext
	/* If this returns (which can happen if __sigprocmask fails) we'll
	   exit the program with the return error value (-1).  */
	movq	%rax,%rdi

2:
	call	HIDDEN_JUMPTARGET(exit)
	/* The 'exit' call should never return.  In case it does cause
	   the process to terminate.  */
L(hlt):
	hlt
END(__start_context)
