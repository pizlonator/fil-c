/* Function coshf vectorized with AVX2.
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
 *   Compute cosh(x) as (exp(x)+exp(-x))/2,
 *   where exp is calculated as
 *   exp(M*ln2 + ln2*(j/2^k) + r) = 2^M * 2^(j/2^k) * exp(r)
 *
 *   Special cases:
 *
 *   cosh(NaN) = quiet NaN, and raise invalid exception
 *   cosh(INF) = that INF
 *   cosh(0)   = 1
 *   cosh(x) overflows for big x and returns MAXLOG+log(2)
 *
 */

/* Offsets for data table __svml_scosh_data_internal
 */
#define _sInvLn2			0
#define _sLn2hi				32
#define _sLn2lo				64
#define _sSign				96
#define _sShifter			128
#define _iDomainRange			160
#define _sPC1				192
#define _sPC2				224
#define _sPC3				256
#define _sPC4				288
#define _sPC5				320
#define _sPC6				352
#define _iHalf				384

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_coshf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	vmovups	_sSign+__svml_scosh_data_internal(%rip), %ymm2
	vmovups	_sShifter+__svml_scosh_data_internal(%rip), %ymm7

	/*
	 *  Load argument
	 * dM = x/log(2) + RShifter
	 */
	vmovups	_sInvLn2+__svml_scosh_data_internal(%rip), %ymm10
	vmovups	_sLn2hi+__svml_scosh_data_internal(%rip), %ymm8
	vmovups	_iDomainRange+__svml_scosh_data_internal(%rip), %ymm3

	/*
	 * sinh(r) = r*((a1=1)+r^2*(a3+r^2*(a5+{v1 r^2*a7})))) = r + r*(r^2*(a3+r^2*(a5+r^2*a7))) ....
	 * sSinh_r = (a3+r^2*a5)
	 */
	vmovups	_sPC5+__svml_scosh_data_internal(%rip), %ymm15
	vmovups	_iHalf+__svml_scosh_data_internal(%rip), %ymm11
	vmovaps	%ymm0, %ymm1

	/*
	 *  Implementation
	 *  Abs argument
	 */
	vandnps	%ymm1, %ymm2, %ymm0
	vfmadd213ps %ymm7, %ymm0, %ymm10

	/*
	 *  R
	 * sN = sM - RShifter
	 */
	vsubps	%ymm7, %ymm10, %ymm9

	/*
	 *  G1, G2 2^N, 2^(-N)
	 * iM now is an EXP(2^N)
	 */
	vpslld	$23, %ymm10, %ymm12

	/* Check for overflow\underflow  */
	vpcmpgtd %ymm3, %ymm0, %ymm4
	vpcmpeqd %ymm3, %ymm0, %ymm5

	/* sR = sX - sN*Log2_hi */
	vfnmadd231ps %ymm8, %ymm9, %ymm0
	vpaddd	%ymm12, %ymm11, %ymm13
	vpsubd	%ymm12, %ymm11, %ymm14
	vpor	%ymm5, %ymm4, %ymm6

	/* sR = (sX - sN*Log2_hi) - sN*Log2_lo */
	vfnmadd231ps _sLn2lo+__svml_scosh_data_internal(%rip), %ymm9, %ymm0

	/* sG1 = 2^(N-1)-2^(-N-1) */
	vsubps	%ymm14, %ymm13, %ymm4

	/* sG2 = 2^(N-1)+2^(-N-1) */
	vaddps	%ymm14, %ymm13, %ymm3

	/* sR2 = sR^2, shaffled */
	vmulps	%ymm0, %ymm0, %ymm2
	vfmadd213ps _sPC3+__svml_scosh_data_internal(%rip), %ymm2, %ymm15

	/* sSinh_r = r^2*(a3+r^2*a5) */
	vmulps	%ymm15, %ymm2, %ymm13

	/* sSinh_r = r + r*(r^2*(a3+r^2*a5)) */
	vfmadd213ps %ymm0, %ymm0, %ymm13

	/*
	 * sinh(X) = sG2 + sG1*sinh(dR) + sG2*sR2*(a2+sR2*(a4+a6*sR2)
	 * sOut = (a4 +a6*sR2)
	 */
	vmovups	_sPC6+__svml_scosh_data_internal(%rip), %ymm0
	vfmadd213ps _sPC4+__svml_scosh_data_internal(%rip), %ymm2, %ymm0

	/* sOut = a2+sR2*(a4+a6*sR2) */
	vfmadd213ps _sPC2+__svml_scosh_data_internal(%rip), %ymm2, %ymm0

	/* sOut = sR2*(a2+sR2*(a4+a6*sR2) */
	vmulps	%ymm0, %ymm2, %ymm15

	/* sOut = sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	vmulps	%ymm15, %ymm3, %ymm14

	/* sOut = sG1*sinh(dR)+sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	vfmadd213ps %ymm14, %ymm13, %ymm4
	vmovmskps %ymm6, %edx

	/* sOut = sG2 + sG1*sinh(dR) + sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	vaddps	%ymm4, %ymm3, %ymm0

	/*  Ret H  */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm1

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
	vmovups	%ymm1, 32(%rsp)
	vmovups	%ymm0, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
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
	vmovups	64(%rsp), %ymm0

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
	vmovss	32(%rsp, %r14, 4), %xmm0
	call	coshf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_coshf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_scosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _sInvLn2[8][1];
	__declspec(align(32)) VUINT32 _sLn2hi[8][1];
	__declspec(align(32)) VUINT32 _sLn2lo[8][1];
	__declspec(align(32)) VUINT32 _sSign[8][1];
	__declspec(align(32)) VUINT32 _sShifter[8][1];
	__declspec(align(32)) VUINT32 _iDomainRange[8][1];
	__declspec(align(32)) VUINT32 _sPC1[8][1];
	__declspec(align(32)) VUINT32 _sPC2[8][1];
	__declspec(align(32)) VUINT32 _sPC3[8][1];
	__declspec(align(32)) VUINT32 _sPC4[8][1];
	__declspec(align(32)) VUINT32 _sPC5[8][1];
	__declspec(align(32)) VUINT32 _sPC6[8][1];
	__declspec(align(32)) VUINT32 _iHalf[8][1];
} __svml_scosh_data_internal;
#endif
__svml_scosh_data_internal:
	.long	0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B /* _sInvLn2 */ // k=0
	.align	32
	.long	0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000 /* _sLn2hi */
	.align	32
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4 /* _sLn2lo */
	.align	32
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000 /* _sSign */
	.align	32
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter */
	.align	32
	.long	0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E /* _iDomainRange */
	.align	32
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 /* _sPC1=1 */
	.align	32
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _sPC2 */
	.align	32
	.long	0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57 /* _sPC3 */
	.align	32
	.long	0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72 /* _sPC4 */
	.align	32
	.long	0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461 /* _sPC5 */
	.align	32
	.long	0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3 /* _sPC6 */
	// Integer constants
	.align	32
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _iHalf */
	.align	32
	.type	__svml_scosh_data_internal, @object
	.size	__svml_scosh_data_internal, .-__svml_scosh_data_internal
