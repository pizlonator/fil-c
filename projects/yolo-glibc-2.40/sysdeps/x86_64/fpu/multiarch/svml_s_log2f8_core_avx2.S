/* Function log2f vectorized with AVX2.
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
 *    Get short reciprocal approximation Rcp ~ 1/mantissa(x)
 *    R = Rcp*x - 1.0
 *    log2(x) = k - log2(Rcp) + poly_approximation(R)
 *       log2(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_slog2_data_internal
 */
#define MinNorm				0
#define MaxNorm				32
#define iBrkValue			64
#define iOffExpoMask			96
#define One				128
#define sPoly				160

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_log2f_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/* reduction: compute r, n */
	vmovups	iBrkValue+__svml_slog2_data_internal(%rip), %ymm4
	vmovups	sPoly+64+__svml_slog2_data_internal(%rip), %ymm9
	vmovups	sPoly+128+__svml_slog2_data_internal(%rip), %ymm10
	vmovups	sPoly+192+__svml_slog2_data_internal(%rip), %ymm12
	vpsubd	%ymm4, %ymm0, %ymm1
	vcmplt_oqps MinNorm+__svml_slog2_data_internal(%rip), %ymm0, %ymm5
	vcmpnle_uqps MaxNorm+__svml_slog2_data_internal(%rip), %ymm0, %ymm6
	vpand	iOffExpoMask+__svml_slog2_data_internal(%rip), %ymm1, %ymm3
	vpsrad	$23, %ymm1, %ymm2
	vmovups	sPoly+__svml_slog2_data_internal(%rip), %ymm1
	vpaddd	%ymm4, %ymm3, %ymm8
	vcvtdq2ps %ymm2, %ymm14
	vsubps	One+__svml_slog2_data_internal(%rip), %ymm8, %ymm13
	vfmadd213ps sPoly+32+__svml_slog2_data_internal(%rip), %ymm13, %ymm1
	vfmadd213ps sPoly+96+__svml_slog2_data_internal(%rip), %ymm13, %ymm9
	vmulps	%ymm13, %ymm13, %ymm11
	vfmadd213ps sPoly+160+__svml_slog2_data_internal(%rip), %ymm13, %ymm10
	vfmadd213ps sPoly+224+__svml_slog2_data_internal(%rip), %ymm13, %ymm12
	vfmadd213ps %ymm9, %ymm11, %ymm1
	vfmadd213ps %ymm10, %ymm11, %ymm1
	vfmadd213ps %ymm12, %ymm11, %ymm1
	vfmadd213ps sPoly+256+__svml_slog2_data_internal(%rip), %ymm13, %ymm1
	vorps	%ymm6, %ymm5, %ymm7

	/* combine and get argument value range mask */
	vmovmskps %ymm7, %edx
	vfmadd213ps %ymm14, %ymm13, %ymm1
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
	call	log2f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_log2f_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_slog2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 MinNorm[8][1];
	__declspec(align(32)) VUINT32 MaxNorm[8][1];
	__declspec(align(32)) VUINT32 iBrkValue[8][1];
	__declspec(align(32)) VUINT32 iOffExpoMask[8][1];
	__declspec(align(32)) VUINT32 One[8][1];
	__declspec(align(32)) VUINT32 sPoly[9][8][1];
} __svml_slog2_data_internal;
#endif
__svml_slog2_data_internal:
	/* MinNorm */
	.long	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000
	/* MaxNorm */
	.align	32
	.long	0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff
	/* iBrkValue = SP 2/3 */
	.align	32
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	32
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sOne = SP 1.0 */
	.align	32
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* spoly[9] */
	.align	32
	.long	0x3e554012, 0x3e554012, 0x3e554012, 0x3e554012, 0x3e554012, 0x3e554012, 0x3e554012, 0x3e554012 /* coeff9 */
	.long	0xbe638E14, 0xbe638E14, 0xbe638E14, 0xbe638E14, 0xbe638E14, 0xbe638E14, 0xbe638E14, 0xbe638E14 /* coeff8 */
	.long	0x3e4D660B, 0x3e4D660B, 0x3e4D660B, 0x3e4D660B, 0x3e4D660B, 0x3e4D660B, 0x3e4D660B, 0x3e4D660B /* coeff7 */
	.long	0xbe727824, 0xbe727824, 0xbe727824, 0xbe727824, 0xbe727824, 0xbe727824, 0xbe727824, 0xbe727824 /* coeff6 */
	.long	0x3e93DD07, 0x3e93DD07, 0x3e93DD07, 0x3e93DD07, 0x3e93DD07, 0x3e93DD07, 0x3e93DD07, 0x3e93DD07 /* coeff5 */
	.long	0xbeB8B969, 0xbeB8B969, 0xbeB8B969, 0xbeB8B969, 0xbeB8B969, 0xbeB8B969, 0xbeB8B969, 0xbeB8B969 /* coeff4 */
	.long	0x3eF637C0, 0x3eF637C0, 0x3eF637C0, 0x3eF637C0, 0x3eF637C0, 0x3eF637C0, 0x3eF637C0, 0x3eF637C0 /* coeff3 */
	.long	0xbf38AA2B, 0xbf38AA2B, 0xbf38AA2B, 0xbf38AA2B, 0xbf38AA2B, 0xbf38AA2B, 0xbf38AA2B, 0xbf38AA2B /* coeff2 */
	.long	0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B /* coeff1 */
	.align	32
	.type	__svml_slog2_data_internal, @object
	.size	__svml_slog2_data_internal, .-__svml_slog2_data_internal
