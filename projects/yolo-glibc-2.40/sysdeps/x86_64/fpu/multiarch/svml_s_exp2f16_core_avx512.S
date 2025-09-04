/* Function exp2f vectorized with AVX-512.
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
 *     Single precision mantissa represented as: 1.b1b2b3 ... b23
 *     Constant for single precision: S = 2^19 x 1.5
 *
 *     2^X = 2^Xo  x  2^{X-Xo}
 *     2^X = 2^K  x  2^fo  x  2^{X-Xo}
 *     2^X = 2^K  x  2^fo  x  2^r
 *
 *     2^K  --> Manual scaling
 *     2^fo --> Table lookup
 *     r    --> 1 + poly    (r = X - Xo)
 *
 *     Xo = K  +  fo
 *     Xo = K  +  0.x1x2x3x4
 *
 *     r = X - Xo
 *       = Vreduce(X, imm)
 *       = X - VRndScale(X, imm),    where Xo = VRndScale(X, imm)
 *
 *     Rnd(S + X) = S + Xo,    where S is selected as S = 2^19 x 1.5
 *         S + X = S + floor(X) + 0.x1x2x3x4
 *     Rnd(S + X) = Rnd(2^19 x 1.5 + X)
 *     (Note: 2^exp x 1.b1b2b3 ... b23,  2^{exp-23} = 2^-4 for exp=19)
 *
 *     exp2(x) =  2^K  x  2^fo  x (1 + poly(r)),   where 2^r = 1 + poly(r)
 *
 *     Scale back:
 *     dest = src1 x 2^floor(src2)
 *
 *
 */

/* Offsets for data table __svml_sexp2_data_internal_avx512
 */
#define Frac_PowerS0			0
#define poly_coeff1			64
#define poly_coeff2			128
#define poly_coeff3			192
#define add_const			256
#define AbsMask				320
#define Threshold			384

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_exp2f_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	add_const+__svml_sexp2_data_internal_avx512(%rip), %zmm3

	/*
	 * Reduced argument
	 * where VREDUCE is available
	 */
	vreduceps $65, {sae}, %zmm0, %zmm6
	vmovups	poly_coeff3+__svml_sexp2_data_internal_avx512(%rip), %zmm5
	vmovups	poly_coeff2+__svml_sexp2_data_internal_avx512(%rip), %zmm10
	vmovups	Threshold+__svml_sexp2_data_internal_avx512(%rip), %zmm2

	/*
	 *
	 *  HA
	 * Variables and constants
	 * Load constants and vector(s)
	 */
	vmovups	poly_coeff1+__svml_sexp2_data_internal_avx512(%rip), %zmm7

	/*
	 * Integer form of K+0.b1b2b3b4 in lower bits - call K_plus_f0
	 * Mantisssa of normalized single precision FP: 1.b1b2...b23
	 */
	vaddps	{rd-sae}, %zmm3, %zmm0, %zmm4
	vandps	AbsMask+__svml_sexp2_data_internal_avx512(%rip), %zmm0, %zmm1

	/* c3*r   + c2 */
	vfmadd231ps {rn-sae}, %zmm6, %zmm5, %zmm10
	vcmpps	$30, {sae}, %zmm2, %zmm1, %k0

	/* c3*r^2 + c2*r + c1 */
	vfmadd213ps {rn-sae}, %zmm7, %zmm6, %zmm10

	/* Table value: 2^(0.b1b2b3b4) */
	vpermps	__svml_sexp2_data_internal_avx512(%rip), %zmm4, %zmm9
	kmovw	%k0, %edx

	/* T*r */
	vmulps	{rn-sae}, %zmm6, %zmm9, %zmm8

	/* T + (T*r*(c3*r^2 + c2*r + c1) */
	vfmadd213ps {rn-sae}, %zmm9, %zmm8, %zmm10

	/* Scaling placed at the end to avoid accuracy loss when T*r*scale underflows */
	vscalefps {rn-sae}, %zmm0, %zmm10, %zmm1
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
	call	exp2f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_exp2f_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sexp2_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Frac_PowerS0[16][1];
	__declspec(align(64)) VUINT32 poly_coeff1[16][1];
	__declspec(align(64)) VUINT32 poly_coeff2[16][1];
	__declspec(align(64)) VUINT32 poly_coeff3[16][1];
	__declspec(align(64)) VUINT32 add_const[16][1];
	__declspec(align(64)) VUINT32 AbsMask[16][1];
	__declspec(align(64)) VUINT32 Threshold[16][1];
} __svml_sexp2_data_internal_avx512;
#endif
__svml_sexp2_data_internal_avx512:
	/* Frac_PowerS0 */
	.long	0x3F800000
	.long	0x3F85AAC3
	.long	0x3F8B95C2
	.long	0x3F91C3D3
	.long	0x3F9837F0
	.long	0x3F9EF532
	.long	0x3FA5FED7
	.long	0x3FAD583F
	.long	0x3FB504F3
	.long	0x3FBD08A4
	.long	0x3FC5672A
	.long	0x3FCE248C
	.long	0x3FD744FD
	.long	0x3FE0CCDF
	.long	0x3FEAC0C7
	.long	0x3FF5257D
	.align	64
	.long	0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222, 0x3F317222 /* == poly_coeff1 == */
	.align	64
	.long	0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B, 0x3E75F16B /* == poly_coeff2 == */
	.align	64
	.long	0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA, 0x3D6854CA /* == poly_coeff3 == */
	.align	64
	.long	0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000, 0x49400000 /* add_const */
	.align	64
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* AbsMask */
	.align	64
	.long	0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000 /* Threshold=126.0 */
	.align	64
	.type	__svml_sexp2_data_internal_avx512, @object
	.size	__svml_sexp2_data_internal_avx512, .-__svml_sexp2_data_internal_avx512
