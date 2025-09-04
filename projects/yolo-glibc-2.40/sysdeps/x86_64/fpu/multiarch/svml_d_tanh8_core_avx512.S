/* Function tanh vectorized with AVX-512.
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
 *   NOTE: Since the hyperbolic tangent function is odd
 *         (tanh(x) = -tanh(-x)), below algorithm deals with the absolute
 *         value of the argument |x|: tanh(x) = sign(x) * tanh(|x|)
 *
 *   We use a table lookup method to compute tanh(|x|).
 *   The basic idea is to split the input range into a number of subintervals
 *   and to approximate tanh(.) with a polynomial on each of them.
 *
 *   IEEE SPECIAL CONDITIONS:
 *   x = [+, -]0, r = [+, -]0
 *   x = +Inf,   r = +1
 *   x = -Inf,   r = -1
 *   x = QNaN,   r = QNaN
 *   x = SNaN,   r = QNaN
 *
 *
 *   ALGORITHM DETAILS
 *   We handle special values in a callout function, aside from main path
 *   computations. "Special" for this algorithm are:
 *   INF, NAN, |x| > HUGE_THRESHOLD
 *
 *
 *   Main path computations are organized as follows:
 *   Actually we split the interval [0, SATURATION_THRESHOLD)
 *   into a number of subintervals.  On each subinterval we approximate tanh(.)
 *   with a minimax polynomial of pre-defined degree. Polynomial coefficients
 *   are computed beforehand and stored in table. We also use
 *
 *       y := |x| + B,
 *
 *   here B depends on subinterval and is used to make argument
 *   closer to zero.
 *   We also add large fake interval [SATURATION_THRESHOLD, HUGE_THRESHOLD],
 *   where 1.0 + 0.0*y + 0.0*y^2 ... coefficients are stored - just to
 *   preserve main path computation logic but return 1.0 for all arguments.
 *
 *   Hence reconstruction looks as follows:
 *   we extract proper polynomial and range reduction coefficients
 *        (Pj and B), corresponding to subinterval, to which |x| belongs,
 *        and return
 *
 *       r := sign(x) * (P0 + P1 * y + ... + Pn * y^n)
 *
 *   NOTE: we use multiprecision technique to multiply and sum the first
 *         K terms of the polynomial. So Pj, j = 0..K are stored in
 *         table each as a pair of target precision numbers (Pj and PLj) to
 *         achieve wider than target precision.
 *
 *
 */

/* Offsets for data table __svml_dtanh_data_internal
 */
