/* Function atan2 vectorized with AVX2.
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
 *      For    0.0    <= x <=  7.0/16.0: atan(x) = atan(0.0) + atan(s), where s=(x-0.0)/(1.0+0.0*x)
 *      For  7.0/16.0 <= x <= 11.0/16.0: atan(x) = atan(0.5) + atan(s), where s=(x-0.5)/(1.0+0.5*x)
 *      For 11.0/16.0 <= x <= 19.0/16.0: atan(x) = atan(1.0) + atan(s), where s=(x-1.0)/(1.0+1.0*x)
 *      For 19.0/16.0 <= x <= 39.0/16.0: atan(x) = atan(1.5) + atan(s), where s=(x-1.5)/(1.0+1.5*x)
 *      For 39.0/16.0 <= x <=    inf   : atan(x) = atan(inf) + atan(s), where s=-1.0/x
 *      Where atan(s) ~= s+s^3*Poly11(s^2) on interval |s|<7.0/0.16.
 *
 *
 */

/* Offsets for data table __svml_datan2_data_internal
 */
#define dPI				0
#define dPIO2				32
#define dA19				64
#define dA18				96
#define dA17				128
#define dA16				160
#define dA15				192
#define dA14				224
#define dA13				256
#define dA12				288
#define dA11				320
#define dA10				352
#define dA09				384
#define dA08				416
#define dA07				448
#define dA06				480
#define dA05				512
#define dA04				544
#define dA03				576
#define dA02				608
#define dA01				640
#define dA00				672
#define dSIGN_MASK			704
#define iCHK_WORK_SUB			736
#define iCHK_WORK_CMP			768
#define dABS_MASK			800
#define dZERO				832

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4vv_atan2_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$128, %rsp
	xorl	%edx, %edx

	/*
	 * #define NO_VECTOR_ZERO_ATAN2_ARGS
	 *  Declarations
	 * Variables
	 * Constants
	 *  The end of declarations
	 *  Implementation
	 * Get r0~=1/B
	 * Cannot be replaced by VQRCP(D, dR0, dB);
	 * Argument Absolute values
	 */
	vmovupd	dABS_MASK+__svml_datan2_data_internal(%rip), %ymm5

	/* Argument signs */
	vmovupd	dSIGN_MASK+__svml_datan2_data_internal(%rip), %ymm4
	vmovups	iCHK_WORK_SUB+__svml_datan2_data_internal(%rip), %xmm13
	vmovupd	%ymm0, (%rsp)
	vmovapd	%ymm1, %ymm8
	vandpd	%ymm5, %ymm8, %ymm2
	vandpd	%ymm5, %ymm0, %ymm1
	vcmpnlt_uqpd %ymm2, %ymm1, %ymm15

	/*
	 * 1) If y<x then a= y, b=x, PIO2=0
	 * 2) If y>x then a=-x, b=y, PIO2=Pi/2
	 */
	vorpd	%ymm4, %ymm2, %ymm6
	vblendvpd %ymm15, %ymm6, %ymm1, %ymm3
	vblendvpd %ymm15, %ymm1, %ymm2, %ymm6
	vdivpd	%ymm6, %ymm3, %ymm14
	vmovups	iCHK_WORK_CMP+__svml_datan2_data_internal(%rip), %xmm3
	vmovupd	%ymm6, 32(%rsp)
	vandpd	%ymm4, %ymm0, %ymm7
	vandpd	%ymm4, %ymm8, %ymm5
	vandpd	dPIO2+__svml_datan2_data_internal(%rip), %ymm15, %ymm4

	/* Check if y and x are on main path. */
	vextractf128 $1, %ymm2, %xmm9
	vextractf128 $1, %ymm1, %xmm10
	vshufps	$221, %xmm9, %xmm2, %xmm11
	vshufps	$221, %xmm10, %xmm1, %xmm12
	vpsubd	%xmm13, %xmm11, %xmm0
	vpsubd	%xmm13, %xmm12, %xmm9
	vpcmpgtd %xmm3, %xmm0, %xmm15
	vpcmpeqd %xmm3, %xmm0, %xmm6
	vpcmpgtd %xmm3, %xmm9, %xmm10
	vpcmpeqd %xmm3, %xmm9, %xmm3
	vpor	%xmm6, %xmm15, %xmm11
	vpor	%xmm3, %xmm10, %xmm12

	/* Polynomial. */
	vmulpd	%ymm14, %ymm14, %ymm10
	vpor	%xmm12, %xmm11, %xmm3
	vmovupd	dA18+__svml_datan2_data_internal(%rip), %ymm9
	vmovupd	dA17+__svml_datan2_data_internal(%rip), %ymm12
	vmovupd	dA16+__svml_datan2_data_internal(%rip), %ymm15
	vmulpd	%ymm10, %ymm10, %ymm11

	/* if x<0, dPI = Pi, else dPI =0 */
	vcmple_oqpd dZERO+__svml_datan2_data_internal(%rip), %ymm8, %ymm13
	vmovmskps %xmm3, %eax
	vmulpd	%ymm11, %ymm11, %ymm0
	vandpd	__svml_datan2_data_internal(%rip), %ymm13, %ymm6
	vmovupd	dA19+__svml_datan2_data_internal(%rip), %ymm13
	vfmadd213pd dA14+__svml_datan2_data_internal(%rip), %ymm0, %ymm9
	vfmadd213pd dA13+__svml_datan2_data_internal(%rip), %ymm0, %ymm12
	vfmadd213pd dA12+__svml_datan2_data_internal(%rip), %ymm0, %ymm15
	vfmadd213pd dA15+__svml_datan2_data_internal(%rip), %ymm0, %ymm13
	vfmadd213pd dA10+__svml_datan2_data_internal(%rip), %ymm0, %ymm9
	vfmadd213pd dA09+__svml_datan2_data_internal(%rip), %ymm0, %ymm12
	vfmadd213pd dA08+__svml_datan2_data_internal(%rip), %ymm0, %ymm15
	vfmadd213pd dA11+__svml_datan2_data_internal(%rip), %ymm0, %ymm13
	vfmadd213pd dA06+__svml_datan2_data_internal(%rip), %ymm0, %ymm9
	vfmadd213pd dA05+__svml_datan2_data_internal(%rip), %ymm0, %ymm12
	vfmadd213pd dA04+__svml_datan2_data_internal(%rip), %ymm0, %ymm15
	vfmadd213pd dA07+__svml_datan2_data_internal(%rip), %ymm0, %ymm13
	vfmadd213pd dA02+__svml_datan2_data_internal(%rip), %ymm0, %ymm9
	vfmadd213pd dA01+__svml_datan2_data_internal(%rip), %ymm0, %ymm12
	vfmadd213pd dA03+__svml_datan2_data_internal(%rip), %ymm0, %ymm13

	/* A00=1.0, account for it later  VQFMA(D, dP4, dP4, dR8, dA00); */
	vmulpd	%ymm15, %ymm0, %ymm0
	vfmadd213pd %ymm9, %ymm10, %ymm13
	vfmadd213pd %ymm0, %ymm10, %ymm12
	vfmadd213pd %ymm12, %ymm11, %ymm13

	/*
	 * Reconstruction.
	 * dP=(R+R*dP) + dPIO2
	 */
	vfmadd213pd %ymm14, %ymm14, %ymm13
	vaddpd	%ymm13, %ymm4, %ymm14
	vorpd	%ymm5, %ymm14, %ymm0
	vaddpd	%ymm0, %ymm6, %ymm9
	vorpd	%ymm7, %ymm9, %ymm0

	/*  Special branch for fast (vector) processing of zero arguments  */
	testl	%eax, %eax

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx xmm3 ymm0 ymm1 ymm2 ymm4 ymm5 ymm6 ymm7 ymm8

	/* Return from auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH_RETURN):
	/*
	 *  Special branch for fast (vector) processing of zero arguments
	 *  The end of implementation
	 */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm8

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
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
	vmovupd	(%rsp), %ymm1
	vmovupd	%ymm8, 64(%rsp)
	vmovupd	%ymm0, 96(%rsp)
	vmovupd	%ymm1, 32(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
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
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	vmovupd	96(%rsp), %ymm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	32(%rsp, %r14, 8), %xmm0
	vmovsd	64(%rsp, %r14, 8), %xmm1
	call	atan2@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 96(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	cfi_restore(12)
	cfi_restore(13)
	cfi_restore(14)
	# LOE rbx r15 r12d r13d

	/* Auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH):
	vmovupd	(%rsp), %ymm11

	/* Check if at least on of Y or Y is zero: iAXAYZERO */
	vmovupd	dZERO+__svml_datan2_data_internal(%rip), %ymm10

	/* Check if both X & Y are not NaNs:  iXYnotNAN */
	vcmpordpd %ymm8, %ymm8, %ymm12
	vcmpordpd %ymm11, %ymm11, %ymm13
	vcmpeqpd %ymm10, %ymm2, %ymm2
	vcmpeqpd %ymm10, %ymm1, %ymm1
	vandpd	%ymm13, %ymm12, %ymm14
	vorpd	%ymm1, %ymm2, %ymm2
	vextractf128 $1, %ymm14, %xmm15
	vextractf128 $1, %ymm2, %xmm11
	vshufps	$221, %xmm15, %xmm14, %xmm9
	vshufps	$221, %xmm11, %xmm2, %xmm12

	/*
	 *  Path for zero arguments (at least one of both)
	 * Check if both args are zeros (den. is zero)
	 */
	vcmpeqpd 32(%rsp), %ymm10, %ymm2

	/* Check if at least on of Y or Y is zero and not NaN: iAXAYZEROnotNAN */
	vpand	%xmm9, %xmm12, %xmm1

	/* Exclude from previous callout mask zero (and not NaN) arguments */
	vpandn	%xmm3, %xmm1, %xmm3

	/* Go to callout */
	vmovmskps %xmm3, %edx

	/* Set sPIO2 to zero if den. is zero */
	vblendvpd %ymm2, %ymm10, %ymm4, %ymm4
	vorpd	%ymm5, %ymm4, %ymm5

	/* Res = sign(Y)*(X<0)?(PIO2+PI):PIO2 */
	vextractf128 $1, %ymm10, %xmm2
	vextractf128 $1, %ymm8, %xmm3
	vshufps	$221, %xmm2, %xmm10, %xmm4
	vshufps	$221, %xmm3, %xmm8, %xmm9
	vpcmpgtd %xmm9, %xmm4, %xmm12
	vpshufd	$80, %xmm12, %xmm11
	vpshufd	$250, %xmm12, %xmm13
	vinsertf128 $1, %xmm13, %ymm11, %ymm14
	vandpd	%ymm6, %ymm14, %ymm6
	vaddpd	%ymm6, %ymm5, %ymm2
	vorpd	%ymm7, %ymm2, %ymm2

	/* Merge results from main and spec path */
	vpshufd	$80, %xmm1, %xmm7
	vpshufd	$250, %xmm1, %xmm1
	vinsertf128 $1, %xmm1, %ymm7, %ymm3
	vblendvpd %ymm3, %ymm2, %ymm0, %ymm0

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm8
END(_ZGVdN4vv_atan2_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_datan2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 dPI[4][2];
	__declspec(align(32)) VUINT32 dPIO2[4][2];
	__declspec(align(32)) VUINT32 dA19[4][2];
	__declspec(align(32)) VUINT32 dA18[4][2];
	__declspec(align(32)) VUINT32 dA17[4][2];
	__declspec(align(32)) VUINT32 dA16[4][2];
	__declspec(align(32)) VUINT32 dA15[4][2];
	__declspec(align(32)) VUINT32 dA14[4][2];
	__declspec(align(32)) VUINT32 dA13[4][2];
	__declspec(align(32)) VUINT32 dA12[4][2];
	__declspec(align(32)) VUINT32 dA11[4][2];
	__declspec(align(32)) VUINT32 dA10[4][2];
	__declspec(align(32)) VUINT32 dA09[4][2];
	__declspec(align(32)) VUINT32 dA08[4][2];
	__declspec(align(32)) VUINT32 dA07[4][2];
	__declspec(align(32)) VUINT32 dA06[4][2];
	__declspec(align(32)) VUINT32 dA05[4][2];
	__declspec(align(32)) VUINT32 dA04[4][2];
	__declspec(align(32)) VUINT32 dA03[4][2];
	__declspec(align(32)) VUINT32 dA02[4][2];
	__declspec(align(32)) VUINT32 dA01[4][2];
	__declspec(align(32)) VUINT32 dA00[4][2];
	__declspec(align(32)) VUINT32 dSIGN_MASK[4][2];
	__declspec(align(32)) VUINT32 iCHK_WORK_SUB[8][1];
	__declspec(align(32)) VUINT32 iCHK_WORK_CMP[8][1];
	__declspec(align(32)) VUINT32 dABS_MASK[4][2];
	__declspec(align(32)) VUINT32 dZERO[4][2];
} __svml_datan2_data_internal;
#endif
__svml_datan2_data_internal:
	.quad	0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18 // dPI
	.align	32
	.quad	0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18 // dPIO2
	.align	32
	.quad	0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3 // dA19
	.align	32
	.quad	0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209 // dA18
	.align	32
	.quad	0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23 // dA17
	.align	32
	.quad	0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3 // dA16
	.align	32
	.quad	0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD // dA15
	.align	32
	.quad	0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5 // dA14
	.align	32
	.quad	0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C // dA13
	.align	32
	.quad	0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F // dA12
	.align	32
	.quad	0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC // dA11
	.align	32
	.quad	0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B // dA10
	.align	32
	.quad	0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954 // dA09
	.align	32
	.quad	0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF // dA08
	.align	32
	.quad	0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0 // dA07
	.align	32
	.quad	0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651 // dA06
	.align	32
	.quad	0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94 // dA05
	.align	32
	.quad	0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5 // dA04
	.align	32
	.quad	0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7 // dA03
	.align	32
	.quad	0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34 // dA02
	.align	32
	.quad	0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5 // dA01
	.align	32
	.quad	0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000 // dA00
	.align	32
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000 // dSIGN_MASK
	.align	32
	.long	0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000 // iCHK_WORK_SUB
	.align	32
	.long	0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000 // iCHK_WORK_CMP
	.align	32
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff // dABS_MASK
	.align	32
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 // dZERO
	.align	32
	.type	__svml_datan2_data_internal, @object
	.size	__svml_datan2_data_internal, .-__svml_datan2_data_internal
