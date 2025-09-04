/* Function exp2f vectorized with SSE4.
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
 *   exp2(x)  = 2^n * T[j] * (1 + P(y))
 *   where
 *        x = m*(1/K) + y,    y in [-1/K..1/K]
 *        m = n*K + j,           m, n,j - signed integer, j in [-K/2..K/2]
 *
 *        values of 2^j/K are tabulated
 *
 *        P(y) is a minimax polynomial approximation of exp2(x)-1
 *        on small interval [-1/K..1/K]
 *
 *  Special cases:
 *
 *   exp2(NaN)  = NaN
 *   exp2(+INF) = +INF
 *   exp2(-INF) = 0
 *   exp2(x)    = 1 for subnormals
 *   For IEEE float
 *     if x >= 128.0 then exp2f(x) overflow
 *     if x < -151.0 then exp2f(x) underflow
 *
 */

/* Offsets for data table __svml_sexp2_data_internal
 */
#define _sShifter			0
#define _sPC0				16
#define _sPC1				32
#define _sPC2				48
#define _sPC3				64
#define _sPC4				80
#define _sPC5				96
#define _sPC6				112
#define _iAbsMask			128
#define _iDomainRange			144

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_exp2f_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/* Check for overflow\underflow  */
	movups	__svml_sexp2_data_internal(%rip), %xmm1

	/*  Implementation  */
	movaps	%xmm1, %xmm5

	/*  Polynomial  */
	movups	_sPC6+__svml_sexp2_data_internal(%rip), %xmm4
	addps	%xmm0, %xmm5
	movaps	%xmm5, %xmm3

	/*  2^N  */
	pslld	$23, %xmm5

	/* Check for overflow\underflow  */
	movdqu	_iAbsMask+__svml_sexp2_data_internal(%rip), %xmm2
	subps	%xmm1, %xmm3

	/*  R  */
	movaps	%xmm0, %xmm1
	pand	%xmm0, %xmm2
	pcmpgtd	_iDomainRange+__svml_sexp2_data_internal(%rip), %xmm2
	subps	%xmm3, %xmm1
	movmskps %xmm2, %edx
	mulps	%xmm1, %xmm4
	addps	_sPC5+__svml_sexp2_data_internal(%rip), %xmm4
	mulps	%xmm1, %xmm4
	addps	_sPC4+__svml_sexp2_data_internal(%rip), %xmm4
	mulps	%xmm1, %xmm4
	addps	_sPC3+__svml_sexp2_data_internal(%rip), %xmm4
	mulps	%xmm1, %xmm4
	addps	_sPC2+__svml_sexp2_data_internal(%rip), %xmm4
	mulps	%xmm1, %xmm4
	addps	_sPC1+__svml_sexp2_data_internal(%rip), %xmm4
	mulps	%xmm4, %xmm1
	addps	_sPC0+__svml_sexp2_data_internal(%rip), %xmm1

	/*  Reconstruction  */
	paddd	%xmm5, %xmm1
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm1

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm1, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm1, 48(%rsp)
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
	movups	48(%rsp), %xmm1

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm1

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	exp2f@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_exp2f_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sexp2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _sShifter[4][1];
	__declspec(align(16)) VUINT32 _sPC0[4][1];
	__declspec(align(16)) VUINT32 _sPC1[4][1];
	__declspec(align(16)) VUINT32 _sPC2[4][1];
	__declspec(align(16)) VUINT32 _sPC3[4][1];
	__declspec(align(16)) VUINT32 _sPC4[4][1];
	__declspec(align(16)) VUINT32 _sPC5[4][1];
	__declspec(align(16)) VUINT32 _sPC6[4][1];
	__declspec(align(16)) VUINT32 _iAbsMask[4][1];
	__declspec(align(16)) VUINT32 _iDomainRange[4][1];
} __svml_sexp2_data_internal;
#endif
__svml_sexp2_data_internal:
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter */
	.align	16
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 /* _sPC0 */
	.align	16
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218 /* _sPC1 */
	.align	16
	.long	0x3e75fdef, 0x3e75fdef, 0x3e75fdef, 0x3e75fdef /* _sPC2 */
	.align	16
	.long	0x3d6357cf, 0x3d6357cf, 0x3d6357cf, 0x3d6357cf /* _sPC3 */
	.align	16
	.long	0x3c1d962c, 0x3c1d962c, 0x3c1d962c, 0x3c1d962c /* _sPC4 */
	.align	16
	.long	0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51, 0x3aaf7a51 /* _sPC5 */
	.align	16
	.long	0x39213c8c, 0x39213c8c, 0x39213c8c, 0x39213c8c /* _sPC6 */
	//common
	.align	16
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	16
	.long	0x42fc0000, 0x42fc0000, 0x42fc0000, 0x42fc0000 /* _iDomainRange=126.0 */
	.align	16
	.type	__svml_sexp2_data_internal, @object
	.size	__svml_sexp2_data_internal, .-__svml_sexp2_data_internal
