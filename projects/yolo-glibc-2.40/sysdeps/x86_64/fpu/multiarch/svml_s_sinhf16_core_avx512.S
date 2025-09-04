/* Function sinhf vectorized with AVX-512.
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
 *   Compute sinh(x) as (exp(x)-exp(-x))/2,
 *   where exp is calculated as
 *   exp(M*ln2 + ln2*(j/2^k) + r) = 2^M * 2^(j/2^k) * exp(r)
 *
 *   Special cases:
 *
 *   sinh(NaN) = quiet NaN, and raise invalid exception
 *   sinh(INF) = that INF
 *   sinh(x)   = x for subnormals
 *   sinh(x) overflows for big x and returns MAXLOG+log(2)
 *
 */

/* Offsets for data table __svml_ssinh_data_internal
 */
#define _sInvLn2			0
#define _sLn2hi				64
#define _sLn2lo				128
#define _sSign				192
#define _sShifter			256
#define _iDomainRange			320
#define _sPC1				384
#define _sPC2				448
#define _sPC3				512
#define _sPC4				576
#define _sPC5				640
#define _sPC6				704
#define _iHalf				768

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_sinhf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovaps	%zmm0, %zmm5

	/*
	 *  Implementation
	 *  Abs argument
	 */
	vandps	_sSign+__svml_ssinh_data_internal(%rip), %zmm5, %zmm4

	/*
	 * Check for overflow\underflow
	 * MORE faster than GE?
	 */
	vpternlogd $255, %zmm6, %zmm6, %zmm6
	vmovups	_sShifter+__svml_ssinh_data_internal(%rip), %zmm7

	/*
	 *  Load argument
	 * dM = x/log(2) + RShifter
	 */
	vmovups	_sInvLn2+__svml_ssinh_data_internal(%rip), %zmm11
	vmovups	_sLn2hi+__svml_ssinh_data_internal(%rip), %zmm8
	vmovups	_sLn2lo+__svml_ssinh_data_internal(%rip), %zmm10
	vmovups	_iHalf+__svml_ssinh_data_internal(%rip), %zmm12
	vmovups	_sPC5+__svml_ssinh_data_internal(%rip), %zmm0
	vmovups	_sPC6+__svml_ssinh_data_internal(%rip), %zmm3

	/* x^2 */
	vmovups	_sPC2+__svml_ssinh_data_internal(%rip), %zmm2
	vxorps	%zmm5, %zmm4, %zmm1
	vfmadd213ps {rn-sae}, %zmm7, %zmm1, %zmm11
	vpcmpd	$2, _iDomainRange+__svml_ssinh_data_internal(%rip), %zmm1, %k1

	/*
	 *  G1, G2 2^N, 2^(-N)
	 * iM now is an EXP(2^N)
	 */
	vpslld	$23, %zmm11, %zmm13

	/*
	 *  R
	 * sN = sM - RShifter
	 */
	vsubps	{rn-sae}, %zmm7, %zmm11, %zmm9
	vpaddd	%zmm13, %zmm12, %zmm14
	vpsubd	%zmm13, %zmm12, %zmm15

	/* sG1 = 2^(N-1)+2^(-N-1) */
	vaddps	{rn-sae}, %zmm15, %zmm14, %zmm7
	vpandnd	%zmm1, %zmm1, %zmm6{%k1}

	/* sR = sX - sN*Log2_hi */
	vfnmadd231ps {rn-sae}, %zmm8, %zmm9, %zmm1
	vptestmd %zmm6, %zmm6, %k0

	/* sG2 = 2^(N-1)-2^(-N-1) */
	vsubps	{rn-sae}, %zmm15, %zmm14, %zmm8

	/* sR = (sX - sN*Log2_hi) - sN*Log2_lo */
	vfnmadd231ps {rn-sae}, %zmm10, %zmm9, %zmm1

	/*
	 * sinh(r) = r*((a1=1)+r^2*(a3+r^2*(a5+{v1 r^2*a7})))) = r + r*(r^2*(a3+r^2*(a5+r^2*a7))) ....
	 * sSinh_r = (a3+r^2*a5)
	 */
	vmovups	_sPC3+__svml_ssinh_data_internal(%rip), %zmm14
	kmovw	%k0, %edx

	/* sR2 = sR^2 */
	vmulps	{rn-sae}, %zmm1, %zmm1, %zmm6
	vfmadd231ps {rn-sae}, %zmm6, %zmm0, %zmm14

	/* sSinh_r = r^2*(a3+r^2*a5) */
	vmulps	{rn-sae}, %zmm6, %zmm14, %zmm0

	/* sSinh_r = r + r*(r^2*(a3+r^2*a5)) */
	vfmadd213ps {rn-sae}, %zmm1, %zmm1, %zmm0

	/*
	 * sinh(X) = sG2 + sG1*sinh(dR) + sG2*sR2*(a2+sR2*(a4+a6*sR2)
	 * sOut = (a4 +a6*sR2)
	 */
	vmovups	_sPC4+__svml_ssinh_data_internal(%rip), %zmm1
	vfmadd231ps {rn-sae}, %zmm6, %zmm3, %zmm1

	/* sOut = a2+sR2*(a4+a6*sR2) */
	vfmadd213ps {rn-sae}, %zmm2, %zmm6, %zmm1

	/* sOut = sR2*(a2+sR2*(a4+a6*sR2) */
	vmulps	{rn-sae}, %zmm6, %zmm1, %zmm2

	/* sOut = sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	vmulps	{rn-sae}, %zmm8, %zmm2, %zmm3

	/* sOut = sG1*sinh(dR)+sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	vfmadd213ps {rn-sae}, %zmm3, %zmm0, %zmm7

	/* sOut = sG2 + sG1*sinh(dR) + sG2*sR2*(a2+sR2*(a4+a6*sR2) */
	vaddps	{rn-sae}, %zmm8, %zmm7, %zmm9

	/*  Ret H  */
	vorps	%zmm9, %zmm4, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm5

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
	vmovups	%zmm5, 64(%rsp)
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
	cmpl	$16, %r12d

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
	vmovss	64(%rsp, %r14, 4), %xmm0
	call	sinhf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_sinhf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_ssinh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _sInvLn2[16][1];
	__declspec(align(64)) VUINT32 _sLn2hi[16][1];
	__declspec(align(64)) VUINT32 _sLn2lo[16][1];
	__declspec(align(64)) VUINT32 _sSign[16][1];
	__declspec(align(64)) VUINT32 _sShifter[16][1];
	__declspec(align(64)) VUINT32 _iDomainRange[16][1];
	__declspec(align(64)) VUINT32 _sPC1[16][1];
	__declspec(align(64)) VUINT32 _sPC2[16][1];
	__declspec(align(64)) VUINT32 _sPC3[16][1];
	__declspec(align(64)) VUINT32 _sPC4[16][1];
	__declspec(align(64)) VUINT32 _sPC5[16][1];
	__declspec(align(64)) VUINT32 _sPC6[16][1];
	__declspec(align(64)) VUINT32 _iHalf[16][1];
} __svml_ssinh_data_internal;
#endif
__svml_ssinh_data_internal:
	.long	0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B /* _sInvLn2 */ // k=0
	.align	64
	.long	0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000 /* _sLn2hi */
	.align	64
	.long	0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4, 0x3805FDF4 /* _sLn2lo */
	.align	64
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000 /* _sSign */
	.align	64
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000 /* _sShifter */
	.align	64
	.long	0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E /* _iDomainRange */
	.align	64
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 /* _sPC1=1 */
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _sPC2 */
	.align	64
	.long	0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57, 0x3e2aaa57 /* _sPC3 */
	.align	64
	.long	0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72, 0x3d2aaa72 /* _sPC4 */
	.align	64
	.long	0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461, 0x3c091461 /* _sPC5 */
	.align	64
	.long	0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3, 0x3ab6a8a3 /* _sPC6 */
	// Integer constants
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000 /* _iHalf */
	.align	64
	.type	__svml_ssinh_data_internal, @object
	.size	__svml_ssinh_data_internal, .-__svml_ssinh_data_internal
