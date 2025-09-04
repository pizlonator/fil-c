/* Common float/double wrapper implementations of vector math
   functions.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* AVX/AVX2 ISA version as wrapper to SSE ISA version.  */
.macro WRAPPER_IMPL_AVX callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	movq	%rsp, %rbp
	cfi_def_cfa_register (%rbp)
	andq	$-32, %rsp
	subq	$32, %rsp
	vmovaps	%ymm0, (%rsp)
	vzeroupper
	call	HIDDEN_JUMPTARGET(\callee)
	vmovaps	%xmm0, (%rsp)
	vmovaps	16(%rsp), %xmm0
	call	HIDDEN_JUMPTARGET(\callee)
	/* combine xmm0 (return of second call) with result of first
	   call (saved on stack). Might be worth exploring logic that
	   uses `vpblend` and reads in ymm1 using -16(rsp).  */
	vmovaps	(%rsp), %xmm1
	vinsertf128 $1, %xmm0, %ymm1, %ymm0
	movq	%rbp, %rsp
	cfi_def_cfa_register (%rsp)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm

/* 2 argument AVX/AVX2 ISA version as wrapper to SSE ISA version.  */
.macro WRAPPER_IMPL_AVX_ff callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	movq	%rsp, %rbp
	cfi_def_cfa_register (%rbp)
	andq	$-32, %rsp
	subq	$64, %rsp
	vmovaps	%ymm0, (%rsp)
	vmovaps	%ymm1, 32(%rsp)
	vzeroupper
	call	HIDDEN_JUMPTARGET(\callee)
	vmovaps	48(%rsp), %xmm1
	vmovaps	%xmm0, (%rsp)
	vmovaps	16(%rsp), %xmm0
	call	HIDDEN_JUMPTARGET(\callee)
	/* combine xmm0 (return of second call) with result of first
	   call (saved on stack). Might be worth exploring logic that
	   uses `vpblend` and reads in ymm1 using -16(rsp).  */
	vmovaps	(%rsp), %xmm1
	vinsertf128 $1, %xmm0, %ymm1, %ymm0
	movq	%rbp, %rsp
	cfi_def_cfa_register (%rsp)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm

/* 3 argument AVX/AVX2 ISA version as wrapper to SSE ISA version.  */
.macro WRAPPER_IMPL_AVX_fFF callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	movq	%rsp, %rbp
	andq	$-32, %rsp
	subq	$32, %rsp
	vmovaps	%ymm0, (%rsp)
	pushq	%rbx
	pushq	%r14
	movq	%rdi, %rbx
	movq	%rsi, %r14
	vzeroupper
	call	HIDDEN_JUMPTARGET(\callee)
	vmovaps	32(%rsp), %xmm0
	leaq	16(%rbx), %rdi
	leaq	16(%r14), %rsi
	call	HIDDEN_JUMPTARGET(\callee)
	popq	%r14
	popq	%rbx
	movq	%rbp, %rsp
	cfi_def_cfa_register (%rsp)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm

/* AVX512 ISA version as wrapper to AVX2 ISA version.  */
.macro WRAPPER_IMPL_AVX512 callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	movq	%rsp, %rbp
	cfi_def_cfa_register (%rbp)
	andq	$-64, %rsp
	subq	$64, %rsp
	vmovups	%zmm0, (%rsp)
	call	HIDDEN_JUMPTARGET(\callee)
	vmovupd	%ymm0, (%rsp)
	vmovupd	32(%rsp), %ymm0
	call	HIDDEN_JUMPTARGET(\callee)
	/* combine ymm0 (return of second call) with result of first
	   call (saved on stack).  */
	vmovaps	(%rsp), %ymm1
	vinserti64x4 $0x1, %ymm0, %zmm1, %zmm0
	movq	%rbp, %rsp
	cfi_def_cfa_register (%rsp)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm

/* 2 argument AVX512 ISA version as wrapper to AVX2 ISA version.  */
.macro WRAPPER_IMPL_AVX512_ff callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	movq	%rsp, %rbp
	cfi_def_cfa_register (%rbp)
	andq	$-64, %rsp
	addq	$-128, %rsp
	vmovups	%zmm0, (%rsp)
	vmovups	%zmm1, 64(%rsp)
	/* ymm0 and ymm1 are already set.  */
	call	HIDDEN_JUMPTARGET(\callee)
	vmovups	96(%rsp), %ymm1
	vmovaps	%ymm0, (%rsp)
	vmovups	32(%rsp), %ymm0
	call	HIDDEN_JUMPTARGET(\callee)
	/* combine ymm0 (return of second call) with result of first
	   call (saved on stack).  */
	vmovaps	(%rsp), %ymm1
	vinserti64x4 $0x1, %ymm0, %zmm1, %zmm0
	movq	%rbp, %rsp
	cfi_def_cfa_register (%rsp)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm

/* 3 argument AVX512 ISA version as wrapper to AVX2 ISA version.  */
.macro WRAPPER_IMPL_AVX512_fFF callee
	pushq	%rbp
	cfi_adjust_cfa_offset (8)
	cfi_rel_offset (%rbp, 0)
	movq	%rsp, %rbp
	cfi_def_cfa_register (%rbp)
	andq	$-64, %rsp
	subq	$64, %rsp
	vmovaps	%zmm0, (%rsp)
	pushq	%rbx
	pushq	%r14
	movq	%rdi, %rbx
	movq	%rsi, %r14
	/* ymm0 is already set.  */
	call	HIDDEN_JUMPTARGET(\callee)
	vmovaps	48(%rsp), %ymm0
	leaq	32(%rbx), %rdi
	leaq	32(%r14), %rsi
	call	HIDDEN_JUMPTARGET(\callee)
	popq	%r14
	popq	%rbx
	movq	%rbp, %rsp
	cfi_def_cfa_register (%rsp)
	popq	%rbp
	cfi_adjust_cfa_offset (-8)
	cfi_restore (%rbp)
	ret
.endm
