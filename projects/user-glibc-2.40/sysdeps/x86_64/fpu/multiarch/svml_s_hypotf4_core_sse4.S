/* Function hypotf vectorized with SSE4.
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
 *      Calculate make two NR iterations
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
 *    Check _z exponent to be within borders [1E3 ; 60A] else goto Callout
 *
 *    Compute resciplicle sqrt s0 ~ 1.0/sqrt(_z),
 *      that multiplied by _z, is final result for _EP_ version.
 *
 *    First iteration (or zero iteration):
 *       s =  z * s0
 *       h = .5 * s0
 *       d =  s *  h - .5
 *
 *    Second iteration:
 *       h = d * h + h
 *       s = s * d + s
 *       d = s * s - z (in multiprecision for _HA_)
 *
 *    result = s - h * d
 *
 *    EP version of the function can be implemented as y[i]=sqrt(a[i]^2+b[i]^2)
 *    with all intermediate operations done in target precision for i=1, .., n.
 *    It can return result y[i]=0 in case a[i]^2 and b[i]^2 underflow in target
 *    precision (for some i). It can return result y[i]=NAN in case
 *    a[i]^2+b[i]^2 overflow in target precision, for some i. It can return
 *    result y[i]=NAN in case a[i] or b[i] is infinite, for some i.
 *
 *
 */

/* Offsets for data table __svml_shypot_data_internal
 */
#define _sHiLoMask			0
#define _sAbsMask			16
#define _sHalf				32
#define _LowBoundary			48
#define _HighBoundary			64

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4vv_hypotf_sse4)
	subq	$88, %rsp
	cfi_def_cfa_offset(96)

	/*
	 *  Implementation
	 * Multiprecision branch for _HA_ only
	 * No multiprecision branch for _LA_
	 * _z = _VARG1 * _VARG1 + _VARG2 * _VARG2
	 */
	movaps	%xmm0, %xmm8
	movaps	%xmm1, %xmm2
	mulps	%xmm0, %xmm8
	mulps	%xmm1, %xmm2

	/*
	 *  Variables
	 *  Defines
	 *  Constants loading
	 */
	movups	_sHalf+__svml_shypot_data_internal(%rip), %xmm5
	addps	%xmm2, %xmm8

	/* _s0  ~ 1.0/sqrt(_z) */
	rsqrtps	%xmm8, %xmm10

	/* First iteration */
	movaps	%xmm10, %xmm2
	movaps	%xmm8, %xmm3
	mulps	%xmm8, %xmm2
	mulps	%xmm5, %xmm10
	movaps	%xmm2, %xmm6
	mulps	%xmm10, %xmm6

	/* Check _z exponent to be within borders [1E3 ; 60A] else goto Callout */
	movdqu	_LowBoundary+__svml_shypot_data_internal(%rip), %xmm4
	subps	%xmm6, %xmm5

	/* Second iteration */
	movaps	%xmm5, %xmm7
	pcmpgtd	%xmm8, %xmm4
	mulps	%xmm2, %xmm5
	mulps	%xmm10, %xmm7
	addps	%xmm5, %xmm2
	addps	%xmm7, %xmm10

	/* Finish second iteration in native precision for _LA_ */
	movaps	%xmm2, %xmm9
	mulps	%xmm2, %xmm9
	pcmpgtd	_HighBoundary+__svml_shypot_data_internal(%rip), %xmm3
	subps	%xmm8, %xmm9
	mulps	%xmm9, %xmm10
	por	%xmm3, %xmm4
	movmskps %xmm4, %edx
	subps	%xmm10, %xmm2

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
	movss	32(%rsp, %r14, 4), %xmm0
	movss	48(%rsp, %r14, 4), %xmm1
	call	hypotf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4vv_hypotf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_shypot_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _sHiLoMask[4][1];
	__declspec(align(16)) VUINT32 _sAbsMask[4][1];
	__declspec(align(16)) VUINT32 _sHalf[4][1];
	__declspec(align(16)) VUINT32 _LowBoundary[4][1];
	__declspec(align(16)) VUINT32 _HighBoundary[4][1];
} __svml_shypot_data_internal;
#endif
__svml_shypot_data_internal:
	/* legacy algorithm */
	.long	0xFFF80000, 0xFFF80000, 0xFFF80000, 0xFFF80000 /* _sHiLoMask */
	.align	16
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _sAbsMask */
	.align	16
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _sHalf */
	.align	16
	.long	0x1E300000, 0x1E300000, 0x1E300000, 0x1E300000 /* _LowBoundary */
	.align	16
	.long	0x60A00000, 0x60A00000, 0x60A00000, 0x60A00000 /* _HighBoundary */
	.align	16
	.type	__svml_shypot_data_internal, @object
	.size	__svml_shypot_data_internal, .-__svml_shypot_data_internal
