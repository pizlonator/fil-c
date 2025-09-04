/* Function tanhf vectorized with AVX2.
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
#include "svml_s_tanhf_rodata.S"

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_tanhf_avx2)
	/* Here huge arguments, INF and NaNs are filtered out to callout. */
	vpand	TANHF_DATA(_iExpMantMask)(%rip), %ymm0, %ymm4
	vpsubd	TANHF_DATA(_iMinIdxOfsMask)(%rip), %ymm4, %ymm2

	/* Selection of arguments between [0, 0x04280000] into ymm2.  */
	vpxor	%ymm3, %ymm3, %ymm3
	vpmaxsd	%ymm3, %ymm2, %ymm2
	vpminsd	TANHF_DATA(_iMaxIdxMask)(%rip), %ymm2, %ymm2

	/*
	 *  small table specific variables *
	 *  Constant loading
	 */
	vpsrld	$14, %ymm2, %ymm1

	/* We are splitting xmm1 into 8 GPRs. This may be faster to do with
	   store/load as we can take advantage of store-forwarding.  */
	vmovq	%xmm1, %r8
	/* We have eliminated all negative values for ymm1 so no need to sign
	   extend.  */
	movl	%r8d, %r9d
	shrq	$32, %r8

	/* Store base of lookup table in rax.  */
	leaq	TANHF_DATA(_lookupTable)(%rip), %rax

	/* Instead of using cross-lane permutes on ymm vectors, use vpinsertf128
	   with memory operand. This helps alleviate bottleneck on p5.  */
	vmovupd	16(%r9, %rax), %xmm5

	vpextrq	$1, %xmm1, %rsi
	movl	%esi, %edi
	shrq	$32, %rsi

	vinsertf128 $1, 16(%rdi, %rax), %ymm5, %ymm5

	vextracti128 $1, %ymm1, %xmm2
	vmovq	%xmm2, %rdx
	movl	%edx, %ecx
	shrq	$32, %rdx

	vmovupd	(%rcx, %rax), %xmm6

	vpextrq	$1, %xmm2, %r10
	movl	%r10d, %r11d
	shrq	$32, %r10

	vinsertf128 $1, (%r11, %rax), %ymm6, %ymm6

	vmovupd	16(%r8, %rax), %xmm1
	vinsertf128 $1, 16(%rsi, %rax), %ymm1, %ymm1
	vmovupd	(%rdx, %rax), %xmm3
	vinsertf128 $1, (%r10, %rax), %ymm3, %ymm3

	vunpcklpd %ymm3, %ymm6, %ymm7
	vunpckhpd %ymm3, %ymm6, %ymm6

	vunpcklpd %ymm1, %ymm5, %ymm3
	vunpckhpd %ymm1, %ymm5, %ymm1

	vmovaps	TANHF_DATA(_sAbsMask)(%rip), %ymm11
	/* Store special cases in ymm15.  */
	vpcmpgtd TANHF_DATA(_iExpMask)(%rip), %ymm4, %ymm15

	vandps	%ymm11, %ymm0, %ymm4

	vcvtps2pd %xmm4, %ymm5

	vextractf128 $1, %ymm4, %xmm4
	vcvtps2pd %xmm4, %ymm4

	vmovupd	16(%rcx, %rax), %xmm2
	vinsertf128 $1, 16(%r11, %rax), %ymm2, %ymm2

	vfmadd213pd %ymm3, %ymm5, %ymm1

	vmovupd	16(%rdx, %rax), %xmm3
	vinsertf128 $1, 16(%r10, %rax), %ymm3, %ymm3

	vunpcklpd %ymm3, %ymm2, %ymm10
	vunpckhpd %ymm3, %ymm2, %ymm2

	vfmadd213pd %ymm10, %ymm4, %ymm2
	vfmadd213pd %ymm6, %ymm4, %ymm2
	vfmadd213pd %ymm7, %ymm4, %ymm2
	vcvtpd2ps %ymm2, %xmm2

	vmovupd	(%r9, %rax), %xmm7
	vinsertf128 $1, (%rdi, %rax), %ymm7, %ymm7

	vmovupd	(%r8, %rax), %xmm3
	vinsertf128 $1, (%rsi, %rax), %ymm3, %ymm3

	vunpckhpd %ymm3, %ymm7, %ymm4
	vunpcklpd %ymm3, %ymm7, %ymm7

	vfmadd213pd %ymm4, %ymm5, %ymm1
	vfmadd213pd %ymm7, %ymm5, %ymm1


	vcvtpd2ps %ymm1, %xmm1
	vinsertf128 $1, %xmm2, %ymm1, %ymm1

	vmovmskps %ymm15, %edx
	vandnps	%ymm0, %ymm11, %ymm2
	testl	%edx, %edx
	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 ymm0 ymm1 ymm2
	/* Wait until after branch of write over ymm0.  */
	vorps	%ymm2, %ymm1, %ymm0
	/* No stack restoration on the fastpath.  */
	ret


	/* Cold case. edx has 1s where there was a special value that
	   needs to be handled by a tanhf call. Optimize for code size
	   more so than speed here. */
