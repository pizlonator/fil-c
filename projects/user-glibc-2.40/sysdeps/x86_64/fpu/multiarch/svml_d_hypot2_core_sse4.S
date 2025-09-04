/* Function hypot vectorized with SSE4.
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
 *      HIGH LEVEL OVERVIEW
 *
 *      Calculate z = (x*x+y*y)
 *      Calculate reciplicle sqrt (z)
 *      Calculate error = z*(rsqrt(z)*rsqrt(z)) - 1
 *      Calculate fixing part p with polynom
 *      Fix answer with sqrt(z) = z * rsqrt(z) + error * p * z
 *
 *      ALGORITHM DETAILS
 *
 *    Multiprecision branch for _HA_ only
 *      Remove sigm from both arguments
 *      Find maximum (_x) and minimum (_y) (by abs value) between arguments
 *      Split _x int _a and _b for multiprecision
 *      If _x >> _y we will we will not split _y for multiprecision
 *      all _y will be put into lower part (_d) and higher part (_c = 0)
 *      Fixing _hilo_mask for the case _x >> _y
 *      Split _y into _c and _d for multiprecision with fixed mask
 *
 *      compute Hi and Lo parts of _z = _x*_x + _y*_y
 *
 *      _zHi = _a*_a + _c*_c
 *      _zLo = (_x + _a)*_b + _d*_y + _d*_c
 *      _z = _zHi + _zLo
 *
 *    No multiprecision branch for _LA_ and _EP_
 *      _z = _VARG1 * _VARG1 + _VARG2 * _VARG2
 *
 *    Check _z exponent to be within borders [3BC ; 441] else goto Callout
 *
 *    _s  ~ 1.0/sqrt(_z)
 *    _s2 ~ 1.0/(sqrt(_z)*sqrt(_z)) ~ 1.0/_z = (1.0/_z + O)
 *    _e[rror]  =  (1.0/_z + O) * _z - 1.0
 *    calculate fixing part _p
 *    _p = (((_POLY_C5*_e + _POLY_C4)*_e +_POLY_C3)*_e +_POLY_C2)*_e + _POLY_C1
 *    some parts of polynom are skipped for lower flav
 *
 *    result = _z * (1.0/sqrt(_z) + O) + _p * _e[rror] * _z
 *
 *
 */

/* Offsets for data table __svml_dhypot_data_internal
 */
