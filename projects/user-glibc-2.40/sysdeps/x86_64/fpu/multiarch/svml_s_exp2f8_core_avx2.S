/* Function exp2f vectorized with AVX2.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
   https://www.gnu.org/licenses/.  */

/*
 * ALGORITHM DESCRIPTION:
 *
 *   exp2(x)  = 2^n * T[j] * (1 + P(y))
 *   where
 *        x = m*(1/K) + y,    y in [-1/K..1/K]
 *        m = n*K + j,           m, n,j - signed integer, j in [-K/2..K/2]
 *
 *        values of 2^j/K are tabulated
 *
 *        P(y) is a minimax polynomial approximation of exp2(x)-1
 *        on small interval [-1/K..1/K]
 *
 *  Special cases:
 *
 *   exp2(NaN)  = NaN
 *   exp2(+INF) = +INF
 *   exp2(-INF) = 0
 *   exp2(x)    = 1 for subnormals
 *   For IEEE float
 *     if x >= 128.0 then exp2f(x) overflow
 *     if x < -151.0 then exp2f(x) underflow
 *
 */

/* Offsets for data table __svml_sexp2_data_internal
 */
#define _sShifter			0
#define _sPC0				32
#define _sPC1				64
#define _sPC2				96
#define _sPC3				128
#define _sPC4				160
#define _sPC5				192
#define _sPC6				224
#define _iAbsMask			256
#define _iDomainRange			288

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_exp2f_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	vmovups	__svml_sexp2_data_internal(%rip), %ymm1

	/* Check for overflow\underflow  */
	vmovups	_sPC6+__svml_sexp2_data_internal(%rip), %ymm7

	/*  Implementation  */
	vaddps	%ymm1, %ymm0, %ymm6
	vsubps	%ymm1, %ymm6, %ymm4

	/*  2^N  */
	vpslld	$23, %ymm6, %ymm8

	/*  R  */
	vsubps	%ymm4, %ymm0, %ymm5

	/*  Polynomial  */
	vfmadd213ps _sPC5+__svml_sexp2_data_internal(%rip), %ymm5, %ymm7
	vfmadd213ps _sPC4+__svml_sexp2_data_internal(%rip), %ymm5, %ymm7
	vfmadd213ps _sPC3+__svml_sexp2_data_internal(%rip), %ymm5, %ymm7
	vfmadd213ps _sPC2+__svml_sexp2_data_internal(%rip), %ymm5, %ymm7
	vfmadd213ps _sPC1+__svml_sexp2_data_internal(%rip), %ymm5, %ymm7
	vfmadd213ps _sPC0+__svml_sexp2_data_internal(%rip), %ymm5, %ymm7

	/* Check for overflow\underflow  */
	vandps	_iAbsMask+__svml_sexp2_data_internal(%rip), %ymm0, %ymm2
	vpcmpgtd _iDomainRange+__svml_sexp2_data_internal(%rip), %ymm2, %ymm3
	vmovmskps %ymm3, %edx

	/*  Reconstruction  */
	vpaddd	%ymm8, %ymm7, %ymm1
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm1

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%ymm1, %ymm0
	movq	%rbp, %rsp
	popq	%rbp
	cfi_def_cfa(7, 8)
	cfi_restore(6)
	ret
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	vmovups	%ymm0, 32(%rsp)
	vmovups	%ymm1, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm1

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -96; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r15 r12d r13d

	/* Range mask
	 * bits check
	 */

L(RANGEMASK_CHECK):
	btl	%r12d, %r13d

	/* Call scalar math function */
	jc	L(SCALAR_MATH_CALL)
	# LOE rbx r15 r12d r13d

	/* Special inputs
	 * processing loop
	 */

L(SPECIAL_VALUES_LOOP):
	incl	%r12d
	cmpl	$8, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	vmovups	64(%rsp), %ymm1

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -96; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm1

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	32(%rsp, %r14, 4), %xmm0
	call	exp2f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_exp2f_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sexp2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _sShifter[8][1];
	__declspec(align(32)) VUINT32 _sPC0[8][1];
	__declspec(align(32)) VUINT32 _sPC1[8][1];
	__declspec(align(32)) VUINT32 _sPC2[8][1];
	__declspec(align(32)) VUINT32 _sPC3[8][1];
	__declspec(align(32)) VUINT32 _sPC4[8][1];
	__declspec(align(32)) VUINT32 _sPC5[8][1];
	__declspec(align(32)) VUINT32 _sPC6[8][1];
	__declspec(align(32)) VUINT32 _iAbsMask[8][1];
	__declspec(align(32)) VUINT32 _iDomainRange[8][1];
} __svml_sexp2_data_internal;
#endif
__svml_sexp2_data_internal:
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter */
	.align	32
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 /* _sPC0 */
	.align	32
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218 /* _sPC1 */
	.align	32
	.long	0x3e75fdef, 0x3e75fdef, 0x3e75fdef, 0x3e75fdef, 0x3e75fdef, 0x3e75fdef, 0x3e75fdef, 0x3e75fdef /* _sPC2 */
	.align	32
	.long	0x3d6357cf, 0x3d6357cf, 0x3d6357cf, 0x3d6357cf, 0x3d6357cf, 0x3d6357cf, 0x3d6357cf, 0x3d6357cf /* _sPC3 */
	.align	32
	.long	0x3c1d962c, 0x3c1d962c, 0x3c1d962c, 0x3c1d962c, 0x3c1d962c, 0x3c1d962c, 0x3c1d962c, 0x3c1d962c /* _sPC4 */
	.align	32
	.long	0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51 /* _sPC5 */
	.align	32
	.long	0x39213c8c, 0x39213c8c, 0x39213c8c, 0x39213c8c, 0x39213c8c, 0x39213c8c, 0x39213c8c, 0x39213c8c /* _sPC6 */
	//common
	.align	32
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	32
	.long	0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000 /* _iDomainRange=126.0 */
	.align	32
	.type	__svml_sexp2_data_internal, @object
	.size	__svml_sexp2_data_internal, .-__svml_sexp2_data_internal
