/* longjmp support.  x86_64/Hurd version.
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
#include <pointer_guard.h>
#include <jmpbuf-offsets.h>
#include <asm-syntax.h>
#include <tcb-offsets.h>
#include <jmp_buf-ssp.h>
#include <signal-defines.h>

#define SS_ONSTACK 1

/* Don't restore shadow stack register if shadow stack isn't enabled.  */
#if !SHSTK_ENABLED
# undef SHADOW_STACK_POINTER_OFFSET
#endif

/* Jump to the position specified by ENV, causing the
   setjmp call there to return VAL, or 1 if VAL is 0.
   void __longjmp (__jmp_buf env, int val).  */
	.text
ENTRY(__longjmp)
	/* Restore registers.  */
	mov	(JB_RSP*8)(%rdi), %R8_LP
	mov	(JB_RBP*8)(%rdi),%R9_LP
	mov	(JB_PC*8)(%rdi), %RDX_LP
#ifdef PTR_DEMANGLE
	PTR_DEMANGLE (%R8_LP)
	PTR_DEMANGLE (%R9_LP)
	PTR_DEMANGLE (%RDX_LP)
#endif

#if !defined (SHARED) || IS_IN (rtld)
	cmpb	$0, __libc_tls_initialized(%rip)
	jz	.Lok		/* TLS not initialized yet */
#endif

	movq %fs:SIGSTATE_OFFSET, %R10_LP
	testq %R10_LP, %R10_LP
	jz	.Lok		/* sigstate not initialized yet */

	testl	$SS_ONSTACK, (HURD_SIGSTATE__SIGALTSTACK__OFFSET + SIGALTSTACK__SS_FLAGS__OFFSET)(%R10_LP)
	jz	.Lok

.Lonstack:
	/* We were on the alternate stack.  */

	cmpq	(HURD_SIGSTATE__SIGALTSTACK__OFFSET + SIGALTSTACK__SS_SP__OFFSET)(%R10_LP), %R8_LP
	jb	.Loks		/* Jumping below the altstack, switch */

	movq	(HURD_SIGSTATE__SIGALTSTACK__OFFSET + SIGALTSTACK__SS_SP__OFFSET)(%R10_LP), %R11_LP
	addq	(HURD_SIGSTATE__SIGALTSTACK__OFFSET + SIGALTSTACK__SS_SIZE__OFFSET)(%R10_LP), %R11_LP
	cmpq	%R11_LP, %R8_LP
	jb	.Lok		/* Jumping inside the altstack, do not switch */

	/* Jumping above the altstack, switch */

.Loks:
	andl	$~(SS_ONSTACK), (HURD_SIGSTATE__SIGALTSTACK__OFFSET + SIGALTSTACK__SS_FLAGS__OFFSET)(%R10_LP)

.Lok:
	/* We add unwind information for the target here.  */
	cfi_def_cfa(%rdi, 0)
	cfi_register(%rsp,%r8)
	cfi_register(%rbp,%r9)
	cfi_register(%rip,%rdx)
	cfi_offset(%rbx,JB_RBX*8)
	cfi_offset(%r12,JB_R12*8)
	cfi_offset(%r13,JB_R13*8)
	cfi_offset(%r14,JB_R14*8)
	cfi_offset(%r15,JB_R15*8)
	movq	(JB_RBX*8)(%rdi), %rbx
	movq	(JB_R12*8)(%rdi), %r12
	movq	(JB_R13*8)(%rdi), %r13
	movq	(JB_R14*8)(%rdi), %r14
	movq	(JB_R15*8)(%rdi), %r15
	/* Set return value for setjmp.  */
	movl	%esi, %eax
	mov	%R8_LP, %RSP_LP
	movq	%r9,%rbp
	jmpq	*%rdx
END (__longjmp)
