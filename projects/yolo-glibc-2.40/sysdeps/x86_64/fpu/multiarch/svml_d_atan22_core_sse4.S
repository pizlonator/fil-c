/* Function atan2 vectorized with SSE4.
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
#define dPIO2				16
#define dA19				32
#define dA18				48
#define dA17				64
#define dA16				80
#define dA15				96
#define dA14				112
#define dA13				128
#define dA12				144
#define dA11				160
#define dA10				176
#define dA09				192
#define dA08				208
#define dA07				224
#define dA06				240
#define dA05				256
#define dA04				272
#define dA03				288
#define dA02				304
#define dA01				320
#define dA00				336
#define dSIGN_MASK			352
#define iCHK_WORK_SUB			368
#define iCHK_WORK_CMP			384
#define dABS_MASK			400
#define dZERO				416

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2vv_atan2_sse4)
	subq	$88, %rsp
	cfi_def_cfa_offset(96)
	movaps	%xmm1, %xmm11

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
	movups	dABS_MASK+__svml_datan2_data_internal(%rip), %xmm1
	movaps	%xmm0, %xmm10
	movaps	%xmm1, %xmm9
	andps	%xmm10, %xmm1
	andps	%xmm11, %xmm9
	movaps	%xmm1, %xmm4
	cmpnltpd %xmm9, %xmm4

	/* Argument signs */
	movups	dSIGN_MASK+__svml_datan2_data_internal(%rip), %xmm5
	movaps	%xmm4, %xmm0
	movaps	%xmm5, %xmm8
	movaps	%xmm5, %xmm7

	/*
	 * 1) If y<x then a= y, b=x, PIO2=0
	 * 2) If y>x then a=-x, b=y, PIO2=Pi/2
	 */
	orps	%xmm9, %xmm5
	andnps	%xmm1, %xmm0
	andps	%xmm4, %xmm5
	andps	%xmm11, %xmm8
	movups	dPIO2+__svml_datan2_data_internal(%rip), %xmm6
	orps	%xmm5, %xmm0
	movaps	%xmm4, %xmm5
	andps	%xmm4, %xmm6
	andnps	%xmm9, %xmm5
	andps	%xmm1, %xmm4
	orps	%xmm4, %xmm5
	andps	%xmm10, %xmm7
	divpd	%xmm5, %xmm0
	movq	iCHK_WORK_SUB+__svml_datan2_data_internal(%rip), %xmm2
	xorl	%edx, %edx

	/* Check if y and x are on main path. */
	pshufd	$221, %xmm9, %xmm3
	xorl	%eax, %eax
	pshufd	$221, %xmm1, %xmm13
	psubd	%xmm2, %xmm3
	psubd	%xmm2, %xmm13
	movdqa	%xmm3, %xmm4
	movq	iCHK_WORK_CMP+__svml_datan2_data_internal(%rip), %xmm12
	movdqa	%xmm13, %xmm14
	pcmpgtd	%xmm12, %xmm4
	pcmpeqd	%xmm12, %xmm3
	pcmpgtd	%xmm12, %xmm14
	pcmpeqd	%xmm12, %xmm13

	/* Polynomial. */
	movaps	%xmm0, %xmm12
	por	%xmm3, %xmm4
	mulpd	%xmm0, %xmm12

	/* P = A19*R2 + A18 */
	movups	dA19+__svml_datan2_data_internal(%rip), %xmm15
	movaps	%xmm11, %xmm2
	mulpd	%xmm12, %xmm15
	addpd	dA18+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A17 */
	mulpd	%xmm12, %xmm15
	addpd	dA17+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A16 */
	mulpd	%xmm12, %xmm15
	addpd	dA16+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A15 */
	mulpd	%xmm12, %xmm15
	addpd	dA15+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A14 */
	mulpd	%xmm12, %xmm15
	addpd	dA14+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A13 */
	mulpd	%xmm12, %xmm15
	addpd	dA13+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A12 */
	mulpd	%xmm12, %xmm15
	addpd	dA12+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A11 */
	mulpd	%xmm12, %xmm15
	addpd	dA11+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A10 */
	mulpd	%xmm12, %xmm15
	addpd	dA10+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A09 */
	mulpd	%xmm12, %xmm15
	addpd	dA09+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A08 */
	mulpd	%xmm12, %xmm15
	addpd	dA08+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A07 */
	mulpd	%xmm12, %xmm15
	addpd	dA07+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A06 */
	mulpd	%xmm12, %xmm15
	addpd	dA06+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A05 */
	mulpd	%xmm12, %xmm15
	addpd	dA05+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A04 */
	mulpd	%xmm12, %xmm15
	addpd	dA04+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A03 */
	mulpd	%xmm12, %xmm15
	addpd	dA03+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A02 */
	mulpd	%xmm12, %xmm15
	addpd	dA02+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 + A01 */
	mulpd	%xmm12, %xmm15
	addpd	dA01+__svml_datan2_data_internal(%rip), %xmm15

	/* P = P*R2 */
	mulpd	%xmm15, %xmm12

	/*
	 * Reconstruction.
	 * dP=(R+R*dP) + dPIO2
	 */
	mulpd	%xmm0, %xmm12
	addpd	%xmm12, %xmm0

	/* if x<0, dPI = Pi, else dPI =0 */
	movups	dZERO+__svml_datan2_data_internal(%rip), %xmm3
	por	%xmm13, %xmm14
	cmplepd	%xmm3, %xmm2
	addpd	%xmm6, %xmm0
	andps	__svml_datan2_data_internal(%rip), %xmm2
	orps	%xmm8, %xmm0
	addpd	%xmm2, %xmm0
	por	%xmm14, %xmm4
	orps	%xmm7, %xmm0
	movmskps %xmm4, %ecx

	/*  Special branch for fast (vector) processing of zero arguments  */
	testb	$3, %cl

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7 xmm8 xmm9 xmm10 xmm11

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
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0 xmm10 xmm11

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	addq	$88, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(96)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm10, 32(%rsp)
	movups	%xmm11, 48(%rsp)
	movups	%xmm0, 64(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0

	movq	%r12, 16(%rsp)
	cfi_offset(12, -80)
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -88)
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	cfi_offset(14, -96)
	# LOE rbx rbp r15 r12d r13d

	/* Range mask
	 * bits check
	 */

