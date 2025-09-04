/* Function log10f vectorized with AVX-512.
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
 *    log10(x) = k*log10(2.0) - log10(Rcp) + poly_approximation(R)
 *       log10(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_slog10_data_internal_avx512
 */
#define One				0
#define coeff4				64
#define coeff3				128
#define coeff2				192
#define coeff1				256
#define L2				320

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_log10f_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vgetmantps $11, {sae}, %zmm0, %zmm3
	vmovups	__svml_slog10_data_internal_avx512(%rip), %zmm1
	vgetexpps {sae}, %zmm0, %zmm5
	vmovups	L2+__svml_slog10_data_internal_avx512(%rip), %zmm10
	vpsrld	$19, %zmm3, %zmm7
	vgetexpps {sae}, %zmm3, %zmm6
	vsubps	{rn-sae}, %zmm1, %zmm3, %zmm11
	vpermps	coeff4+__svml_slog10_data_internal_avx512(%rip), %zmm7, %zmm1
	vpermps	coeff3+__svml_slog10_data_internal_avx512(%rip), %zmm7, %zmm2
	vsubps	{rn-sae}, %zmm6, %zmm5, %zmm9
	vpermps	coeff2+__svml_slog10_data_internal_avx512(%rip), %zmm7, %zmm4
	vpermps	coeff1+__svml_slog10_data_internal_avx512(%rip), %zmm7, %zmm8

	/* x<=0? */
	vfpclassps $94, %zmm0, %k0
	vfmadd213ps {rn-sae}, %zmm2, %zmm11, %zmm1
	vmulps	{rn-sae}, %zmm10, %zmm9, %zmm12
	vfmadd213ps {rn-sae}, %zmm4, %zmm11, %zmm1
	kmovw	%k0, %edx
	vfmadd213ps {rn-sae}, %zmm8, %zmm11, %zmm1
	vfmadd213ps {rn-sae}, %zmm12, %zmm11, %zmm1
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm1, %zmm0
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
	vmovups	%zmm0, 64(%rsp)
	vmovups	%zmm1, 128(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm1

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
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
	cmpl	$16, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	vmovups	128(%rsp), %zmm1

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm1

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	64(%rsp, %r14, 4), %xmm0
	call	log10f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_log10f_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_slog10_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 coeff4[16][1];
	__declspec(align(64)) VUINT32 coeff3[16][1];
	__declspec(align(64)) VUINT32 coeff2[16][1];
	__declspec(align(64)) VUINT32 coeff1[16][1];
	__declspec(align(64)) VUINT32 L2[16][1];
} __svml_slog10_data_internal_avx512;
#endif
__svml_slog10_data_internal_avx512:
	/* One */
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	// c4
	.align	64
	.long	0xbdc9ae9b, 0xbda6fcf4
	.long	0xbd8bac76, 0xbd6bca30
	.long	0xbd48a99b, 0xbd2c0a9f
	.long	0xbd1480db, 0xbd00faf2
	.long	0xbe823aa9, 0xbe656348
	.long	0xbe4afbb9, 0xbe346895
	.long	0xbe20ffff, 0xbe103a0b
	.long	0xbe01a91c, 0xbde9e84e
	// c3
	.align	64
	.long	0x3e13d888, 0x3e10a87c
	.long	0x3e0b95c3, 0x3e057f0b
	.long	0x3dfde038, 0x3df080d9
	.long	0x3de34c1e, 0x3dd68333
	.long	0x3dac6e8e, 0x3dd54a51
	.long	0x3df30f40, 0x3e04235d
	.long	0x3e0b7033, 0x3e102c90
	.long	0x3e12ebad, 0x3e141ff8
	// c2
	.align	64
	.long	0xbe5e5a9b, 0xbe5e2677
	.long	0xbe5d83f5, 0xbe5c6016
	.long	0xbe5abd0b, 0xbe58a6fd
	.long	0xbe562e02, 0xbe5362f8
	.long	0xbe68e27c, 0xbe646747
	.long	0xbe619a73, 0xbe5ff05a
	.long	0xbe5f0570, 0xbe5e92d0
	.long	0xbe5e662b, 0xbe5e5c08
	// c1
	.align	64
	.long	0x3ede5bd8, 0x3ede5b45
	.long	0x3ede57d8, 0x3ede4eb1
	.long	0x3ede3d37, 0x3ede2166
	.long	0x3eddf9d9, 0x3eddc5bb
	.long	0x3ede08ed, 0x3ede32e7
	.long	0x3ede4967, 0x3ede5490
	.long	0x3ede597f, 0x3ede5b50
	.long	0x3ede5bca, 0x3ede5bd9
	/* L2 */
	.align	64
	.long	0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b
	.align	64
	.type	__svml_slog10_data_internal_avx512, @object
	.size	__svml_slog10_data_internal_avx512, .-__svml_slog10_data_internal_avx512
