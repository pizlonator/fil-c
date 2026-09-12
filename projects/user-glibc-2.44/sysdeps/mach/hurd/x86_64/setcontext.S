/* Install given context.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
	/* Save argument since call will destroy it.  */
	pushq	%rdi
	cfi_adjust_cfa_offset(8)

	/* Set the signal mask with
	   __sigprocmask (SIG_SETMASK, mask, NULL).  */
	xorl	%edx, %edx
	leaq	oSIGMASK(%rdi), %rsi
	movl	$SIG_SETMASK, %edi
	call	HIDDEN_JUMPTARGET (__sigprocmask)
	/* Pop the pointer into RDX. The choice is arbitrary, but
	   leaving RDI and RSI available for use later can avoid
	   shuffling values.  */
	popq	%rdx

	test	%rax, %rax
	jne	L(pseudo_end)

	/* Restore the floating-point context.  Not the registers, only the
	   rest.  */
	movq	oFPREGS(%rdx), %rcx
	fldenv	(%rcx)
	ldmxcsr oMXCSR(%rdx)

	/* Load the new stack pointer, the preserved registers and
	   registers used for passing args.  */
	cfi_def_cfa(%rdx, 0)
	cfi_offset(%rbx,oRBX)
	cfi_offset(%rbp,oRBP)
	cfi_offset(%r12,oR12)
	cfi_offset(%r13,oR13)
	cfi_offset(%r14,oR14)
	cfi_offset(%r15,oR15)
	cfi_offset(%rsp,oRSP)
	cfi_offset(%rip,oRIP)

	movq	oRSP(%rdx), %rsp
	movq	oRBX(%rdx), %rbx
	movq	oRBP(%rdx), %rbp
	movq	oR12(%rdx), %r12
	movq	oR13(%rdx), %r13
	movq	oR14(%rdx), %r14
	movq	oR15(%rdx), %r15

	/* The following ret should return to the address set with
	getcontext.  Therefore push the address on the stack.  */
	movq	oRIP(%rdx), %rcx
	pushq	%rcx

	movq	oRSI(%rdx), %rsi
	movq	oRDI(%rdx), %rdi
	movq	oRCX(%rdx), %rcx
	movq	oR8(%rdx), %r8
	movq	oR9(%rdx), %r9

	/* Setup finally %rdx.  */
	movq	oRDX(%rdx), %rdx

	/* End FDE here, we fall into another context.  */
	cfi_endproc
	cfi_startproc

	/* Clear rax to indicate success.  */
	xorl	%eax, %eax
L(pseudo_end):
	/* The following 'ret' will pop the address of the code and jump
	   to it.  */
	ret
PSEUDO_END(__setcontext)
libc_hidden_def (__setcontext)

weak_alias (__setcontext, setcontext)
