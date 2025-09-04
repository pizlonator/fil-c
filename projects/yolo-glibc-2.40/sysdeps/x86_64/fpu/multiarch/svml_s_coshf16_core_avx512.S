/* Function coshf vectorized with AVX-512.
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

/* Offsets for data table __svml_scosh_data_internal
 */
#define _sExp_tbl_PH			0
#define _sExp_tbl_NH			128
#define _sShifter_UISA			256
#define _iDomainRange_UISA		320
#define _sPC1_UISA			384
#define _sPC2_UISA			448
#define _sPC3_UISA			512
#define _sInvLn2			576
#define _sLn2hi				640
#define _sLn2lo				704
#define _sSign				768
#define _iExpMask			832
#define _sShifter			896
#define _iDomainRange			960
#define _sPC1				1024
#define _sPC2				1088
#define _sPC3				1152

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_coshf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	_sSign+__svml_scosh_data_internal(%rip), %zmm4
	vmovups	_sShifter_UISA+__svml_scosh_data_internal(%rip), %zmm6

	/*
	 *  Load argument
	 * dM = x/log(2) + RShifter
	 */
	vmovups	_sInvLn2+__svml_scosh_data_internal(%rip), %zmm10
	vmovups	_sLn2hi+__svml_scosh_data_internal(%rip), %zmm7
	vmovups	_sLn2lo+__svml_scosh_data_internal(%rip), %zmm9

	/*  */
	vmovups	_sPC3_UISA+__svml_scosh_data_internal(%rip), %zmm2

	/* x^2 */
	vmovups	_sPC2_UISA+__svml_scosh_data_internal(%rip), %zmm3

	/*  G1, G2 2^N, 2^(-N)  */
	vmovups	__svml_scosh_data_internal(%rip), %zmm12
	vmovups	_sExp_tbl_NH+__svml_scosh_data_internal(%rip), %zmm13

	/*
	 *  Implementation
	 *  Abs argument
	 */
	vandnps	%zmm0, %zmm4, %zmm1

	/* Check for overflow\underflow  */
	vpternlogd $255, %zmm5, %zmm5, %zmm5
	vfmadd213ps {rn-sae}, %zmm6, %zmm1, %zmm10
	vpcmpd	$1, _iDomainRange_UISA+__svml_scosh_data_internal(%rip), %zmm1, %k1

	/* iM now is an EXP(2^N) */
	vpslld	$18, %zmm10, %zmm11

	/*
	 *  R
	 * sN = sM - RShifter
	 */
	vsubps	{rn-sae}, %zmm6, %zmm10, %zmm8
	vpermt2ps _sExp_tbl_PH+64+__svml_scosh_data_internal(%rip), %zmm10, %zmm12
	vpermt2ps _sExp_tbl_NH+64+__svml_scosh_data_internal(%rip), %zmm10, %zmm13
	vpandnd	%zmm1, %zmm1, %zmm5{%k1}

	/* sR = sX - sN*Log2_hi */
	vfnmadd231ps {rn-sae}, %zmm7, %zmm8, %zmm1
	vptestmd %zmm5, %zmm5, %k0

	/* sR = (sX - sN*Log2_hi) - sN*Log2_lo */
	vfnmadd231ps {rn-sae}, %zmm9, %zmm8, %zmm1
	kmovw	%k0, %edx
	vmulps	{rn-sae}, %zmm1, %zmm1, %zmm4
	vmulps	{rn-sae}, %zmm4, %zmm2, %zmm2

	/* sSinh_r = r + r*(r^2*(a3)) */
	vfmadd213ps {rn-sae}, %zmm1, %zmm1, %zmm2

	/* sOut = r^2*(a2) */
	vmulps	{rn-sae}, %zmm4, %zmm3, %zmm1
	vpandd	_iExpMask+__svml_scosh_data_internal(%rip), %zmm11, %zmm14
	vpaddd	%zmm14, %zmm12, %zmm15
	vpsubd	%zmm14, %zmm13, %zmm10

	/* sG2 = 2^N*Th + 2^(-N)*T_h */
	vaddps	{rn-sae}, %zmm10, %zmm15, %zmm5

	/* sG1 = 2^N*Th - 2^(-N)*T_h */
	vsubps	{rn-sae}, %zmm10, %zmm15, %zmm6

	/* res = sG1*(r + r*(r^2*(a3))) + sG2*(1+r^2*(a2)) */
	vfmadd213ps {rn-sae}, %zmm5, %zmm5, %zmm1
	vfmadd213ps {rn-sae}, %zmm1, %zmm2, %zmm6
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm6

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm6, %zmm0
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
	vmovups	%zmm0, 64(%rsp)
	vmovups	%zmm6, 128(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm6

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
	vmovups	128(%rsp), %zmm6

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm6

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	64(%rsp, %r14, 4), %xmm0
	call	coshf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_coshf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_scosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _sExp_tbl_PH[32][1];
	__declspec(align(64)) VUINT32 _sExp_tbl_NH[32][1];
	__declspec(align(64)) VUINT32 _sShifter_UISA[16][1];
	__declspec(align(64)) VUINT32 _iDomainRange_UISA[16][1];
	__declspec(align(64)) VUINT32 _sPC1_UISA[16][1];
	__declspec(align(64)) VUINT32 _sPC2_UISA[16][1];
	__declspec(align(64)) VUINT32 _sPC3_UISA[16][1];
	__declspec(align(64)) VUINT32 _sInvLn2[16][1];
	__declspec(align(64)) VUINT32 _sLn2hi[16][1];
	__declspec(align(64)) VUINT32 _sLn2lo[16][1];
	__declspec(align(64)) VUINT32 _sSign[16][1];
	__declspec(align(64)) VUINT32 _iExpMask[16][1];
	__declspec(align(64)) VUINT32 _sShifter[16][1];
	__declspec(align(64)) VUINT32 _iDomainRange[16][1];
	__declspec(align(64)) VUINT32 _sPC1[16][1];
	__declspec(align(64)) VUINT32 _sPC2[16][1];
	__declspec(align(64)) VUINT32 _sPC3[16][1];
} __svml_scosh_data_internal;
#endif
__svml_scosh_data_internal:
	/* _sExp_tbl_PH 2^(i/32-1), i=0..31 */
	.long	0x3f000000, 0x3f02cd87, 0x3f05aac3, 0x3f08980f
	.long	0x3f0b95c2, 0x3f0ea43a, 0x3f11c3d3, 0x3f14f4f0
	.long	0x3f1837f0, 0x3f1b8d3a, 0x3f1ef532, 0x3f227043
	.long	0x3f25fed7, 0x3f29a15b, 0x3f2d583f, 0x3f3123f6
	.long	0x3f3504f3, 0x3f38fbaf, 0x3f3d08a4, 0x3f412c4d
	.long	0x3f45672a, 0x3f49b9be, 0x3f4e248c, 0x3f52a81e
	.long	0x3f5744fd, 0x3f5bfbb8, 0x3f60ccdf, 0x3f65b907
	.long	0x3f6ac0c7, 0x3f6fe4ba, 0x3f75257d, 0x3f7a83b3
	/* _sExp_tbl_NH 2^(-i/32-1), i=0..31 */
	.align	64
	.long	0x3f000000, 0x3efa83b3, 0x3ef5257d, 0x3eefe4ba
	.long	0x3eeac0c7, 0x3ee5b907, 0x3ee0ccdf, 0x3edbfbb8
	.long	0x3ed744fd, 0x3ed2a81e, 0x3ece248c, 0x3ec9b9be
	.long	0x3ec5672a, 0x3ec12c4d, 0x3ebd08a4, 0x3eb8fbaf
	.long	0x3eb504f3, 0x3eb123f6, 0x3ead583f, 0x3ea9a15b
	.long	0x3ea5fed7, 0x3ea27043, 0x3e9ef532, 0x3e9b8d3a
	.long	0x3e9837f0, 0x3e94f4f0, 0x3e91c3d3, 0x3e8ea43a
	.long	0x3e8b95c2, 0x3e88980f, 0x3e85aac3, 0x3e82cd87
	.align	64
	.long	0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000 /* 1.5*2^18 _sShifter_UISA */
	.align	64
	.long	0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E, 0x42AEAC4E /* _iDomainRange_UISA */
	.align	64
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 /* _sPC1_UISA=1 */
	.align	64
	.long	0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f, 0x3f00010f /* _sPC2_UISA */
	.align	64
	.long	0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd, 0x3e2aaacd /* _sPC3_UISA */
	.align	64
	.long	0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B, 0x3FB8AA3B /* _sInvLn2 */ // k=0
	.align	64
	.long	0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000, 0x3F317000 /* _sLn2hi */
	.align	64
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4 /* _sLn2lo */
	.align	64
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000 /* _sSign */
	.align	64
	.long	0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000 /* _iExpMask */
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
	.type	__svml_scosh_data_internal, @object
	.size	__svml_scosh_data_internal, .-__svml_scosh_data_internal