#define _dC				0
#define _dP0				128
#define _dP1				256
#define _dP2				384
#define _dP3				512
#define _dP4				640
#define _dP5				768
#define _dP6				896
#define _dP7				1024
#define _dP8				1152
#define _dP9				1280
#define _dP10				1408
#define _dP11				1536
#define _dP12				1664
#define _dP13				1792
#define _dP14				1920
#define _dP15				2048
#define _dP16				2176
#define _dP17				2304
#define _iExpMantMask_UISA		2432
#define _iMinIdxOfsMask_UISA		2496
#define _iMaxIdxMask_UISA		2560
#define _dbSignMask			2624
#define _dbAbsMask			2688
#define _iExpMantMask			2752
#define _iExpMask			2816
#define _iMinIdxOfsMask			2880
#define _iMaxIdxMask			2944

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_tanh_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$320, %rsp
	vpsrlq	$32, %zmm0, %zmm4
	vmovups	%zmm0, (%rsp)
	vmovups	__svml_dtanh_data_internal(%rip), %zmm14
	vmovups	_dP0+__svml_dtanh_data_internal(%rip), %zmm15
	vpmovqd	%zmm4, %ymm5

	/*  Constant loading  */
	vandpd	_dbAbsMask+__svml_dtanh_data_internal(%rip), %zmm0, %zmm13
	vandpd	_dbSignMask+__svml_dtanh_data_internal(%rip), %zmm0, %zmm3

	/* Here huge arguments, INF and NaNs are filtered out to callout. */
	vpand	_iExpMantMask_UISA+__svml_dtanh_data_internal(%rip), %ymm5, %ymm7
	vmovups	_dP2+__svml_dtanh_data_internal(%rip), %zmm0
	vmovups	_dP16+__svml_dtanh_data_internal(%rip), %zmm4
	vmovups	_dP15+__svml_dtanh_data_internal(%rip), %zmm5
	vmovups	%zmm3, 64(%rsp)
	vmovups	_dP3+__svml_dtanh_data_internal(%rip), %zmm3
	vpsubd	_iMinIdxOfsMask_UISA+__svml_dtanh_data_internal(%rip), %ymm7, %ymm8

	/* if VMIN, VMAX is defined for I type */
	vxorps	%ymm9, %ymm9, %ymm9
	vpmaxsd	%ymm9, %ymm8, %ymm10
	vpminsd	_iMaxIdxMask_UISA+__svml_dtanh_data_internal(%rip), %ymm10, %ymm11
	vpsrld	$19, %ymm11, %ymm12
	vmovups	_dP12+__svml_dtanh_data_internal(%rip), %zmm8
	vmovups	_dP11+__svml_dtanh_data_internal(%rip), %zmm9
	vmovups	_dP10+__svml_dtanh_data_internal(%rip), %zmm10
	vmovups	_dP9+__svml_dtanh_data_internal(%rip), %zmm11
	vpmovzxdq %ymm12, %zmm2
	vmovups	_dP8+__svml_dtanh_data_internal(%rip), %zmm12
	vpermt2pd _dP2+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm0
	vpermt2pd _dC+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm14
	vpermt2pd _dP16+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm4
	vpermt2pd _dP15+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm5
	vsubpd	{rn-sae}, %zmm14, %zmm13, %zmm1
	vpermt2pd _dP12+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm8
	vpermt2pd _dP11+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm9
	vpermt2pd _dP10+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm10
	vpermt2pd _dP9+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm11
	vpermt2pd _dP8+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm12
	vpermt2pd _dP3+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm3
	vpermt2pd _dP0+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm15
	vmovups	%zmm0, 192(%rsp)
	vmovups	_dP17+__svml_dtanh_data_internal(%rip), %zmm0
	vmovups	_dP7+__svml_dtanh_data_internal(%rip), %zmm13
	vmovups	_dP6+__svml_dtanh_data_internal(%rip), %zmm14
	vmovups	%zmm3, 256(%rsp)
	vmovups	_dP5+__svml_dtanh_data_internal(%rip), %zmm3
	vmovups	%zmm15, 128(%rsp)
	vmovups	_dP4+__svml_dtanh_data_internal(%rip), %zmm15
	vpermt2pd _dP17+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm0
	vpermt2pd _dP7+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm13
	vpermt2pd _dP6+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm14
	vpermt2pd _dP5+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm3
	vpermt2pd _dP4+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm15
	vfmadd213pd {rn-sae}, %zmm4, %zmm1, %zmm0
	vpcmpgtd _iExpMask+__svml_dtanh_data_internal(%rip), %ymm7, %ymm6
	vmovmskps %ymm6, %edx
	vmovups	_dP14+__svml_dtanh_data_internal(%rip), %zmm6
	vfmadd213pd {rn-sae}, %zmm5, %zmm1, %zmm0
	vmovups	_dP13+__svml_dtanh_data_internal(%rip), %zmm7
	vpermt2pd _dP14+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm6
	vpermt2pd _dP13+64+__svml_dtanh_data_internal(%rip), %zmm2, %zmm7
	vfmadd213pd {rn-sae}, %zmm6, %zmm1, %zmm0
	vmovups	256(%rsp), %zmm2
	vfmadd213pd {rn-sae}, %zmm7, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm8, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm9, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm10, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm11, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm12, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm13, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm14, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm3, %zmm1, %zmm0
	vmovups	128(%rsp), %zmm3
	vfmadd213pd {rn-sae}, %zmm15, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm2, %zmm1, %zmm0
	vmovups	192(%rsp), %zmm2
	vfmadd213pd {rn-sae}, %zmm2, %zmm1, %zmm0
	vfmadd213pd {rn-sae}, %zmm3, %zmm1, %zmm0
	vorpd	64(%rsp), %zmm0, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0

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
	vmovups	(%rsp), %zmm1
	vmovups	%zmm0, 128(%rsp)
	vmovups	%zmm1, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -304; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xfe, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -312; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xfe, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -320; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xfe, 0xff, 0xff, 0x22
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
	vmovups	128(%rsp), %zmm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -304; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xfe, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -312; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xfe, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -320; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xfe, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	64(%rsp, %r14, 8), %xmm0
	call	tanh@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_tanh_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dtanh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _dC[16][2];
	__declspec(align(64)) VUINT32 _dP0[16][2];
	__declspec(align(64)) VUINT32 _dP1[16][2];
	__declspec(align(64)) VUINT32 _dP2[16][2];
	__declspec(align(64)) VUINT32 _dP3[16][2];
	__declspec(align(64)) VUINT32 _dP4[16][2];
	__declspec(align(64)) VUINT32 _dP5[16][2];
	__declspec(align(64)) VUINT32 _dP6[16][2];
	__declspec(align(64)) VUINT32 _dP7[16][2];
	__declspec(align(64)) VUINT32 _dP8[16][2];
	__declspec(align(64)) VUINT32 _dP9[16][2];
	__declspec(align(64)) VUINT32 _dP10[16][2];
	__declspec(align(64)) VUINT32 _dP11[16][2];
	__declspec(align(64)) VUINT32 _dP12[16][2];
	__declspec(align(64)) VUINT32 _dP13[16][2];
	__declspec(align(64)) VUINT32 _dP14[16][2];
	__declspec(align(64)) VUINT32 _dP15[16][2];
	__declspec(align(64)) VUINT32 _dP16[16][2];
	__declspec(align(64)) VUINT32 _dP17[16][2];
	__declspec(align(64)) VUINT32 _iExpMantMask_UISA[16][1];
	__declspec(align(64)) VUINT32 _iMinIdxOfsMask_UISA[16][1];
	__declspec(align(64)) VUINT32 _iMaxIdxMask_UISA[16][1];
	__declspec(align(64)) VUINT32 _dbSignMask[8][2];
	__declspec(align(64)) VUINT32 _dbAbsMask[8][2];
	__declspec(align(64)) VUINT32 _iExpMantMask[16][1];
	__declspec(align(64)) VUINT32 _iExpMask[16][1];
	__declspec(align(64)) VUINT32 _iMinIdxOfsMask[16][1];
	__declspec(align(64)) VUINT32 _iMaxIdxMask[16][1];
} __svml_dtanh_data_internal;
#endif
__svml_dtanh_data_internal:
	/* _dC */
	.quad	0x0000000000000000, 0x3fcc000000000000, 0x3fd4000000000000, 0x3fdc000000000000
	.quad	0x3fe4000000000000, 0x3fec000000000000, 0x3ff4000000000000, 0x3ffc000000000000
	.quad	0x4004000000000000, 0x400c000000000000, 0x4014000000000000, 0x401c000000000000
	.quad	0x4024000000000000, 0x402c000000000000, 0x4034000000000000, 0x0000000000000000
	/* p0 */
	.align	64
	.quad	0x0000000000000000, 0x3fcb8fd0416a7c92, 0x3fd35f98a0ea650e, 0x3fda5729ee488037
	.quad	0x3fe1bf47eabb8f95, 0x3fe686650b8c2015, 0x3feb2523bb6b2dee, 0x3fee1fbf97e33527
	.quad	0x3fef9258260a71c2, 0x3feff112c63a9077, 0x3fefff419668df11, 0x3feffffc832750f2
	.quad	0x3feffffffdc96f35, 0x3fefffffffffcf58, 0x3ff0000000000000, 0x3ff0000000000000
	/* p1 */
	.align	64
	.quad	0x0000000000000000, 0x3c65e23ebcd3bcbe, 0xbc4c600bac3adf00, 0x3c6c44091785d040
	.quad	0x3c8221d7a6e3674b, 0x3c69f89d2cf6b85c, 0x3c73b3e9ec0b8f1c, 0xbc7f8d4b0428aada
	.quad	0xbc7c52d880cf43c0, 0x3c7dd36e37096480, 0x3c7b4f6380c442ca, 0xbc729755de470096
	.quad	0x3c84cf852845efbd, 0x3c6fc4fb440a5378, 0xbc63981083b55870, 0x0000000000000000
	/* p2 */
	.align	64
	.quad	0x3ff0000000000000, 0x3fee842ca3f08532, 0x3fed11574af58f1b, 0x3fea945b9c24e4f9
	.quad	0x3fe6284c3374f815, 0x3fe02500a09f8d6e, 0x3fd1f25131e3a8c0, 0x3fbd22ca1c24a139
	.quad	0x3f9b3afe1fba5c76, 0x3f6dd37d19b22b21, 0x3f27ccec13a9ef96, 0x3ecbe6c3f33250ae
	.quad	0x3e41b4865394f75f, 0x3d8853f01bda5f28, 0x3c73953c0197ef58, 0x0000000000000000
	/* p3 */
	.align	64
	.quad	0xbbf0b3ea3fdfaa19, 0xbfca48aaeb53bc21, 0xbfd19921f4329916, 0xbfd5e0f09bef8011
	.quad	0xbfd893b59c35c882, 0xbfd6ba7cb7576538, 0xbfce7291743d7555, 0xbfbb6d85a01efb80
	.quad	0xbf9addae58c7141a, 0xbf6dc59376c7aa19, 0xbf27cc5e74677410, 0xbecbe6c0e8b4cc87
	.quad	0xbe41b486526b0565, 0xbd8853f01bef63a4, 0xbc73955be519be31, 0x0000000000000000
	/* p4 */
	.align	64
	.quad	0xbfd5555555555555, 0xbfd183afc292ba11, 0xbfcc1a4b039c9bfa, 0xbfc16e1e6d8d0be6
	.quad	0xbf92426c751e48a2, 0x3fb4f152b2bad124, 0x3fbbba40cbef72be, 0x3fb01ba038be6a3d
	.quad	0x3f916df44871efc8, 0x3f63c6869dfc8870, 0x3f1fb9aef915d828, 0x3ec299d1e27c6e11
	.quad	0x3e379b5ddcca334c, 0x3d8037f57bc62c9a, 0x3c6a2d4b50a2cff7, 0x0000000000000000
	/* p5 */
	.align	64
	.quad	0xbce6863ee44ed636, 0x3fc04dcd0476c75e, 0x3fc43d3449a80f08, 0x3fc5c26f3699b7e7
	.quad	0x3fc1a686f6ab2533, 0x3faf203c316ce730, 0xbf89c7a02788557c, 0xbf98157e26e0d541
	.quad	0xbf807b55c1c7d278, 0xbf53a18d5843190f, 0xbf0fb6bbc89b1a5b, 0xbeb299c9c684a963
	.quad	0xbe279b5dd4fb3d01, 0xbd7037f57ae72aa6, 0xbc5a2ca2bba78e86, 0x0000000000000000
	/* p6 */
	.align	64
	.quad	0x3fc1111111112ab5, 0x3fb5c19efdfc08ad, 0x3fa74c98dc34fbac, 0xbf790d6a8eff0a77
	.quad	0xbfac3c021789a786, 0xbfae2196b7326859, 0xbf93a7a011ff8c2a, 0x3f6e4709c7e8430e
	.quad	0x3f67682afa611151, 0x3f3ef2ee77717cbf, 0x3ef95a4482f180b7, 0x3e9dc2c27da3b603
	.quad	0x3e12e2afd9f7433e, 0x3d59f320348679ba, 0x3c44b61d9bbcc940, 0x0000000000000000
	/* p7 */
	.align	64
	.quad	0xbda1ea19ddddb3b4, 0xbfb0b8df995ce4df, 0xbfb2955cf41e8164, 0xbfaf9d05c309f7c6
	.quad	0xbf987d27ccff4291, 0x3f8b2ca62572b098, 0x3f8f1cf6c7f5b00a, 0x3f60379811e43dd5
	.quad	0xbf4793826f78537e, 0xbf2405695e36240f, 0xbee0e08de39ce756, 0xbe83d709ba5f714e
	.quad	0xbdf92e3fc5ee63e0, 0xbd414cc030f2110e, 0xbc2ba022e8d82a87, 0x0000000000000000
	/* p8 */
	.align	64
	.quad	0xbfaba1ba1990520b, 0xbf96e37bba52f6fc, 0x3ecff7df18455399, 0x3f97362834d33a4e
	.quad	0x3f9e7f8380184b45, 0x3f869543e7c420d4, 0xbf7326bd4914222a, 0xbf5fc15b0a9d98fa
	.quad	0x3f14cffcfa69fbb6, 0x3f057e48e5b79d10, 0x3ec33b66d7d77264, 0x3e66ac4e578b9b10
	.quad	0x3ddcc74b8d3d5c42, 0x3d23c589137f92b4, 0x3c107f8e2c8707a1, 0x0000000000000000
	/* p9 */
	.align	64
	.quad	0xbe351ca7f096011f, 0x3f9eaaf3320c3851, 0x3f9cf823fe761fc1, 0x3f9022271754ff1f
	.quad	0xbf731fe77c9c60af, 0xbf84a6046865ec7d, 0xbf4ca3f1f2b9192b, 0x3f4c77dee0afd227
	.quad	0x3f04055bce68597a, 0xbee2bf0cb4a71647, 0xbea31eaafe73efd5, 0xbe46abb02c4368ed
	.quad	0xbdbcc749ca8079dd, 0xbd03c5883836b9d2, 0xbbf07a5416264aec, 0x0000000000000000
	/* p10 */
	.align	64
	.quad	0x3f9664f94e6ac14e, 0xbf94d3343bae39dd, 0xbf7bc748e60df843, 0xbf8c89372b43ba85
	.quad	0xbf8129a092de747a, 0x3f60c85b4d538746, 0x3f5be9392199ec18, 0xbf2a0c68a4489f10
	.quad	0xbf00462601dc2faa, 0x3eb7b6a219dea9f4, 0x3e80cbcc8d4c5c8a, 0x3e2425bb231a5e29
	.quad	0x3d9992a4beac8662, 0x3ce191ba5ed3fb67, 0x3bc892450bad44c4, 0x0000000000000000
	/* p11 */
	.align	64
	.quad	0xbea8c4c1fd7852fe, 0xbfccce16b1046f13, 0xbf81a16f224bb7b6, 0xbf62cbf00406bc09
	.quad	0x3f75b29bb02cf69b, 0x3f607df0f9f90c17, 0xbf4b852a6e0758d5, 0xbf0078c63d1b8445
	.quad	0x3eec12eadd55be7a, 0xbe6fa600f593181b, 0xbe5a3c935dce3f7d, 0xbe001c6d95e3ae96
	.quad	0xbd74755a00ea1fd3, 0xbcbc1c6c063bb7ac, 0xbba3be9a4460fe00, 0x0000000000000000
	/* p12 */
	.align	64
	.quad	0xbf822404577aa9dd, 0x403d8b07f7a82aa3, 0xbf9f44ab92fbab0a, 0x3fb2eac604473d6a
	.quad	0x3f45f87d903aaac8, 0xbf5e104671036300, 0x3f19bc98ddf0f340, 0x3f0d4304bc9246e8
	.quad	0xbed13c415f7b9d41, 0xbe722b8d9720cdb0, 0x3e322666d739bec0, 0x3dd76a553d7e7918
	.quad	0x3d4de0fa59416a39, 0x3c948716cf3681b4, 0x3b873f9f2d2fda99, 0x0000000000000000
	/* p13 */
	.align	64
	.quad	0xbefdd99a221ed573, 0x4070593a3735bab4, 0xbfccab654e44835e, 0x3fd13ed80037dbac
	.quad	0xbf6045b9076cc487, 0x3f2085ee7e8ac170, 0x3f23524622610430, 0xbeff12a6626911b4
	.quad	0x3eab9008bca408af, 0x3e634df71865f620, 0xbe05bb1bcf83ca73, 0xbdaf2ac143fb6762
	.quad	0xbd23eae52a3dbf57, 0xbc6b5e3e9ca0955e, 0xbb5eca68e2c1ba2e, 0x0000000000000000
	/* p14 */
	.align	64
	.quad	0x3f6e3be689423841, 0xc0d263511f5baac1, 0x40169f73b15ebe5c, 0xc025c1dd41cd6cb5
	.quad	0xbf58fd89fe05e0d1, 0x3f73f7af01d5af7a, 0xbf1e40bdead17e6b, 0x3ee224cd6c4513e5
	.quad	0xbe24b645e68eeaa3, 0xbe4abfebfb72bc83, 0x3dd51c38f8695ed3, 0x3d8313ac38c6832b
	.quad	0x3cf7787935626685, 0x3c401ffc49c6bc29, 0xbabf0b21acfa52ab, 0x0000000000000000
	/* p15 */
	.align	64
	.quad	0xbf2a1306713a4f3a, 0xc1045e509116b066, 0x4041fab9250984ce, 0xc0458d090ec3de95
	.quad	0xbf74949d60113d63, 0x3f7c9fd6200d0ade, 0x3f02cd40e0ad0a9f, 0xbe858ab8e019f311
	.quad	0xbe792fa6323b7cf8, 0x3e2df04d67876402, 0xbd95c72be95e4d2c, 0xbd55a89c30203106
	.quad	0xbccad6b3bb9eff65, 0xbc12705ccd3dd884, 0xba8e0a4c47ae75f5, 0x0000000000000000
	/* p16 */
	.align	64
	.quad	0xbf55d7e76dc56871, 0x41528c38809c90c7, 0xc076d57fb5190b02, 0x4085f09f888f8ada
	.quad	0x3fa246332a2fcba5, 0xbfb29d851a896fcd, 0x3ed9065ae369b212, 0xbeb8e1ba4c98a030
	.quad	0x3e6ffd0766ad4016, 0xbe0c63c29f505f5b, 0xbd7fab216b9e0e49, 0x3d2826b62056aa27
	.quad	0x3ca313e31762f523, 0x3bea37aa21895319, 0x3ae5c7f1fd871496, 0x0000000000000000
	/* p17 */
	.align	64
	.quad	0x3f35e67ab76a26e7, 0x41848ee0627d8206, 0xc0a216d618b489ec, 0x40a5b89107c8af4f
	.quad	0x3fb69d8374520eda, 0xbfbded519f981716, 0xbef02d288b5b3371, 0x3eb290981209c1a6
	.quad	0xbe567e924bf5ff6e, 0x3de3f7f7de6b0eb6, 0x3d69ed18bae3ebbc, 0xbcf7534c4f3dfa71
	.quad	0xbc730b73f1eaff20, 0xbbba2cff8135d462, 0xbab5a71b5f7d9035, 0x0000000000000000
	.align	64
	.long	0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000, 0x7ff80000 /* _iExpMantMask_UISA */
	.align	64
	.long	0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000, 0x3fc00000 /* _iMinIdxOfsMask_UISA */
	.align	64
	.long	0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000, 0x00780000 /* _iMaxIdxMask_UISA */
	.align	64
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000 /* _dbSignMask */
	.align	64
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff /* _dbAbsMask */
	.align	64
	.long	0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000, 0x7ffe0000 /* _iExpMantMask */
	.align	64
	.long	0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000 /* _iExpMask */
	.align	64
	.long	0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000, 0x3fbe0000 /* _iMinIdxOfsMask */
	.align	64
	.long	0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000, 0x00760000 /* _iMaxIdxMask */
	.align	64
	.type	__svml_dtanh_data_internal, @object
	.size	__svml_dtanh_data_internal, .-__svml_dtanh_data_internal
