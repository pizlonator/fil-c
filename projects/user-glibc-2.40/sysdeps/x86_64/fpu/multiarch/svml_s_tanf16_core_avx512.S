/* Function tanf vectorized with AVX-512.
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
 *      ( optimized for throughput, with small table lookup, works when HW FMA is available )
 *
 *       Implementation reduces argument x to |R|<pi/64
 *       32-entry tables used to store high and low parts of tan(x0)
 *       Argument x = N*pi + x0 + (R);   x0 = k*pi/32, with k in {0, 1, ..., 31}
 *       (very large arguments reduction resolved in _vsreduction_core.i)
 *       Compute result as (tan(x0) + tan(R))/(1-tan(x0)*tan(R))
 *       _HA_ version keeps extra precision for numerator, denominator, and during
 *       final NR-iteration computing quotient.
 *
 *
 */

/* Offsets for data table __svml_stan_data_internal
 */
#define _sInvPI_uisa			0
#define _sPI1_uisa			64
#define _sPI2_uisa			128
#define _sPI3_uisa			192
#define Th_tbl_uisa			256
#define _sPC3_uisa			384
#define _sPC5_uisa			448
#define _sRangeReductionVal_uisa	512
#define _sAbsMask			576
#define _sRangeVal			640
#define _sRShifter			704
#define _sOne				768
#define _sRangeReductionVal		832
#define _sPI1				896
#define _sPI2				960
#define _sPI3				1024

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_tanf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	xorl	%edx, %edx

	/* Large values check */
	vmovups	_sRangeReductionVal_uisa+__svml_stan_data_internal(%rip), %zmm10

	/*
	 *
	 * Main path
	 *
	 * start arg. reduction
	 */
	vmovups	_sRShifter+__svml_stan_data_internal(%rip), %zmm1
	vmovups	_sPI1_uisa+__svml_stan_data_internal(%rip), %zmm4
	vmovups	_sPI2_uisa+__svml_stan_data_internal(%rip), %zmm2
	vmovups	_sPI3_uisa+__svml_stan_data_internal(%rip), %zmm3
	vmovaps	%zmm0, %zmm11
	vandps	_sAbsMask+__svml_stan_data_internal(%rip), %zmm11, %zmm0
	vcmpps	$22, {sae}, %zmm10, %zmm0, %k6
	vmovups	__svml_stan_data_internal(%rip), %zmm10

	/*
	 *
	 * End of main path
	 */

	kortestw %k6, %k6
	vfmadd213ps {rn-sae}, %zmm1, %zmm11, %zmm10
	vsubps	{rn-sae}, %zmm1, %zmm10, %zmm5
	vfnmadd213ps {rn-sae}, %zmm11, %zmm5, %zmm4
	vfnmadd231ps {rn-sae}, %zmm5, %zmm2, %zmm4
	vfnmadd213ps {rn-sae}, %zmm4, %zmm3, %zmm5

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm5 zmm10 zmm11 k6

	/* Return from auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH_RETURN):
	/* Table lookup */
	vmovups	Th_tbl_uisa+__svml_stan_data_internal(%rip), %zmm3
	vmovups	_sPC3_uisa+__svml_stan_data_internal(%rip), %zmm0
	vmulps	{rn-sae}, %zmm5, %zmm5, %zmm1
	vpermt2ps Th_tbl_uisa+64+__svml_stan_data_internal(%rip), %zmm10, %zmm3
	vmovups	_sPC5_uisa+__svml_stan_data_internal(%rip), %zmm10
	vfmadd231ps {rn-sae}, %zmm1, %zmm10, %zmm0
	vmulps	{rn-sae}, %zmm5, %zmm0, %zmm4
	vfmadd213ps {rn-sae}, %zmm5, %zmm1, %zmm4

	/*
	 * Computer Denominator:
	 * sDenominator - sDlow ~= 1-(sTh+sTl)*(sP+sPlow)
	 */
	vmovups	_sOne+__svml_stan_data_internal(%rip), %zmm5
	vmulps	{rn-sae}, %zmm4, %zmm3, %zmm7

	/*
	 * Compute Numerator:
	 * sNumerator + sNlow ~= sTh+sTl+sP+sPlow
	 */
	vaddps	{rn-sae}, %zmm3, %zmm4, %zmm8
	vsubps	{rn-sae}, %zmm7, %zmm5, %zmm9
	vsubps	{rn-sae}, %zmm3, %zmm8, %zmm2

	/*
	 * Now computes (sNumerator + sNlow)/(sDenominator - sDlow)
	 * Choose NR iteration instead of hardware division
	 */
	vrcp14ps %zmm9, %zmm14
	vsubps	{rn-sae}, %zmm5, %zmm9, %zmm6
	vsubps	{rn-sae}, %zmm2, %zmm4, %zmm13
	vmulps	{rn-sae}, %zmm8, %zmm14, %zmm15
	vaddps	{rn-sae}, %zmm7, %zmm6, %zmm12

	/* One NR iteration to refine sQuotient */
	vfmsub213ps {rn-sae}, %zmm8, %zmm15, %zmm9
	vfnmadd213ps {rn-sae}, %zmm9, %zmm15, %zmm12
	vsubps	{rn-sae}, %zmm13, %zmm12, %zmm0
	vfnmadd213ps {rn-sae}, %zmm15, %zmm14, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm11

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
	vmovups	%zmm11, 64(%rsp)
	vmovups	%zmm0, 128(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm0

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
	vmovups	128(%rsp), %zmm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	64(%rsp, %r14, 4), %xmm0
	call	tanf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

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
	vmovups	_sRangeVal+__svml_stan_data_internal(%rip), %zmm6

	/*
	 * Get the (2^a / 2pi) mod 1 values from the table.
	 * Because doesn't have I-type gather, we need a trivial cast
	 */
	lea	__svml_stan_reduction_data_internal(%rip), %rax
	vmovups	%zmm5, (%rsp)
	vandps	%zmm0, %zmm6, %zmm14
	vcmpps	$0, {sae}, %zmm6, %zmm14, %k0

	/*
	 * Break the P_xxx and m into 16-bit chunks ready for
	 * the long multiplication via 16x16->32 multiplications
	 */
	vmovups	.FLT_15(%rip), %zmm6
	kxnorw	%k0, %k0, %k1
	kxnorw	%k0, %k0, %k2
	kxnorw	%k0, %k0, %k3
	kmovw	%k0, %edx
	vpandd	.FLT_12(%rip), %zmm11, %zmm5
	vpsrld	$23, %zmm5, %zmm7
	vpslld	$1, %zmm7, %zmm8
	vpaddd	%zmm7, %zmm8, %zmm9
	vpslld	$2, %zmm9, %zmm4
	vpxord	%zmm3, %zmm3, %zmm3
	vpxord	%zmm15, %zmm15, %zmm15
	vpxord	%zmm2, %zmm2, %zmm2
	vgatherdps (%rax, %zmm4), %zmm3{%k1}
	vgatherdps 4(%rax, %zmm4), %zmm15{%k2}
	vgatherdps 8(%rax, %zmm4), %zmm2{%k3}
	vpsrld	$16, %zmm3, %zmm5
	vpsrld	$16, %zmm2, %zmm13

	/*
	 * Also get the significand as an integer
	 * NB: adding in the integer bit is wrong for denorms!
	 * To make this work for denorms we should do something slightly different
	 */
	vpandd	.FLT_13(%rip), %zmm11, %zmm0
	vpaddd	.FLT_14(%rip), %zmm0, %zmm1
	vpsrld	$16, %zmm15, %zmm0
	vpsrld	$16, %zmm1, %zmm8
	vpandd	%zmm6, %zmm3, %zmm9
	vpandd	%zmm6, %zmm15, %zmm12
	vpandd	%zmm6, %zmm2, %zmm7
	vpandd	%zmm6, %zmm1, %zmm14

	/* Now do the big multiplication and carry propagation */
	vpmulld	%zmm9, %zmm8, %zmm4
	vpmulld	%zmm0, %zmm8, %zmm3
	vpmulld	%zmm12, %zmm8, %zmm2
	vpmulld	%zmm13, %zmm8, %zmm1
	vpmulld	%zmm7, %zmm8, %zmm8
	vpmulld	%zmm5, %zmm14, %zmm7
	vpmulld	%zmm9, %zmm14, %zmm5
	vpmulld	%zmm0, %zmm14, %zmm9
	vpmulld	%zmm12, %zmm14, %zmm0
	vpmulld	%zmm13, %zmm14, %zmm12
	vpsrld	$16, %zmm12, %zmm14
	vpsrld	$16, %zmm0, %zmm13
	vpsrld	$16, %zmm9, %zmm15
	vpsrld	$16, %zmm5, %zmm12
	vpsrld	$16, %zmm8, %zmm8
	vpaddd	%zmm14, %zmm1, %zmm1
	vpaddd	%zmm13, %zmm2, %zmm2
	vpaddd	%zmm15, %zmm3, %zmm15
	vpaddd	%zmm12, %zmm4, %zmm3
	vpandd	%zmm6, %zmm0, %zmm13
	vpaddd	%zmm1, %zmm13, %zmm4
	vpaddd	%zmm4, %zmm8, %zmm14
	vpsrld	$16, %zmm14, %zmm0
	vpandd	%zmm6, %zmm9, %zmm9
	vpaddd	%zmm2, %zmm9, %zmm1
	vpaddd	%zmm1, %zmm0, %zmm8

	/*
	 * Now round at the 2^-8 bit position for reduction mod pi/2^7
	 * instead of the original 2pi (but still with the same 2pi scaling).
	 * Use a shifter of 2^15 + 2^14.
	 * The N we get is our final version; it has an offset of
	 * 2^8 because of the implicit integer bit, and anyway for negative
	 * starting value it's a 2s complement thing. But we need to mask
	 * off the exponent part anyway so it's fine.
	 */
	vmovups	.FLT_18(%rip), %zmm1
	vpandd	%zmm6, %zmm7, %zmm7
	vpaddd	%zmm3, %zmm7, %zmm13
	vpsrld	$16, %zmm8, %zmm3
	vpandd	%zmm6, %zmm5, %zmm5
	vpaddd	%zmm15, %zmm5, %zmm2
	vpaddd	%zmm2, %zmm3, %zmm15
	vpsrld	$16, %zmm15, %zmm12
	vpaddd	%zmm13, %zmm12, %zmm5

	/* Assemble reduced argument from the pieces */
	vpandd	%zmm6, %zmm14, %zmm9
	vpandd	%zmm6, %zmm15, %zmm7
	vpslld	$16, %zmm5, %zmm6
	vpslld	$16, %zmm8, %zmm5
	vpaddd	%zmm7, %zmm6, %zmm4
	vpaddd	%zmm9, %zmm5, %zmm9
	vpsrld	$9, %zmm4, %zmm6

	/*
	 * We want to incorporate the original sign now too.
	 * Do it here for convenience in getting the right N value,
	 * though we could wait right to the end if we were prepared
	 * to modify the sign of N later too.
	 * So get the appropriate sign mask now (or sooner).
	 */
	vpandd	.FLT_16(%rip), %zmm11, %zmm0
	vpandd	.FLT_21(%rip), %zmm9, %zmm13
	vpslld	$5, %zmm13, %zmm14

	/*
	 * Create floating-point high part, implicitly adding integer bit 1
	 * Incorporate overall sign at this stage too.
	 */
	vpxord	.FLT_17(%rip), %zmm0, %zmm8
	vpord	%zmm8, %zmm6, %zmm2
	vaddps	{rn-sae}, %zmm2, %zmm1, %zmm12
	vsubps	{rn-sae}, %zmm1, %zmm12, %zmm3
	vsubps	{rn-sae}, %zmm3, %zmm2, %zmm7

	/*
	 * Create floating-point low and medium parts, respectively
	 * lo_17, ... lo_0, 0, ..., 0
	 * hi_8, ... hi_0, lo_31, ..., lo_18
	 * then subtract off the implicitly added integer bits,
	 * 2^-46 and 2^-23, respectively.
	 * Put the original sign into all of them at this stage.
	 */
	vpxord	.FLT_20(%rip), %zmm0, %zmm6
	vpord	%zmm6, %zmm14, %zmm15
	vpandd	.FLT_23(%rip), %zmm4, %zmm4
	vsubps	{rn-sae}, %zmm6, %zmm15, %zmm8
	vandps	.FLT_26(%rip), %zmm11, %zmm15
	vpsrld	$18, %zmm9, %zmm6

	/*
	 * If the magnitude of the input is <= 2^-20, then
	 * just pass through the input, since no reduction will be needed and
	 * the main path will only work accurately if the reduced argument is
	 * about >= 2^-40 (which it is for all large pi multiples)
	 */
	vmovups	.FLT_27(%rip), %zmm14
	vcmpps	$26, {sae}, %zmm14, %zmm15, %k4
	vcmpps	$22, {sae}, %zmm14, %zmm15, %k5
	vpxord	.FLT_22(%rip), %zmm0, %zmm1
	vpslld	$14, %zmm4, %zmm0
	vpord	%zmm6, %zmm0, %zmm0
	vpord	%zmm1, %zmm0, %zmm4
	vsubps	{rn-sae}, %zmm1, %zmm4, %zmm2
	vpternlogd $255, %zmm6, %zmm6, %zmm6

	/* Now add them up into 2 reasonably aligned pieces */
	vaddps	{rn-sae}, %zmm2, %zmm7, %zmm13
	vsubps	{rn-sae}, %zmm13, %zmm7, %zmm7
	vaddps	{rn-sae}, %zmm7, %zmm2, %zmm3

	/*
	 * The output is _VRES_R (high) + _VRES_E (low), and the integer part is _VRES_IND
	 * Set sRp2 = _VRES_R^2 and then resume the original code.
	 */
	vmovups	.FLT_28(%rip), %zmm2
	vaddps	{rn-sae}, %zmm8, %zmm3, %zmm1
	vmovups	.FLT_25(%rip), %zmm8

	/* Grab our final N value as an integer, appropriately masked mod 2^8 */
	vpandd	.FLT_19(%rip), %zmm12, %zmm5

	/*
	 * Now multiply those numbers all by 2 pi, reasonably accurately.
	 * (RHi + RLo) * (pi_lead + pi_trail) ~=
	 * RHi * pi_lead + (RHi * pi_trail + RLo * pi_lead)
	 */
	vmovups	.FLT_24(%rip), %zmm12
	vmulps	{rn-sae}, %zmm12, %zmm13, %zmm0
	vmovaps	%zmm12, %zmm9
	vfmsub213ps {rn-sae}, %zmm0, %zmm13, %zmm9
	vfmadd213ps {rn-sae}, %zmm9, %zmm8, %zmm13
	vmovaps	%zmm6, %zmm8
	vfmadd213ps {rn-sae}, %zmm13, %zmm12, %zmm1
	vpandnd	%zmm15, %zmm15, %zmm8{%k4}
	vpandnd	%zmm15, %zmm15, %zmm6{%k5}
	vandps	%zmm11, %zmm6, %zmm14
	vandps	%zmm0, %zmm8, %zmm15
	vandps	%zmm1, %zmm8, %zmm12
	vorps	%zmm15, %zmm14, %zmm6
	vpsrld	$31, %zmm6, %zmm3
	vpsubd	%zmm3, %zmm2, %zmm4
	vpaddd	%zmm4, %zmm5, %zmm7
	vpsrld	$2, %zmm7, %zmm13
	vpslld	$2, %zmm13, %zmm9

	/*
	 *
	 * End of large arguments path
	 *
	 * Merge results from main and large paths:
	 */
	vblendmps %zmm13, %zmm10, %zmm10{%k6}
	vpsubd	%zmm9, %zmm5, %zmm5
	vmovups	.FLT_29(%rip), %zmm9
	vcvtdq2ps {rn-sae}, %zmm5, %zmm0
	vmovups	.FLT_30(%rip), %zmm5
	vfmadd231ps {rn-sae}, %zmm0, %zmm5, %zmm12
	vmovups	(%rsp), %zmm5
	vaddps	{rn-sae}, %zmm6, %zmm12, %zmm6
	vfmadd213ps {rn-sae}, %zmm6, %zmm9, %zmm0
	vblendmps %zmm0, %zmm5, %zmm5{%k6}

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx r12 r13 r14 r15 edx zmm5 zmm10 zmm11
END(_ZGVeN16v_tanf_skx)

	.section .rodata, "a"
	.align	64

.FLT_12:
	.long	0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000
	.type	.FLT_12, @object
	.size	.FLT_12, 64
	.align	64

.FLT_13:
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	.type	.FLT_13, @object
	.size	.FLT_13, 64
	.align	64

.FLT_14:
	.long	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000
	.type	.FLT_14, @object
	.size	.FLT_14, 64
	.align	64

.FLT_15:
	.long	0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff
	.type	.FLT_15, @object
	.size	.FLT_15, 64
	.align	64

.FLT_16:
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
	.type	.FLT_16, @object
	.size	.FLT_16, 64
	.align	64

.FLT_17:
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	.type	.FLT_17, @object
	.size	.FLT_17, 64
	.align	64

.FLT_18:
	.long	0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000, 0x47400000
	.type	.FLT_18, @object
	.size	.FLT_18, 64
	.align	64

.FLT_19:
	.long	0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
	.type	.FLT_19, @object
	.size	.FLT_19, 64
	.align	64

.FLT_20:
	.long	0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000, 0x28800000
	.type	.FLT_20, @object
	.size	.FLT_20, 64
	.align	64

.FLT_21:
	.long	0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff, 0x0003ffff
	.type	.FLT_21, @object
	.size	.FLT_21, 64
	.align	64

.FLT_22:
	.long	0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000, 0x34000000
	.type	.FLT_22, @object
	.size	.FLT_22, 64
	.align	64

.FLT_23:
	.long	0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff
	.type	.FLT_23, @object
	.size	.FLT_23, 64
	.align	64

.FLT_24:
	.long	0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb
	.type	.FLT_24, @object
	.size	.FLT_24, 64
	.align	64

.FLT_25:
	.long	0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e, 0xb43bbd2e
	.type	.FLT_25, @object
	.size	.FLT_25, 64
	.align	64

.FLT_26:
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	.type	.FLT_26, @object
	.size	.FLT_26, 64
	.align	64

.FLT_27:
	.long	0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000, 0x35800000
	.type	.FLT_27, @object
	.size	.FLT_27, 64
	.align	64

.FLT_28:
	.long	0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002, 0x00000002
	.type	.FLT_28, @object
	.size	.FLT_28, 64
	.align	64

.FLT_29:
	.long	0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb, 0x3cc90fdb
	.type	.FLT_29, @object
	.size	.FLT_29, 64
	.align	64

.FLT_30:
	.long	0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e, 0xb03bbd2e
	.type	.FLT_30, @object
	.size	.FLT_30, 64
	.align	64

#ifdef __svml_stan_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _sInvPI_uisa[16][1];
	__declspec(align(64)) VUINT32 _sPI1_uisa[16][1];
	__declspec(align(64)) VUINT32 _sPI2_uisa[16][1];
	__declspec(align(64)) VUINT32 _sPI3_uisa[16][1];
	__declspec(align(64)) VUINT32 Th_tbl_uisa[32][1];
	__declspec(align(64)) VUINT32 _sPC3_uisa[16][1];
	__declspec(align(64)) VUINT32 _sPC5_uisa[16][1];
	__declspec(align(64)) VUINT32 _sRangeReductionVal_uisa[16][1];
	__declspec(align(64)) VUINT32 _sAbsMask[16][1];
	__declspec(align(64)) VUINT32 _sRangeVal[16][1];
	__declspec(align(64)) VUINT32 _sRShifter[16][1];
	__declspec(align(64)) VUINT32 _sOne[16][1];
	__declspec(align(64)) VUINT32 _sRangeReductionVal[16][1];
	__declspec(align(64)) VUINT32 _sPI1[16][1];
	__declspec(align(64)) VUINT32 _sPI2[16][1];
	__declspec(align(64)) VUINT32 _sPI3[16][1];
} __svml_stan_data_internal;
#endif
__svml_stan_data_internal:
	/* UISA */
	.long	0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983, 0x4122f983 /* _sInvPI_uisa */
	.align	64
	.long	0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda, 0x3dc90fda /* _sPI1_uisa */
	.align	64
	.long	0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168, 0x31a22168 /* _sPI2_uisa */
	.align	64
	.long	0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5, 0x25c234c5 /* _sPI3_uisa */
	/* Th_tbl_uisa for i from 0 to 31 do printsingle(tan(i*Pi/32)); */
	.align	64
	.long	0x80000000, 0x3dc9b5dc, 0x3e4bafaf, 0x3e9b5042
	.long	0x3ed413cd, 0x3f08d5b9, 0x3f2b0dc1, 0x3f521801
	.long	0x3f800000, 0x3f9bf7ec, 0x3fbf90c7, 0x3fef789e
	.long	0x401a827a, 0x4052facf, 0x40a0dff7, 0x41227363
	.long	0xff7fffff, 0xc1227363, 0xc0a0dff7, 0xc052facf
	.long	0xc01a827a, 0xbfef789e, 0xbfbf90c7, 0xbf9bf7ec
	.long	0xbf800000, 0xbf521801, 0xbf2b0dc1, 0xbf08d5b9
	.long	0xbed413cd, 0xbe9b5042, 0xbe4bafaf, 0xbdc9b5dc
	.align	64
	.long	0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6, 0x3eaaaaa6 /* _sPC3_uisa */
	.align	64
	.long	0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888, 0x3e08b888 /* _sPC5_uisa */
	.align	64
	.long	0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000 /* _sRangeReductionVal_uisa */
	.align	64
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF /* _sAbsMask */
	.align	64
	.long	0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000 /* _sRangeVal */
	.align	64
	.long	0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000, 0x4B400000 /* _sRShifter */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 /* _sOne */
	.align	64
	.long	0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000, 0x46010000 /* _sRangeVal */
	.align	64
	.long	0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000, 0x3FC90000 /* _sPI1 */
	.align	64
	.long	0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000, 0x39FDA000 /* _sPI2 */
	.align	64
	.long	0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000, 0x33A22000 /* _sPI3 */
	.align	64
	.type	__svml_stan_data_internal, @object
	.size	__svml_stan_data_internal, .-__svml_stan_data_internal
	.align	64

