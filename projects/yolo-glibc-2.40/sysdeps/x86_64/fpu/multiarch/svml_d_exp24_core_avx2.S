/* Function exp2 vectorized with AVX2.
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
 *   For IEEE double
 *     if x >= 1024.0 then exp2(x) overflows
 *     if x < -1076.0 then exp2(x) underflows
 *
 */

/* Offsets for data table __svml_dexp2_data_internal
 */
#define _dbT				0
#define _dbShifter			1024
#define _dPC1				1056
#define _dPC2				1088
#define _dPC3				1120
#define _dPC4				1152
#define _lIndexMask			1184
#define _iAbsMask			1216
#define _iDomainRange			1248

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4v_exp2_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/* out, basePtr, iIndex, iBaseOfs, iSize, iGran, iOfs */
	lea	__svml_dexp2_data_internal(%rip), %r8
	vmovupd	_dbShifter+__svml_dexp2_data_internal(%rip), %ymm4
	vmovupd	_lIndexMask+__svml_dexp2_data_internal(%rip), %ymm3
	vmovapd	%ymm0, %ymm1

	/*  Load argument  */
	vaddpd	%ymm4, %ymm1, %ymm2
	vsubpd	%ymm4, %ymm2, %ymm0

	/*  Index and lookup  */
	vandps	%ymm3, %ymm2, %ymm9
	vpandn	%ymm2, %ymm3, %ymm2

	/*  2^N  */
	vpsllq	$45, %ymm2, %ymm3

	/*  R  */
	vsubpd	%ymm0, %ymm1, %ymm15

	/* Check for overflow\underflow  */
	vextractf128 $1, %ymm1, %xmm5

	/*
	 *  Polynomial
	 * poly(dN) = a1*dR+...+a4*dR^4
	 */
	vmovupd	_dPC4+__svml_dexp2_data_internal(%rip), %ymm0
	vshufps	$221, %xmm5, %xmm1, %xmm6
	vandps	_iAbsMask+__svml_dexp2_data_internal(%rip), %xmm6, %xmm7
	vpcmpgtd _iDomainRange+__svml_dexp2_data_internal(%rip), %xmm7, %xmm8
	vfmadd213pd _dPC3+__svml_dexp2_data_internal(%rip), %ymm15, %ymm0
	vmovmskps %xmm8, %eax
	vfmadd213pd _dPC2+__svml_dexp2_data_internal(%rip), %ymm15, %ymm0

	/* a1+...+a4*dR^3 ! */
	vfmadd213pd _dPC1+__svml_dexp2_data_internal(%rip), %ymm15, %ymm0
	vextractf128 $1, %ymm9, %xmm12
	vmovd	%xmm9, %edx
	vmovd	%xmm12, %esi
	shll	$3, %edx
	vpextrd	$2, %xmm9, %ecx
	shll	$3, %esi
	vpextrd	$2, %xmm12, %edi
	shll	$3, %ecx
	vmovq	(%r8, %rdx), %xmm10
	shll	$3, %edi
	vmovq	(%r8, %rsi), %xmm13
	vmovhpd	(%r8, %rcx), %xmm10, %xmm11
	vmovhpd	(%r8, %rdi), %xmm13, %xmm14
	vinsertf128 $1, %xmm14, %ymm11, %ymm4

	/* dR=dR*dT */
	vmulpd	%ymm15, %ymm4, %ymm15

	/*
	 *  Reconstruction
	 * exp2 = {2^N later}*(Tj+Tj*poly)
	 * dN = dT+dT*dR*(a1+...+a4*dR^3)
	 */
	vfmadd213pd %ymm4, %ymm15, %ymm0

	/* quick 2^N */
	vpaddq	%ymm3, %ymm0, %ymm0

	/*  Finish   */
	testl	%eax, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 eax ymm0 ymm1

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
	vmovupd	%ymm1, 32(%rsp)
	vmovupd	%ymm0, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 eax ymm0

	xorl	%edx, %edx
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r13d
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
	vmovupd	64(%rsp), %ymm0

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
	vmovsd	32(%rsp, %r14, 8), %xmm0
	call	exp2@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 64(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN4v_exp2_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_dexp2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _dbT[(1<<7)][2];
	__declspec(align(32)) VUINT32 _dbShifter[4][2];
	__declspec(align(32)) VUINT32 _dPC1[4][2];
	__declspec(align(32)) VUINT32 _dPC2[4][2];
	__declspec(align(32)) VUINT32 _dPC3[4][2];
	__declspec(align(32)) VUINT32 _dPC4[4][2];
	__declspec(align(32)) VUINT32 _lIndexMask[4][2];
	__declspec(align(32)) VUINT32 _iAbsMask[8][1];
	__declspec(align(32)) VUINT32 _iDomainRange[8][1];
} __svml_dexp2_data_internal;
#endif
__svml_dexp2_data_internal:
	/* _dbT */
	.quad	0x3ff0000000000000, 0x3ff0163da9fb3335 /* 2^( 0 /128), 2^( 1 /128) */
	.quad	0x3ff02c9a3e778061, 0x3ff04315e86e7f85 /* 2^( 2 /128), 2^( 3 /128) */
	.quad	0x3ff059b0d3158574, 0x3ff0706b29ddf6de /* 2^( 4 /128), 2^( 5 /128) */
	.quad	0x3ff0874518759bc8, 0x3ff09e3ecac6f383 /* 2^( 6 /128), 2^( 7 /128) */
	.quad	0x3ff0b5586cf9890f, 0x3ff0cc922b7247f7 /* 2^( 8 /128), 2^( 9 /128) */
	.quad	0x3ff0e3ec32d3d1a2, 0x3ff0fb66affed31b /* 2^( 10 /128), 2^( 11 /128) */
	.quad	0x3ff11301d0125b51, 0x3ff12abdc06c31cc /* 2^( 12 /128), 2^( 13 /128) */
	.quad	0x3ff1429aaea92de0, 0x3ff15a98c8a58e51 /* 2^( 14 /128), 2^( 15 /128) */
	.quad	0x3ff172b83c7d517b, 0x3ff18af9388c8dea /* 2^( 16 /128), 2^( 17 /128) */
	.quad	0x3ff1a35beb6fcb75, 0x3ff1bbe084045cd4 /* 2^( 18 /128), 2^( 19 /128) */
	.quad	0x3ff1d4873168b9aa, 0x3ff1ed5022fcd91d /* 2^( 20 /128), 2^( 21 /128) */
	.quad	0x3ff2063b88628cd6, 0x3ff21f49917ddc96 /* 2^( 22 /128), 2^( 23 /128) */
	.quad	0x3ff2387a6e756238, 0x3ff251ce4fb2a63f /* 2^( 24 /128), 2^( 25 /128) */
	.quad	0x3ff26b4565e27cdd, 0x3ff284dfe1f56381 /* 2^( 26 /128), 2^( 27 /128) */
	.quad	0x3ff29e9df51fdee1, 0x3ff2b87fd0dad990 /* 2^( 28 /128), 2^( 29 /128) */
	.quad	0x3ff2d285a6e4030b, 0x3ff2ecafa93e2f56 /* 2^( 30 /128), 2^( 31 /128) */
	.quad	0x3ff306fe0a31b715, 0x3ff32170fc4cd831 /* 2^( 32 /128), 2^( 33 /128) */
	.quad	0x3ff33c08b26416ff, 0x3ff356c55f929ff1 /* 2^( 34 /128), 2^( 35 /128) */
	.quad	0x3ff371a7373aa9cb, 0x3ff38cae6d05d866 /* 2^( 36 /128), 2^( 37 /128) */
	.quad	0x3ff3a7db34e59ff7, 0x3ff3c32dc313a8e5 /* 2^( 38 /128), 2^( 39 /128) */
	.quad	0x3ff3dea64c123422, 0x3ff3fa4504ac801c /* 2^( 40 /128), 2^( 41 /128) */
	.quad	0x3ff4160a21f72e2a, 0x3ff431f5d950a897 /* 2^( 42 /128), 2^( 43 /128) */
	.quad	0x3ff44e086061892d, 0x3ff46a41ed1d0057 /* 2^( 44 /128), 2^( 45 /128) */
	.quad	0x3ff486a2b5c13cd0, 0x3ff4a32af0d7d3de /* 2^( 46 /128), 2^( 47 /128) */
	.quad	0x3ff4bfdad5362a27, 0x3ff4dcb299fddd0d /* 2^( 48 /128), 2^( 49 /128) */
	.quad	0x3ff4f9b2769d2ca7, 0x3ff516daa2cf6642 /* 2^( 50 /128), 2^( 51 /128) */
	.quad	0x3ff5342b569d4f82, 0x3ff551a4ca5d920f /* 2^( 52 /128), 2^( 53 /128) */
	.quad	0x3ff56f4736b527da, 0x3ff58d12d497c7fd /* 2^( 54 /128), 2^( 55 /128) */
	.quad	0x3ff5ab07dd485429, 0x3ff5c9268a5946b7 /* 2^( 56 /128), 2^( 57 /128) */
	.quad	0x3ff5e76f15ad2148, 0x3ff605e1b976dc09 /* 2^( 58 /128), 2^( 59 /128) */
	.quad	0x3ff6247eb03a5585, 0x3ff6434634ccc320 /* 2^( 60 /128), 2^( 61 /128) */
	.quad	0x3ff6623882552225, 0x3ff68155d44ca973 /* 2^( 62 /128), 2^( 63 /128) */
	.quad	0x3ff6a09e667f3bcd, 0x3ff6c012750bdabf /* 2^( 64 /128), 2^( 65 /128) */
	.quad	0x3ff6dfb23c651a2f, 0x3ff6ff7df9519484 /* 2^( 66 /128), 2^( 67 /128) */
	.quad	0x3ff71f75e8ec5f74, 0x3ff73f9a48a58174 /* 2^( 68 /128), 2^( 69 /128) */
	.quad	0x3ff75feb564267c9, 0x3ff780694fde5d3f /* 2^( 70 /128), 2^( 71 /128) */
	.quad	0x3ff7a11473eb0187, 0x3ff7c1ed0130c132 /* 2^( 72 /128), 2^( 73 /128) */
	.quad	0x3ff7e2f336cf4e62, 0x3ff80427543e1a12 /* 2^( 74 /128), 2^( 75 /128) */
	.quad	0x3ff82589994cce13, 0x3ff8471a4623c7ad /* 2^( 76 /128), 2^( 77 /128) */
	.quad	0x3ff868d99b4492ed, 0x3ff88ac7d98a6699 /* 2^( 78 /128), 2^( 79 /128) */
	.quad	0x3ff8ace5422aa0db, 0x3ff8cf3216b5448c /* 2^( 80 /128), 2^( 81 /128) */
	.quad	0x3ff8f1ae99157736, 0x3ff9145b0b91ffc6 /* 2^( 82 /128), 2^( 83 /128) */
	.quad	0x3ff93737b0cdc5e5, 0x3ff95a44cbc8520f /* 2^( 84 /128), 2^( 85 /128) */
	.quad	0x3ff97d829fde4e50, 0x3ff9a0f170ca07ba /* 2^( 86 /128), 2^( 87 /128) */
	.quad	0x3ff9c49182a3f090, 0x3ff9e86319e32323 /* 2^( 88 /128), 2^( 89 /128) */
	.quad	0x3ffa0c667b5de565, 0x3ffa309bec4a2d33 /* 2^( 90 /128), 2^( 91 /128) */
	.quad	0x3ffa5503b23e255d, 0x3ffa799e1330b358 /* 2^( 92 /128), 2^( 93 /128) */
	.quad	0x3ffa9e6b5579fdbf, 0x3ffac36bbfd3f37a /* 2^( 94 /128), 2^( 95 /128) */
	.quad	0x3ffae89f995ad3ad, 0x3ffb0e07298db666 /* 2^( 96 /128), 2^( 97 /128) */
	.quad	0x3ffb33a2b84f15fb, 0x3ffb59728de5593a /* 2^( 98 /128), 2^( 99 /128) */
	.quad	0x3ffb7f76f2fb5e47, 0x3ffba5b030a1064a /* 2^( 100 /128), 2^( 101 /128) */
	.quad	0x3ffbcc1e904bc1d2, 0x3ffbf2c25bd71e09 /* 2^( 102 /128), 2^( 103 /128) */
	.quad	0x3ffc199bdd85529c, 0x3ffc40ab5fffd07a /* 2^( 104 /128), 2^( 105 /128) */
	.quad	0x3ffc67f12e57d14b, 0x3ffc8f6d9406e7b5 /* 2^( 106 /128), 2^( 107 /128) */
	.quad	0x3ffcb720dcef9069, 0x3ffcdf0b555dc3fa /* 2^( 108 /128), 2^( 109 /128) */
	.quad	0x3ffd072d4a07897c, 0x3ffd2f87080d89f2 /* 2^( 110 /128), 2^( 111 /128) */
	.quad	0x3ffd5818dcfba487, 0x3ffd80e316c98398 /* 2^( 112 /128), 2^( 113 /128) */
	.quad	0x3ffda9e603db3285, 0x3ffdd321f301b460 /* 2^( 114 /128), 2^( 115 /128) */
	.quad	0x3ffdfc97337b9b5f, 0x3ffe264614f5a129 /* 2^( 116 /128), 2^( 117 /128) */
	.quad	0x3ffe502ee78b3ff6, 0x3ffe7a51fbc74c83 /* 2^( 118 /128), 2^( 119 /128) */
	.quad	0x3ffea4afa2a490da, 0x3ffecf482d8e67f1 /* 2^( 120 /128), 2^( 121 /128) */
	.quad	0x3ffefa1bee615a27, 0x3fff252b376bba97 /* 2^( 122 /128), 2^( 123 /128) */
	.quad	0x3fff50765b6e4540, 0x3fff7bfdad9cbe14 /* 2^( 124 /128), 2^( 125 /128) */
	.quad	0x3fffa7c1819e90d8, 0x3fffd3c22b8f71f1 /* 2^( 126 /128), 2^( 127 /128) */
	.align	32
	.quad	0x42c8000000000000, 0x42c8000000000000, 0x42c8000000000000, 0x42c8000000000000 /* _dbShifter - 0x433-7=0x42c shifted right on K! */
	//log2(relerr) = -53.547756365162
	.align	32
	.quad	0x3fe62e42fefa3685, 0x3fe62e42fefa3685, 0x3fe62e42fefa3685, 0x3fe62e42fefa3685 /* _dPC1 */
	.align	32
	.quad	0x3fcebfbdff82ca48, 0x3fcebfbdff82ca48, 0x3fcebfbdff82ca48, 0x3fcebfbdff82ca48 /* _dPC2 */
	.align	32
	.quad	0x3fac6b09b180f045, 0x3fac6b09b180f045, 0x3fac6b09b180f045, 0x3fac6b09b180f045 /* _dPC3 */
	.align	32
	.quad	0x3f83b2ab5bb1268f, 0x3f83b2ab5bb1268f, 0x3f83b2ab5bb1268f, 0x3f83b2ab5bb1268f /* _dPC4 */
	.align	32
	.quad	0x000000000000007f, 0x000000000000007f, 0x000000000000007f, 0x000000000000007f /* _lIndexMask =(2^K-1) */
	.align	32
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	32
	.long	0x408fefff, 0x408fefff, 0x408fefff, 0x408fefff, 0x408fefff, 0x408fefff, 0x408fefff, 0x408fefff /* _iDomainRange */
	.align	32
	.type	__svml_dexp2_data_internal, @object
	.size	__svml_dexp2_data_internal, .-__svml_dexp2_data_internal
