/* Function expm1f vectorized with AVX2.
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
#define Log2e				640
#define L2H				672
#define L2L				704
#define ExpAddConst			736
#define IndexMask			768
#define ExpMask				800
#define MOne				832
#define AbsMask				864
#define Threshold			896
#define L2				928

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_expm1f_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	lea	__svml_sexpm1_data_internal(%rip), %rax
	vmovaps	%ymm0, %ymm3
	vmulps	Log2e+__svml_sexpm1_data_internal(%rip), %ymm3, %ymm4

	/* argument reduction */
	vmovups	L2H+__svml_sexpm1_data_internal(%rip), %ymm2
	vmovups	AbsMask+__svml_sexpm1_data_internal(%rip), %ymm5
	vroundps $0, %ymm4, %ymm8
	vaddps	ExpAddConst+__svml_sexpm1_data_internal(%rip), %ymm8, %ymm0
	vfnmadd213ps %ymm3, %ymm8, %ymm2

	/* table lookup */
	vandps	IndexMask+__svml_sexpm1_data_internal(%rip), %ymm0, %ymm9
	vandps	%ymm5, %ymm3, %ymm6
	vcmpnle_uqps Threshold+__svml_sexpm1_data_internal(%rip), %ymm6, %ymm7
	vfnmadd231ps L2L+__svml_sexpm1_data_internal(%rip), %ymm8, %ymm2
	vandps	ExpMask+__svml_sexpm1_data_internal(%rip), %ymm0, %ymm0
	vandnps	%ymm3, %ymm5, %ymm1
	vpslld	$14, %ymm0, %ymm0
	vmovmskps %ymm7, %edx
	vmovd	%xmm9, %ecx
	vextractf128 $1, %ymm9, %xmm10
	movslq	%ecx, %rcx
	vmovd	%xmm10, %r9d
	vpextrd	$1, %xmm9, %esi
	vpextrd	$2, %xmm9, %edi
	vpextrd	$3, %xmm9, %r8d
	vmovq	(%rax, %rcx), %xmm11
	vpextrd	$1, %xmm10, %r10d
	vpextrd	$2, %xmm10, %r11d
	vpextrd	$3, %xmm10, %ecx
	movslq	%esi, %rsi
	movslq	%edi, %rdi
	movslq	%r8d, %r8
	movslq	%r9d, %r9
	movslq	%r10d, %r10
	movslq	%r11d, %r11
	movslq	%ecx, %rcx
	vmovq	(%rax, %rsi), %xmm13
	vmovq	(%rax, %rdi), %xmm12
	vmovq	(%rax, %r8), %xmm14
	vmovq	(%rax, %r9), %xmm15
	vmovq	(%rax, %r10), %xmm5
	vmovq	(%rax, %r11), %xmm4
	vmovq	(%rax, %rcx), %xmm6
	vunpcklps %xmm12, %xmm11, %xmm7
	vunpcklps %xmm14, %xmm13, %xmm8
	vunpcklps %xmm4, %xmm15, %xmm15
	vunpcklps %xmm6, %xmm5, %xmm9
	vmulps	%ymm2, %ymm2, %ymm13
	vinsertf128 $1, %xmm15, %ymm7, %ymm10
	vinsertf128 $1, %xmm9, %ymm8, %ymm11
	vunpcklps %ymm11, %ymm10, %ymm12
	vorps	%ymm0, %ymm12, %ymm14

	/* polynomial */
	vmovups	poly_coeff+__svml_sexpm1_data_internal(%rip), %ymm12
	vfmadd213ps poly_coeff+32+__svml_sexpm1_data_internal(%rip), %ymm2, %ymm12
	vfmadd213ps %ymm2, %ymm13, %ymm12

	/* T-1 */
	vmovups	MOne+__svml_sexpm1_data_internal(%rip), %ymm13
	vaddps	%ymm13, %ymm14, %ymm2
	vunpckhps %ymm11, %ymm10, %ymm4
	vfmadd213ps %ymm2, %ymm0, %ymm4
	vsubps	%ymm13, %ymm4, %ymm0
	vfmadd213ps %ymm4, %ymm12, %ymm0
	vorps	%ymm1, %ymm0, %ymm0
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
	call	expm1f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_expm1f_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sexpm1_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 Expm1_HA_table[(1<<7)][1];
	__declspec(align(32)) VUINT32 poly_coeff[4][8][1];
	__declspec(align(32)) VUINT32 Log2e[8][1];
	__declspec(align(32)) VUINT32 L2H[8][1];
	__declspec(align(32)) VUINT32 L2L[8][1];
	__declspec(align(32)) VUINT32 ExpAddConst[8][1];
	__declspec(align(32)) VUINT32 IndexMask[8][1];
	__declspec(align(32)) VUINT32 ExpMask[8][1];
	__declspec(align(32)) VUINT32 MOne[8][1];
	__declspec(align(32)) VUINT32 AbsMask[8][1];
	__declspec(align(32)) VUINT32 Threshold[8][1];
	__declspec(align(32)) VUINT32 L2[8][1];
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
	.align	32
	.long	0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF, 0x3e2AAABF /* coeff3 */
	.long	0x3f00000F, 0x3f00000F, 0x3f00000F, 0x3f00000F, 0x3f00000F, 0x3f00000F, 0x3f00000F, 0x3f00000F /* coeff2 */
	/* 64 Byte Padding */
	.zero	64
	/* Log2e */
	.align	32
	.long	0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B, 0x42B8AA3B
	/* L2H */
	.align	32
	.long	0x3c318000, 0x3c318000, 0x3c318000, 0x3c318000, 0x3c318000, 0x3c318000, 0x3c318000, 0x3c318000
	/* L2L */
	.align	32
	.long	0xb65e8083, 0xb65e8083, 0xb65e8083, 0xb65e8083, 0xb65e8083, 0xb65e8083, 0xb65e8083, 0xb65e8083
	/* ExpAddConst */
	.align	32
	.long	0x49f0fe00, 0x49f0fe00, 0x49f0fe00, 0x49f0fe00, 0x49f0fe00, 0x49f0fe00, 0x49f0fe00, 0x49f0fe00
	/* IndexMask */
	.align	32
	.long	0x000001f8, 0x000001f8, 0x000001f8, 0x000001f8, 0x000001f8, 0x000001f8, 0x000001f8, 0x000001f8
	/* ExpMask */
	.align	32
	.long	0x0001fe00, 0x0001fe00, 0x0001fe00, 0x0001fe00, 0x0001fe00, 0x0001fe00, 0x0001fe00, 0x0001fe00
	/* MOne */
	.align	32
	.long	0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
	/* AbsMask */
	.align	32
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* Threshold */
	.align	32
	.long	0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B // 86.643394
	/* L2 */
	.align	32
	.long	0x3cb17218, 0x3cb17218, 0x3cb17218, 0x3cb17218, 0x3cb17218, 0x3cb17218, 0x3cb17218, 0x3cb17218
	.align	32
	.type	__svml_sexpm1_data_internal, @object
	.size	__svml_sexpm1_data_internal, .-__svml_sexpm1_data_internal
