/* Function coshf vectorized with SSE4.
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
#define _sLn2hi				16
#define _sLn2lo				32
#define _sSign				48
#define _sShifter			64
#define _iDomainRange			80
#define _sPC1				96
#define _sPC2				112
#define _sPC3				128
#define _sPC4				144
#define _sPC5				160
#define _sPC6				176
#define _iHalf				192

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_coshf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/*
	 *  Implementation
	 *  Abs argument
	 */
	movups	_sSign+__svml_scosh_data_internal(%rip), %xmm1

	/*
	 *  Load argument
	 * dM = x/log(2) + RShifter
	 */
	movups	_sInvLn2+__svml_scosh_data_internal(%rip), %xmm9
	andnps	%xmm0, %xmm1
	mulps	%xmm1, %xmm9

	/* Check for overflow\underflow  */
	movaps	%xmm1, %xmm3
	movups	_sShifter+__svml_scosh_data_internal(%rip), %xmm4
	movups	_sLn2hi+__svml_scosh_data_internal(%rip), %xmm5
	addps	%xmm4, %xmm9

	/*
	 *  R
	 * sN = sM - RShifter
	 */
	movaps	%xmm9, %xmm6

	/*
	 *  G1, G2 2^N, 2^(-N)
	 * iM now is an EXP(2^N)
	 */
	pslld	$23, %xmm9
	movups	_sLn2lo+__svml_scosh_data_internal(%rip), %xmm7
	subps	%xmm4, %xmm6

	/* sR = sX - sN*Log2_hi */
	mulps	%xmm6, %xmm5

	/* sR = (sX - sN*Log2_hi) - sN*Log2_lo */
	mulps	%xmm6, %xmm7
	movdqu	_iDomainRange+__svml_scosh_data_internal(%rip), %xmm2
	pcmpgtd	%xmm2, %xmm3
	pcmpeqd	%xmm1, %xmm2

	/*
	 * sinh(r) = r*((a1=1)+r^2*(a3+r^2*(a5+{v1 r^2*a7})))) = r + r*(r^2*(a3+r^2*(a5+r^2*a7))) ....
	 * sSinh_r = (a3+r^2*a5)
	 */
	movups	_sPC5+__svml_scosh_data_internal(%rip), %xmm10
	por	%xmm2, %xmm3

	/*
	 * sinh(X) = sG2 + sG1*sinh(dR) + sG2*sR2*(a2+sR2*(a4+a6*sR2)
	 * sOut = (a4 +a6*sR2)
	 */
	movups	_sPC6+__svml_scosh_data_internal(%rip), %xmm11
	subps	%xmm5, %xmm1
	movmskps %xmm3, %edx
	movdqu	_iHalf+__svml_scosh_data_internal(%rip), %xmm8
	subps	%xmm7, %xmm1

	/* sR2 = sR^2, shaffled */
	movaps	%xmm1, %xmm13
	movdqa	%xmm8, %xmm2
	mulps	%xmm1, %xmm13
	paddd	%xmm9, %xmm2
	mulps	%xmm13, %xmm10
	psubd	%xmm9, %xmm8
	mulps	%xmm13, %xmm11
	addps	_sPC3+__svml_scosh_data_internal(%rip), %xmm10
	addps	_sPC4+__svml_scosh_data_internal(%rip), %xmm11

	/* sSinh_r = r^2*(a3+r^2*a5) */
	mulps	%xmm13, %xmm10

	/* sOut = a2+sR2*(a4+a6*sR2) */
	mulps	%xmm13, %xmm11

	/* sSinh_r = r + r*(r^2*(a3+r^2*a5)) */
	mulps	%xmm1, %xmm10
	addps	_sPC2+__svml_scosh_data_internal(%rip), %xmm11
	addps	%xmm10, %xmm1

	/* sOut = sR2*(a2+sR2*(a4+a6*sR2) */
	mulps	%xmm11, %xmm13

	/* sG1 = 2^(N-1)-2^(-N-1) */
	movdqa	%xmm2, %xmm12

	/* sG2 = 2^(N-1)+2^(-N-1) */
	addps	%xmm8, %xmm2
	subps	%xmm8, %xmm12

	/* sOut = sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	mulps	%xmm2, %xmm13

	/* sOut = sG1*sinh(dR)+sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	mulps	%xmm1, %xmm12
	addps	%xmm12, %xmm13

	/* sOut = sG2 + sG1*sinh(dR) + sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	addps	%xmm13, %xmm2

	/*  Ret H  */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm2

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm2, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm2, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 edx

	xorl	%eax, %eax
	movq	%r12, 16(%rsp)
	cfi_offset(12, -64)
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -72)
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	cfi_offset(14, -80)
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
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm2

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm2

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	coshf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_coshf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_scosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _sInvLn2[4][1];
	__declspec(align(16)) VUINT32 _sLn2hi[4][1];
	__declspec(align(16)) VUINT32 _sLn2lo[4][1];
	__declspec(align(16)) VUINT32 _sSign[4][1];
	__declspec(align(16)) VUINT32 _sShifter[4][1];
	__declspec(align(16)) VUINT32 _iDomainRange[4][1];
	__declspec(align(16)) VUINT32 _sPC1[4][1];
	__declspec(align(16)) VUINT32 _sPC2[4][1];
	__declspec(align(16)) VUINT32 _sPC3[4][1];
	__declspec(align(16)) VUINT32 _sPC4[4][1];
	__declspec(align(16)) VUINT32 _sPC5[4][1];
	__declspec(align(16)) VUINT32 _sPC6[4][1];
	__declspec(align(16)) VUINT32 _iHalf[4][1];
} __svml_scosh_data_internal;
#endif
__svml_scosh_data_internal:
	.long	0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B /* _sInvLn2 */ // k=0
	.align	16
	.long	0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000 /* _sLn2hi */
	.align	16
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4 /* _sLn2lo */
	.align	16
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000 /* _sSign */
	.align	16
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter */
	.align	16
	.long	0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E /* _iDomainRange */
	.align	16
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 /* _sPC1=1 */
	.align	16
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _sPC2 */
	.align	16
	.long	0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57 /* _sPC3 */
	.align	16
	.long	0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72 /* _sPC4 */
	.align	16
	.long	0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461 /* _sPC5 */
	.align	16
	.long	0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3 /* _sPC6 */
	// Integer constants
	.align	16
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _iHalf */
	.align	16
	.type	__svml_scosh_data_internal, @object
	.size	__svml_scosh_data_internal, .-__svml_scosh_data_internal
