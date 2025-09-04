/* Install given context.
   Copyright (C) 2001-2024 Free Software Foundation, Inc.
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

#include "ucontext_i.h"


ENTRY(__setcontext)
	/* Load address of the context data structure.  */
	movl	4(%esp), %eax

	/* Get the current signal mask.  Note that we preserve EBX in case
	   the system call fails and we return from the function with an
	   error.  */
	pushl	%ebx
	cfi_adjust_cfa_offset (4)
	xorl	%edx, %edx
	leal	oSIGMASK(%eax), %ecx
	movl	$SIG_SETMASK, %ebx
	cfi_rel_offset (ebx, 0)
	movl	$__NR_sigprocmask, %eax
	ENTER_KERNEL
	popl	%ebx
	cfi_adjust_cfa_offset (-4)
	cfi_restore (ebx)
	cmpl	$-4095, %eax		/* Check %eax for error.  */
	jae	SYSCALL_ERROR_LABEL	/* Jump to error handler if error.  */

	/* EAX was modified, reload it.  */
	movl	4(%esp), %eax

	/* Restore the floating-point context.  Not the registers, only the
	   rest.  */
	movl	oFPREGS(%eax), %ecx
	fldenv	(%ecx)

	/* Restore the FS segment register.  We don't touch the GS register
	   since it is used for threads.  */
	movl	oFS(%eax), %ecx
	movw	%cx, %fs

	/* Fetch the address to return to.  */
	movl	oEIP(%eax), %ecx

	/* Load the new stack pointer.  */
	cfi_def_cfa (eax, 0)
	cfi_offset (edi, oEDI)
	cfi_offset (esi, oESI)
	cfi_offset (ebp, oEBP)
	cfi_offset (ebx, oEBX)
	movl	oESP(%eax), %esp

	/* Push the return address on the new stack so we can return there.  */
	pushl	%ecx

	/* Load the values of all the preserved registers (except ESP).  */
	movl	oEDI(%eax), %edi
	movl	oESI(%eax), %esi
	movl	oEBP(%eax), %ebp
	movl	oEBX(%eax), %ebx

	/* All done, return 0 for success.  */
	xorl	%eax, %eax

	/* End FDE here, we fall into another context.  */
	cfi_endproc
	cfi_startproc

	/* The following 'ret' will pop the address of the code and jump
	   to it.  */

	ret
PSEUDO_END(__setcontext)
libc_hidden_def (__setcontext)

weak_alias (__setcontext, setcontext)