#define _dHiLoMask			0
#define _dAbsMask			16
#define _dOne				32
#define _POLY_C5			48
#define _POLY_C4			64
#define _POLY_C3			80
#define _POLY_C2			96
#define _POLY_C1			112
#define _LowBoundary			128
#define _HighBoundary			144

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2vv_hypot_sse4)
	subq	$88, %rsp
	cfi_def_cfa_offset(96)

	/*
	 *  Defines
	 *  Implementation
	 * Multiprecision branch for _HA_ only
	 * _z = _VARG1 * _VARG1 + _VARG2 * _VARG2
	 */
	movaps	%xmm0, %xmm10
	movaps	%xmm1, %xmm2
	mulpd	%xmm0, %xmm10
	mulpd	%xmm1, %xmm2
	addpd	%xmm2, %xmm10

	/*
	 * _s  ~ 1.0/sqrt(_z)
	 * _s2 ~ 1.0/(sqrt(_z)*sqrt(_z)) ~ 1.0/_z
	 */
	cvtpd2ps %xmm10, %xmm7
	movlhps	%xmm7, %xmm7
	rsqrtps	%xmm7, %xmm8
	cvtps2pd %xmm8, %xmm11
	movaps	%xmm11, %xmm2
	mulpd	%xmm11, %xmm2

	/* _e[rror]  ~  (1.0/_z + O) * _z - 1.0 */
	mulpd	%xmm10, %xmm2
	subpd	_dOne+__svml_dhypot_data_internal(%rip), %xmm2

	/*
	 * calculate fixing part _p
	 * _p = (((_POLY_C5*_e + _POLY_C4)*_e +_POLY_C3)*_e +_POLY_C2)*_e + _POLY_C1
	 * some parts of polynom are skipped for lower flav
	 */
	movups	_POLY_C4+__svml_dhypot_data_internal(%rip), %xmm9
	mulpd	%xmm2, %xmm9
	addpd	_POLY_C3+__svml_dhypot_data_internal(%rip), %xmm9
	mulpd	%xmm2, %xmm9
	addpd	_POLY_C2+__svml_dhypot_data_internal(%rip), %xmm9
	mulpd	%xmm2, %xmm9
	addpd	_POLY_C1+__svml_dhypot_data_internal(%rip), %xmm9

	/* result = _z * (1.0/sqrt(_z) + O) + _p * _e[rror] * _z */
	mulpd	%xmm9, %xmm2
	mulpd	%xmm11, %xmm2
	mulpd	%xmm10, %xmm11
	mulpd	%xmm10, %xmm2

	/* Check _z exponent to be within borders [3BC ; 441] else goto Callout */
	movq	_LowBoundary+__svml_dhypot_data_internal(%rip), %xmm5
	movq	_HighBoundary+__svml_dhypot_data_internal(%rip), %xmm3
	pshufd	$221, %xmm10, %xmm4
	pcmpgtd	%xmm4, %xmm5
	pcmpgtd	%xmm3, %xmm4
	por	%xmm4, %xmm5
	pshufd	$80, %xmm5, %xmm6
	movmskpd %xmm6, %edx
	addpd	%xmm11, %xmm2

	/*  The end of implementation  */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm1 xmm2

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm2, %xmm0
	addq	$88, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(96)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm1, 48(%rsp)
	movups	%xmm2, 64(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 edx

	xorl	%eax, %eax
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
	movups	64(%rsp), %xmm2

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -80)
	cfi_offset(13, -88)
	cfi_offset(14, -96)
	# LOE rbx rbp r12 r13 r14 r15 xmm2

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	movsd	48(%rsp, %r14, 8), %xmm1
	call	hypot@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 64(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN2vv_hypot_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dhypot_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _dHiLoMask[2][2];
	__declspec(align(16)) VUINT32 _dAbsMask[2][2];
	__declspec(align(16)) VUINT32 _dOne[2][2];
	__declspec(align(16)) VUINT32 _POLY_C5[2][2];
	__declspec(align(16)) VUINT32 _POLY_C4[2][2];
	__declspec(align(16)) VUINT32 _POLY_C3[2][2];
	__declspec(align(16)) VUINT32 _POLY_C2[2][2];
	__declspec(align(16)) VUINT32 _POLY_C1[2][2];
	__declspec(align(16)) VUINT32 _LowBoundary[4][1];
	__declspec(align(16)) VUINT32 _HighBoundary[4][1];
} __svml_dhypot_data_internal;
#endif
__svml_dhypot_data_internal:
	/* legacy algorithm */
	.quad	0xffffc00000000000, 0xffffc00000000000 /* _dHiLoMask */
	.align	16
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff /* _dAbsMask */
	.align	16
	.quad	0x3FF0000000000000, 0x3FF0000000000000 /* _dOne */
	.align	16
	.quad	0xBFCF800000000000, 0xBFCF800000000000 /* _POLY_C5 */
	.align	16
	.quad	0x3FD1800000000000, 0x3FD1800000000000 /* _POLY_C4 */
	.align	16
	.quad	0xBFD4000000000000, 0xBFD4000000000000 /* _POLY_C3 */
	.align	16
	.quad	0x3FD8000000000000, 0x3FD8000000000000 /* _POLY_C2 */
	.align	16
	.quad	0xBFE0000000000000, 0xBFE0000000000000 /* _POLY_C1 */
	.align	16
	.long	0x3BC00000, 0x3BC00000, 0x3BC00000, 0x3BC00000 /* _LowBoundary */
	.align	16
	.long	0x44100000, 0x44100000, 0x44100000, 0x44100000 /* _HighBoundary */
	.align	16
	.type	__svml_dhypot_data_internal, @object
	.size	__svml_dhypot_data_internal, .-__svml_dhypot_data_internal
