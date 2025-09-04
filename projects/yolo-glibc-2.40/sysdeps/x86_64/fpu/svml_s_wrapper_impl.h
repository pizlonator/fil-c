/* Wrapper implementations of vector math functions.
   Copyright (C) 2014-2024 Free Software Foundation, Inc.
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

/* SSE2 ISA version as wrapper to scalar.  */
.macro WRAPPER_IMPL_SSE2 callee
	push	%rbx
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbx, 0)
	subq	$16, %rsp
	cfi_adjust_cfa_offset (16)
	movaps	%xmm0, (%rsp)
	call	JUMPTARGET(\callee)
	movss	%xmm0, (%rsp)
	movss	4(%rsp), %xmm0
	call	JUMPTARGET(\callee)
	movss	%xmm0, 4(%rsp)
	movss	8(%rsp), %xmm0
	call	JUMPTARGET(\callee)
	movd	%xmm0, %ebx
	movss	12(%rsp), %xmm0
	call	JUMPTARGET(\callee)
	movd	%ebx, %xmm1
	unpcklps %xmm0, %xmm1
	movsd	(%rsp), %xmm0
	unpcklpd %xmm1, %xmm0
	addq	$16, %rsp
	cfi_adjust_cfa_offset (-16)
	popq	%rbx
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbx)
	ret
.endm

/* 2 argument SSE2 ISA version as wrapper to scalar.  */
.macro WRAPPER_IMPL_SSE2_ff callee
	push	%rbx
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbx, 0)
	subq	$32, %rsp
	cfi_adjust_cfa_offset (40)
	movaps	%xmm0, (%rsp)
	movaps	%xmm1, 16(%rsp)
	call	JUMPTARGET(\callee)
	movss	20(%rsp), %xmm1
	movss	%xmm0, 0(%rsp)
	movss	4(%rsp), %xmm0
	call	JUMPTARGET(\callee)
	movss	24(%rsp), %xmm1
	movss	%xmm0, 4(%rsp)
	movss	8(%rsp), %xmm0
	call	JUMPTARGET(\callee)
	movss	28(%rsp), %xmm1
	movd	%xmm0, %ebx
	movss	12(%rsp), %xmm0
	call	JUMPTARGET(\callee)
	/* merge 4x results into xmm0.  */
	movd	%ebx, %xmm1
	unpcklps %xmm0, %xmm1
	movsd	(%rsp), %xmm0
	unpcklpd %xmm1, %xmm0
	addq	$32, %rsp
	cfi_adjust_cfa_offset (-32)
	popq	%rbx
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbx)
	ret
.endm

/* 3 argument SSE2 ISA version as wrapper to scalar.  */
.macro WRAPPER_IMPL_SSE2_fFF callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	pushq	%rbx
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbx, 0)
	movq	%rdi, %rbp
	movq	%rsi, %rbx
	subq	$24, %rsp
	cfi_adjust_cfa_offset (24)
	movaps	%xmm0, (%rsp)
	call	JUMPTARGET(\callee)
	movss	4(%rsp), %xmm0
	leaq	4(%rbp), %rdi
	leaq	4(%rbx), %rsi
	call	JUMPTARGET(\callee)
	movss	8(%rsp), %xmm0
	leaq	8(%rbp), %rdi
	leaq	8(%rbx), %rsi
	call	JUMPTARGET(\callee)
	movss	12(%rsp), %xmm0
	leaq	12(%rbp), %rdi
	leaq	12(%rbx), %rsi
	call	JUMPTARGET(\callee)
	addq	$24, %rsp
	cfi_adjust_cfa_offset (-24)
	popq	%rbx
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbx)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm

#include "svml_sd_wrapper_impl.h"
