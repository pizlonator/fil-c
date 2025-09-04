/* Function hypotf vectorized with AVX-512.
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
#define _sAbsMask			0
#define _sHalf				64
#define _iExpBound			128

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16vv_hypotf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$256, %rsp
	vgetexpps {sae}, %zmm0, %zmm2
	vgetexpps {sae}, %zmm1, %zmm3
	vmovups	_sHalf+__svml_shypot_data_internal(%rip), %zmm6
	vmaxps	{sae}, %zmm3, %zmm2, %zmm4
	vmulps	{rn-sae}, %zmm0, %zmm0, %zmm2
	vandps	_sAbsMask+__svml_shypot_data_internal(%rip), %zmm4, %zmm5
	vfmadd231ps {rn-sae}, %zmm1, %zmm1, %zmm2
	vpcmpd	$5, _iExpBound+__svml_shypot_data_internal(%rip), %zmm5, %k0
	vrsqrt14ps %zmm2, %zmm7
	kmovw	%k0, %edx
	vmulps	{rn-sae}, %zmm7, %zmm2, %zmm9
	vmulps	{rn-sae}, %zmm7, %zmm6, %zmm8
	vfnmadd231ps {rn-sae}, %zmm9, %zmm9, %zmm2
	vfmadd213ps {rn-sae}, %zmm9, %zmm8, %zmm2

	/*
	 * VSCALEF( S, _VRES1, _VRES1, sExp );
	 *  The end of implementation
	 */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1 zmm2

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm2, %zmm0
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
	vmovups	%zmm1, 128(%rsp)
	vmovups	%zmm2, 192(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm2

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
	vmovups	192(%rsp), %zmm2

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm2

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	64(%rsp, %r14, 4), %xmm0
	vmovss	128(%rsp, %r14, 4), %xmm1
	call	hypotf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 192(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16vv_hypotf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_shypot_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _sAbsMask[16][1];
	__declspec(align(64)) VUINT32 _sHalf[16][1];
	__declspec(align(64)) VUINT32 _iExpBound[16][1];
} __svml_shypot_data_internal;
#endif
__svml_shypot_data_internal:
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _sAbsMask */
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _sHalf */
	/* fma based algorithm*/
	.align	64
	.long	0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000 /* _iExpBound */
	.align	64
	.type	__svml_shypot_data_internal, @object
	.size	__svml_shypot_data_internal, .-__svml_shypot_data_internal
