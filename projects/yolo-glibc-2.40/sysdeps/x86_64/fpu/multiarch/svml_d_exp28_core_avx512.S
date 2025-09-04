/* Function exp2 vectorized with AVX-512.
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
 *     Double precision mantissa represented as: 1.b1b2b3 ... b52
 *     Constant for double precision: S = 2^48 x 1.5
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
 *     Rnd(S + X) = Rnd(2^48 x 1.5 + X)
 *     (Note: 2^exp x 1.b1b2b3 ... b52,  2^{exp-52} = 2^-4 for exp=48)
 *
 *     exp2(x) =  2^K  x  2^fo  x (1 + poly(r)),   where 2^r = 1 + poly(r)
 *
 *     Scale back:
 *     dest = src1 x 2^floor(src2)
 *
 *
 */

/* Offsets for data table __svml_dexp2_data_internal_avx512
 */
#define Frac_PowerD0			0
#define poly_coeff1			128
#define poly_coeff2			192
#define poly_coeff3			256
#define poly_coeff4			320
#define poly_coeff5			384
#define poly_coeff6			448
#define add_const			512
#define AbsMask				576
#define Threshold			640
#define _lIndexMask			704

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_exp2_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	poly_coeff5+__svml_dexp2_data_internal_avx512(%rip), %zmm14
	vmovups	poly_coeff6+__svml_dexp2_data_internal_avx512(%rip), %zmm6

	/*
	 * Reduced argument
	 * where VREDUCE is available
	 */
	vreducepd $65, {sae}, %zmm0, %zmm10
	vmovups	poly_coeff4+__svml_dexp2_data_internal_avx512(%rip), %zmm7
	vmovups	add_const+__svml_dexp2_data_internal_avx512(%rip), %zmm3
	vmovups	poly_coeff3+__svml_dexp2_data_internal_avx512(%rip), %zmm8
	vmovups	__svml_dexp2_data_internal_avx512(%rip), %zmm13

	/* c6*r   + c5 */
	vfmadd231pd {rn-sae}, %zmm10, %zmm6, %zmm14
	vmovups	poly_coeff2+__svml_dexp2_data_internal_avx512(%rip), %zmm9
	vmovups	Threshold+__svml_dexp2_data_internal_avx512(%rip), %zmm2

	/*
	 *
	 *  HA
	 * Variables and constants
	 * Load constants and vector(s)
	 */
	vmovups	poly_coeff1+__svml_dexp2_data_internal_avx512(%rip), %zmm11

	/* c6*r^2 + c5*r + c4 */
	vfmadd213pd {rn-sae}, %zmm7, %zmm10, %zmm14

	/*
	 * Integer form of K+0.b1b2b3b4 in lower bits - call K_plus_f0
	 * Mantisssa of normalized double precision FP: 1.b1b2...b52
	 */
	vaddpd	{rd-sae}, %zmm3, %zmm0, %zmm4
	vandpd	AbsMask+__svml_dexp2_data_internal_avx512(%rip), %zmm0, %zmm1

	/* c6*r^3 + c5*r^2 + c4*r + c3 */
	vfmadd213pd {rn-sae}, %zmm8, %zmm10, %zmm14
	vcmppd	$29, {sae}, %zmm2, %zmm1, %k0

	/* c6*r^4 + c5*r^3 + c4*r^2 + c3*r + c2 */
	vfmadd213pd {rn-sae}, %zmm9, %zmm10, %zmm14
	kmovw	%k0, %edx

	/* c6*r^5 + c5*r^4 + c4*r^3 + c3*r^2 + c2*r + c1 */
	vfmadd213pd {rn-sae}, %zmm11, %zmm10, %zmm14

	/* Table value: 2^(0.b1b2b3b4) */
	vpandq	_lIndexMask+__svml_dexp2_data_internal_avx512(%rip), %zmm4, %zmm5
	vpermt2pd Frac_PowerD0+64+__svml_dexp2_data_internal_avx512(%rip), %zmm5, %zmm13

	/* T*r */
	vmulpd	{rn-sae}, %zmm10, %zmm13, %zmm12

	/* T + (T*r*(c6*r^5 + c5*r^4 + c4*r^3 + c3*r^2 + c2*r + c1)) */
	vfmadd213pd {rn-sae}, %zmm13, %zmm12, %zmm14

	/* Scaling placed at the end to avoid accuracy loss when T*r*scale underflows */
	vscalefpd {rn-sae}, %zmm0, %zmm14, %zmm1
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
	vmovsd	64(%rsp, %r14, 8), %xmm0
	call	exp2@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_exp2_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dexp2_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Frac_PowerD0[16][2];
	__declspec(align(64)) VUINT32 poly_coeff1[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 add_const[8][2];
	__declspec(align(64)) VUINT32 AbsMask[8][2];
	__declspec(align(64)) VUINT32 Threshold[8][2];
	__declspec(align(64)) VUINT32 _lIndexMask[8][2];
} __svml_dexp2_data_internal_avx512;
#endif
__svml_dexp2_data_internal_avx512:
	/* Frac_PowerD0 */
	.quad	0x3FF0000000000000
	.quad	0x3FF0B5586CF9890F
	.quad	0x3FF172B83C7D517B
	.quad	0x3FF2387A6E756238
	.quad	0x3FF306FE0A31B715
	.quad	0x3FF3DEA64C123422
	.quad	0x3FF4BFDAD5362A27
	.quad	0x3FF5AB07DD485429
	.quad	0x3FF6A09E667F3BCD
	.quad	0x3FF7A11473EB0187
	.quad	0x3FF8ACE5422AA0DB
	.quad	0x3FF9C49182A3F090
	.quad	0x3FFAE89F995AD3AD
	.quad	0x3FFC199BDD85529C
	.quad	0x3FFD5818DCFBA487
	.quad	0x3FFEA4AFA2A490DA
	.align	64
	.quad	0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B, 0x3FE62E42FEFA398B /* == poly_coeff1 == */
	.align	64
	.quad	0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A, 0x3FCEBFBDFF84555A /* == poly_coeff2 == */
	.align	64
	.quad	0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9, 0x3FAC6B08D4AD86B9 /* == poly_coeff3 == */
	.align	64
	.quad	0x3F83B2AD1B172252, 0x3F83B2AD1B172252, 0x3F83B2AD1B172252, 0x3F83B2AD1B172252, 0x3F83B2AD1B172252, 0x3F83B2AD1B172252, 0x3F83B2AD1B172252, 0x3F83B2AD1B172252 /* == poly_coeff4 == */
	.align	64
	.quad	0x3F55D7472713CD19, 0x3F55D7472713CD19, 0x3F55D7472713CD19, 0x3F55D7472713CD19, 0x3F55D7472713CD19, 0x3F55D7472713CD19, 0x3F55D7472713CD19, 0x3F55D7472713CD19 /* == poly_coeff5 == */
	.align	64
	.quad	0x3F24A1D7F526371B, 0x3F24A1D7F526371B, 0x3F24A1D7F526371B, 0x3F24A1D7F526371B, 0x3F24A1D7F526371B, 0x3F24A1D7F526371B, 0x3F24A1D7F526371B, 0x3F24A1D7F526371B /* == poly_coeff6 == */
	.align	64
	.quad	0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000 /* add_const */
	.align	64
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff /* AbsMask */
	.align	64
	.quad	0x408fefff00000000, 0x408fefff00000000, 0x408fefff00000000, 0x408fefff00000000, 0x408fefff00000000, 0x408fefff00000000, 0x408fefff00000000, 0x408fefff00000000 /* Threshold */
	.align	64
	.quad	0x000000000000000F, 0x000000000000000F, 0x000000000000000F, 0x000000000000000F, 0x000000000000000F, 0x000000000000000F, 0x000000000000000F, 0x000000000000000F /* _lIndexMask */
	.align	64
	.type	__svml_dexp2_data_internal_avx512, @object
	.size	__svml_dexp2_data_internal_avx512, .-__svml_dexp2_data_internal_avx512
