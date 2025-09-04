/* Function cosh vectorized with SSE4.
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
 *   Compute cosh(x) as (exp(x)+exp(-x))/2,
 *   where exp is calculated as
 *   exp(M*ln2 + ln2*(j/2^k) + r) = 2^M * 2^(j/2^k) * exp(r)
 *
 *   Special cases:
 *
 *   cosh(NaN) = quiet NaN, and raise invalid exception
 *   cosh(INF) = that INF
 *   cosh(0)   = 1
 *   cosh(x) overflows for big x and returns MAXLOG+log(2)
 *
 */

/* Offsets for data table __svml_dcosh_data_internal
 */
#define _dbT				0
#define _dbInvLn2			2064
#define _dbLn2hi			2080
#define _dbLn2lo			2096
#define _dbShifter			2112
#define _iIndexMask			2128
#define _dPC2				2144
#define _dPC3				2160
#define _dPC4				2176
#define _iMaxIndex			2192
#define _lExpMask			2208
#define _dSign				2224
#define _iDomainRange			2240

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_cosh_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm4
	movups	_dSign+__svml_dcosh_data_internal(%rip), %xmm2
	lea	_dbT+__svml_dcosh_data_internal(%rip), %r8

	/*  Abs argument  */
	movaps	%xmm2, %xmm5

	/* dXSign=0x001000000000 */
	psrlq	$11, %xmm2

	/*
	 *  Load argument
	 * dM = x*2^K/log(2) + RShifter
	 */
	movups	_dbInvLn2+__svml_dcosh_data_internal(%rip), %xmm3
	andnps	%xmm4, %xmm5
	mulpd	%xmm5, %xmm3
	movups	_dbShifter+__svml_dcosh_data_internal(%rip), %xmm1
	addpd	%xmm1, %xmm3

	/*
	 *  R
	 * dN = dM - RShifter
	 */
	movaps	%xmm3, %xmm15
	subpd	%xmm1, %xmm15

	/* dR = dX - dN*Log2_hi/2^K */
	movups	_dbLn2hi+__svml_dcosh_data_internal(%rip), %xmm14
	mulpd	%xmm15, %xmm14

	/* dR = (dX - dN*Log2_hi/2^K) - dN*Log2_lo/2^K */
	movups	_dbLn2lo+__svml_dcosh_data_internal(%rip), %xmm1
	mulpd	%xmm15, %xmm1

	/*
	 * Check for overflow\underflow
	 *
	 */
	pshufd	$221, %xmm5, %xmm7
	subpd	%xmm14, %xmm5
	movq	_iIndexMask+__svml_dcosh_data_internal(%rip), %xmm8

	/*  Index and lookup  */
	pshufd	$136, %xmm3, %xmm9

	/*
	 *  G1, G2, G3: dTdif, dTn * 2^N, 2^(-N)
	 * NB: copied from sinh_la - to be optimized!!!!!
	 */
	psllq	$44, %xmm3

	/*
	 * trick
	 * 256=-iIndex
	 */
	movq	_iMaxIndex+__svml_dcosh_data_internal(%rip), %xmm12
	pand	%xmm8, %xmm9
	subpd	%xmm1, %xmm5
	psubd	%xmm9, %xmm12

	/* iIndex*=3 */
	movdqa	%xmm9, %xmm10

	/* iDomainRange*=3 */
	pslld	$3, %xmm12
	pslld	$3, %xmm10
	movd	%xmm12, %esi
	pshufd	$1, %xmm12, %xmm13
	movq	_iDomainRange+__svml_dcosh_data_internal(%rip), %xmm6
	movd	%xmm13, %edi
	pcmpgtd	%xmm6, %xmm7
	movmskps %xmm7, %eax

	/* dR2 = dR^2 */
	movaps	%xmm5, %xmm7

	/* lM now is an EXP(2^N) */
	pand	_lExpMask+__svml_dcosh_data_internal(%rip), %xmm3
	pshufd	$1, %xmm10, %xmm11
	movslq	%esi, %rsi
	mulpd	%xmm5, %xmm7
	movd	%xmm10, %edx
	movsd	(%r8, %rsi), %xmm6
	movd	%xmm11, %ecx
	movslq	%edi, %rdi
	movslq	%edx, %rdx
	movslq	%ecx, %rcx
	movhpd	(%r8, %rdi), %xmm6

	/*  */
	psubq	%xmm3, %xmm6

	/* lX- = EXP(1/2) */
	psubq	%xmm2, %xmm6

	/*
	 * sinh(r) = r +r*r^2*a3 ....
	 * dSinh_r = r^2*a3
	 */
	movups	_dPC3+__svml_dcosh_data_internal(%rip), %xmm2
	mulpd	%xmm7, %xmm2

	/* dSinh_r = r + r*r^2*a3 */
	mulpd	%xmm5, %xmm2
	movsd	(%r8, %rdx), %xmm0
	movhpd	(%r8, %rcx), %xmm0
	paddq	%xmm3, %xmm0
	addpd	%xmm2, %xmm5

	/* dTn = dTn*2^N - dTn*2^-N */
	movaps	%xmm0, %xmm3
	subpd	%xmm6, %xmm3

	/* dTp = dTn*2^N + dTn*2^-N */
	addpd	%xmm6, %xmm0
	mulpd	%xmm5, %xmm3

	/* poly(r) = dTp + dTn*sinh(dR)+dTp*dR2*(a2 +a4*dR2) */
	movups	_dPC4+__svml_dcosh_data_internal(%rip), %xmm5
	mulpd	%xmm7, %xmm5
	addpd	_dPC2+__svml_dcosh_data_internal(%rip), %xmm5
	mulpd	%xmm5, %xmm7

	/* dTn*sinh(dR)+dTp*dR2*(a2 +a4*dR2) */
	mulpd	%xmm0, %xmm7
	addpd	%xmm7, %xmm3

	/* _VRES1 = dTp + dTn*sinh(dR)+dTp*dR2*(a2 +a4*dR2) */
	addpd	%xmm3, %xmm0
	andl	$3, %eax

	/*  Ret H  */

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
	# LOE rbx rbp r12 r13 r14 r15 eax xmm0

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
	movsd	32(%rsp, %r14, 8), %xmm0
	call	cosh@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN2v_cosh_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dcosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _dbT[(1+(1<<8))][2]; // dTpj ONLY!
	__declspec(align(16)) VUINT32 _dbInvLn2[2][2];
	__declspec(align(16)) VUINT32 _dbLn2hi[2][2];
	__declspec(align(16)) VUINT32 _dbLn2lo[2][2];
	__declspec(align(16)) VUINT32 _dbShifter[2][2];
	__declspec(align(16)) VUINT32 _iIndexMask[4][1]; // (1<<K)1-
	__declspec(align(16)) VUINT32 _dPC2[2][2];
	__declspec(align(16)) VUINT32 _dPC3[2][2];
	__declspec(align(16)) VUINT32 _dPC4[2][2];
	__declspec(align(16)) VUINT32 _iMaxIndex[4][1]; // (1<<K)
	__declspec(align(16)) VUINT32 _lExpMask[2][2];
	__declspec(align(16)) VUINT32 _dSign[2][2]; // 0x8000000000000000
	__declspec(align(16)) VUINT32 _iDomainRange[4][1];
} __svml_dcosh_data_internal;
#endif
__svml_dcosh_data_internal:
	/* _dbT */
	.quad	0x3fe0000000000000, 0x3fe00b1afa5abcbf, 0x3fe0163da9fb3335, 0x3fe02168143b0281
	.quad	0x3fe02c9a3e778061, 0x3fe037d42e11bbcc, 0x3fe04315e86e7f85, 0x3fe04e5f72f654b1
	.quad	0x3fe059b0d3158574, 0x3fe0650a0e3c1f89, 0x3fe0706b29ddf6de, 0x3fe07bd42b72a836
	.quad	0x3fe0874518759bc8, 0x3fe092bdf66607e0, 0x3fe09e3ecac6f383, 0x3fe0a9c79b1f3919
	.quad	0x3fe0b5586cf9890f, 0x3fe0c0f145e46c85, 0x3fe0cc922b7247f7, 0x3fe0d83b23395dec
	.quad	0x3fe0e3ec32d3d1a2, 0x3fe0efa55fdfa9c5, 0x3fe0fb66affed31b, 0x3fe1073028d7233e
	.quad	0x3fe11301d0125b51, 0x3fe11edbab5e2ab6, 0x3fe12abdc06c31cc, 0x3fe136a814f204ab
	.quad	0x3fe1429aaea92de0, 0x3fe14e95934f312e, 0x3fe15a98c8a58e51, 0x3fe166a45471c3c2
	.quad	0x3fe172b83c7d517b, 0x3fe17ed48695bbc0, 0x3fe18af9388c8dea, 0x3fe1972658375d2f
	.quad	0x3fe1a35beb6fcb75, 0x3fe1af99f8138a1c, 0x3fe1bbe084045cd4, 0x3fe1c82f95281c6b
	.quad	0x3fe1d4873168b9aa, 0x3fe1e0e75eb44027, 0x3fe1ed5022fcd91d, 0x3fe1f9c18438ce4d
	.quad	0x3fe2063b88628cd6, 0x3fe212be3578a819, 0x3fe21f49917ddc96, 0x3fe22bdda27912d1
	.quad	0x3fe2387a6e756238, 0x3fe2451ffb82140a, 0x3fe251ce4fb2a63f, 0x3fe25e85711ece75
	.quad	0x3fe26b4565e27cdd, 0x3fe2780e341ddf29, 0x3fe284dfe1f56381, 0x3fe291ba7591bb70
	.quad	0x3fe29e9df51fdee1, 0x3fe2ab8a66d10f13, 0x3fe2b87fd0dad990, 0x3fe2c57e39771b2f
	.quad	0x3fe2d285a6e4030b, 0x3fe2df961f641589, 0x3fe2ecafa93e2f56, 0x3fe2f9d24abd886b
	.quad	0x3fe306fe0a31b715, 0x3fe31432edeeb2fd, 0x3fe32170fc4cd831, 0x3fe32eb83ba8ea32
	.quad	0x3fe33c08b26416ff, 0x3fe3496266e3fa2d, 0x3fe356c55f929ff1, 0x3fe36431a2de883b
	.quad	0x3fe371a7373aa9cb, 0x3fe37f26231e754a, 0x3fe38cae6d05d866, 0x3fe39a401b7140ef
	.quad	0x3fe3a7db34e59ff7, 0x3fe3b57fbfec6cf4, 0x3fe3c32dc313a8e5, 0x3fe3d0e544ede173
	.quad	0x3fe3dea64c123422, 0x3fe3ec70df1c5175, 0x3fe3fa4504ac801c, 0x3fe40822c367a024
	.quad	0x3fe4160a21f72e2a, 0x3fe423fb2709468a, 0x3fe431f5d950a897, 0x3fe43ffa3f84b9d4
	.quad	0x3fe44e086061892d, 0x3fe45c2042a7d232, 0x3fe46a41ed1d0057, 0x3fe4786d668b3237
	.quad	0x3fe486a2b5c13cd0, 0x3fe494e1e192aed2, 0x3fe4a32af0d7d3de, 0x3fe4b17dea6db7d7
	.quad	0x3fe4bfdad5362a27, 0x3fe4ce41b817c114, 0x3fe4dcb299fddd0d, 0x3fe4eb2d81d8abff
	.quad	0x3fe4f9b2769d2ca7, 0x3fe508417f4531ee, 0x3fe516daa2cf6642, 0x3fe5257de83f4eef
	.quad	0x3fe5342b569d4f82, 0x3fe542e2f4f6ad27, 0x3fe551a4ca5d920f, 0x3fe56070dde910d2
	.quad	0x3fe56f4736b527da, 0x3fe57e27dbe2c4cf, 0x3fe58d12d497c7fd, 0x3fe59c0827ff07cc
	.quad	0x3fe5ab07dd485429, 0x3fe5ba11fba87a03, 0x3fe5c9268a5946b7, 0x3fe5d84590998b93
	.quad	0x3fe5e76f15ad2148, 0x3fe5f6a320dceb71, 0x3fe605e1b976dc09, 0x3fe6152ae6cdf6f4
	.quad	0x3fe6247eb03a5585, 0x3fe633dd1d1929fd, 0x3fe6434634ccc320, 0x3fe652b9febc8fb7
	.quad	0x3fe6623882552225, 0x3fe671c1c70833f6, 0x3fe68155d44ca973, 0x3fe690f4b19e9538
	.quad	0x3fe6a09e667f3bcd, 0x3fe6b052fa75173e, 0x3fe6c012750bdabf, 0x3fe6cfdcddd47645
	.quad	0x3fe6dfb23c651a2f, 0x3fe6ef9298593ae5, 0x3fe6ff7df9519484, 0x3fe70f7466f42e87
	.quad	0x3fe71f75e8ec5f74, 0x3fe72f8286ead08a, 0x3fe73f9a48a58174, 0x3fe74fbd35d7cbfd
	.quad	0x3fe75feb564267c9, 0x3fe77024b1ab6e09, 0x3fe780694fde5d3f, 0x3fe790b938ac1cf6
	.quad	0x3fe7a11473eb0187, 0x3fe7b17b0976cfdb, 0x3fe7c1ed0130c132, 0x3fe7d26a62ff86f0
	.quad	0x3fe7e2f336cf4e62, 0x3fe7f3878491c491, 0x3fe80427543e1a12, 0x3fe814d2add106d9
	.quad	0x3fe82589994cce13, 0x3fe8364c1eb941f7, 0x3fe8471a4623c7ad, 0x3fe857f4179f5b21
	.quad	0x3fe868d99b4492ed, 0x3fe879cad931a436, 0x3fe88ac7d98a6699, 0x3fe89bd0a478580f
	.quad	0x3fe8ace5422aa0db, 0x3fe8be05bad61778, 0x3fe8cf3216b5448c, 0x3fe8e06a5e0866d9
	.quad	0x3fe8f1ae99157736, 0x3fe902fed0282c8a, 0x3fe9145b0b91ffc6, 0x3fe925c353aa2fe2
	.quad	0x3fe93737b0cdc5e5, 0x3fe948b82b5f98e5, 0x3fe95a44cbc8520f, 0x3fe96bdd9a7670b3
	.quad	0x3fe97d829fde4e50, 0x3fe98f33e47a22a2, 0x3fe9a0f170ca07ba, 0x3fe9b2bb4d53fe0d
	.quad	0x3fe9c49182a3f090, 0x3fe9d674194bb8d5, 0x3fe9e86319e32323, 0x3fe9fa5e8d07f29e
	.quad	0x3fea0c667b5de565, 0x3fea1e7aed8eb8bb, 0x3fea309bec4a2d33, 0x3fea42c980460ad8
	.quad	0x3fea5503b23e255d, 0x3fea674a8af46052, 0x3fea799e1330b358, 0x3fea8bfe53c12e59
	.quad	0x3fea9e6b5579fdbf, 0x3feab0e521356eba, 0x3feac36bbfd3f37a, 0x3fead5ff3a3c2774
	.quad	0x3feae89f995ad3ad, 0x3feafb4ce622f2ff, 0x3feb0e07298db666, 0x3feb20ce6c9a8952
	.quad	0x3feb33a2b84f15fb, 0x3feb468415b749b1, 0x3feb59728de5593a, 0x3feb6c6e29f1c52a
	.quad	0x3feb7f76f2fb5e47, 0x3feb928cf22749e4, 0x3feba5b030a1064a, 0x3febb8e0b79a6f1f
	.quad	0x3febcc1e904bc1d2, 0x3febdf69c3f3a207, 0x3febf2c25bd71e09, 0x3fec06286141b33d
	.quad	0x3fec199bdd85529c, 0x3fec2d1cd9fa652c, 0x3fec40ab5fffd07a, 0x3fec544778fafb22
	.quad	0x3fec67f12e57d14b, 0x3fec7ba88988c933, 0x3fec8f6d9406e7b5, 0x3feca3405751c4db
	.quad	0x3fecb720dcef9069, 0x3feccb0f2e6d1675, 0x3fecdf0b555dc3fa, 0x3fecf3155b5bab74
	.quad	0x3fed072d4a07897c, 0x3fed1b532b08c968, 0x3fed2f87080d89f2, 0x3fed43c8eacaa1d6
	.quad	0x3fed5818dcfba487, 0x3fed6c76e862e6d3, 0x3fed80e316c98398, 0x3fed955d71ff6075
	.quad	0x3feda9e603db3285, 0x3fedbe7cd63a8315, 0x3fedd321f301b460, 0x3fede7d5641c0658
	.quad	0x3fedfc97337b9b5f, 0x3fee11676b197d17, 0x3fee264614f5a129, 0x3fee3b333b16ee12
	.quad	0x3fee502ee78b3ff6, 0x3fee653924676d76, 0x3fee7a51fbc74c83, 0x3fee8f7977cdb740
	.quad	0x3feea4afa2a490da, 0x3feeb9f4867cca6e, 0x3feecf482d8e67f1, 0x3feee4aaa2188510
	.quad	0x3feefa1bee615a27, 0x3fef0f9c1cb6412a, 0x3fef252b376bba97, 0x3fef3ac948dd7274
	.quad	0x3fef50765b6e4540, 0x3fef6632798844f8, 0x3fef7bfdad9cbe14, 0x3fef91d802243c89
	.quad	0x3fefa7c1819e90d8, 0x3fefbdba3692d514, 0x3fefd3c22b8f71f1, 0x3fefe9d96b2a23d9
	.quad	0x3ff0000000000000
	.align	16
	.quad	0x3ff71547652b82fe, 0x3ff71547652b82fe /* _dbInvLn2 = 1/log(2) */
	.align	16
	.quad	0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000 /* _dbLn2hi  = log(2) hi */
	.align	16
	.quad	0xBDAC610CA86C3899, 0xBDAC610CA86C3899 /* _dbLn2lo  = log(2) lo */
	.align	16
	.quad	0x42B8000000000000, 0x42B8000000000000 /* _dbShifter */
	.align	16
	.long	0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF /* _iIndexMask */
	.align	16
	.quad	0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD /* _dPC2 */
	.align	16
	.quad	0x3FC5555570813E14, 0x3FC5555570813E14 /* _dPC3 */
	.align	16
	.quad	0x3FA55555CF16D299, 0x3FA55555CF16D299 /* _dPC4 */
	.align	16
	.long	0x00000100, 0x00000100, 0x00000100, 0x00000100 /* _iMaxIndex */
	.align	16
	.quad	0x7ff0000000000000, 0x7ff0000000000000 /* _lExpMask */
	.align	16
	.quad	0x8000000000000000, 0x8000000000000000 /* _dSign */
	.align	16
	.long	0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99 /* _iDomainRange 0x40861d9ac12a3e85 =(1021*2^K-0.5)*log(2)/2^K -needed for quick exp */
	.align	16
	.type	__svml_dcosh_data_internal, @object
	.size	__svml_dcosh_data_internal, .-__svml_dcosh_data_internal
