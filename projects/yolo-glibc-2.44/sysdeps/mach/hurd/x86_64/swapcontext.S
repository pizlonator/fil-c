/* Save current context and install the given one.
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


/* int __swapcontext (ucontext_t *oucp, const ucontext_t *ucp);

  Saves the machine context in oucp such that when it is activated,
  it appears as if __swapcontextt() returned again, restores the
  machine context in ucp and thereby resumes execution in that
  context.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to save anything
  other than the PRESERVED state.  */

ENTRY(__swapcontext)
	/* Save the preserved registers, the registers used for passing args,
	   and the return address.  */
	movq	%rbx, oRBX(%rdi)
	movq	%rbp, oRBP(%rdi)
	movq	%r12, oR12(%rdi)
	movq	%r13, oR13(%rdi)
	movq	%r14, oR14(%rdi)
	movq	%r15, oR15(%rdi)

	movq	%rdi, oRDI(%rdi)
	movq	%rsi, oRSI(%rdi)
	movq	%rdx, oRDX(%rdi)
	movq	%rcx, oRCX(%rdi)
	movq	%r8, oR8(%rdi)
	movq	%r9, oR9(%rdi)

	movq	(%rsp), %rcx
	movq	%rcx, oRIP(%rdi)
	leaq	8(%rsp), %rcx		/* Exclude the return address.  */
	movq	%rcx, oRSP(%rdi)

	/* We have separate floating-point register content memory on the
	   stack.  We use the __fpregs_mem block in the context.  Set the
	   links up correctly.  */
	leaq	oFPREGSMEM(%rdi), %rcx
	movq	%rcx, oFPREGS(%rdi)
	/* Save the floating-point environment.  */
	fnstenv	(%rcx)
	stmxcsr oMXCSR(%rdi)


	/* The function call destroys some registers, save ucp.  */
	movq	%rsi, %r12

	/* Save the current signal mask and install the new one with
	   __sigprocmask (SIG_BLOCK, newset, oldset).  */
	leaq	oSIGMASK(%rdi), %rdx
	leaq	oSIGMASK(%rsi), %rsi
	movl	$SIG_SETMASK, %edi
	call	HIDDEN_JUMPTARGET (__sigprocmask)
	test	%rax, %rax
	jne	L(pseudo_end)

	/* Restore destroyed register into RDX. The choice is arbitrary,
	   but leaving RDI and RSI available for use later can avoid
	   shuffling values.  */
	movq	%r12, %rdx

	/* Restore the floating-point context.  Not the registers, only the
	   rest.  */
	movq	oFPREGS(%rdx), %rcx
	fldenv	(%rcx)
	ldmxcsr oMXCSR(%rdx)

	/* Load the new stack pointer and the preserved registers.  */
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

	/* Setup registers used for passing args.  */
	movq	oRDI(%rdx), %rdi
	movq	oRSI(%rdx), %rsi
	movq	oRCX(%rdx), %rcx
	movq	oR8(%rdx), %r8
	movq	oR9(%rdx), %r9

	/* Setup finally %rdx.  */
	movq	oRDX(%rdx), %rdx

	/* Clear rax to indicate success.  */
	xorl	%eax, %eax
L(pseudo_end):
	ret
PSEUDO_END(__swapcontext)

weak_alias (__swapcontext, swapcontext)
