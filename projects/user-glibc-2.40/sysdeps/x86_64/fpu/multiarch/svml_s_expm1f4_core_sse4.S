/* Function expm1f vectorized with SSE4.
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
 *    N = (int)(x*2^k/log(2.0)), R = x - N*log(2)/2^k
 *    exp(x) = 2^(N/2^k) * poly(R) is computed in high-low parts
 *    expm1(x) = exp(x)-1 is then obtained via multi-precision computation
 *
 *
 */

/* Offsets for data table __svml_sexpm1_data_internal
 */
#define Expm1_HA_table			0
#define poly_coeff			512
#define Log2e				576
#define L2H				592
#define L2L				608
#define ExpAddConst			624
#define IndexMask			640
#define ExpMask				656
#define MOne				672
#define AbsMask				688
#define Threshold			704
#define L2				720

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_expm1f_sse4)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$64, %rsp
	movaps	%xmm0, %xmm4
	movups	Log2e+__svml_sexpm1_data_internal(%rip), %xmm9
	lea	__svml_sexpm1_data_internal(%rip), %r8
	mulps	%xmm0, %xmm9
	movups	.FLT_10(%rip), %xmm5
	movups	ExpAddConst+__svml_sexpm1_data_internal(%rip), %xmm2
	addps	%xmm5, %xmm9

	/* argument reduction */
	movups	L2H+__svml_sexpm1_data_internal(%rip), %xmm6
	subps	%xmm5, %xmm9
	mulps	%xmm9, %xmm6
	addps	%xmm9, %xmm2

	/* table lookup */
	movdqu	IndexMask+__svml_sexpm1_data_internal(%rip), %xmm12
	subps	%xmm6, %xmm4
	pand	%xmm2, %xmm12
	movups	L2L+__svml_sexpm1_data_internal(%rip), %xmm7
	movups	AbsMask+__svml_sexpm1_data_internal(%rip), %xmm3
	pshufd	$1, %xmm12, %xmm10
	movaps	%xmm3, %xmm8
	mulps	%xmm9, %xmm7
	andps	%xmm0, %xmm8
	cmpnleps Threshold+__svml_sexpm1_data_internal(%rip), %xmm8
	movd	%xmm12, %edx
	subps	%xmm7, %xmm4
	movd	%xmm10, %ecx
	movmskps %xmm8, %eax
	pshufd	$2, %xmm12, %xmm11
	movaps	%xmm4, %xmm7
	pshufd	$3, %xmm12, %xmm13
	andnps	%xmm0, %xmm3
	movd	%xmm11, %esi
	movd	%xmm13, %edi

	/* polynomial */
	movups	poly_coeff+__svml_sexpm1_data_internal(%rip), %xmm8
	movdqu	ExpMask+__svml_sexpm1_data_internal(%rip), %xmm6
	movslq	%edx, %rdx
	pand	%xmm6, %xmm2
	movslq	%ecx, %rcx
	pslld	$14, %xmm2
	movslq	%esi, %rsi
	movslq	%edi, %rdi
	movq	(%r8, %rdx), %xmm1
	movq	(%r8, %rcx), %xmm14
	movq	(%r8, %rsi), %xmm5
	movq	(%r8, %rdi), %xmm15
	unpcklps %xmm14, %xmm1
	mulps	%xmm4, %xmm8
	movaps	%xmm1, %xmm10
	mulps	%xmm4, %xmm7
	addps	poly_coeff+16+__svml_sexpm1_data_internal(%rip), %xmm8
	unpcklps %xmm15, %xmm5
	movlhps	%xmm5, %xmm10
	shufps	$238, %xmm5, %xmm1
	orps	%xmm2, %xmm10

	/* T-1 */
	movups	MOne+__svml_sexpm1_data_internal(%rip), %xmm9
	mulps	%xmm2, %xmm1
	addps	%xmm9, %xmm10
	mulps	%xmm7, %xmm8
	addps	%xmm1, %xmm10
	addps	%xmm8, %xmm4
	movaps	%xmm10, %xmm1
	subps	%xmm9, %xmm1
	mulps	%xmm1, %xmm4
	addps	%xmm4, %xmm10
	orps	%xmm3, %xmm10
	testl	%eax, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 eax xmm0 xmm10

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm10, %xmm0
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
	movups	%xmm0, 32(%rsp)
	movups	%xmm10, 48(%rsp)
	# LOE rbx r12 r13 r14 r15 eax

	xorl	%edx, %edx
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -48; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -56; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -64; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x22
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
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm10

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -48; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -56; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -64; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 xmm10

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	expm1f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVbN4v_expm1f_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sexpm1_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 Expm1_HA_table[(1<<7)][1];
	__declspec(align(16)) VUINT32 poly_coeff[4][4][1];
	__declspec(align(16)) VUINT32 Log2e[4][1];
	__declspec(align(16)) VUINT32 L2H[4][1];
	__declspec(align(16)) VUINT32 L2L[4][1];
	__declspec(align(16)) VUINT32 ExpAddConst[4][1];
	__declspec(align(16)) VUINT32 IndexMask[4][1];
	__declspec(align(16)) VUINT32 ExpMask[4][1];
	__declspec(align(16)) VUINT32 MOne[4][1];
	__declspec(align(16)) VUINT32 AbsMask[4][1];
	__declspec(align(16)) VUINT32 Threshold[4][1];
	__declspec(align(16)) VUINT32 L2[4][1];
} __svml_sexpm1_data_internal;
#endif
__svml_sexpm1_data_internal:
	/* Expm1_HA_table */
	.long	0x00000000, 0x00000000
	.long	0x00016000, 0x391a3e78
	.long	0x0002d000, 0xb89e59d5
	.long	0x00044000, 0xb93ae78a
	.long	0x0005b000, 0xb9279306
	.long	0x00072000, 0xb79e6961
	.long	0x0008a000, 0xb97e2fee
	.long	0x000a1000, 0x391aaea9
	.long	0x000b9000, 0x39383c7d
	.long	0x000d2000, 0xb9241490
	.long	0x000ea000, 0x39073169
	.long	0x00103000, 0x386e218a
	.long	0x0011c000, 0x38f4dceb
	.long	0x00136000, 0xb93a9a1e
	.long	0x0014f000, 0x391df520
	.long	0x00169000, 0x3905a6e4
	.long	0x00183000, 0x397e0a32
	.long	0x0019e000, 0x370b2641
	.long	0x001b9000, 0xb8b1918b
	.long	0x001d4000, 0xb8132c6a
	.long	0x001ef000, 0x39264c12
	.long	0x0020b000, 0x37221f73
	.long	0x00227000, 0x37060619
	.long	0x00243000, 0x3922b5c1
	.long	0x00260000, 0xb814ab27
	.long	0x0027d000, 0xb89b12c6
	.long	0x0029a000, 0x382d5a75
	.long	0x002b8000, 0xb938c94b
	.long	0x002d6000, 0xb97822b8
	.long	0x002f4000, 0xb910ea53
	.long	0x00312000, 0x38fd6075
	.long	0x00331000, 0x38620955
	.long	0x00350000, 0x391e667f
	.long	0x00370000, 0xb89b8736
	.long	0x00390000, 0xb90a1714
	.long	0x003b0000, 0xb7a54ded
	.long	0x003d1000, 0xb96b8c15
	.long	0x003f1000, 0x397336cf
	.long	0x00413000, 0xb8eccd66
	.long	0x00434000, 0x39599b45
	.long	0x00456000, 0x3965422b
	.long	0x00479000, 0xb8a2cdd5
	.long	0x0049c000, 0xb9484f32
	.long	0x004bf000, 0xb8fac043
	.long	0x004e2000, 0x391182a4
	.long	0x00506000, 0x38ccf6bc
	.long	0x0052b000, 0xb97c4dc2
	.long	0x0054f000, 0x38d6aaf4
	.long	0x00574000, 0x391f995b
	.long	0x0059a000, 0xb8ba8f62
	.long	0x005c0000, 0xb9090d05
	.long	0x005e6000, 0x37f4825e
	.long	0x0060d000, 0xb8c844f5
	.long	0x00634000, 0xb76d1a83
	.long	0x0065c000, 0xb95f2310
	.long	0x00684000, 0xb952b5f8
	.long	0x006ac000, 0x37c6e7dd
	.long	0x006d5000, 0xb7cfe126
	.long	0x006fe000, 0x3917337c
	.long	0x00728000, 0x383b9e2d
	.long	0x00752000, 0x392fa2a5
	.long	0x0077d000, 0x37df730b
	.long	0x007a8000, 0x38ecb6dd
	.long	0x007d4000, 0xb879f986
	/* poly_coeff[4] */
	.align	16
	.long	0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF /* coeff3 */
	.long	0x3f00000F, 0x3f00000F, 0x3f00000F, 0x3f00000F /* coeff2 */
	/* 32 Byte Padding */
	.zero	32
	/* Log2e */
	.align	16
	.long	0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B
	/* L2H */
	.align	16
	.long	0x3c318000, 0x3c318000, 0x3c318000, 0x3c318000
	/* L2L */
	.align	16
	.long	0xb65e8083, 0xb65e8083, 0xb65e8083, 0xb65e8083
	/* ExpAddConst */
	.align	16
	.long	0x49f0fe00, 0x49f0fe00, 0x49f0fe00, 0x49f0fe00
	/* IndexMask */
	.align	16
	.long	0x000001f8, 0x000001f8, 0x000001f8, 0x000001f8
	/* ExpMask */
	.align	16
	.long	0x0001fe00, 0x0001fe00, 0x0001fe00, 0x0001fe00
	/* MOne */
	.align	16
	.long	0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
	/* AbsMask */
	.align	16
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* Threshold */
	.align	16
	.long	0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B // 86.643394
	/* L2 */
	.align	16
	.long	0x3cb17218, 0x3cb17218, 0x3cb17218, 0x3cb17218
	.align	16
	.type	__svml_sexpm1_data_internal, @object
	.size	__svml_sexpm1_data_internal, .-__svml_sexpm1_data_internal
	.align	16

.FLT_10:
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000
	.type	.FLT_10, @object
	.size	.FLT_10, 16