L(SPECIAL_VALUES_BRANCH):
	# LOE rbx rdx r12 r13 r14 r15 ymm0 ymm1 ymm2
    /* Use r13 to save/restore the stack. This allows us to use rbp as
       callee save register saving code size. */
	pushq	%r13
	cfi_adjust_cfa_offset(8)
	cfi_offset(r13, -16)
	/* Need to callee save registers to preserve state across tanhf calls.
	 */
	pushq	%rbx
	cfi_adjust_cfa_offset(8)
	cfi_offset(rbx, -24)
	pushq	%rbp
	cfi_adjust_cfa_offset(8)
	cfi_offset(rbp, -32)
	movq	%rsp, %r13
	cfi_def_cfa_register(r13)

	/* Align stack and make room for 2x ymm vectors.  */
	andq	$-32, %rsp
	addq	$-64, %rsp

	/* Save all already computed inputs.  */
	vorps	%ymm2, %ymm1, %ymm1
	vmovaps	%ymm1, (%rsp)
	/* Save original input (ymm0 unchanged up to this point).  */
	vmovaps	%ymm0, 32(%rsp)

	vzeroupper

	/* edx has 1s where there was a special value that needs to be handled
	   by a tanhf call.  */
	movl	%edx, %ebx
L(SPECIAL_VALUES_LOOP):
	# LOE rbx rbp r12 r13 r14 r15
	/* use rbp as index for special value that is saved across calls to
	   tanhf. We technically don't need a callee save register here as offset
	   to rsp is always [0, 28] so we can restore rsp by realigning to 64.
	   Essentially the tradeoff is 1 extra save/restore vs 2 extra instructions
	   in the loop. Realigning also costs more code size.  */
	xorl	%ebp, %ebp
	tzcntl	%ebx, %ebp

	/* Scalar math function call to process special input.  */
	vmovss	32(%rsp, %rbp, 4), %xmm0
	call	tanhf@PLT

	/* No good way to avoid the store-forwarding fault this will cause on
	   return. `lfence` avoids the SF fault but at greater cost as it
	   serialized stack/callee save restoration.  */
	vmovss	%xmm0, (%rsp, %rbp, 4)

	blsrl   %ebx, %ebx
	jnz	L(SPECIAL_VALUES_LOOP)
	# LOE r12 r13 r14 r15


	/* All results have been written to (%rsp).  */
	vmovups	(%rsp), %ymm0
	/* Restore rsp.  */
	movq	%r13, %rsp
	cfi_def_cfa_register(rsp)
	/* Restore callee save registers.  */
	popq	%rbp
	cfi_adjust_cfa_offset(-8)
	cfi_restore(rbp)
	popq	%rbx
	cfi_adjust_cfa_offset(-8)
	cfi_restore(rbp)
	popq	%r13
	cfi_adjust_cfa_offset(-8)
	cfi_restore(r13)
	ret
END(_ZGVdN8v_tanhf_avx2)