L(RANGEMASK_CHECK):
	btl	%r12d, %r13d

	/* Call scalar math function */
	jc	L(SCALAR_MATH_CALL)
	# LOE rbx rbp r15 r12d r13d

	/* Special inputs
	 * processing loop
	 */

L(SPECIAL_VALUES_LOOP):
	incl	%r12d
	cmpl	$2, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	64(%rsp), %xmm0

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -80)
	cfi_offset(13, -88)
	cfi_offset(14, -96)
	# LOE rbx rbp r12 r13 r14 r15 xmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	movsd	48(%rsp, %r14, 8), %xmm1
	call	atan2@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 64(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	cfi_restore(12)
	cfi_restore(13)
	cfi_restore(14)
	# LOE rbx rbp r15 r12d r13d

	/* Auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH):
	/* Check if both X & Y are not NaNs:  iXYnotNAN */
	movaps	%xmm11, %xmm13
	movaps	%xmm10, %xmm12
	cmpordpd %xmm11, %xmm13
	cmpordpd %xmm10, %xmm12

	/* Check if at least on of Y or Y is zero: iAXAYZERO */
	cmpeqpd	%xmm3, %xmm9
	cmpeqpd	%xmm3, %xmm1

	/*
	 *  Path for zero arguments (at least one of both)
	 * Check if both args are zeros (den. is zero)
	 */
	cmpeqpd	%xmm3, %xmm5
	andps	%xmm12, %xmm13
	orps	%xmm1, %xmm9
	pshufd	$221, %xmm9, %xmm1
	pshufd	$221, %xmm13, %xmm9

	/* Check if at least on of Y or Y is zero and not NaN: iAXAYZEROnotNAN */
	pand	%xmm9, %xmm1

	/* Exclude from previous callout mask zero (and not NaN) arguments */
	movdqa	%xmm1, %xmm14
	pandn	%xmm4, %xmm14

	/* Set sPIO2 to zero if den. is zero */
	movaps	%xmm5, %xmm4
	andnps	%xmm6, %xmm4
	andps	%xmm3, %xmm5

	/* Res = sign(Y)*(X<0)?(PIO2+PI):PIO2 */
	pshufd	$221, %xmm3, %xmm3
	orps	%xmm5, %xmm4
	pshufd	$221, %xmm11, %xmm5
	orps	%xmm8, %xmm4
	pcmpgtd	%xmm5, %xmm3
	pshufd	$80, %xmm3, %xmm6
	andps	%xmm2, %xmm6
	addpd	%xmm6, %xmm4

	/* Go to callout */
	movmskps %xmm14, %edx

	/* Merge results from main and spec path */
	pshufd	$80, %xmm1, %xmm2
	orps	%xmm7, %xmm4
	movdqa	%xmm2, %xmm7
	andps	%xmm2, %xmm4
	andnps	%xmm0, %xmm7
	andl	$3, %edx
	movaps	%xmm7, %xmm0
	orps	%xmm4, %xmm0

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0 xmm10 xmm11
END(_ZGVbN2vv_atan2_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_datan2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 dPI[2][2];
	__declspec(align(16)) VUINT32 dPIO2[2][2];
	__declspec(align(16)) VUINT32 dA19[2][2];
	__declspec(align(16)) VUINT32 dA18[2][2];
	__declspec(align(16)) VUINT32 dA17[2][2];
	__declspec(align(16)) VUINT32 dA16[2][2];
	__declspec(align(16)) VUINT32 dA15[2][2];
	__declspec(align(16)) VUINT32 dA14[2][2];
	__declspec(align(16)) VUINT32 dA13[2][2];
	__declspec(align(16)) VUINT32 dA12[2][2];
	__declspec(align(16)) VUINT32 dA11[2][2];
	__declspec(align(16)) VUINT32 dA10[2][2];
	__declspec(align(16)) VUINT32 dA09[2][2];
	__declspec(align(16)) VUINT32 dA08[2][2];
	__declspec(align(16)) VUINT32 dA07[2][2];
	__declspec(align(16)) VUINT32 dA06[2][2];
	__declspec(align(16)) VUINT32 dA05[2][2];
	__declspec(align(16)) VUINT32 dA04[2][2];
	__declspec(align(16)) VUINT32 dA03[2][2];
	__declspec(align(16)) VUINT32 dA02[2][2];
	__declspec(align(16)) VUINT32 dA01[2][2];
	__declspec(align(16)) VUINT32 dA00[2][2];
	__declspec(align(16)) VUINT32 dSIGN_MASK[2][2];
	__declspec(align(16)) VUINT32 iCHK_WORK_SUB[4][1];
	__declspec(align(16)) VUINT32 iCHK_WORK_CMP[4][1];
	__declspec(align(16)) VUINT32 dABS_MASK[2][2];
	__declspec(align(16)) VUINT32 dZERO[2][2];
} __svml_datan2_data_internal;
#endif
__svml_datan2_data_internal:
	.quad	0x400921FB54442D18, 0x400921FB54442D18 // dPI
	.align	16
	.quad	0x3FF921FB54442D18, 0x3FF921FB54442D18 // dPIO2
	.align	16
	.quad	0xBEF4FDB537ABC7A3, 0xBEF4FDB537ABC7A3 // dA19
	.align	16
	.quad	0x3F2CED0A36665209, 0x3F2CED0A36665209 // dA18
	.align	16
	.quad	0xBF52E67C93954C23, 0xBF52E67C93954C23 // dA17
	.align	16
	.quad	0x3F6F5A1DAE82AFB3, 0x3F6F5A1DAE82AFB3 // dA16
	.align	16
	.quad	0xBF82B2EC618E4BAD, 0xBF82B2EC618E4BAD // dA15
	.align	16
	.quad	0x3F914F4C661116A5, 0x3F914F4C661116A5 // dA14
	.align	16
	.quad	0xBF9A5E83B081F69C, 0xBF9A5E83B081F69C // dA13
	.align	16
	.quad	0x3FA169980CB6AD4F, 0x3FA169980CB6AD4F // dA12
	.align	16
	.quad	0xBFA4EFA2E563C1BC, 0xBFA4EFA2E563C1BC // dA11
	.align	16
	.quad	0x3FA7EC0FBC50683B, 0x3FA7EC0FBC50683B // dA10
	.align	16
	.quad	0xBFAAD261EAA09954, 0xBFAAD261EAA09954 // dA09
	.align	16
	.quad	0x3FAE1749BD612DCF, 0x3FAE1749BD612DCF // dA08
	.align	16
	.quad	0xBFB11084009435E0, 0xBFB11084009435E0 // dA07
	.align	16
	.quad	0x3FB3B12A49295651, 0x3FB3B12A49295651 // dA06
	.align	16
	.quad	0xBFB745D009BADA94, 0xBFB745D009BADA94 // dA05
	.align	16
	.quad	0x3FBC71C707F7D5B5, 0x3FBC71C707F7D5B5 // dA04
	.align	16
	.quad	0xBFC2492491EE55C7, 0xBFC2492491EE55C7 // dA03
	.align	16
	.quad	0x3FC999999997EE34, 0x3FC999999997EE34 // dA02
	.align	16
	.quad	0xBFD55555555553C5, 0xBFD55555555553C5 // dA01
	.align	16
	.quad	0x3FF0000000000000, 0x3FF0000000000000 // dA00
	.align	16
	.quad	0x8000000000000000, 0x8000000000000000 // dSIGN_MASK
	.align	16
	.long	0x80300000, 0x80300000, 0x80300000, 0x80300000 // iCHK_WORK_SUB
	.align	16
	.long	0xfdd00000, 0xfdd00000, 0xfdd00000, 0xfdd00000 // iCHK_WORK_CMP
	.align	16
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff // dABS_MASK
	.align	16
	.quad	0x0000000000000000, 0x0000000000000000 // dZERO
	.align	16
	.type	__svml_datan2_data_internal, @object
	.size	__svml_datan2_data_internal, .-__svml_datan2_data_internal
