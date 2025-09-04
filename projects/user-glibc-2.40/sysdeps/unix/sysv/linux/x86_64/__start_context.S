/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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

#if SHSTK_ENABLED
# include <asm/prctl.h>
# include "ucontext_i.h"

/* Use CALL to push __start_context onto the new stack as well as the new
   shadow stack.  RDI points to ucontext:
   Incoming:
     __ssp[0]: The new shadow stack pointer.
     __ssp[1]: The base address of the new shadow stack.
     __ssp[2]: The size of the new shadow stack.
 */

ENTRY(__push___start_context)
	/* Get the original shadow stack pointer.  */
	rdsspq	%rcx
	/* Save the original stack pointer.  */
	movq	%rsp, %rdx
	/* Load the top of the new stack into RSI.  */
	movq 	oRSP(%rdi), %rsi
	/* Add 8 bytes to RSI since CALL will push the 8-byte return
	   address onto stack.  */
	leaq	8(%rsi), %rsp
	/* The size of the new shadow stack is stored in __ssp[2].  */
	mov	(oSSP + 16)(%rdi), %RSI_LP
	/* The new shadow stack base is stored in __ssp[1].  */
	mov	(oSSP + 8)(%rdi), %RAX_LP
	/* Use the restore stoken to restore the new shadow stack.  */
	rstorssp -8(%rax, %rsi)

	/* Save the restore token on the original shadow stack.  */
	saveprevssp

	/* Push the address of "jmp __start_context" onto the new stack
	   as well as the new shadow stack.  */
	call	1f
	jmp	__start_context
1:

	/* Use the restore stoken to restore the original shadow stack.  */
	rstorssp -8(%rcx)

	/* Save the restore token on the new shadow stack.  */
	saveprevssp

	/* Restore the original stack.  */
	mov	%rdx, %rsp
	ret
END(__push___start_context)
#endif

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
	/* If this returns (which can happen if the syscall fails) we'll
	   exit the program with the return error value (-1).  */
	movq	%rax,%rdi

2:
	call	HIDDEN_JUMPTARGET(exit)
	/* The 'exit' call should never return.  In case it does cause
	   the process to terminate.  */
L(hlt):
	hlt
END(__start_context)
