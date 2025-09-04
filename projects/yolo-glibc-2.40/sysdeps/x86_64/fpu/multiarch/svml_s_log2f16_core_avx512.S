/* Function log2f vectorized with AVX-512.
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

/* Offsets for data table __svml_slog2_data_internal_avx512
 */
#define One				0
#define coeff4				64
#define coeff3				128
#define coeff2				192
#define coeff1				256

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_log2f_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vgetmantps $11, {sae}, %zmm0, %zmm3
	vmovups	__svml_slog2_data_internal_avx512(%rip), %zmm1
	vgetexpps {sae}, %zmm0, %zmm5

	/* x<=0? */
	vfpclassps $94, %zmm0, %k0
	vsubps	{rn-sae}, %zmm1, %zmm3, %zmm9
	vpsrld	$19, %zmm3, %zmm7
	vgetexpps {sae}, %zmm3, %zmm6
	vpermps	coeff4+__svml_slog2_data_internal_avx512(%rip), %zmm7, %zmm1
	vpermps	coeff3+__svml_slog2_data_internal_avx512(%rip), %zmm7, %zmm2
	vpermps	coeff2+__svml_slog2_data_internal_avx512(%rip), %zmm7, %zmm4
	vpermps	coeff1+__svml_slog2_data_internal_avx512(%rip), %zmm7, %zmm8
	vsubps	{rn-sae}, %zmm6, %zmm5, %zmm10
	vfmadd213ps {rn-sae}, %zmm2, %zmm9, %zmm1
	kmovw	%k0, %edx
	vfmadd213ps {rn-sae}, %zmm4, %zmm9, %zmm1
	vfmadd213ps {rn-sae}, %zmm8, %zmm9, %zmm1
	vfmadd213ps {rn-sae}, %zmm10, %zmm9, %zmm1
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
	call	log2f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_log2f_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_slog2_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 coeff4[16][1];
	__declspec(align(64)) VUINT32 coeff3[16][1];
	__declspec(align(64)) VUINT32 coeff2[16][1];
	__declspec(align(64)) VUINT32 coeff1[16][1];
} __svml_slog2_data_internal_avx512;
#endif
__svml_slog2_data_internal_avx512:
	/* One */
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	// c4
	.align	64
	.long	0xbea77e4a, 0xbe8aae3d
	.long	0xbe67fe32, 0xbe43d1b6
	.long	0xbe26a589, 0xbe0ee09b
	.long	0xbdf6a8a1, 0xbdd63b49
	.long	0xbf584e51, 0xbf3e80a1
	.long	0xbf2892f0, 0xbf15d377
	.long	0xbf05b525, 0xbeef8e30
	.long	0xbed75c8f, 0xbec24184
	// c3
	.align	64
	.long	0x3ef5910c, 0x3ef045a1
	.long	0x3ee7d87e, 0x3eddbb84
	.long	0x3ed2d6df, 0x3ec7bbd2
	.long	0x3ebcc42f, 0x3eb22616
	.long	0x3e8f3399, 0x3eb1223e
	.long	0x3ec9db4a, 0x3edb7a09
	.long	0x3ee79a1a, 0x3eef77cb
	.long	0x3ef407a4, 0x3ef607b4
	// c2
	.align	64
	.long	0xbf38a934, 0xbf387de6
	.long	0xbf37f6f0, 0xbf37048b
	.long	0xbf35a88a, 0xbf33ed04
	.long	0xbf31df56, 0xbf2f8d82
	.long	0xbf416814, 0xbf3daf58
	.long	0xbf3b5c08, 0xbf39fa2a
	.long	0xbf393713, 0xbf38d7e1
	.long	0xbf38b2cd, 0xbf38aa62
	// c1
	.align	64
	.long	0x3fb8aa3b, 0x3fb8a9c0
	.long	0x3fb8a6e8, 0x3fb89f4e
	.long	0x3fb890cb, 0x3fb879b1
	.long	0x3fb858d8, 0x3fb82d90
	.long	0x3fb8655e, 0x3fb8883a
	.long	0x3fb89aea, 0x3fb8a42f
	.long	0x3fb8a848, 0x3fb8a9c9
	.long	0x3fb8aa2f, 0x3fb8aa3b
	.align	64
	.type	__svml_slog2_data_internal_avx512, @object
	.size	__svml_slog2_data_internal_avx512, .-__svml_slog2_data_internal_avx512
