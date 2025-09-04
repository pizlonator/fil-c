/* Function tanhf vectorized with SSE4.
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


#include <sysdep.h>

/* tanhf data tables for avx2 and sse4 implementations defined here.
 */
#define ONLY_DECL_OFFSET
#include "svml_s_tanhf_rodata.S"

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_tanhf_sse4)
	/* Save copy of input in xmm12.  */
	movaps	%xmm0, %xmm12

	/* Here huge arguments, INF and NaNs are filtered out to callout. */
	movdqu	TANHF_DATA(_iExpMantMask)(%rip), %xmm3
	pand	%xmm0, %xmm3


	/* Selection of arguments between [0, 0x04280000] into xmm3.  */
	pxor	%xmm7, %xmm7
	/* Save xmm3 for special values check at end.  */
	movdqa	%xmm3, %xmm8
	psubd	TANHF_DATA(_iMinIdxOfsMask)(%rip), %xmm3
	pmaxsd	%xmm7, %xmm3
	pminsd	TANHF_DATA(_iMaxIdxMask)(%rip), %xmm3
	psrld	$14, %xmm3

	movq	%xmm3, %rcx
	movl	%ecx, %edx
	shrq	$32, %rcx

	pshufd	$0x0e, %xmm3, %xmm3
	movq	%xmm3, %rdi
	movl	%edi, %esi
	shrq	$32, %rdi

	movaps	TANHF_DATA(_sAbsMask)(%rip), %xmm1
	andps	%xmm1, %xmm0

	leaq	TANHF_DATA(_lookupTable)(%rip), %rax
	movups	(%rdx, %rax), %xmm2
	movups	(%rcx, %rax), %xmm6

	/*
	 *  small table specific variables *
	 *  Constant loading
	 */
	movaps	%xmm2, %xmm4
	movlhps	%xmm6, %xmm4
	unpckhpd %xmm6, %xmm2

	cvtps2pd %xmm0, %xmm6
	movhlps	%xmm0, %xmm0
	cvtps2pd %xmm0, %xmm0

	movups	16(%rdx, %rax), %xmm5
	movups	16(%rsi, %rax), %xmm13

	movaps	%xmm5, %xmm10
	movaps	%xmm13, %xmm11

	movups	16(%rcx, %rax), %xmm7
	movups	16(%rdi, %rax), %xmm3

	unpckhpd %xmm7, %xmm5
	unpckhpd %xmm3, %xmm13

	mulpd	%xmm6, %xmm5
	mulpd	%xmm0, %xmm13

	movlhps	%xmm7, %xmm10
	movlhps	%xmm3, %xmm11

	addpd	%xmm10, %xmm5
	addpd	%xmm11, %xmm13

	mulpd	%xmm6, %xmm5
	mulpd	%xmm0, %xmm13

	addpd	%xmm2, %xmm5

	movups	(%rsi, %rax), %xmm2
	movups	(%rdi, %rax), %xmm7

	movaps	%xmm2, %xmm3

	unpckhpd %xmm7, %xmm2
	movlhps	%xmm7, %xmm3

	addpd	%xmm13, %xmm2

	mulpd	%xmm5, %xmm6
	addpd	%xmm4, %xmm6

	mulpd	%xmm2, %xmm0
	addpd	%xmm3, %xmm0

	cvtpd2ps %xmm0, %xmm2
	cvtpd2ps %xmm6, %xmm0

	movlhps	%xmm2, %xmm0
	andnps	%xmm12, %xmm1
	orps	%xmm1, %xmm0

	/* xmm8 contains mask of special values.  */
	pcmpgtd	TANHF_DATA(_iExpMask)(%rip), %xmm8

	movmskps %xmm8, %edx
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 xmm0
	/* No stack restoration on the fastpath.  */
	ret

	/* Cold case. edx has 1s where there was a special value that
	   needs to be handled by a tanhf call. Optimize for code size
	   more so than speed here. */
L(SPECIAL_VALUES_BRANCH):
	# LOE rbx rdx rbp r12 r13 r14 r15 xmm0 xmm12
	/* Stack coming in 16-byte aligned. Set 8-byte misaligned so on
       call entry will be 16-byte aligned. */
	subq	$56, %rsp
	cfi_def_cfa_offset(64)
	movups	%xmm0, 24(%rsp)
	movups	%xmm12, 40(%rsp)

	/* Use rbx/rbp for callee save registers as they get short
       encoding for many instructions (as compared with r12/r13). */
	movq	%rbx, (%rsp)
	cfi_offset(rbx, -64)
	movq	%rbp, 8(%rsp)
	cfi_offset(rbp, -56)
	/* edx has 1s where there was a special value that needs to be handled
	   by a tanhf call.  */
	movl	%edx, %ebx
L(SPECIAL_VALUES_LOOP):
	# LOE rbx rbp r12 r13 r14 r15
	/* use rbp as index for special value that is saved across calls to
	   tanhf. We technically don't need a callee save register here as offset
	   to rsp is always [0, 12] so we can restore rsp by realigning to 64.
	   Essentially the tradeoff is 1 extra save/restore vs 2 extra instructions
	   in the loop.  */
	xorl	%ebp, %ebp
	bsfl	%ebx, %ebp

	/* Scalar math function call to process special input.  */
	movss	40(%rsp, %rbp, 4), %xmm0
	call	tanhf@PLT
	/* No good way to avoid the store-forwarding fault this will cause on
	   return. `lfence` avoids the SF fault but at greater cost as it
	   serialized stack/callee save restoration.  */
	movss	%xmm0, 24(%rsp, %rbp, 4)

	leal	-1(%rbx), %eax
	andl	%eax, %ebx
	jnz	L(SPECIAL_VALUES_LOOP)
	# LOE r12 r13 r14 r15
	/* All results have been written to 24(%rsp).  */
	movups	24(%rsp), %xmm0
	movq	(%rsp), %rbx
	cfi_restore(rbx)
	movq	8(%rsp), %rbp
	cfi_restore(rbp)
	addq	$56, %rsp
	cfi_def_cfa_offset(8)
	ret
END(_ZGVbN4v_tanhf_sse4)
