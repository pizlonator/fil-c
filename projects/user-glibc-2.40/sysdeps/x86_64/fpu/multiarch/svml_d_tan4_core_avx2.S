/* Function tan vectorized with AVX2.
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
 *      1) Range reduction to [-Pi/4; +Pi/4] interval
 *       a) Grab sign from source argument and save it.
 *       b) Remove sign using AND 0x7fffffffffffffff operation
 *       c) Getting octant Y by 2/Pi multiplication
 *       d) Add "Right Shifter" (0x4330000000000000) value
 *       e) Treat obtained value as integer for destination sign setting.
 *          Shift first bit of this value to the last (sign) position (S << 63)
 *       f) Change destination sign if source sign is negative
 *          using XOR operation.
 *       g) Subtract "Right Shifter" (0x4330000000000000) value
 *       h) Subtract Y*(PI/2) from X argument, where PI/2 divided to 4 parts:
 *          X = X - Y*PI1 - Y*PI2 - Y*PI3 - Y*PI4;
 *      2) Rational polynomial approximation ( at [-Pi/4; +Pi/4] interval)
 *       a) Calculate X^2 = X * X
 *       b) Calculate 2 polynomials:
 *          P = X * (P0 + X^2 * (P1 + x^2 * (P2 + x^2 * (P3))));
 *          Q = Q0 + X^2 * (Q1 + x^2 * (Q2 + x^2 * (Q3)));
 *       c) Swap P and Q if first bit of obtained value after
 *          Right Shifting is set to 1. Using And, Andnot & Or operations.
 *       d) Divide R = P / Q;
 *      3) Destination sign setting
 *       a) Set shifted destination sign using XOR operation:
 *          R = XOR( R, S );
 *
 */

/* Offsets for data table __svml_dtan_data_internal
 */
#define _dAbsMask			0
#define _dRangeVal			32
#define _dRShift			64
#define _dCoeffs			96
#define _dReductionRangeVal		26720
#define _dInvPi				26752
#define _dPI1				26784
#define _dPI2				26816
#define _dPI3				26848
#define _dPI1_FMA			26880
#define _dPI2_FMA			26912
#define _dPI3_FMA			26944
#define _dP1				26976
#define _dP2				27008
#define _dP3				27040
#define _dQ0				27072
#define _dQ1				27104
#define _dQ2				27136
#define _dQ3				27168

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4v_tan_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/* Legacy Code */
	xorl	%r9d, %r9d
	vmovupd	_dAbsMask+__svml_dtan_data_internal(%rip), %ymm14
	vmovupd	_dRShift+__svml_dtan_data_internal(%rip), %ymm2
	vmovupd	_dInvPi+__svml_dtan_data_internal(%rip), %ymm6
	vmovupd	_dPI1_FMA+__svml_dtan_data_internal(%rip), %ymm3
	vmovupd	_dP3+__svml_dtan_data_internal(%rip), %ymm9
	vmovapd	%ymm0, %ymm15

	/* b) Remove sign using AND 0x7fffffffffffffff operation */
	vandpd	%ymm14, %ymm15, %ymm1

	/*
	 * c) Getting octant Y by 2/Pi multiplication
	 * d) Add "Right Shifter" (0x4330000000000000) value
	 */
	vfmadd213pd %ymm2, %ymm1, %ymm6

	/* g) Subtract "Right Shifter" (0x4330000000000000) value */
	vsubpd	%ymm2, %ymm6, %ymm11
	vpsllq	$62, %ymm6, %ymm7

	/*
	 * e) Treat obtained value as integer for destination sign setting.
	 * Shift first bit of this value to the last (sign) position (S << 63)
	 * f) Change destination sign if source sign is negative
	 * using XOR operation.
	 */
	vpsllq	$63, %ymm6, %ymm4

	/* X = X - Y*PI1 - Y*PI2 - Y*PI3 - Y*PI4; */
	vfnmadd213pd %ymm1, %ymm11, %ymm3
	vfnmadd231pd _dPI2_FMA+__svml_dtan_data_internal(%rip), %ymm11, %ymm3
	vfnmadd132pd _dPI3_FMA+__svml_dtan_data_internal(%rip), %ymm3, %ymm11
	vmovupd	_dQ3+__svml_dtan_data_internal(%rip), %ymm3

	/* a) Calculate X^2 = X * X */
	vmulpd	%ymm11, %ymm11, %ymm10

	/*
	 * b) Calculate 2 polynomials:
	 * P = X * (P0 + X^2 * (P1 + x^2 * (P2 + x^2 * (P3))));
	 * Q = Q0 + X^2 * (Q1 + x^2 * (Q2 + x^2 * (Q3)));
	 * Assume P0 = 1
	 */
	vfmadd213pd _dP2+__svml_dtan_data_internal(%rip), %ymm10, %ymm9
	vfmadd213pd _dQ2+__svml_dtan_data_internal(%rip), %ymm10, %ymm3
	vfmadd213pd _dP1+__svml_dtan_data_internal(%rip), %ymm10, %ymm9
	vfmadd213pd _dQ1+__svml_dtan_data_internal(%rip), %ymm10, %ymm3
	vmulpd	%ymm9, %ymm10, %ymm9
	vfmadd213pd _dQ0+__svml_dtan_data_internal(%rip), %ymm10, %ymm3
	vfmadd213pd %ymm11, %ymm11, %ymm9

	/* 1) Range reduction to [-Pi/4; +Pi/4] interval */
	vxorpd	%ymm8, %ymm8, %ymm8
	vcmpneqpd %ymm8, %ymm7, %ymm2

	/* a) Grab sign from source argument and save it. */
	vandnpd	%ymm15, %ymm14, %ymm5
	vxorpd	%ymm5, %ymm4, %ymm0

	/*
	 * c) Swap P and Q if first bit of obtained value after
	 * Right Shifting is set to 1. Using And, Andnot & Or operations.
	 */
	vandnpd	%ymm9, %ymm2, %ymm12
	vandpd	%ymm3, %ymm2, %ymm13
	vandpd	%ymm9, %ymm2, %ymm4
	vandnpd	%ymm3, %ymm2, %ymm5
	vorpd	%ymm13, %ymm12, %ymm6
	vorpd	%ymm5, %ymm4, %ymm7

	/* d) Divide R = P / Q; */
	vdivpd	%ymm7, %ymm6, %ymm8

	/* Large values check */
	vcmpnle_uqpd _dReductionRangeVal+__svml_dtan_data_internal(%rip), %ymm1, %ymm14
	vmovmskpd %ymm14, %eax

	/*
	 * 3) Destination sign setting
	 * a) Set shifted destination sign using XOR operation:
	 * R = XOR( R, S );
	 */
	vxorpd	%ymm0, %ymm8, %ymm0
	testl	%eax, %eax

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx r12 r13 r14 r15 r9d ymm0 ymm1 ymm14 ymm15

	/* Return from auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH_RETURN):
	testl	%r9d, %r9d

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 r9d ymm0 ymm15

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
	vmovupd	%ymm15, 32(%rsp)
	vmovupd	%ymm0, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 r9d ymm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax r9d

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	movl	%r9d, %r13d
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
	vmovupd	64(%rsp), %ymm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -96; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	32(%rsp, %r14, 8), %xmm0
	call	tan@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 64(%rsp, %r14, 8)

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
	vpand	.FLT_17(%rip), %ymm15, %ymm4

	/*
	 * Get the (2^a / 2pi) mod 1 values from the table.
	 * Because doesn't have L-type gather, we need a trivial cast
	 */
	lea	__svml_dtan_reduction_data_internal(%rip), %r8
	vmovupd	_dRangeVal+__svml_dtan_data_internal(%rip), %ymm6
	vmovupd	%ymm0, (%rsp)
	vpsrlq	$52, %ymm4, %ymm13
	vandpd	%ymm1, %ymm6, %ymm12
	vcmpeqpd %ymm6, %ymm12, %ymm7
	vmovmskpd %ymm7, %r9d
	vextractf128 $1, %ymm13, %xmm8
	vmovd	%xmm13, %r10d
	vmovd	%xmm8, %edx
	vpextrd	$2, %xmm13, %r11d
	lea	(%r10, %r10, 2), %edi
	vpextrd	$2, %xmm8, %eax
	lea	(%rdx, %rdx, 2), %ecx
	shll	$3, %edi
	shll	$3, %ecx

	/*
	 * Also get the significand as an integer
	 * NB: adding in the integer bit is wrong for denorms!
	 * To make this work for denorms we should do something slightly different
	 */
	vpand	.FLT_18(%rip), %ymm15, %ymm13
	lea	(%r11, %r11, 2), %esi
	vmovq	(%rdi, %r8), %xmm10
	vmovq	(%rcx, %r8), %xmm5
	vmovq	8(%rdi, %r8), %xmm3
	shll	$3, %esi
	lea	(%rax, %rax, 2), %r10d
	shll	$3, %r10d
	vmovhpd	(%rsi, %r8), %xmm10, %xmm9
	vmovhpd	(%r10, %r8), %xmm5, %xmm12
	vpaddq	.FLT_19(%rip), %ymm13, %ymm8
	vmovq	16(%rdi, %r8), %xmm1
	vmovq	8(%rcx, %r8), %xmm7
	vmovq	16(%rcx, %r8), %xmm11
	vmovhpd	8(%rsi, %r8), %xmm3, %xmm6

	/*  Load constants (not all needed at once)  */
	lea	_dCoeffs+96+__svml_dtan_data_internal(%rip), %rcx
	vmovhpd	16(%rsi, %r8), %xmm1, %xmm4
	vmovhpd	8(%r10, %r8), %xmm7, %xmm2
	vmovhpd	16(%r10, %r8), %xmm11, %xmm0
	vinsertf128 $1, %xmm12, %ymm9, %ymm10

	/*
	 * Break the P_xxx and m into 32-bit chunks ready for
	 * the long multiplication via 32x32->64 multiplications
	 */
	vmovdqu	.FLT_20(%rip), %ymm12
	vpand	%ymm12, %ymm10, %ymm7
	vpsrlq	$32, %ymm10, %ymm10
	vinsertf128 $1, %xmm2, %ymm6, %ymm9
	vinsertf128 $1, %xmm0, %ymm4, %ymm11
	vpsrlq	$32, %ymm9, %ymm4
	vpand	%ymm12, %ymm9, %ymm2
	vpsrlq	$32, %ymm8, %ymm9
	vpand	%ymm12, %ymm11, %ymm5
	vpsrlq	$32, %ymm11, %ymm0
	vpand	%ymm12, %ymm8, %ymm6
	vpmuludq %ymm5, %ymm9, %ymm11
	vpsrlq	$32, %ymm11, %ymm5
	vpmuludq %ymm0, %ymm9, %ymm11
	vpmuludq %ymm0, %ymm6, %ymm0
	vpsrlq	$32, %ymm0, %ymm0
	vpmuludq %ymm2, %ymm6, %ymm1
	vpand	%ymm12, %ymm1, %ymm13
	vpaddq	%ymm0, %ymm11, %ymm11
	vpsrlq	$32, %ymm1, %ymm1
	vpaddq	%ymm11, %ymm13, %ymm13
	vpmuludq %ymm4, %ymm6, %ymm3
	vpmuludq %ymm2, %ymm9, %ymm2
	vpaddq	%ymm13, %ymm5, %ymm11
	vpand	%ymm12, %ymm3, %ymm5
	vpaddq	%ymm1, %ymm2, %ymm2
	vpsrlq	$32, %ymm3, %ymm3
	vpsrlq	$32, %ymm11, %ymm13
	vpaddq	%ymm2, %ymm5, %ymm5
	vpmuludq %ymm7, %ymm6, %ymm8
	vpaddq	%ymm5, %ymm13, %ymm5
	vpand	%ymm12, %ymm8, %ymm1
	vpsrlq	$32, %ymm5, %ymm0
	vpsllq	$32, %ymm5, %ymm5
	vpmuludq %ymm4, %ymm9, %ymm13
	vpaddq	%ymm3, %ymm13, %ymm13
	vpsrlq	$32, %ymm8, %ymm4
	vpaddq	%ymm13, %ymm1, %ymm3
	vpmuludq %ymm10, %ymm6, %ymm6

	/* Now do the big multiplication and carry propagation */
	vpmuludq %ymm7, %ymm9, %ymm7
	vpaddq	%ymm3, %ymm0, %ymm2
	vpand	%ymm12, %ymm6, %ymm10
	vpaddq	%ymm4, %ymm7, %ymm9
	vpsrlq	$32, %ymm2, %ymm13
	vpand	%ymm12, %ymm2, %ymm6
	vpaddq	%ymm9, %ymm10, %ymm8

	/* Assemble reduced argument from the pieces */
	vpand	%ymm12, %ymm11, %ymm12
	vpaddq	%ymm8, %ymm13, %ymm13

	/*
	 * We want to incorporate the original sign now too.
	 * Do it here for convenience in getting the right N value,
	 * though we could wait right to the end if we were prepared
	 * to modify the sign of N later too.
	 * So get the appropriate sign mask now (or sooner).
	 */
	vpand	.FLT_21(%rip), %ymm15, %ymm8
	vpaddq	%ymm12, %ymm5, %ymm9

	/*
	 * Now round at the 2^-9 bit position for reduction mod pi/2^8
	 * instead of the original 2pi (but still with the same 2pi scaling).
	 * Use a shifter of 2^43 + 2^42.
	 * The N we get is our final version; it has an offset of
	 * 2^9 because of the implicit integer bit, and anyway for negative
	 * starting value it's a 2s complement thing. But we need to mask
	 * off the exponent part anyway so it's fine.
	 */
	vmovupd	.FLT_23(%rip), %ymm5
	vpsllq	$32, %ymm13, %ymm3

	/*
	 * Create floating-point high part, implicitly adding integer bit 1
	 * Incorporate overall sign at this stage too.
	 */
	vpxor	.FLT_22(%rip), %ymm8, %ymm10

	/*
	 * Create floating-point low and medium parts, respectively
	 * lo_23, ... lo_0, 0, ..., 0
	 * hi_11, ... hi_0, lo_63, ..., lo_24
	 * then subtract off the implicitly added integer bits,
	 * 2^-104 and 2^-52, respectively.
	 * Put the original sign into all of them at this stage.
	 */
	vpxor	.FLT_24(%rip), %ymm8, %ymm4
	vpand	.FLT_25(%rip), %ymm9, %ymm7
	vpaddq	%ymm6, %ymm3, %ymm3
	vpsrlq	$24, %ymm9, %ymm9
	vpsllq	$28, %ymm7, %ymm2
	vpsrlq	$12, %ymm3, %ymm13
	vpor	%ymm4, %ymm2, %ymm1
	vpor	%ymm10, %ymm13, %ymm12
	vsubpd	%ymm4, %ymm1, %ymm13
	vaddpd	%ymm5, %ymm12, %ymm10
	vsubpd	%ymm5, %ymm10, %ymm6
	vpxor	.FLT_26(%rip), %ymm8, %ymm5
	vpand	.FLT_27(%rip), %ymm3, %ymm8
	vsubpd	%ymm6, %ymm12, %ymm0
	vpsllq	$40, %ymm8, %ymm3
	vpor	%ymm9, %ymm3, %ymm11

	/*
	 * If the magnitude of the input is <= 2^-20, then
	 * just pass through the input, since no reduction will be needed and
	 * the main path will only work accurately if the reduced argument is
	 * about >= 2^-70 (which it is for all large pi multiples)
	 */
	vmovupd	.FLT_31(%rip), %ymm3
	vpor	%ymm5, %ymm11, %ymm9
	vsubpd	%ymm5, %ymm9, %ymm8

	/* Now add them up into 2 reasonably aligned pieces */
	vaddpd	%ymm8, %ymm0, %ymm12
	vsubpd	%ymm12, %ymm0, %ymm0
	vaddpd	%ymm0, %ymm8, %ymm9

	/*
	 * Now multiply those numbers all by 2 pi, reasonably accurately.
	 * (RHi + RLo) * (pi_lead + pi_trail) ~=
	 * RHi * pi_lead + (RHi * pi_trail + RLo * pi_lead)
	 */
	vmovupd	.FLT_28(%rip), %ymm8
	vaddpd	%ymm9, %ymm13, %ymm11
	vmulpd	%ymm12, %ymm8, %ymm7
	vmovapd	%ymm8, %ymm13
	vfmsub213pd %ymm7, %ymm12, %ymm13

	/* Grab our final N value as an integer, appropriately masked mod 2^9 */
	vandps	.FLT_32(%rip), %ymm10, %ymm10

	/*
	 * Argument reduction is now finished: x = n * pi/256 + r
	 * where n = lIndex and r = dZ
	 * But we have n modulo 512, needed for sin/cos with period 2pi
	 * but we want it modulo 256 since tan has period pi.
	 */
	vandps	.FLT_33(%rip), %ymm10, %ymm10
	vandpd	.FLT_30(%rip), %ymm15, %ymm5
	vcmpgt_oqpd %ymm3, %ymm5, %ymm4
	vcmple_oqpd %ymm3, %ymm5, %ymm6
	vfmadd132pd .FLT_29(%rip), %ymm13, %ymm12
	vfmadd213pd %ymm12, %ymm8, %ymm11
	vandpd	%ymm15, %ymm6, %ymm2
	vandpd	%ymm7, %ymm4, %ymm1
	vorpd	%ymm1, %ymm2, %ymm0
	vandpd	%ymm11, %ymm4, %ymm13

	/*
	 * The output is _VRES_Z (high) + _VRES_E (low), and the integer part is _VRES_IND
	 * Simply absorb E into Z instead of keeping a 2-part result.
	 */
	vaddpd	%ymm13, %ymm0, %ymm13
	vextractf128 $1, %ymm10, %xmm5
	vmovd	%xmm10, %edi
	vmovd	%xmm5, %r11d
	imull	$104, %edi, %edx
	imull	$104, %r11d, %esi
	vmovq	-96(%rdx, %rcx), %xmm9
	vmovq	-80(%rdx, %rcx), %xmm6
	vpextrd	$2, %xmm10, %r8d
	vpextrd	$2, %xmm5, %edi
	imull	$104, %r8d, %eax
	imull	$104, %edi, %r8d
	vmovq	-96(%rsi, %rcx), %xmm12
	vmovq	-72(%rdx, %rcx), %xmm4
	vmovq	-80(%rsi, %rcx), %xmm2
	vmovhpd	-96(%rax, %rcx), %xmm9, %xmm8
	vmovhpd	-96(%r8, %rcx), %xmm12, %xmm3
	vmovq	-56(%rdx, %rcx), %xmm9
	vmovq	-72(%rsi, %rcx), %xmm0
	vmovq	-56(%rsi, %rcx), %xmm5
	vmovhpd	-80(%rax, %rcx), %xmm6, %xmm7
	vmovhpd	-72(%rax, %rcx), %xmm4, %xmm11
	vmovhpd	-80(%r8, %rcx), %xmm2, %xmm1
	vmovhpd	-72(%r8, %rcx), %xmm0, %xmm10
	vmovq	-48(%rdx, %rcx), %xmm6
	vmovq	-48(%rsi, %rcx), %xmm0
	vinsertf128 $1, %xmm3, %ymm8, %ymm12
	vmovhpd	-56(%rax, %rcx), %xmm9, %xmm8
	vmovhpd	-56(%r8, %rcx), %xmm5, %xmm3
	vmovq	-40(%rdx, %rcx), %xmm9
	vmovq	-40(%rsi, %rcx), %xmm5
	vinsertf128 $1, %xmm1, %ymm7, %ymm2
	vinsertf128 $1, %xmm10, %ymm11, %ymm1
	vinsertf128 $1, %xmm3, %ymm8, %ymm4
	vmovhpd	-48(%rax, %rcx), %xmm6, %xmm7
	vmovhpd	-40(%rax, %rcx), %xmm9, %xmm8
	vmovq	-32(%rdx, %rcx), %xmm11
	vmovhpd	-48(%r8, %rcx), %xmm0, %xmm10
	vmovhpd	-40(%r8, %rcx), %xmm5, %xmm6
	vmovq	-32(%rsi, %rcx), %xmm9
	vmovhpd	-32(%rax, %rcx), %xmm11, %xmm0
	vmovq	-24(%rsi, %rcx), %xmm11
	vmovq	-24(%rdx, %rcx), %xmm5

	/*
	 *  Sum of dominant component(s)
	 * Compute C0_hi + C1_hi * Z + Recip_hi = H4
	 * H2 = C0_hi + C1_hi * Z (exact since C1_hi is 1 bit)
	 */
	vfmadd213pd %ymm1, %ymm13, %ymm4

	/*
	 *  Higher polynomial terms
	 * Stage 1 (with unlimited parallelism)
	 * Z2 = Z^2
	 */
	vmulpd	%ymm13, %ymm13, %ymm1
	vinsertf128 $1, %xmm10, %ymm7, %ymm3
	vinsertf128 $1, %xmm6, %ymm8, %ymm10
	vmovhpd	-32(%r8, %rcx), %xmm9, %xmm8
	vmovhpd	-24(%r8, %rcx), %xmm11, %xmm9
	vmovq	-16(%rsi, %rcx), %xmm11
	vmovhpd	-24(%rax, %rcx), %xmm5, %xmm7

	/* P4 = C1_lo + C2 * Z */
	vfmadd213pd %ymm3, %ymm13, %ymm10
	vinsertf128 $1, %xmm8, %ymm0, %ymm6
	vmovq	-16(%rdx, %rcx), %xmm8
	vmovhpd	-16(%rax, %rcx), %xmm8, %xmm5
	vmovhpd	-16(%r8, %rcx), %xmm11, %xmm0
	vmovq	-8(%rdx, %rcx), %xmm8
	vmovq	-8(%rsi, %rcx), %xmm11
	vinsertf128 $1, %xmm9, %ymm7, %ymm9
	vinsertf128 $1, %xmm0, %ymm5, %ymm7
	vmovhpd	-8(%rax, %rcx), %xmm8, %xmm5
	vmovhpd	-8(%r8, %rcx), %xmm11, %xmm0

	/* P5 = C3 + C4 * Z */
	vfmadd213pd %ymm6, %ymm13, %ymm9

	/* P10 = C1_lo + C2 * Z + C3 * Z^2 + C4 * Z^3 */
	vfmadd213pd %ymm10, %ymm1, %ymm9
	vinsertf128 $1, %xmm0, %ymm5, %ymm8
	vmovq	(%rdx, %rcx), %xmm5
	vmovq	(%rsi, %rcx), %xmm0
	vmovhpd	(%rax, %rcx), %xmm5, %xmm11
	vmovhpd	(%r8, %rcx), %xmm0, %xmm5

	/* P6 = C5 + C6 * Z */
	vfmadd213pd %ymm7, %ymm13, %ymm8
	vinsertf128 $1, %xmm5, %ymm11, %ymm5

	/*
	 *  Compute reciprocal component
	 * Construct a separate reduced argument modulo pi near pi/2 multiples.
	 * i.e. (pi/2 - x) mod pi, simply by subtracting the reduced argument
	 * from an accurate B_hi = (128 - n) pi/256.
	 */
	vsubpd	%ymm13, %ymm12, %ymm11

	/* P9 = C5 + C6 * Z + C7 * Z^2 */
	vfmadd213pd %ymm8, %ymm1, %ymm5

	/* Now compute an approximate reciprocal to mix into the computation. */
	vcvtpd2ps %ymm11, %xmm12
	vrcpps	%xmm12, %xmm12
	vcvtps2pd %xmm12, %ymm0

	/*
	 * Now compute the error dEr where dRecip_hi = (1/R_full) * (1 - dEr)
	 * so that we can compensate for it.
	 */
	vfnmadd213pd .FLT_34(%rip), %ymm0, %ymm11

	/*
	 * Get a working-precision reciprocal 1/dR_full
	 * using a fourth-order polynomial approximation
	 * R + (E*R) * (1 + E) * (1 + E^2)
	 */
	vmulpd	%ymm11, %ymm11, %ymm12
	vfmadd213pd %ymm11, %ymm11, %ymm11
	vfmadd213pd %ymm0, %ymm0, %ymm12
	vfmadd213pd %ymm0, %ymm11, %ymm12

	/*
	 *
	 * End of large arguments path
	 *
	 * Merge results from main and large paths:
	 */
	vmovupd	(%rsp), %ymm0

	/*
	 * dRecip_hi is only used when dTau is one (cotangent path)
	 * H4 = C0_hi + C1_hi * Z + Recip_hi
	 */
	vfmadd213pd %ymm4, %ymm2, %ymm12

	/*
	 * Stage 2 (with unlimited parallelism)
	 * Z4 = Z^4
	 */
	vmulpd	%ymm1, %ymm1, %ymm2

	/*
	 * Stage 3 (with unlimited parallelism)
	 * P12 = C1_lo + C2 * Z + ... + C7 * Z^6
	 */
	vfmadd213pd %ymm9, %ymm2, %ymm5

	/* And now the very final summation */
	vfmadd213pd %ymm12, %ymm13, %ymm5
	vblendvpd %ymm14, %ymm5, %ymm0, %ymm0

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx r12 r13 r14 r15 r9d ymm0 ymm15
END(_ZGVdN4v_tan_avx2)

	.section .rodata, "a"
	.align	32

.FLT_17:
	.long	0x00000000, 0x7ff00000, 0x00000000, 0x7ff00000, 0x00000000, 0x7ff00000, 0x00000000, 0x7ff00000
	.type	.FLT_17, @object
	.size	.FLT_17, 32
	.align	32

.FLT_18:
	.long	0xffffffff, 0x000fffff, 0xffffffff, 0x000fffff, 0xffffffff, 0x000fffff, 0xffffffff, 0x000fffff
	.type	.FLT_18, @object
	.size	.FLT_18, 32
	.align	32

.FLT_19:
	.long	0x00000000, 0x00100000, 0x00000000, 0x00100000, 0x00000000, 0x00100000, 0x00000000, 0x00100000
	.type	.FLT_19, @object
	.size	.FLT_19, 32
	.align	32

.FLT_20:
	.long	0xffffffff, 0x00000000, 0xffffffff, 0x00000000, 0xffffffff, 0x00000000, 0xffffffff, 0x00000000
	.type	.FLT_20, @object
	.size	.FLT_20, 32
	.align	32

.FLT_21:
	.long	0x00000000, 0x80000000, 0x00000000, 0x80000000, 0x00000000, 0x80000000, 0x00000000, 0x80000000
	.type	.FLT_21, @object
	.size	.FLT_21, 32
	.align	32

.FLT_22:
	.long	0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000
	.type	.FLT_22, @object
	.size	.FLT_22, 32
	.align	32

.FLT_23:
	.long	0x00000000, 0x42a80000, 0x00000000, 0x42a80000, 0x00000000, 0x42a80000, 0x00000000, 0x42a80000
	.type	.FLT_23, @object
	.size	.FLT_23, 32
	.align	32

.FLT_24:
	.long	0x00000000, 0x39700000, 0x00000000, 0x39700000, 0x00000000, 0x39700000, 0x00000000, 0x39700000
	.type	.FLT_24, @object
	.size	.FLT_24, 32
	.align	32

.FLT_25:
	.long	0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000
	.type	.FLT_25, @object
	.size	.FLT_25, 32
	.align	32

.FLT_26:
	.long	0x00000000, 0x3cb00000, 0x00000000, 0x3cb00000, 0x00000000, 0x3cb00000, 0x00000000, 0x3cb00000
	.type	.FLT_26, @object
	.size	.FLT_26, 32
	.align	32

.FLT_27:
	.long	0x00000fff, 0x00000000, 0x00000fff, 0x00000000, 0x00000fff, 0x00000000, 0x00000fff, 0x00000000
	.type	.FLT_27, @object
	.size	.FLT_27, 32
	.align	32

.FLT_28:
	.long	0x54442d18, 0x401921fb, 0x54442d18, 0x401921fb, 0x54442d18, 0x401921fb, 0x54442d18, 0x401921fb
	.type	.FLT_28, @object
	.size	.FLT_28, 32
	.align	32

.FLT_29:
	.long	0x33145c07, 0x3cb1a626, 0x33145c07, 0x3cb1a626, 0x33145c07, 0x3cb1a626, 0x33145c07, 0x3cb1a626
	.type	.FLT_29, @object
	.size	.FLT_29, 32
	.align	32

.FLT_30:
	.long	0xffffffff, 0x7fffffff, 0xffffffff, 0x7fffffff, 0xffffffff, 0x7fffffff, 0xffffffff, 0x7fffffff
	.type	.FLT_30, @object
	.size	.FLT_30, 32
	.align	32

.FLT_31:
	.long	0x00000000, 0x3eb00000, 0x00000000, 0x3eb00000, 0x00000000, 0x3eb00000, 0x00000000, 0x3eb00000
	.type	.FLT_31, @object
	.size	.FLT_31, 32
	.align	32

.FLT_32:
	.long	0x000001ff, 0x00000000, 0x000001ff, 0x00000000, 0x000001ff, 0x00000000, 0x000001ff, 0x00000000
	.type	.FLT_32, @object
	.size	.FLT_32, 32
	.align	32

.FLT_33:
	.long	0x000000ff, 0x00000000, 0x000000ff, 0x00000000, 0x000000ff, 0x00000000, 0x000000ff, 0x00000000
	.type	.FLT_33, @object
	.size	.FLT_33, 32
	.align	32

.FLT_34:
	.long	0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000
	.type	.FLT_34, @object
	.size	.FLT_34, 32
	.align	32

#ifdef __svml_dtan_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _dAbsMask[4][2];
	__declspec(align(32)) VUINT32 _dRangeVal[4][2];
	__declspec(align(32)) VUINT32 _dRShift[4][2];
	__declspec(align(32)) VUINT32 _dCoeffs[256][13][2];
	__declspec(align(32)) VUINT32 _dReductionRangeVal[4][2];
	__declspec(align(32)) VUINT32 _dInvPi[4][2];
	__declspec(align(32)) VUINT32 _dPI1[4][2];
	__declspec(align(32)) VUINT32 _dPI2[4][2];
	__declspec(align(32)) VUINT32 _dPI3[4][2];
	__declspec(align(32)) VUINT32 _dPI1_FMA[4][2];
	__declspec(align(32)) VUINT32 _dPI2_FMA[4][2];
	__declspec(align(32)) VUINT32 _dPI3_FMA[4][2];
	__declspec(align(32)) VUINT32 _dP1[4][2];
	__declspec(align(32)) VUINT32 _dP2[4][2];
	__declspec(align(32)) VUINT32 _dP3[4][2];
	__declspec(align(32)) VUINT32 _dQ0[4][2];
	__declspec(align(32)) VUINT32 _dQ1[4][2];
	__declspec(align(32)) VUINT32 _dQ2[4][2];
	__declspec(align(32)) VUINT32 _dQ3[4][2];
} __svml_dtan_data_internal;
#endif
__svml_dtan_data_internal:
	/* Shared value*/
	.quad	0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF /* _dAbsMask */
	.align	32
	.quad	0x7FF0000000000000, 0x7FF0000000000000, 0x7FF0000000000000, 0x7FF0000000000000 /* _dRangeVal */
	.align	32
	.quad	0x4338000000000000, 0x4338000000000000, 0x4338000000000000, 0x4338000000000000 /* _dRShift */
	// _dCoeffs Breakpoint B = 0 * pi/256, function tan(B + x)
	.align	32
	.quad	0x3FF921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C91A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x0000000000000000 // c0 (high double)
	.quad	0x0000000000000000 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x0000000000000000 // c1 (low double)
	.quad	0x0000000000000000 // c2
	.quad	0x3FD5555555555559 // c3
	.quad	0x0000000000000000 // c4
	.quad	0x3FC1111110FE825D // c5
	.quad	0x0000000000000000 // c6
	.quad	0x3FABA1EEC0271523 // c7
	.quad	0x3FF8EFB75D9BA4BE // B' = pi/2 - B (high double)
	.quad	0x3C760B679AB8CD3B // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3F89224E047E368E // c0 (high double)
	.quad	0xBC1A268EB2C67108 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F23BDBEAD8CF800 // c1 (low double)
	.quad	0x3F8923461AE96D62 // c2
	.quad	0x3FD5589FB822D5EE // c3
	.quad	0x3F80C326DCDCFCD0 // c4
	.quad	0x3FC116A999125B7F // c5
	.quad	0x3F7300CC26596ADA // c6
	.quad	0x3FAB7B6F1DE289A8 // c7
	.quad	0x3FF8BD7366F31C64 // B' = pi/2 - B (high double)
	.quad	0xBC7A81C996DFD5A5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3F992346247A91F0 // c0 (high double)
	.quad	0xBBF126D920C15240 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F43BF447B091000 // c1 (low double)
	.quad	0x3F992726F0F98D7F // c2
	.quad	0x3FD5628108EFA55C // c3
	.quad	0x3F90C8A60639C114 // c4
	.quad	0x3FC127789EC0BD38 // c5
	.quad	0x3F830BA6F46C58B7 // c6
	.quad	0x3FABBEE2C1E62B81 // c7
	.quad	0x3FF88B2F704A940A // B' = pi/2 - B (high double)
	.quad	0xBC92C3BEB21E1E21 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FA2DBAAE9A05DB0 // c0 (high double)
	.quad	0xBC4371F88E84877C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F563A086B860B00 // c1 (low double)
	.quad	0x3FA2E23785EEB488 // c2
	.quad	0x3FD572FFC353B741 // c3
	.quad	0x3F993ABD2276BF6A // c4
	.quad	0x3FC1439260886581 // c5
	.quad	0x3F8CAC359DCF5E16 // c6
	.quad	0x3FAB56F9A068D4C5 // c7
	.quad	0x3FF858EB79A20BB0 // B' = pi/2 - B (high double)
	.quad	0xBC9EE70AFE8446D9 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FA927278A3B1162 // c0 (high double)
	.quad	0x3BFB01BD7A51D5C0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F63C55DB08E1680 // c1 (low double)
	.quad	0x3FA936B1EC652161 // c2
	.quad	0x3FD58A26BD6CCCCD // c3
	.quad	0x3FA0DEB5445B4098 // c4
	.quad	0x3FC16B146127E154 // c5
	.quad	0x3F9336A98365D094 // c6
	.quad	0x3FAC19302EB1C4A9 // c7
	.quad	0x3FF826A782F98355 // B' = pi/2 - B (high double)
	.quad	0x3C94F5A8B515906F // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FAF7495EA3F3783 // c0 (high double)
	.quad	0xBC407F10D50F4C0A // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F6EEB8B377B1A00 // c1 (low double)
	.quad	0x3FAF92FABFD0B7B3 // c2
	.quad	0x3FD5A805341437FF // c3
	.quad	0x3FA52B295E2DCF92 // c4
	.quad	0x3FC19E2C59B8C033 // c5
	.quad	0x3F982CF88466EFEC // c6
	.quad	0x3FAD27509D6C3FEA // c7
	.quad	0x3FF7F4638C50FAFB // B' = pi/2 - B (high double)
	.quad	0x3C81A4B8D15ECF6D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FB2E239CCFF3831 // c0 (high double)
	.quad	0x3C5879ED3FF4EEAE // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F764980AEEE3740 // c1 (low double)
	.quad	0x3FB2FC879C81123C // c2
	.quad	0x3FD5CCAEDC1DC96E // c3
	.quad	0x3FA98599B71D86B1 // c4
	.quad	0x3FC1DD150D55E5CE // c5
	.quad	0x3F9D3EB9450DBD01 // c6
	.quad	0x3FADD6CFBE9AA234 // c7
	.quad	0x3FF7C21F95A872A1 // B' = pi/2 - B (high double)
	.quad	0xBC6A877F1DB6080C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FB60B9F7597FDEC // c0 (high double)
	.quad	0x3C3D1E3FBF3B4E18 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F7E5FFEF4D8DFC0 // c1 (low double)
	.quad	0x3FB6357984E124B6 // c2
	.quad	0x3FD5F83BF8B56900 // c3
	.quad	0x3FADF0F50CAA06C2 // c4
	.quad	0x3FC2281476904258 // c5
	.quad	0x3FA138F0F640FD68 // c6
	.quad	0x3FAF8D1BCDD8D11F // c7
	.quad	0x3FF78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0xBC8EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FB936BB8C5B2DA2 // c0 (high double)
	.quad	0x3C587D23392C2BA2 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F83DDE2A8935DE0 // c1 (low double)
	.quad	0x3FB97558DC230BA8 // c2
	.quad	0x3FD62AC976FBC1B8 // c3
	.quad	0x3FB1381EB60159F6 // c4
	.quad	0x3FC27F84438D5029 // c5
	.quad	0x3FA3E63B3A462E45 // c6
	.quad	0x3FAF0AE614B2D473 // c7
	.quad	0x3FF75D97A85761ED // B' = pi/2 - B (high double)
	.quad	0xBC9B97887C831272 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FBC63CE377FC802 // c0 (high double)
	.quad	0xBC543F7D42DBAC49 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F892FE02A47EFA0 // c1 (low double)
	.quad	0x3FBCBD3001C956C5 // c2
	.quad	0x3FD664790F293C47 // c3
	.quad	0x3FB38345DE7C4B0B // c4
	.quad	0x3FC2E3C4C02FFDFB // c5
	.quad	0x3FA6AA7F0C13520B // c6
	.quad	0x3FAFE3C754A9F092 // c7
	.quad	0x3FF72B53B1AED992 // B' = pi/2 - B (high double)
	.quad	0x3C98452B3716C4D6 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FBF93183A8DB9E9 // c0 (high double)
	.quad	0xBC4124AC452772E2 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F8F27A3181EE680 // c1 (low double)
	.quad	0x3FC00707549D7ECC // c2
	.quad	0x3FD6A5716B500730 // c3
	.quad	0x3FB5DB89BB6B245F // c4
	.quad	0x3FC3554867F59648 // c5
	.quad	0x3FA98919BF6D176A // c6
	.quad	0x3FB1AF9A1823147B // c7
	.quad	0x3FF6F90FBB065138 // B' = pi/2 - B (high double)
	.quad	0x3C8843BDD561383D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FC1626D85A91E70 // c0 (high double)
	.quad	0xBC50084844291CB4 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F92E3863C9A69E0 // c1 (low double)
	.quad	0x3FC1B4854BB2B147 // c2
	.quad	0x3FD6EDDE5416DFFB // c3
	.quad	0x3FB8429341CCA17A // c4
	.quad	0x3FC3D49AC2C9F46F // c5
	.quad	0x3FAC85843914CF90 // c6
	.quad	0x3FB1B1DC3FA33DDD // c7
	.quad	0x3FF6C6CBC45DC8DE // B' = pi/2 - B (high double)
	.quad	0xBBD6D61B58C99C43 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FC2FCAC73A60640 // c0 (high double)
	.quad	0x3C52CF95F70B55BA // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F96881A43B33590 // c1 (low double)
	.quad	0x3FC367A033B52F5E // c2
	.quad	0x3FD73DF0E3CC557A // c3
	.quad	0x3FBABA1CAC4F18CD // c4
	.quad	0x3FC4624C36BE941B // c5
	.quad	0x3FAFA38B2F8876EF // c6
	.quad	0x3FB215B1F404D921 // c7
	.quad	0x3FF69487CDB54084 // B' = pi/2 - B (high double)
	.quad	0xBC8849735C376AA4 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FC4986A74CF4E57 // c0 (high double)
	.quad	0x3C69AB0A24392F72 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F9A82B60D75F5C0 // c1 (low double)
	.quad	0x3FC520EA2BA6A0AE // c2
	.quad	0x3FD795DFBFFEE39E // c3
	.quad	0x3FBD43F3A7862CC7 // c4
	.quad	0x3FC4FF04247D700A // c5
	.quad	0x3FB1739726786F4F // c6
	.quad	0x3FB2FAC67533455B // c7
	.quad	0x3FF66243D70CB82A // B' = pi/2 - B (high double)
	.quad	0xBC984805FA81DE0A // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FC635C990CE0D36 // c0 (high double)
	.quad	0xBC59DFC0C48C1978 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F9ED49F1F8144C0 // c1 (low double)
	.quad	0x3FC6E0F98ED00C32 // c2
	.quad	0x3FD7F5E75A2DD66E // c3
	.quad	0x3FBFE1FBA10F6B0E // c4
	.quad	0x3FC5AB7CD1905EB8 // c5
	.quad	0x3FB32A57A4FAE3F9 // c6
	.quad	0x3FB4BA60BA656817 // c7
	.quad	0x3FF62FFFE0642FCF // B' = pi/2 - B (high double)
	.quad	0x3C9B94ADB917F93E // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FC7D4EC54FB5968 // c0 (high double)
	.quad	0x3C4FB65648F8ABF4 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA1BF9C7C60B940 // c1 (low double)
	.quad	0x3FC8A86964A1796B // c2
	.quad	0x3FD85E4A37F23352 // c3
	.quad	0x3FC14B181F5BF6AE // c4
	.quad	0x3FC66888E3A5D3B3 // c5
	.quad	0x3FB4F84D7AC0F5DA // c6
	.quad	0x3FB62BA6F082700C // c7
	.quad	0x3FF5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0x3C8EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FC975F5E0553158 // c0 (high double)
	.quad	0x3C2EEEA31ABB0A10 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA4420303DEC550 // c1 (low double)
	.quad	0x3FCA77D9D62CA610 // c2
	.quad	0x3FD8CF5143323608 // c3
	.quad	0x3FC2B15401455507 // c4
	.quad	0x3FC7370FF0965570 // c5
	.quad	0x3FB6DFEACA56EACB // c6
	.quad	0x3FB6B75DF89DE1EC // c7
	.quad	0x3FF5CB77F3131F1B // B' = pi/2 - B (high double)
	.quad	0x3C6A70A9025D3E70 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FCB1909EFD8B762 // c0 (high double)
	.quad	0x3C5843049E1321E0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA6F2545C478E00 // c1 (low double)
	.quad	0x3FCC4FF0A9D74F70 // c2
	.quad	0x3FD9494C22EF2A54 // c3
	.quad	0x3FC424CB909A6722 // c4
	.quad	0x3FC8180A481D5934 // c5
	.quad	0x3FB8E3D90C36FA83 // c6
	.quad	0x3FB8D27EAFF0DF70 // c7
	.quad	0x3FF59933FC6A96C1 // B' = pi/2 - B (high double)
	.quad	0xBC81AA6E583501D4 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FCCBE4CEB4B4CF2 // c0 (high double)
	.quad	0x3C64A514F790E428 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA9D1724DE6C940 // c1 (low double)
	.quad	0x3FCE3159C5ED3A4E // c2
	.quad	0x3FD9CC919D4271B2 // c3
	.quad	0x3FC5A6A9489AA7F4 // c4
	.quad	0x3FC90C961E2B49F6 // c5
	.quad	0x3FBB06EE7CFA3693 // c6
	.quad	0x3FB9E9447AB1D521 // c7
	.quad	0x3FF566F005C20E67 // B' = pi/2 - B (high double)
	.quad	0xBC94F8837880A9A2 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FCE65E3F27C9F2A // c0 (high double)
	.quad	0x3C6F62A8B8B40082 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FACE04FD3EC1C90 // c1 (low double)
	.quad	0x3FD00E63DD639A45 // c2
	.quad	0x3FDA598003751442 // c3
	.quad	0x3FC73829AC409F93 // c4
	.quad	0x3FCA15E3A26F9D65 // c5
	.quad	0x3FBD4C4574E0CB34 // c6
	.quad	0x3FBB9C5881865EC1 // c7
	.quad	0x3FF534AC0F19860C // B' = pi/2 - B (high double)
	.quad	0x3C9EE4303B192DA6 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD007FA758626AE // c0 (high double)
	.quad	0x3C65A5EB9CA505B6 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB00FF8E583BEBC // c1 (low double)
	.quad	0x3FD1097A2AA570F5 // c2
	.quad	0x3FDAF07DA8CAEBA7 // c3
	.quad	0x3FC8DA9D107525AD // c4
	.quad	0x3FCB3545F2ADC1D6 // c5
	.quad	0x3FBFB739DB465891 // c6
	.quad	0x3FBDA2C23752D365 // c7
	.quad	0x3FF502681870FDB2 // B' = pi/2 - B (high double)
	.quad	0x3C92C0E3EEB304EE // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD0DE53475F3B3C // c0 (high double)
	.quad	0x3C67B2CC0A83B8D0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB1C8B7D76C7EE0 // c1 (low double)
	.quad	0x3FD20A509D4BB6C1 // c2
	.quad	0x3FDB91F96510DF38 // c3
	.quad	0x3FCA8F698D62832E // c4
	.quad	0x3FCC6C340829132A // c5
	.quad	0x3FC125B6D8F6980A // c6
	.quad	0x3FBE80E8F3AEF448 // c7
	.quad	0x3FF4D02421C87558 // B' = pi/2 - B (high double)
	.quad	0x3C7A765E893370D7 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD1B6103D3597E9 // c0 (high double)
	.quad	0x3C6CBCB5FED7BFB0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB39AFA3392E164 // c1 (low double)
	.quad	0x3FD3114C46E2AEF3 // c2
	.quad	0x3FDC3E6B23EF73B5 // c3
	.quad	0x3FCC580D1B42EE7F // c4
	.quad	0x3FCDBC3D6E14D1FF // c5
	.quad	0x3FC28671B66B7EBD // c6
	.quad	0x3FC0BFBCFE643140 // c7
	.quad	0x3FF49DE02B1FECFE // B' = pi/2 - B (high double)
	.quad	0xBC7616D2A8653209 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD28F459ECAD74D // c0 (high double)
	.quad	0x3C302C86D50F5140 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB5875F92EDBDF4 // c1 (low double)
	.quad	0x3FD41ED6D05A0010 // c2
	.quad	0x3FDCF65481FC5BEB // c3
	.quad	0x3FCE361FE028368D // c4
	.quad	0x3FCF2728B941710D // c5
	.quad	0x3FC3FFF67496086B // c6
	.quad	0x3FC1A12109F041C7 // c7
	.quad	0x3FF46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0xBC91A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD36A08355C63DC // c0 (high double)
	.quad	0xBC6D7145B6A0EB84 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB78E922B82AB64 // c1 (low double)
	.quad	0x3FD5335ED87AB0D5 // c2
	.quad	0x3FDDBA417913D89C // c3
	.quad	0x3FD015AB59BD876B // c4
	.quad	0x3FD0576D1EF5DB86 // c5
	.quad	0x3FC594A6DD2CA170 // c6
	.quad	0x3FC2F51FBC1D0708 // c7
	.quad	0x3FF439583DCEDC4A // B' = pi/2 - B (high double)
	.quad	0xBC9DCC4D42E59DF2 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD4466D542BAC92 // c0 (high double)
	.quad	0xBC705073A04E3471 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB9B14751F8BA20 // c1 (low double)
	.quad	0x3FD64F58590170A2 // c2
	.quad	0x3FDE8AC91D15CFCF // c3
	.quad	0x3FD11CC2EF09C664 // c4
	.quad	0x3FD12AB5AB65B965 // c5
	.quad	0x3FC7471C9AC8155B // c6
	.quad	0x3FC4826BA97C3E62 // c7
	.quad	0x3FF40714472653EF // B' = pi/2 - B (high double)
	.quad	0x3C96106670B43955 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD5248AE1701B17 // c0 (high double)
	.quad	0x3C7BE8CC944DBC57 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FBBF04005723CCC // c1 (low double)
	.quad	0x3FD7733D1300258A // c2
	.quad	0x3FDF688E6ACC7DF5 // c3
	.quad	0x3FD231520EA793C8 // c4
	.quad	0x3FD20E93C5183A32 // c5
	.quad	0x3FC91A331D55B3EE // c6
	.quad	0x3FC651721E153056 // c7
	.quad	0x3FF3D4D0507DCB95 // B' = pi/2 - B (high double)
	.quad	0x3C83DA34489C213B // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD604775FBB27DF // c0 (high double)
	.quad	0x3C6AC647C9CF38D0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FBE4C49866D2D78 // c1 (low double)
	.quad	0x3FD89F8D033946D5 // c2
	.quad	0x3FE02A20954EE23A // c3
	.quad	0x3FD35466F88C5ABC // c4
	.quad	0x3FD3044BAC5DC8C8 // c5
	.quad	0x3FCB110C3D6FEB5E // c6
	.quad	0x3FC7761A8DBF67E9 // c7
	.quad	0x3FF3A28C59D5433B // B' = pi/2 - B (high double)
	.quad	0xBC61B19140C0C0D5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD6E649F7D78649 // c0 (high double)
	.quad	0x3C678D2AC032531A // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC0631EFCBE0D16 // c1 (low double)
	.quad	0x3FD9D4CEDF38DFCD // c2
	.quad	0x3FE0A74F7488C3A6 // c3
	.quad	0x3FD487249EA3BD6C // c4
	.quad	0x3FD40D3A9BDEFA27 // c5
	.quad	0x3FCD2F1CBD1B58BD // c6
	.quad	0x3FC98CBDCEBDACE8 // c7
	.quad	0x3FF37048632CBAE1 // B' = pi/2 - B (high double)
	.quad	0xBC8CB2FCE8FC81A5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD7CA1A832D0F84 // c0 (high double)
	.quad	0xBC7EE5A71208FD7E // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC1AF828B5ED674 // c1 (low double)
	.quad	0x3FDB13909C0269A2 // c2
	.quad	0x3FE12C3A0394B7FA // c3
	.quad	0x3FD5CAC4A421216C // c4
	.quad	0x3FD52AE625505253 // c5
	.quad	0x3FCF7831018A6159 // c6
	.quad	0x3FCB98AB42F54A08 // c7
	.quad	0x3FF33E046C843287 // B' = pi/2 - B (high double)
	.quad	0xBC9A7CCAC0E4698B // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD8B00196B3D022 // c0 (high double)
	.quad	0xBC67341EE3041CF4 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC30BCA73878656 // c1 (low double)
	.quad	0x3FDC5C67FF403B73 // c2
	.quad	0x3FE1B94E72981059 // c3
	.quad	0x3FD7209990BAE1F6 // c4
	.quad	0x3FD65EF72AC3A4FA // c5
	.quad	0x3FD0F83D8377C340 // c6
	.quad	0x3FCD97555AF455A0 // c7
	.quad	0x3FF30BC075DBAA2C // B' = pi/2 - B (high double)
	.quad	0x3C995FE8F2B56DBD // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FD998188E816BF0 // c0 (high double)
	.quad	0x3C73DB2BD13E24E8 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC478794806DE66 // c1 (low double)
	.quad	0x3FDDAFF33BF96274 // c2
	.quad	0x3FE24F039543C659 // c3
	.quad	0x3FD88A113E071D2D // c4
	.quad	0x3FD7AB3E661E65FA // c5
	.quad	0x3FD24E4E1B8DC6C3 // c6
	.quad	0x3FD01ED5FCAC1031 // c7
	.quad	0x3FF2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0x3C8A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FDA827999FCEF32 // c0 (high double)
	.quad	0x3C708A11019E58E9 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC5F619980C4336 // c1 (low double)
	.quad	0x3FDF0ED99BED9B65 // c2
	.quad	0x3FE2EDD996C1BBEB // c3
	.quad	0x3FDA08B785C6EA59 // c4
	.quad	0x3FD911BC7B12902D // c5
	.quad	0x3FD3C0DA5B1130BA // c6
	.quad	0x3FD1B590151E4269 // c7
	.quad	0x3FF2A738888A9978 // B' = pi/2 - B (high double)
	.quad	0x3C5195059E91C4D2 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FDB6F3FC8C61E5B // c0 (high double)
	.quad	0xBC6A1CC48267E6BE // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC7853E65CE5524 // c1 (low double)
	.quad	0x3FE03CE61B6A9E63 // c2
	.quad	0x3FE3965ABF5C8FAB // c3
	.quad	0x3FDB9E3938F261E9 // c4
	.quad	0x3FDA94A771ED6FE6 // c5
	.quad	0x3FD552B7D4636639 // c6
	.quad	0x3FD2F58F20A08A92 // c7
	.quad	0x3FF274F491E2111E // B' = pi/2 - B (high double)
	.quad	0xBC8613F7E4FA18D6 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FDC5E87185E67B6 // c0 (high double)
	.quad	0x3C4FEBA1F149DF00 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC92683A68EB5D0 // c1 (low double)
	.quad	0x3FE0F8C35C7247C7 // c2
	.quad	0x3FE4491C4DCA262E // c3
	.quad	0x3FDD4C67683395D6 // c4
	.quad	0x3FDC3667C15EBF94 // c5
	.quad	0x3FD7070B7FE443C4 // c6
	.quad	0x3FD4805841782C0C // c7
	.quad	0x3FF242B09B3988C4 // B' = pi/2 - B (high double)
	.quad	0xBC972D483EE33523 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FDD506C82A2C800 // c0 (high double)
	.quad	0x3C6E169413E029AE // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FCADA8ECCC42C48 // c1 (low double)
	.quad	0x3FE1BB681D0BE8A7 // c2
	.quad	0x3FE506BF662527D3 // c3
	.quad	0x3FDF153B07CB2306 // c4
	.quad	0x3FDDF9A636394DC9 // c5
	.quad	0x3FD8E153A4A78673 // c6
	.quad	0x3FD6937B3797DBB1 // c7
	.quad	0x3FF2106CA4910069 // B' = pi/2 - B (high double)
	.quad	0x3C9CAF6B74B6A225 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FDE450E0D273E7A // c0 (high double)
	.quad	0x3C74138BBC424A0D // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FCCA20F5D4879EA // c1 (low double)
	.quad	0x3FE2853E13FBB7C7 // c2
	.quad	0x3FE5CFF21905E477 // c3
	.quad	0x3FE07D6C7D0DAC66 // c4
	.quad	0x3FDFE1550F550216 // c5
	.quad	0x3FDAE570CE7F6BD7 // c6
	.quad	0x3FD89328A3ABE601 // c7
	.quad	0x3FF1DE28ADE8780F // B' = pi/2 - B (high double)
	.quad	0x3C908C1F2850796D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FDF3C8AD985D9EE // c0 (high double)
	.quad	0x3C455C31F5355658 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FCE7DBF907D8618 // c1 (low double)
	.quad	0x3FE356B5A1D9CA5A // c2
	.quad	0x3FE6A57085697F99 // c3
	.quad	0x3FE17FCB46BE280B // c4
	.quad	0x3FE0F858275BAA48 // c5
	.quad	0x3FDD17B47CA8A83B // c6
	.quad	0x3FDA9B369A707A32 // c7
	.quad	0x3FF1ABE4B73FEFB5 // B' = pi/2 - B (high double)
	.quad	0x3C71A34B6FA942D3 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE01B819B5A7CF7 // c0 (high double)
	.quad	0x3C80BED996FBB65F // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD03732803B2A4C // c1 (low double)
	.quad	0x3FE430465B55C6C0 // c2
	.quad	0x3FE78806185ACF72 // c3
	.quad	0x3FE292FF3DF61A34 // c4
	.quad	0x3FE215A449967BC2 // c5
	.quad	0x3FDF7CEF2946D3A8 // c6
	.quad	0x3FDD0F4FF3CD442F // c7
	.quad	0x3FF179A0C097675B // B' = pi/2 - B (high double)
	.quad	0xBC7EE9E5C1EF600D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE09A4C59BD0D4D // c0 (high double)
	.quad	0xBC451D94A7B70320 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD13A68B2A5E855 // c1 (low double)
	.quad	0x3FE5126F9FD8D89F // c2
	.quad	0x3FE8788EEDC53F19 // c3
	.quad	0x3FE3B86B3E883B76 // c4
	.quad	0x3FE34A881E7F09C9 // c5
	.quad	0x3FE10D3EF07D106A // c6
	.quad	0x3FDFEBFE6AA0C4E7 // c7
	.quad	0x3FF1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0xBC93DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE11AB7190834EC // c0 (high double)
	.quad	0xBC8E577A07DB7162 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD248F1B07217E3 // c1 (low double)
	.quad	0x3FE5FDB93DD01D06 // c2
	.quad	0x3FE977F956462152 // c3
	.quad	0x3FE4F192BE91E40A // c4
	.quad	0x3FE49932EEB2A51B // c5
	.quad	0x3FE27B305435DE1B // c6
	.quad	0x3FE176C2BDD3B0A4 // c7
	.quad	0x3FF11518D34656A6 // B' = pi/2 - B (high double)
	.quad	0x3C9FFEEDF6B7D68D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE19CD3FE8E405D // c0 (high double)
	.quad	0xBC84E5C1D482FB99 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD36343ADCAF866 // c1 (low double)
	.quad	0x3FE6F2B4260FF6A8 // c2
	.quad	0x3FEA8747864942E1 // c3
	.quad	0x3FE6401D47362936 // c4
	.quad	0x3FE6040F50A52FBF // c5
	.quad	0x3FE40BA6192CC669 // c6
	.quad	0x3FE302E7DAB9BDE8 // c7
	.quad	0x3FF0E2D4DC9DCE4C // B' = pi/2 - B (high double)
	.quad	0x3C93DBA1AA51ADD5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE220B5EF047825 // c0 (high double)
	.quad	0x3C5AE1F6C713C510 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD489DC399ADDEC // c1 (low double)
	.quad	0x3FE7F1FB2FEBE8C6 // c2
	.quad	0x3FEBA791734D6CCE // c3
	.quad	0x3FE7A5DA56BAB9DA // c4
	.quad	0x3FE78DC9A17C17D7 // c5
	.quad	0x3FE5C262725357A1 // c6
	.quad	0x3FE4D1FE58052A33 // c7
	.quad	0x3FF0B090E5F545F2 // B' = pi/2 - B (high double)
	.quad	0x3C7EE15577AE1472 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE2A6709A74F289 // c0 (high double)
	.quad	0x3C76E3B98C26B440 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD5BD40BF249496 // c1 (low double)
	.quad	0x3FE8FC33EFCF6F3A // c2
	.quad	0x3FECDA06E4CFC822 // c3
	.quad	0x3FE924C5BA1C7515 // c4
	.quad	0x3FE9395A86CEDBCC // c5
	.quad	0x3FE7A39B6931C7C5 // c6
	.quad	0x3FE6D875BED7333E // c7
	.quad	0x3FF07E4CEF4CBD98 // B' = pi/2 - B (high double)
	.quad	0xBC71ABDBB9EA8E6E // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE32E1889047FFD // c0 (high double)
	.quad	0xBC810D08F0134DCF // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD6FDFF12A4297B // c1 (low double)
	.quad	0x3FEA120FA25B9D83 // c2
	.quad	0x3FEE1FF1BF27E6AB // c3
	.quad	0x3FEABF0C6E045EA0 // c4
	.quad	0x3FEB0A0E2CDCAB88 // c5
	.quad	0x3FE9B40D41660E8E // c6
	.quad	0x3FE92FCC1848D932 // c7
	.quad	0x3FF04C08F8A4353E // B' = pi/2 - B (high double)
	.quad	0xBC908E433AE0CC54 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE3B7C3289ED6F3 // c0 (high double)
	.quad	0xBC8C4CF3D327FEEC // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD84CAE0A06DE7C // c1 (low double)
	.quad	0x3FEB344C2E4C0F47 // c2
	.quad	0x3FEF7AB88F5F820C // c3
	.quad	0x3FEC771219A99E98 // c4
	.quad	0x3FED0390B144EACB // c5
	.quad	0x3FEBF90A32A17503 // c6
	.quad	0x3FEBAC50A2912BE1 // c7
	.quad	0x3FF019C501FBACE4 // B' = pi/2 - B (high double)
	.quad	0xBC9CB18F8746F50C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE44386DB9CE5DB // c0 (high double)
	.quad	0xBC8A7F79AD342CDE // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD9A9EE22E82AF2 // c1 (low double)
	.quad	0x3FEC63B53FACFD1B // c2
	.quad	0x3FF075F0B010F1C9 // c3
	.quad	0x3FEE4F7738A37AC1 // c4
	.quad	0x3FEF29F7068A6E02 // c5
	.quad	0x3FEE78912F3E4890 // c6
	.quad	0x3FEE7DE5AD19FDA8 // c7
	.quad	0x3FEFCF0216A64913 // B' = pi/2 - B (high double)
	.quad	0xBC81A9B7A75A3B87 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE4D17B087B265D // c0 (high double)
	.quad	0xBC788AAC4AE1F0D0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDB166A3723839C // c1 (low double)
	.quad	0x3FEDA1257F3D5E87 // c2
	.quad	0x3FF13A8A70704F6B // c3
	.quad	0x3FF025900612C493 // c4
	.quad	0x3FF0C0E778680DE8 // c5
	.quad	0x3FF09CB2B02DFCCB // c6
	.quad	0x3FF0D681147681D3 // c7
	.quad	0x3FEF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0x3C860FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE561B82AB7F990 // c0 (high double)
	.quad	0x3C87A5FA3E37FD12 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDC92D84170BCC6 // c1 (low double)
	.quad	0x3FEEED87E934644C // c2
	.quad	0x3FF20C10F4D7D072 // c3
	.quad	0x3FF1369E37DA5741 // c4
	.quad	0x3FF20816F61AB3EE // c5
	.quad	0x3FF221953D8712EF // c6
	.quad	0x3FF29CA76CD561F8 // c7
	.quad	0x3FEF05F23C0427AA // B' = pi/2 - B (high double)
	.quad	0xBC51B746C796F33C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE5F457E4F4812E // c0 (high double)
	.quad	0x3C8F6EE06A43F015 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDE1FFA33ABCB2D // c1 (low double)
	.quad	0x3FF024ECA3857FDF // c2
	.quad	0x3FF2EB80B3E2C969 // c3
	.quad	0x3FF25CA858ECE33E // c4
	.quad	0x3FF36D6159DB2BC3 // c5
	.quad	0x3FF3CF427530AFA1 // c6
	.quad	0x3FF4914D16BF30D2 // c7
	.quad	0x3FEEA16A4EB316F6 // B' = pi/2 - B (high double)
	.quad	0xBC8A7D8171BF2FD8 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE6897514751DB6 // c0 (high double)
	.quad	0x3C7432E08569C822 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDFBE9EE099229D // c1 (low double)
	.quad	0x3FF0DB94E83152C9 // c2
	.quad	0x3FF3D9ED602549D9 // c3
	.quad	0x3FF3999FCB35B63C // c4
	.quad	0x3FF4F3F4BB18D34D // c5
	.quad	0x3FF5AA9FE2712A89 // c6
	.quad	0x3FF6B61F156D157E // c7
	.quad	0x3FEE3CE261620641 // B' = pi/2 - B (high double)
	.quad	0x3C7A77CBEAE8FD71 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE7212BE621BE6D // c0 (high double)
	.quad	0x3C8AD14CAC5AE1D4 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFDE905D04D13F70 // c1 (low double)
	.quad	0x3FF19B4F7BD63206 // c2
	.quad	0x3FF4D8845C8EEC38 // c3
	.quad	0x3FF4EFAAD70924AA // c4
	.quad	0x3FF69F610EDFA788 // c5
	.quad	0x3FF7B93C8525966A // c6
	.quad	0x3FF928F49138F1A9 // c7
	.quad	0x3FEDD85A7410F58D // B' = pi/2 - B (high double)
	.quad	0xBC76156546AFA570 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE7BB99ED2990CF // c0 (high double)
	.quad	0xBC8AABB1289366EE // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFDCCC0DD257FE9F // c1 (low double)
	.quad	0x3FF264BAB3E8C3FC // c2
	.quad	0x3FF5E88F7CC1F7BC // c3
	.quad	0x3FF6612B1AE228E1 // c4
	.quad	0x3FF873A64164BF00 // c5
	.quad	0x3FFA016987C44E2A // c6
	.quad	0x3FFBFCA608E41B3A // c7
	.quad	0x3FED73D286BFE4D8 // B' = pi/2 - B (high double)
	.quad	0x3C8CAEB4C3DBDBD8 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE858DE3B716571 // c0 (high double)
	.quad	0x3C8C6340D74EFAEE // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFDAF377B50F3DE8 // c1 (low double)
	.quad	0x3FF338819C6A585B // c2
	.quad	0x3FF70B781BB3F9F8 // c3
	.quad	0x3FF7F0C4DC952BEE // c4
	.quad	0x3FFA75444794B0BE // c5
	.quad	0x3FFC8A58AEEA152D // c6
	.quad	0x3FFF1509C3EC473F // c7
	.quad	0x3FED0F4A996ED424 // B' = pi/2 - B (high double)
	.quad	0x3C61A070AC3E29A0 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE8F9197BF85EEB // c0 (high double)
	.quad	0xBC77971D706288A0 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD9058D62BBB98D // c1 (low double)
	.quad	0x3FF4175D31199B0C // c2
	.quad	0x3FF842CA948070CE // c3
	.quad	0x3FF9A1675E6F2233 // c4
	.quad	0x3FFCA94A75D1DF36 // c5
	.quad	0x3FFF5C3EE98CB77C // c6
	.quad	0x4001549BBD7761F7 // c7
	.quad	0x3FECAAC2AC1DC370 // B' = pi/2 - B (high double)
	.quad	0xBC83DE7C6DBCC708 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FE99C6E0F634394 // c0 (high double)
	.quad	0x3C81A125C4B0D75F // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD7012EAAC8A987 // c1 (low double)
	.quad	0x3FF50215B95FD940 // c2
	.quad	0x3FF9903A2B193F49 // c3
	.quad	0x3FFB7656607D37C1 // c4
	.quad	0x3FFF156F461C0EE0 // c5
	.quad	0x4001403E48F2741B // c6
	.quad	0x40034ACD8D38409A // c7
	.quad	0x3FEC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0x3C83DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEA43002AE42850 // c0 (high double)
	.quad	0xBC7A7C0406D92972 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD4E526E33345B8 // c1 (low double)
	.quad	0x3FF5F9844B626284 // c2
	.quad	0x3FFAF5A574C2F3BF // c3
	.quad	0x3FFD7334FD198D13 // c4
	.quad	0x4000E01251879DC7 // c5
	.quad	0x400300E6F5213CC2 // c6
	.quad	0x40058BA67D3A054A // c7
	.quad	0x3FEBE1B2D17BA207 // B' = pi/2 - B (high double)
	.quad	0xBC61AEB67D55A7A1 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEAECF5F9BA35A6 // c0 (high double)
	.quad	0x3C4535083409FF20 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD2B02B2D565D55 // c1 (low double)
	.quad	0x3FF6FE947B408BAC // c2
	.quad	0x3FFC751B52A83938 // c3
	.quad	0x3FFF9C12181CFBBA // c4
	.quad	0x4002585B4421C075 // c5
	.quad	0x4004F6412119D9F1 // c6
	.quad	0x400814BACED69AF1 // c7
	.quad	0x3FEB7D2AE42A9153 // B' = pi/2 - B (high double)
	.quad	0xBC8CB2463821BB58 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEB9A77C18C1AF2 // c0 (high double)
	.quad	0x3C76EA194A3B7D14 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD060D88DF903DF // c1 (low double)
	.quad	0x3FF812463C417D93 // c2
	.quad	0x3FFE10E093E6DE8D // c3
	.quad	0x4000FABB50DD022E // c4
	.quad	0x4003F7B45B053F76 // c5
	.quad	0x4007275FB67DBBCC // c6
	.quad	0x400B02CB76CD8FDB // c7
	.quad	0x3FEB18A2F6D9809E // B' = pi/2 - B (high double)
	.quad	0x3C760E425E23E66F // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEC4BB009E77983 // c0 (high double)
	.quad	0xBC5346B6895525B8 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFCBEB63A6D04570 // c1 (low double)
	.quad	0x3FF935AFFA8DCC1D // c2
	.quad	0x3FFFCB7657D15F0B // c3
	.quad	0x4002423AFCE75F54 // c4
	.quad	0x4005C2CF61AD4B89 // c5
	.quad	0x40099C675A298F48 // c6
	.quad	0x400E49F38B657F73 // c7
	.quad	0x3FEAB41B09886FEA // B' = pi/2 - B (high double)
	.quad	0xBC7A7EEED374BC71 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FED00CBC7384D2E // c0 (high double)
	.quad	0x3C78C4FC635D2596 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFC6DA3A87611EA6 // c1 (low double)
	.quad	0x3FFA6A00F50D3A6F // c2
	.quad	0x4000D3D0A6ABBD46 // c3
	.quad	0x4003A760672280D0 // c4
	.quad	0x4007BF026C103550 // c5
	.quad	0x400C5EBD23F4E2D6 // c6
	.quad	0x4011024DB7867574 // c7
	.quad	0x3FEA4F931C375F35 // B' = pi/2 - B (high double)
	.quad	0x3C8A79EFFD795057 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEDB9FA89953FCF // c0 (high double)
	.quad	0x3C286C68F5592C40 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFC18AC418849A6E // c1 (low double)
	.quad	0x3FFBB083E02786D7 // c2
	.quad	0x4001D438F8A20402 // c3
	.quad	0x40052D5D6FF7A4BD // c4
	.quad	0x4009F262067BAA5A // c5
	.quad	0x400F793DA8608BD3 // c6
	.quad	0x401328BFC5F60A12 // c7
	.quad	0x3FE9EB0B2EE64E81 // B' = pi/2 - B (high double)
	.quad	0x3C519ABB2567F739 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEE776EAFC91706 // c0 (high double)
	.quad	0x3C890664EDC679F9 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFB7F295AD658764 // c1 (low double)
	.quad	0x3FFD0AA1DB7F5E58 // c2
	.quad	0x4002E8A6F9693556 // c3
	.quad	0x4006D7CA16B298C6 // c4
	.quad	0x400C63DFCAA3FB6C // c5
	.quad	0x40117C3EE50D7733 // c6
	.quad	0x401599932C96B999 // c7
	.quad	0x3FE9868341953DCD // B' = pi/2 - B (high double)
	.quad	0xBC861341341F5289 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0x3FEF395D9F0E3C92 // c0 (high double)
	.quad	0xBC550BEC65673F08 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFA8873C53BE0540 // c1 (low double)
	.quad	0x3FFE79E5C65202DE // c2
	.quad	0x400412FD76797C7D // c3
	.quad	0x4008AAB30DC82D37 // c4
	.quad	0x400F1B6C68B4AEA4 // c5
	.quad	0x4013758CE9BD9EE9 // c6
	.quad	0x40186872A06858F5 // c7
	.quad	0x3FE921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C81A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFD17CC1B727220B // c0 (high double)
	.quad	0x3C7AC07996DF1E94 // c0 (low double)
	.quad	0x3FE0000000000000 // c1 (high single bit)
	.quad	0xBFBF02F6222C71FB // c1 (low double)
	.quad	0xBFB068BD68BAEFC3 // c2
	.quad	0x3FA3C0213E30B2A4 // c3
	.quad	0xBF8A5494A0DD32B3 // c4
	.quad	0x3F7940CD1D8E923E // c5
	.quad	0xBF63CC1BE5088783 // c6
	.quad	0x3F3FA4A6A27659EE // c7
	.quad	0x3FE8BD7366F31C64 // B' = pi/2 - B (high double)
	.quad	0xBC6A81C996DFD5A5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFD130BD396BB4A9 // c0 (high double)
	.quad	0x3C657D8592EA0982 // c0 (low double)
	.quad	0x3FE0000000000000 // c1 (high single bit)
	.quad	0xBFBF68ED595308D6 // c1 (low double)
	.quad	0xBFB00C6BF6A673E6 // c2
	.quad	0x3FA36E9DA9556AA6 // c3
	.quad	0xBF8991120C7D4BB9 // c4
	.quad	0x3F7889ABD600821A // c5
	.quad	0xBF630EE40468887B // c6
	.quad	0x3F52CAD75A194915 // c7
	.quad	0x3FE858EB79A20BB0 // B' = pi/2 - B (high double)
	.quad	0xBC8EE70AFE8446D9 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFD0E507EE8B93D7 // c0 (high double)
	.quad	0xBC679F09CDD5D366 // c0 (low double)
	.quad	0x3FE0000000000000 // c1 (high single bit)
	.quad	0xBFBFCCA52E2AE201 // c1 (low double)
	.quad	0xBFAF632A1681FD0B // c2
	.quad	0x3FA31F7794BDF922 // c3
	.quad	0xBF88D31259E30525 // c4
	.quad	0x3F77D97B728544D6 // c5
	.quad	0xBF62591A3A2B69D8 // c6
	.quad	0x3F3605B3887E1AF3 // c7
	.quad	0x3FE7F4638C50FAFB // B' = pi/2 - B (high double)
	.quad	0x3C71A4B8D15ECF6D // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFD099A0163BCD3F // c0 (high double)
	.quad	0xBC7A7F11D963D761 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBFD1D937A1F094 // c1 (low double)
	.quad	0xBFAEB05B52EFB4C0 // c2
	.quad	0x3FA2D29E032A57DB // c3
	.quad	0xBF881A60F7D38723 // c4
	.quad	0x3F772FBBF1BFA292 // c5
	.quad	0xBF61AA745B27BAF7 // c6
	.quad	0x3F51CC1913635CA5 // c7
	.quad	0x3FE78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0xBC7EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFD04E83F7474877 // c0 (high double)
	.quad	0xBC73B117AC20AD03 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBF7284F47C0E61 // c1 (low double)
	.quad	0xBFAE0056458FCED5 // c2
	.quad	0x3FA28800992AEEE6 // c3
	.quad	0xBF8766CB6439B11D // c4
	.quad	0x3F768C54DBB3684B // c5
	.quad	0xBF610297B13942A7 // c6
	.quad	0x3F5536BD1C2C92E8 // c7
	.quad	0x3FE72B53B1AED992 // B' = pi/2 - B (high double)
	.quad	0x3C88452B3716C4D6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFD003B1DF5A0F86 // c0 (high double)
	.quad	0xBC716CEB71FB7130 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBF155566FA6884 // c1 (low double)
	.quad	0xBFAD530628A37C7A // c2
	.quad	0x3FA23F8F96E80A99 // c3
	.quad	0xBF86B82113BDC04E // c4
	.quad	0x3F75EF0B9C9DF1C1 // c5
	.quad	0xBF60613421CB6A8B // c6
	.quad	0x3F48E48E9081596F // c7
	.quad	0x3FE6C6CBC45DC8DE // B' = pi/2 - B (high double)
	.quad	0xBBC6D61B58C99C43 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCF7250459C1976 // c0 (high double)
	.quad	0xBC40B5B081F067D4 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBEBA422E0FE0E5 // c1 (low double)
	.quad	0xBFACA856C815D042 // c2
	.quad	0x3FA1F93BD218DCE9 // c3
	.quad	0xBF860E335AB42933 // c4
	.quad	0x3F75578EEA5D18CC // c5
	.quad	0xBF5F8BFBAD564EB5 // c6
	.quad	0x3F434A80D203FDB7 // c7
	.quad	0x3FE66243D70CB82A // B' = pi/2 - B (high double)
	.quad	0xBC884805FA81DE0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCEDDCA38F25FEC // c0 (high double)
	.quad	0x3C65AACB9E8B4E8C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBE6143274A35BF // c1 (low double)
	.quad	0xBFAC00347C15ABB2 // c2
	.quad	0x3FA1B4F6B03A62C8 // c3
	.quad	0xBF8568D557225239 // c4
	.quad	0x3F74C5A8346B014E // c5
	.quad	0xBF5E61680C7E3887 // c6
	.quad	0x3F45A81E03A2004A // c7
	.quad	0x3FE5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0x3C7EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCE49CE5AF9DC29 // c0 (high double)
	.quad	0x3C382336F1C6B420 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBE0A506D218F3C // c1 (low double)
	.quad	0xBFAB5A8C23E49FEA // c2
	.quad	0x3FA172B22117D625 // c3
	.quad	0xBF84C7DBDBF304B9 // c4
	.quad	0x3F74391DF553E941 // c5
	.quad	0xBF5D4211231EF20F // c6
	.quad	0x3F52AB1CCD6F9451 // c7
	.quad	0x3FE59933FC6A96C1 // B' = pi/2 - B (high double)
	.quad	0xBC71AA6E583501D4 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCDB6597A2E1B61 // c0 (high double)
	.quad	0x3C6CB71A947AAB6B // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBDB56255580CA4 // c1 (low double)
	.quad	0xBFAAB74B20D856E7 // c2
	.quad	0x3FA1326099896038 // c3
	.quad	0xBF842B1D5CF84F74 // c4
	.quad	0x3F73B1CC2C4E32E8 // c5
	.quad	0xBF5C2D7D75B7FBCE // c6
	.quad	0x3F53DB94CE05D10A // c7
	.quad	0x3FE534AC0F19860C // B' = pi/2 - B (high double)
	.quad	0x3C8EE4303B192DA6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCD236870E769B5 // c0 (high double)
	.quad	0xBC523CE78218C3F0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBD62716F68B5FE // c1 (low double)
	.quad	0xBFAA165F518C38AB // c2
	.quad	0x3FA0F3F50E789484 // c3
	.quad	0xBF839271DC18DB75 // c4
	.quad	0x3F732F8BAC4C5496 // c5
	.quad	0xBF5B23248F865BB8 // c6
	.quad	0x3F36E307E32E173B // c7
	.quad	0x3FE4D02421C87558 // B' = pi/2 - B (high double)
	.quad	0x3C6A765E893370D7 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCC90F8250157F7 // c0 (high double)
	.quad	0xBC657D8312BFEA16 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBD117683053E13 // c1 (low double)
	.quad	0xBFA977B70D411C29 // c2
	.quad	0x3FA0B762F022F359 // c3
	.quad	0xBF82FDB2D7324A8F // c4
	.quad	0x3F72B2030A36AAE0 // c5
	.quad	0xBF5A22A22953FC15 // c6
	.quad	0x3F473A2F09CB1310 // c7
	.quad	0x3FE46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0xBC81A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCBFF05878394D3 // c0 (high double)
	.quad	0xBC486912F813DEBC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBCC26A8EA2077F // c1 (low double)
	.quad	0xBFA8DB411F68EE69 // c2
	.quad	0x3FA07C9E25737218 // c3
	.quad	0xBF826CBB3713A292 // c4
	.quad	0x3F72392A32F26712 // c5
	.quad	0xBF592B76EEB6E333 // c6
	.quad	0x3F39BADB57E2A4EB // c7
	.quad	0x3FE40714472653EF // B' = pi/2 - B (high double)
	.quad	0x3C86106670B43955 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCB6D8D944D0371 // c0 (high double)
	.quad	0x3C6CDD9D6EA4B883 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBC7546C60FE5C7 // c1 (low double)
	.quad	0xBFA840ECC35C53FD // c2
	.quad	0x3FA0439B07C380F3 // c3
	.quad	0xBF81DF673F038AB5 // c4
	.quad	0x3F71C4B3C36B8A2B // c5
	.quad	0xBF583D44FA2FB1AC // c6
	.quad	0x3F52EE3E6107070C // c7
	.quad	0x3FE3A28C59D5433B // B' = pi/2 - B (high double)
	.quad	0xBC51B19140C0C0D5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCADC8D51C0FA5B // c0 (high double)
	.quad	0xBC54B350B5D39454 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBC2A04912319A8 // c1 (low double)
	.quad	0xBFA7A8A9A038608E // c2
	.quad	0x3FA00C4E5E972915 // c3
	.quad	0xBF8155947D4C5C83 // c4
	.quad	0x3F7154A7B149DA51 // c5
	.quad	0xBF5757960EDFC478 // c6
	.quad	0x3F3F895707140B54 // c7
	.quad	0x3FE33E046C843287 // B' = pi/2 - B (high double)
	.quad	0xBC8A7CCAC0E4698B // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFCA4C01D0769648 // c0 (high double)
	.quad	0xBC4DDC96CAE4A560 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBBE09D8A670D72 // c1 (low double)
	.quad	0xBFA71267C4E29753 // c2
	.quad	0x3F9FAD5AB78BB764 // c3
	.quad	0xBF80CF21BC414F68 // c4
	.quad	0x3F70E8AC1DCF4364 // c5
	.quad	0xBF567A14C0530821 // c6
	.quad	0x3F4F621D6DA2AD4C // c7
	.quad	0x3FE2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0x3C7A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC9BBE82AEA0D12 // c0 (high double)
	.quad	0x3C62D116CA770BB7 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBB990B7DDE5BFC // c1 (low double)
	.quad	0xBFA67E17A4317C6C // c2
	.quad	0x3F9F455B2F170454 // c3
	.quad	0xBF804BEEF437A2B3 // c4
	.quad	0x3F7080CEADF2974D // c5
	.quad	0xBF55A44E3A71E98C // c6
	.quad	0xBF039F9468E5AC7C // c7
	.quad	0x3FE274F491E2111E // B' = pi/2 - B (high double)
	.quad	0xBC7613F7E4FA18D6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC92C3D852FEDF2 // c0 (high double)
	.quad	0xBC20F5E7403E63E0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBB534867CEB1CB // c1 (low double)
	.quad	0xBFA5EBAA113813D4 // c2
	.quad	0x3F9EE08A1A202CC5 // c3
	.quad	0xBF7F97BA7BA1301A // c4
	.quad	0x3F701CAD7F191E00 // c5
	.quad	0xBF54D60D132212FE // c6
	.quad	0x3F45F5FE99746255 // c7
	.quad	0x3FE2106CA4910069 // B' = pi/2 - B (high double)
	.quad	0x3C8CAF6B74B6A225 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC89CFF0CAA3CB4 // c0 (high double)
	.quad	0x3C6A8D50D06FC30E // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBB0F4E73981D4B // c1 (low double)
	.quad	0xBFA55B103BB2BD6D // c2
	.quad	0x3F9E7ED42D91EC8D // c3
	.quad	0xBF7E9D9D8AAE7B14 // c4
	.quad	0x3F6F78B5039EFACA // c5
	.quad	0xBF540EDBA3A7865E // c6
	.quad	0x3F3AA2CB5E8CB7D1 // c7
	.quad	0x3FE1ABE4B73FEFB5 // B' = pi/2 - B (high double)
	.quad	0x3C61A34B6FA942D3 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC80E29F7BF5633 // c0 (high double)
	.quad	0xBC67F8F06ACC6DE1 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBACD17FA976786 // c1 (low double)
	.quad	0xBFA4CC3BAC93F075 // c2
	.quad	0x3F9E2026D6967836 // c3
	.quad	0xBF7DA94D7C694B78 // c4
	.quad	0x3F6EBF4F14B98A4D // c5
	.quad	0xBF534E7211F8744B // c6
	.quad	0xBF2290D134413454 // c7
	.quad	0x3FE1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0xBC83DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC77FBB85928D2D // c0 (high double)
	.quad	0x3C6121F4E1AC8205 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBA8C9F83131391 // c1 (low double)
	.quad	0xBFA43F1E42AF6970 // c2
	.quad	0x3F9DC4703433CA75 // c3
	.quad	0xBF7CBA92AE7FE396 // c4
	.quad	0x3F6E0CB6D0475CBF // c5
	.quad	0xBF529491FB8790F1 // c6
	.quad	0x3F37152AFD512A19 // c7
	.quad	0x3FE0E2D4DC9DCE4C // B' = pi/2 - B (high double)
	.quad	0x3C83DBA1AA51ADD5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC6F1B0FDBE6EDE // c0 (high double)
	.quad	0xBC614F63025E350F // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBA4DDFBF32A727 // c1 (low double)
	.quad	0xBFA3B3AA2F826E47 // c2
	.quad	0x3F9D6B9F112A3CA9 // c3
	.quad	0xBF7BD1375F2B326E // c4
	.quad	0x3F6D60EF002A5944 // c5
	.quad	0xBF51E0E0E3E64206 // c6
	.quad	0x3F32BFA3B8DB7E72 // c7
	.quad	0x3FE07E4CEF4CBD98 // B' = pi/2 - B (high double)
	.quad	0xBC61ABDBB9EA8E6E // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC66407B0109FA6 // c0 (high double)
	.quad	0xBC55C3EC26D04386 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBA10D38BFFE319 // c1 (low double)
	.quad	0xBFA329D1F417DBD6 // c2
	.quad	0x3F9D15A2DE544C7C // c3
	.quad	0xBF7AED07977D3236 // c4
	.quad	0x3F6CBBAD3C53FDBF // c5
	.quad	0xBF513318B342B4E5 // c6
	.quad	0x3F1AF93D1229A6F6 // c7
	.quad	0x3FE019C501FBACE4 // B' = pi/2 - B (high double)
	.quad	0xBC8CB18F8746F50C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC5D6BCF447403E // c0 (high double)
	.quad	0x3C6C6C662876B222 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB9D575F071964A // c1 (low double)
	.quad	0xBFA2A1885E06BEAC // c2
	.quad	0x3F9CC26BAD1B38B8 // c3
	.quad	0xBF7A0DD116DA5229 // c4
	.quad	0x3F6C1C921E809C9E // c5
	.quad	0xBF508B03CEF8B07A // c6
	.quad	0x3F4BCE57D03F879A // c7
	.quad	0x3FDF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0x3C760FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC549CE29CFCCBE // c0 (high double)
	.quad	0x3C69881846E0C1CC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB99BC21C7FB9B2 // c1 (low double)
	.quad	0xBFA21AC0848A46C5 // c2
	.quad	0x3F9C71EA2A16819D // c3
	.quad	0xBF7933633F8F0072 // c4
	.quad	0x3F6B83D1C55099C3 // c5
	.quad	0xBF4FD088727B159E // c6
	.quad	0xBF2306C94816B8C4 // c7
	.quad	0x3FDEA16A4EB316F6 // B' = pi/2 - B (high double)
	.quad	0xBC7A7D8171BF2FD8 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC4BD38B7876716 // c0 (high double)
	.quad	0xBC5504069A91A9E0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB963B368408780 // c1 (low double)
	.quad	0xBFA1956DC5B1F35A // c2
	.quad	0x3F9C240F984C07DC // c3
	.quad	0xBF785D8F0388F11C // c4
	.quad	0x3F6AF0B073DF1CCA // c5
	.quad	0xBF4E9580D70D34E0 // c6
	.quad	0x3F4571315C44A425 // c7
	.quad	0x3FDDD85A7410F58D // B' = pi/2 - B (high double)
	.quad	0xBC66156546AFA570 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC430FA0B7C7F15 // c0 (high double)
	.quad	0xBC63115D3C69C624 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB92D45530E4319 // c1 (low double)
	.quad	0xBFA11183C3A8DFF6 // c2
	.quad	0x3F9BD8CDCC073185 // c3
	.quad	0xBF778C26D2F52E9A // c4
	.quad	0x3F6A638650FB676D // c5
	.quad	0xBF4D6420527BDB5B // c6
	.quad	0xBF2DA5A645CA3434 // c7
	.quad	0x3FDD0F4A996ED424 // B' = pi/2 - B (high double)
	.quad	0x3C51A070AC3E29A0 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC3A50F9AB1CA91 // c0 (high double)
	.quad	0xBC50CA3B48D907FA // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB8F87382B56ACB // c1 (low double)
	.quad	0xBFA08EF662132882 // c2
	.quad	0x3F9B901726A3CD1C // c3
	.quad	0xBF76BEFE8A4461A2 // c4
	.quad	0x3F69DBAE42D8A416 // c5
	.quad	0xBF4C3C52357BF011 // c6
	.quad	0x3F32456D9E7FA4E8 // c7
	.quad	0x3FDC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0x3C73DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC31976E0E280B3 // c0 (high double)
	.quad	0x3C667516EE4F26F2 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB8C539C2AB0F89 // c1 (low double)
	.quad	0xBFA00DB9C3806462 // c2
	.quad	0x3F9B49DE91DB8E44 // c3
	.quad	0xBF75F5EB6258C838 // c4
	.quad	0x3F695952C6BEE603 // c5
	.quad	0xBF4B1D691B1E9F12 // c6
	.quad	0xBEFC715673917907 // c7
	.quad	0x3FDB7D2AE42A9153 // B' = pi/2 - B (high double)
	.quad	0xBC7CB2463821BB58 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC28E2D6047CBC8 // c0 (high double)
	.quad	0x3C0CEAB2537EA300 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB89394034B11CF // c1 (low double)
	.quad	0xBF9F1B848DE48876 // c2
	.quad	0x3F9B06177BDD3821 // c3
	.quad	0xBF7530C3E0150AA0 // c4
	.quad	0x3F68DBF62FBA70EC // c5
	.quad	0xBF4A0736C860B400 // c6
	.quad	0x3F50A3F1D92A1F47 // c7
	.quad	0x3FDAB41B09886FEA // B' = pi/2 - B (high double)
	.quad	0xBC6A7EEED374BC71 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC20330A15F6568 // c0 (high double)
	.quad	0xBC61BB4F210EA632 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB8637E591E05FE // c1 (low double)
	.quad	0xBF9E1E090AECCE81 // c2
	.quad	0x3F9AC4B5D3090946 // c3
	.quad	0xBF746F5FC5C316BA // c4
	.quad	0x3F686400E51F5377 // c5
	.quad	0xBF48F8EADC3DBAD1 // c6
	.quad	0xBF320AB3DE6FDA08 // c7
	.quad	0x3FD9EB0B2EE64E81 // B' = pi/2 - B (high double)
	.quad	0x3C419ABB2567F739 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC1787E32B35112 // c0 (high double)
	.quad	0x3C5D2D85EBE7CEA8 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB834F4FC268515 // c1 (low double)
	.quad	0xBF9D22EA9FA50E7D // c2
	.quad	0x3F9A85AE02819BA3 // c3
	.quad	0xBF73B19803AB2B21 // c4
	.quad	0x3F67F0B8C64045E8 // c5
	.quad	0xBF47F293167B415A // c6
	.quad	0xBF198BF45BBDE600 // c7
	.quad	0x3FD921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C71A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC0EE13A8A2A9C6 // c0 (high double)
	.quad	0x3C5830A63DDC3262 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB807F44735B1A4 // c1 (low double)
	.quad	0xBF9C2A135884458A // c2
	.quad	0x3F9A48F4EE376E9D // c3
	.quad	0xBF72F746AAC73A47 // c4
	.quad	0x3F678215824701EA // c5
	.quad	0xBF46F3C719FB6619 // c6
	.quad	0x3F4BA88D5E69B64E // c7
	.quad	0x3FD858EB79A20BB0 // B' = pi/2 - B (high double)
	.quad	0xBC7EE70AFE8446D9 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFC063EE9D2B777C // c0 (high double)
	.quad	0xBC4E1787D8B78D9C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB7DC78B746BB2F // c1 (low double)
	.quad	0xBF9B336DA889AC7D // c2
	.quad	0x3F9A0E7FEF8737D6 // c3
	.quad	0xBF724046DF328844 // c4
	.quad	0x3F67185639AB9961 // c5
	.quad	0xBF45FBD25ACB3AED // c6
	.quad	0x3F0E765A16190A6D // c7
	.quad	0x3FD78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0xBC6EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFBFB4195F6B0333 // c0 (high double)
	.quad	0xBC47DB89E27050CE // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB7B27EEAE12CDB // c1 (low double)
	.quad	0xBF9A3EE464F938D9 // c2
	.quad	0x3F99D644D21E7AE8 // c3
	.quad	0xBF718C74CAAA7BA1 // c4
	.quad	0x3F66B2F778B94CFD // c5
	.quad	0xBF450AB96EF7378B // c6
	.quad	0x3F1412EF7D22C6CA // c7
	.quad	0x3FD6C6CBC45DC8DE // B' = pi/2 - B (high double)
	.quad	0xBBB6D61B58C99C43 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFBEA0D709BC27CA // c0 (high double)
	.quad	0xBC48928D103E7F70 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB78A03A181D6AC // c1 (low double)
	.quad	0xBF994C62C1366C88 // c2
	.quad	0x3F99A039D09A41AD // c3
	.quad	0xBF70DBAD90C6529A // c4
	.quad	0x3F66520A2F7F4662 // c5
	.quad	0xBF441FF58F223C4F // c6
	.quad	0xBF357CCF18E00FA5 // c7
	.quad	0x3FD5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0x3C6EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFBD8E118C89577F // c0 (high double)
	.quad	0xBC3344F06C36BFAC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB76303BB0A22C5 // c1 (low double)
	.quad	0xBF985BD44ABC0416 // c2
	.quad	0x3F996C5591BA6DFA // c3
	.quad	0xBF702DCF4265B5B3 // c4
	.quad	0x3F65F52766DBD146 // c5
	.quad	0xBF433B879649EE5C // c6
	.quad	0x3F47C67A932EC95B // c7
	.quad	0x3FD534AC0F19860C // B' = pi/2 - B (high double)
	.quad	0x3C7EE4303B192DA6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFBC7BC444B10758 // c0 (high double)
	.quad	0x3C511540B2268E0E // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB73D7C3735AA80 // c1 (low double)
	.quad	0xBF976D24E52F1F79 // c2
	.quad	0x3F993A8F25560FF4 // c3
	.quad	0xBF6F0571A586F6F3 // c4
	.quad	0x3F659C8E8181F3F0 // c5
	.quad	0xBF425CCABE1033EA // c6
	.quad	0x3F4D59CA0EE986DF // c7
	.quad	0x3FD46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0xBC71A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFBB69EA984DE6EF // c0 (high double)
	.quad	0x3C573CB7437F1792 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB7196A3515E12E // c1 (low double)
	.quad	0xBF968040C68C8FE8 // c2
	.quad	0x3F990ADE01D185E6 // c3
	.quad	0xBF6DB49417B3E65C // c4
	.quad	0x3F6548393E856BDB // c5
	.quad	0xBF418353F782833A // c6
	.quad	0xBF36489E03176323 // c7
	.quad	0x3FD3A28C59D5433B // B' = pi/2 - B (high double)
	.quad	0xBC41B19140C0C0D5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFBA587FF6553726 // c0 (high double)
	.quad	0x3C5559CFE8366BA8 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB6F6CAF293AC7F // c1 (low double)
	.quad	0xBF959514736F007F // c2
	.quad	0x3F98DD3A019E14EB // c3
	.quad	0xBF6C68C706B3D0FB // c4
	.quad	0x3F64F779DB6C50F9 // c5
	.quad	0xBF40AF7CD7497BC1 // c6
	.quad	0x3F4AD3B98213250E // c7
	.quad	0x3FD2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0x3C6A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB9477FD636A4D8 // c0 (high double)
	.quad	0x3C5063C8B22C4842 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB6D59BCBF6C48B // c1 (low double)
	.quad	0xBF94AB8CBB6CBB16 // c2
	.quad	0x3F98B19B6070D56D // c3
	.quad	0xBF6B21CD22DB24B2 // c4
	.quad	0x3F64AAE4575F07CB // c5
	.quad	0xBF3FC08B3806F47F // c6
	.quad	0xBF00E427650BA68A // c7
	.quad	0x3FD2106CA4910069 // B' = pi/2 - B (high double)
	.quad	0x3C7CAF6B74B6A225 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB836E5B77D959A // c0 (high double)
	.quad	0xBC51BDCEFF6925EC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB6B5DA3B72B738 // c1 (low double)
	.quad	0xBF93C396B58BD74A // c2
	.quad	0x3F9887FAB9649830 // c3
	.quad	0xBF69DF6A8A7C820C // c4
	.quad	0x3F6461D2011761C1 // c5
	.quad	0xBF3E2C1220D97FBE // c6
	.quad	0x3F49F3B20EDA3AFF // c7
	.quad	0x3FD1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0xBC73DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB726AD2173D50E // c0 (high double)
	.quad	0xBC1FCEC90C8BEF70 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB69783D8B96CDF // c1 (low double)
	.quad	0xBF92DD1FBCCBA937 // c2
	.quad	0x3F986051045C3883 // c3
	.quad	0xBF68A164B9409BD2 // c4
	.quad	0x3F641CB77D330F8F // c5
	.quad	0xBF3C9FE910D9CBC2 // c6
	.quad	0xBF302CCA98190297 // c7
	.quad	0x3FD07E4CEF4CBD98 // B' = pi/2 - B (high double)
	.quad	0xBC51ABDBB9EA8E6E // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB616D1A2C591A8 // c0 (high double)
	.quad	0x3C2EF832FD935ED0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB67A9658930D84 // c1 (low double)
	.quad	0xBF91F8156CC25284 // c2
	.quad	0x3F983A9794563574 // c3
	.quad	0xBF67678272C8ECDD // c4
	.quad	0x3F63DAE696E3A38C // c5
	.quad	0xBF3B1CDBB342EE5D // c6
	.quad	0x3F49E00A3B463CAF // c7
	.quad	0x3FCF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0x3C660FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB5074ED126983E // c0 (high double)
	.quad	0x3C516E0EB4132D8C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB65F0F8C7B2809 // c1 (low double)
	.quad	0xBF9114659E4D5E41 // c2
	.quad	0x3F9816C815084CDA // c3
	.quad	0xBF66318BB353364D // c4
	.quad	0x3F639CED9C49E6E4 // c5
	.quad	0xBF39A0F6E494228E // c6
	.quad	0xBF2EBBDA5946D8F5 // c7
	.quad	0x3FCDD85A7410F58D // B' = pi/2 - B (high double)
	.quad	0xBC56156546AFA570 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB3F82048F8BE00 // c0 (high double)
	.quad	0xBC4AFD1EBA1D1F08 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB644ED6242FE19 // c1 (low double)
	.quad	0xBF9031FE64545C32 // c2
	.quad	0x3F97F4DC89754A8C // c3
	.quad	0xBF64FF499B056453 // c4
	.quad	0x3F636226CB0F9286 // c5
	.quad	0xBF382D35D5867708 // c6
	.quad	0x3F3E29EBBEAD3F41 // c7
	.quad	0x3FCC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0x3C63DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB2E941ACF368E6 // c0 (high double)
	.quad	0x3C28FF4CA630E160 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB62C2DE3B8D982 // c1 (low double)
	.quad	0xBF8EA19C1138E963 // c2
	.quad	0x3F97D4CF49C89DB5 // c3
	.quad	0xBF63D0865FBE1BAC // c4
	.quad	0x3F632AE80D59D97D // c5
	.quad	0xBF36C01A5963B74D // c6
	.quad	0x3F2F2207CBA79A42 // c7
	.quad	0x3FCAB41B09886FEA // B' = pi/2 - B (high double)
	.quad	0xBC5A7EEED374BC71 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB1DAAEA5CC26DD // c0 (high double)
	.quad	0x3C51577CBB7A4D94 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB614CF365450F4 // c1 (low double)
	.quad	0xBF8CE1861157F322 // c2
	.quad	0x3F97B69B01FB3B26 // c3
	.quad	0xBF62A50D3A3C528B // c4
	.quad	0x3F62F6E9071D40DB // c5
	.quad	0xBF3559C86E0B83FE // c6
	.quad	0x3F41AFC8FEE8B00A // c7
	.quad	0x3FC921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C61A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFB0CC62E1E04467 // c0 (high double)
	.quad	0xBC47805D7AE2B26C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5FECF9AE763EA // c1 (low double)
	.quad	0xBF8B23982579F8E5 // c2
	.quad	0x3F979A3AB01644F9 // c3
	.quad	0xBF617CAA57838224 // c4
	.quad	0x3F62C658DCA7F4AB // c5
	.quad	0xBF33F9251280A953 // c6
	.quad	0xBF2D5C4E17766D5A // c7
	.quad	0x3FC78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0xBC5EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFAF7CB429BEA72C // c0 (high double)
	.quad	0xBC3FFC35F2771324 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5EA2D6D5456CA // c1 (low double)
	.quad	0xBF8967B0057BF87E // c2
	.quad	0x3F977FA9A2ED4D75 // c3
	.quad	0xBF60572AC7A74374 // c4
	.quad	0x3F6298E78AA42A2C // c5
	.quad	0xBF329E8594822820 // c6
	.quad	0xBF37DBAE80B80EC8 // c7
	.quad	0x3FC5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0x3C5EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFAD611FEEED2925 // c0 (high double)
	.quad	0xBC2C094544F37630 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5D6E7244839E4 // c1 (low double)
	.quad	0xBF87ADABBEBF7F7F // c2
	.quad	0x3F9766E378A69DCA // c3
	.quad	0xBF5E68B8DEC46BCD // c4
	.quad	0x3F626E6BF47FF36D // c5
	.quad	0xBF3149F4C32A02B4 // c6
	.quad	0x3F4B764BFF26FBAF // c7
	.quad	0x3FC46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0xBC61A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFAB46008DFA625D // c0 (high double)
	.quad	0x3C1CFC71261A7FB0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5C4FB50FA0748 // c1 (low double)
	.quad	0xBF85F569AE5EF4C9 // c2
	.quad	0x3F974FE41D60092F // c3
	.quad	0xBF5C281BF4A7A8E8 // c4
	.quad	0x3F62476FB6177831 // c5
	.quad	0xBF2FF1DE9AA130D5 // c6
	.quad	0xBF3D841C7D3057BC // c7
	.quad	0x3FC2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0x3C5A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFA92B4D8A427A5E // c0 (high double)
	.quad	0xBC48D0F742399A2E // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5B4689EEE44C8 // c1 (low double)
	.quad	0xBF843EC87B78BC83 // c2
	.quad	0x3F973AA7CA62B764 // c3
	.quad	0xBF59EC1D907A7E74 // c4
	.quad	0x3F62232304CE0BCB // c5
	.quad	0xBF2D5C019E5C9327 // c6
	.quad	0x3F4A5742B30D6EA4 // c7
	.quad	0x3FC1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0xBC63DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFA710FE6F2ADA79 // c0 (high double)
	.quad	0x3C47FCD1AF1BC88C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5A52DD3BF1903 // c1 (low double)
	.quad	0xBF8289A7118F8015 // c2
	.quad	0x3F97272B048B0CB6 // c3
	.quad	0xBF57B45DC9402B07 // c4
	.quad	0x3F620239A251E2D3 // c5
	.quad	0xBF2ACB87C59ED9B1 // c6
	.quad	0xBF314F7BD600B48C // c7
	.quad	0x3FBF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0x3C560FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFA4F70ACF7EED25 // c0 (high double)
	.quad	0xBC36D383618823A2 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB59749CEE8C351 // c1 (low double)
	.quad	0xBF80D5E49AFDF8A4 // c2
	.quad	0x3F97156A9BCB4ECE // c3
	.quad	0xBF55807DE55737F7 // c4
	.quad	0x3F61E4232793575A // c5
	.quad	0xBF28440F1D68D5AE // c6
	.quad	0xBF29FA204333F00C // c7
	.quad	0x3FBC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0x3C53DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFA2DD6A44CE1394 // c0 (high double)
	.quad	0xBC4BFF776DACC8B5 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB58ABB899A6815 // c1 (low double)
	.quad	0xBF7E46C0F6F92ADF // c2
	.quad	0x3F970563A9ED840F // c3
	.quad	0xBF53502047076AB7 // c4
	.quad	0x3F61C8E9B000D23F // c5
	.quad	0xBF25C4A8BD148199 // c6
	.quad	0x3F4131449FFC5A0C // c7
	.quad	0x3FB921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C51A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBFA0C4146ECAB658 // c0 (high double)
	.quad	0x3C0F927420EB8C00 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB57F82168B23EC // c1 (low double)
	.quad	0xBF7AE3F4956ED507 // c2
	.quad	0x3F96F71391CF9DFE // c3
	.quad	0xBF5122E851208A87 // c4
	.quad	0x3F61B0E12D5FF408 // c5
	.quad	0xBF23485E9450F437 // c6
	.quad	0xBF3CE0F3BAD9A7CF // c7
	.quad	0x3FB5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0x3C4EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF9D5601E554ACE4 // c0 (high double)
	.quad	0x3BF1725D940FAE40 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5759CA1D35995 // c1 (low double)
	.quad	0xBF7783239DE5AD22 // c2
	.quad	0x3F96EA77FED69EC2 // c3
	.quad	0xBF4DF0F48A9F5DD6 // c4
	.quad	0x3F619B9AD4B4F692 // c5
	.quad	0xBF20D3375495B3AF // c6
	.quad	0xBF3C5F6BDA9649C1 // c7
	.quad	0x3FB2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0x3C4A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF99244EF50D08B4 // c0 (high double)
	.quad	0xBBF5D43FBFA6A650 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB56D0A70CA2FAE // c1 (low double)
	.quad	0xBF74240DEF7AA427 // c2
	.quad	0x3F96DF8EE40AD931 // c3
	.quad	0xBF49A0F663CD9CED // c4
	.quad	0x3F61892DA49A01B1 // c5
	.quad	0xBF1CC6A5F5CD8613 // c6
	.quad	0xBF2979EF025F5A5D // c7
	.quad	0x3FAF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0x3C460FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF94F2FF697D458F // c0 (high double)
	.quad	0xBC2D47EE0D8639CC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB565CAE1E73473 // c1 (low double)
	.quad	0xBF70C673B159CE1D // c2
	.quad	0x3F96D6567B9EC32E // c3
	.quad	0xBF455521A900DE57 // c4
	.quad	0x3F6179A180C97DC6 // c5
	.quad	0xBF17EE9A1A2093EC // c6
	.quad	0xBF0F289C23A5BE3D // c7
	.quad	0x3FA921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C41A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF90C202A6B4B69C // c0 (high double)
	.quad	0x3C232719F8027342 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB55FDD6CA81EFB // c1 (low double)
	.quad	0xBF6AD42A909B3D64 // c2
	.quad	0x3F96CECD466E9161 // c3
	.quad	0xBF410CC300A993C8 // c4
	.quad	0x3F616CF28C1A20AD // c5
	.quad	0xBF131CA14234B209 // c6
	.quad	0xBF0575725B2F6BD2 // c7
	.quad	0x3FA2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0x3C3A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF8922902DB2AC11 // c0 (high double)
	.quad	0xBC23466BF961968D // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB55B41A17AA68C // c1 (low double)
	.quad	0xBF641D6698BEFAD5 // c2
	.quad	0x3F96C8F20B94D936 // c3
	.quad	0xBF398E50110336DC // c4
	.quad	0x3F61630EF83C40BE // c5
	.quad	0xBF0CA4CD1D39A327 // c6
	.quad	0x3F2D88AA4542B2A9 // c7
	.quad	0x3F9921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C31A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF80C17E51DDB68F // c0 (high double)
	.quad	0xBC2BCDF2CE757F58 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB557F729AA69CB // c1 (low double)
	.quad	0xBF5AD039FA1B06A9 // c2
	.quad	0x3F96C4C3D80DF7DB // c3
	.quad	0xBF31073E4843262F // c4
	.quad	0x3F615BF0FBDD75F6 // c5
	.quad	0xBF032668F6EE96B2 // c6
	.quad	0x3F4AB2BDCBD5B586 // c7
	.quad	0x3F8921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0x3C21A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0xBF70C15D3E7A6D22 // c0 (high double)
	.quad	0xBC031AD41335B3D2 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB555FDC752E0B2 // c1 (low double)
	.quad	0xBF4ACF3DF3195D30 // c2
	.quad	0x3F96C241FE6B7D5F // c3
	.quad	0xBF2105DD68706FD9 // c4
	.quad	0x3F6157CCAEE0E30B // c5
	.quad	0xBEF320E95C86AAB5 // c6
	.quad	0x3F36CC59FF7E57F3 // c7
	.quad	0x0000000000000000 // B' = pi/2 - B (high double)
	.quad	0x0000000000000000 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x0000000000000000 // c0 (high double)
	.quad	0x0000000000000000 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5555555555555 // c1 (low double)
	.quad	0x0000000000000000 // c2
	.quad	0x3F96C16C16C4A87E // c3
	.quad	0x0000000000000000 // c4
	.quad	0x3F615658C88FB22B // c5
	.quad	0x0000000000000000 // c6
	.quad	0x3F45FEB9DABEDDC8 // c7
	.quad	0xBF8921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC21A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F70C15D3E7A6D22 // c0 (high double)
	.quad	0x3C031AD4127D9F3E // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB555FDC752E0B2 // c1 (low double)
	.quad	0x3F4ACF3DF3195D30 // c2
	.quad	0x3F96C241FE6B7D5F // c3
	.quad	0x3F2105DD68707011 // c4
	.quad	0x3F6157CCAEE0E445 // c5
	.quad	0x3EF320E95BE3603D // c6
	.quad	0x3F36CC59FB2A5B52 // c7
	.quad	0xBF9921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC31A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F80C17E51DDB68F // c0 (high double)
	.quad	0x3C2BCDF2CE751153 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB557F729AA69CB // c1 (low double)
	.quad	0x3F5AD039FA1B06A9 // c2
	.quad	0x3F96C4C3D80DF7DB // c3
	.quad	0x3F31073E4843262E // c4
	.quad	0x3F615BF0FBDD7613 // c5
	.quad	0x3F032668F6F21096 // c6
	.quad	0x3F4AB2BDCBA34188 // c7
	.quad	0xBFA2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0xBC3A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F8922902DB2AC11 // c0 (high double)
	.quad	0x3C23466BF9645874 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB55B41A17AA68C // c1 (low double)
	.quad	0x3F641D6698BEFAD5 // c2
	.quad	0x3F96C8F20B94D936 // c3
	.quad	0x3F398E50110336D4 // c4
	.quad	0x3F61630EF83C4025 // c5
	.quad	0x3F0CA4CD1D548C70 // c6
	.quad	0x3F2D88AA497F5A74 // c7
	.quad	0xBFA921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC41A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F90C202A6B4B69C // c0 (high double)
	.quad	0xBC232719F82E8E78 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB55FDD6CA81EFB // c1 (low double)
	.quad	0x3F6AD42A909B3D64 // c2
	.quad	0x3F96CECD466E9161 // c3
	.quad	0x3F410CC300A993FB // c4
	.quad	0x3F616CF28C1A27D9 // c5
	.quad	0x3F131CA1418B03D0 // c6
	.quad	0xBF0575732696DF46 // c7
	.quad	0xBFAF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0xBC460FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F94F2FF697D458F // c0 (high double)
	.quad	0x3C2D47EE0D83F132 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB565CAE1E73473 // c1 (low double)
	.quad	0x3F70C673B159CE1D // c2
	.quad	0x3F96D6567B9EC32E // c3
	.quad	0x3F455521A900DE58 // c4
	.quad	0x3F6179A180C97E50 // c5
	.quad	0x3F17EE9A1A1D88C1 // c6
	.quad	0xBF0F289C329281E6 // c7
	.quad	0xBFB2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0xBC4A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F99244EF50D08B4 // c0 (high double)
	.quad	0x3BF5D43FBF890B40 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB56D0A70CA2FAE // c1 (low double)
	.quad	0x3F74240DEF7AA427 // c2
	.quad	0x3F96DF8EE40AD931 // c3
	.quad	0x3F49A0F663CD9CEE // c4
	.quad	0x3F61892DA49A0241 // c5
	.quad	0x3F1CC6A5F5CAD66B // c6
	.quad	0xBF2979EF063C00F3 // c7
	.quad	0xBFB5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0xBC4EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3F9D5601E554ACE4 // c0 (high double)
	.quad	0xBBF1725D944B6FF0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5759CA1D35995 // c1 (low double)
	.quad	0x3F7783239DE5AD22 // c2
	.quad	0x3F96EA77FED69EC2 // c3
	.quad	0x3F4DF0F48A9F5DD8 // c4
	.quad	0x3F619B9AD4B4F72F // c5
	.quad	0x3F20D33754930861 // c6
	.quad	0xBF3C5F6BDCAA4F14 // c7
	.quad	0xBFB921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC51A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FA0C4146ECAB658 // c0 (high double)
	.quad	0xBC0F9274210C3E30 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB57F82168B23EC // c1 (low double)
	.quad	0x3F7AE3F4956ED507 // c2
	.quad	0x3F96F71391CF9DFE // c3
	.quad	0x3F5122E851208A88 // c4
	.quad	0x3F61B0E12D5FF46B // c5
	.quad	0x3F23485E944E645F // c6
	.quad	0xBF3CE0F3BC206D6A // c7
	.quad	0xBFBC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0xBC53DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FA2DD6A44CE1394 // c0 (high double)
	.quad	0x3C4BFF776DAE2ACC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB58ABB899A6815 // c1 (low double)
	.quad	0x3F7E46C0F6F92ADF // c2
	.quad	0x3F970563A9ED840F // c3
	.quad	0x3F53502047076AB6 // c4
	.quad	0x3F61C8E9B000D225 // c5
	.quad	0x3F25C4A8BD17699A // c6
	.quad	0x3F413144A024F67F // c7
	.quad	0xBFBF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0xBC560FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FA4F70ACF7EED25 // c0 (high double)
	.quad	0x3C36D38361835224 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB59749CEE8C351 // c1 (low double)
	.quad	0x3F80D5E49AFDF8A4 // c2
	.quad	0x3F97156A9BCB4ECE // c3
	.quad	0x3F55807DE55737F9 // c4
	.quad	0x3F61E42327935766 // c5
	.quad	0x3F28440F1D62D6E1 // c6
	.quad	0xBF29FA20436AFB73 // c7
	.quad	0xBFC1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0x3C63DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FA710FE6F2ADA79 // c0 (high double)
	.quad	0xBC47FCD1AF1E5F81 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5A52DD3BF1903 // c1 (low double)
	.quad	0x3F8289A7118F8015 // c2
	.quad	0x3F97272B048B0CB6 // c3
	.quad	0x3F57B45DC9402B09 // c4
	.quad	0x3F620239A251E2C5 // c5
	.quad	0x3F2ACB87C598F86B // c6
	.quad	0xBF314F7BD5BDC85C // c7
	.quad	0xBFC2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0xBC5A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FA92B4D8A427A5E // c0 (high double)
	.quad	0x3C48D0F7423D245A // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5B4689EEE44C8 // c1 (low double)
	.quad	0x3F843EC87B78BC83 // c2
	.quad	0x3F973AA7CA62B764 // c3
	.quad	0x3F59EC1D907A7E71 // c4
	.quad	0x3F62232304CE0BF5 // c5
	.quad	0x3F2D5C019E659382 // c6
	.quad	0x3F4A5742B2B8EFA5 // c7
	.quad	0xBFC46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0x3C61A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FAB46008DFA625D // c0 (high double)
	.quad	0xBC1CFC712619CB40 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5C4FB50FA0748 // c1 (low double)
	.quad	0x3F85F569AE5EF4C9 // c2
	.quad	0x3F974FE41D60092F // c3
	.quad	0x3F5C281BF4A7A8E8 // c4
	.quad	0x3F62476FB617789E // c5
	.quad	0x3F2FF1DE9AA127B2 // c6
	.quad	0xBF3D841C7EAC9D16 // c7
	.quad	0xBFC5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0xBC5EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FAD611FEEED2925 // c0 (high double)
	.quad	0x3C2C094544F32A78 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5D6E7244839E4 // c1 (low double)
	.quad	0x3F87ADABBEBF7F7F // c2
	.quad	0x3F9766E378A69DCA // c3
	.quad	0x3F5E68B8DEC46BCD // c4
	.quad	0x3F626E6BF47FF315 // c5
	.quad	0x3F3149F4C32A0685 // c6
	.quad	0x3F4B764BFFC07D27 // c7
	.quad	0xBFC78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0x3C5EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FAF7CB429BEA72C // c0 (high double)
	.quad	0x3C3FFC35F277313A // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5EA2D6D5456CA // c1 (low double)
	.quad	0x3F8967B0057BF87E // c2
	.quad	0x3F977FA9A2ED4D75 // c3
	.quad	0x3F60572AC7A74374 // c4
	.quad	0x3F6298E78AA42A95 // c5
	.quad	0x3F329E8594822508 // c6
	.quad	0xBF37DBAE822657B7 // c7
	.quad	0xBFC921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC61A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB0CC62E1E04467 // c0 (high double)
	.quad	0x3C47805D7AE2BD94 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB5FECF9AE763EA // c1 (low double)
	.quad	0x3F8B23982579F8E5 // c2
	.quad	0x3F979A3AB01644F9 // c3
	.quad	0x3F617CAA57838224 // c4
	.quad	0x3F62C658DCA7F4F9 // c5
	.quad	0x3F33F9251280A70A // c6
	.quad	0xBF2D5C4E19969DC6 // c7
	.quad	0xBFCAB41B09886FEA // B' = pi/2 - B (high double)
	.quad	0x3C5A7EEED374BC71 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB1DAAEA5CC26DD // c0 (high double)
	.quad	0xBC51577CBB7A50E2 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB614CF365450F4 // c1 (low double)
	.quad	0x3F8CE1861157F322 // c2
	.quad	0x3F97B69B01FB3B26 // c3
	.quad	0x3F62A50D3A3C528B // c4
	.quad	0x3F62F6E9071D40AC // c5
	.quad	0x3F3559C86E0B8558 // c6
	.quad	0x3F41AFC8FF3AAA17 // c7
	.quad	0xBFCC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0xBC63DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB2E941ACF368E6 // c0 (high double)
	.quad	0xBC28FF4CA630E010 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB62C2DE3B8D982 // c1 (low double)
	.quad	0x3F8EA19C1138E963 // c2
	.quad	0x3F97D4CF49C89DB5 // c3
	.quad	0x3F63D0865FBE1BAC // c4
	.quad	0x3F632AE80D59D97F // c5
	.quad	0x3F36C01A5963B73C // c6
	.quad	0x3F2F2207CB99A5D3 // c7
	.quad	0xBFCDD85A7410F58D // B' = pi/2 - B (high double)
	.quad	0x3C56156546AFA570 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB3F82048F8BE00 // c0 (high double)
	.quad	0x3C4AFD1EBA1D1ABA // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB644ED6242FE19 // c1 (low double)
	.quad	0x3F9031FE64545C32 // c2
	.quad	0x3F97F4DC89754A8C // c3
	.quad	0x3F64FF499B056453 // c4
	.quad	0x3F636226CB0F9266 // c5
	.quad	0x3F382D35D58677E8 // c6
	.quad	0x3F3E29EBBF1CE01E // c7
	.quad	0xBFCF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0xBC660FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB5074ED126983E // c0 (high double)
	.quad	0xBC516E0EB4132866 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB65F0F8C7B2809 // c1 (low double)
	.quad	0x3F9114659E4D5E41 // c2
	.quad	0x3F9816C815084CDA // c3
	.quad	0x3F66318BB353364D // c4
	.quad	0x3F639CED9C49E72F // c5
	.quad	0x3F39A0F6E4942078 // c6
	.quad	0xBF2EBBDA5B521AEE // c7
	.quad	0xBFD07E4CEF4CBD98 // B' = pi/2 - B (high double)
	.quad	0x3C51ABDBB9EA8E6E // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB616D1A2C591A8 // c0 (high double)
	.quad	0xBC2EF832FD938898 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB67A9658930D84 // c1 (low double)
	.quad	0x3F91F8156CC25284 // c2
	.quad	0x3F983A9794563574 // c3
	.quad	0x3F67678272C8ECDD // c4
	.quad	0x3F63DAE696E3A33F // c5
	.quad	0x3F3B1CDBB342F07A // c6
	.quad	0x3F49E00A3BCC8A23 // c7
	.quad	0xBFD1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0x3C73DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB726AD2173D50E // c0 (high double)
	.quad	0x3C1FCEC90C8C4150 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB69783D8B96CDF // c1 (low double)
	.quad	0x3F92DD1FBCCBA937 // c2
	.quad	0x3F986051045C3883 // c3
	.quad	0x3F68A164B9409BD2 // c4
	.quad	0x3F641CB77D330FDA // c5
	.quad	0x3F3C9FE910D9C9B2 // c6
	.quad	0xBF302CCA991EA2A4 // c7
	.quad	0xBFD2106CA4910069 // B' = pi/2 - B (high double)
	.quad	0xBC7CAF6B74B6A225 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB836E5B77D959A // c0 (high double)
	.quad	0x3C51BDCEFF692111 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB6B5DA3B72B738 // c1 (low double)
	.quad	0x3F93C396B58BD74A // c2
	.quad	0x3F9887FAB9649830 // c3
	.quad	0x3F69DF6A8A7C820C // c4
	.quad	0x3F6461D201176179 // c5
	.quad	0x3F3E2C1220D981B3 // c6
	.quad	0x3F49F3B20F57CFA8 // c7
	.quad	0xBFD2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0xBC6A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FB9477FD636A4D8 // c0 (high double)
	.quad	0xBC5063C8B22C4544 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB6D59BCBF6C48B // c1 (low double)
	.quad	0x3F94AB8CBB6CBB16 // c2
	.quad	0x3F98B19B6070D56D // c3
	.quad	0x3F6B21CD22DB24B2 // c4
	.quad	0x3F64AAE4575F07F6 // c5
	.quad	0x3F3FC08B3806F34C // c6
	.quad	0xBF00E42769BBA67A // c7
	.quad	0xBFD3A28C59D5433B // B' = pi/2 - B (high double)
	.quad	0x3C41B19140C0C0D5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FBA587FF6553726 // c0 (high double)
	.quad	0xBC5559CFE8367078 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB6F6CAF293AC7F // c1 (low double)
	.quad	0x3F959514736F007F // c2
	.quad	0x3F98DD3A019E14EB // c3
	.quad	0x3F6C68C706B3D0FB // c4
	.quad	0x3F64F779DB6C50B2 // c5
	.quad	0x3F40AF7CD7497CB8 // c6
	.quad	0x3F4AD3B9828EFAF2 // c7
	.quad	0xBFD46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0x3C71A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FBB69EA984DE6EF // c0 (high double)
	.quad	0xBC573CB7437F11CA // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB7196A3515E12E // c1 (low double)
	.quad	0x3F968040C68C8FE8 // c2
	.quad	0x3F990ADE01D185E6 // c3
	.quad	0x3F6DB49417B3E65C // c4
	.quad	0x3F6548393E856C2F // c5
	.quad	0x3F418353F7828212 // c6
	.quad	0xBF36489E043C6801 // c7
	.quad	0xBFD534AC0F19860C // B' = pi/2 - B (high double)
	.quad	0xBC7EE4303B192DA6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FBC7BC444B10758 // c0 (high double)
	.quad	0xBC511540B2269343 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB73D7C3735AA80 // c1 (low double)
	.quad	0x3F976D24E52F1F79 // c2
	.quad	0x3F993A8F25560FF4 // c3
	.quad	0x3F6F0571A586F6F3 // c4
	.quad	0x3F659C8E8181F3A3 // c5
	.quad	0x3F425CCABE1034F4 // c6
	.quad	0x3F4D59CA0F6FD35D // c7
	.quad	0xBFD5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0xBC6EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FBD8E118C89577F // c0 (high double)
	.quad	0x3C3344F06C36B16C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB76303BB0A22C5 // c1 (low double)
	.quad	0x3F985BD44ABC0416 // c2
	.quad	0x3F996C5591BA6DFA // c3
	.quad	0x3F702DCF4265B5B3 // c4
	.quad	0x3F65F52766DBD111 // c5
	.quad	0x3F433B879649EF12 // c6
	.quad	0x3F47C67A938B39B9 // c7
	.quad	0xBFD6C6CBC45DC8DE // B' = pi/2 - B (high double)
	.quad	0x3BB6D61B58C99C43 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FBEA0D709BC27CA // c0 (high double)
	.quad	0x3C48928D103E8AAC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB78A03A181D6AC // c1 (low double)
	.quad	0x3F994C62C1366C88 // c2
	.quad	0x3F99A039D09A41AD // c3
	.quad	0x3F70DBAD90C6529A // c4
	.quad	0x3F66520A2F7F46B3 // c5
	.quad	0x3F441FF58F223B31 // c6
	.quad	0xBF357CCF19FA9D15 // c7
	.quad	0xBFD78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0x3C6EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FBFB4195F6B0333 // c0 (high double)
	.quad	0x3C47DB89E2705528 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB7B27EEAE12CDB // c1 (low double)
	.quad	0x3F9A3EE464F938D9 // c2
	.quad	0x3F99D644D21E7AE8 // c3
	.quad	0x3F718C74CAAA7BA1 // c4
	.quad	0x3F66B2F778B94D1C // c5
	.quad	0x3F450AB96EF7371C // c6
	.quad	0x3F1412EF7B7239D9 // c7
	.quad	0xBFD858EB79A20BB0 // B' = pi/2 - B (high double)
	.quad	0x3C7EE70AFE8446D9 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC063EE9D2B777C // c0 (high double)
	.quad	0x3C4E1787D8B79264 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB7DC78B746BB2F // c1 (low double)
	.quad	0x3F9B336DA889AC7D // c2
	.quad	0x3F9A0E7FEF8737D6 // c3
	.quad	0x3F724046DF328844 // c4
	.quad	0x3F67185639AB9983 // c5
	.quad	0x3F45FBD25ACB3A74 // c6
	.quad	0x3F0E765A126439F7 // c7
	.quad	0xBFD921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC71A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC0EE13A8A2A9C6 // c0 (high double)
	.quad	0xBC5830A63DDC366C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB807F44735B1A4 // c1 (low double)
	.quad	0x3F9C2A135884458A // c2
	.quad	0x3F9A48F4EE376E9D // c3
	.quad	0x3F72F746AAC73A47 // c4
	.quad	0x3F678215824701AF // c5
	.quad	0x3F46F3C719FB66E6 // c6
	.quad	0x3F4BA88D5ED09DD1 // c7
	.quad	0xBFD9EB0B2EE64E81 // B' = pi/2 - B (high double)
	.quad	0xBC419ABB2567F739 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC1787E32B35112 // c0 (high double)
	.quad	0xBC5D2D85EBE7CAE6 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB834F4FC268515 // c1 (low double)
	.quad	0x3F9D22EA9FA50E7D // c2
	.quad	0x3F9A85AE02819BA3 // c3
	.quad	0x3F73B19803AB2B21 // c4
	.quad	0x3F67F0B8C640461D // c5
	.quad	0x3F47F293167B409B // c6
	.quad	0xBF198BF45EA16C3A // c7
	.quad	0xBFDAB41B09886FEA // B' = pi/2 - B (high double)
	.quad	0x3C6A7EEED374BC71 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC20330A15F6568 // c0 (high double)
	.quad	0x3C61BB4F210EA8D1 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB8637E591E05FE // c1 (low double)
	.quad	0x3F9E1E090AECCE81 // c2
	.quad	0x3F9AC4B5D3090946 // c3
	.quad	0x3F746F5FC5C316BA // c4
	.quad	0x3F686400E51F53C2 // c5
	.quad	0x3F48F8EADC3DB9C7 // c6
	.quad	0xBF320AB3DF7578B7 // c7
	.quad	0xBFDB7D2AE42A9153 // B' = pi/2 - B (high double)
	.quad	0x3C7CB2463821BB58 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC28E2D6047CBC8 // c0 (high double)
	.quad	0xBC0CEAB2537F4380 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB89394034B11CF // c1 (low double)
	.quad	0x3F9F1B848DE48876 // c2
	.quad	0x3F9B06177BDD3821 // c3
	.quad	0x3F7530C3E0150AA0 // c4
	.quad	0x3F68DBF62FBA70A3 // c5
	.quad	0x3F4A0736C860B4FE // c6
	.quad	0x3F50A3F1D969C867 // c7
	.quad	0xBFDC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0xBC73DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC31976E0E280B3 // c0 (high double)
	.quad	0xBC667516EE4F2544 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB8C539C2AB0F89 // c1 (low double)
	.quad	0x3FA00DB9C3806462 // c2
	.quad	0x3F9B49DE91DB8E44 // c3
	.quad	0x3F75F5EB6258C838 // c4
	.quad	0x3F695952C6BEE632 // c5
	.quad	0x3F4B1D691B1E9E68 // c6
	.quad	0xBEFC71567DD0AC40 // c7
	.quad	0xBFDD0F4A996ED424 // B' = pi/2 - B (high double)
	.quad	0xBC51A070AC3E29A0 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC3A50F9AB1CA91 // c0 (high double)
	.quad	0x3C50CA3B48D90906 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB8F87382B56ACB // c1 (low double)
	.quad	0x3FA08EF662132882 // c2
	.quad	0x3F9B901726A3CD1C // c3
	.quad	0x3F76BEFE8A4461A2 // c4
	.quad	0x3F69DBAE42D8A424 // c5
	.quad	0x3F4C3C52357BEFDC // c6
	.quad	0x3F32456D9E4ECEE8 // c7
	.quad	0xBFDDD85A7410F58D // B' = pi/2 - B (high double)
	.quad	0x3C66156546AFA570 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC430FA0B7C7F15 // c0 (high double)
	.quad	0x3C63115D3C69C8AB // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB92D45530E4319 // c1 (low double)
	.quad	0x3FA11183C3A8DFF6 // c2
	.quad	0x3F9BD8CDCC073185 // c3
	.quad	0x3F778C26D2F52E9A // c4
	.quad	0x3F6A638650FB67B4 // c5
	.quad	0x3F4D6420527BDA5C // c6
	.quad	0xBF2DA5A647B98994 // c7
	.quad	0xBFDEA16A4EB316F6 // B' = pi/2 - B (high double)
	.quad	0x3C7A7D8171BF2FD8 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC4BD38B7876716 // c0 (high double)
	.quad	0x3C5504069A91A838 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB963B368408780 // c1 (low double)
	.quad	0x3FA1956DC5B1F35A // c2
	.quad	0x3F9C240F984C07DC // c3
	.quad	0x3F785D8F0388F11C // c4
	.quad	0x3F6AF0B073DF1CB2 // c5
	.quad	0x3F4E9580D70D3533 // c6
	.quad	0x3F4571315C6E8041 // c7
	.quad	0xBFDF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0xBC760FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC549CE29CFCCBE // c0 (high double)
	.quad	0xBC69881846E0BF86 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB99BC21C7FB9B2 // c1 (low double)
	.quad	0x3FA21AC0848A46C5 // c2
	.quad	0x3F9C71EA2A16819D // c3
	.quad	0x3F7933633F8F0072 // c4
	.quad	0x3F6B83D1C5509A03 // c5
	.quad	0x3F4FD088727B14B9 // c6
	.quad	0xBF2306C949D5376C // c7
	.quad	0xBFE019C501FBACE4 // B' = pi/2 - B (high double)
	.quad	0x3C8CB18F8746F50C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC5D6BCF447403E // c0 (high double)
	.quad	0xBC6C6C662876B393 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FB9D575F071964A // c1 (low double)
	.quad	0x3FA2A1885E06BEAC // c2
	.quad	0x3F9CC26BAD1B38B8 // c3
	.quad	0x3F7A0DD116DA5229 // c4
	.quad	0x3F6C1C921E809C74 // c5
	.quad	0x3F508B03CEF8B0C3 // c6
	.quad	0x3F4BCE57D088C848 // c7
	.quad	0xBFE07E4CEF4CBD98 // B' = pi/2 - B (high double)
	.quad	0x3C61ABDBB9EA8E6E // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC66407B0109FA6 // c0 (high double)
	.quad	0x3C55C3EC26D04666 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBA10D38BFFE319 // c1 (low double)
	.quad	0x3FA329D1F417DBD6 // c2
	.quad	0x3F9D15A2DE544C7C // c3
	.quad	0x3F7AED07977D3236 // c4
	.quad	0x3F6CBBAD3C53FDE7 // c5
	.quad	0x3F513318B342B49D // c6
	.quad	0x3F1AF93D0FFB8A50 // c7
	.quad	0xBFE0E2D4DC9DCE4C // B' = pi/2 - B (high double)
	.quad	0xBC83DBA1AA51ADD5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC6F1B0FDBE6EDE // c0 (high double)
	.quad	0x3C614F63025E35DC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBA4DDFBF32A727 // c1 (low double)
	.quad	0x3FA3B3AA2F826E47 // c2
	.quad	0x3F9D6B9F112A3CA9 // c3
	.quad	0x3F7BD1375F2B326E // c4
	.quad	0x3F6D60EF002A5959 // c5
	.quad	0x3F51E0E0E3E641DE // c6
	.quad	0x3F32BFA3B8923D31 // c7
	.quad	0xBFE1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0x3C83DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC77FBB85928D2D // c0 (high double)
	.quad	0xBC6121F4E1AC8166 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBA8C9F83131391 // c1 (low double)
	.quad	0x3FA43F1E42AF6970 // c2
	.quad	0x3F9DC4703433CA75 // c3
	.quad	0x3F7CBA92AE7FE396 // c4
	.quad	0x3F6E0CB6D0475CCF // c5
	.quad	0x3F529491FB8790D2 // c6
	.quad	0x3F37152AFD195A10 // c7
	.quad	0xBFE1ABE4B73FEFB5 // B' = pi/2 - B (high double)
	.quad	0xBC61A34B6FA942D3 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC80E29F7BF5633 // c0 (high double)
	.quad	0x3C67F8F06ACC7061 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBACD17FA976786 // c1 (low double)
	.quad	0x3FA4CC3BAC93F075 // c2
	.quad	0x3F9E2026D6967836 // c3
	.quad	0x3F7DA94D7C694B78 // c4
	.quad	0x3F6EBF4F14B98A92 // c5
	.quad	0x3F534E7211F873CE // c6
	.quad	0xBF2290D13622948A // c7
	.quad	0xBFE2106CA4910069 // B' = pi/2 - B (high double)
	.quad	0xBC8CAF6B74B6A225 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC89CFF0CAA3CB4 // c0 (high double)
	.quad	0xBC6A8D50D06FC283 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBB0F4E73981D4B // c1 (low double)
	.quad	0x3FA55B103BB2BD6D // c2
	.quad	0x3F9E7ED42D91EC8D // c3
	.quad	0x3F7E9D9D8AAE7B14 // c4
	.quad	0x3F6F78B5039EFAD8 // c5
	.quad	0x3F540EDBA3A78643 // c6
	.quad	0x3F3AA2CB5E5BE1ED // c7
	.quad	0xBFE274F491E2111E // B' = pi/2 - B (high double)
	.quad	0x3C7613F7E4FA18D6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC92C3D852FEDF2 // c0 (high double)
	.quad	0x3C20F5E7403E5E20 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBB534867CEB1CB // c1 (low double)
	.quad	0x3FA5EBAA113813D4 // c2
	.quad	0x3F9EE08A1A202CC5 // c3
	.quad	0x3F7F97BA7BA1301A // c4
	.quad	0x3F701CAD7F191DFA // c5
	.quad	0x3F54D60D13221310 // c6
	.quad	0x3F45F5FE99894FC6 // c7
	.quad	0xBFE2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0xBC7A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FC9BBE82AEA0D12 // c0 (high double)
	.quad	0xBC62D116CA77096B // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBB990B7DDE5BFC // c1 (low double)
	.quad	0x3FA67E17A4317C6C // c2
	.quad	0x3F9F455B2F170454 // c3
	.quad	0x3F804BEEF437A2B3 // c4
	.quad	0x3F7080CEADF2976D // c5
	.quad	0x3F55A44E3A71E919 // c6
	.quad	0xBF039F946FDF9BBB // c7
	.quad	0xBFE33E046C843287 // B' = pi/2 - B (high double)
	.quad	0x3C8A7CCAC0E4698B // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCA4C01D0769648 // c0 (high double)
	.quad	0x3C4DDC96CAE4A06C // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBBE09D8A670D72 // c1 (low double)
	.quad	0x3FA71267C4E29753 // c2
	.quad	0x3F9FAD5AB78BB764 // c3
	.quad	0x3F80CF21BC414F68 // c4
	.quad	0x3F70E8AC1DCF4352 // c5
	.quad	0x3F567A14C053085F // c6
	.quad	0x3F4F621D6DE176C4 // c7
	.quad	0xBFE3A28C59D5433B // B' = pi/2 - B (high double)
	.quad	0x3C51B19140C0C0D5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCADC8D51C0FA5B // c0 (high double)
	.quad	0x3C54B350B5D39570 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBC2A04912319A8 // c1 (low double)
	.quad	0x3FA7A8A9A038608E // c2
	.quad	0x3FA00C4E5E972915 // c3
	.quad	0x3F8155947D4C5C83 // c4
	.quad	0x3F7154A7B149DA58 // c5
	.quad	0x3F5757960EDFC45C // c6
	.quad	0x3F3F895706E3352B // c7
	.quad	0xBFE40714472653EF // B' = pi/2 - B (high double)
	.quad	0xBC86106670B43955 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCB6D8D944D0371 // c0 (high double)
	.quad	0xBC6CDD9D6EA4BA41 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBC7546C60FE5C7 // c1 (low double)
	.quad	0x3FA840ECC35C53FD // c2
	.quad	0x3FA0439B07C380F3 // c3
	.quad	0x3F81DF673F038AB5 // c4
	.quad	0x3F71C4B3C36B8A12 // c5
	.quad	0x3F583D44FA2FB203 // c6
	.quad	0x3F52EE3E6132A151 // c7
	.quad	0xBFE46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0x3C81A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCBFF05878394D3 // c0 (high double)
	.quad	0x3C486912F813E2EC // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBCC26A8EA2077F // c1 (low double)
	.quad	0x3FA8DB411F68EE69 // c2
	.quad	0x3FA07C9E25737218 // c3
	.quad	0x3F826CBB3713A292 // c4
	.quad	0x3F72392A32F26720 // c5
	.quad	0x3F592B76EEB6E2FF // c6
	.quad	0x3F39BADB5780F9BE // c7
	.quad	0xBFE4D02421C87558 // B' = pi/2 - B (high double)
	.quad	0xBC6A765E893370D7 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCC90F8250157F7 // c0 (high double)
	.quad	0x3C657D8312BFEA17 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBD117683053E13 // c1 (low double)
	.quad	0x3FA977B70D411C29 // c2
	.quad	0x3FA0B762F022F359 // c3
	.quad	0x3F82FDB2D7324A8F // c4
	.quad	0x3F72B2030A36AADF // c5
	.quad	0x3F5A22A22953FC15 // c6
	.quad	0x3F473A2F09CE8F95 // c7
	.quad	0xBFE534AC0F19860C // B' = pi/2 - B (high double)
	.quad	0xBC8EE4303B192DA6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCD236870E769B5 // c0 (high double)
	.quad	0x3C523CE78218C6AA // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBD62716F68B5FE // c1 (low double)
	.quad	0x3FAA165F518C38AB // c2
	.quad	0x3FA0F3F50E789484 // c3
	.quad	0x3F839271DC18DB75 // c4
	.quad	0x3F732F8BAC4C54A8 // c5
	.quad	0x3F5B23248F865B74 // c6
	.quad	0x3F36E307E2B0837C // c7
	.quad	0xBFE59933FC6A96C1 // B' = pi/2 - B (high double)
	.quad	0x3C71AA6E583501D4 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCDB6597A2E1B61 // c0 (high double)
	.quad	0xBC6CB71A947AACF6 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBDB56255580CA4 // c1 (low double)
	.quad	0x3FAAB74B20D856E7 // c2
	.quad	0x3FA1326099896038 // c3
	.quad	0x3F842B1D5CF84F74 // c4
	.quad	0x3F73B1CC2C4E32D2 // c5
	.quad	0x3F5C2D7D75B7FC1B // c6
	.quad	0x3F53DB94CE2C2FD8 // c7
	.quad	0xBFE5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0xBC7EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCE49CE5AF9DC29 // c0 (high double)
	.quad	0xBC382336F1C6BD90 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBE0A506D218F3C // c1 (low double)
	.quad	0x3FAB5A8C23E49FEA // c2
	.quad	0x3FA172B22117D625 // c3
	.quad	0x3F84C7DBDBF304B9 // c4
	.quad	0x3F74391DF553E930 // c5
	.quad	0x3F5D4211231EF24A // c6
	.quad	0x3F52AB1CCD8D3AA9 // c7
	.quad	0xBFE66243D70CB82A // B' = pi/2 - B (high double)
	.quad	0x3C884805FA81DE0A // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCEDDCA38F25FEC // c0 (high double)
	.quad	0xBC65AACB9E8B4DF8 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBE6143274A35BF // c1 (low double)
	.quad	0x3FAC00347C15ABB2 // c2
	.quad	0x3FA1B4F6B03A62C8 // c3
	.quad	0x3F8568D557225239 // c4
	.quad	0x3F74C5A8346B0155 // c5
	.quad	0x3F5E61680C7E386A // c6
	.quad	0x3F45A81E03899535 // c7
	.quad	0xBFE6C6CBC45DC8DE // B' = pi/2 - B (high double)
	.quad	0x3BC6D61B58C99C43 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FCF7250459C1976 // c0 (high double)
	.quad	0x3C40B5B081F06BA0 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBEBA422E0FE0E5 // c1 (low double)
	.quad	0x3FACA856C815D042 // c2
	.quad	0x3FA1F93BD218DCE9 // c3
	.quad	0x3F860E335AB42933 // c4
	.quad	0x3F75578EEA5D18D8 // c5
	.quad	0x3F5F8BFBAD564E86 // c6
	.quad	0x3F434A80D1DA21D1 // c7
	.quad	0xBFE72B53B1AED992 // B' = pi/2 - B (high double)
	.quad	0xBC88452B3716C4D6 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FD003B1DF5A0F86 // c0 (high double)
	.quad	0x3C716CEB71FB716B // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBF155566FA6884 // c1 (low double)
	.quad	0x3FAD530628A37C7A // c2
	.quad	0x3FA23F8F96E80A99 // c3
	.quad	0x3F86B82113BDC04E // c4
	.quad	0x3F75EF0B9C9DF1C6 // c5
	.quad	0x3F60613421CB6A80 // c6
	.quad	0x3F48E48E906FE806 // c7
	.quad	0xBFE78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0x3C7EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FD04E83F7474877 // c0 (high double)
	.quad	0x3C73B117AC20AC65 // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBF7284F47C0E61 // c1 (low double)
	.quad	0x3FAE0056458FCED5 // c2
	.quad	0x3FA28800992AEEE6 // c3
	.quad	0x3F8766CB6439B11D // c4
	.quad	0x3F768C54DBB36839 // c5
	.quad	0x3F610297B13942C6 // c6
	.quad	0x3F5536BD1C4BF79E // c7
	.quad	0xBFE7F4638C50FAFB // B' = pi/2 - B (high double)
	.quad	0xBC71A4B8D15ECF6D // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FD099A0163BCD3F // c0 (high double)
	.quad	0x3C7A7F11D963D72E // c0 (low double)
	.quad	0x3FD0000000000000 // c1 (high single bit)
	.quad	0x3FBFD1D937A1F094 // c1 (low double)
	.quad	0x3FAEB05B52EFB4C0 // c2
	.quad	0x3FA2D29E032A57DB // c3
	.quad	0x3F881A60F7D38723 // c4
	.quad	0x3F772FBBF1BFA28B // c5
	.quad	0x3F61AA745B27BB01 // c6
	.quad	0x3F51CC19136F91C1 // c7
	.quad	0xBFE858EB79A20BB0 // B' = pi/2 - B (high double)
	.quad	0x3C8EE70AFE8446D9 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FD0E507EE8B93D7 // c0 (high double)
	.quad	0x3C679F09CDD5D5B2 // c0 (low double)
	.quad	0x3FE0000000000000 // c1 (high single bit)
	.quad	0xBFBFCCA52E2AE201 // c1 (low double)
	.quad	0x3FAF632A1681FD0B // c2
	.quad	0x3FA31F7794BDF922 // c3
	.quad	0x3F88D31259E30525 // c4
	.quad	0x3F77D97B728544F5 // c5
	.quad	0x3F62591A3A2B699F // c6
	.quad	0x3F3605B387A5D7A0 // c7
	.quad	0xBFE8BD7366F31C64 // B' = pi/2 - B (high double)
	.quad	0x3C6A81C996DFD5A5 // B' = pi/2 - B (low double)
	.quad	0x3FF0000000000000 // tau (1 for cot path)
	.quad	0x3FD130BD396BB4A9 // c0 (high double)
	.quad	0xBC657D8592EA09D4 // c0 (low double)
	.quad	0x3FE0000000000000 // c1 (high single bit)
	.quad	0xBFBF68ED595308D6 // c1 (low double)
	.quad	0x3FB00C6BF6A673E6 // c2
	.quad	0x3FA36E9DA9556AA6 // c3
	.quad	0x3F8991120C7D4BB9 // c4
	.quad	0x3F7889ABD6008215 // c5
	.quad	0x3F630EE404688883 // c6
	.quad	0x3F52CAD75A2201AF // c7
	.quad	0xBFE921FB54442D18 // B' = pi/2 - B (high double)
	.quad	0xBC81A62633145C07 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFF0000000000000 // c0 (high double)
	.quad	0x3C0492B176DF0500 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0x0000000000000000 // c1 (low double)
	.quad	0xC00000000000010A // c2
	.quad	0x4005555555555614 // c3
	.quad	0xC00AAAAAA890EF4D // c4
	.quad	0x4011111110018C73 // c5
	.quad	0xC015B108FB920A0B // c6
	.quad	0x401BA285D6669571 // c7
	.quad	0xBFE9868341953DCD // B' = pi/2 - B (high double)
	.quad	0x3C861341341F5289 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEF395D9F0E3C92 // c0 (high double)
	.quad	0x3C550BEC65726E28 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFA8873C53BE0540 // c1 (low double)
	.quad	0xBFFE79E5C65202DE // c2
	.quad	0x400412FD76797C7D // c3
	.quad	0xC008AAB30DC82D37 // c4
	.quad	0x400F1B6C68B4AEA4 // c5
	.quad	0xC013758CE9BD9EFA // c6
	.quad	0x40186872A0685915 // c7
	.quad	0xBFE9EB0B2EE64E81 // B' = pi/2 - B (high double)
	.quad	0xBC519ABB2567F739 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEE776EAFC91706 // c0 (high double)
	.quad	0xBC890664EDC53F33 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFB7F295AD658764 // c1 (low double)
	.quad	0xBFFD0AA1DB7F5E58 // c2
	.quad	0x4002E8A6F9693556 // c3
	.quad	0xC006D7CA16B298C6 // c4
	.quad	0x400C63DFCAA3FB6C // c5
	.quad	0xC0117C3EE50D7742 // c6
	.quad	0x401599932C96B9B6 // c7
	.quad	0xBFEA4F931C375F35 // B' = pi/2 - B (high double)
	.quad	0xBC8A79EFFD795057 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEDB9FA89953FCF // c0 (high double)
	.quad	0xBC286C68F514A640 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFC18AC418849A6E // c1 (low double)
	.quad	0xBFFBB083E02786D7 // c2
	.quad	0x4001D438F8A20402 // c3
	.quad	0xC0052D5D6FF7A4BD // c4
	.quad	0x4009F262067BAA5A // c5
	.quad	0xC00F793DA8608BED // c6
	.quad	0x401328BFC5F60A29 // c7
	.quad	0xBFEAB41B09886FEA // B' = pi/2 - B (high double)
	.quad	0x3C7A7EEED374BC71 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFED00CBC7384D2E // c0 (high double)
	.quad	0xBC78C4FC635B4060 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFC6DA3A87611EA6 // c1 (low double)
	.quad	0xBFFA6A00F50D3A6F // c2
	.quad	0x4000D3D0A6ABBD46 // c3
	.quad	0xC003A760672280D0 // c4
	.quad	0x4007BF026C103550 // c5
	.quad	0xC00C5EBD23F4E2ED // c6
	.quad	0x4011024DB786758B // c7
	.quad	0xBFEB18A2F6D9809E // B' = pi/2 - B (high double)
	.quad	0xBC760E425E23E66F // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEC4BB009E77983 // c0 (high double)
	.quad	0x3C5346B6895BC358 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFCBEB63A6D04570 // c1 (low double)
	.quad	0xBFF935AFFA8DCC1D // c2
	.quad	0x3FFFCB7657D15F0B // c3
	.quad	0xC002423AFCE75F54 // c4
	.quad	0x4005C2CF61AD4B89 // c5
	.quad	0xC0099C675A298F5C // c6
	.quad	0x400E49F38B657F96 // c7
	.quad	0xBFEB7D2AE42A9153 // B' = pi/2 - B (high double)
	.quad	0x3C8CB2463821BB58 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEB9A77C18C1AF2 // c0 (high double)
	.quad	0xBC76EA194A3A10DE // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD060D88DF903DF // c1 (low double)
	.quad	0xBFF812463C417D93 // c2
	.quad	0x3FFE10E093E6DE8D // c3
	.quad	0xC000FABB50DD022E // c4
	.quad	0x4003F7B45B053F77 // c5
	.quad	0xC007275FB67DBBDD // c6
	.quad	0x400B02CB76CD2071 // c7
	.quad	0xBFEBE1B2D17BA207 // B' = pi/2 - B (high double)
	.quad	0x3C61AEB67D55A7A1 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEAECF5F9BA35A6 // c0 (high double)
	.quad	0xBC45350833FF1F00 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD2B02B2D565D55 // c1 (low double)
	.quad	0xBFF6FE947B408BAC // c2
	.quad	0x3FFC751B52A83938 // c3
	.quad	0xBFFF9C12181CFBBA // c4
	.quad	0x4002585B4421C074 // c5
	.quad	0xC004F6412119DA02 // c6
	.quad	0x400814BACED70A9A // c7
	.quad	0xBFEC463ABECCB2BB // B' = pi/2 - B (high double)
	.quad	0xBC83DAEAF976E788 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFEA43002AE42850 // c0 (high double)
	.quad	0x3C7A7C0406DA50C8 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD4E526E33345B8 // c1 (low double)
	.quad	0xBFF5F9844B626284 // c2
	.quad	0x3FFAF5A574C2F3BF // c3
	.quad	0xBFFD7334FD198D13 // c4
	.quad	0x4000E01251879DC7 // c5
	.quad	0xC00300E6F5213CD0 // c6
	.quad	0x40058BA67D3A0565 // c7
	.quad	0xBFECAAC2AC1DC370 // B' = pi/2 - B (high double)
	.quad	0x3C83DE7C6DBCC708 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE99C6E0F634394 // c0 (high double)
	.quad	0xBC81A125C4B050B6 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD7012EAAC8A987 // c1 (low double)
	.quad	0xBFF50215B95FD940 // c2
	.quad	0x3FF9903A2B193F49 // c3
	.quad	0xBFFB7656607D37C1 // c4
	.quad	0x3FFF156F461C0EDF // c5
	.quad	0xC001403E48F27428 // c6
	.quad	0x40034ACD8D387876 // c7
	.quad	0xBFED0F4A996ED424 // B' = pi/2 - B (high double)
	.quad	0xBC61A070AC3E29A0 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE8F9197BF85EEB // c0 (high double)
	.quad	0x3C77971D70636FAC // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFD9058D62BBB98D // c1 (low double)
	.quad	0xBFF4175D31199B0C // c2
	.quad	0x3FF842CA948070CE // c3
	.quad	0xBFF9A1675E6F2233 // c4
	.quad	0x3FFCA94A75D1DF36 // c5
	.quad	0xBFFF5C3EE98CB792 // c6
	.quad	0x4001549BBD776205 // c7
	.quad	0xBFED73D286BFE4D8 // B' = pi/2 - B (high double)
	.quad	0xBC8CAEB4C3DBDBD8 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE858DE3B716571 // c0 (high double)
	.quad	0xBC8C6340D74E9430 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFDAF377B50F3DE8 // c1 (low double)
	.quad	0xBFF338819C6A585B // c2
	.quad	0x3FF70B781BB3F9F8 // c3
	.quad	0xBFF7F0C4DC952BEE // c4
	.quad	0x3FFA75444794B0BE // c5
	.quad	0xBFFC8A58AEEA1540 // c6
	.quad	0x3FFF1509C3EC4766 // c7
	.quad	0xBFEDD85A7410F58D // B' = pi/2 - B (high double)
	.quad	0x3C76156546AFA570 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE7BB99ED2990CF // c0 (high double)
	.quad	0x3C8AABB12893C19F // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFDCCC0DD257FE9F // c1 (low double)
	.quad	0xBFF264BAB3E8C3FC // c2
	.quad	0x3FF5E88F7CC1F7BC // c3
	.quad	0xBFF6612B1AE228E1 // c4
	.quad	0x3FF873A64164BF01 // c5
	.quad	0xBFFA016987C44E3B // c6
	.quad	0x3FFBFCA608E3ABD2 // c7
	.quad	0xBFEE3CE261620641 // B' = pi/2 - B (high double)
	.quad	0xBC7A77CBEAE8FD71 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE7212BE621BE6D // c0 (high double)
	.quad	0xBC8AD14CAC5A8A36 // c0 (low double)
	.quad	0x4000000000000000 // c1 (high single bit)
	.quad	0xBFDE905D04D13F70 // c1 (low double)
	.quad	0xBFF19B4F7BD63206 // c2
	.quad	0x3FF4D8845C8EEC38 // c3
	.quad	0xBFF4EFAAD70924AA // c4
	.quad	0x3FF69F610EDFA787 // c5
	.quad	0xBFF7B93C8525967B // c6
	.quad	0x3FF928F491396159 // c7
	.quad	0xBFEEA16A4EB316F6 // B' = pi/2 - B (high double)
	.quad	0x3C8A7D8171BF2FD8 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE6897514751DB6 // c0 (high double)
	.quad	0xBC7432E085692DD0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDFBE9EE099229D // c1 (low double)
	.quad	0xBFF0DB94E83152C9 // c2
	.quad	0x3FF3D9ED602549D9 // c3
	.quad	0xBFF3999FCB35B63C // c4
	.quad	0x3FF4F3F4BB18D34D // c5
	.quad	0xBFF5AA9FE2712A98 // c6
	.quad	0x3FF6B61F156D15A2 // c7
	.quad	0xBFEF05F23C0427AA // B' = pi/2 - B (high double)
	.quad	0x3C51B746C796F33C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE5F457E4F4812E // c0 (high double)
	.quad	0xBC8F6EE06A43AD15 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDE1FFA33ABCB2D // c1 (low double)
	.quad	0xBFF024ECA3857FDF // c2
	.quad	0x3FF2EB80B3E2C969 // c3
	.quad	0xBFF25CA858ECE33E // c4
	.quad	0x3FF36D6159DB2BC3 // c5
	.quad	0xBFF3CF427530AFAE // c6
	.quad	0x3FF4914D16BF30E1 // c7
	.quad	0xBFEF6A7A2955385E // B' = pi/2 - B (high double)
	.quad	0xBC860FAFBFD97309 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE561B82AB7F990 // c0 (high double)
	.quad	0xBC87A5FA3E37C2A5 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDC92D84170BCC6 // c1 (low double)
	.quad	0xBFEEED87E934644C // c2
	.quad	0x3FF20C10F4D7D072 // c3
	.quad	0xBFF1369E37DA5741 // c4
	.quad	0x3FF20816F61AB3EE // c5
	.quad	0xBFF221953D8712FA // c6
	.quad	0x3FF29CA76CD56201 // c7
	.quad	0xBFEFCF0216A64913 // B' = pi/2 - B (high double)
	.quad	0x3C81A9B7A75A3B87 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE4D17B087B265D // c0 (high double)
	.quad	0x3C788AAC4AE25B54 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FDB166A3723839C // c1 (low double)
	.quad	0xBFEDA1257F3D5E87 // c2
	.quad	0x3FF13A8A70704F6B // c3
	.quad	0xBFF025900612C493 // c4
	.quad	0x3FF0C0E778680DE8 // c5
	.quad	0xBFF09CB2B02DFCD5 // c6
	.quad	0x3FF0D681147681E0 // c7
	.quad	0xBFF019C501FBACE4 // B' = pi/2 - B (high double)
	.quad	0x3C9CB18F8746F50C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE44386DB9CE5DB // c0 (high double)
	.quad	0x3C8A7F79AD345EDE // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD9A9EE22E82AF2 // c1 (low double)
	.quad	0xBFEC63B53FACFD1B // c2
	.quad	0x3FF075F0B010F1C9 // c3
	.quad	0xBFEE4F7738A37AC1 // c4
	.quad	0x3FEF29F7068A6E01 // c5
	.quad	0xBFEE78912F3E48A3 // c6
	.quad	0x3FEE7DE5AD1A6D54 // c7
	.quad	0xBFF04C08F8A4353E // B' = pi/2 - B (high double)
	.quad	0x3C908E433AE0CC54 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE3B7C3289ED6F3 // c0 (high double)
	.quad	0x3C8C4CF3D3282C13 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD84CAE0A06DE7C // c1 (low double)
	.quad	0xBFEB344C2E4C0F47 // c2
	.quad	0x3FEF7AB88F5F820C // c3
	.quad	0xBFEC771219A99E98 // c4
	.quad	0x3FED0390B144EACA // c5
	.quad	0xBFEBF90A32A17514 // c6
	.quad	0x3FEBAC50A2919B8C // c7
	.quad	0xBFF07E4CEF4CBD98 // B' = pi/2 - B (high double)
	.quad	0x3C71ABDBB9EA8E6E // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE32E1889047FFD // c0 (high double)
	.quad	0x3C810D08F013759C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD6FDFF12A4297B // c1 (low double)
	.quad	0xBFEA120FA25B9D83 // c2
	.quad	0x3FEE1FF1BF27E6AB // c3
	.quad	0xBFEABF0C6E045EA0 // c4
	.quad	0x3FEB0A0E2CDCAB88 // c5
	.quad	0xBFE9B40D41660E9D // c6
	.quad	0x3FE92FCC1848D94B // c7
	.quad	0xBFF0B090E5F545F2 // B' = pi/2 - B (high double)
	.quad	0xBC7EE15577AE1472 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE2A6709A74F289 // c0 (high double)
	.quad	0xBC76E3B98C266662 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD5BD40BF249496 // c1 (low double)
	.quad	0xBFE8FC33EFCF6F3A // c2
	.quad	0x3FECDA06E4CFC822 // c3
	.quad	0xBFE924C5BA1C7515 // c4
	.quad	0x3FE9395A86CEDBCB // c5
	.quad	0xBFE7A39B6931C7D4 // c6
	.quad	0x3FE6D875BED7A2F5 // c7
	.quad	0xBFF0E2D4DC9DCE4C // B' = pi/2 - B (high double)
	.quad	0xBC93DBA1AA51ADD5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE220B5EF047825 // c0 (high double)
	.quad	0xBC5AE1F6C712B338 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD489DC399ADDEC // c1 (low double)
	.quad	0xBFE7F1FB2FEBE8C6 // c2
	.quad	0x3FEBA791734D6CCE // c3
	.quad	0xBFE7A5DA56BAB9DA // c4
	.quad	0x3FE78DC9A17C17D7 // c5
	.quad	0xBFE5C262725357AE // c6
	.quad	0x3FE4D1FE58052A5F // c7
	.quad	0xBFF11518D34656A6 // B' = pi/2 - B (high double)
	.quad	0xBC9FFEEDF6B7D68D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE19CD3FE8E405D // c0 (high double)
	.quad	0x3C84E5C1D4831B21 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD36343ADCAF866 // c1 (low double)
	.quad	0xBFE6F2B4260FF6A8 // c2
	.quad	0x3FEA8747864942E1 // c3
	.quad	0xBFE6401D47362936 // c4
	.quad	0x3FE6040F50A52FBE // c5
	.quad	0xBFE40BA6192CC675 // c6
	.quad	0x3FE302E7DABA2D8E // c7
	.quad	0xBFF1475CC9EEDF01 // B' = pi/2 - B (high double)
	.quad	0x3C93DDC5BCE200BB // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE11AB7190834EC // c0 (high double)
	.quad	0x3C8E577A07DB8B3E // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD248F1B07217E3 // c1 (low double)
	.quad	0xBFE5FDB93DD01D06 // c2
	.quad	0x3FE977F956462152 // c3
	.quad	0xBFE4F192BE91E40A // c4
	.quad	0x3FE49932EEB2A51C // c5
	.quad	0xBFE27B305435DE25 // c6
	.quad	0x3FE176C2BDD3411E // c7
	.quad	0xBFF179A0C097675B // B' = pi/2 - B (high double)
	.quad	0x3C7EE9E5C1EF600D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE09A4C59BD0D4D // c0 (high double)
	.quad	0x3C451D94A7B87B10 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD13A68B2A5E855 // c1 (low double)
	.quad	0xBFE5126F9FD8D89F // c2
	.quad	0x3FE8788EEDC53F19 // c3
	.quad	0xBFE3B86B3E883B76 // c4
	.quad	0x3FE34A881E7F09CA // c5
	.quad	0xBFE10D3EF07D1073 // c6
	.quad	0x3FDFEBFE6A9FE5DD // c7
	.quad	0xBFF1ABE4B73FEFB5 // B' = pi/2 - B (high double)
	.quad	0xBC71A34B6FA942D3 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFE01B819B5A7CF7 // c0 (high double)
	.quad	0xBC80BED996FB9ED6 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FD03732803B2A4C // c1 (low double)
	.quad	0xBFE430465B55C6C0 // c2
	.quad	0x3FE78806185ACF72 // c3
	.quad	0xBFE292FF3DF61A34 // c4
	.quad	0x3FE215A449967BC2 // c5
	.quad	0xBFDF7CEF2946D3BA // c6
	.quad	0x3FDD0F4FF3CD4465 // c7
	.quad	0xBFF1DE28ADE8780F // B' = pi/2 - B (high double)
	.quad	0xBC908C1F2850796D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFDF3C8AD985D9EE // c0 (high double)
	.quad	0xBC455C31F533FF68 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FCE7DBF907D8618 // c1 (low double)
	.quad	0xBFE356B5A1D9CA5A // c2
	.quad	0x3FE6A57085697F99 // c3
	.quad	0xBFE17FCB46BE280B // c4
	.quad	0x3FE0F858275BAA48 // c5
	.quad	0xBFDD17B47CA8A84B // c6
	.quad	0x3FDA9B369A707A6B // c7
	.quad	0xBFF2106CA4910069 // B' = pi/2 - B (high double)
	.quad	0xBC9CAF6B74B6A225 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFDE450E0D273E7A // c0 (high double)
	.quad	0xBC74138BBC4225C5 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FCCA20F5D4879EA // c1 (low double)
	.quad	0xBFE2853E13FBB7C7 // c2
	.quad	0x3FE5CFF21905E477 // c3
	.quad	0xBFE07D6C7D0DAC66 // c4
	.quad	0x3FDFE1550F550217 // c5
	.quad	0xBFDAE570CE7F6BE5 // c6
	.quad	0x3FD89328A3AB7677 // c7
	.quad	0xBFF242B09B3988C4 // B' = pi/2 - B (high double)
	.quad	0x3C972D483EE33523 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFDD506C82A2C800 // c0 (high double)
	.quad	0xBC6E169413DFE956 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FCADA8ECCC42C48 // c1 (low double)
	.quad	0xBFE1BB681D0BE8A7 // c2
	.quad	0x3FE506BF662527D3 // c3
	.quad	0xBFDF153B07CB2306 // c4
	.quad	0x3FDDF9A636394DCA // c5
	.quad	0xBFD8E153A4A7867F // c6
	.quad	0x3FD6937B37976C23 // c7
	.quad	0xBFF274F491E2111E // B' = pi/2 - B (high double)
	.quad	0x3C8613F7E4FA18D6 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFDC5E87185E67B6 // c0 (high double)
	.quad	0xBC4FEBA1F148ADA8 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC92683A68EB5D0 // c1 (low double)
	.quad	0xBFE0F8C35C7247C7 // c2
	.quad	0x3FE4491C4DCA262E // c3
	.quad	0xBFDD4C67683395D6 // c4
	.quad	0x3FDC3667C15EBF92 // c5
	.quad	0xBFD7070B7FE443D3 // c6
	.quad	0x3FD4805841790B6E // c7
	.quad	0xBFF2A738888A9978 // B' = pi/2 - B (high double)
	.quad	0xBC5195059E91C4D2 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFDB6F3FC8C61E5B // c0 (high double)
	.quad	0x3C6A1CC48268287A // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC7853E65CE5524 // c1 (low double)
	.quad	0xBFE03CE61B6A9E63 // c2
	.quad	0x3FE3965ABF5C8FAB // c3
	.quad	0xBFDB9E3938F261E9 // c4
	.quad	0x3FDA94A771ED6FE5 // c5
	.quad	0xBFD552B7D4636646 // c6
	.quad	0x3FD2F58F20A0FA49 // c7
	.quad	0xBFF2D97C7F3321D2 // B' = pi/2 - B (high double)
	.quad	0xBC8A79394C9E8A0A // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFDA827999FCEF32 // c0 (high double)
	.quad	0xBC708A11019E41CE // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC5F619980C4336 // c1 (low double)
	.quad	0xBFDF0ED99BED9B65 // c2
	.quad	0x3FE2EDD996C1BBEB // c3
	.quad	0xBFDA08B785C6EA59 // c4
	.quad	0x3FD911BC7B12902F // c5
	.quad	0xBFD3C0DA5B1130C3 // c6
	.quad	0x3FD1B590151D6334 // c7
	.quad	0xBFF30BC075DBAA2C // B' = pi/2 - B (high double)
	.quad	0xBC995FE8F2B56DBD // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD998188E816BF0 // c0 (high double)
	.quad	0xBC73DB2BD13E0A0C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC478794806DE66 // c1 (low double)
	.quad	0xBFDDAFF33BF96274 // c2
	.quad	0x3FE24F039543C659 // c3
	.quad	0xBFD88A113E071D2D // c4
	.quad	0x3FD7AB3E661E65F9 // c5
	.quad	0xBFD24E4E1B8DC6CD // c6
	.quad	0x3FD01ED5FCAC7FDD // c7
	.quad	0xBFF33E046C843287 // B' = pi/2 - B (high double)
	.quad	0x3C9A7CCAC0E4698B // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD8B00196B3D022 // c0 (high double)
	.quad	0x3C67341EE30455A6 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC30BCA73878656 // c1 (low double)
	.quad	0xBFDC5C67FF403B73 // c2
	.quad	0x3FE1B94E72981059 // c3
	.quad	0xBFD7209990BAE1F6 // c4
	.quad	0x3FD65EF72AC3A4F8 // c5
	.quad	0xBFD0F83D8377C34B // c6
	.quad	0x3FCD97555AF6144C // c7
	.quad	0xBFF37048632CBAE1 // B' = pi/2 - B (high double)
	.quad	0x3C8CB2FCE8FC81A5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD7CA1A832D0F84 // c0 (high double)
	.quad	0x3C7EE5A712091434 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC1AF828B5ED674 // c1 (low double)
	.quad	0xBFDB13909C0269A2 // c2
	.quad	0x3FE12C3A0394B7FA // c3
	.quad	0xBFD5CAC4A421216C // c4
	.quad	0x3FD52AE625505252 // c5
	.quad	0xBFCF7831018A616A // c6
	.quad	0x3FCB98AB42F62940 // c7
	.quad	0xBFF3A28C59D5433B // B' = pi/2 - B (high double)
	.quad	0x3C61B19140C0C0D5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD6E649F7D78649 // c0 (high double)
	.quad	0xBC678D2AC0322984 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FC0631EFCBE0D16 // c1 (low double)
	.quad	0xBFD9D4CEDF38DFCD // c2
	.quad	0x3FE0A74F7488C3A6 // c3
	.quad	0xBFD487249EA3BD6C // c4
	.quad	0x3FD40D3A9BDEFA27 // c5
	.quad	0xBFCD2F1CBD1B58CD // c6
	.quad	0x3FC98CBDCEBDAD19 // c7
	.quad	0xBFF3D4D0507DCB95 // B' = pi/2 - B (high double)
	.quad	0xBC83DA34489C213B // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD604775FBB27DF // c0 (high double)
	.quad	0xBC6AC647C9CF0A4C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FBE4C49866D2D78 // c1 (low double)
	.quad	0xBFD89F8D033946D5 // c2
	.quad	0x3FE02A20954EE23A // c3
	.quad	0xBFD35466F88C5ABC // c4
	.quad	0x3FD3044BAC5DC8C6 // c5
	.quad	0xBFCB110C3D6FEB70 // c6
	.quad	0x3FC7761A8DC12688 // c7
	.quad	0xBFF40714472653EF // B' = pi/2 - B (high double)
	.quad	0xBC96106670B43955 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD5248AE1701B17 // c0 (high double)
	.quad	0xBC7BE8CC944DAFEA // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FBBF04005723CCC // c1 (low double)
	.quad	0xBFD7733D1300258A // c2
	.quad	0x3FDF688E6ACC7DF5 // c3
	.quad	0xBFD231520EA793C8 // c4
	.quad	0x3FD20E93C5183A34 // c5
	.quad	0xBFC91A331D55B3F7 // c6
	.quad	0x3FC651721E1371DD // c7
	.quad	0xBFF439583DCEDC4A // B' = pi/2 - B (high double)
	.quad	0x3C9DCC4D42E59DF2 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD4466D542BAC92 // c0 (high double)
	.quad	0x3C705073A04E444A // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB9B14751F8BA20 // c1 (low double)
	.quad	0xBFD64F58590170A2 // c2
	.quad	0x3FDE8AC91D15CFCF // c3
	.quad	0xBFD11CC2EF09C664 // c4
	.quad	0x3FD12AB5AB65B965 // c5
	.quad	0xBFC7471C9AC81567 // c6
	.quad	0x3FC4826BA97C3E69 // c7
	.quad	0xBFF46B9C347764A4 // B' = pi/2 - B (high double)
	.quad	0x3C91A900F67F753A // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD36A08355C63DC // c0 (high double)
	.quad	0x3C6D7145B6A10D86 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB78E922B82AB64 // c1 (low double)
	.quad	0xBFD5335ED87AB0D5 // c2
	.quad	0x3FDDBA417913D89C // c3
	.quad	0xBFD015AB59BD876B // c4
	.quad	0x3FD0576D1EF5DB85 // c5
	.quad	0xBFC594A6DD2CA17D // c6
	.quad	0x3FC2F51FBC1DE653 // c7
	.quad	0xBFF49DE02B1FECFE // B' = pi/2 - B (high double)
	.quad	0x3C7616D2A8653209 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD28F459ECAD74D // c0 (high double)
	.quad	0xBC302C86D50E46C0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB5875F92EDBDF4 // c1 (low double)
	.quad	0xBFD41ED6D05A0010 // c2
	.quad	0x3FDCF65481FC5BEB // c3
	.quad	0xBFCE361FE028368D // c4
	.quad	0x3FCF2728B941710B // c5
	.quad	0xBFC3FFF674960878 // c6
	.quad	0x3FC1A12109F12125 // c7
	.quad	0xBFF4D02421C87558 // B' = pi/2 - B (high double)
	.quad	0xBC7A765E893370D7 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD1B6103D3597E9 // c0 (high double)
	.quad	0xBC6CBCB5FED7AC2C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB39AFA3392E164 // c1 (low double)
	.quad	0xBFD3114C46E2AEF3 // c2
	.quad	0x3FDC3E6B23EF73B5 // c3
	.quad	0xBFCC580D1B42EE7F // c4
	.quad	0x3FCDBC3D6E14D202 // c5
	.quad	0xBFC28671B66B7EC4 // c6
	.quad	0x3FC0BFBCFE62E275 // c7
	.quad	0xBFF502681870FDB2 // B' = pi/2 - B (high double)
	.quad	0xBC92C0E3EEB304EE // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD0DE53475F3B3C // c0 (high double)
	.quad	0xBC67B2CC0A839654 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB1C8B7D76C7EE0 // c1 (low double)
	.quad	0xBFD20A509D4BB6C1 // c2
	.quad	0x3FDB91F96510DF38 // c3
	.quad	0xBFCA8F698D62832E // c4
	.quad	0x3FCC6C3408291325 // c5
	.quad	0xBFC125B6D8F69817 // c6
	.quad	0x3FBE80E8F3B35090 // c7
	.quad	0xBFF534AC0F19860C // B' = pi/2 - B (high double)
	.quad	0xBC9EE4303B192DA6 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFD007FA758626AE // c0 (high double)
	.quad	0xBC65A5EB9CA4F97A // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FB00FF8E583BEBC // c1 (low double)
	.quad	0xBFD1097A2AA570F5 // c2
	.quad	0x3FDAF07DA8CAEBA7 // c3
	.quad	0xBFC8DA9D107525AD // c4
	.quad	0x3FCB3545F2ADC1DB // c5
	.quad	0xBFBFB739DB46589A // c6
	.quad	0x3FBDA2C2374E7733 // c7
	.quad	0xBFF566F005C20E67 // B' = pi/2 - B (high double)
	.quad	0x3C94F8837880A9A2 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFCE65E3F27C9F2A // c0 (high double)
	.quad	0xBC6F62A8B8B3F2A2 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FACE04FD3EC1C90 // c1 (low double)
	.quad	0xBFD00E63DD639A45 // c2
	.quad	0x3FDA598003751442 // c3
	.quad	0xBFC73829AC409F93 // c4
	.quad	0x3FCA15E3A26F9D69 // c5
	.quad	0xBFBD4C4574E0CB3E // c6
	.quad	0x3FBB9C588182E205 // c7
	.quad	0xBFF59933FC6A96C1 // B' = pi/2 - B (high double)
	.quad	0x3C81AA6E583501D4 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFCCBE4CEB4B4CF2 // c0 (high double)
	.quad	0xBC64A514F790D588 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA9D1724DE6C940 // c1 (low double)
	.quad	0xBFCE3159C5ED3A4E // c2
	.quad	0x3FD9CC919D4271B2 // c3
	.quad	0xBFC5A6A9489AA7F4 // c4
	.quad	0x3FC90C961E2B49F9 // c5
	.quad	0xBFBB06EE7CFA369E // c6
	.quad	0x3FB9E9447AAF378D // c7
	.quad	0xBFF5CB77F3131F1B // B' = pi/2 - B (high double)
	.quad	0xBC6A70A9025D3E70 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFCB1909EFD8B762 // c0 (high double)
	.quad	0xBC5843049E1318BC // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA6F2545C478E00 // c1 (low double)
	.quad	0xBFCC4FF0A9D74F70 // c2
	.quad	0x3FD9494C22EF2A54 // c3
	.quad	0xBFC424CB909A6722 // c4
	.quad	0x3FC8180A481D593B // c5
	.quad	0xBFB8E3D90C36FA86 // c6
	.quad	0x3FB8D27EAFEAC4D3 // c7
	.quad	0xBFF5FDBBE9BBA775 // B' = pi/2 - B (high double)
	.quad	0xBC8EE2C2D963A10C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFC975F5E0553158 // c0 (high double)
	.quad	0xBC2EEEA31AB9D050 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA4420303DEC550 // c1 (low double)
	.quad	0xBFCA77D9D62CA610 // c2
	.quad	0x3FD8CF5143323608 // c3
	.quad	0xBFC2B15401455507 // c4
	.quad	0x3FC7370FF096556F // c5
	.quad	0xBFB6DFEACA56EADA // c6
	.quad	0x3FB6B75DF89EC13C // c7
	.quad	0xBFF62FFFE0642FCF // B' = pi/2 - B (high double)
	.quad	0xBC9B94ADB917F93E // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFC7D4EC54FB5968 // c0 (high double)
	.quad	0xBC4FB65648F8A4A0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3FA1BF9C7C60B940 // c1 (low double)
	.quad	0xBFC8A86964A1796B // c2
	.quad	0x3FD85E4A37F23352 // c3
	.quad	0xBFC14B181F5BF6AE // c4
	.quad	0x3FC66888E3A5D3BA // c5
	.quad	0xBFB4F84D7AC0F5DB // c6
	.quad	0x3FB62BA6F07C5587 // c7
	.quad	0xBFF66243D70CB82A // B' = pi/2 - B (high double)
	.quad	0x3C984805FA81DE0A // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFC635C990CE0D36 // c0 (high double)
	.quad	0x3C59DFC0C48C2B20 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F9ED49F1F8144C0 // c1 (low double)
	.quad	0xBFC6E0F98ED00C32 // c2
	.quad	0x3FD7F5E75A2DD66E // c3
	.quad	0xBFBFE1FBA10F6B0E // c4
	.quad	0x3FC5AB7CD1905EBB // c5
	.quad	0xBFB32A57A4FAE3FF // c6
	.quad	0x3FB4BA60BA62CA68 // c7
	.quad	0xBFF69487CDB54084 // B' = pi/2 - B (high double)
	.quad	0x3C8849735C376AA4 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFC4986A74CF4E57 // c0 (high double)
	.quad	0xBC69AB0A243914D0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F9A82B60D75F5C0 // c1 (low double)
	.quad	0xBFC520EA2BA6A0AE // c2
	.quad	0x3FD795DFBFFEE39E // c3
	.quad	0xBFBD43F3A7862CC7 // c4
	.quad	0x3FC4FF04247D7006 // c5
	.quad	0xBFB1739726786F64 // c6
	.quad	0x3FB2FAC67536C2B7 // c7
	.quad	0xBFF6C6CBC45DC8DE // B' = pi/2 - B (high double)
	.quad	0x3BD6D61B58C99C43 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFC2FCAC73A60640 // c0 (high double)
	.quad	0xBC52CF95F70D438C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F96881A43B33590 // c1 (low double)
	.quad	0xBFC367A033B52F5E // c2
	.quad	0x3FD73DF0E3CC557A // c3
	.quad	0xBFBABA1CAC4F18CD // c4
	.quad	0x3FC4624C36BE941D // c5
	.quad	0xBFAFA38B2F88747D // c6
	.quad	0x3FB215B1F402C9E1 // c7
	.quad	0xBFF6F90FBB065138 // B' = pi/2 - B (high double)
	.quad	0xBC8843BDD561383D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFC1626D85A91E70 // c0 (high double)
	.quad	0x3C50084844291A00 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F92E3863C9A69E0 // c1 (low double)
	.quad	0xBFC1B4854BB2B147 // c2
	.quad	0x3FD6EDDE5416DFFB // c3
	.quad	0xBFB8429341CCA17A // c4
	.quad	0x3FC3D49AC2C9F46E // c5
	.quad	0xBFAC85843914CF63 // c6
	.quad	0x3FB1B1DC3FA41043 // c7
	.quad	0xBFF72B53B1AED992 // B' = pi/2 - B (high double)
	.quad	0xBC98452B3716C4D6 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFBF93183A8DB9E9 // c0 (high double)
	.quad	0x3C4124AC452DA622 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F8F27A3181EE680 // c1 (low double)
	.quad	0xBFC00707549D7ECC // c2
	.quad	0x3FD6A5716B500730 // c3
	.quad	0xBFB5DB89BB6B245F // c4
	.quad	0x3FC3554867F59647 // c5
	.quad	0xBFA98919BF6D1AC0 // c6
	.quad	0x3FB1AF9A18244AC2 // c7
	.quad	0xBFF75D97A85761ED // B' = pi/2 - B (high double)
	.quad	0x3C9B97887C831272 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFBC63CE377FC802 // c0 (high double)
	.quad	0x3C543F7D42D9FFC4 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F892FE02A47EFA0 // c1 (low double)
	.quad	0xBFBCBD3001C956C5 // c2
	.quad	0x3FD664790F293C47 // c3
	.quad	0xBFB38345DE7C4B0B // c4
	.quad	0x3FC2E3C4C02FFDF9 // c5
	.quad	0xBFA6AA7F0C135018 // c6
	.quad	0x3FAFE3C754ACF687 // c7
	.quad	0xBFF78FDB9EFFEA47 // B' = pi/2 - B (high double)
	.quad	0x3C8EE8786039D373 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFB936BB8C5B2DA2 // c0 (high double)
	.quad	0xBC587D23392D360C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F83DDE2A8935DE0 // c1 (low double)
	.quad	0xBFB97558DC230BA8 // c2
	.quad	0x3FD62AC976FBC1B8 // c3
	.quad	0xBFB1381EB60159F6 // c4
	.quad	0x3FC27F84438D5027 // c5
	.quad	0xBFA3E63B3A462D11 // c6
	.quad	0x3FAF0AE614B60705 // c7
	.quad	0xBFF7C21F95A872A1 // B' = pi/2 - B (high double)
	.quad	0x3C6A877F1DB6080C // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFB60B9F7597FDEC // c0 (high double)
	.quad	0xBC3D1E3FBF349CC0 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F7E5FFEF4D8DFC0 // c1 (low double)
	.quad	0xBFB6357984E124B6 // c2
	.quad	0x3FD5F83BF8B56900 // c3
	.quad	0xBFADF0F50CAA06C2 // c4
	.quad	0x3FC228147690425D // c5
	.quad	0xBFA138F0F640FF12 // c6
	.quad	0x3FAF8D1BCDD067BD // c7
	.quad	0xBFF7F4638C50FAFB // B' = pi/2 - B (high double)
	.quad	0xBC81A4B8D15ECF6D // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFB2E239CCFF3831 // c0 (high double)
	.quad	0xBC5879ED3FF4D06C // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F764980AEEE3740 // c1 (low double)
	.quad	0xBFB2FC879C81123C // c2
	.quad	0x3FD5CCAEDC1DC96E // c3
	.quad	0xBFA98599B71D86B1 // c4
	.quad	0x3FC1DD150D55E5CE // c5
	.quad	0xBF9D3EB9450DBD20 // c6
	.quad	0x3FADD6CFBE9A9DDE // c7
	.quad	0xBFF826A782F98355 // B' = pi/2 - B (high double)
	.quad	0xBC94F5A8B515906F // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFAF7495EA3F3783 // c0 (high double)
	.quad	0x3C407F10D50F8AC9 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F6EEB8B377B1A00 // c1 (low double)
	.quad	0xBFAF92FABFD0B7B3 // c2
	.quad	0x3FD5A805341437FF // c3
	.quad	0xBFA52B295E2DCF92 // c4
	.quad	0x3FC19E2C59B8C033 // c5
	.quad	0xBF982CF88466F013 // c6
	.quad	0x3FAD27509D6C3D7C // c7
	.quad	0xBFF858EB79A20BB0 // B' = pi/2 - B (high double)
	.quad	0x3C9EE70AFE8446D9 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFA927278A3B1162 // c0 (high double)
	.quad	0xBBFB01BD7F1D4620 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F63C55DB08E1680 // c1 (low double)
	.quad	0xBFA936B1EC652161 // c2
	.quad	0x3FD58A26BD6CCCCD // c3
	.quad	0xBFA0DEB5445B4095 // c4
	.quad	0x3FC16B146127E0E0 // c5
	.quad	0xBF9336A9836851D5 // c6
	.quad	0x3FAC19302F7F3ACC // c7
	.quad	0xBFF88B2F704A940A // B' = pi/2 - B (high double)
	.quad	0x3C92C3BEB21E1E21 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBFA2DBAAE9A05DB0 // c0 (high double)
	.quad	0x3C4371F88E195029 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F563A086B860B00 // c1 (low double)
	.quad	0xBFA2E23785EEB488 // c2
	.quad	0x3FD572FFC353B741 // c3
	.quad	0xBF993ABD2276BF58 // c4
	.quad	0x3FC143926088643D // c5
	.quad	0xBF8CAC359DDE7BCE // c6
	.quad	0x3FAB56F9A2A6CD28 // c7
	.quad	0xBFF8BD7366F31C64 // B' = pi/2 - B (high double)
	.quad	0x3C7A81C996DFD5A5 // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBF992346247A91F0 // c0 (high double)
	.quad	0x3BF126D920C63160 // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F43BF447B091000 // c1 (low double)
	.quad	0xBF992726F0F98D7F // c2
	.quad	0x3FD5628108EFA55C // c3
	.quad	0xBF90C8A60639C114 // c4
	.quad	0x3FC127789EC0BD38 // c5
	.quad	0xBF830BA6F46C58D9 // c6
	.quad	0x3FABBEE2C1E62B03 // c7
	.quad	0xBFF8EFB75D9BA4BE // B' = pi/2 - B (high double)
	.quad	0xBC760B679AB8CD3B // B' = pi/2 - B (low double)
	.quad	0x0000000000000000 // tau (1 for cot path)
	.quad	0xBF89224E047E368E // c0 (high double)
	.quad	0x3C1A268EB3BB9B7A // c0 (low double)
	.quad	0x3FF0000000000000 // c1 (high single bit)
	.quad	0x3F23BDBEAD8CF800 // c1 (low double)
	.quad	0xBF8923461AE96D62 // c2
	.quad	0x3FD5589FB822D5EE // c3
	.quad	0xBF80C326DCDCFCD2 // c4
	.quad	0x3FC116A999125B8A // c5
	.quad	0xBF7300CC26580304 // c6
	.quad	0x3FAB7B6F1DCF22D1 // c7
	.align	32
	.quad	0x40FF400000000000, 0x40FF400000000000, 0x40FF400000000000, 0x40FF400000000000 /* dReductionRangeVal */
	.align	32
	.quad	0x3FE45F306DC9C883, 0x3FE45F306DC9C883, 0x3FE45F306DC9C883, 0x3FE45F306DC9C883 /* _dInvPi */
	.align	32
	.quad	0x3FF921FB54440000, 0x3FF921FB54440000, 0x3FF921FB54440000, 0x3FF921FB54440000 /* _dPI1 */
	.align	32
	.quad	0x3D868C234C4C0000, 0x3D868C234C4C0000, 0x3D868C234C4C0000, 0x3D868C234C4C0000 /* _dPI2 */
	.align	32
	.quad	0x3B298A2E03707345, 0x3B298A2E03707345, 0x3B298A2E03707345, 0x3B298A2E03707345 /* _dPI3 */
	// PI1, PI2, and PI3 when FMA is available
	.align	32
	.quad	0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18 /* _dPI1_FMA */
	.align	32
	.quad	0x3c91a62633145c06, 0x3c91a62633145c06, 0x3c91a62633145c06, 0x3c91a62633145c06 /* _dPI2_FMA */
	.align	32
	.quad	0x394c1cd129024e09, 0x394c1cd129024e09, 0x394c1cd129024e09, 0x394c1cd129024e09 /* _dPI3_FMA */
	.align	32
	.quad	0xbfc06b97be36d389, 0xbfc06b97be36d389, 0xbfc06b97be36d389, 0xbfc06b97be36d389 /* _dP1 */
	.align	32
	.quad	0x3f66fc6fdce5d301, 0x3f66fc6fdce5d301, 0x3f66fc6fdce5d301, 0x3f66fc6fdce5d301 /* _dP2 */
	.align	32
	.quad	0xbedf637dce2a3800, 0xbedf637dce2a3800, 0xbedf637dce2a3800, 0xbedf637dce2a3800 /* _dP3 */
	.align	32
	.quad	0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000 /* _dQ0 */
	.align	32
	.quad	0xbfdd8b213470beee, 0xbfdd8b213470beee, 0xbfdd8b213470beee, 0xbfdd8b213470beee /* _dQ1 */
	.align	32
	.quad	0x3f97e7b68ac28a56, 0x3f97e7b68ac28a56, 0x3f97e7b68ac28a56, 0x3f97e7b68ac28a56 /* _dQ2 */
	.align	32
	.quad	0xbf2b525b03bc92a6, 0xbf2b525b03bc92a6, 0xbf2b525b03bc92a6, 0xbf2b525b03bc92a6 /* _dQ3 */
	.align	32
	.type	__svml_dtan_data_internal, @object
	.size	__svml_dtan_data_internal, .-__svml_dtan_data_internal
	.align	32

#ifdef __svml_dtan_reduction_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _dPtable[2048][3][2];
} __svml_dtan_reduction_data_internal;
#endif
__svml_dtan_reduction_data_internal:
	/*            P_hi                          P_med                      P_lo         */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 0 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 1 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 2 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 3 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 4 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 5 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 6 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 7 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 8 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 9 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 10 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 11 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 12 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 13 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 14 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 15 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 16 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 17 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 18 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 19 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 20 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 21 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 22 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 23 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 24 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 25 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 26 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 27 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 28 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 29 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 30 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 31 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 32 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 33 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 34 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 35 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 36 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 37 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 38 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 39 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 40 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 41 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 42 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 43 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 44 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 45 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 46 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 47 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 48 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 49 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 50 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 51 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 52 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 53 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 54 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 55 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 56 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 57 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 58 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 59 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 60 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 61 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 62 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 63 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 64 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 65 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 66 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 67 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 68 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 69 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 70 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 71 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 72 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 73 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 74 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 75 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 76 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 77 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 78 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 79 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 80 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 81 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 82 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 83 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 84 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 85 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 86 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 87 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 88 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 89 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 90 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 91 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 92 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 93 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 94 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 95 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 96 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 97 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 98 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 99 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 100 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 101 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 102 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 103 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 104 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 105 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 106 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 107 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 108 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 109 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 110 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 111 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 112 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 113 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 114 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 115 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 116 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 117 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 118 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 119 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 120 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 121 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 122 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 123 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 124 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 125 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 126 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 127 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 128 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 129 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 130 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 131 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 132 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 133 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 134 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 135 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 136 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 137 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 138 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 139 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 140 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 141 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 142 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 143 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 144 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 145 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 146 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 147 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 148 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 149 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 150 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 151 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 152 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 153 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 154 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 155 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 156 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 157 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 158 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 159 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 160 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 161 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 162 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 163 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 164 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 165 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 166 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 167 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 168 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 169 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 170 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 171 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 172 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 173 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 174 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 175 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 176 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 177 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 178 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 179 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 180 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 181 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 182 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 183 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 184 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 185 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 186 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 187 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 188 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 189 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 190 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 191 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 192 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 193 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 194 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 195 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 196 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 197 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 198 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 199 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 200 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 201 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 202 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 203 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 204 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 205 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 206 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 207 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 208 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 209 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 210 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 211 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 212 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 213 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 214 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 215 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 216 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 217 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 218 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 219 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 220 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 221 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 222 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 223 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 224 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 225 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 226 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 227 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 228 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 229 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 230 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 231 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 232 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 233 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 234 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 235 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 236 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 237 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 238 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 239 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 240 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 241 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 242 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 243 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 244 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 245 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 246 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 247 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 248 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 249 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 250 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 251 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 252 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 253 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 254 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 255 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 256 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 257 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 258 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 259 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 260 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 261 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 262 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 263 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 264 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 265 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 266 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 267 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 268 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 269 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 270 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 271 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 272 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 273 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 274 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 275 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 276 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 277 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 278 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 279 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 280 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 281 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 282 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 283 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 284 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 285 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 286 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 287 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 288 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 289 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 290 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 291 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 292 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 293 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 294 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 295 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 296 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 297 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 298 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 299 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 300 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 301 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 302 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 303 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 304 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 305 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 306 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 307 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 308 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 309 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 310 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 311 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 312 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 313 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 314 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 315 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 316 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 317 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 318 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 319 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 320 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 321 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 322 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 323 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 324 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 325 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 326 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 327 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 328 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 329 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 330 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 331 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 332 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 333 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 334 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 335 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 336 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 337 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 338 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 339 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 340 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 341 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 342 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 343 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 344 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 345 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 346 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 347 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 348 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 349 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 350 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 351 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 352 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 353 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 354 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 355 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 356 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 357 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 358 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 359 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 360 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 361 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 362 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 363 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 364 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 365 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 366 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 367 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 368 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 369 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 370 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 371 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 372 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 373 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 374 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 375 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 376 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 377 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 378 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 379 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 380 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 381 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 382 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 383 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 384 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 385 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 386 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 387 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 388 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 389 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 390 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 391 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 392 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 393 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 394 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 395 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 396 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 397 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 398 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 399 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 400 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 401 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 402 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 403 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 404 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 405 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 406 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 407 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 408 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 409 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 410 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 411 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 412 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 413 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 414 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 415 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 416 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 417 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 418 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 419 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 420 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 421 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 422 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 423 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 424 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 425 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 426 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 427 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 428 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 429 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 430 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 431 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 432 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 433 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 434 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 435 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 436 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 437 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 438 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 439 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 440 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 441 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 442 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 443 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 444 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 445 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 446 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 447 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 448 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 449 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 450 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 451 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 452 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 453 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 454 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 455 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 456 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 457 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 458 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 459 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 460 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 461 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 462 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 463 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 464 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 465 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 466 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 467 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 468 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 469 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 470 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 471 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 472 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 473 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 474 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 475 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 476 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 477 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 478 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 479 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 480 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 481 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 482 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 483 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 484 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 485 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 486 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 487 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 488 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 489 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 490 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 491 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 492 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 493 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 494 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 495 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 496 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 497 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 498 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 499 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 500 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 501 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 502 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 503 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 504 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 505 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 506 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 507 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 508 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 509 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 510 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 511 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 512 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 513 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 514 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 515 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 516 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 517 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 518 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 519 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 520 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 521 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 522 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 523 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 524 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 525 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 526 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 527 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 528 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 529 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 530 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 531 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 532 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 533 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 534 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 535 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 536 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 537 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 538 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 539 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 540 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 541 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 542 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 543 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 544 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 545 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 546 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 547 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 548 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 549 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 550 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 551 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 552 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 553 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 554 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 555 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 556 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 557 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 558 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 559 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 560 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 561 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 562 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 563 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 564 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 565 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 566 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 567 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 568 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 569 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 570 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 571 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 572 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 573 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 574 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 575 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 576 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 577 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 578 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 579 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 580 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 581 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 582 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 583 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 584 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 585 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 586 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 587 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 588 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 589 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 590 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 591 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 592 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 593 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 594 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 595 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 596 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 597 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 598 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 599 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 600 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 601 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 602 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 603 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 604 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 605 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 606 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 607 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 608 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 609 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 610 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 611 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 612 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 613 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 614 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 615 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 616 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 617 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 618 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 619 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 620 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 621 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 622 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 623 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 624 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 625 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 626 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 627 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 628 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 629 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 630 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 631 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 632 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 633 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 634 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 635 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 636 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 637 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 638 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 639 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 640 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 641 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 642 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 643 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 644 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 645 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 646 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 647 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 648 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 649 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 650 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 651 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 652 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 653 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 654 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 655 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 656 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 657 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 658 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 659 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 660 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 661 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 662 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 663 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 664 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 665 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 666 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 667 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 668 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 669 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 670 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 671 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 672 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 673 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 674 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 675 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 676 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 677 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 678 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 679 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 680 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 681 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 682 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 683 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 684 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 685 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 686 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 687 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 688 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 689 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 690 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 691 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 692 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 693 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 694 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 695 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 696 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 697 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 698 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 699 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 700 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 701 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 702 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 703 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 704 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 705 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 706 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 707 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 708 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 709 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 710 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 711 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 712 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 713 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 714 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 715 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 716 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 717 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 718 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 719 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 720 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 721 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 722 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 723 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 724 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 725 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 726 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 727 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 728 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 729 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 730 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 731 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 732 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 733 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 734 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 735 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 736 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 737 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 738 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 739 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 740 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 741 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 742 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 743 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 744 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 745 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 746 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 747 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 748 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 749 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 750 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 751 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 752 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 753 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 754 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 755 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 756 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 757 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 758 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 759 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 760 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 761 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 762 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 763 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 764 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 765 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 766 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 767 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 768 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 769 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 770 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 771 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 772 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 773 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 774 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 775 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 776 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 777 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 778 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 779 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 780 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 781 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 782 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 783 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 784 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 785 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 786 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 787 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 788 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 789 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 790 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 791 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 792 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 793 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 794 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 795 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 796 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 797 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 798 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 799 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 800 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 801 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 802 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 803 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 804 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 805 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 806 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 807 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 808 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 809 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 810 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 811 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 812 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 813 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 814 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 815 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 816 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 817 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 818 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 819 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 820 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 821 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 822 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 823 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 824 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 825 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 826 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 827 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 828 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 829 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 830 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 831 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 832 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 833 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 834 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 835 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 836 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 837 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 838 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 839 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 840 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 841 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 842 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 843 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 844 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 845 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 846 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 847 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 848 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 849 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 850 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 851 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 852 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 853 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 854 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 855 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 856 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 857 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 858 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 859 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 860 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 861 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 862 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 863 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 864 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 865 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 866 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 867 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 868 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 869 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 870 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 871 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 872 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 873 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 874 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 875 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 876 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 877 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 878 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 879 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 880 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 881 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 882 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 883 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 884 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000 /* 885 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000001 /* 886 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000002 /* 887 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000005 /* 888 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000000000A /* 889 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000014 /* 890 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000028 /* 891 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000051 /* 892 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000000000A2 /* 893 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000145 /* 894 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000000028B /* 895 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000517 /* 896 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000A2F /* 897 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000000145F /* 898 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000000028BE /* 899 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000000517C /* 900 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000000A2F9 /* 901 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000000145F3 /* 902 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000028BE6 /* 903 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000000517CC /* 904 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000000A2F98 /* 905 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000145F30 /* 906 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000028BE60 /* 907 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000517CC1 /* 908 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000A2F983 /* 909 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000145F306 /* 910 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000028BE60D /* 911 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000517CC1B /* 912 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000000A2F9836 /* 913 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000145F306D /* 914 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000028BE60DB /* 915 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000517CC1B7 /* 916 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000000A2F9836E /* 917 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000145F306DC /* 918 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000028BE60DB9 /* 919 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000517CC1B72 /* 920 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000A2F9836E4 /* 921 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000145F306DC9 /* 922 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000028BE60DB93 /* 923 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000517CC1B727 /* 924 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000000A2F9836E4E /* 925 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000145F306DC9C /* 926 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000028BE60DB939 /* 927 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000517CC1B7272 /* 928 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00000A2F9836E4E4 /* 929 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000145F306DC9C8 /* 930 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000028BE60DB9391 /* 931 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000517CC1B72722 /* 932 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000A2F9836E4E44 /* 933 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000145F306DC9C88 /* 934 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00028BE60DB93910 /* 935 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000517CC1B727220 /* 936 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x000A2F9836E4E441 /* 937 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00145F306DC9C882 /* 938 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0028BE60DB939105 /* 939 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00517CC1B727220A /* 940 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x00A2F9836E4E4415 /* 941 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0145F306DC9C882A /* 942 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x028BE60DB9391054 /* 943 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0517CC1B727220A9 /* 944 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x0A2F9836E4E44152 /* 945 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x145F306DC9C882A5 /* 946 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x28BE60DB9391054A /* 947 */
	.quad	0x0000000000000000, 0x0000000000000000, 0x517CC1B727220A94 /* 948 */
	.quad	0x0000000000000000, 0x0000000000000000, 0xA2F9836E4E441529 /* 949 */
	.quad	0x0000000000000000, 0x0000000000000001, 0x45F306DC9C882A53 /* 950 */
	.quad	0x0000000000000000, 0x0000000000000002, 0x8BE60DB9391054A7 /* 951 */
	.quad	0x0000000000000000, 0x0000000000000005, 0x17CC1B727220A94F /* 952 */
	.quad	0x0000000000000000, 0x000000000000000A, 0x2F9836E4E441529F /* 953 */
	.quad	0x0000000000000000, 0x0000000000000014, 0x5F306DC9C882A53F /* 954 */
	.quad	0x0000000000000000, 0x0000000000000028, 0xBE60DB9391054A7F /* 955 */
	.quad	0x0000000000000000, 0x0000000000000051, 0x7CC1B727220A94FE /* 956 */
	.quad	0x0000000000000000, 0x00000000000000A2, 0xF9836E4E441529FC /* 957 */
	.quad	0x0000000000000000, 0x0000000000000145, 0xF306DC9C882A53F8 /* 958 */
	.quad	0x0000000000000000, 0x000000000000028B, 0xE60DB9391054A7F0 /* 959 */
	.quad	0x0000000000000000, 0x0000000000000517, 0xCC1B727220A94FE1 /* 960 */
	.quad	0x0000000000000000, 0x0000000000000A2F, 0x9836E4E441529FC2 /* 961 */
	.quad	0x0000000000000000, 0x000000000000145F, 0x306DC9C882A53F84 /* 962 */
	.quad	0x0000000000000000, 0x00000000000028BE, 0x60DB9391054A7F09 /* 963 */
	.quad	0x0000000000000000, 0x000000000000517C, 0xC1B727220A94FE13 /* 964 */
	.quad	0x0000000000000000, 0x000000000000A2F9, 0x836E4E441529FC27 /* 965 */
	.quad	0x0000000000000000, 0x00000000000145F3, 0x06DC9C882A53F84E /* 966 */
	.quad	0x0000000000000000, 0x0000000000028BE6, 0x0DB9391054A7F09D /* 967 */
	.quad	0x0000000000000000, 0x00000000000517CC, 0x1B727220A94FE13A /* 968 */
	.quad	0x0000000000000000, 0x00000000000A2F98, 0x36E4E441529FC275 /* 969 */
	.quad	0x0000000000000000, 0x0000000000145F30, 0x6DC9C882A53F84EA /* 970 */
	.quad	0x0000000000000000, 0x000000000028BE60, 0xDB9391054A7F09D5 /* 971 */
	.quad	0x0000000000000000, 0x0000000000517CC1, 0xB727220A94FE13AB /* 972 */
	.quad	0x0000000000000000, 0x0000000000A2F983, 0x6E4E441529FC2757 /* 973 */
	.quad	0x0000000000000000, 0x000000000145F306, 0xDC9C882A53F84EAF /* 974 */
	.quad	0x0000000000000000, 0x00000000028BE60D, 0xB9391054A7F09D5F /* 975 */
	.quad	0x0000000000000000, 0x000000000517CC1B, 0x727220A94FE13ABE /* 976 */
	.quad	0x0000000000000000, 0x000000000A2F9836, 0xE4E441529FC2757D /* 977 */
	.quad	0x0000000000000000, 0x00000000145F306D, 0xC9C882A53F84EAFA /* 978 */
	.quad	0x0000000000000000, 0x0000000028BE60DB, 0x9391054A7F09D5F4 /* 979 */
	.quad	0x0000000000000000, 0x00000000517CC1B7, 0x27220A94FE13ABE8 /* 980 */
	.quad	0x0000000000000000, 0x00000000A2F9836E, 0x4E441529FC2757D1 /* 981 */
	.quad	0x0000000000000000, 0x0000000145F306DC, 0x9C882A53F84EAFA3 /* 982 */
	.quad	0x0000000000000000, 0x000000028BE60DB9, 0x391054A7F09D5F47 /* 983 */
	.quad	0x0000000000000000, 0x0000000517CC1B72, 0x7220A94FE13ABE8F /* 984 */
	.quad	0x0000000000000000, 0x0000000A2F9836E4, 0xE441529FC2757D1F /* 985 */
	.quad	0x0000000000000000, 0x000000145F306DC9, 0xC882A53F84EAFA3E /* 986 */
	.quad	0x0000000000000000, 0x00000028BE60DB93, 0x91054A7F09D5F47D /* 987 */
	.quad	0x0000000000000000, 0x000000517CC1B727, 0x220A94FE13ABE8FA /* 988 */
	.quad	0x0000000000000000, 0x000000A2F9836E4E, 0x441529FC2757D1F5 /* 989 */
	.quad	0x0000000000000000, 0x00000145F306DC9C, 0x882A53F84EAFA3EA /* 990 */
	.quad	0x0000000000000000, 0x0000028BE60DB939, 0x1054A7F09D5F47D4 /* 991 */
	.quad	0x0000000000000000, 0x00000517CC1B7272, 0x20A94FE13ABE8FA9 /* 992 */
	.quad	0x0000000000000000, 0x00000A2F9836E4E4, 0x41529FC2757D1F53 /* 993 */
	.quad	0x0000000000000000, 0x0000145F306DC9C8, 0x82A53F84EAFA3EA6 /* 994 */
	.quad	0x0000000000000000, 0x000028BE60DB9391, 0x054A7F09D5F47D4D /* 995 */
	.quad	0x0000000000000000, 0x0000517CC1B72722, 0x0A94FE13ABE8FA9A /* 996 */
	.quad	0x0000000000000000, 0x0000A2F9836E4E44, 0x1529FC2757D1F534 /* 997 */
	.quad	0x0000000000000000, 0x000145F306DC9C88, 0x2A53F84EAFA3EA69 /* 998 */
	.quad	0x0000000000000000, 0x00028BE60DB93910, 0x54A7F09D5F47D4D3 /* 999 */
	.quad	0x0000000000000000, 0x000517CC1B727220, 0xA94FE13ABE8FA9A6 /* 1000 */
	.quad	0x0000000000000000, 0x000A2F9836E4E441, 0x529FC2757D1F534D /* 1001 */
	.quad	0x0000000000000000, 0x00145F306DC9C882, 0xA53F84EAFA3EA69B /* 1002 */
	.quad	0x0000000000000000, 0x0028BE60DB939105, 0x4A7F09D5F47D4D37 /* 1003 */
	.quad	0x0000000000000000, 0x00517CC1B727220A, 0x94FE13ABE8FA9A6E /* 1004 */
	.quad	0x0000000000000000, 0x00A2F9836E4E4415, 0x29FC2757D1F534DD /* 1005 */
	.quad	0x0000000000000000, 0x0145F306DC9C882A, 0x53F84EAFA3EA69BB /* 1006 */
	.quad	0x0000000000000000, 0x028BE60DB9391054, 0xA7F09D5F47D4D377 /* 1007 */
	.quad	0x0000000000000000, 0x0517CC1B727220A9, 0x4FE13ABE8FA9A6EE /* 1008 */
	.quad	0x0000000000000000, 0x0A2F9836E4E44152, 0x9FC2757D1F534DDC /* 1009 */
	.quad	0x0000000000000000, 0x145F306DC9C882A5, 0x3F84EAFA3EA69BB8 /* 1010 */
	.quad	0x0000000000000000, 0x28BE60DB9391054A, 0x7F09D5F47D4D3770 /* 1011 */
	.quad	0x0000000000000000, 0x517CC1B727220A94, 0xFE13ABE8FA9A6EE0 /* 1012 */
	.quad	0x0000000000000000, 0xA2F9836E4E441529, 0xFC2757D1F534DDC0 /* 1013 */
	.quad	0x0000000000000001, 0x45F306DC9C882A53, 0xF84EAFA3EA69BB81 /* 1014 */
	.quad	0x0000000000000002, 0x8BE60DB9391054A7, 0xF09D5F47D4D37703 /* 1015 */
	.quad	0x0000000000000005, 0x17CC1B727220A94F, 0xE13ABE8FA9A6EE06 /* 1016 */
	.quad	0x000000000000000A, 0x2F9836E4E441529F, 0xC2757D1F534DDC0D /* 1017 */
	.quad	0x0000000000000014, 0x5F306DC9C882A53F, 0x84EAFA3EA69BB81B /* 1018 */
	.quad	0x0000000000000028, 0xBE60DB9391054A7F, 0x09D5F47D4D377036 /* 1019 */
	.quad	0x0000000000000051, 0x7CC1B727220A94FE, 0x13ABE8FA9A6EE06D /* 1020 */
	.quad	0x00000000000000A2, 0xF9836E4E441529FC, 0x2757D1F534DDC0DB /* 1021 */
	.quad	0x0000000000000145, 0xF306DC9C882A53F8, 0x4EAFA3EA69BB81B6 /* 1022 */
	.quad	0x000000000000028B, 0xE60DB9391054A7F0, 0x9D5F47D4D377036D /* 1023 */
	.quad	0x0000000000000517, 0xCC1B727220A94FE1, 0x3ABE8FA9A6EE06DB /* 1024 */
	.quad	0x0000000000000A2F, 0x9836E4E441529FC2, 0x757D1F534DDC0DB6 /* 1025 */
	.quad	0x000000000000145F, 0x306DC9C882A53F84, 0xEAFA3EA69BB81B6C /* 1026 */
	.quad	0x00000000000028BE, 0x60DB9391054A7F09, 0xD5F47D4D377036D8 /* 1027 */
	.quad	0x000000000000517C, 0xC1B727220A94FE13, 0xABE8FA9A6EE06DB1 /* 1028 */
	.quad	0x000000000000A2F9, 0x836E4E441529FC27, 0x57D1F534DDC0DB62 /* 1029 */
	.quad	0x00000000000145F3, 0x06DC9C882A53F84E, 0xAFA3EA69BB81B6C5 /* 1030 */
	.quad	0x0000000000028BE6, 0x0DB9391054A7F09D, 0x5F47D4D377036D8A /* 1031 */
	.quad	0x00000000000517CC, 0x1B727220A94FE13A, 0xBE8FA9A6EE06DB14 /* 1032 */
	.quad	0x00000000000A2F98, 0x36E4E441529FC275, 0x7D1F534DDC0DB629 /* 1033 */
	.quad	0x0000000000145F30, 0x6DC9C882A53F84EA, 0xFA3EA69BB81B6C52 /* 1034 */
	.quad	0x000000000028BE60, 0xDB9391054A7F09D5, 0xF47D4D377036D8A5 /* 1035 */
	.quad	0x0000000000517CC1, 0xB727220A94FE13AB, 0xE8FA9A6EE06DB14A /* 1036 */
	.quad	0x0000000000A2F983, 0x6E4E441529FC2757, 0xD1F534DDC0DB6295 /* 1037 */
	.quad	0x000000000145F306, 0xDC9C882A53F84EAF, 0xA3EA69BB81B6C52B /* 1038 */
	.quad	0x00000000028BE60D, 0xB9391054A7F09D5F, 0x47D4D377036D8A56 /* 1039 */
	.quad	0x000000000517CC1B, 0x727220A94FE13ABE, 0x8FA9A6EE06DB14AC /* 1040 */
	.quad	0x000000000A2F9836, 0xE4E441529FC2757D, 0x1F534DDC0DB62959 /* 1041 */
	.quad	0x00000000145F306D, 0xC9C882A53F84EAFA, 0x3EA69BB81B6C52B3 /* 1042 */
	.quad	0x0000000028BE60DB, 0x9391054A7F09D5F4, 0x7D4D377036D8A566 /* 1043 */
	.quad	0x00000000517CC1B7, 0x27220A94FE13ABE8, 0xFA9A6EE06DB14ACC /* 1044 */
	.quad	0x00000000A2F9836E, 0x4E441529FC2757D1, 0xF534DDC0DB629599 /* 1045 */
	.quad	0x0000000145F306DC, 0x9C882A53F84EAFA3, 0xEA69BB81B6C52B32 /* 1046 */
	.quad	0x000000028BE60DB9, 0x391054A7F09D5F47, 0xD4D377036D8A5664 /* 1047 */
	.quad	0x0000000517CC1B72, 0x7220A94FE13ABE8F, 0xA9A6EE06DB14ACC9 /* 1048 */
	.quad	0x0000000A2F9836E4, 0xE441529FC2757D1F, 0x534DDC0DB6295993 /* 1049 */
	.quad	0x000000145F306DC9, 0xC882A53F84EAFA3E, 0xA69BB81B6C52B327 /* 1050 */
	.quad	0x00000028BE60DB93, 0x91054A7F09D5F47D, 0x4D377036D8A5664F /* 1051 */
	.quad	0x000000517CC1B727, 0x220A94FE13ABE8FA, 0x9A6EE06DB14ACC9E /* 1052 */
	.quad	0x000000A2F9836E4E, 0x441529FC2757D1F5, 0x34DDC0DB6295993C /* 1053 */
	.quad	0x00000145F306DC9C, 0x882A53F84EAFA3EA, 0x69BB81B6C52B3278 /* 1054 */
	.quad	0x0000028BE60DB939, 0x1054A7F09D5F47D4, 0xD377036D8A5664F1 /* 1055 */
	.quad	0x00000517CC1B7272, 0x20A94FE13ABE8FA9, 0xA6EE06DB14ACC9E2 /* 1056 */
	.quad	0x00000A2F9836E4E4, 0x41529FC2757D1F53, 0x4DDC0DB6295993C4 /* 1057 */
	.quad	0x0000145F306DC9C8, 0x82A53F84EAFA3EA6, 0x9BB81B6C52B32788 /* 1058 */
	.quad	0x000028BE60DB9391, 0x054A7F09D5F47D4D, 0x377036D8A5664F10 /* 1059 */
	.quad	0x0000517CC1B72722, 0x0A94FE13ABE8FA9A, 0x6EE06DB14ACC9E21 /* 1060 */
	.quad	0x0000A2F9836E4E44, 0x1529FC2757D1F534, 0xDDC0DB6295993C43 /* 1061 */
	.quad	0x000145F306DC9C88, 0x2A53F84EAFA3EA69, 0xBB81B6C52B327887 /* 1062 */
	.quad	0x00028BE60DB93910, 0x54A7F09D5F47D4D3, 0x77036D8A5664F10E /* 1063 */
	.quad	0x000517CC1B727220, 0xA94FE13ABE8FA9A6, 0xEE06DB14ACC9E21C /* 1064 */
	.quad	0x000A2F9836E4E441, 0x529FC2757D1F534D, 0xDC0DB6295993C439 /* 1065 */
	.quad	0x00145F306DC9C882, 0xA53F84EAFA3EA69B, 0xB81B6C52B3278872 /* 1066 */
	.quad	0x0028BE60DB939105, 0x4A7F09D5F47D4D37, 0x7036D8A5664F10E4 /* 1067 */
	.quad	0x00517CC1B727220A, 0x94FE13ABE8FA9A6E, 0xE06DB14ACC9E21C8 /* 1068 */
	.quad	0x00A2F9836E4E4415, 0x29FC2757D1F534DD, 0xC0DB6295993C4390 /* 1069 */
	.quad	0x0145F306DC9C882A, 0x53F84EAFA3EA69BB, 0x81B6C52B32788720 /* 1070 */
	.quad	0x028BE60DB9391054, 0xA7F09D5F47D4D377, 0x036D8A5664F10E41 /* 1071 */
	.quad	0x0517CC1B727220A9, 0x4FE13ABE8FA9A6EE, 0x06DB14ACC9E21C82 /* 1072 */
	.quad	0x0A2F9836E4E44152, 0x9FC2757D1F534DDC, 0x0DB6295993C43904 /* 1073 */
	.quad	0x145F306DC9C882A5, 0x3F84EAFA3EA69BB8, 0x1B6C52B327887208 /* 1074 */
	.quad	0x28BE60DB9391054A, 0x7F09D5F47D4D3770, 0x36D8A5664F10E410 /* 1075 */
	.quad	0x517CC1B727220A94, 0xFE13ABE8FA9A6EE0, 0x6DB14ACC9E21C820 /* 1076 */
	.quad	0xA2F9836E4E441529, 0xFC2757D1F534DDC0, 0xDB6295993C439041 /* 1077 */
	.quad	0x45F306DC9C882A53, 0xF84EAFA3EA69BB81, 0xB6C52B3278872083 /* 1078 */
	.quad	0x8BE60DB9391054A7, 0xF09D5F47D4D37703, 0x6D8A5664F10E4107 /* 1079 */
	.quad	0x17CC1B727220A94F, 0xE13ABE8FA9A6EE06, 0xDB14ACC9E21C820F /* 1080 */
	.quad	0x2F9836E4E441529F, 0xC2757D1F534DDC0D, 0xB6295993C439041F /* 1081 */
	.quad	0x5F306DC9C882A53F, 0x84EAFA3EA69BB81B, 0x6C52B3278872083F /* 1082 */
	.quad	0xBE60DB9391054A7F, 0x09D5F47D4D377036, 0xD8A5664F10E4107F /* 1083 */
	.quad	0x7CC1B727220A94FE, 0x13ABE8FA9A6EE06D, 0xB14ACC9E21C820FF /* 1084 */
	.quad	0xF9836E4E441529FC, 0x2757D1F534DDC0DB, 0x6295993C439041FE /* 1085 */
	.quad	0xF306DC9C882A53F8, 0x4EAFA3EA69BB81B6, 0xC52B3278872083FC /* 1086 */
	.quad	0xE60DB9391054A7F0, 0x9D5F47D4D377036D, 0x8A5664F10E4107F9 /* 1087 */
	.quad	0xCC1B727220A94FE1, 0x3ABE8FA9A6EE06DB, 0x14ACC9E21C820FF2 /* 1088 */
	.quad	0x9836E4E441529FC2, 0x757D1F534DDC0DB6, 0x295993C439041FE5 /* 1089 */
	.quad	0x306DC9C882A53F84, 0xEAFA3EA69BB81B6C, 0x52B3278872083FCA /* 1090 */
	.quad	0x60DB9391054A7F09, 0xD5F47D4D377036D8, 0xA5664F10E4107F94 /* 1091 */
	.quad	0xC1B727220A94FE13, 0xABE8FA9A6EE06DB1, 0x4ACC9E21C820FF28 /* 1092 */
	.quad	0x836E4E441529FC27, 0x57D1F534DDC0DB62, 0x95993C439041FE51 /* 1093 */
	.quad	0x06DC9C882A53F84E, 0xAFA3EA69BB81B6C5, 0x2B3278872083FCA2 /* 1094 */
	.quad	0x0DB9391054A7F09D, 0x5F47D4D377036D8A, 0x5664F10E4107F945 /* 1095 */
	.quad	0x1B727220A94FE13A, 0xBE8FA9A6EE06DB14, 0xACC9E21C820FF28B /* 1096 */
	.quad	0x36E4E441529FC275, 0x7D1F534DDC0DB629, 0x5993C439041FE516 /* 1097 */
	.quad	0x6DC9C882A53F84EA, 0xFA3EA69BB81B6C52, 0xB3278872083FCA2C /* 1098 */
	.quad	0xDB9391054A7F09D5, 0xF47D4D377036D8A5, 0x664F10E4107F9458 /* 1099 */
	.quad	0xB727220A94FE13AB, 0xE8FA9A6EE06DB14A, 0xCC9E21C820FF28B1 /* 1100 */
	.quad	0x6E4E441529FC2757, 0xD1F534DDC0DB6295, 0x993C439041FE5163 /* 1101 */
	.quad	0xDC9C882A53F84EAF, 0xA3EA69BB81B6C52B, 0x3278872083FCA2C7 /* 1102 */
	.quad	0xB9391054A7F09D5F, 0x47D4D377036D8A56, 0x64F10E4107F9458E /* 1103 */
	.quad	0x727220A94FE13ABE, 0x8FA9A6EE06DB14AC, 0xC9E21C820FF28B1D /* 1104 */
	.quad	0xE4E441529FC2757D, 0x1F534DDC0DB62959, 0x93C439041FE5163A /* 1105 */
	.quad	0xC9C882A53F84EAFA, 0x3EA69BB81B6C52B3, 0x278872083FCA2C75 /* 1106 */
	.quad	0x9391054A7F09D5F4, 0x7D4D377036D8A566, 0x4F10E4107F9458EA /* 1107 */
	.quad	0x27220A94FE13ABE8, 0xFA9A6EE06DB14ACC, 0x9E21C820FF28B1D5 /* 1108 */
	.quad	0x4E441529FC2757D1, 0xF534DDC0DB629599, 0x3C439041FE5163AB /* 1109 */
	.quad	0x9C882A53F84EAFA3, 0xEA69BB81B6C52B32, 0x78872083FCA2C757 /* 1110 */
	.quad	0x391054A7F09D5F47, 0xD4D377036D8A5664, 0xF10E4107F9458EAF /* 1111 */
	.quad	0x7220A94FE13ABE8F, 0xA9A6EE06DB14ACC9, 0xE21C820FF28B1D5E /* 1112 */
	.quad	0xE441529FC2757D1F, 0x534DDC0DB6295993, 0xC439041FE5163ABD /* 1113 */
	.quad	0xC882A53F84EAFA3E, 0xA69BB81B6C52B327, 0x8872083FCA2C757B /* 1114 */
	.quad	0x91054A7F09D5F47D, 0x4D377036D8A5664F, 0x10E4107F9458EAF7 /* 1115 */
	.quad	0x220A94FE13ABE8FA, 0x9A6EE06DB14ACC9E, 0x21C820FF28B1D5EF /* 1116 */
	.quad	0x441529FC2757D1F5, 0x34DDC0DB6295993C, 0x439041FE5163ABDE /* 1117 */
	.quad	0x882A53F84EAFA3EA, 0x69BB81B6C52B3278, 0x872083FCA2C757BD /* 1118 */
	.quad	0x1054A7F09D5F47D4, 0xD377036D8A5664F1, 0x0E4107F9458EAF7A /* 1119 */
	.quad	0x20A94FE13ABE8FA9, 0xA6EE06DB14ACC9E2, 0x1C820FF28B1D5EF5 /* 1120 */
	.quad	0x41529FC2757D1F53, 0x4DDC0DB6295993C4, 0x39041FE5163ABDEB /* 1121 */
	.quad	0x82A53F84EAFA3EA6, 0x9BB81B6C52B32788, 0x72083FCA2C757BD7 /* 1122 */
	.quad	0x054A7F09D5F47D4D, 0x377036D8A5664F10, 0xE4107F9458EAF7AE /* 1123 */
	.quad	0x0A94FE13ABE8FA9A, 0x6EE06DB14ACC9E21, 0xC820FF28B1D5EF5D /* 1124 */
	.quad	0x1529FC2757D1F534, 0xDDC0DB6295993C43, 0x9041FE5163ABDEBB /* 1125 */
	.quad	0x2A53F84EAFA3EA69, 0xBB81B6C52B327887, 0x2083FCA2C757BD77 /* 1126 */
	.quad	0x54A7F09D5F47D4D3, 0x77036D8A5664F10E, 0x4107F9458EAF7AEF /* 1127 */
	.quad	0xA94FE13ABE8FA9A6, 0xEE06DB14ACC9E21C, 0x820FF28B1D5EF5DE /* 1128 */
	.quad	0x529FC2757D1F534D, 0xDC0DB6295993C439, 0x041FE5163ABDEBBC /* 1129 */
	.quad	0xA53F84EAFA3EA69B, 0xB81B6C52B3278872, 0x083FCA2C757BD778 /* 1130 */
	.quad	0x4A7F09D5F47D4D37, 0x7036D8A5664F10E4, 0x107F9458EAF7AEF1 /* 1131 */
	.quad	0x94FE13ABE8FA9A6E, 0xE06DB14ACC9E21C8, 0x20FF28B1D5EF5DE2 /* 1132 */
	.quad	0x29FC2757D1F534DD, 0xC0DB6295993C4390, 0x41FE5163ABDEBBC5 /* 1133 */
	.quad	0x53F84EAFA3EA69BB, 0x81B6C52B32788720, 0x83FCA2C757BD778A /* 1134 */
	.quad	0xA7F09D5F47D4D377, 0x036D8A5664F10E41, 0x07F9458EAF7AEF15 /* 1135 */
	.quad	0x4FE13ABE8FA9A6EE, 0x06DB14ACC9E21C82, 0x0FF28B1D5EF5DE2B /* 1136 */
	.quad	0x9FC2757D1F534DDC, 0x0DB6295993C43904, 0x1FE5163ABDEBBC56 /* 1137 */
	.quad	0x3F84EAFA3EA69BB8, 0x1B6C52B327887208, 0x3FCA2C757BD778AC /* 1138 */
	.quad	0x7F09D5F47D4D3770, 0x36D8A5664F10E410, 0x7F9458EAF7AEF158 /* 1139 */
	.quad	0xFE13ABE8FA9A6EE0, 0x6DB14ACC9E21C820, 0xFF28B1D5EF5DE2B0 /* 1140 */
	.quad	0xFC2757D1F534DDC0, 0xDB6295993C439041, 0xFE5163ABDEBBC561 /* 1141 */
	.quad	0xF84EAFA3EA69BB81, 0xB6C52B3278872083, 0xFCA2C757BD778AC3 /* 1142 */
	.quad	0xF09D5F47D4D37703, 0x6D8A5664F10E4107, 0xF9458EAF7AEF1586 /* 1143 */
	.quad	0xE13ABE8FA9A6EE06, 0xDB14ACC9E21C820F, 0xF28B1D5EF5DE2B0D /* 1144 */
	.quad	0xC2757D1F534DDC0D, 0xB6295993C439041F, 0xE5163ABDEBBC561B /* 1145 */
	.quad	0x84EAFA3EA69BB81B, 0x6C52B3278872083F, 0xCA2C757BD778AC36 /* 1146 */
	.quad	0x09D5F47D4D377036, 0xD8A5664F10E4107F, 0x9458EAF7AEF1586D /* 1147 */
	.quad	0x13ABE8FA9A6EE06D, 0xB14ACC9E21C820FF, 0x28B1D5EF5DE2B0DB /* 1148 */
	.quad	0x2757D1F534DDC0DB, 0x6295993C439041FE, 0x5163ABDEBBC561B7 /* 1149 */
	.quad	0x4EAFA3EA69BB81B6, 0xC52B3278872083FC, 0xA2C757BD778AC36E /* 1150 */
	.quad	0x9D5F47D4D377036D, 0x8A5664F10E4107F9, 0x458EAF7AEF1586DC /* 1151 */
	.quad	0x3ABE8FA9A6EE06DB, 0x14ACC9E21C820FF2, 0x8B1D5EF5DE2B0DB9 /* 1152 */
	.quad	0x757D1F534DDC0DB6, 0x295993C439041FE5, 0x163ABDEBBC561B72 /* 1153 */
	.quad	0xEAFA3EA69BB81B6C, 0x52B3278872083FCA, 0x2C757BD778AC36E4 /* 1154 */
	.quad	0xD5F47D4D377036D8, 0xA5664F10E4107F94, 0x58EAF7AEF1586DC9 /* 1155 */
	.quad	0xABE8FA9A6EE06DB1, 0x4ACC9E21C820FF28, 0xB1D5EF5DE2B0DB92 /* 1156 */
	.quad	0x57D1F534DDC0DB62, 0x95993C439041FE51, 0x63ABDEBBC561B724 /* 1157 */
	.quad	0xAFA3EA69BB81B6C5, 0x2B3278872083FCA2, 0xC757BD778AC36E48 /* 1158 */
	.quad	0x5F47D4D377036D8A, 0x5664F10E4107F945, 0x8EAF7AEF1586DC91 /* 1159 */
	.quad	0xBE8FA9A6EE06DB14, 0xACC9E21C820FF28B, 0x1D5EF5DE2B0DB923 /* 1160 */
	.quad	0x7D1F534DDC0DB629, 0x5993C439041FE516, 0x3ABDEBBC561B7246 /* 1161 */
	.quad	0xFA3EA69BB81B6C52, 0xB3278872083FCA2C, 0x757BD778AC36E48D /* 1162 */
	.quad	0xF47D4D377036D8A5, 0x664F10E4107F9458, 0xEAF7AEF1586DC91B /* 1163 */
	.quad	0xE8FA9A6EE06DB14A, 0xCC9E21C820FF28B1, 0xD5EF5DE2B0DB9237 /* 1164 */
	.quad	0xD1F534DDC0DB6295, 0x993C439041FE5163, 0xABDEBBC561B7246E /* 1165 */
	.quad	0xA3EA69BB81B6C52B, 0x3278872083FCA2C7, 0x57BD778AC36E48DC /* 1166 */
	.quad	0x47D4D377036D8A56, 0x64F10E4107F9458E, 0xAF7AEF1586DC91B8 /* 1167 */
	.quad	0x8FA9A6EE06DB14AC, 0xC9E21C820FF28B1D, 0x5EF5DE2B0DB92371 /* 1168 */
	.quad	0x1F534DDC0DB62959, 0x93C439041FE5163A, 0xBDEBBC561B7246E3 /* 1169 */
	.quad	0x3EA69BB81B6C52B3, 0x278872083FCA2C75, 0x7BD778AC36E48DC7 /* 1170 */
	.quad	0x7D4D377036D8A566, 0x4F10E4107F9458EA, 0xF7AEF1586DC91B8E /* 1171 */
	.quad	0xFA9A6EE06DB14ACC, 0x9E21C820FF28B1D5, 0xEF5DE2B0DB92371D /* 1172 */
	.quad	0xF534DDC0DB629599, 0x3C439041FE5163AB, 0xDEBBC561B7246E3A /* 1173 */
	.quad	0xEA69BB81B6C52B32, 0x78872083FCA2C757, 0xBD778AC36E48DC74 /* 1174 */
	.quad	0xD4D377036D8A5664, 0xF10E4107F9458EAF, 0x7AEF1586DC91B8E9 /* 1175 */
	.quad	0xA9A6EE06DB14ACC9, 0xE21C820FF28B1D5E, 0xF5DE2B0DB92371D2 /* 1176 */
	.quad	0x534DDC0DB6295993, 0xC439041FE5163ABD, 0xEBBC561B7246E3A4 /* 1177 */
	.quad	0xA69BB81B6C52B327, 0x8872083FCA2C757B, 0xD778AC36E48DC748 /* 1178 */
	.quad	0x4D377036D8A5664F, 0x10E4107F9458EAF7, 0xAEF1586DC91B8E90 /* 1179 */
	.quad	0x9A6EE06DB14ACC9E, 0x21C820FF28B1D5EF, 0x5DE2B0DB92371D21 /* 1180 */
	.quad	0x34DDC0DB6295993C, 0x439041FE5163ABDE, 0xBBC561B7246E3A42 /* 1181 */
	.quad	0x69BB81B6C52B3278, 0x872083FCA2C757BD, 0x778AC36E48DC7484 /* 1182 */
	.quad	0xD377036D8A5664F1, 0x0E4107F9458EAF7A, 0xEF1586DC91B8E909 /* 1183 */
	.quad	0xA6EE06DB14ACC9E2, 0x1C820FF28B1D5EF5, 0xDE2B0DB92371D212 /* 1184 */
	.quad	0x4DDC0DB6295993C4, 0x39041FE5163ABDEB, 0xBC561B7246E3A424 /* 1185 */
	.quad	0x9BB81B6C52B32788, 0x72083FCA2C757BD7, 0x78AC36E48DC74849 /* 1186 */
	.quad	0x377036D8A5664F10, 0xE4107F9458EAF7AE, 0xF1586DC91B8E9093 /* 1187 */
	.quad	0x6EE06DB14ACC9E21, 0xC820FF28B1D5EF5D, 0xE2B0DB92371D2126 /* 1188 */
	.quad	0xDDC0DB6295993C43, 0x9041FE5163ABDEBB, 0xC561B7246E3A424D /* 1189 */
	.quad	0xBB81B6C52B327887, 0x2083FCA2C757BD77, 0x8AC36E48DC74849B /* 1190 */
	.quad	0x77036D8A5664F10E, 0x4107F9458EAF7AEF, 0x1586DC91B8E90937 /* 1191 */
	.quad	0xEE06DB14ACC9E21C, 0x820FF28B1D5EF5DE, 0x2B0DB92371D2126E /* 1192 */
	.quad	0xDC0DB6295993C439, 0x041FE5163ABDEBBC, 0x561B7246E3A424DD /* 1193 */
	.quad	0xB81B6C52B3278872, 0x083FCA2C757BD778, 0xAC36E48DC74849BA /* 1194 */
	.quad	0x7036D8A5664F10E4, 0x107F9458EAF7AEF1, 0x586DC91B8E909374 /* 1195 */
	.quad	0xE06DB14ACC9E21C8, 0x20FF28B1D5EF5DE2, 0xB0DB92371D2126E9 /* 1196 */
	.quad	0xC0DB6295993C4390, 0x41FE5163ABDEBBC5, 0x61B7246E3A424DD2 /* 1197 */
	.quad	0x81B6C52B32788720, 0x83FCA2C757BD778A, 0xC36E48DC74849BA5 /* 1198 */
	.quad	0x036D8A5664F10E41, 0x07F9458EAF7AEF15, 0x86DC91B8E909374B /* 1199 */
	.quad	0x06DB14ACC9E21C82, 0x0FF28B1D5EF5DE2B, 0x0DB92371D2126E97 /* 1200 */
	.quad	0x0DB6295993C43904, 0x1FE5163ABDEBBC56, 0x1B7246E3A424DD2E /* 1201 */
	.quad	0x1B6C52B327887208, 0x3FCA2C757BD778AC, 0x36E48DC74849BA5C /* 1202 */
	.quad	0x36D8A5664F10E410, 0x7F9458EAF7AEF158, 0x6DC91B8E909374B8 /* 1203 */
	.quad	0x6DB14ACC9E21C820, 0xFF28B1D5EF5DE2B0, 0xDB92371D2126E970 /* 1204 */
	.quad	0xDB6295993C439041, 0xFE5163ABDEBBC561, 0xB7246E3A424DD2E0 /* 1205 */
	.quad	0xB6C52B3278872083, 0xFCA2C757BD778AC3, 0x6E48DC74849BA5C0 /* 1206 */
	.quad	0x6D8A5664F10E4107, 0xF9458EAF7AEF1586, 0xDC91B8E909374B80 /* 1207 */
	.quad	0xDB14ACC9E21C820F, 0xF28B1D5EF5DE2B0D, 0xB92371D2126E9700 /* 1208 */
	.quad	0xB6295993C439041F, 0xE5163ABDEBBC561B, 0x7246E3A424DD2E00 /* 1209 */
	.quad	0x6C52B3278872083F, 0xCA2C757BD778AC36, 0xE48DC74849BA5C00 /* 1210 */
	.quad	0xD8A5664F10E4107F, 0x9458EAF7AEF1586D, 0xC91B8E909374B801 /* 1211 */
	.quad	0xB14ACC9E21C820FF, 0x28B1D5EF5DE2B0DB, 0x92371D2126E97003 /* 1212 */
	.quad	0x6295993C439041FE, 0x5163ABDEBBC561B7, 0x246E3A424DD2E006 /* 1213 */
	.quad	0xC52B3278872083FC, 0xA2C757BD778AC36E, 0x48DC74849BA5C00C /* 1214 */
	.quad	0x8A5664F10E4107F9, 0x458EAF7AEF1586DC, 0x91B8E909374B8019 /* 1215 */
	.quad	0x14ACC9E21C820FF2, 0x8B1D5EF5DE2B0DB9, 0x2371D2126E970032 /* 1216 */
	.quad	0x295993C439041FE5, 0x163ABDEBBC561B72, 0x46E3A424DD2E0064 /* 1217 */
	.quad	0x52B3278872083FCA, 0x2C757BD778AC36E4, 0x8DC74849BA5C00C9 /* 1218 */
	.quad	0xA5664F10E4107F94, 0x58EAF7AEF1586DC9, 0x1B8E909374B80192 /* 1219 */
	.quad	0x4ACC9E21C820FF28, 0xB1D5EF5DE2B0DB92, 0x371D2126E9700324 /* 1220 */
	.quad	0x95993C439041FE51, 0x63ABDEBBC561B724, 0x6E3A424DD2E00649 /* 1221 */
	.quad	0x2B3278872083FCA2, 0xC757BD778AC36E48, 0xDC74849BA5C00C92 /* 1222 */
	.quad	0x5664F10E4107F945, 0x8EAF7AEF1586DC91, 0xB8E909374B801924 /* 1223 */
	.quad	0xACC9E21C820FF28B, 0x1D5EF5DE2B0DB923, 0x71D2126E97003249 /* 1224 */
	.quad	0x5993C439041FE516, 0x3ABDEBBC561B7246, 0xE3A424DD2E006492 /* 1225 */
	.quad	0xB3278872083FCA2C, 0x757BD778AC36E48D, 0xC74849BA5C00C925 /* 1226 */
	.quad	0x664F10E4107F9458, 0xEAF7AEF1586DC91B, 0x8E909374B801924B /* 1227 */
	.quad	0xCC9E21C820FF28B1, 0xD5EF5DE2B0DB9237, 0x1D2126E970032497 /* 1228 */
	.quad	0x993C439041FE5163, 0xABDEBBC561B7246E, 0x3A424DD2E006492E /* 1229 */
	.quad	0x3278872083FCA2C7, 0x57BD778AC36E48DC, 0x74849BA5C00C925D /* 1230 */
	.quad	0x64F10E4107F9458E, 0xAF7AEF1586DC91B8, 0xE909374B801924BB /* 1231 */
	.quad	0xC9E21C820FF28B1D, 0x5EF5DE2B0DB92371, 0xD2126E9700324977 /* 1232 */
	.quad	0x93C439041FE5163A, 0xBDEBBC561B7246E3, 0xA424DD2E006492EE /* 1233 */
	.quad	0x278872083FCA2C75, 0x7BD778AC36E48DC7, 0x4849BA5C00C925DD /* 1234 */
	.quad	0x4F10E4107F9458EA, 0xF7AEF1586DC91B8E, 0x909374B801924BBA /* 1235 */
	.quad	0x9E21C820FF28B1D5, 0xEF5DE2B0DB92371D, 0x2126E97003249775 /* 1236 */
	.quad	0x3C439041FE5163AB, 0xDEBBC561B7246E3A, 0x424DD2E006492EEA /* 1237 */
	.quad	0x78872083FCA2C757, 0xBD778AC36E48DC74, 0x849BA5C00C925DD4 /* 1238 */
	.quad	0xF10E4107F9458EAF, 0x7AEF1586DC91B8E9, 0x09374B801924BBA8 /* 1239 */
	.quad	0xE21C820FF28B1D5E, 0xF5DE2B0DB92371D2, 0x126E970032497750 /* 1240 */
	.quad	0xC439041FE5163ABD, 0xEBBC561B7246E3A4, 0x24DD2E006492EEA0 /* 1241 */
	.quad	0x8872083FCA2C757B, 0xD778AC36E48DC748, 0x49BA5C00C925DD41 /* 1242 */
	.quad	0x10E4107F9458EAF7, 0xAEF1586DC91B8E90, 0x9374B801924BBA82 /* 1243 */
	.quad	0x21C820FF28B1D5EF, 0x5DE2B0DB92371D21, 0x26E9700324977504 /* 1244 */
	.quad	0x439041FE5163ABDE, 0xBBC561B7246E3A42, 0x4DD2E006492EEA09 /* 1245 */
	.quad	0x872083FCA2C757BD, 0x778AC36E48DC7484, 0x9BA5C00C925DD413 /* 1246 */
	.quad	0x0E4107F9458EAF7A, 0xEF1586DC91B8E909, 0x374B801924BBA827 /* 1247 */
	.quad	0x1C820FF28B1D5EF5, 0xDE2B0DB92371D212, 0x6E9700324977504E /* 1248 */
	.quad	0x39041FE5163ABDEB, 0xBC561B7246E3A424, 0xDD2E006492EEA09D /* 1249 */
	.quad	0x72083FCA2C757BD7, 0x78AC36E48DC74849, 0xBA5C00C925DD413A /* 1250 */
	.quad	0xE4107F9458EAF7AE, 0xF1586DC91B8E9093, 0x74B801924BBA8274 /* 1251 */
	.quad	0xC820FF28B1D5EF5D, 0xE2B0DB92371D2126, 0xE9700324977504E8 /* 1252 */
	.quad	0x9041FE5163ABDEBB, 0xC561B7246E3A424D, 0xD2E006492EEA09D1 /* 1253 */
	.quad	0x2083FCA2C757BD77, 0x8AC36E48DC74849B, 0xA5C00C925DD413A3 /* 1254 */
	.quad	0x4107F9458EAF7AEF, 0x1586DC91B8E90937, 0x4B801924BBA82746 /* 1255 */
	.quad	0x820FF28B1D5EF5DE, 0x2B0DB92371D2126E, 0x9700324977504E8C /* 1256 */
	.quad	0x041FE5163ABDEBBC, 0x561B7246E3A424DD, 0x2E006492EEA09D19 /* 1257 */
	.quad	0x083FCA2C757BD778, 0xAC36E48DC74849BA, 0x5C00C925DD413A32 /* 1258 */
	.quad	0x107F9458EAF7AEF1, 0x586DC91B8E909374, 0xB801924BBA827464 /* 1259 */
	.quad	0x20FF28B1D5EF5DE2, 0xB0DB92371D2126E9, 0x700324977504E8C9 /* 1260 */
	.quad	0x41FE5163ABDEBBC5, 0x61B7246E3A424DD2, 0xE006492EEA09D192 /* 1261 */
	.quad	0x83FCA2C757BD778A, 0xC36E48DC74849BA5, 0xC00C925DD413A324 /* 1262 */
	.quad	0x07F9458EAF7AEF15, 0x86DC91B8E909374B, 0x801924BBA8274648 /* 1263 */
	.quad	0x0FF28B1D5EF5DE2B, 0x0DB92371D2126E97, 0x00324977504E8C90 /* 1264 */
	.quad	0x1FE5163ABDEBBC56, 0x1B7246E3A424DD2E, 0x006492EEA09D1921 /* 1265 */
	.quad	0x3FCA2C757BD778AC, 0x36E48DC74849BA5C, 0x00C925DD413A3243 /* 1266 */
	.quad	0x7F9458EAF7AEF158, 0x6DC91B8E909374B8, 0x01924BBA82746487 /* 1267 */
	.quad	0xFF28B1D5EF5DE2B0, 0xDB92371D2126E970, 0x0324977504E8C90E /* 1268 */
	.quad	0xFE5163ABDEBBC561, 0xB7246E3A424DD2E0, 0x06492EEA09D1921C /* 1269 */
	.quad	0xFCA2C757BD778AC3, 0x6E48DC74849BA5C0, 0x0C925DD413A32439 /* 1270 */
	.quad	0xF9458EAF7AEF1586, 0xDC91B8E909374B80, 0x1924BBA827464873 /* 1271 */
	.quad	0xF28B1D5EF5DE2B0D, 0xB92371D2126E9700, 0x324977504E8C90E7 /* 1272 */
	.quad	0xE5163ABDEBBC561B, 0x7246E3A424DD2E00, 0x6492EEA09D1921CF /* 1273 */
	.quad	0xCA2C757BD778AC36, 0xE48DC74849BA5C00, 0xC925DD413A32439F /* 1274 */
	.quad	0x9458EAF7AEF1586D, 0xC91B8E909374B801, 0x924BBA827464873F /* 1275 */
	.quad	0x28B1D5EF5DE2B0DB, 0x92371D2126E97003, 0x24977504E8C90E7F /* 1276 */
	.quad	0x5163ABDEBBC561B7, 0x246E3A424DD2E006, 0x492EEA09D1921CFE /* 1277 */
	.quad	0xA2C757BD778AC36E, 0x48DC74849BA5C00C, 0x925DD413A32439FC /* 1278 */
	.quad	0x458EAF7AEF1586DC, 0x91B8E909374B8019, 0x24BBA827464873F8 /* 1279 */
	.quad	0x8B1D5EF5DE2B0DB9, 0x2371D2126E970032, 0x4977504E8C90E7F0 /* 1280 */
	.quad	0x163ABDEBBC561B72, 0x46E3A424DD2E0064, 0x92EEA09D1921CFE1 /* 1281 */
	.quad	0x2C757BD778AC36E4, 0x8DC74849BA5C00C9, 0x25DD413A32439FC3 /* 1282 */
	.quad	0x58EAF7AEF1586DC9, 0x1B8E909374B80192, 0x4BBA827464873F87 /* 1283 */
	.quad	0xB1D5EF5DE2B0DB92, 0x371D2126E9700324, 0x977504E8C90E7F0E /* 1284 */
	.quad	0x63ABDEBBC561B724, 0x6E3A424DD2E00649, 0x2EEA09D1921CFE1D /* 1285 */
	.quad	0xC757BD778AC36E48, 0xDC74849BA5C00C92, 0x5DD413A32439FC3B /* 1286 */
	.quad	0x8EAF7AEF1586DC91, 0xB8E909374B801924, 0xBBA827464873F877 /* 1287 */
	.quad	0x1D5EF5DE2B0DB923, 0x71D2126E97003249, 0x77504E8C90E7F0EF /* 1288 */
	.quad	0x3ABDEBBC561B7246, 0xE3A424DD2E006492, 0xEEA09D1921CFE1DE /* 1289 */
	.quad	0x757BD778AC36E48D, 0xC74849BA5C00C925, 0xDD413A32439FC3BD /* 1290 */
	.quad	0xEAF7AEF1586DC91B, 0x8E909374B801924B, 0xBA827464873F877A /* 1291 */
	.quad	0xD5EF5DE2B0DB9237, 0x1D2126E970032497, 0x7504E8C90E7F0EF5 /* 1292 */
	.quad	0xABDEBBC561B7246E, 0x3A424DD2E006492E, 0xEA09D1921CFE1DEB /* 1293 */
	.quad	0x57BD778AC36E48DC, 0x74849BA5C00C925D, 0xD413A32439FC3BD6 /* 1294 */
	.quad	0xAF7AEF1586DC91B8, 0xE909374B801924BB, 0xA827464873F877AC /* 1295 */
	.quad	0x5EF5DE2B0DB92371, 0xD2126E9700324977, 0x504E8C90E7F0EF58 /* 1296 */
	.quad	0xBDEBBC561B7246E3, 0xA424DD2E006492EE, 0xA09D1921CFE1DEB1 /* 1297 */
	.quad	0x7BD778AC36E48DC7, 0x4849BA5C00C925DD, 0x413A32439FC3BD63 /* 1298 */
	.quad	0xF7AEF1586DC91B8E, 0x909374B801924BBA, 0x827464873F877AC7 /* 1299 */
	.quad	0xEF5DE2B0DB92371D, 0x2126E97003249775, 0x04E8C90E7F0EF58E /* 1300 */
	.quad	0xDEBBC561B7246E3A, 0x424DD2E006492EEA, 0x09D1921CFE1DEB1C /* 1301 */
	.quad	0xBD778AC36E48DC74, 0x849BA5C00C925DD4, 0x13A32439FC3BD639 /* 1302 */
	.quad	0x7AEF1586DC91B8E9, 0x09374B801924BBA8, 0x27464873F877AC72 /* 1303 */
	.quad	0xF5DE2B0DB92371D2, 0x126E970032497750, 0x4E8C90E7F0EF58E5 /* 1304 */
	.quad	0xEBBC561B7246E3A4, 0x24DD2E006492EEA0, 0x9D1921CFE1DEB1CB /* 1305 */
	.quad	0xD778AC36E48DC748, 0x49BA5C00C925DD41, 0x3A32439FC3BD6396 /* 1306 */
	.quad	0xAEF1586DC91B8E90, 0x9374B801924BBA82, 0x7464873F877AC72C /* 1307 */
	.quad	0x5DE2B0DB92371D21, 0x26E9700324977504, 0xE8C90E7F0EF58E58 /* 1308 */
	.quad	0xBBC561B7246E3A42, 0x4DD2E006492EEA09, 0xD1921CFE1DEB1CB1 /* 1309 */
	.quad	0x778AC36E48DC7484, 0x9BA5C00C925DD413, 0xA32439FC3BD63962 /* 1310 */
	.quad	0xEF1586DC91B8E909, 0x374B801924BBA827, 0x464873F877AC72C4 /* 1311 */
	.quad	0xDE2B0DB92371D212, 0x6E9700324977504E, 0x8C90E7F0EF58E589 /* 1312 */
	.quad	0xBC561B7246E3A424, 0xDD2E006492EEA09D, 0x1921CFE1DEB1CB12 /* 1313 */
	.quad	0x78AC36E48DC74849, 0xBA5C00C925DD413A, 0x32439FC3BD639625 /* 1314 */
	.quad	0xF1586DC91B8E9093, 0x74B801924BBA8274, 0x64873F877AC72C4A /* 1315 */
	.quad	0xE2B0DB92371D2126, 0xE9700324977504E8, 0xC90E7F0EF58E5894 /* 1316 */
	.quad	0xC561B7246E3A424D, 0xD2E006492EEA09D1, 0x921CFE1DEB1CB129 /* 1317 */
	.quad	0x8AC36E48DC74849B, 0xA5C00C925DD413A3, 0x2439FC3BD6396253 /* 1318 */
	.quad	0x1586DC91B8E90937, 0x4B801924BBA82746, 0x4873F877AC72C4A6 /* 1319 */
	.quad	0x2B0DB92371D2126E, 0x9700324977504E8C, 0x90E7F0EF58E5894D /* 1320 */
	.quad	0x561B7246E3A424DD, 0x2E006492EEA09D19, 0x21CFE1DEB1CB129A /* 1321 */
	.quad	0xAC36E48DC74849BA, 0x5C00C925DD413A32, 0x439FC3BD63962534 /* 1322 */
	.quad	0x586DC91B8E909374, 0xB801924BBA827464, 0x873F877AC72C4A69 /* 1323 */
	.quad	0xB0DB92371D2126E9, 0x700324977504E8C9, 0x0E7F0EF58E5894D3 /* 1324 */
	.quad	0x61B7246E3A424DD2, 0xE006492EEA09D192, 0x1CFE1DEB1CB129A7 /* 1325 */
	.quad	0xC36E48DC74849BA5, 0xC00C925DD413A324, 0x39FC3BD63962534E /* 1326 */
	.quad	0x86DC91B8E909374B, 0x801924BBA8274648, 0x73F877AC72C4A69C /* 1327 */
	.quad	0x0DB92371D2126E97, 0x00324977504E8C90, 0xE7F0EF58E5894D39 /* 1328 */
	.quad	0x1B7246E3A424DD2E, 0x006492EEA09D1921, 0xCFE1DEB1CB129A73 /* 1329 */
	.quad	0x36E48DC74849BA5C, 0x00C925DD413A3243, 0x9FC3BD63962534E7 /* 1330 */
	.quad	0x6DC91B8E909374B8, 0x01924BBA82746487, 0x3F877AC72C4A69CF /* 1331 */
	.quad	0xDB92371D2126E970, 0x0324977504E8C90E, 0x7F0EF58E5894D39F /* 1332 */
	.quad	0xB7246E3A424DD2E0, 0x06492EEA09D1921C, 0xFE1DEB1CB129A73E /* 1333 */
	.quad	0x6E48DC74849BA5C0, 0x0C925DD413A32439, 0xFC3BD63962534E7D /* 1334 */
	.quad	0xDC91B8E909374B80, 0x1924BBA827464873, 0xF877AC72C4A69CFB /* 1335 */
	.quad	0xB92371D2126E9700, 0x324977504E8C90E7, 0xF0EF58E5894D39F7 /* 1336 */
	.quad	0x7246E3A424DD2E00, 0x6492EEA09D1921CF, 0xE1DEB1CB129A73EE /* 1337 */
	.quad	0xE48DC74849BA5C00, 0xC925DD413A32439F, 0xC3BD63962534E7DD /* 1338 */
	.quad	0xC91B8E909374B801, 0x924BBA827464873F, 0x877AC72C4A69CFBA /* 1339 */
	.quad	0x92371D2126E97003, 0x24977504E8C90E7F, 0x0EF58E5894D39F74 /* 1340 */
	.quad	0x246E3A424DD2E006, 0x492EEA09D1921CFE, 0x1DEB1CB129A73EE8 /* 1341 */
	.quad	0x48DC74849BA5C00C, 0x925DD413A32439FC, 0x3BD63962534E7DD1 /* 1342 */
	.quad	0x91B8E909374B8019, 0x24BBA827464873F8, 0x77AC72C4A69CFBA2 /* 1343 */
	.quad	0x2371D2126E970032, 0x4977504E8C90E7F0, 0xEF58E5894D39F744 /* 1344 */
	.quad	0x46E3A424DD2E0064, 0x92EEA09D1921CFE1, 0xDEB1CB129A73EE88 /* 1345 */
	.quad	0x8DC74849BA5C00C9, 0x25DD413A32439FC3, 0xBD63962534E7DD10 /* 1346 */
	.quad	0x1B8E909374B80192, 0x4BBA827464873F87, 0x7AC72C4A69CFBA20 /* 1347 */
	.quad	0x371D2126E9700324, 0x977504E8C90E7F0E, 0xF58E5894D39F7441 /* 1348 */
	.quad	0x6E3A424DD2E00649, 0x2EEA09D1921CFE1D, 0xEB1CB129A73EE882 /* 1349 */
	.quad	0xDC74849BA5C00C92, 0x5DD413A32439FC3B, 0xD63962534E7DD104 /* 1350 */
	.quad	0xB8E909374B801924, 0xBBA827464873F877, 0xAC72C4A69CFBA208 /* 1351 */
	.quad	0x71D2126E97003249, 0x77504E8C90E7F0EF, 0x58E5894D39F74411 /* 1352 */
	.quad	0xE3A424DD2E006492, 0xEEA09D1921CFE1DE, 0xB1CB129A73EE8823 /* 1353 */
	.quad	0xC74849BA5C00C925, 0xDD413A32439FC3BD, 0x63962534E7DD1046 /* 1354 */
	.quad	0x8E909374B801924B, 0xBA827464873F877A, 0xC72C4A69CFBA208D /* 1355 */
	.quad	0x1D2126E970032497, 0x7504E8C90E7F0EF5, 0x8E5894D39F74411A /* 1356 */
	.quad	0x3A424DD2E006492E, 0xEA09D1921CFE1DEB, 0x1CB129A73EE88235 /* 1357 */
	.quad	0x74849BA5C00C925D, 0xD413A32439FC3BD6, 0x3962534E7DD1046B /* 1358 */
	.quad	0xE909374B801924BB, 0xA827464873F877AC, 0x72C4A69CFBA208D7 /* 1359 */
	.quad	0xD2126E9700324977, 0x504E8C90E7F0EF58, 0xE5894D39F74411AF /* 1360 */
	.quad	0xA424DD2E006492EE, 0xA09D1921CFE1DEB1, 0xCB129A73EE88235F /* 1361 */
	.quad	0x4849BA5C00C925DD, 0x413A32439FC3BD63, 0x962534E7DD1046BE /* 1362 */
	.quad	0x909374B801924BBA, 0x827464873F877AC7, 0x2C4A69CFBA208D7D /* 1363 */
	.quad	0x2126E97003249775, 0x04E8C90E7F0EF58E, 0x5894D39F74411AFA /* 1364 */
	.quad	0x424DD2E006492EEA, 0x09D1921CFE1DEB1C, 0xB129A73EE88235F5 /* 1365 */
	.quad	0x849BA5C00C925DD4, 0x13A32439FC3BD639, 0x62534E7DD1046BEA /* 1366 */
	.quad	0x09374B801924BBA8, 0x27464873F877AC72, 0xC4A69CFBA208D7D4 /* 1367 */
	.quad	0x126E970032497750, 0x4E8C90E7F0EF58E5, 0x894D39F74411AFA9 /* 1368 */
	.quad	0x24DD2E006492EEA0, 0x9D1921CFE1DEB1CB, 0x129A73EE88235F52 /* 1369 */
	.quad	0x49BA5C00C925DD41, 0x3A32439FC3BD6396, 0x2534E7DD1046BEA5 /* 1370 */
	.quad	0x9374B801924BBA82, 0x7464873F877AC72C, 0x4A69CFBA208D7D4B /* 1371 */
	.quad	0x26E9700324977504, 0xE8C90E7F0EF58E58, 0x94D39F74411AFA97 /* 1372 */
	.quad	0x4DD2E006492EEA09, 0xD1921CFE1DEB1CB1, 0x29A73EE88235F52E /* 1373 */
	.quad	0x9BA5C00C925DD413, 0xA32439FC3BD63962, 0x534E7DD1046BEA5D /* 1374 */
	.quad	0x374B801924BBA827, 0x464873F877AC72C4, 0xA69CFBA208D7D4BA /* 1375 */
	.quad	0x6E9700324977504E, 0x8C90E7F0EF58E589, 0x4D39F74411AFA975 /* 1376 */
	.quad	0xDD2E006492EEA09D, 0x1921CFE1DEB1CB12, 0x9A73EE88235F52EB /* 1377 */
	.quad	0xBA5C00C925DD413A, 0x32439FC3BD639625, 0x34E7DD1046BEA5D7 /* 1378 */
	.quad	0x74B801924BBA8274, 0x64873F877AC72C4A, 0x69CFBA208D7D4BAE /* 1379 */
	.quad	0xE9700324977504E8, 0xC90E7F0EF58E5894, 0xD39F74411AFA975D /* 1380 */
	.quad	0xD2E006492EEA09D1, 0x921CFE1DEB1CB129, 0xA73EE88235F52EBB /* 1381 */
	.quad	0xA5C00C925DD413A3, 0x2439FC3BD6396253, 0x4E7DD1046BEA5D76 /* 1382 */
	.quad	0x4B801924BBA82746, 0x4873F877AC72C4A6, 0x9CFBA208D7D4BAED /* 1383 */
	.quad	0x9700324977504E8C, 0x90E7F0EF58E5894D, 0x39F74411AFA975DA /* 1384 */
	.quad	0x2E006492EEA09D19, 0x21CFE1DEB1CB129A, 0x73EE88235F52EBB4 /* 1385 */
	.quad	0x5C00C925DD413A32, 0x439FC3BD63962534, 0xE7DD1046BEA5D768 /* 1386 */
	.quad	0xB801924BBA827464, 0x873F877AC72C4A69, 0xCFBA208D7D4BAED1 /* 1387 */
	.quad	0x700324977504E8C9, 0x0E7F0EF58E5894D3, 0x9F74411AFA975DA2 /* 1388 */
	.quad	0xE006492EEA09D192, 0x1CFE1DEB1CB129A7, 0x3EE88235F52EBB44 /* 1389 */
	.quad	0xC00C925DD413A324, 0x39FC3BD63962534E, 0x7DD1046BEA5D7689 /* 1390 */
	.quad	0x801924BBA8274648, 0x73F877AC72C4A69C, 0xFBA208D7D4BAED12 /* 1391 */
	.quad	0x00324977504E8C90, 0xE7F0EF58E5894D39, 0xF74411AFA975DA24 /* 1392 */
	.quad	0x006492EEA09D1921, 0xCFE1DEB1CB129A73, 0xEE88235F52EBB448 /* 1393 */
	.quad	0x00C925DD413A3243, 0x9FC3BD63962534E7, 0xDD1046BEA5D76890 /* 1394 */
	.quad	0x01924BBA82746487, 0x3F877AC72C4A69CF, 0xBA208D7D4BAED121 /* 1395 */
	.quad	0x0324977504E8C90E, 0x7F0EF58E5894D39F, 0x74411AFA975DA242 /* 1396 */
	.quad	0x06492EEA09D1921C, 0xFE1DEB1CB129A73E, 0xE88235F52EBB4484 /* 1397 */
	.quad	0x0C925DD413A32439, 0xFC3BD63962534E7D, 0xD1046BEA5D768909 /* 1398 */
	.quad	0x1924BBA827464873, 0xF877AC72C4A69CFB, 0xA208D7D4BAED1213 /* 1399 */
	.quad	0x324977504E8C90E7, 0xF0EF58E5894D39F7, 0x4411AFA975DA2427 /* 1400 */
	.quad	0x6492EEA09D1921CF, 0xE1DEB1CB129A73EE, 0x88235F52EBB4484E /* 1401 */
	.quad	0xC925DD413A32439F, 0xC3BD63962534E7DD, 0x1046BEA5D768909D /* 1402 */
	.quad	0x924BBA827464873F, 0x877AC72C4A69CFBA, 0x208D7D4BAED1213A /* 1403 */
	.quad	0x24977504E8C90E7F, 0x0EF58E5894D39F74, 0x411AFA975DA24274 /* 1404 */
	.quad	0x492EEA09D1921CFE, 0x1DEB1CB129A73EE8, 0x8235F52EBB4484E9 /* 1405 */
	.quad	0x925DD413A32439FC, 0x3BD63962534E7DD1, 0x046BEA5D768909D3 /* 1406 */
	.quad	0x24BBA827464873F8, 0x77AC72C4A69CFBA2, 0x08D7D4BAED1213A6 /* 1407 */
	.quad	0x4977504E8C90E7F0, 0xEF58E5894D39F744, 0x11AFA975DA24274C /* 1408 */
	.quad	0x92EEA09D1921CFE1, 0xDEB1CB129A73EE88, 0x235F52EBB4484E99 /* 1409 */
	.quad	0x25DD413A32439FC3, 0xBD63962534E7DD10, 0x46BEA5D768909D33 /* 1410 */
	.quad	0x4BBA827464873F87, 0x7AC72C4A69CFBA20, 0x8D7D4BAED1213A67 /* 1411 */
	.quad	0x977504E8C90E7F0E, 0xF58E5894D39F7441, 0x1AFA975DA24274CE /* 1412 */
	.quad	0x2EEA09D1921CFE1D, 0xEB1CB129A73EE882, 0x35F52EBB4484E99C /* 1413 */
	.quad	0x5DD413A32439FC3B, 0xD63962534E7DD104, 0x6BEA5D768909D338 /* 1414 */
	.quad	0xBBA827464873F877, 0xAC72C4A69CFBA208, 0xD7D4BAED1213A671 /* 1415 */
	.quad	0x77504E8C90E7F0EF, 0x58E5894D39F74411, 0xAFA975DA24274CE3 /* 1416 */
	.quad	0xEEA09D1921CFE1DE, 0xB1CB129A73EE8823, 0x5F52EBB4484E99C7 /* 1417 */
	.quad	0xDD413A32439FC3BD, 0x63962534E7DD1046, 0xBEA5D768909D338E /* 1418 */
	.quad	0xBA827464873F877A, 0xC72C4A69CFBA208D, 0x7D4BAED1213A671C /* 1419 */
	.quad	0x7504E8C90E7F0EF5, 0x8E5894D39F74411A, 0xFA975DA24274CE38 /* 1420 */
	.quad	0xEA09D1921CFE1DEB, 0x1CB129A73EE88235, 0xF52EBB4484E99C70 /* 1421 */
	.quad	0xD413A32439FC3BD6, 0x3962534E7DD1046B, 0xEA5D768909D338E0 /* 1422 */
	.quad	0xA827464873F877AC, 0x72C4A69CFBA208D7, 0xD4BAED1213A671C0 /* 1423 */
	.quad	0x504E8C90E7F0EF58, 0xE5894D39F74411AF, 0xA975DA24274CE381 /* 1424 */
	.quad	0xA09D1921CFE1DEB1, 0xCB129A73EE88235F, 0x52EBB4484E99C702 /* 1425 */
	.quad	0x413A32439FC3BD63, 0x962534E7DD1046BE, 0xA5D768909D338E04 /* 1426 */
	.quad	0x827464873F877AC7, 0x2C4A69CFBA208D7D, 0x4BAED1213A671C09 /* 1427 */
	.quad	0x04E8C90E7F0EF58E, 0x5894D39F74411AFA, 0x975DA24274CE3813 /* 1428 */
	.quad	0x09D1921CFE1DEB1C, 0xB129A73EE88235F5, 0x2EBB4484E99C7026 /* 1429 */
	.quad	0x13A32439FC3BD639, 0x62534E7DD1046BEA, 0x5D768909D338E04D /* 1430 */
	.quad	0x27464873F877AC72, 0xC4A69CFBA208D7D4, 0xBAED1213A671C09A /* 1431 */
	.quad	0x4E8C90E7F0EF58E5, 0x894D39F74411AFA9, 0x75DA24274CE38135 /* 1432 */
	.quad	0x9D1921CFE1DEB1CB, 0x129A73EE88235F52, 0xEBB4484E99C7026B /* 1433 */
	.quad	0x3A32439FC3BD6396, 0x2534E7DD1046BEA5, 0xD768909D338E04D6 /* 1434 */
	.quad	0x7464873F877AC72C, 0x4A69CFBA208D7D4B, 0xAED1213A671C09AD /* 1435 */
	.quad	0xE8C90E7F0EF58E58, 0x94D39F74411AFA97, 0x5DA24274CE38135A /* 1436 */
	.quad	0xD1921CFE1DEB1CB1, 0x29A73EE88235F52E, 0xBB4484E99C7026B4 /* 1437 */
	.quad	0xA32439FC3BD63962, 0x534E7DD1046BEA5D, 0x768909D338E04D68 /* 1438 */
	.quad	0x464873F877AC72C4, 0xA69CFBA208D7D4BA, 0xED1213A671C09AD1 /* 1439 */
	.quad	0x8C90E7F0EF58E589, 0x4D39F74411AFA975, 0xDA24274CE38135A2 /* 1440 */
	.quad	0x1921CFE1DEB1CB12, 0x9A73EE88235F52EB, 0xB4484E99C7026B45 /* 1441 */
	.quad	0x32439FC3BD639625, 0x34E7DD1046BEA5D7, 0x68909D338E04D68B /* 1442 */
	.quad	0x64873F877AC72C4A, 0x69CFBA208D7D4BAE, 0xD1213A671C09AD17 /* 1443 */
	.quad	0xC90E7F0EF58E5894, 0xD39F74411AFA975D, 0xA24274CE38135A2F /* 1444 */
	.quad	0x921CFE1DEB1CB129, 0xA73EE88235F52EBB, 0x4484E99C7026B45F /* 1445 */
	.quad	0x2439FC3BD6396253, 0x4E7DD1046BEA5D76, 0x8909D338E04D68BE /* 1446 */
	.quad	0x4873F877AC72C4A6, 0x9CFBA208D7D4BAED, 0x1213A671C09AD17D /* 1447 */
	.quad	0x90E7F0EF58E5894D, 0x39F74411AFA975DA, 0x24274CE38135A2FB /* 1448 */
	.quad	0x21CFE1DEB1CB129A, 0x73EE88235F52EBB4, 0x484E99C7026B45F7 /* 1449 */
	.quad	0x439FC3BD63962534, 0xE7DD1046BEA5D768, 0x909D338E04D68BEF /* 1450 */
	.quad	0x873F877AC72C4A69, 0xCFBA208D7D4BAED1, 0x213A671C09AD17DF /* 1451 */
	.quad	0x0E7F0EF58E5894D3, 0x9F74411AFA975DA2, 0x4274CE38135A2FBF /* 1452 */
	.quad	0x1CFE1DEB1CB129A7, 0x3EE88235F52EBB44, 0x84E99C7026B45F7E /* 1453 */
	.quad	0x39FC3BD63962534E, 0x7DD1046BEA5D7689, 0x09D338E04D68BEFC /* 1454 */
	.quad	0x73F877AC72C4A69C, 0xFBA208D7D4BAED12, 0x13A671C09AD17DF9 /* 1455 */
	.quad	0xE7F0EF58E5894D39, 0xF74411AFA975DA24, 0x274CE38135A2FBF2 /* 1456 */
	.quad	0xCFE1DEB1CB129A73, 0xEE88235F52EBB448, 0x4E99C7026B45F7E4 /* 1457 */
	.quad	0x9FC3BD63962534E7, 0xDD1046BEA5D76890, 0x9D338E04D68BEFC8 /* 1458 */
	.quad	0x3F877AC72C4A69CF, 0xBA208D7D4BAED121, 0x3A671C09AD17DF90 /* 1459 */
	.quad	0x7F0EF58E5894D39F, 0x74411AFA975DA242, 0x74CE38135A2FBF20 /* 1460 */
	.quad	0xFE1DEB1CB129A73E, 0xE88235F52EBB4484, 0xE99C7026B45F7E41 /* 1461 */
	.quad	0xFC3BD63962534E7D, 0xD1046BEA5D768909, 0xD338E04D68BEFC82 /* 1462 */
	.quad	0xF877AC72C4A69CFB, 0xA208D7D4BAED1213, 0xA671C09AD17DF904 /* 1463 */
	.quad	0xF0EF58E5894D39F7, 0x4411AFA975DA2427, 0x4CE38135A2FBF209 /* 1464 */
	.quad	0xE1DEB1CB129A73EE, 0x88235F52EBB4484E, 0x99C7026B45F7E413 /* 1465 */
	.quad	0xC3BD63962534E7DD, 0x1046BEA5D768909D, 0x338E04D68BEFC827 /* 1466 */
	.quad	0x877AC72C4A69CFBA, 0x208D7D4BAED1213A, 0x671C09AD17DF904E /* 1467 */
	.quad	0x0EF58E5894D39F74, 0x411AFA975DA24274, 0xCE38135A2FBF209C /* 1468 */
	.quad	0x1DEB1CB129A73EE8, 0x8235F52EBB4484E9, 0x9C7026B45F7E4139 /* 1469 */
	.quad	0x3BD63962534E7DD1, 0x046BEA5D768909D3, 0x38E04D68BEFC8273 /* 1470 */
	.quad	0x77AC72C4A69CFBA2, 0x08D7D4BAED1213A6, 0x71C09AD17DF904E6 /* 1471 */
	.quad	0xEF58E5894D39F744, 0x11AFA975DA24274C, 0xE38135A2FBF209CC /* 1472 */
	.quad	0xDEB1CB129A73EE88, 0x235F52EBB4484E99, 0xC7026B45F7E41399 /* 1473 */
	.quad	0xBD63962534E7DD10, 0x46BEA5D768909D33, 0x8E04D68BEFC82732 /* 1474 */
	.quad	0x7AC72C4A69CFBA20, 0x8D7D4BAED1213A67, 0x1C09AD17DF904E64 /* 1475 */
	.quad	0xF58E5894D39F7441, 0x1AFA975DA24274CE, 0x38135A2FBF209CC8 /* 1476 */
	.quad	0xEB1CB129A73EE882, 0x35F52EBB4484E99C, 0x7026B45F7E413991 /* 1477 */
	.quad	0xD63962534E7DD104, 0x6BEA5D768909D338, 0xE04D68BEFC827323 /* 1478 */
	.quad	0xAC72C4A69CFBA208, 0xD7D4BAED1213A671, 0xC09AD17DF904E647 /* 1479 */
	.quad	0x58E5894D39F74411, 0xAFA975DA24274CE3, 0x8135A2FBF209CC8E /* 1480 */
	.quad	0xB1CB129A73EE8823, 0x5F52EBB4484E99C7, 0x026B45F7E413991D /* 1481 */
	.quad	0x63962534E7DD1046, 0xBEA5D768909D338E, 0x04D68BEFC827323A /* 1482 */
	.quad	0xC72C4A69CFBA208D, 0x7D4BAED1213A671C, 0x09AD17DF904E6475 /* 1483 */
	.quad	0x8E5894D39F74411A, 0xFA975DA24274CE38, 0x135A2FBF209CC8EB /* 1484 */
	.quad	0x1CB129A73EE88235, 0xF52EBB4484E99C70, 0x26B45F7E413991D6 /* 1485 */
	.quad	0x3962534E7DD1046B, 0xEA5D768909D338E0, 0x4D68BEFC827323AC /* 1486 */
	.quad	0x72C4A69CFBA208D7, 0xD4BAED1213A671C0, 0x9AD17DF904E64758 /* 1487 */
	.quad	0xE5894D39F74411AF, 0xA975DA24274CE381, 0x35A2FBF209CC8EB1 /* 1488 */
	.quad	0xCB129A73EE88235F, 0x52EBB4484E99C702, 0x6B45F7E413991D63 /* 1489 */
	.quad	0x962534E7DD1046BE, 0xA5D768909D338E04, 0xD68BEFC827323AC7 /* 1490 */
	.quad	0x2C4A69CFBA208D7D, 0x4BAED1213A671C09, 0xAD17DF904E64758E /* 1491 */
	.quad	0x5894D39F74411AFA, 0x975DA24274CE3813, 0x5A2FBF209CC8EB1C /* 1492 */
	.quad	0xB129A73EE88235F5, 0x2EBB4484E99C7026, 0xB45F7E413991D639 /* 1493 */
	.quad	0x62534E7DD1046BEA, 0x5D768909D338E04D, 0x68BEFC827323AC73 /* 1494 */
	.quad	0xC4A69CFBA208D7D4, 0xBAED1213A671C09A, 0xD17DF904E64758E6 /* 1495 */
	.quad	0x894D39F74411AFA9, 0x75DA24274CE38135, 0xA2FBF209CC8EB1CC /* 1496 */
	.quad	0x129A73EE88235F52, 0xEBB4484E99C7026B, 0x45F7E413991D6398 /* 1497 */
	.quad	0x2534E7DD1046BEA5, 0xD768909D338E04D6, 0x8BEFC827323AC730 /* 1498 */
	.quad	0x4A69CFBA208D7D4B, 0xAED1213A671C09AD, 0x17DF904E64758E60 /* 1499 */
	.quad	0x94D39F74411AFA97, 0x5DA24274CE38135A, 0x2FBF209CC8EB1CC1 /* 1500 */
	.quad	0x29A73EE88235F52E, 0xBB4484E99C7026B4, 0x5F7E413991D63983 /* 1501 */
	.quad	0x534E7DD1046BEA5D, 0x768909D338E04D68, 0xBEFC827323AC7306 /* 1502 */
	.quad	0xA69CFBA208D7D4BA, 0xED1213A671C09AD1, 0x7DF904E64758E60D /* 1503 */
	.quad	0x4D39F74411AFA975, 0xDA24274CE38135A2, 0xFBF209CC8EB1CC1A /* 1504 */
	.quad	0x9A73EE88235F52EB, 0xB4484E99C7026B45, 0xF7E413991D639835 /* 1505 */
	.quad	0x34E7DD1046BEA5D7, 0x68909D338E04D68B, 0xEFC827323AC7306A /* 1506 */
	.quad	0x69CFBA208D7D4BAE, 0xD1213A671C09AD17, 0xDF904E64758E60D4 /* 1507 */
	.quad	0xD39F74411AFA975D, 0xA24274CE38135A2F, 0xBF209CC8EB1CC1A9 /* 1508 */
	.quad	0xA73EE88235F52EBB, 0x4484E99C7026B45F, 0x7E413991D6398353 /* 1509 */
	.quad	0x4E7DD1046BEA5D76, 0x8909D338E04D68BE, 0xFC827323AC7306A6 /* 1510 */
	.quad	0x9CFBA208D7D4BAED, 0x1213A671C09AD17D, 0xF904E64758E60D4C /* 1511 */
	.quad	0x39F74411AFA975DA, 0x24274CE38135A2FB, 0xF209CC8EB1CC1A99 /* 1512 */
	.quad	0x73EE88235F52EBB4, 0x484E99C7026B45F7, 0xE413991D63983533 /* 1513 */
	.quad	0xE7DD1046BEA5D768, 0x909D338E04D68BEF, 0xC827323AC7306A67 /* 1514 */
	.quad	0xCFBA208D7D4BAED1, 0x213A671C09AD17DF, 0x904E64758E60D4CE /* 1515 */
	.quad	0x9F74411AFA975DA2, 0x4274CE38135A2FBF, 0x209CC8EB1CC1A99C /* 1516 */
	.quad	0x3EE88235F52EBB44, 0x84E99C7026B45F7E, 0x413991D639835339 /* 1517 */
	.quad	0x7DD1046BEA5D7689, 0x09D338E04D68BEFC, 0x827323AC7306A673 /* 1518 */
	.quad	0xFBA208D7D4BAED12, 0x13A671C09AD17DF9, 0x04E64758E60D4CE7 /* 1519 */
	.quad	0xF74411AFA975DA24, 0x274CE38135A2FBF2, 0x09CC8EB1CC1A99CF /* 1520 */
	.quad	0xEE88235F52EBB448, 0x4E99C7026B45F7E4, 0x13991D639835339F /* 1521 */
	.quad	0xDD1046BEA5D76890, 0x9D338E04D68BEFC8, 0x27323AC7306A673E /* 1522 */
	.quad	0xBA208D7D4BAED121, 0x3A671C09AD17DF90, 0x4E64758E60D4CE7D /* 1523 */
	.quad	0x74411AFA975DA242, 0x74CE38135A2FBF20, 0x9CC8EB1CC1A99CFA /* 1524 */
	.quad	0xE88235F52EBB4484, 0xE99C7026B45F7E41, 0x3991D639835339F4 /* 1525 */
	.quad	0xD1046BEA5D768909, 0xD338E04D68BEFC82, 0x7323AC7306A673E9 /* 1526 */
	.quad	0xA208D7D4BAED1213, 0xA671C09AD17DF904, 0xE64758E60D4CE7D2 /* 1527 */
	.quad	0x4411AFA975DA2427, 0x4CE38135A2FBF209, 0xCC8EB1CC1A99CFA4 /* 1528 */
	.quad	0x88235F52EBB4484E, 0x99C7026B45F7E413, 0x991D639835339F49 /* 1529 */
	.quad	0x1046BEA5D768909D, 0x338E04D68BEFC827, 0x323AC7306A673E93 /* 1530 */
	.quad	0x208D7D4BAED1213A, 0x671C09AD17DF904E, 0x64758E60D4CE7D27 /* 1531 */
	.quad	0x411AFA975DA24274, 0xCE38135A2FBF209C, 0xC8EB1CC1A99CFA4E /* 1532 */
	.quad	0x8235F52EBB4484E9, 0x9C7026B45F7E4139, 0x91D639835339F49C /* 1533 */
	.quad	0x046BEA5D768909D3, 0x38E04D68BEFC8273, 0x23AC7306A673E939 /* 1534 */
	.quad	0x08D7D4BAED1213A6, 0x71C09AD17DF904E6, 0x4758E60D4CE7D272 /* 1535 */
	.quad	0x11AFA975DA24274C, 0xE38135A2FBF209CC, 0x8EB1CC1A99CFA4E4 /* 1536 */
	.quad	0x235F52EBB4484E99, 0xC7026B45F7E41399, 0x1D639835339F49C8 /* 1537 */
	.quad	0x46BEA5D768909D33, 0x8E04D68BEFC82732, 0x3AC7306A673E9390 /* 1538 */
	.quad	0x8D7D4BAED1213A67, 0x1C09AD17DF904E64, 0x758E60D4CE7D2721 /* 1539 */
	.quad	0x1AFA975DA24274CE, 0x38135A2FBF209CC8, 0xEB1CC1A99CFA4E42 /* 1540 */
	.quad	0x35F52EBB4484E99C, 0x7026B45F7E413991, 0xD639835339F49C84 /* 1541 */
	.quad	0x6BEA5D768909D338, 0xE04D68BEFC827323, 0xAC7306A673E93908 /* 1542 */
	.quad	0xD7D4BAED1213A671, 0xC09AD17DF904E647, 0x58E60D4CE7D27211 /* 1543 */
	.quad	0xAFA975DA24274CE3, 0x8135A2FBF209CC8E, 0xB1CC1A99CFA4E422 /* 1544 */
	.quad	0x5F52EBB4484E99C7, 0x026B45F7E413991D, 0x639835339F49C845 /* 1545 */
	.quad	0xBEA5D768909D338E, 0x04D68BEFC827323A, 0xC7306A673E93908B /* 1546 */
	.quad	0x7D4BAED1213A671C, 0x09AD17DF904E6475, 0x8E60D4CE7D272117 /* 1547 */
	.quad	0xFA975DA24274CE38, 0x135A2FBF209CC8EB, 0x1CC1A99CFA4E422F /* 1548 */
	.quad	0xF52EBB4484E99C70, 0x26B45F7E413991D6, 0x39835339F49C845F /* 1549 */
	.quad	0xEA5D768909D338E0, 0x4D68BEFC827323AC, 0x7306A673E93908BF /* 1550 */
	.quad	0xD4BAED1213A671C0, 0x9AD17DF904E64758, 0xE60D4CE7D272117E /* 1551 */
	.quad	0xA975DA24274CE381, 0x35A2FBF209CC8EB1, 0xCC1A99CFA4E422FC /* 1552 */
	.quad	0x52EBB4484E99C702, 0x6B45F7E413991D63, 0x9835339F49C845F8 /* 1553 */
	.quad	0xA5D768909D338E04, 0xD68BEFC827323AC7, 0x306A673E93908BF1 /* 1554 */
	.quad	0x4BAED1213A671C09, 0xAD17DF904E64758E, 0x60D4CE7D272117E2 /* 1555 */
	.quad	0x975DA24274CE3813, 0x5A2FBF209CC8EB1C, 0xC1A99CFA4E422FC5 /* 1556 */
	.quad	0x2EBB4484E99C7026, 0xB45F7E413991D639, 0x835339F49C845F8B /* 1557 */
	.quad	0x5D768909D338E04D, 0x68BEFC827323AC73, 0x06A673E93908BF17 /* 1558 */
	.quad	0xBAED1213A671C09A, 0xD17DF904E64758E6, 0x0D4CE7D272117E2E /* 1559 */
	.quad	0x75DA24274CE38135, 0xA2FBF209CC8EB1CC, 0x1A99CFA4E422FC5D /* 1560 */
	.quad	0xEBB4484E99C7026B, 0x45F7E413991D6398, 0x35339F49C845F8BB /* 1561 */
	.quad	0xD768909D338E04D6, 0x8BEFC827323AC730, 0x6A673E93908BF177 /* 1562 */
	.quad	0xAED1213A671C09AD, 0x17DF904E64758E60, 0xD4CE7D272117E2EF /* 1563 */
	.quad	0x5DA24274CE38135A, 0x2FBF209CC8EB1CC1, 0xA99CFA4E422FC5DE /* 1564 */
	.quad	0xBB4484E99C7026B4, 0x5F7E413991D63983, 0x5339F49C845F8BBD /* 1565 */
	.quad	0x768909D338E04D68, 0xBEFC827323AC7306, 0xA673E93908BF177B /* 1566 */
	.quad	0xED1213A671C09AD1, 0x7DF904E64758E60D, 0x4CE7D272117E2EF7 /* 1567 */
	.quad	0xDA24274CE38135A2, 0xFBF209CC8EB1CC1A, 0x99CFA4E422FC5DEF /* 1568 */
	.quad	0xB4484E99C7026B45, 0xF7E413991D639835, 0x339F49C845F8BBDF /* 1569 */
	.quad	0x68909D338E04D68B, 0xEFC827323AC7306A, 0x673E93908BF177BF /* 1570 */
	.quad	0xD1213A671C09AD17, 0xDF904E64758E60D4, 0xCE7D272117E2EF7E /* 1571 */
	.quad	0xA24274CE38135A2F, 0xBF209CC8EB1CC1A9, 0x9CFA4E422FC5DEFC /* 1572 */
	.quad	0x4484E99C7026B45F, 0x7E413991D6398353, 0x39F49C845F8BBDF9 /* 1573 */
	.quad	0x8909D338E04D68BE, 0xFC827323AC7306A6, 0x73E93908BF177BF2 /* 1574 */
	.quad	0x1213A671C09AD17D, 0xF904E64758E60D4C, 0xE7D272117E2EF7E4 /* 1575 */
	.quad	0x24274CE38135A2FB, 0xF209CC8EB1CC1A99, 0xCFA4E422FC5DEFC9 /* 1576 */
	.quad	0x484E99C7026B45F7, 0xE413991D63983533, 0x9F49C845F8BBDF92 /* 1577 */
	.quad	0x909D338E04D68BEF, 0xC827323AC7306A67, 0x3E93908BF177BF25 /* 1578 */
	.quad	0x213A671C09AD17DF, 0x904E64758E60D4CE, 0x7D272117E2EF7E4A /* 1579 */
	.quad	0x4274CE38135A2FBF, 0x209CC8EB1CC1A99C, 0xFA4E422FC5DEFC94 /* 1580 */
	.quad	0x84E99C7026B45F7E, 0x413991D639835339, 0xF49C845F8BBDF928 /* 1581 */
	.quad	0x09D338E04D68BEFC, 0x827323AC7306A673, 0xE93908BF177BF250 /* 1582 */
	.quad	0x13A671C09AD17DF9, 0x04E64758E60D4CE7, 0xD272117E2EF7E4A0 /* 1583 */
	.quad	0x274CE38135A2FBF2, 0x09CC8EB1CC1A99CF, 0xA4E422FC5DEFC941 /* 1584 */
	.quad	0x4E99C7026B45F7E4, 0x13991D639835339F, 0x49C845F8BBDF9283 /* 1585 */
	.quad	0x9D338E04D68BEFC8, 0x27323AC7306A673E, 0x93908BF177BF2507 /* 1586 */
	.quad	0x3A671C09AD17DF90, 0x4E64758E60D4CE7D, 0x272117E2EF7E4A0E /* 1587 */
	.quad	0x74CE38135A2FBF20, 0x9CC8EB1CC1A99CFA, 0x4E422FC5DEFC941D /* 1588 */
	.quad	0xE99C7026B45F7E41, 0x3991D639835339F4, 0x9C845F8BBDF9283B /* 1589 */
	.quad	0xD338E04D68BEFC82, 0x7323AC7306A673E9, 0x3908BF177BF25076 /* 1590 */
	.quad	0xA671C09AD17DF904, 0xE64758E60D4CE7D2, 0x72117E2EF7E4A0EC /* 1591 */
	.quad	0x4CE38135A2FBF209, 0xCC8EB1CC1A99CFA4, 0xE422FC5DEFC941D8 /* 1592 */
	.quad	0x99C7026B45F7E413, 0x991D639835339F49, 0xC845F8BBDF9283B1 /* 1593 */
	.quad	0x338E04D68BEFC827, 0x323AC7306A673E93, 0x908BF177BF250763 /* 1594 */
	.quad	0x671C09AD17DF904E, 0x64758E60D4CE7D27, 0x2117E2EF7E4A0EC7 /* 1595 */
	.quad	0xCE38135A2FBF209C, 0xC8EB1CC1A99CFA4E, 0x422FC5DEFC941D8F /* 1596 */
	.quad	0x9C7026B45F7E4139, 0x91D639835339F49C, 0x845F8BBDF9283B1F /* 1597 */
	.quad	0x38E04D68BEFC8273, 0x23AC7306A673E939, 0x08BF177BF250763F /* 1598 */
	.quad	0x71C09AD17DF904E6, 0x4758E60D4CE7D272, 0x117E2EF7E4A0EC7F /* 1599 */
	.quad	0xE38135A2FBF209CC, 0x8EB1CC1A99CFA4E4, 0x22FC5DEFC941D8FF /* 1600 */
	.quad	0xC7026B45F7E41399, 0x1D639835339F49C8, 0x45F8BBDF9283B1FF /* 1601 */
	.quad	0x8E04D68BEFC82732, 0x3AC7306A673E9390, 0x8BF177BF250763FF /* 1602 */
	.quad	0x1C09AD17DF904E64, 0x758E60D4CE7D2721, 0x17E2EF7E4A0EC7FE /* 1603 */
	.quad	0x38135A2FBF209CC8, 0xEB1CC1A99CFA4E42, 0x2FC5DEFC941D8FFC /* 1604 */
	.quad	0x7026B45F7E413991, 0xD639835339F49C84, 0x5F8BBDF9283B1FF8 /* 1605 */
	.quad	0xE04D68BEFC827323, 0xAC7306A673E93908, 0xBF177BF250763FF1 /* 1606 */
	.quad	0xC09AD17DF904E647, 0x58E60D4CE7D27211, 0x7E2EF7E4A0EC7FE2 /* 1607 */
	.quad	0x8135A2FBF209CC8E, 0xB1CC1A99CFA4E422, 0xFC5DEFC941D8FFC4 /* 1608 */
	.quad	0x026B45F7E413991D, 0x639835339F49C845, 0xF8BBDF9283B1FF89 /* 1609 */
	.quad	0x04D68BEFC827323A, 0xC7306A673E93908B, 0xF177BF250763FF12 /* 1610 */
	.quad	0x09AD17DF904E6475, 0x8E60D4CE7D272117, 0xE2EF7E4A0EC7FE25 /* 1611 */
	.quad	0x135A2FBF209CC8EB, 0x1CC1A99CFA4E422F, 0xC5DEFC941D8FFC4B /* 1612 */
	.quad	0x26B45F7E413991D6, 0x39835339F49C845F, 0x8BBDF9283B1FF897 /* 1613 */
	.quad	0x4D68BEFC827323AC, 0x7306A673E93908BF, 0x177BF250763FF12F /* 1614 */
	.quad	0x9AD17DF904E64758, 0xE60D4CE7D272117E, 0x2EF7E4A0EC7FE25F /* 1615 */
	.quad	0x35A2FBF209CC8EB1, 0xCC1A99CFA4E422FC, 0x5DEFC941D8FFC4BF /* 1616 */
	.quad	0x6B45F7E413991D63, 0x9835339F49C845F8, 0xBBDF9283B1FF897F /* 1617 */
	.quad	0xD68BEFC827323AC7, 0x306A673E93908BF1, 0x77BF250763FF12FF /* 1618 */
	.quad	0xAD17DF904E64758E, 0x60D4CE7D272117E2, 0xEF7E4A0EC7FE25FF /* 1619 */
	.quad	0x5A2FBF209CC8EB1C, 0xC1A99CFA4E422FC5, 0xDEFC941D8FFC4BFF /* 1620 */
	.quad	0xB45F7E413991D639, 0x835339F49C845F8B, 0xBDF9283B1FF897FF /* 1621 */
	.quad	0x68BEFC827323AC73, 0x06A673E93908BF17, 0x7BF250763FF12FFF /* 1622 */
	.quad	0xD17DF904E64758E6, 0x0D4CE7D272117E2E, 0xF7E4A0EC7FE25FFF /* 1623 */
	.quad	0xA2FBF209CC8EB1CC, 0x1A99CFA4E422FC5D, 0xEFC941D8FFC4BFFE /* 1624 */
	.quad	0x45F7E413991D6398, 0x35339F49C845F8BB, 0xDF9283B1FF897FFD /* 1625 */
	.quad	0x8BEFC827323AC730, 0x6A673E93908BF177, 0xBF250763FF12FFFB /* 1626 */
	.quad	0x17DF904E64758E60, 0xD4CE7D272117E2EF, 0x7E4A0EC7FE25FFF7 /* 1627 */
	.quad	0x2FBF209CC8EB1CC1, 0xA99CFA4E422FC5DE, 0xFC941D8FFC4BFFEF /* 1628 */
	.quad	0x5F7E413991D63983, 0x5339F49C845F8BBD, 0xF9283B1FF897FFDE /* 1629 */
	.quad	0xBEFC827323AC7306, 0xA673E93908BF177B, 0xF250763FF12FFFBC /* 1630 */
	.quad	0x7DF904E64758E60D, 0x4CE7D272117E2EF7, 0xE4A0EC7FE25FFF78 /* 1631 */
	.quad	0xFBF209CC8EB1CC1A, 0x99CFA4E422FC5DEF, 0xC941D8FFC4BFFEF0 /* 1632 */
	.quad	0xF7E413991D639835, 0x339F49C845F8BBDF, 0x9283B1FF897FFDE0 /* 1633 */
	.quad	0xEFC827323AC7306A, 0x673E93908BF177BF, 0x250763FF12FFFBC0 /* 1634 */
	.quad	0xDF904E64758E60D4, 0xCE7D272117E2EF7E, 0x4A0EC7FE25FFF781 /* 1635 */
	.quad	0xBF209CC8EB1CC1A9, 0x9CFA4E422FC5DEFC, 0x941D8FFC4BFFEF02 /* 1636 */
	.quad	0x7E413991D6398353, 0x39F49C845F8BBDF9, 0x283B1FF897FFDE05 /* 1637 */
	.quad	0xFC827323AC7306A6, 0x73E93908BF177BF2, 0x50763FF12FFFBC0B /* 1638 */
	.quad	0xF904E64758E60D4C, 0xE7D272117E2EF7E4, 0xA0EC7FE25FFF7816 /* 1639 */
	.quad	0xF209CC8EB1CC1A99, 0xCFA4E422FC5DEFC9, 0x41D8FFC4BFFEF02C /* 1640 */
	.quad	0xE413991D63983533, 0x9F49C845F8BBDF92, 0x83B1FF897FFDE059 /* 1641 */
	.quad	0xC827323AC7306A67, 0x3E93908BF177BF25, 0x0763FF12FFFBC0B3 /* 1642 */
	.quad	0x904E64758E60D4CE, 0x7D272117E2EF7E4A, 0x0EC7FE25FFF78166 /* 1643 */
	.quad	0x209CC8EB1CC1A99C, 0xFA4E422FC5DEFC94, 0x1D8FFC4BFFEF02CC /* 1644 */
	.quad	0x413991D639835339, 0xF49C845F8BBDF928, 0x3B1FF897FFDE0598 /* 1645 */
	.quad	0x827323AC7306A673, 0xE93908BF177BF250, 0x763FF12FFFBC0B30 /* 1646 */
	.quad	0x04E64758E60D4CE7, 0xD272117E2EF7E4A0, 0xEC7FE25FFF781660 /* 1647 */
	.quad	0x09CC8EB1CC1A99CF, 0xA4E422FC5DEFC941, 0xD8FFC4BFFEF02CC0 /* 1648 */
	.quad	0x13991D639835339F, 0x49C845F8BBDF9283, 0xB1FF897FFDE05980 /* 1649 */
	.quad	0x27323AC7306A673E, 0x93908BF177BF2507, 0x63FF12FFFBC0B301 /* 1650 */
	.quad	0x4E64758E60D4CE7D, 0x272117E2EF7E4A0E, 0xC7FE25FFF7816603 /* 1651 */
	.quad	0x9CC8EB1CC1A99CFA, 0x4E422FC5DEFC941D, 0x8FFC4BFFEF02CC07 /* 1652 */
	.quad	0x3991D639835339F4, 0x9C845F8BBDF9283B, 0x1FF897FFDE05980F /* 1653 */
	.quad	0x7323AC7306A673E9, 0x3908BF177BF25076, 0x3FF12FFFBC0B301F /* 1654 */
	.quad	0xE64758E60D4CE7D2, 0x72117E2EF7E4A0EC, 0x7FE25FFF7816603F /* 1655 */
	.quad	0xCC8EB1CC1A99CFA4, 0xE422FC5DEFC941D8, 0xFFC4BFFEF02CC07F /* 1656 */
	.quad	0x991D639835339F49, 0xC845F8BBDF9283B1, 0xFF897FFDE05980FE /* 1657 */
	.quad	0x323AC7306A673E93, 0x908BF177BF250763, 0xFF12FFFBC0B301FD /* 1658 */
	.quad	0x64758E60D4CE7D27, 0x2117E2EF7E4A0EC7, 0xFE25FFF7816603FB /* 1659 */
	.quad	0xC8EB1CC1A99CFA4E, 0x422FC5DEFC941D8F, 0xFC4BFFEF02CC07F7 /* 1660 */
	.quad	0x91D639835339F49C, 0x845F8BBDF9283B1F, 0xF897FFDE05980FEF /* 1661 */
	.quad	0x23AC7306A673E939, 0x08BF177BF250763F, 0xF12FFFBC0B301FDE /* 1662 */
	.quad	0x4758E60D4CE7D272, 0x117E2EF7E4A0EC7F, 0xE25FFF7816603FBC /* 1663 */
	.quad	0x8EB1CC1A99CFA4E4, 0x22FC5DEFC941D8FF, 0xC4BFFEF02CC07F79 /* 1664 */
	.quad	0x1D639835339F49C8, 0x45F8BBDF9283B1FF, 0x897FFDE05980FEF2 /* 1665 */
	.quad	0x3AC7306A673E9390, 0x8BF177BF250763FF, 0x12FFFBC0B301FDE5 /* 1666 */
	.quad	0x758E60D4CE7D2721, 0x17E2EF7E4A0EC7FE, 0x25FFF7816603FBCB /* 1667 */
	.quad	0xEB1CC1A99CFA4E42, 0x2FC5DEFC941D8FFC, 0x4BFFEF02CC07F797 /* 1668 */
	.quad	0xD639835339F49C84, 0x5F8BBDF9283B1FF8, 0x97FFDE05980FEF2F /* 1669 */
	.quad	0xAC7306A673E93908, 0xBF177BF250763FF1, 0x2FFFBC0B301FDE5E /* 1670 */
	.quad	0x58E60D4CE7D27211, 0x7E2EF7E4A0EC7FE2, 0x5FFF7816603FBCBC /* 1671 */
	.quad	0xB1CC1A99CFA4E422, 0xFC5DEFC941D8FFC4, 0xBFFEF02CC07F7978 /* 1672 */
	.quad	0x639835339F49C845, 0xF8BBDF9283B1FF89, 0x7FFDE05980FEF2F1 /* 1673 */
	.quad	0xC7306A673E93908B, 0xF177BF250763FF12, 0xFFFBC0B301FDE5E2 /* 1674 */
	.quad	0x8E60D4CE7D272117, 0xE2EF7E4A0EC7FE25, 0xFFF7816603FBCBC4 /* 1675 */
	.quad	0x1CC1A99CFA4E422F, 0xC5DEFC941D8FFC4B, 0xFFEF02CC07F79788 /* 1676 */
	.quad	0x39835339F49C845F, 0x8BBDF9283B1FF897, 0xFFDE05980FEF2F11 /* 1677 */
	.quad	0x7306A673E93908BF, 0x177BF250763FF12F, 0xFFBC0B301FDE5E23 /* 1678 */
	.quad	0xE60D4CE7D272117E, 0x2EF7E4A0EC7FE25F, 0xFF7816603FBCBC46 /* 1679 */
	.quad	0xCC1A99CFA4E422FC, 0x5DEFC941D8FFC4BF, 0xFEF02CC07F79788C /* 1680 */
	.quad	0x9835339F49C845F8, 0xBBDF9283B1FF897F, 0xFDE05980FEF2F118 /* 1681 */
	.quad	0x306A673E93908BF1, 0x77BF250763FF12FF, 0xFBC0B301FDE5E231 /* 1682 */
	.quad	0x60D4CE7D272117E2, 0xEF7E4A0EC7FE25FF, 0xF7816603FBCBC462 /* 1683 */
	.quad	0xC1A99CFA4E422FC5, 0xDEFC941D8FFC4BFF, 0xEF02CC07F79788C5 /* 1684 */
	.quad	0x835339F49C845F8B, 0xBDF9283B1FF897FF, 0xDE05980FEF2F118B /* 1685 */
	.quad	0x06A673E93908BF17, 0x7BF250763FF12FFF, 0xBC0B301FDE5E2316 /* 1686 */
	.quad	0x0D4CE7D272117E2E, 0xF7E4A0EC7FE25FFF, 0x7816603FBCBC462D /* 1687 */
	.quad	0x1A99CFA4E422FC5D, 0xEFC941D8FFC4BFFE, 0xF02CC07F79788C5A /* 1688 */
	.quad	0x35339F49C845F8BB, 0xDF9283B1FF897FFD, 0xE05980FEF2F118B5 /* 1689 */
	.quad	0x6A673E93908BF177, 0xBF250763FF12FFFB, 0xC0B301FDE5E2316B /* 1690 */
	.quad	0xD4CE7D272117E2EF, 0x7E4A0EC7FE25FFF7, 0x816603FBCBC462D6 /* 1691 */
	.quad	0xA99CFA4E422FC5DE, 0xFC941D8FFC4BFFEF, 0x02CC07F79788C5AD /* 1692 */
	.quad	0x5339F49C845F8BBD, 0xF9283B1FF897FFDE, 0x05980FEF2F118B5A /* 1693 */
	.quad	0xA673E93908BF177B, 0xF250763FF12FFFBC, 0x0B301FDE5E2316B4 /* 1694 */
	.quad	0x4CE7D272117E2EF7, 0xE4A0EC7FE25FFF78, 0x16603FBCBC462D68 /* 1695 */
	.quad	0x99CFA4E422FC5DEF, 0xC941D8FFC4BFFEF0, 0x2CC07F79788C5AD0 /* 1696 */
	.quad	0x339F49C845F8BBDF, 0x9283B1FF897FFDE0, 0x5980FEF2F118B5A0 /* 1697 */
	.quad	0x673E93908BF177BF, 0x250763FF12FFFBC0, 0xB301FDE5E2316B41 /* 1698 */
	.quad	0xCE7D272117E2EF7E, 0x4A0EC7FE25FFF781, 0x6603FBCBC462D682 /* 1699 */
	.quad	0x9CFA4E422FC5DEFC, 0x941D8FFC4BFFEF02, 0xCC07F79788C5AD05 /* 1700 */
	.quad	0x39F49C845F8BBDF9, 0x283B1FF897FFDE05, 0x980FEF2F118B5A0A /* 1701 */
	.quad	0x73E93908BF177BF2, 0x50763FF12FFFBC0B, 0x301FDE5E2316B414 /* 1702 */
	.quad	0xE7D272117E2EF7E4, 0xA0EC7FE25FFF7816, 0x603FBCBC462D6829 /* 1703 */
	.quad	0xCFA4E422FC5DEFC9, 0x41D8FFC4BFFEF02C, 0xC07F79788C5AD053 /* 1704 */
	.quad	0x9F49C845F8BBDF92, 0x83B1FF897FFDE059, 0x80FEF2F118B5A0A6 /* 1705 */
	.quad	0x3E93908BF177BF25, 0x0763FF12FFFBC0B3, 0x01FDE5E2316B414D /* 1706 */
	.quad	0x7D272117E2EF7E4A, 0x0EC7FE25FFF78166, 0x03FBCBC462D6829B /* 1707 */
	.quad	0xFA4E422FC5DEFC94, 0x1D8FFC4BFFEF02CC, 0x07F79788C5AD0536 /* 1708 */
	.quad	0xF49C845F8BBDF928, 0x3B1FF897FFDE0598, 0x0FEF2F118B5A0A6D /* 1709 */
	.quad	0xE93908BF177BF250, 0x763FF12FFFBC0B30, 0x1FDE5E2316B414DA /* 1710 */
	.quad	0xD272117E2EF7E4A0, 0xEC7FE25FFF781660, 0x3FBCBC462D6829B4 /* 1711 */
	.quad	0xA4E422FC5DEFC941, 0xD8FFC4BFFEF02CC0, 0x7F79788C5AD05368 /* 1712 */
	.quad	0x49C845F8BBDF9283, 0xB1FF897FFDE05980, 0xFEF2F118B5A0A6D1 /* 1713 */
	.quad	0x93908BF177BF2507, 0x63FF12FFFBC0B301, 0xFDE5E2316B414DA3 /* 1714 */
	.quad	0x272117E2EF7E4A0E, 0xC7FE25FFF7816603, 0xFBCBC462D6829B47 /* 1715 */
	.quad	0x4E422FC5DEFC941D, 0x8FFC4BFFEF02CC07, 0xF79788C5AD05368F /* 1716 */
	.quad	0x9C845F8BBDF9283B, 0x1FF897FFDE05980F, 0xEF2F118B5A0A6D1F /* 1717 */
	.quad	0x3908BF177BF25076, 0x3FF12FFFBC0B301F, 0xDE5E2316B414DA3E /* 1718 */
	.quad	0x72117E2EF7E4A0EC, 0x7FE25FFF7816603F, 0xBCBC462D6829B47D /* 1719 */
	.quad	0xE422FC5DEFC941D8, 0xFFC4BFFEF02CC07F, 0x79788C5AD05368FB /* 1720 */
	.quad	0xC845F8BBDF9283B1, 0xFF897FFDE05980FE, 0xF2F118B5A0A6D1F6 /* 1721 */
	.quad	0x908BF177BF250763, 0xFF12FFFBC0B301FD, 0xE5E2316B414DA3ED /* 1722 */
	.quad	0x2117E2EF7E4A0EC7, 0xFE25FFF7816603FB, 0xCBC462D6829B47DB /* 1723 */
	.quad	0x422FC5DEFC941D8F, 0xFC4BFFEF02CC07F7, 0x9788C5AD05368FB6 /* 1724 */
	.quad	0x845F8BBDF9283B1F, 0xF897FFDE05980FEF, 0x2F118B5A0A6D1F6D /* 1725 */
	.quad	0x08BF177BF250763F, 0xF12FFFBC0B301FDE, 0x5E2316B414DA3EDA /* 1726 */
	.quad	0x117E2EF7E4A0EC7F, 0xE25FFF7816603FBC, 0xBC462D6829B47DB4 /* 1727 */
	.quad	0x22FC5DEFC941D8FF, 0xC4BFFEF02CC07F79, 0x788C5AD05368FB69 /* 1728 */
	.quad	0x45F8BBDF9283B1FF, 0x897FFDE05980FEF2, 0xF118B5A0A6D1F6D3 /* 1729 */
	.quad	0x8BF177BF250763FF, 0x12FFFBC0B301FDE5, 0xE2316B414DA3EDA6 /* 1730 */
	.quad	0x17E2EF7E4A0EC7FE, 0x25FFF7816603FBCB, 0xC462D6829B47DB4D /* 1731 */
	.quad	0x2FC5DEFC941D8FFC, 0x4BFFEF02CC07F797, 0x88C5AD05368FB69B /* 1732 */
	.quad	0x5F8BBDF9283B1FF8, 0x97FFDE05980FEF2F, 0x118B5A0A6D1F6D36 /* 1733 */
	.quad	0xBF177BF250763FF1, 0x2FFFBC0B301FDE5E, 0x2316B414DA3EDA6C /* 1734 */
	.quad	0x7E2EF7E4A0EC7FE2, 0x5FFF7816603FBCBC, 0x462D6829B47DB4D9 /* 1735 */
	.quad	0xFC5DEFC941D8FFC4, 0xBFFEF02CC07F7978, 0x8C5AD05368FB69B3 /* 1736 */
	.quad	0xF8BBDF9283B1FF89, 0x7FFDE05980FEF2F1, 0x18B5A0A6D1F6D367 /* 1737 */
	.quad	0xF177BF250763FF12, 0xFFFBC0B301FDE5E2, 0x316B414DA3EDA6CF /* 1738 */
	.quad	0xE2EF7E4A0EC7FE25, 0xFFF7816603FBCBC4, 0x62D6829B47DB4D9F /* 1739 */
	.quad	0xC5DEFC941D8FFC4B, 0xFFEF02CC07F79788, 0xC5AD05368FB69B3F /* 1740 */
	.quad	0x8BBDF9283B1FF897, 0xFFDE05980FEF2F11, 0x8B5A0A6D1F6D367E /* 1741 */
	.quad	0x177BF250763FF12F, 0xFFBC0B301FDE5E23, 0x16B414DA3EDA6CFD /* 1742 */
	.quad	0x2EF7E4A0EC7FE25F, 0xFF7816603FBCBC46, 0x2D6829B47DB4D9FB /* 1743 */
	.quad	0x5DEFC941D8FFC4BF, 0xFEF02CC07F79788C, 0x5AD05368FB69B3F6 /* 1744 */
	.quad	0xBBDF9283B1FF897F, 0xFDE05980FEF2F118, 0xB5A0A6D1F6D367EC /* 1745 */
	.quad	0x77BF250763FF12FF, 0xFBC0B301FDE5E231, 0x6B414DA3EDA6CFD9 /* 1746 */
	.quad	0xEF7E4A0EC7FE25FF, 0xF7816603FBCBC462, 0xD6829B47DB4D9FB3 /* 1747 */
	.quad	0xDEFC941D8FFC4BFF, 0xEF02CC07F79788C5, 0xAD05368FB69B3F67 /* 1748 */
	.quad	0xBDF9283B1FF897FF, 0xDE05980FEF2F118B, 0x5A0A6D1F6D367ECF /* 1749 */
	.quad	0x7BF250763FF12FFF, 0xBC0B301FDE5E2316, 0xB414DA3EDA6CFD9E /* 1750 */
	.quad	0xF7E4A0EC7FE25FFF, 0x7816603FBCBC462D, 0x6829B47DB4D9FB3C /* 1751 */
	.quad	0xEFC941D8FFC4BFFE, 0xF02CC07F79788C5A, 0xD05368FB69B3F679 /* 1752 */
	.quad	0xDF9283B1FF897FFD, 0xE05980FEF2F118B5, 0xA0A6D1F6D367ECF2 /* 1753 */
	.quad	0xBF250763FF12FFFB, 0xC0B301FDE5E2316B, 0x414DA3EDA6CFD9E4 /* 1754 */
	.quad	0x7E4A0EC7FE25FFF7, 0x816603FBCBC462D6, 0x829B47DB4D9FB3C9 /* 1755 */
	.quad	0xFC941D8FFC4BFFEF, 0x02CC07F79788C5AD, 0x05368FB69B3F6793 /* 1756 */
	.quad	0xF9283B1FF897FFDE, 0x05980FEF2F118B5A, 0x0A6D1F6D367ECF27 /* 1757 */
	.quad	0xF250763FF12FFFBC, 0x0B301FDE5E2316B4, 0x14DA3EDA6CFD9E4F /* 1758 */
	.quad	0xE4A0EC7FE25FFF78, 0x16603FBCBC462D68, 0x29B47DB4D9FB3C9F /* 1759 */
	.quad	0xC941D8FFC4BFFEF0, 0x2CC07F79788C5AD0, 0x5368FB69B3F6793E /* 1760 */
	.quad	0x9283B1FF897FFDE0, 0x5980FEF2F118B5A0, 0xA6D1F6D367ECF27C /* 1761 */
	.quad	0x250763FF12FFFBC0, 0xB301FDE5E2316B41, 0x4DA3EDA6CFD9E4F9 /* 1762 */
	.quad	0x4A0EC7FE25FFF781, 0x6603FBCBC462D682, 0x9B47DB4D9FB3C9F2 /* 1763 */
	.quad	0x941D8FFC4BFFEF02, 0xCC07F79788C5AD05, 0x368FB69B3F6793E5 /* 1764 */
	.quad	0x283B1FF897FFDE05, 0x980FEF2F118B5A0A, 0x6D1F6D367ECF27CB /* 1765 */
	.quad	0x50763FF12FFFBC0B, 0x301FDE5E2316B414, 0xDA3EDA6CFD9E4F96 /* 1766 */
	.quad	0xA0EC7FE25FFF7816, 0x603FBCBC462D6829, 0xB47DB4D9FB3C9F2C /* 1767 */
	.quad	0x41D8FFC4BFFEF02C, 0xC07F79788C5AD053, 0x68FB69B3F6793E58 /* 1768 */
	.quad	0x83B1FF897FFDE059, 0x80FEF2F118B5A0A6, 0xD1F6D367ECF27CB0 /* 1769 */
	.quad	0x0763FF12FFFBC0B3, 0x01FDE5E2316B414D, 0xA3EDA6CFD9E4F961 /* 1770 */
	.quad	0x0EC7FE25FFF78166, 0x03FBCBC462D6829B, 0x47DB4D9FB3C9F2C2 /* 1771 */
	.quad	0x1D8FFC4BFFEF02CC, 0x07F79788C5AD0536, 0x8FB69B3F6793E584 /* 1772 */
	.quad	0x3B1FF897FFDE0598, 0x0FEF2F118B5A0A6D, 0x1F6D367ECF27CB09 /* 1773 */
	.quad	0x763FF12FFFBC0B30, 0x1FDE5E2316B414DA, 0x3EDA6CFD9E4F9613 /* 1774 */
	.quad	0xEC7FE25FFF781660, 0x3FBCBC462D6829B4, 0x7DB4D9FB3C9F2C26 /* 1775 */
	.quad	0xD8FFC4BFFEF02CC0, 0x7F79788C5AD05368, 0xFB69B3F6793E584D /* 1776 */
	.quad	0xB1FF897FFDE05980, 0xFEF2F118B5A0A6D1, 0xF6D367ECF27CB09B /* 1777 */
	.quad	0x63FF12FFFBC0B301, 0xFDE5E2316B414DA3, 0xEDA6CFD9E4F96136 /* 1778 */
	.quad	0xC7FE25FFF7816603, 0xFBCBC462D6829B47, 0xDB4D9FB3C9F2C26D /* 1779 */
	.quad	0x8FFC4BFFEF02CC07, 0xF79788C5AD05368F, 0xB69B3F6793E584DB /* 1780 */
	.quad	0x1FF897FFDE05980F, 0xEF2F118B5A0A6D1F, 0x6D367ECF27CB09B7 /* 1781 */
	.quad	0x3FF12FFFBC0B301F, 0xDE5E2316B414DA3E, 0xDA6CFD9E4F96136E /* 1782 */
	.quad	0x7FE25FFF7816603F, 0xBCBC462D6829B47D, 0xB4D9FB3C9F2C26DD /* 1783 */
	.quad	0xFFC4BFFEF02CC07F, 0x79788C5AD05368FB, 0x69B3F6793E584DBA /* 1784 */
	.quad	0xFF897FFDE05980FE, 0xF2F118B5A0A6D1F6, 0xD367ECF27CB09B74 /* 1785 */
	.quad	0xFF12FFFBC0B301FD, 0xE5E2316B414DA3ED, 0xA6CFD9E4F96136E9 /* 1786 */
	.quad	0xFE25FFF7816603FB, 0xCBC462D6829B47DB, 0x4D9FB3C9F2C26DD3 /* 1787 */
	.quad	0xFC4BFFEF02CC07F7, 0x9788C5AD05368FB6, 0x9B3F6793E584DBA7 /* 1788 */
	.quad	0xF897FFDE05980FEF, 0x2F118B5A0A6D1F6D, 0x367ECF27CB09B74F /* 1789 */
	.quad	0xF12FFFBC0B301FDE, 0x5E2316B414DA3EDA, 0x6CFD9E4F96136E9E /* 1790 */
	.quad	0xE25FFF7816603FBC, 0xBC462D6829B47DB4, 0xD9FB3C9F2C26DD3D /* 1791 */
	.quad	0xC4BFFEF02CC07F79, 0x788C5AD05368FB69, 0xB3F6793E584DBA7A /* 1792 */
	.quad	0x897FFDE05980FEF2, 0xF118B5A0A6D1F6D3, 0x67ECF27CB09B74F4 /* 1793 */
	.quad	0x12FFFBC0B301FDE5, 0xE2316B414DA3EDA6, 0xCFD9E4F96136E9E8 /* 1794 */
	.quad	0x25FFF7816603FBCB, 0xC462D6829B47DB4D, 0x9FB3C9F2C26DD3D1 /* 1795 */
	.quad	0x4BFFEF02CC07F797, 0x88C5AD05368FB69B, 0x3F6793E584DBA7A3 /* 1796 */
	.quad	0x97FFDE05980FEF2F, 0x118B5A0A6D1F6D36, 0x7ECF27CB09B74F46 /* 1797 */
	.quad	0x2FFFBC0B301FDE5E, 0x2316B414DA3EDA6C, 0xFD9E4F96136E9E8C /* 1798 */
	.quad	0x5FFF7816603FBCBC, 0x462D6829B47DB4D9, 0xFB3C9F2C26DD3D18 /* 1799 */
	.quad	0xBFFEF02CC07F7978, 0x8C5AD05368FB69B3, 0xF6793E584DBA7A31 /* 1800 */
	.quad	0x7FFDE05980FEF2F1, 0x18B5A0A6D1F6D367, 0xECF27CB09B74F463 /* 1801 */
	.quad	0xFFFBC0B301FDE5E2, 0x316B414DA3EDA6CF, 0xD9E4F96136E9E8C7 /* 1802 */
	.quad	0xFFF7816603FBCBC4, 0x62D6829B47DB4D9F, 0xB3C9F2C26DD3D18F /* 1803 */
	.quad	0xFFEF02CC07F79788, 0xC5AD05368FB69B3F, 0x6793E584DBA7A31F /* 1804 */
	.quad	0xFFDE05980FEF2F11, 0x8B5A0A6D1F6D367E, 0xCF27CB09B74F463F /* 1805 */
	.quad	0xFFBC0B301FDE5E23, 0x16B414DA3EDA6CFD, 0x9E4F96136E9E8C7E /* 1806 */
	.quad	0xFF7816603FBCBC46, 0x2D6829B47DB4D9FB, 0x3C9F2C26DD3D18FD /* 1807 */
	.quad	0xFEF02CC07F79788C, 0x5AD05368FB69B3F6, 0x793E584DBA7A31FB /* 1808 */
	.quad	0xFDE05980FEF2F118, 0xB5A0A6D1F6D367EC, 0xF27CB09B74F463F6 /* 1809 */
	.quad	0xFBC0B301FDE5E231, 0x6B414DA3EDA6CFD9, 0xE4F96136E9E8C7EC /* 1810 */
	.quad	0xF7816603FBCBC462, 0xD6829B47DB4D9FB3, 0xC9F2C26DD3D18FD9 /* 1811 */
	.quad	0xEF02CC07F79788C5, 0xAD05368FB69B3F67, 0x93E584DBA7A31FB3 /* 1812 */
	.quad	0xDE05980FEF2F118B, 0x5A0A6D1F6D367ECF, 0x27CB09B74F463F66 /* 1813 */
	.quad	0xBC0B301FDE5E2316, 0xB414DA3EDA6CFD9E, 0x4F96136E9E8C7ECD /* 1814 */
	.quad	0x7816603FBCBC462D, 0x6829B47DB4D9FB3C, 0x9F2C26DD3D18FD9A /* 1815 */
	.quad	0xF02CC07F79788C5A, 0xD05368FB69B3F679, 0x3E584DBA7A31FB34 /* 1816 */
	.quad	0xE05980FEF2F118B5, 0xA0A6D1F6D367ECF2, 0x7CB09B74F463F669 /* 1817 */
	.quad	0xC0B301FDE5E2316B, 0x414DA3EDA6CFD9E4, 0xF96136E9E8C7ECD3 /* 1818 */
	.quad	0x816603FBCBC462D6, 0x829B47DB4D9FB3C9, 0xF2C26DD3D18FD9A7 /* 1819 */
	.quad	0x02CC07F79788C5AD, 0x05368FB69B3F6793, 0xE584DBA7A31FB34F /* 1820 */
	.quad	0x05980FEF2F118B5A, 0x0A6D1F6D367ECF27, 0xCB09B74F463F669E /* 1821 */
	.quad	0x0B301FDE5E2316B4, 0x14DA3EDA6CFD9E4F, 0x96136E9E8C7ECD3C /* 1822 */
	.quad	0x16603FBCBC462D68, 0x29B47DB4D9FB3C9F, 0x2C26DD3D18FD9A79 /* 1823 */
	.quad	0x2CC07F79788C5AD0, 0x5368FB69B3F6793E, 0x584DBA7A31FB34F2 /* 1824 */
	.quad	0x5980FEF2F118B5A0, 0xA6D1F6D367ECF27C, 0xB09B74F463F669E5 /* 1825 */
	.quad	0xB301FDE5E2316B41, 0x4DA3EDA6CFD9E4F9, 0x6136E9E8C7ECD3CB /* 1826 */
	.quad	0x6603FBCBC462D682, 0x9B47DB4D9FB3C9F2, 0xC26DD3D18FD9A797 /* 1827 */
	.quad	0xCC07F79788C5AD05, 0x368FB69B3F6793E5, 0x84DBA7A31FB34F2F /* 1828 */
	.quad	0x980FEF2F118B5A0A, 0x6D1F6D367ECF27CB, 0x09B74F463F669E5F /* 1829 */
	.quad	0x301FDE5E2316B414, 0xDA3EDA6CFD9E4F96, 0x136E9E8C7ECD3CBF /* 1830 */
	.quad	0x603FBCBC462D6829, 0xB47DB4D9FB3C9F2C, 0x26DD3D18FD9A797F /* 1831 */
	.quad	0xC07F79788C5AD053, 0x68FB69B3F6793E58, 0x4DBA7A31FB34F2FF /* 1832 */
	.quad	0x80FEF2F118B5A0A6, 0xD1F6D367ECF27CB0, 0x9B74F463F669E5FE /* 1833 */
	.quad	0x01FDE5E2316B414D, 0xA3EDA6CFD9E4F961, 0x36E9E8C7ECD3CBFD /* 1834 */
	.quad	0x03FBCBC462D6829B, 0x47DB4D9FB3C9F2C2, 0x6DD3D18FD9A797FA /* 1835 */
	.quad	0x07F79788C5AD0536, 0x8FB69B3F6793E584, 0xDBA7A31FB34F2FF5 /* 1836 */
	.quad	0x0FEF2F118B5A0A6D, 0x1F6D367ECF27CB09, 0xB74F463F669E5FEA /* 1837 */
	.quad	0x1FDE5E2316B414DA, 0x3EDA6CFD9E4F9613, 0x6E9E8C7ECD3CBFD4 /* 1838 */
	.quad	0x3FBCBC462D6829B4, 0x7DB4D9FB3C9F2C26, 0xDD3D18FD9A797FA8 /* 1839 */
	.quad	0x7F79788C5AD05368, 0xFB69B3F6793E584D, 0xBA7A31FB34F2FF51 /* 1840 */
	.quad	0xFEF2F118B5A0A6D1, 0xF6D367ECF27CB09B, 0x74F463F669E5FEA2 /* 1841 */
	.quad	0xFDE5E2316B414DA3, 0xEDA6CFD9E4F96136, 0xE9E8C7ECD3CBFD45 /* 1842 */
	.quad	0xFBCBC462D6829B47, 0xDB4D9FB3C9F2C26D, 0xD3D18FD9A797FA8B /* 1843 */
	.quad	0xF79788C5AD05368F, 0xB69B3F6793E584DB, 0xA7A31FB34F2FF516 /* 1844 */
	.quad	0xEF2F118B5A0A6D1F, 0x6D367ECF27CB09B7, 0x4F463F669E5FEA2D /* 1845 */
	.quad	0xDE5E2316B414DA3E, 0xDA6CFD9E4F96136E, 0x9E8C7ECD3CBFD45A /* 1846 */
	.quad	0xBCBC462D6829B47D, 0xB4D9FB3C9F2C26DD, 0x3D18FD9A797FA8B5 /* 1847 */
	.quad	0x79788C5AD05368FB, 0x69B3F6793E584DBA, 0x7A31FB34F2FF516B /* 1848 */
	.quad	0xF2F118B5A0A6D1F6, 0xD367ECF27CB09B74, 0xF463F669E5FEA2D7 /* 1849 */
	.quad	0xE5E2316B414DA3ED, 0xA6CFD9E4F96136E9, 0xE8C7ECD3CBFD45AE /* 1850 */
	.quad	0xCBC462D6829B47DB, 0x4D9FB3C9F2C26DD3, 0xD18FD9A797FA8B5D /* 1851 */
	.quad	0x9788C5AD05368FB6, 0x9B3F6793E584DBA7, 0xA31FB34F2FF516BA /* 1852 */
	.quad	0x2F118B5A0A6D1F6D, 0x367ECF27CB09B74F, 0x463F669E5FEA2D75 /* 1853 */
	.quad	0x5E2316B414DA3EDA, 0x6CFD9E4F96136E9E, 0x8C7ECD3CBFD45AEA /* 1854 */
	.quad	0xBC462D6829B47DB4, 0xD9FB3C9F2C26DD3D, 0x18FD9A797FA8B5D4 /* 1855 */
	.quad	0x788C5AD05368FB69, 0xB3F6793E584DBA7A, 0x31FB34F2FF516BA9 /* 1856 */
	.quad	0xF118B5A0A6D1F6D3, 0x67ECF27CB09B74F4, 0x63F669E5FEA2D752 /* 1857 */
	.quad	0xE2316B414DA3EDA6, 0xCFD9E4F96136E9E8, 0xC7ECD3CBFD45AEA4 /* 1858 */
	.quad	0xC462D6829B47DB4D, 0x9FB3C9F2C26DD3D1, 0x8FD9A797FA8B5D49 /* 1859 */
	.quad	0x88C5AD05368FB69B, 0x3F6793E584DBA7A3, 0x1FB34F2FF516BA93 /* 1860 */
	.quad	0x118B5A0A6D1F6D36, 0x7ECF27CB09B74F46, 0x3F669E5FEA2D7527 /* 1861 */
	.quad	0x2316B414DA3EDA6C, 0xFD9E4F96136E9E8C, 0x7ECD3CBFD45AEA4F /* 1862 */
	.quad	0x462D6829B47DB4D9, 0xFB3C9F2C26DD3D18, 0xFD9A797FA8B5D49E /* 1863 */
	.quad	0x8C5AD05368FB69B3, 0xF6793E584DBA7A31, 0xFB34F2FF516BA93D /* 1864 */
	.quad	0x18B5A0A6D1F6D367, 0xECF27CB09B74F463, 0xF669E5FEA2D7527B /* 1865 */
	.quad	0x316B414DA3EDA6CF, 0xD9E4F96136E9E8C7, 0xECD3CBFD45AEA4F7 /* 1866 */
	.quad	0x62D6829B47DB4D9F, 0xB3C9F2C26DD3D18F, 0xD9A797FA8B5D49EE /* 1867 */
	.quad	0xC5AD05368FB69B3F, 0x6793E584DBA7A31F, 0xB34F2FF516BA93DD /* 1868 */
	.quad	0x8B5A0A6D1F6D367E, 0xCF27CB09B74F463F, 0x669E5FEA2D7527BA /* 1869 */
	.quad	0x16B414DA3EDA6CFD, 0x9E4F96136E9E8C7E, 0xCD3CBFD45AEA4F75 /* 1870 */
	.quad	0x2D6829B47DB4D9FB, 0x3C9F2C26DD3D18FD, 0x9A797FA8B5D49EEB /* 1871 */
	.quad	0x5AD05368FB69B3F6, 0x793E584DBA7A31FB, 0x34F2FF516BA93DD6 /* 1872 */
	.quad	0xB5A0A6D1F6D367EC, 0xF27CB09B74F463F6, 0x69E5FEA2D7527BAC /* 1873 */
	.quad	0x6B414DA3EDA6CFD9, 0xE4F96136E9E8C7EC, 0xD3CBFD45AEA4F758 /* 1874 */
	.quad	0xD6829B47DB4D9FB3, 0xC9F2C26DD3D18FD9, 0xA797FA8B5D49EEB1 /* 1875 */
	.quad	0xAD05368FB69B3F67, 0x93E584DBA7A31FB3, 0x4F2FF516BA93DD63 /* 1876 */
	.quad	0x5A0A6D1F6D367ECF, 0x27CB09B74F463F66, 0x9E5FEA2D7527BAC7 /* 1877 */
	.quad	0xB414DA3EDA6CFD9E, 0x4F96136E9E8C7ECD, 0x3CBFD45AEA4F758F /* 1878 */
	.quad	0x6829B47DB4D9FB3C, 0x9F2C26DD3D18FD9A, 0x797FA8B5D49EEB1F /* 1879 */
	.quad	0xD05368FB69B3F679, 0x3E584DBA7A31FB34, 0xF2FF516BA93DD63F /* 1880 */
	.quad	0xA0A6D1F6D367ECF2, 0x7CB09B74F463F669, 0xE5FEA2D7527BAC7E /* 1881 */
	.quad	0x414DA3EDA6CFD9E4, 0xF96136E9E8C7ECD3, 0xCBFD45AEA4F758FD /* 1882 */
	.quad	0x829B47DB4D9FB3C9, 0xF2C26DD3D18FD9A7, 0x97FA8B5D49EEB1FA /* 1883 */
	.quad	0x05368FB69B3F6793, 0xE584DBA7A31FB34F, 0x2FF516BA93DD63F5 /* 1884 */
	.quad	0x0A6D1F6D367ECF27, 0xCB09B74F463F669E, 0x5FEA2D7527BAC7EB /* 1885 */
	.quad	0x14DA3EDA6CFD9E4F, 0x96136E9E8C7ECD3C, 0xBFD45AEA4F758FD7 /* 1886 */
	.quad	0x29B47DB4D9FB3C9F, 0x2C26DD3D18FD9A79, 0x7FA8B5D49EEB1FAF /* 1887 */
	.quad	0x5368FB69B3F6793E, 0x584DBA7A31FB34F2, 0xFF516BA93DD63F5F /* 1888 */
	.quad	0xA6D1F6D367ECF27C, 0xB09B74F463F669E5, 0xFEA2D7527BAC7EBE /* 1889 */
	.quad	0x4DA3EDA6CFD9E4F9, 0x6136E9E8C7ECD3CB, 0xFD45AEA4F758FD7C /* 1890 */
	.quad	0x9B47DB4D9FB3C9F2, 0xC26DD3D18FD9A797, 0xFA8B5D49EEB1FAF9 /* 1891 */
	.quad	0x368FB69B3F6793E5, 0x84DBA7A31FB34F2F, 0xF516BA93DD63F5F2 /* 1892 */
	.quad	0x6D1F6D367ECF27CB, 0x09B74F463F669E5F, 0xEA2D7527BAC7EBE5 /* 1893 */
	.quad	0xDA3EDA6CFD9E4F96, 0x136E9E8C7ECD3CBF, 0xD45AEA4F758FD7CB /* 1894 */
	.quad	0xB47DB4D9FB3C9F2C, 0x26DD3D18FD9A797F, 0xA8B5D49EEB1FAF97 /* 1895 */
	.quad	0x68FB69B3F6793E58, 0x4DBA7A31FB34F2FF, 0x516BA93DD63F5F2F /* 1896 */
	.quad	0xD1F6D367ECF27CB0, 0x9B74F463F669E5FE, 0xA2D7527BAC7EBE5F /* 1897 */
	.quad	0xA3EDA6CFD9E4F961, 0x36E9E8C7ECD3CBFD, 0x45AEA4F758FD7CBE /* 1898 */
	.quad	0x47DB4D9FB3C9F2C2, 0x6DD3D18FD9A797FA, 0x8B5D49EEB1FAF97C /* 1899 */
	.quad	0x8FB69B3F6793E584, 0xDBA7A31FB34F2FF5, 0x16BA93DD63F5F2F8 /* 1900 */
	.quad	0x1F6D367ECF27CB09, 0xB74F463F669E5FEA, 0x2D7527BAC7EBE5F1 /* 1901 */
	.quad	0x3EDA6CFD9E4F9613, 0x6E9E8C7ECD3CBFD4, 0x5AEA4F758FD7CBE2 /* 1902 */
	.quad	0x7DB4D9FB3C9F2C26, 0xDD3D18FD9A797FA8, 0xB5D49EEB1FAF97C5 /* 1903 */
	.quad	0xFB69B3F6793E584D, 0xBA7A31FB34F2FF51, 0x6BA93DD63F5F2F8B /* 1904 */
	.quad	0xF6D367ECF27CB09B, 0x74F463F669E5FEA2, 0xD7527BAC7EBE5F17 /* 1905 */
	.quad	0xEDA6CFD9E4F96136, 0xE9E8C7ECD3CBFD45, 0xAEA4F758FD7CBE2F /* 1906 */
	.quad	0xDB4D9FB3C9F2C26D, 0xD3D18FD9A797FA8B, 0x5D49EEB1FAF97C5E /* 1907 */
	.quad	0xB69B3F6793E584DB, 0xA7A31FB34F2FF516, 0xBA93DD63F5F2F8BD /* 1908 */
	.quad	0x6D367ECF27CB09B7, 0x4F463F669E5FEA2D, 0x7527BAC7EBE5F17B /* 1909 */
	.quad	0xDA6CFD9E4F96136E, 0x9E8C7ECD3CBFD45A, 0xEA4F758FD7CBE2F6 /* 1910 */
	.quad	0xB4D9FB3C9F2C26DD, 0x3D18FD9A797FA8B5, 0xD49EEB1FAF97C5EC /* 1911 */
	.quad	0x69B3F6793E584DBA, 0x7A31FB34F2FF516B, 0xA93DD63F5F2F8BD9 /* 1912 */
	.quad	0xD367ECF27CB09B74, 0xF463F669E5FEA2D7, 0x527BAC7EBE5F17B3 /* 1913 */
	.quad	0xA6CFD9E4F96136E9, 0xE8C7ECD3CBFD45AE, 0xA4F758FD7CBE2F67 /* 1914 */
	.quad	0x4D9FB3C9F2C26DD3, 0xD18FD9A797FA8B5D, 0x49EEB1FAF97C5ECF /* 1915 */
	.quad	0x9B3F6793E584DBA7, 0xA31FB34F2FF516BA, 0x93DD63F5F2F8BD9E /* 1916 */
	.quad	0x367ECF27CB09B74F, 0x463F669E5FEA2D75, 0x27BAC7EBE5F17B3D /* 1917 */
	.quad	0x6CFD9E4F96136E9E, 0x8C7ECD3CBFD45AEA, 0x4F758FD7CBE2F67A /* 1918 */
	.quad	0xD9FB3C9F2C26DD3D, 0x18FD9A797FA8B5D4, 0x9EEB1FAF97C5ECF4 /* 1919 */
	.quad	0xB3F6793E584DBA7A, 0x31FB34F2FF516BA9, 0x3DD63F5F2F8BD9E8 /* 1920 */
	.quad	0x67ECF27CB09B74F4, 0x63F669E5FEA2D752, 0x7BAC7EBE5F17B3D0 /* 1921 */
	.quad	0xCFD9E4F96136E9E8, 0xC7ECD3CBFD45AEA4, 0xF758FD7CBE2F67A0 /* 1922 */
	.quad	0x9FB3C9F2C26DD3D1, 0x8FD9A797FA8B5D49, 0xEEB1FAF97C5ECF41 /* 1923 */
	.quad	0x3F6793E584DBA7A3, 0x1FB34F2FF516BA93, 0xDD63F5F2F8BD9E83 /* 1924 */
	.quad	0x7ECF27CB09B74F46, 0x3F669E5FEA2D7527, 0xBAC7EBE5F17B3D07 /* 1925 */
	.quad	0xFD9E4F96136E9E8C, 0x7ECD3CBFD45AEA4F, 0x758FD7CBE2F67A0E /* 1926 */
	.quad	0xFB3C9F2C26DD3D18, 0xFD9A797FA8B5D49E, 0xEB1FAF97C5ECF41C /* 1927 */
	.quad	0xF6793E584DBA7A31, 0xFB34F2FF516BA93D, 0xD63F5F2F8BD9E839 /* 1928 */
	.quad	0xECF27CB09B74F463, 0xF669E5FEA2D7527B, 0xAC7EBE5F17B3D073 /* 1929 */
	.quad	0xD9E4F96136E9E8C7, 0xECD3CBFD45AEA4F7, 0x58FD7CBE2F67A0E7 /* 1930 */
	.quad	0xB3C9F2C26DD3D18F, 0xD9A797FA8B5D49EE, 0xB1FAF97C5ECF41CE /* 1931 */
	.quad	0x6793E584DBA7A31F, 0xB34F2FF516BA93DD, 0x63F5F2F8BD9E839C /* 1932 */
	.quad	0xCF27CB09B74F463F, 0x669E5FEA2D7527BA, 0xC7EBE5F17B3D0739 /* 1933 */
	.quad	0x9E4F96136E9E8C7E, 0xCD3CBFD45AEA4F75, 0x8FD7CBE2F67A0E73 /* 1934 */
	.quad	0x3C9F2C26DD3D18FD, 0x9A797FA8B5D49EEB, 0x1FAF97C5ECF41CE7 /* 1935 */
	.quad	0x793E584DBA7A31FB, 0x34F2FF516BA93DD6, 0x3F5F2F8BD9E839CF /* 1936 */
	.quad	0xF27CB09B74F463F6, 0x69E5FEA2D7527BAC, 0x7EBE5F17B3D0739F /* 1937 */
	.quad	0xE4F96136E9E8C7EC, 0xD3CBFD45AEA4F758, 0xFD7CBE2F67A0E73E /* 1938 */
	.quad	0xC9F2C26DD3D18FD9, 0xA797FA8B5D49EEB1, 0xFAF97C5ECF41CE7D /* 1939 */
	.quad	0x93E584DBA7A31FB3, 0x4F2FF516BA93DD63, 0xF5F2F8BD9E839CFB /* 1940 */
	.quad	0x27CB09B74F463F66, 0x9E5FEA2D7527BAC7, 0xEBE5F17B3D0739F7 /* 1941 */
	.quad	0x4F96136E9E8C7ECD, 0x3CBFD45AEA4F758F, 0xD7CBE2F67A0E73EF /* 1942 */
	.quad	0x9F2C26DD3D18FD9A, 0x797FA8B5D49EEB1F, 0xAF97C5ECF41CE7DE /* 1943 */
	.quad	0x3E584DBA7A31FB34, 0xF2FF516BA93DD63F, 0x5F2F8BD9E839CFBC /* 1944 */
	.quad	0x7CB09B74F463F669, 0xE5FEA2D7527BAC7E, 0xBE5F17B3D0739F78 /* 1945 */
	.quad	0xF96136E9E8C7ECD3, 0xCBFD45AEA4F758FD, 0x7CBE2F67A0E73EF1 /* 1946 */
	.quad	0xF2C26DD3D18FD9A7, 0x97FA8B5D49EEB1FA, 0xF97C5ECF41CE7DE2 /* 1947 */
	.quad	0xE584DBA7A31FB34F, 0x2FF516BA93DD63F5, 0xF2F8BD9E839CFBC5 /* 1948 */
	.quad	0xCB09B74F463F669E, 0x5FEA2D7527BAC7EB, 0xE5F17B3D0739F78A /* 1949 */
	.quad	0x96136E9E8C7ECD3C, 0xBFD45AEA4F758FD7, 0xCBE2F67A0E73EF14 /* 1950 */
	.quad	0x2C26DD3D18FD9A79, 0x7FA8B5D49EEB1FAF, 0x97C5ECF41CE7DE29 /* 1951 */
	.quad	0x584DBA7A31FB34F2, 0xFF516BA93DD63F5F, 0x2F8BD9E839CFBC52 /* 1952 */
	.quad	0xB09B74F463F669E5, 0xFEA2D7527BAC7EBE, 0x5F17B3D0739F78A5 /* 1953 */
	.quad	0x6136E9E8C7ECD3CB, 0xFD45AEA4F758FD7C, 0xBE2F67A0E73EF14A /* 1954 */
	.quad	0xC26DD3D18FD9A797, 0xFA8B5D49EEB1FAF9, 0x7C5ECF41CE7DE294 /* 1955 */
	.quad	0x84DBA7A31FB34F2F, 0xF516BA93DD63F5F2, 0xF8BD9E839CFBC529 /* 1956 */
	.quad	0x09B74F463F669E5F, 0xEA2D7527BAC7EBE5, 0xF17B3D0739F78A52 /* 1957 */
	.quad	0x136E9E8C7ECD3CBF, 0xD45AEA4F758FD7CB, 0xE2F67A0E73EF14A5 /* 1958 */
	.quad	0x26DD3D18FD9A797F, 0xA8B5D49EEB1FAF97, 0xC5ECF41CE7DE294A /* 1959 */
	.quad	0x4DBA7A31FB34F2FF, 0x516BA93DD63F5F2F, 0x8BD9E839CFBC5294 /* 1960 */
	.quad	0x9B74F463F669E5FE, 0xA2D7527BAC7EBE5F, 0x17B3D0739F78A529 /* 1961 */
	.quad	0x36E9E8C7ECD3CBFD, 0x45AEA4F758FD7CBE, 0x2F67A0E73EF14A52 /* 1962 */
	.quad	0x6DD3D18FD9A797FA, 0x8B5D49EEB1FAF97C, 0x5ECF41CE7DE294A4 /* 1963 */
	.quad	0xDBA7A31FB34F2FF5, 0x16BA93DD63F5F2F8, 0xBD9E839CFBC52949 /* 1964 */
	.quad	0xB74F463F669E5FEA, 0x2D7527BAC7EBE5F1, 0x7B3D0739F78A5292 /* 1965 */
	.quad	0x6E9E8C7ECD3CBFD4, 0x5AEA4F758FD7CBE2, 0xF67A0E73EF14A525 /* 1966 */
	.quad	0xDD3D18FD9A797FA8, 0xB5D49EEB1FAF97C5, 0xECF41CE7DE294A4B /* 1967 */
	.quad	0xBA7A31FB34F2FF51, 0x6BA93DD63F5F2F8B, 0xD9E839CFBC529497 /* 1968 */
	.quad	0x74F463F669E5FEA2, 0xD7527BAC7EBE5F17, 0xB3D0739F78A5292E /* 1969 */
	.quad	0xE9E8C7ECD3CBFD45, 0xAEA4F758FD7CBE2F, 0x67A0E73EF14A525D /* 1970 */
	.quad	0xD3D18FD9A797FA8B, 0x5D49EEB1FAF97C5E, 0xCF41CE7DE294A4BA /* 1971 */
	.quad	0xA7A31FB34F2FF516, 0xBA93DD63F5F2F8BD, 0x9E839CFBC5294975 /* 1972 */
	.quad	0x4F463F669E5FEA2D, 0x7527BAC7EBE5F17B, 0x3D0739F78A5292EA /* 1973 */
	.quad	0x9E8C7ECD3CBFD45A, 0xEA4F758FD7CBE2F6, 0x7A0E73EF14A525D4 /* 1974 */
	.quad	0x3D18FD9A797FA8B5, 0xD49EEB1FAF97C5EC, 0xF41CE7DE294A4BA9 /* 1975 */
	.quad	0x7A31FB34F2FF516B, 0xA93DD63F5F2F8BD9, 0xE839CFBC52949753 /* 1976 */
	.quad	0xF463F669E5FEA2D7, 0x527BAC7EBE5F17B3, 0xD0739F78A5292EA6 /* 1977 */
	.quad	0xE8C7ECD3CBFD45AE, 0xA4F758FD7CBE2F67, 0xA0E73EF14A525D4D /* 1978 */
	.quad	0xD18FD9A797FA8B5D, 0x49EEB1FAF97C5ECF, 0x41CE7DE294A4BA9A /* 1979 */
	.quad	0xA31FB34F2FF516BA, 0x93DD63F5F2F8BD9E, 0x839CFBC529497535 /* 1980 */
	.quad	0x463F669E5FEA2D75, 0x27BAC7EBE5F17B3D, 0x0739F78A5292EA6B /* 1981 */
	.quad	0x8C7ECD3CBFD45AEA, 0x4F758FD7CBE2F67A, 0x0E73EF14A525D4D7 /* 1982 */
	.quad	0x18FD9A797FA8B5D4, 0x9EEB1FAF97C5ECF4, 0x1CE7DE294A4BA9AF /* 1983 */
	.quad	0x31FB34F2FF516BA9, 0x3DD63F5F2F8BD9E8, 0x39CFBC529497535F /* 1984 */
	.quad	0x63F669E5FEA2D752, 0x7BAC7EBE5F17B3D0, 0x739F78A5292EA6BF /* 1985 */
	.quad	0xC7ECD3CBFD45AEA4, 0xF758FD7CBE2F67A0, 0xE73EF14A525D4D7F /* 1986 */
	.quad	0x8FD9A797FA8B5D49, 0xEEB1FAF97C5ECF41, 0xCE7DE294A4BA9AFE /* 1987 */
	.quad	0x1FB34F2FF516BA93, 0xDD63F5F2F8BD9E83, 0x9CFBC529497535FD /* 1988 */
	.quad	0x3F669E5FEA2D7527, 0xBAC7EBE5F17B3D07, 0x39F78A5292EA6BFB /* 1989 */
	.quad	0x7ECD3CBFD45AEA4F, 0x758FD7CBE2F67A0E, 0x73EF14A525D4D7F6 /* 1990 */
	.quad	0xFD9A797FA8B5D49E, 0xEB1FAF97C5ECF41C, 0xE7DE294A4BA9AFED /* 1991 */
	.quad	0xFB34F2FF516BA93D, 0xD63F5F2F8BD9E839, 0xCFBC529497535FDA /* 1992 */
	.quad	0xF669E5FEA2D7527B, 0xAC7EBE5F17B3D073, 0x9F78A5292EA6BFB5 /* 1993 */
	.quad	0xECD3CBFD45AEA4F7, 0x58FD7CBE2F67A0E7, 0x3EF14A525D4D7F6B /* 1994 */
	.quad	0xD9A797FA8B5D49EE, 0xB1FAF97C5ECF41CE, 0x7DE294A4BA9AFED7 /* 1995 */
	.quad	0xB34F2FF516BA93DD, 0x63F5F2F8BD9E839C, 0xFBC529497535FDAF /* 1996 */
	.quad	0x669E5FEA2D7527BA, 0xC7EBE5F17B3D0739, 0xF78A5292EA6BFB5F /* 1997 */
	.quad	0xCD3CBFD45AEA4F75, 0x8FD7CBE2F67A0E73, 0xEF14A525D4D7F6BF /* 1998 */
	.quad	0x9A797FA8B5D49EEB, 0x1FAF97C5ECF41CE7, 0xDE294A4BA9AFED7E /* 1999 */
	.quad	0x34F2FF516BA93DD6, 0x3F5F2F8BD9E839CF, 0xBC529497535FDAFD /* 2000 */
	.quad	0x69E5FEA2D7527BAC, 0x7EBE5F17B3D0739F, 0x78A5292EA6BFB5FB /* 2001 */
	.quad	0xD3CBFD45AEA4F758, 0xFD7CBE2F67A0E73E, 0xF14A525D4D7F6BF6 /* 2002 */
	.quad	0xA797FA8B5D49EEB1, 0xFAF97C5ECF41CE7D, 0xE294A4BA9AFED7EC /* 2003 */
	.quad	0x4F2FF516BA93DD63, 0xF5F2F8BD9E839CFB, 0xC529497535FDAFD8 /* 2004 */
	.quad	0x9E5FEA2D7527BAC7, 0xEBE5F17B3D0739F7, 0x8A5292EA6BFB5FB1 /* 2005 */
	.quad	0x3CBFD45AEA4F758F, 0xD7CBE2F67A0E73EF, 0x14A525D4D7F6BF62 /* 2006 */
	.quad	0x797FA8B5D49EEB1F, 0xAF97C5ECF41CE7DE, 0x294A4BA9AFED7EC4 /* 2007 */
	.quad	0xF2FF516BA93DD63F, 0x5F2F8BD9E839CFBC, 0x529497535FDAFD88 /* 2008 */
	.quad	0xE5FEA2D7527BAC7E, 0xBE5F17B3D0739F78, 0xA5292EA6BFB5FB11 /* 2009 */
	.quad	0xCBFD45AEA4F758FD, 0x7CBE2F67A0E73EF1, 0x4A525D4D7F6BF623 /* 2010 */
	.quad	0x97FA8B5D49EEB1FA, 0xF97C5ECF41CE7DE2, 0x94A4BA9AFED7EC47 /* 2011 */
	.quad	0x2FF516BA93DD63F5, 0xF2F8BD9E839CFBC5, 0x29497535FDAFD88F /* 2012 */
	.quad	0x5FEA2D7527BAC7EB, 0xE5F17B3D0739F78A, 0x5292EA6BFB5FB11F /* 2013 */
	.quad	0xBFD45AEA4F758FD7, 0xCBE2F67A0E73EF14, 0xA525D4D7F6BF623F /* 2014 */
	.quad	0x7FA8B5D49EEB1FAF, 0x97C5ECF41CE7DE29, 0x4A4BA9AFED7EC47E /* 2015 */
	.quad	0xFF516BA93DD63F5F, 0x2F8BD9E839CFBC52, 0x9497535FDAFD88FC /* 2016 */
	.quad	0xFEA2D7527BAC7EBE, 0x5F17B3D0739F78A5, 0x292EA6BFB5FB11F8 /* 2017 */
	.quad	0xFD45AEA4F758FD7C, 0xBE2F67A0E73EF14A, 0x525D4D7F6BF623F1 /* 2018 */
	.quad	0xFA8B5D49EEB1FAF9, 0x7C5ECF41CE7DE294, 0xA4BA9AFED7EC47E3 /* 2019 */
	.quad	0xF516BA93DD63F5F2, 0xF8BD9E839CFBC529, 0x497535FDAFD88FC6 /* 2020 */
	.quad	0xEA2D7527BAC7EBE5, 0xF17B3D0739F78A52, 0x92EA6BFB5FB11F8D /* 2021 */
	.quad	0xD45AEA4F758FD7CB, 0xE2F67A0E73EF14A5, 0x25D4D7F6BF623F1A /* 2022 */
	.quad	0xA8B5D49EEB1FAF97, 0xC5ECF41CE7DE294A, 0x4BA9AFED7EC47E35 /* 2023 */
	.quad	0x516BA93DD63F5F2F, 0x8BD9E839CFBC5294, 0x97535FDAFD88FC6A /* 2024 */
	.quad	0xA2D7527BAC7EBE5F, 0x17B3D0739F78A529, 0x2EA6BFB5FB11F8D5 /* 2025 */
	.quad	0x45AEA4F758FD7CBE, 0x2F67A0E73EF14A52, 0x5D4D7F6BF623F1AB /* 2026 */
	.quad	0x8B5D49EEB1FAF97C, 0x5ECF41CE7DE294A4, 0xBA9AFED7EC47E357 /* 2027 */
	.quad	0x16BA93DD63F5F2F8, 0xBD9E839CFBC52949, 0x7535FDAFD88FC6AE /* 2028 */
	.quad	0x2D7527BAC7EBE5F1, 0x7B3D0739F78A5292, 0xEA6BFB5FB11F8D5D /* 2029 */
	.quad	0x5AEA4F758FD7CBE2, 0xF67A0E73EF14A525, 0xD4D7F6BF623F1ABA /* 2030 */
	.quad	0xB5D49EEB1FAF97C5, 0xECF41CE7DE294A4B, 0xA9AFED7EC47E3574 /* 2031 */
	.quad	0x6BA93DD63F5F2F8B, 0xD9E839CFBC529497, 0x535FDAFD88FC6AE8 /* 2032 */
	.quad	0xD7527BAC7EBE5F17, 0xB3D0739F78A5292E, 0xA6BFB5FB11F8D5D0 /* 2033 */
	.quad	0xAEA4F758FD7CBE2F, 0x67A0E73EF14A525D, 0x4D7F6BF623F1ABA1 /* 2034 */
	.quad	0x5D49EEB1FAF97C5E, 0xCF41CE7DE294A4BA, 0x9AFED7EC47E35742 /* 2035 */
	.quad	0xBA93DD63F5F2F8BD, 0x9E839CFBC5294975, 0x35FDAFD88FC6AE84 /* 2036 */
	.quad	0x7527BAC7EBE5F17B, 0x3D0739F78A5292EA, 0x6BFB5FB11F8D5D08 /* 2037 */
	.quad	0xEA4F758FD7CBE2F6, 0x7A0E73EF14A525D4, 0xD7F6BF623F1ABA10 /* 2038 */
	.quad	0xD49EEB1FAF97C5EC, 0xF41CE7DE294A4BA9, 0xAFED7EC47E357421 /* 2039 */
	.quad	0xA93DD63F5F2F8BD9, 0xE839CFBC52949753, 0x5FDAFD88FC6AE842 /* 2040 */
	.quad	0x527BAC7EBE5F17B3, 0xD0739F78A5292EA6, 0xBFB5FB11F8D5D085 /* 2041 */
	.quad	0xA4F758FD7CBE2F67, 0xA0E73EF14A525D4D, 0x7F6BF623F1ABA10A /* 2042 */
	.quad	0x49EEB1FAF97C5ECF, 0x41CE7DE294A4BA9A, 0xFED7EC47E3574215 /* 2043 */
	.quad	0x93DD63F5F2F8BD9E, 0x839CFBC529497535, 0xFDAFD88FC6AE842B /* 2044 */
	.quad	0x27BAC7EBE5F17B3D, 0x0739F78A5292EA6B, 0xFB5FB11F8D5D0856 /* 2045 */
	.quad	0x4F758FD7CBE2F67A, 0x0E73EF14A525D4D7, 0xF6BF623F1ABA10AC /* 2046 */
	.quad	0x9EEB1FAF97C5ECF4, 0x1CE7DE294A4BA9AF, 0xED7EC47E35742158 /* 2047 */
	.align	32
	.type	__svml_dtan_reduction_data_internal, @object
	.size	__svml_dtan_reduction_data_internal, .-__svml_dtan_reduction_data_internal
