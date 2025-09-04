/* Function atan2 vectorized with AVX-512.
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
#define dPIO2				64
#define dA19				128
#define dA18				192
#define dA17				256
#define dA16				320
#define dA15				384
#define dA14				448
#define dA13				512
#define dA12				576
#define dA11				640
#define dA10				704
#define dA09				768
#define dA08				832
#define dA07				896
#define dA06				960
#define dA05				1024
#define dA04				1088
#define dA03				1152
#define dA02				1216
#define dA01				1280
#define dA00				1344
#define dSIGN_MASK			1408
#define iCHK_WORK_SUB			1472
#define iCHK_WORK_CMP			1536
#define dABS_MASK			1600
#define dZERO				1664

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8vv_atan2_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$256, %rsp
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
	vmovups	dABS_MASK+__svml_datan2_data_internal(%rip), %zmm4

	/* Argument signs */
	vmovups	dSIGN_MASK+__svml_datan2_data_internal(%rip), %zmm6

	/*
	 * 1) If y<x then a= y, b=x, PIO2=0
	 * 2) If y>x then a=-x, b=y, PIO2=Pi/2
	 */
	vmovups	dPIO2+__svml_datan2_data_internal(%rip), %zmm3
	vandpd	%zmm4, %zmm0, %zmm11
	vmovaps	%zmm1, %zmm7
	vandpd	%zmm4, %zmm7, %zmm2
	vandpd	%zmm6, %zmm7, %zmm5
	vandpd	%zmm6, %zmm0, %zmm4
	vorpd	%zmm6, %zmm2, %zmm12
	vcmppd	$17, {sae}, %zmm2, %zmm11, %k1
	vmovdqu	iCHK_WORK_CMP+__svml_datan2_data_internal(%rip), %ymm6
	vmovups	%zmm11, 64(%rsp)

	/* Check if y and x are on main path. */
	vpsrlq	$32, %zmm2, %zmm9
	vblendmpd %zmm11, %zmm12, %zmm13{%k1}
	vblendmpd %zmm2, %zmm11, %zmm15{%k1}
	vpsrlq	$32, %zmm11, %zmm8
	vmovdqu	iCHK_WORK_SUB+__svml_datan2_data_internal(%rip), %ymm12
	vdivpd	{rn-sae}, %zmm15, %zmm13, %zmm1
	vmovups	%zmm15, (%rsp)
	vpmovqd	%zmm9, %ymm14
	vpmovqd	%zmm8, %ymm10
	vxorpd	%zmm3, %zmm3, %zmm3{%k1}
	vpsubd	%ymm12, %ymm14, %ymm13
	vpsubd	%ymm12, %ymm10, %ymm9

	/* Polynomial. */
	vmulpd	{rn-sae}, %zmm1, %zmm1, %zmm12
	vpcmpgtd %ymm6, %ymm13, %ymm15
	vpcmpeqd %ymm6, %ymm13, %ymm11
	vmulpd	{rn-sae}, %zmm12, %zmm12, %zmm13
	vpor	%ymm11, %ymm15, %ymm8
	vmovups	dA19+__svml_datan2_data_internal(%rip), %zmm11
	vmovups	dA15+__svml_datan2_data_internal(%rip), %zmm15
	vpcmpgtd %ymm6, %ymm9, %ymm14
	vpcmpeqd %ymm6, %ymm9, %ymm6
	vpor	%ymm6, %ymm14, %ymm10
	vmulpd	{rn-sae}, %zmm13, %zmm13, %zmm14
	vmovups	dA18+__svml_datan2_data_internal(%rip), %zmm9
	vpor	%ymm10, %ymm8, %ymm6
	vmovups	dA17+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd231pd {rn-sae}, %zmm14, %zmm11, %zmm15
	vmovups	dA14+__svml_datan2_data_internal(%rip), %zmm11
	vmovups	dA12+__svml_datan2_data_internal(%rip), %zmm8
	vfmadd231pd {rn-sae}, %zmm14, %zmm9, %zmm11
	vmovups	dA13+__svml_datan2_data_internal(%rip), %zmm9
	vfmadd231pd {rn-sae}, %zmm14, %zmm10, %zmm9
	vmovups	dA16+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd231pd {rn-sae}, %zmm14, %zmm10, %zmm8
	vmovups	dA11+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm15
	vmovups	dA10+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm11
	vmovups	dA09+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm9
	vmovups	dA08+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm8
	vmovups	dA07+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm15
	vmovups	dA06+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm11
	vmovups	dA05+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm9
	vmovups	dA04+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm8
	vmovups	dA03+__svml_datan2_data_internal(%rip), %zmm10

	/* A00=1.0, account for it later  VQFMA(D, dP4, dP4, dR8, dA00); */
	vmulpd	{rn-sae}, %zmm14, %zmm8, %zmm8
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm15
	vmovups	dA02+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm11
	vmovups	dA01+__svml_datan2_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm11, %zmm12, %zmm15
	vfmadd213pd {rn-sae}, %zmm10, %zmm14, %zmm9
	vfmadd213pd {rn-sae}, %zmm8, %zmm12, %zmm9
	vmovups	__svml_datan2_data_internal(%rip), %zmm8
	vfmadd213pd {rn-sae}, %zmm9, %zmm13, %zmm15

	/*
	 * Reconstruction.
	 * dP=(R+R*dP) + dPIO2
	 */
	vfmadd213pd {rn-sae}, %zmm1, %zmm1, %zmm15
	vaddpd	{rn-sae}, %zmm3, %zmm15, %zmm1
	vorpd	%zmm5, %zmm1, %zmm9

	/* if x<0, dPI = Pi, else dPI =0 */
	vmovups	dZERO+__svml_datan2_data_internal(%rip), %zmm1
	vcmppd	$18, {sae}, %zmm1, %zmm7, %k2
	vaddpd	{rn-sae}, %zmm8, %zmm9, %zmm9{%k2}
	vmovmskps %ymm6, %eax
	vorpd	%zmm4, %zmm9, %zmm11

	/*  Special branch for fast (vector) processing of zero arguments  */
	vmovups	64(%rsp), %zmm9
	testl	%eax, %eax

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm6 zmm0 zmm2 zmm3 zmm4 zmm5 zmm7 zmm9 zmm11

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
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm7 zmm11

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm11, %zmm0
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
	vmovups	%zmm7, 128(%rsp)
	vmovups	%zmm11, 192(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm11

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
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
	vmovups	192(%rsp), %zmm11

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm11

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	64(%rsp, %r14, 8), %xmm0
	vmovsd	128(%rsp, %r14, 8), %xmm1
	call	atan2@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 192(%rsp, %r14, 8)

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
	/* Check if at least on of Y or Y is zero: iAXAYZERO */
	vmovups	dZERO+__svml_datan2_data_internal(%rip), %zmm8

	/* Check if both X & Y are not NaNs:  iXYnotNAN */
	vcmppd	$3, {sae}, %zmm7, %zmm7, %k1
	vcmppd	$3, {sae}, %zmm0, %zmm0, %k2
	vcmppd	$4, {sae}, %zmm8, %zmm2, %k3
	vcmppd	$4, {sae}, %zmm8, %zmm9, %k4

	/* Res = sign(Y)*(X<0)?(PIO2+PI):PIO2 */
	vpcmpgtq %zmm7, %zmm8, %k6
	vpternlogd $0xff, %zmm1, %zmm1, %zmm10
	vmovaps	%zmm10, %zmm15
	vmovaps	%zmm10, %zmm12
	vmovaps	%zmm10, %zmm13
	vpandnq	%zmm2, %zmm2, %zmm15{%k3}
	vmovaps	%zmm10, %zmm2
	vpandnq	%zmm7, %zmm7, %zmm12{%k1}
	vpandnq	%zmm0, %zmm0, %zmm13{%k2}
	vpandnq	%zmm9, %zmm9, %zmm2{%k4}
	vandpd	%zmm13, %zmm12, %zmm14
	vorpd	%zmm2, %zmm15, %zmm9
	vpsrlq	$32, %zmm14, %zmm1
	vpsrlq	$32, %zmm9, %zmm2
	vpmovqd	%zmm1, %ymm1
	vpmovqd	%zmm2, %ymm9

	/* Check if at least on of Y or Y is zero and not NaN: iAXAYZEROnotNAN */
	vpand	%ymm1, %ymm9, %ymm2

	/*
	 *  Path for zero arguments (at least one of both)
	 * Check if both args are zeros (den. is zero)
	 */
	vmovups	(%rsp), %zmm1

	/* Exclude from previous callout mask zero (and not NaN) arguments */
	vpandn	%ymm6, %ymm2, %ymm6
	vcmppd	$4, {sae}, %zmm8, %zmm1, %k5

	/* Go to callout */
	vmovmskps %ymm6, %edx
	vpandnq	%zmm1, %zmm1, %zmm10{%k5}

	/* Set sPIO2 to zero if den. is zero */
	vpandnq	%zmm3, %zmm10, %zmm3
	vpandq	%zmm10, %zmm8, %zmm1
	vporq	%zmm1, %zmm3, %zmm3
	vorpd	%zmm5, %zmm3, %zmm1
	vmovups	__svml_datan2_data_internal(%rip), %zmm5
	vaddpd	{rn-sae}, %zmm5, %zmm1, %zmm1{%k6}
	vorpd	%zmm4, %zmm1, %zmm1

	/* Merge results from main and spec path */
	vpmovzxdq %ymm2, %zmm4
	vpsllq	$32, %zmm4, %zmm2
	vpord	%zmm2, %zmm4, %zmm3
	vpandnq	%zmm11, %zmm3, %zmm11
	vpandq	%zmm3, %zmm1, %zmm1
	vporq	%zmm1, %zmm11, %zmm11

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm7 zmm11
END(_ZGVeN8vv_atan2_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_datan2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 dPI[8][2];
	__declspec(align(64)) VUINT32 dPIO2[8][2];
	__declspec(align(64)) VUINT32 dA19[8][2];
	__declspec(align(64)) VUINT32 dA18[8][2];
	__declspec(align(64)) VUINT32 dA17[8][2];
	__declspec(align(64)) VUINT32 dA16[8][2];
	__declspec(align(64)) VUINT32 dA15[8][2];
	__declspec(align(64)) VUINT32 dA14[8][2];
	__declspec(align(64)) VUINT32 dA13[8][2];
	__declspec(align(64)) VUINT32 dA12[8][2];
	__declspec(align(64)) VUINT32 dA11[8][2];
	__declspec(align(64)) VUINT32 dA10[8][2];
	__declspec(align(64)) VUINT32 dA09[8][2];
	__declspec(align(64)) VUINT32 dA08[8][2];
	__declspec(align(64)) VUINT32 dA07[8][2];
	__declspec(align(64)) VUINT32 dA06[8][2];
	__declspec(align(64)) VUINT32 dA05[8][2];
	__declspec(align(64)) VUINT32 dA04[8][2];
	__declspec(align(64)) VUINT32 dA03[8][2];
	__declspec(align(64)) VUINT32 dA02[8][2];
	__declspec(align(64)) VUINT32 dA01[8][2];
	__declspec(align(64)) VUINT32 dA00[8][2];
	__declspec(align(64)) VUINT32 dSIGN_MASK[8][2];
	__declspec(align(64)) VUINT32 iCHK_WORK_SUB[16][1];
	__declspec(align(64)) VUINT32 iCHK_WORK_CMP[16][1];
	__declspec(align(64)) VUINT32 dABS_MASK[8][2];
	__declspec(align(64)) VUINT32 dZERO[8][2];
} __svml_datan2_data_internal;
#endif
__svml_datan2_data_internal:
	.quad	0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18, 0x400921FB54442D18 // dPI
	.align	64
	.quad	0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18, 0x3FF921FB54442D18 // dPIO2
	.align	64
	.quad	0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3 // dA19
	.align	64
	.quad	0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209, 0x3F2CED0A36665209 // dA18
	.align	64
	.quad	0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23, 0xBF52E67C93954C23 // dA17
	.align	64
	.quad	0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3 // dA16
	.align	64
	.quad	0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD // dA15
	.align	64
	.quad	0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5, 0x3F914F4C661116A5 // dA14
	.align	64
	.quad	0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C // dA13
	.align	64
	.quad	0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F // dA12
	.align	64
	.quad	0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC // dA11
	.align	64
	.quad	0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B // dA10
	.align	64
	.quad	0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954, 0xBFAAD261EAA09954 // dA09
	.align	64
	.quad	0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF // dA08
	.align	64
	.quad	0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0, 0xBFB11084009435E0 // dA07
	.align	64
	.quad	0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651, 0x3FB3B12A49295651 // dA06
	.align	64
	.quad	0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94, 0xBFB745D009BADA94 // dA05
	.align	64
	.quad	0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5 // dA04
	.align	64
	.quad	0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7, 0xBFC2492491EE55C7 // dA03
	.align	64
	.quad	0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34, 0x3FC999999997EE34 // dA02
	.align	64
	.quad	0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5, 0xBFD55555555553C5 // dA01
	.align	64
	.quad	0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000 // dA00
	.align	64
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000 // dSIGN_MASK
	.align	64
	.long	0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000, 0x80300000 // iCHK_WORK_SUB
	.align	64
	.long	0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000 // iCHK_WORK_CMP
	.align	64
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff // dABS_MASK
	.align	64
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000 // dZERO
	.align	64
	.type	__svml_datan2_data_internal, @object
	.size	__svml_datan2_data_internal, .-__svml_datan2_data_internal
