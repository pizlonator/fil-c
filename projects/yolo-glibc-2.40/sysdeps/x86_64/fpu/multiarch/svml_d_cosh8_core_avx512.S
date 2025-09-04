/* Function cosh vectorized with AVX-512.
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
#define _dTp_h				0
#define _dTn_h				128
#define _dbShifter_UISA			256
#define _dPC2_UISA			320
#define _dPC3_UISA			384
#define _dPC4_UISA			448
#define _dPC5_UISA			512
#define _dPC6_UISA			576
#define _dPC7_UISA			640
#define _dbInvLn2			704
#define _dbLn2hi			768
#define _dbLn2lo			832
#define _dbShifter			896
#define _dPC2				960
#define _dPC3				1024
#define _dPC4				1088
#define _lExpMask			1152
#define _dSign				1216
#define _iDomainRange			1280

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_cosh_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	_dSign+__svml_dcosh_data_internal(%rip), %zmm11
	vmovups	_dbShifter_UISA+__svml_dcosh_data_internal(%rip), %zmm15

	/*
	 *  Load argument
	 * dM = x*2^K/log(2) + RShifter
	 */
	vmovups	_dbInvLn2+__svml_dcosh_data_internal(%rip), %zmm4
	vmovups	_dbLn2hi+__svml_dcosh_data_internal(%rip), %zmm2
	vmovups	_dbLn2lo+__svml_dcosh_data_internal(%rip), %zmm3
	vmovups	_dPC7_UISA+__svml_dcosh_data_internal(%rip), %zmm8
	vmovups	_dPC6_UISA+__svml_dcosh_data_internal(%rip), %zmm9
	vmovups	_dPC2_UISA+__svml_dcosh_data_internal(%rip), %zmm7
	vmovups	_dPC3_UISA+__svml_dcosh_data_internal(%rip), %zmm6
	vmovaps	%zmm0, %zmm10

	/*  Abs argument  */
	vandnpd	%zmm10, %zmm11, %zmm5

	/*  Index and lookup  */
	vmovups	__svml_dcosh_data_internal(%rip), %zmm11
	vmovups	_dTn_h+__svml_dcosh_data_internal(%rip), %zmm0
	vfmadd213pd {rn-sae}, %zmm15, %zmm5, %zmm4

	/*
	 * Check for overflow\underflow
	 *
	 */
	vpsrlq	$32, %zmm5, %zmm12

	/* dN = dM - RShifter */
	vsubpd	{rn-sae}, %zmm15, %zmm4, %zmm1
	vpmovqd	%zmm12, %ymm13
	vpermt2pd _dTn_h+64+__svml_dcosh_data_internal(%rip), %zmm4, %zmm0
	vpermt2pd _dTp_h+64+__svml_dcosh_data_internal(%rip), %zmm4, %zmm11

	/* dR = dX - dN*Log2_hi/2^K */
	vfnmadd231pd {rn-sae}, %zmm2, %zmm1, %zmm5

	/*
	 * poly(r) = Gmjp(1 + a2*r^2 + a4*r^4) + Gmjn*(r+ a3*r^3 +a5*r^5)       =
	 * = Gmjp_h +Gmjp_l+ Gmjp*r^2*(a2 + a4*r^2) + Gmjn*(r+ r^3*(a3 +a5*r^2)
	 */
	vmovups	_dPC5_UISA+__svml_dcosh_data_internal(%rip), %zmm12
	vpsllq	$48, %zmm4, %zmm2

	/* dR = dX - dN*Log2_hi/2^K */
	vfnmadd231pd {rn-sae}, %zmm3, %zmm1, %zmm5
	vmulpd	{rn-sae}, %zmm5, %zmm5, %zmm1
	vfmadd231pd {rn-sae}, %zmm1, %zmm8, %zmm12
	vmovups	_dPC4_UISA+__svml_dcosh_data_internal(%rip), %zmm8
	vfmadd213pd {rn-sae}, %zmm6, %zmm1, %zmm12
	vfmadd231pd {rn-sae}, %zmm1, %zmm9, %zmm8
	vfmadd213pd {rn-sae}, %zmm7, %zmm1, %zmm8
	vpcmpgtd _iDomainRange+__svml_dcosh_data_internal(%rip), %ymm13, %ymm14
	vmovmskps %ymm14, %edx

	/* dOut=r^2*(a2 + a4*r^2) */
	vmulpd	{rn-sae}, %zmm1, %zmm8, %zmm6

	/* lM now is an EXP(2^N) */
	vpandq	_lExpMask+__svml_dcosh_data_internal(%rip), %zmm2, %zmm3
	vpaddq	%zmm3, %zmm11, %zmm4
	vpsubq	%zmm3, %zmm0, %zmm0
	vsubpd	{rn-sae}, %zmm0, %zmm4, %zmm14
	vaddpd	{rn-sae}, %zmm0, %zmm4, %zmm13

	/* dM=r^2*(a3 +a5*r^2) */
	vmulpd	{rn-sae}, %zmm1, %zmm12, %zmm0
	vfmadd213pd {rn-sae}, %zmm13, %zmm13, %zmm6

	/* dM= r + r^3*(a3 +a5*r^2) */
	vfmadd213pd {rn-sae}, %zmm5, %zmm5, %zmm0
	vfmadd213pd {rn-sae}, %zmm6, %zmm14, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm10

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
	vmovups	%zmm10, 64(%rsp)
	vmovups	%zmm0, 128(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
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
	vmovups	128(%rsp), %zmm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	64(%rsp, %r14, 8), %xmm0
	call	cosh@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_cosh_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dcosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _dTp_h[(1<<4)][2];
	__declspec(align(64)) VUINT32 _dTn_h[(1<<4)][2];
	__declspec(align(64)) VUINT32 _dbShifter_UISA[8][2];
	__declspec(align(64)) VUINT32 _dPC2_UISA[8][2];
	__declspec(align(64)) VUINT32 _dPC3_UISA[8][2];
	__declspec(align(64)) VUINT32 _dPC4_UISA[8][2];
	__declspec(align(64)) VUINT32 _dPC5_UISA[8][2];
	__declspec(align(64)) VUINT32 _dPC6_UISA[8][2];
	__declspec(align(64)) VUINT32 _dPC7_UISA[8][2];
	__declspec(align(64)) VUINT32 _dbInvLn2[8][2];
	__declspec(align(64)) VUINT32 _dbLn2hi[8][2];
	__declspec(align(64)) VUINT32 _dbLn2lo[8][2];
	__declspec(align(64)) VUINT32 _dbShifter[8][2];
	__declspec(align(64)) VUINT32 _dPC2[8][2];
	__declspec(align(64)) VUINT32 _dPC3[8][2];
	__declspec(align(64)) VUINT32 _dPC4[8][2];
	__declspec(align(64)) VUINT32 _lExpMask[8][2];
	__declspec(align(64)) VUINT32 _dSign[8][2]; // 0x8000000000000000
	__declspec(align(64)) VUINT32 _iDomainRange[16][1];
} __svml_dcosh_data_internal;
#endif
__svml_dcosh_data_internal:
	/* _dTp_h */
	.quad	0x3fe0000000000000, 0x3fe0b5586cf9890f, 0x3fe172b83c7d517b, 0x3fe2387a6e756238
	.quad	0x3fe306fe0a31b715, 0x3fe3dea64c123422, 0x3fe4bfdad5362a27, 0x3fe5ab07dd485429
	.quad	0x3fe6a09e667f3bcd, 0x3fe7a11473eb0187, 0x3fe8ace5422aa0db, 0x3fe9c49182a3f090
	.quad	0x3feae89f995ad3ad, 0x3fec199bdd85529c, 0x3fed5818dcfba487, 0x3feea4afa2a490da
	/* dTn_h */
	.align	64
	.quad	0x3fe0000000000000, 0x3fdea4afa2a490da, 0x3fdd5818dcfba487, 0x3fdc199bdd85529c
	.quad	0x3fdae89f995ad3ad, 0x3fd9c49182a3f090, 0x3fd8ace5422aa0db, 0x3fd7a11473eb0187
	.quad	0x3fd6a09e667f3bcd, 0x3fd5ab07dd485429, 0x3fd4bfdad5362a27, 0x3fd3dea64c123422
	.quad	0x3fd306fe0a31b715, 0x3fd2387a6e756238, 0x3fd172b83c7d517b, 0x3fd0b5586cf9890f
	.align	64
	.quad	0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000, 0x42F8000000000000 /* _dbShifter_UISA */
	.align	64
	.quad	0x3fe0000000000004, 0x3fe0000000000004, 0x3fe0000000000004, 0x3fe0000000000004, 0x3fe0000000000004, 0x3fe0000000000004, 0x3fe0000000000004, 0x3fe0000000000004 /* _dPC2_UISA */
	.align	64
	.quad	0x3fc5555555555543, 0x3fc5555555555543, 0x3fc5555555555543, 0x3fc5555555555543, 0x3fc5555555555543, 0x3fc5555555555543, 0x3fc5555555555543, 0x3fc5555555555543 /* _dPC3_UISA */
	.align	64
	.quad	0x3fa5555555484f37, 0x3fa5555555484f37, 0x3fa5555555484f37, 0x3fa5555555484f37, 0x3fa5555555484f37, 0x3fa5555555484f37, 0x3fa5555555484f37, 0x3fa5555555484f37 /* _dPC4_UISA */
	.align	64
	.quad	0x3f81111111286a0c, 0x3f81111111286a0c, 0x3f81111111286a0c, 0x3f81111111286a0c, 0x3f81111111286a0c, 0x3f81111111286a0c, 0x3f81111111286a0c, 0x3f81111111286a0c /* _dPC5_UISA */
	.align	64
	.quad	0x3f56c183da08f116, 0x3f56c183da08f116, 0x3f56c183da08f116, 0x3f56c183da08f116, 0x3f56c183da08f116, 0x3f56c183da08f116, 0x3f56c183da08f116, 0x3f56c183da08f116 /* _dPC6_UISA */
	.align	64
	.quad	0x3f2a018d76da03da, 0x3f2a018d76da03da, 0x3f2a018d76da03da, 0x3f2a018d76da03da, 0x3f2a018d76da03da, 0x3f2a018d76da03da, 0x3f2a018d76da03da, 0x3f2a018d76da03da /* _dPC7_UISA */
	/* _dbT */
	.align	64
	.quad	0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe /* _dbInvLn2 = 1/log(2) */
	.align	64
	.quad	0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000, 0x3FE62E42FEFC0000 /* _dbLn2hi  = log(2) hi */
	.align	64
	.quad	0xBDAC610CA86C3899, 0xBDAC610CA86C3899, 0xBDAC610CA86C3899, 0xBDAC610CA86C3899, 0xBDAC610CA86C3899, 0xBDAC610CA86C3899, 0xBDAC610CA86C3899, 0xBDAC610CA86C3899 /* _dbLn2lo  = log(2) lo */
	.align	64
	.quad	0x42B8000000000000, 0x42B8000000000000, 0x42B8000000000000, 0x42B8000000000000, 0x42B8000000000000, 0x42B8000000000000, 0x42B8000000000000, 0x42B8000000000000 /* _dbShifter */
	.align	64
	.quad	0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD /* _dPC2 */
	.align	64
	.quad	0x3FC5555570813E14, 0x3FC5555570813E14, 0x3FC5555570813E14, 0x3FC5555570813E14, 0x3FC5555570813E14, 0x3FC5555570813E14, 0x3FC5555570813E14, 0x3FC5555570813E14 /* _dPC3 */
	.align	64
	.quad	0x3FA55555CF16D299, 0x3FA55555CF16D299, 0x3FA55555CF16D299, 0x3FA55555CF16D299, 0x3FA55555CF16D299, 0x3FA55555CF16D299, 0x3FA55555CF16D299, 0x3FA55555CF16D299 /* _dPC4 */
	.align	64
	.quad	0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000 /* _lExpMask */
	.align	64
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000 /* _dSign */
	.align	64
	.long	0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99 /* _iDomainRange 0x40861d9ac12a3e85 =(1021*2^K-0.5)*log(2)/2^K -needed for quick exp */
	.align	64
	.type	__svml_dcosh_data_internal, @object
	.size	__svml_dcosh_data_internal, .-__svml_dcosh_data_internal
