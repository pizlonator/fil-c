/* Function hypotf vectorized with AVX2.
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
#define _sAbsMask			32
#define _sHalf				64
#define _LowBoundary			96
#define _HighBoundary			128

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8vv_hypotf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$128, %rsp

	/*
	 *  Implementation
	 * Multiprecision branch for _HA_ only
	 * No multiprecision branch for _LA_
	 * _z = _VARG1 * _VARG1 + _VARG2 * _VARG2
	 */
	vmulps	%ymm0, %ymm0, %ymm8

	/*
	 *  Variables
	 *  Defines
	 *  Constants loading
	 */
	vmovups	_sHalf+__svml_shypot_data_internal(%rip), %ymm7

	/* Check _z exponent to be within borders [1E3 ; 60A] else goto Callout */
	vmovups	_LowBoundary+__svml_shypot_data_internal(%rip), %ymm2
	vfmadd231ps %ymm1, %ymm1, %ymm8

	/* _s0  ~ 1.0/sqrt(_z) */
	vrsqrtps %ymm8, %ymm6
	vpcmpgtd %ymm8, %ymm2, %ymm3

	/* First iteration */
	vmulps	%ymm8, %ymm6, %ymm9
	vmulps	%ymm7, %ymm6, %ymm2
	vfnmadd231ps %ymm9, %ymm2, %ymm7
	vfmadd213ps %ymm9, %ymm7, %ymm9

	/* Second iteration */
	vfmadd132ps %ymm7, %ymm2, %ymm2
	vpcmpgtd _HighBoundary+__svml_shypot_data_internal(%rip), %ymm8, %ymm4
	vpor	%ymm4, %ymm3, %ymm5

	/* Finish second iteration in native precision for _LA_ */
	vfmsub231ps %ymm9, %ymm9, %ymm8
	vmovmskps %ymm5, %edx
	vfnmadd213ps %ymm9, %ymm8, %ymm2

	/*  The end of implementation  */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm1 ymm2

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%ymm2, %ymm0
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
	vmovups	%ymm0, 32(%rsp)
	vmovups	%ymm1, 64(%rsp)
	vmovups	%ymm2, 96(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm2

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
	vmovups	96(%rsp), %ymm2

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm2

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	32(%rsp, %r14, 4), %xmm0
	vmovss	64(%rsp, %r14, 4), %xmm1
	call	hypotf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 96(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8vv_hypotf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_shypot_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _sHiLoMask[8][1];
	__declspec(align(32)) VUINT32 _sAbsMask[8][1];
	__declspec(align(32)) VUINT32 _sHalf[8][1];
	__declspec(align(32)) VUINT32 _LowBoundary[8][1];
	__declspec(align(32)) VUINT32 _HighBoundary[8][1];
} __svml_shypot_data_internal;
#endif
__svml_shypot_data_internal:
	/* legacy algorithm */
	.long	0xFFF80000, 0xFFF80000, 0xFFF80000, 0xFFF80000, 0xFFF80000, 0xFFF80000, 0xFFF80000, 0xFFF80000 /* _sHiLoMask */
	.align	32
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _sAbsMask */
	.align	32
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _sHalf */
	.align	32
	.long	0x1E300000, 0x1E300000, 0x1E300000, 0x1E300000, 0x1E300000, 0x1E300000, 0x1E300000, 0x1E300000 /* _LowBoundary */
	.align	32
	.long	0x60A00000, 0x60A00000, 0x60A00000, 0x60A00000, 0x60A00000, 0x60A00000, 0x60A00000, 0x60A00000 /* _HighBoundary */
	.align	32
	.type	__svml_shypot_data_internal, @object
	.size	__svml_shypot_data_internal, .-__svml_shypot_data_internal
