/* Function exp10f vectorized with SSE4.
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
 *   exp10(x)  = 2^x/log10(2) = 2^n * (1 + T[j]) * (1 + P(y))
 *   where
 *        x = m*log10(2)/K + y,  y in [-log10(2)/K..log10(2)/K]
 *        m = n*K + j,           m, n,j - signed integer, j in [-K/2..K/2]
 *
 *        values of 2^j/K are tabulated
 *
 *        P(y) is a minimax polynomial approximation of exp10(x)-1
 *        on small interval [-log10(2)/K..log10(2)/K]
 *
 *  Special cases:
 *
 *   exp10(NaN)  = NaN
 *   exp10(+INF) = +INF
 *   exp10(-INF) = 0
 *   exp10(x)    = 1 for subnormals
 *   For IEEE float
 *     if x >  38.5318412780761720 then exp10f(x) overflow
 *     if x < -45.4555282592773440 then exp10f(x) underflow
 *
 */

/* Offsets for data table __svml_sexp10_data_internal
 */
#define _sT				0
#define _sLg2_10			128
#define _sShifter			144
#define _sInvLg2_10hi			160
#define _sInvLg2_10lo			176
#define _sPC0				192
#define _sPC1				208
#define _sPC2				224
#define _iIndexMask			240
#define _iAbsMask			256
#define _iDomainRange			272

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_exp10f_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm4

	/*  Load argument  */
	movups	_sLg2_10+__svml_sexp10_data_internal(%rip), %xmm2
	lea	__svml_sexp10_data_internal(%rip), %r8
	mulps	%xmm4, %xmm2
	movups	_sShifter+__svml_sexp10_data_internal(%rip), %xmm5

	/*  R  */
	movups	_sInvLg2_10hi+__svml_sexp10_data_internal(%rip), %xmm14
	addps	%xmm5, %xmm2
	movaps	%xmm2, %xmm1
	movups	_sInvLg2_10lo+__svml_sexp10_data_internal(%rip), %xmm15
	subps	%xmm5, %xmm1
	mulps	%xmm1, %xmm14
	movaps	%xmm4, %xmm5
	mulps	%xmm1, %xmm15
	subps	%xmm14, %xmm5

	/*
	 *  Polynomial
	 * exp10 = 2^N*(Tj+Tj*poly)
	 * poly(sN) = {1+later} a0+a1*sR
	 */
	movups	_sPC2+__svml_sexp10_data_internal(%rip), %xmm1
	subps	%xmm15, %xmm5
	mulps	%xmm5, %xmm1
	movdqu	_iIndexMask+__svml_sexp10_data_internal(%rip), %xmm3

	/*  Index and lookup  */
	movdqa	%xmm3, %xmm10

	/* remove index bits */
	pandn	%xmm2, %xmm3
	pand	%xmm2, %xmm10

	/*  2^N  */
	pslld	$18, %xmm3

	/* iIndex *= sizeof(S); */
	pslld	$2, %xmm10
	addps	_sPC1+__svml_sexp10_data_internal(%rip), %xmm1
	movd	%xmm10, %edx
	pshufd	$1, %xmm10, %xmm7
	pshufd	$2, %xmm10, %xmm9
	pshufd	$3, %xmm10, %xmm11
	movd	%xmm7, %ecx
	movd	%xmm9, %esi
	movd	%xmm11, %edi

	/* Check for overflow\underflow  */
	movdqu	_iAbsMask+__svml_sexp10_data_internal(%rip), %xmm6
	pand	%xmm4, %xmm6
	mulps	%xmm1, %xmm5
	movslq	%edx, %rdx
	addps	_sPC0+__svml_sexp10_data_internal(%rip), %xmm5
	movslq	%ecx, %rcx
	movslq	%esi, %rsi
	movslq	%edi, %rdi
	movd	(%r8, %rdx), %xmm0
	movd	(%r8, %rcx), %xmm8
	movd	(%r8, %rsi), %xmm13
	movd	(%r8, %rdi), %xmm12
	punpckldq %xmm8, %xmm0
	punpckldq %xmm12, %xmm13
	punpcklqdq %xmm13, %xmm0

	/* Tj_l+Tj_h*poly */
	mulps	%xmm0, %xmm5
	pcmpgtd	_iDomainRange+__svml_sexp10_data_internal(%rip), %xmm6
	addps	%xmm5, %xmm0
	movmskps %xmm6, %eax

	/* quick mul 2^N */
	paddd	%xmm3, %xmm0

	/*  Finish   */
	testl	%eax, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 eax xmm0 xmm4

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm4, 32(%rsp)
	movups	%xmm0, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 eax

	xorl	%edx, %edx
	movq	%r12, 16(%rsp)
	cfi_offset(12, -64)
	movl	%edx, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -72)
	movl	%eax, %r13d
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
	movups	48(%rsp), %xmm0

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	exp10f@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_exp10f_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sexp10_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _sT[(1<<5)][1];
	__declspec(align(16)) VUINT32 _sLg2_10[4][1];
	__declspec(align(16)) VUINT32 _sShifter[4][1];
	__declspec(align(16)) VUINT32 _sInvLg2_10hi[4][1];
	__declspec(align(16)) VUINT32 _sInvLg2_10lo[4][1];
	__declspec(align(16)) VUINT32 _sPC0[4][1];
	__declspec(align(16)) VUINT32 _sPC1[4][1];
	__declspec(align(16)) VUINT32 _sPC2[4][1];
	__declspec(align(16)) VUINT32 _iIndexMask[4][1];
	__declspec(align(16)) VUINT32 _iAbsMask[4][1];
	__declspec(align(16)) VUINT32 _iDomainRange[4][1];
} __svml_sexp10_data_internal;
#endif
__svml_sexp10_data_internal:
	/* _sT */
	.long	0x3f800000 // 2^( 0 /32 )
	.long	0x3f82cd87 // 2^( 1 /32 )
	.long	0x3f85aac3 // 2^( 2 /32 )
	.long	0x3f88980f // 2^( 3 /32 )
	.long	0x3f8b95c2 // 2^( 4 /32 )
	.long	0x3f8ea43a // 2^( 5 /32 )
	.long	0x3f91c3d3 // 2^( 6 /32 )
	.long	0x3f94f4f0 // 2^( 7 /32 )
	.long	0x3f9837f0 // 2^( 8 /32 )
	.long	0x3f9b8d3a // 2^( 9 /32 )
	.long	0x3f9ef532 // 2^( 10/32 )
	.long	0x3fa27043 // 2^( 11/32 )
	.long	0x3fa5fed7 // 2^( 12/32 )
	.long	0x3fa9a15b // 2^( 13/32 )
	.long	0x3fad583f // 2^( 14/32 )
	.long	0x3fb123f6 // 2^( 15/32 )
	.long	0x3fb504f3 // 2^( 16/32 )
	.long	0x3fb8fbaf // 2^( 17/32 )
	.long	0x3fbd08a4 // 2^( 18/32 )
	.long	0x3fc12c4d // 2^( 19/32 )
	.long	0x3fc5672a // 2^( 20/32 )
	.long	0x3fc9b9be // 2^( 21/32 )
	.long	0x3fce248c // 2^( 22/32 )
	.long	0x3fd2a81e // 2^( 23/32 )
	.long	0x3fd744fd // 2^( 24/32 )
	.long	0x3fdbfbb8 // 2^( 25/32 )
	.long	0x3fe0ccdf // 2^( 26/32 )
	.long	0x3fe5b907 // 2^( 27/32 )
	.long	0x3feac0c7 // 2^( 28/32 )
	.long	0x3fefe4ba // 2^( 29/32 )
	.long	0x3ff5257d // 2^( 30/32 )
	.long	0x3ffa83b3 // 2^( 31/32 )
	.align	16
	.long	0x42d49a78, 0x42d49a78, 0x42d49a78, 0x42d49a78 /* _sLg2_10*2^K */
	.align	16
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter) */
	.align	16
	.long	0x3c1a2000, 0x3c1a2000, 0x3c1a2000, 0x3c1a2000 /* _sInvLg2_10hi/2^K hi (24-K-7) bits */
	.align	16
	.long	0x341a84fc, 0x341a84fc, 0x341a84fc, 0x341a84fc /* _sInvLg2_10lo/2^K  lo bits */
	// otherwise exp10(0) won't produce exact 1.0
	.align	16
	.long	0x2fecc868, 0x2fecc868, 0x2fecc868, 0x2fecc868 /* _sPC0 */
	.align	16
	.long	0x40135e1b, 0x40135e1b, 0x40135e1b, 0x40135e1b /* _sPC1 */
	.align	16
	.long	0x4029a8d2, 0x4029a8d2, 0x4029a8d2, 0x4029a8d2 /* _sPC2 */
	.align	16
	.long	0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f /* _iIndexMask =(2^K-1) */
	//common
	.align	16
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	16
	.long	0x4217b818, 0x4217b818, 0x4217b818, 0x4217b818 /* _iDomainRange=-log10(max_denormal=0x007fffff) RZ */
	.align	16
	.type	__svml_sexp10_data_internal, @object
	.size	__svml_sexp10_data_internal, .-__svml_sexp10_data_internal
