/* Copyright (C) 2001-2024 Free Software Foundation, Inc.
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

#include <sigaltstack-offsets.h>

	.section .rodata.str1.1,"aMS",@progbits,1
	.type	longjmp_msg,@object
longjmp_msg:
	.string "longjmp causes uninitialized stack frame"
	.size	longjmp_msg, .-longjmp_msg

#ifdef PIC
# define LOAD_MSG	lea	longjmp_msg(%rip), %RDI_LP
#else
# define LOAD_MSG	mov	$longjmp_msg, %RDI_LP
#endif

#define CHECK_INVALID_LONGJMP \
	cmp	%R8_LP, %RSP_LP;					\
	jbe	.Lok;							\
	/* Save function parameters.  */				\
	movq	%rdi, %r10;						\
	cfi_register (%rdi, %r10);					\
	movl	%esi, %ebx;						\
	cfi_register (%rsi, %rbx);					\
	xorl	%edi, %edi;						\
	lea	-sizeSS(%rsp), %RSI_LP;					\
	movl	$__NR_sigaltstack, %eax;				\
	syscall;							\
	/* Without working sigaltstack we cannot perform the test.  */	\
	testl	%eax, %eax;						\
	jne	.Lok2;							\
	testl	$1, (-sizeSS + oSS_FLAGS)(%rsp);			\
	jz	.Lfail;							\
	mov	(-sizeSS + oSS_SP)(%rsp), %RAX_LP;			\
	add	(-sizeSS + oSS_SIZE)(%rsp), %RAX_LP;			\
	sub	%R8_LP, %RAX_LP;					\
	cmp	(-sizeSS + oSS_SIZE)(%rsp), %RAX_LP;			\
	jae	.Lok2;							\
.Lfail:									\
	sub	$8, %RSP_LP;						\
	cfi_remember_state;						\
	cfi_def_cfa_offset(16);						\
	LOAD_MSG;							\
	call	HIDDEN_JUMPTARGET(__fortify_fail);			\
	cfi_restore_state;						\
	.p2align 3, 5;								\
.Lok2:									\
	movq	%r10, %rdi;						\
	cfi_restore (%rdi);						\
	movl	%ebx, %esi;						\
	cfi_restore (%rsi);						\
.Lok:

#define __longjmp ____longjmp_chk
#include <__longjmp.S>
