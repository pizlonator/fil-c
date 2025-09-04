/* Function exp10 vectorized with SSE4.
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
 *   For IEEE double
 *     if x >  3.39782712893383973096e+02 then exp10(x) overflow
 *     if x < -3.45133219101941108420e+02 then exp10(x) underflow
 *
 */

/* Offsets for data table __svml_dexp10_data_internal
 */
#define _dbT				0
#define _dbLg2_10			1024
#define _dbShifter			1040
#define _dbInvLg2_10hi			1056
#define _dbInvLg2_10lo			1072
#define _dPC1				1088
#define _dPC2				1104
#define _dPC3				1120
#define _dPC4				1136
#define _dPC5				1152
#define _lExpMask			1168
#define _iIndexMask			1184
#define _iAbsMask			1200
#define _iDomainRange			1216

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_exp10_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/*  R  */
	movaps	%xmm0, %xmm12

	/*  Load argument  */
	movups	_dbLg2_10+__svml_dexp10_data_internal(%rip), %xmm13
	lea	__svml_dexp10_data_internal(%rip), %rsi
	mulpd	%xmm0, %xmm13
	movups	_dbShifter+__svml_dexp10_data_internal(%rip), %xmm1
	addpd	%xmm1, %xmm13
	movaps	%xmm13, %xmm9
	subpd	%xmm1, %xmm9
	movups	_dbInvLg2_10hi+__svml_dexp10_data_internal(%rip), %xmm8
	mulpd	%xmm9, %xmm8
	movups	_dbInvLg2_10lo+__svml_dexp10_data_internal(%rip), %xmm10
	mulpd	%xmm9, %xmm10
	subpd	%xmm8, %xmm12
	subpd	%xmm10, %xmm12

	/*
	 *  Polynomial
	 * poly(dN) = a1*dR+...+a5*dR^5
	 */
	movups	_dPC5+__svml_dexp10_data_internal(%rip), %xmm11
	mulpd	%xmm12, %xmm11
	addpd	_dPC4+__svml_dexp10_data_internal(%rip), %xmm11
	mulpd	%xmm12, %xmm11
	addpd	_dPC3+__svml_dexp10_data_internal(%rip), %xmm11
	mulpd	%xmm12, %xmm11
	addpd	_dPC2+__svml_dexp10_data_internal(%rip), %xmm11

	/* a1+...+a5*dR^4 ! */
	mulpd	%xmm12, %xmm11
	addpd	_dPC1+__svml_dexp10_data_internal(%rip), %xmm11
	movq	_iIndexMask+__svml_dexp10_data_internal(%rip), %xmm5

	/*  Index and lookup  */
	pshufd	$136, %xmm13, %xmm6

	/*  2^N  */
	psllq	$45, %xmm13
	pand	%xmm5, %xmm6

	/* iIndex*=sizeof(D); */
	pslld	$3, %xmm6
	movd	%xmm6, %eax
	pshufd	$1, %xmm6, %xmm7
	movq	_iAbsMask+__svml_dexp10_data_internal(%rip), %xmm2

	/* a1*dR+...+a5*dR^5 */
	mulpd	%xmm11, %xmm12
	movd	%xmm7, %ecx

	/* Check for overflow\underflow  */
	pshufd	$221, %xmm0, %xmm4
	movq	_iDomainRange+__svml_dexp10_data_internal(%rip), %xmm3
	pand	%xmm2, %xmm4
	movslq	%eax, %rax
	pcmpgtd	%xmm3, %xmm4
	movslq	%ecx, %rcx
	movmskps %xmm4, %edx

	/* lM==EXP(2^N) */
	pand	_lExpMask+__svml_dexp10_data_internal(%rip), %xmm13
	movsd	(%rsi, %rax), %xmm1
	movhpd	(%rsi, %rcx), %xmm1

	/* Tj*poly */
	mulpd	%xmm1, %xmm12
	addpd	%xmm12, %xmm1

	/* quick 2^N */
	paddq	%xmm13, %xmm1
	andl	$3, %edx

	/*  Finish   */

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
	# LOE rbx rbp r12 r13 r14 r15 edx xmm1

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
	cmpl	$2, %r12d

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
	movsd	32(%rsp, %r14, 8), %xmm0
	call	exp10@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN2v_exp10_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dexp10_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _dbT[(1<<7)][2];
	__declspec(align(16)) VUINT32 _dbLg2_10[2][2];
	__declspec(align(16)) VUINT32 _dbShifter[2][2];
	__declspec(align(16)) VUINT32 _dbInvLg2_10hi[2][2];
	__declspec(align(16)) VUINT32 _dbInvLg2_10lo[2][2];
	__declspec(align(16)) VUINT32 _dPC1[2][2];
	__declspec(align(16)) VUINT32 _dPC2[2][2];
	__declspec(align(16)) VUINT32 _dPC3[2][2];
	__declspec(align(16)) VUINT32 _dPC4[2][2];
	__declspec(align(16)) VUINT32 _dPC5[2][2];
	__declspec(align(16)) VUINT32 _lExpMask[2][2];
	__declspec(align(16)) VUINT32 _iIndexMask[4][1];
	__declspec(align(16)) VUINT32 _iAbsMask[4][1];
	__declspec(align(16)) VUINT32 _iDomainRange[4][1];
} __svml_dexp10_data_internal;
#endif
__svml_dexp10_data_internal:
	/* _dbT */
	.quad	0x3ff0000000000000 /* 2^( 0 /128) */
	.quad	0x3ff0163da9fb3335 /* 2^( 1 /128) */
	.quad	0x3ff02c9a3e778061 /* 2^( 2 /128) */
	.quad	0x3ff04315e86e7f85 /* 2^( 3 /128) */
	.quad	0x3ff059b0d3158574 /* 2^( 4 /128) */
	.quad	0x3ff0706b29ddf6de /* 2^( 5 /128) */
	.quad	0x3ff0874518759bc8 /* 2^( 6 /128) */
	.quad	0x3ff09e3ecac6f383 /* 2^( 7 /128) */
	.quad	0x3ff0b5586cf9890f /* 2^( 8 /128) */
	.quad	0x3ff0cc922b7247f7 /* 2^( 9 /128) */
	.quad	0x3ff0e3ec32d3d1a2 /* 2^( 10 /128) */
	.quad	0x3ff0fb66affed31b /* 2^( 11 /128) */
	.quad	0x3ff11301d0125b51 /* 2^( 12 /128) */
	.quad	0x3ff12abdc06c31cc /* 2^( 13 /128) */
	.quad	0x3ff1429aaea92de0 /* 2^( 14 /128) */
	.quad	0x3ff15a98c8a58e51 /* 2^( 15 /128) */
	.quad	0x3ff172b83c7d517b /* 2^( 16 /128) */
	.quad	0x3ff18af9388c8dea /* 2^( 17 /128) */
	.quad	0x3ff1a35beb6fcb75 /* 2^( 18 /128) */
	.quad	0x3ff1bbe084045cd4 /* 2^( 19 /128) */
	.quad	0x3ff1d4873168b9aa /* 2^( 20 /128) */
	.quad	0x3ff1ed5022fcd91d /* 2^( 21 /128) */
	.quad	0x3ff2063b88628cd6 /* 2^( 22 /128) */
	.quad	0x3ff21f49917ddc96 /* 2^( 23 /128) */
	.quad	0x3ff2387a6e756238 /* 2^( 24 /128) */
	.quad	0x3ff251ce4fb2a63f /* 2^( 25 /128) */
	.quad	0x3ff26b4565e27cdd /* 2^( 26 /128) */
	.quad	0x3ff284dfe1f56381 /* 2^( 27 /128) */
	.quad	0x3ff29e9df51fdee1 /* 2^( 28 /128) */
	.quad	0x3ff2b87fd0dad990 /* 2^( 29 /128) */
	.quad	0x3ff2d285a6e4030b /* 2^( 30 /128) */
	.quad	0x3ff2ecafa93e2f56 /* 2^( 31 /128) */
	.quad	0x3ff306fe0a31b715 /* 2^( 32 /128) */
	.quad	0x3ff32170fc4cd831 /* 2^( 33 /128) */
	.quad	0x3ff33c08b26416ff /* 2^( 34 /128) */
	.quad	0x3ff356c55f929ff1 /* 2^( 35 /128) */
	.quad	0x3ff371a7373aa9cb /* 2^( 36 /128) */
	.quad	0x3ff38cae6d05d866 /* 2^( 37 /128) */
	.quad	0x3ff3a7db34e59ff7 /* 2^( 38 /128) */
	.quad	0x3ff3c32dc313a8e5 /* 2^( 39 /128) */
	.quad	0x3ff3dea64c123422 /* 2^( 40 /128) */
	.quad	0x3ff3fa4504ac801c /* 2^( 41 /128) */
	.quad	0x3ff4160a21f72e2a /* 2^( 42 /128) */
	.quad	0x3ff431f5d950a897 /* 2^( 43 /128) */
	.quad	0x3ff44e086061892d /* 2^( 44 /128) */
	.quad	0x3ff46a41ed1d0057 /* 2^( 45 /128) */
	.quad	0x3ff486a2b5c13cd0 /* 2^( 46 /128) */
	.quad	0x3ff4a32af0d7d3de /* 2^( 47 /128) */
	.quad	0x3ff4bfdad5362a27 /* 2^( 48 /128) */
	.quad	0x3ff4dcb299fddd0d /* 2^( 49 /128) */
	.quad	0x3ff4f9b2769d2ca7 /* 2^( 50 /128) */
	.quad	0x3ff516daa2cf6642 /* 2^( 51 /128) */
	.quad	0x3ff5342b569d4f82 /* 2^( 52 /128) */
	.quad	0x3ff551a4ca5d920f /* 2^( 53 /128) */
	.quad	0x3ff56f4736b527da /* 2^( 54 /128) */
	.quad	0x3ff58d12d497c7fd /* 2^( 55 /128) */
	.quad	0x3ff5ab07dd485429 /* 2^( 56 /128) */
	.quad	0x3ff5c9268a5946b7 /* 2^( 57 /128) */
	.quad	0x3ff5e76f15ad2148 /* 2^( 58 /128) */
	.quad	0x3ff605e1b976dc09 /* 2^( 59 /128) */
	.quad	0x3ff6247eb03a5585 /* 2^( 60 /128) */
	.quad	0x3ff6434634ccc320 /* 2^( 61 /128) */
	.quad	0x3ff6623882552225 /* 2^( 62 /128) */
	.quad	0x3ff68155d44ca973 /* 2^( 63 /128) */
	.quad	0x3ff6a09e667f3bcd /* 2^( 64 /128) */
	.quad	0x3ff6c012750bdabf /* 2^( 65 /128) */
	.quad	0x3ff6dfb23c651a2f /* 2^( 66 /128) */
	.quad	0x3ff6ff7df9519484 /* 2^( 67 /128) */
	.quad	0x3ff71f75e8ec5f74 /* 2^( 68 /128) */
	.quad	0x3ff73f9a48a58174 /* 2^( 69 /128) */
	.quad	0x3ff75feb564267c9 /* 2^( 70 /128) */
	.quad	0x3ff780694fde5d3f /* 2^( 71 /128) */
	.quad	0x3ff7a11473eb0187 /* 2^( 72 /128) */
	.quad	0x3ff7c1ed0130c132 /* 2^( 73 /128) */
	.quad	0x3ff7e2f336cf4e62 /* 2^( 74 /128) */
	.quad	0x3ff80427543e1a12 /* 2^( 75 /128) */
	.quad	0x3ff82589994cce13 /* 2^( 76 /128) */
	.quad	0x3ff8471a4623c7ad /* 2^( 77 /128) */
	.quad	0x3ff868d99b4492ed /* 2^( 78 /128) */
	.quad	0x3ff88ac7d98a6699 /* 2^( 79 /128) */
	.quad	0x3ff8ace5422aa0db /* 2^( 80 /128) */
	.quad	0x3ff8cf3216b5448c /* 2^( 81 /128) */
	.quad	0x3ff8f1ae99157736 /* 2^( 82 /128) */
	.quad	0x3ff9145b0b91ffc6 /* 2^( 83 /128) */
	.quad	0x3ff93737b0cdc5e5 /* 2^( 84 /128) */
	.quad	0x3ff95a44cbc8520f /* 2^( 85 /128) */
	.quad	0x3ff97d829fde4e50 /* 2^( 86 /128) */
	.quad	0x3ff9a0f170ca07ba /* 2^( 87 /128) */
	.quad	0x3ff9c49182a3f090 /* 2^( 88 /128) */
	.quad	0x3ff9e86319e32323 /* 2^( 89 /128) */
	.quad	0x3ffa0c667b5de565 /* 2^( 90 /128) */
	.quad	0x3ffa309bec4a2d33 /* 2^( 91 /128) */
	.quad	0x3ffa5503b23e255d /* 2^( 92 /128) */
	.quad	0x3ffa799e1330b358 /* 2^( 93 /128) */
	.quad	0x3ffa9e6b5579fdbf /* 2^( 94 /128) */
	.quad	0x3ffac36bbfd3f37a /* 2^( 95 /128) */
	.quad	0x3ffae89f995ad3ad /* 2^( 96 /128) */
	.quad	0x3ffb0e07298db666 /* 2^( 97 /128) */
	.quad	0x3ffb33a2b84f15fb /* 2^( 98 /128) */
	.quad	0x3ffb59728de5593a /* 2^( 99 /128) */
	.quad	0x3ffb7f76f2fb5e47 /* 2^( 100 /128) */
	.quad	0x3ffba5b030a1064a /* 2^( 101 /128) */
	.quad	0x3ffbcc1e904bc1d2 /* 2^( 102 /128) */
	.quad	0x3ffbf2c25bd71e09 /* 2^( 103 /128) */
	.quad	0x3ffc199bdd85529c /* 2^( 104 /128) */
	.quad	0x3ffc40ab5fffd07a /* 2^( 105 /128) */
	.quad	0x3ffc67f12e57d14b /* 2^( 106 /128) */
	.quad	0x3ffc8f6d9406e7b5 /* 2^( 107 /128) */
	.quad	0x3ffcb720dcef9069 /* 2^( 108 /128) */
	.quad	0x3ffcdf0b555dc3fa /* 2^( 109 /128) */
	.quad	0x3ffd072d4a07897c /* 2^( 110 /128) */
	.quad	0x3ffd2f87080d89f2 /* 2^( 111 /128) */
	.quad	0x3ffd5818dcfba487 /* 2^( 112 /128) */
	.quad	0x3ffd80e316c98398 /* 2^( 113 /128) */
	.quad	0x3ffda9e603db3285 /* 2^( 114 /128) */
	.quad	0x3ffdd321f301b460 /* 2^( 115 /128) */
	.quad	0x3ffdfc97337b9b5f /* 2^( 116 /128) */
	.quad	0x3ffe264614f5a129 /* 2^( 117 /128) */
	.quad	0x3ffe502ee78b3ff6 /* 2^( 118 /128) */
	.quad	0x3ffe7a51fbc74c83 /* 2^( 119 /128) */
	.quad	0x3ffea4afa2a490da /* 2^( 120 /128) */
	.quad	0x3ffecf482d8e67f1 /* 2^( 121 /128) */
	.quad	0x3ffefa1bee615a27 /* 2^( 122 /128) */
	.quad	0x3fff252b376bba97 /* 2^( 123 /128) */
	.quad	0x3fff50765b6e4540 /* 2^( 124 /128) */
	.quad	0x3fff7bfdad9cbe14 /* 2^( 125 /128) */
	.quad	0x3fffa7c1819e90d8 /* 2^( 126 /128) */
	.quad	0x3fffd3c22b8f71f1 /* 2^( 127 /128) */
	.align	16
	.quad	0x407a934f0979a371, 0x407a934f0979a371 /* _dbLg2_10*2^K */
	.align	16
	.quad	0x4338800000000000, 0x4338800000000000 /* _dbShifter */
	.align	16
	.quad	0x3f63441350a00000, 0x3f63441350a00000 /* _dbInvLg2_10hi/2^K 53-11-K bits */
	.align	16
	.quad	0xbd10c0219dc1da99, 0xbd10c0219dc1da99 /* _dbInvLg2_10lo/2^K */
	//PC0 = 1.0
	.align	16
	.quad	0x40026bb1bbb55516, 0x40026bb1bbb55516 /* _dPC1 */
	.align	16
	.quad	0x40053524c73ce8e3, 0x40053524c73ce8e3 /* _dPC2 */
	.align	16
	.quad	0x4000470591ccea8b, 0x4000470591ccea8b /* _dPC3 */
	.align	16
	.quad	0x3ff2bd767584db59, 0x3ff2bd767584db59 /* _dPC4 */
	.align	16
	.quad	0x3fe144c03efafb54, 0x3fe144c03efafb54 /* _dPC5 */
	.align	16
	.quad	0xfff0000000000000, 0xfff0000000000000 /* _lExpMask */
	.align	16
	.long	0x0000007f, 0x0000007f, 0x0000007f, 0x0000007f /* _iIndexMask =(2^K-1) */
	//common
	.align	16
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	16
	.long	0x40733a70, 0x40733a70, 0x40733a70, 0x40733a70 /* _iDomainRange */
	.align	16
	.type	__svml_dexp10_data_internal, @object
	.size	__svml_dexp10_data_internal, .-__svml_dexp10_data_internal
