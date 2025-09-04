/* Function exp10f vectorized with AVX2.
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
#define _sShifter			160
#define _sInvLg2_10hi			192
#define _sInvLg2_10lo			224
#define _sPC0				256
#define _sPC1				288
#define _sPC2				320
#define _iIndexMask			352
#define _iAbsMask			384
#define _iDomainRange			416

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_exp10f_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	lea	__svml_sexp10_data_internal(%rip), %rax
	vmovups	_sShifter+__svml_sexp10_data_internal(%rip), %ymm4

	/*  Load argument  */
	vmovups	_sLg2_10+__svml_sexp10_data_internal(%rip), %ymm1
	vmovups	_iIndexMask+__svml_sexp10_data_internal(%rip), %ymm2
	vmovaps	%ymm0, %ymm3
	vfmadd213ps %ymm4, %ymm3, %ymm1

	/*  Index and lookup  */
	vandps	%ymm2, %ymm1, %ymm7

	/* iIndex *= sizeof(S); */
	vpslld	$2, %ymm7, %ymm10
	vsubps	%ymm4, %ymm1, %ymm0

	/* Check for overflow\underflow  */
	vandps	_iAbsMask+__svml_sexp10_data_internal(%rip), %ymm3, %ymm5
	vpcmpgtd _iDomainRange+__svml_sexp10_data_internal(%rip), %ymm5, %ymm6
	vmovmskps %ymm6, %edx
	vmovd	%xmm10, %ecx
	vextractf128 $1, %ymm10, %xmm6
	vpextrd	$1, %xmm10, %esi
	vpextrd	$2, %xmm10, %edi
	vpextrd	$3, %xmm10, %r8d
	movslq	%ecx, %rcx
	movslq	%esi, %rsi
	movslq	%edi, %rdi
	movslq	%r8d, %r8
	vmovd	(%rax, %rcx), %xmm8
	vmovd	(%rax, %rsi), %xmm9
	vmovd	(%rax, %rdi), %xmm11
	vmovd	(%rax, %r8), %xmm12
	vpunpckldq %xmm9, %xmm8, %xmm13
	vpunpckldq %xmm12, %xmm11, %xmm14
	vpunpcklqdq %xmm14, %xmm13, %xmm15

	/*  R  */
	vmovups	_sInvLg2_10hi+__svml_sexp10_data_internal(%rip), %ymm13
	vmovd	%xmm6, %r9d
	vfnmadd213ps %ymm3, %ymm0, %ymm13
	vpextrd	$1, %xmm6, %r10d
	movslq	%r9d, %r9
	movslq	%r10d, %r10
	vfnmadd132ps _sInvLg2_10lo+__svml_sexp10_data_internal(%rip), %ymm13, %ymm0
	vmovd	(%rax, %r9), %xmm4
	vmovd	(%rax, %r10), %xmm5
	vpunpckldq %xmm5, %xmm4, %xmm9

	/*
	 *  Polynomial
	 * exp10 = 2^N*(Tj+Tj*poly)
	 * poly(sN) = {1+later} a0+a1*sR
	 */
	vmovups	_sPC2+__svml_sexp10_data_internal(%rip), %ymm4
	vfmadd213ps _sPC1+__svml_sexp10_data_internal(%rip), %ymm0, %ymm4
	vpextrd	$2, %xmm6, %r11d
	vpextrd	$3, %xmm6, %ecx
	movslq	%r11d, %r11
	movslq	%ecx, %rcx
	vfmadd213ps _sPC0+__svml_sexp10_data_internal(%rip), %ymm0, %ymm4
	vmovd	(%rax, %r11), %xmm7
	vmovd	(%rax, %rcx), %xmm8
	vpunpckldq %xmm8, %xmm7, %xmm11

	/* remove index bits */
	vpandn	%ymm1, %ymm2, %ymm0
	vpunpcklqdq %xmm11, %xmm9, %xmm12

	/*  2^N  */
	vpslld	$18, %ymm0, %ymm1
	vinsertf128 $1, %xmm12, %ymm15, %ymm14

	/* Tj_l+Tj_h*poly */
	vfmadd213ps %ymm14, %ymm14, %ymm4

	/* quick mul 2^N */
	vpaddd	%ymm1, %ymm4, %ymm0

	/*  Finish   */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm3

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
	vmovups	%ymm3, 32(%rsp)
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
	call	exp10f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_exp10f_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sexp10_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _sT[(1<<5)][1];
	__declspec(align(32)) VUINT32 _sLg2_10[8][1];
	__declspec(align(32)) VUINT32 _sShifter[8][1];
	__declspec(align(32)) VUINT32 _sInvLg2_10hi[8][1];
	__declspec(align(32)) VUINT32 _sInvLg2_10lo[8][1];
	__declspec(align(32)) VUINT32 _sPC0[8][1];
	__declspec(align(32)) VUINT32 _sPC1[8][1];
	__declspec(align(32)) VUINT32 _sPC2[8][1];
	__declspec(align(32)) VUINT32 _iIndexMask[8][1];
	__declspec(align(32)) VUINT32 _iAbsMask[8][1];
	__declspec(align(32)) VUINT32 _iDomainRange[8][1];
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
	.align	32
	.long	0x42d49a78, 0x42d49a78, 0x42d49a78, 0x42d49a78, 0x42d49a78, 0x42d49a78, 0x42d49a78, 0x42d49a78 /* _sLg2_10*2^K */
	.align	32
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter) */
	.align	32
	.long	0x3c1a2000, 0x3c1a2000, 0x3c1a2000, 0x3c1a2000, 0x3c1a2000, 0x3c1a2000, 0x3c1a2000, 0x3c1a2000 /* _sInvLg2_10hi/2^K hi (24-K-7) bits */
	.align	32
	.long	0x341a84fc, 0x341a84fc, 0x341a84fc, 0x341a84fc, 0x341a84fc, 0x341a84fc, 0x341a84fc, 0x341a84fc /* _sInvLg2_10lo/2^K  lo bits */
	// otherwise exp10(0) won't produce exact 1.0
	.align	32
	.long	0x2fecc868, 0x2fecc868, 0x2fecc868, 0x2fecc868, 0x2fecc868, 0x2fecc868, 0x2fecc868, 0x2fecc868 /* _sPC0 */
	.align	32
	.long	0x40135e1b, 0x40135e1b, 0x40135e1b, 0x40135e1b, 0x40135e1b, 0x40135e1b, 0x40135e1b, 0x40135e1b /* _sPC1 */
	.align	32
	.long	0x4029a8d2, 0x4029a8d2, 0x4029a8d2, 0x4029a8d2, 0x4029a8d2, 0x4029a8d2, 0x4029a8d2, 0x4029a8d2 /* _sPC2 */
	.align	32
	.long	0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f /* _iIndexMask =(2^K-1) */
	//common
	.align	32
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	32
	.long	0x4217b818, 0x4217b818, 0x4217b818, 0x4217b818, 0x4217b818, 0x4217b818, 0x4217b818, 0x4217b818 /* _iDomainRange=-log10(max_denormal=0x007fffff) RZ */
	.align	32
	.type	__svml_sexp10_data_internal, @object
	.size	__svml_sexp10_data_internal, .-__svml_sexp10_data_internal