#ifdef __svml_stan_reduction_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _sPtable[256][3][1];
} __svml_stan_reduction_data_internal;
#endif
__svml_stan_reduction_data_internal:
	/*     P_hi                  P_med               P_lo                */
	.long	0x00000000, 0x00000000, 0x00000000 /* 0 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 1 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 2 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 3 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 4 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 5 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 6 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 7 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 8 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 9 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 10 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 11 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 12 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 13 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 14 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 15 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 16 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 17 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 18 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 19 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 20 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 21 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 22 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 23 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 24 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 25 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 26 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 27 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 28 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 29 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 30 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 31 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 32 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 33 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 34 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 35 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 36 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 37 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 38 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 39 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 40 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 41 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 42 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 43 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 44 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 45 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 46 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 47 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 48 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 49 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 50 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 51 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 52 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 53 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 54 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 55 */
	.long	0x00000000, 0x00000000, 0x00000000 /* 56 */
	.long	0x00000000, 0x00000000, 0x00000001 /* 57 */
	.long	0x00000000, 0x00000000, 0x00000002 /* 58 */
	.long	0x00000000, 0x00000000, 0x00000005 /* 59 */
	.long	0x00000000, 0x00000000, 0x0000000A /* 60 */
	.long	0x00000000, 0x00000000, 0x00000014 /* 61 */
	.long	0x00000000, 0x00000000, 0x00000028 /* 62 */
	.long	0x00000000, 0x00000000, 0x00000051 /* 63 */
	.long	0x00000000, 0x00000000, 0x000000A2 /* 64 */
	.long	0x00000000, 0x00000000, 0x00000145 /* 65 */
	.long	0x00000000, 0x00000000, 0x0000028B /* 66 */
	.long	0x00000000, 0x00000000, 0x00000517 /* 67 */
	.long	0x00000000, 0x00000000, 0x00000A2F /* 68 */
	.long	0x00000000, 0x00000000, 0x0000145F /* 69 */
	.long	0x00000000, 0x00000000, 0x000028BE /* 70 */
	.long	0x00000000, 0x00000000, 0x0000517C /* 71 */
	.long	0x00000000, 0x00000000, 0x0000A2F9 /* 72 */
	.long	0x00000000, 0x00000000, 0x000145F3 /* 73 */
	.long	0x00000000, 0x00000000, 0x00028BE6 /* 74 */
	.long	0x00000000, 0x00000000, 0x000517CC /* 75 */
	.long	0x00000000, 0x00000000, 0x000A2F98 /* 76 */
	.long	0x00000000, 0x00000000, 0x00145F30 /* 77 */
	.long	0x00000000, 0x00000000, 0x0028BE60 /* 78 */
	.long	0x00000000, 0x00000000, 0x00517CC1 /* 79 */
	.long	0x00000000, 0x00000000, 0x00A2F983 /* 80 */
	.long	0x00000000, 0x00000000, 0x0145F306 /* 81 */
	.long	0x00000000, 0x00000000, 0x028BE60D /* 82 */
	.long	0x00000000, 0x00000000, 0x0517CC1B /* 83 */
	.long	0x00000000, 0x00000000, 0x0A2F9836 /* 84 */
	.long	0x00000000, 0x00000000, 0x145F306D /* 85 */
	.long	0x00000000, 0x00000000, 0x28BE60DB /* 86 */
	.long	0x00000000, 0x00000000, 0x517CC1B7 /* 87 */
	.long	0x00000000, 0x00000000, 0xA2F9836E /* 88 */
	.long	0x00000000, 0x00000001, 0x45F306DC /* 89 */
	.long	0x00000000, 0x00000002, 0x8BE60DB9 /* 90 */
	.long	0x00000000, 0x00000005, 0x17CC1B72 /* 91 */
	.long	0x00000000, 0x0000000A, 0x2F9836E4 /* 92 */
	.long	0x00000000, 0x00000014, 0x5F306DC9 /* 93 */
	.long	0x00000000, 0x00000028, 0xBE60DB93 /* 94 */
	.long	0x00000000, 0x00000051, 0x7CC1B727 /* 95 */
	.long	0x00000000, 0x000000A2, 0xF9836E4E /* 96 */
	.long	0x00000000, 0x00000145, 0xF306DC9C /* 97 */
	.long	0x00000000, 0x0000028B, 0xE60DB939 /* 98 */
	.long	0x00000000, 0x00000517, 0xCC1B7272 /* 99 */
	.long	0x00000000, 0x00000A2F, 0x9836E4E4 /* 100 */
	.long	0x00000000, 0x0000145F, 0x306DC9C8 /* 101 */
	.long	0x00000000, 0x000028BE, 0x60DB9391 /* 102 */
	.long	0x00000000, 0x0000517C, 0xC1B72722 /* 103 */
	.long	0x00000000, 0x0000A2F9, 0x836E4E44 /* 104 */
	.long	0x00000000, 0x000145F3, 0x06DC9C88 /* 105 */
	.long	0x00000000, 0x00028BE6, 0x0DB93910 /* 106 */
	.long	0x00000000, 0x000517CC, 0x1B727220 /* 107 */
	.long	0x00000000, 0x000A2F98, 0x36E4E441 /* 108 */
	.long	0x00000000, 0x00145F30, 0x6DC9C882 /* 109 */
	.long	0x00000000, 0x0028BE60, 0xDB939105 /* 110 */
	.long	0x00000000, 0x00517CC1, 0xB727220A /* 111 */
	.long	0x00000000, 0x00A2F983, 0x6E4E4415 /* 112 */
	.long	0x00000000, 0x0145F306, 0xDC9C882A /* 113 */
	.long	0x00000000, 0x028BE60D, 0xB9391054 /* 114 */
	.long	0x00000000, 0x0517CC1B, 0x727220A9 /* 115 */
	.long	0x00000000, 0x0A2F9836, 0xE4E44152 /* 116 */
	.long	0x00000000, 0x145F306D, 0xC9C882A5 /* 117 */
	.long	0x00000000, 0x28BE60DB, 0x9391054A /* 118 */
	.long	0x00000000, 0x517CC1B7, 0x27220A94 /* 119 */
	.long	0x00000000, 0xA2F9836E, 0x4E441529 /* 120 */
	.long	0x00000001, 0x45F306DC, 0x9C882A53 /* 121 */
	.long	0x00000002, 0x8BE60DB9, 0x391054A7 /* 122 */
	.long	0x00000005, 0x17CC1B72, 0x7220A94F /* 123 */
	.long	0x0000000A, 0x2F9836E4, 0xE441529F /* 124 */
	.long	0x00000014, 0x5F306DC9, 0xC882A53F /* 125 */
	.long	0x00000028, 0xBE60DB93, 0x91054A7F /* 126 */
	.long	0x00000051, 0x7CC1B727, 0x220A94FE /* 127 */
	.long	0x000000A2, 0xF9836E4E, 0x441529FC /* 128 */
	.long	0x00000145, 0xF306DC9C, 0x882A53F8 /* 129 */
	.long	0x0000028B, 0xE60DB939, 0x1054A7F0 /* 130 */
	.long	0x00000517, 0xCC1B7272, 0x20A94FE1 /* 131 */
	.long	0x00000A2F, 0x9836E4E4, 0x41529FC2 /* 132 */
	.long	0x0000145F, 0x306DC9C8, 0x82A53F84 /* 133 */
	.long	0x000028BE, 0x60DB9391, 0x054A7F09 /* 134 */
	.long	0x0000517C, 0xC1B72722, 0x0A94FE13 /* 135 */
	.long	0x0000A2F9, 0x836E4E44, 0x1529FC27 /* 136 */
	.long	0x000145F3, 0x06DC9C88, 0x2A53F84E /* 137 */
	.long	0x00028BE6, 0x0DB93910, 0x54A7F09D /* 138 */
	.long	0x000517CC, 0x1B727220, 0xA94FE13A /* 139 */
	.long	0x000A2F98, 0x36E4E441, 0x529FC275 /* 140 */
	.long	0x00145F30, 0x6DC9C882, 0xA53F84EA /* 141 */
	.long	0x0028BE60, 0xDB939105, 0x4A7F09D5 /* 142 */
	.long	0x00517CC1, 0xB727220A, 0x94FE13AB /* 143 */
	.long	0x00A2F983, 0x6E4E4415, 0x29FC2757 /* 144 */
	.long	0x0145F306, 0xDC9C882A, 0x53F84EAF /* 145 */
	.long	0x028BE60D, 0xB9391054, 0xA7F09D5F /* 146 */
	.long	0x0517CC1B, 0x727220A9, 0x4FE13ABE /* 147 */
	.long	0x0A2F9836, 0xE4E44152, 0x9FC2757D /* 148 */
	.long	0x145F306D, 0xC9C882A5, 0x3F84EAFA /* 149 */
	.long	0x28BE60DB, 0x9391054A, 0x7F09D5F4 /* 150 */
	.long	0x517CC1B7, 0x27220A94, 0xFE13ABE8 /* 151 */
	.long	0xA2F9836E, 0x4E441529, 0xFC2757D1 /* 152 */
	.long	0x45F306DC, 0x9C882A53, 0xF84EAFA3 /* 153 */
	.long	0x8BE60DB9, 0x391054A7, 0xF09D5F47 /* 154 */
	.long	0x17CC1B72, 0x7220A94F, 0xE13ABE8F /* 155 */
	.long	0x2F9836E4, 0xE441529F, 0xC2757D1F /* 156 */
	.long	0x5F306DC9, 0xC882A53F, 0x84EAFA3E /* 157 */
	.long	0xBE60DB93, 0x91054A7F, 0x09D5F47D /* 158 */
	.long	0x7CC1B727, 0x220A94FE, 0x13ABE8FA /* 159 */
	.long	0xF9836E4E, 0x441529FC, 0x2757D1F5 /* 160 */
	.long	0xF306DC9C, 0x882A53F8, 0x4EAFA3EA /* 161 */
	.long	0xE60DB939, 0x1054A7F0, 0x9D5F47D4 /* 162 */
	.long	0xCC1B7272, 0x20A94FE1, 0x3ABE8FA9 /* 163 */
	.long	0x9836E4E4, 0x41529FC2, 0x757D1F53 /* 164 */
	.long	0x306DC9C8, 0x82A53F84, 0xEAFA3EA6 /* 165 */
	.long	0x60DB9391, 0x054A7F09, 0xD5F47D4D /* 166 */
	.long	0xC1B72722, 0x0A94FE13, 0xABE8FA9A /* 167 */
	.long	0x836E4E44, 0x1529FC27, 0x57D1F534 /* 168 */
	.long	0x06DC9C88, 0x2A53F84E, 0xAFA3EA69 /* 169 */
	.long	0x0DB93910, 0x54A7F09D, 0x5F47D4D3 /* 170 */
	.long	0x1B727220, 0xA94FE13A, 0xBE8FA9A6 /* 171 */
	.long	0x36E4E441, 0x529FC275, 0x7D1F534D /* 172 */
	.long	0x6DC9C882, 0xA53F84EA, 0xFA3EA69B /* 173 */
	.long	0xDB939105, 0x4A7F09D5, 0xF47D4D37 /* 174 */
	.long	0xB727220A, 0x94FE13AB, 0xE8FA9A6E /* 175 */
	.long	0x6E4E4415, 0x29FC2757, 0xD1F534DD /* 176 */
	.long	0xDC9C882A, 0x53F84EAF, 0xA3EA69BB /* 177 */
	.long	0xB9391054, 0xA7F09D5F, 0x47D4D377 /* 178 */
	.long	0x727220A9, 0x4FE13ABE, 0x8FA9A6EE /* 179 */
	.long	0xE4E44152, 0x9FC2757D, 0x1F534DDC /* 180 */
	.long	0xC9C882A5, 0x3F84EAFA, 0x3EA69BB8 /* 181 */
	.long	0x9391054A, 0x7F09D5F4, 0x7D4D3770 /* 182 */
	.long	0x27220A94, 0xFE13ABE8, 0xFA9A6EE0 /* 183 */
	.long	0x4E441529, 0xFC2757D1, 0xF534DDC0 /* 184 */
	.long	0x9C882A53, 0xF84EAFA3, 0xEA69BB81 /* 185 */
	.long	0x391054A7, 0xF09D5F47, 0xD4D37703 /* 186 */
	.long	0x7220A94F, 0xE13ABE8F, 0xA9A6EE06 /* 187 */
	.long	0xE441529F, 0xC2757D1F, 0x534DDC0D /* 188 */
	.long	0xC882A53F, 0x84EAFA3E, 0xA69BB81B /* 189 */
	.long	0x91054A7F, 0x09D5F47D, 0x4D377036 /* 190 */
	.long	0x220A94FE, 0x13ABE8FA, 0x9A6EE06D /* 191 */
	.long	0x441529FC, 0x2757D1F5, 0x34DDC0DB /* 192 */
	.long	0x882A53F8, 0x4EAFA3EA, 0x69BB81B6 /* 193 */
	.long	0x1054A7F0, 0x9D5F47D4, 0xD377036D /* 194 */
	.long	0x20A94FE1, 0x3ABE8FA9, 0xA6EE06DB /* 195 */
	.long	0x41529FC2, 0x757D1F53, 0x4DDC0DB6 /* 196 */
	.long	0x82A53F84, 0xEAFA3EA6, 0x9BB81B6C /* 197 */
	.long	0x054A7F09, 0xD5F47D4D, 0x377036D8 /* 198 */
	.long	0x0A94FE13, 0xABE8FA9A, 0x6EE06DB1 /* 199 */
	.long	0x1529FC27, 0x57D1F534, 0xDDC0DB62 /* 200 */
	.long	0x2A53F84E, 0xAFA3EA69, 0xBB81B6C5 /* 201 */
	.long	0x54A7F09D, 0x5F47D4D3, 0x77036D8A /* 202 */
	.long	0xA94FE13A, 0xBE8FA9A6, 0xEE06DB14 /* 203 */
	.long	0x529FC275, 0x7D1F534D, 0xDC0DB629 /* 204 */
	.long	0xA53F84EA, 0xFA3EA69B, 0xB81B6C52 /* 205 */
	.long	0x4A7F09D5, 0xF47D4D37, 0x7036D8A5 /* 206 */
	.long	0x94FE13AB, 0xE8FA9A6E, 0xE06DB14A /* 207 */
	.long	0x29FC2757, 0xD1F534DD, 0xC0DB6295 /* 208 */
	.long	0x53F84EAF, 0xA3EA69BB, 0x81B6C52B /* 209 */
	.long	0xA7F09D5F, 0x47D4D377, 0x036D8A56 /* 210 */
	.long	0x4FE13ABE, 0x8FA9A6EE, 0x06DB14AC /* 211 */
	.long	0x9FC2757D, 0x1F534DDC, 0x0DB62959 /* 212 */
	.long	0x3F84EAFA, 0x3EA69BB8, 0x1B6C52B3 /* 213 */
	.long	0x7F09D5F4, 0x7D4D3770, 0x36D8A566 /* 214 */
	.long	0xFE13ABE8, 0xFA9A6EE0, 0x6DB14ACC /* 215 */
	.long	0xFC2757D1, 0xF534DDC0, 0xDB629599 /* 216 */
	.long	0xF84EAFA3, 0xEA69BB81, 0xB6C52B32 /* 217 */
	.long	0xF09D5F47, 0xD4D37703, 0x6D8A5664 /* 218 */
	.long	0xE13ABE8F, 0xA9A6EE06, 0xDB14ACC9 /* 219 */
	.long	0xC2757D1F, 0x534DDC0D, 0xB6295993 /* 220 */
	.long	0x84EAFA3E, 0xA69BB81B, 0x6C52B327 /* 221 */
	.long	0x09D5F47D, 0x4D377036, 0xD8A5664F /* 222 */
	.long	0x13ABE8FA, 0x9A6EE06D, 0xB14ACC9E /* 223 */
	.long	0x2757D1F5, 0x34DDC0DB, 0x6295993C /* 224 */
	.long	0x4EAFA3EA, 0x69BB81B6, 0xC52B3278 /* 225 */
	.long	0x9D5F47D4, 0xD377036D, 0x8A5664F1 /* 226 */
	.long	0x3ABE8FA9, 0xA6EE06DB, 0x14ACC9E2 /* 227 */
	.long	0x757D1F53, 0x4DDC0DB6, 0x295993C4 /* 228 */
	.long	0xEAFA3EA6, 0x9BB81B6C, 0x52B32788 /* 229 */
	.long	0xD5F47D4D, 0x377036D8, 0xA5664F10 /* 230 */
	.long	0xABE8FA9A, 0x6EE06DB1, 0x4ACC9E21 /* 231 */
	.long	0x57D1F534, 0xDDC0DB62, 0x95993C43 /* 232 */
	.long	0xAFA3EA69, 0xBB81B6C5, 0x2B327887 /* 233 */
	.long	0x5F47D4D3, 0x77036D8A, 0x5664F10E /* 234 */
	.long	0xBE8FA9A6, 0xEE06DB14, 0xACC9E21C /* 235 */
	.long	0x7D1F534D, 0xDC0DB629, 0x5993C439 /* 236 */
	.long	0xFA3EA69B, 0xB81B6C52, 0xB3278872 /* 237 */
	.long	0xF47D4D37, 0x7036D8A5, 0x664F10E4 /* 238 */
	.long	0xE8FA9A6E, 0xE06DB14A, 0xCC9E21C8 /* 239 */
	.long	0xD1F534DD, 0xC0DB6295, 0x993C4390 /* 240 */
	.long	0xA3EA69BB, 0x81B6C52B, 0x32788720 /* 241 */
	.long	0x47D4D377, 0x036D8A56, 0x64F10E41 /* 242 */
	.long	0x8FA9A6EE, 0x06DB14AC, 0xC9E21C82 /* 243 */
	.long	0x1F534DDC, 0x0DB62959, 0x93C43904 /* 244 */
	.long	0x3EA69BB8, 0x1B6C52B3, 0x27887208 /* 245 */
	.long	0x7D4D3770, 0x36D8A566, 0x4F10E410 /* 246 */
	.long	0xFA9A6EE0, 0x6DB14ACC, 0x9E21C820 /* 247 */
	.long	0xF534DDC0, 0xDB629599, 0x3C439041 /* 248 */
	.long	0xEA69BB81, 0xB6C52B32, 0x78872083 /* 249 */
	.long	0xD4D37703, 0x6D8A5664, 0xF10E4107 /* 250 */
	.long	0xA9A6EE06, 0xDB14ACC9, 0xE21C820F /* 251 */
	.long	0x534DDC0D, 0xB6295993, 0xC439041F /* 252 */
	.long	0xA69BB81B, 0x6C52B327, 0x8872083F /* 253 */
	.long	0x4D377036, 0xD8A5664F, 0x10E4107F /* 254 */
	.long	0x9A6EE06D, 0xB14ACC9E, 0x21C820FF /* 255 */
	.align	64
	.type	__svml_stan_reduction_data_internal, @object
	.size	__svml_stan_reduction_data_internal, .-__svml_stan_reduction_data_internal
