/* Function acoshf vectorized with AVX-512.
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
 *   Compute acosh(x) as log(x + sqrt(x*x - 1))
 *   using RSQRT instructions for starting the
 *   square root approximation, and small table lookups for log
 *   that map to AVX-512 permute instructions
 *
 *   Special cases:
 *
 *   acosh(NaN)  = quiet NaN, and raise invalid exception
 *   acosh(-INF) = NaN
 *   acosh(+INF) = +INF
 *   acosh(x)    = NaN if x < 1
 *   acosh(1)    = +0
 *
 */

/* Offsets for data table __svml_sacosh_data_internal_avx512
 */
#define Log_tbl_H			0
#define Log_tbl_L			128
#define One				256
#define SmallThreshold			320
#define Threshold			384
#define LargeThreshold			448
#define ca1				512
#define c2s				576
#define c1s				640
#define AddB5				704
#define RcpBitMask			768
#define OneEighth			832
#define Four				896
#define poly_coeff3			960
#define poly_coeff2			1024
#define poly_coeff1			1088
#define L2H				1152
#define L2L				1216

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_acoshf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	One+__svml_sacosh_data_internal_avx512(%rip), %zmm1

	/*
	 * sqrt(1+x^2) ~ Sh + Sl + Sh*Eh*poly_s
	 * poly_s = c1+c2*Eh
	 */
	vmovups	c2s+__svml_sacosh_data_internal_avx512(%rip), %zmm13
	vmovups	c1s+__svml_sacosh_data_internal_avx512(%rip), %zmm15

	/* polynomial computation for small inputs */
	vmovups	ca1+__svml_sacosh_data_internal_avx512(%rip), %zmm9

	/* very large inputs ? */
	vmovups	Threshold+__svml_sacosh_data_internal_avx512(%rip), %zmm10

	/* out of range inputs? */
	vmovups	LargeThreshold+__svml_sacosh_data_internal_avx512(%rip), %zmm11

	/* not a very small input ? */
	vmovups	SmallThreshold+__svml_sacosh_data_internal_avx512(%rip), %zmm6
	vmovaps	%zmm0, %zmm8

	/* x^2 - 1 */
	vmovaps	%zmm1, %zmm7
	vfmsub231ps {rn-sae}, %zmm8, %zmm8, %zmm7
	vcmpps	$21, {sae}, %zmm10, %zmm8, %k2
	vcmpps	$22, {sae}, %zmm11, %zmm8, %k0
	vcmpps	$18, {sae}, %zmm1, %zmm8, %k1
	vrsqrt14ps %zmm7, %zmm12
	vcmpps	$21, {sae}, %zmm6, %zmm7, %k3
	vmulps	{rn-sae}, %zmm9, %zmm7, %zmm4

	/* Sh ~sqrt(-1+x^2) */
	vmulps	{rn-sae}, %zmm12, %zmm7, %zmm5

	/* Sh+x */
	vaddps	{rn-sae}, %zmm8, %zmm5, %zmm9

	/* (Yh*R0)_low */
	vmovaps	%zmm7, %zmm0
	korw	%k0, %k1, %k0

	/* rel. error term: Eh=1-Sh*R0 */
	vmovaps	%zmm1, %zmm14
	vfmsub213ps {rn-sae}, %zmm5, %zmm12, %zmm0
	vfnmadd231ps {rn-sae}, %zmm5, %zmm12, %zmm14

	/* rel. error term: Eh=(1-Sh*R0)-Sl*R0 */
	vfnmadd231ps {rn-sae}, %zmm0, %zmm12, %zmm14

	/* Sh*Eh */
	vmulps	{rn-sae}, %zmm14, %zmm5, %zmm3
	vfmadd231ps {rn-sae}, %zmm14, %zmm13, %zmm15

	/* Sl + Sh*Eh*poly_s */
	vfmadd213ps {rn-sae}, %zmm0, %zmm15, %zmm3

	/* Shh */
	vsubps	{rn-sae}, %zmm8, %zmm9, %zmm15

	/* polynomial computation for small inputs */
	vaddps	{rn-sae}, %zmm3, %zmm5, %zmm0

	/* Xin0+Sl+Sh*Eh*poly_s ~ x+sqrt(1+x^2) */
	vaddps	{rn-sae}, %zmm3, %zmm9, %zmm2

	/* Shl */
	vsubps	{rn-sae}, %zmm15, %zmm5, %zmm10
	vfmadd231ps {rn-sae}, %zmm0, %zmm4, %zmm0

	/* fixup for very large inputs */
	vmovups	OneEighth+__svml_sacosh_data_internal_avx512(%rip), %zmm4

	/* Sl_high */
	vsubps	{rn-sae}, %zmm9, %zmm2, %zmm5

	/* polynomial */
	vmovups	poly_coeff3+__svml_sacosh_data_internal_avx512(%rip), %zmm9
	vmulps	{rn-sae}, %zmm4, %zmm8, %zmm2{%k2}

	/* -K*L2L + Tl */
	vmovups	L2L+__svml_sacosh_data_internal_avx512(%rip), %zmm4

	/* Sl_l */
	vsubps	{rn-sae}, %zmm5, %zmm3, %zmm3
	vrcp14ps %zmm2, %zmm11
	vmovups	Log_tbl_L+__svml_sacosh_data_internal_avx512(%rip), %zmm5

	/* Xin_low */
	vaddps	{rn-sae}, %zmm10, %zmm3, %zmm13

	/* round reciprocal to 1+4b mantissas */
	vpaddd	AddB5+__svml_sacosh_data_internal_avx512(%rip), %zmm11, %zmm12
	vmovups	poly_coeff1+__svml_sacosh_data_internal_avx512(%rip), %zmm10
	vandps	RcpBitMask+__svml_sacosh_data_internal_avx512(%rip), %zmm12, %zmm14

	/* fixup for very large inputs */
	vxorps	%zmm13, %zmm13, %zmm13{%k2}

	/* reduced argument for log(): (Rcp*Xin-1)+Rcp*Xin_low */
	vfmsub231ps {rn-sae}, %zmm14, %zmm2, %zmm1

	/* exponents */
	vgetexpps {sae}, %zmm14, %zmm12
	vmovups	Four+__svml_sacosh_data_internal_avx512(%rip), %zmm2

	/* Prepare table index */
	vpsrld	$18, %zmm14, %zmm3
	vfmadd231ps {rn-sae}, %zmm14, %zmm13, %zmm1
	vmovups	poly_coeff2+__svml_sacosh_data_internal_avx512(%rip), %zmm13

	/* Table lookups */
	vmovups	__svml_sacosh_data_internal_avx512(%rip), %zmm14
	vsubps	{rn-sae}, %zmm2, %zmm12, %zmm12{%k2}
	vpermt2ps Log_tbl_L+64+__svml_sacosh_data_internal_avx512(%rip), %zmm3, %zmm5
	vpermt2ps Log_tbl_H+64+__svml_sacosh_data_internal_avx512(%rip), %zmm3, %zmm14

	/* R^2 */
	vmulps	{rn-sae}, %zmm1, %zmm1, %zmm11

	/* -K*L2H + Th */
	vmovups	L2H+__svml_sacosh_data_internal_avx512(%rip), %zmm2
	vfmadd231ps {rn-sae}, %zmm1, %zmm9, %zmm13
	vfnmadd231ps {rn-sae}, %zmm12, %zmm2, %zmm14
	vfnmadd213ps {rn-sae}, %zmm5, %zmm4, %zmm12
	vfmadd213ps {rn-sae}, %zmm10, %zmm1, %zmm13

	/* Tl + R^2*Poly */
	vfmadd213ps {rn-sae}, %zmm12, %zmm11, %zmm13

	/* R+Tl + R^2*Poly */
	vaddps	{rn-sae}, %zmm1, %zmm13, %zmm1
	vaddps	{rn-sae}, %zmm1, %zmm14, %zmm0{%k3}

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 k0 zmm0 zmm8

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
	vmovups	%zmm8, 64(%rsp)
	vmovups	%zmm0, 128(%rsp)
	# LOE rbx r12 r13 r14 r15 k0 zmm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax k0

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	kmovd	%k0, %r13d
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
	call	acoshf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_acoshf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sacosh_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Log_tbl_H[32][1];
	__declspec(align(64)) VUINT32 Log_tbl_L[32][1];
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 SmallThreshold[16][1];
	__declspec(align(64)) VUINT32 Threshold[16][1];
	__declspec(align(64)) VUINT32 LargeThreshold[16][1];
	__declspec(align(64)) VUINT32 ca1[16][1];
	__declspec(align(64)) VUINT32 c2s[16][1];
	__declspec(align(64)) VUINT32 c1s[16][1];
	__declspec(align(64)) VUINT32 AddB5[16][1];
	__declspec(align(64)) VUINT32 RcpBitMask[16][1];
	__declspec(align(64)) VUINT32 OneEighth[16][1];
	__declspec(align(64)) VUINT32 Four[16][1];
	__declspec(align(64)) VUINT32 poly_coeff3[16][1];
	__declspec(align(64)) VUINT32 poly_coeff2[16][1];
	__declspec(align(64)) VUINT32 poly_coeff1[16][1];
	__declspec(align(64)) VUINT32 L2H[16][1];
	__declspec(align(64)) VUINT32 L2L[16][1];
} __svml_sacosh_data_internal_avx512;
#endif
__svml_sacosh_data_internal_avx512:
	/* Log_tbl_H */
	.long	0x00000000
	.long	0xbcfc0000
	.long	0xbd788000
	.long	0xbdb78000
	.long	0xbdf14000
	.long	0xbe14a000
	.long	0xbe300000
	.long	0xbe4aa000
	.long	0xbe648000
	.long	0xbe7dc000
	.long	0xbe8b4000
	.long	0xbe974000
	.long	0xbea31000
	.long	0xbeae9000
	.long	0xbeb9d000
	.long	0xbec4d000
	.long	0xbecfa000
	.long	0xbeda2000
	.long	0xbee48000
	.long	0xbeeea000
	.long	0xbef89000
	.long	0xbf012800
	.long	0xbf05f000
	.long	0xbf0aa800
	.long	0xbf0f4000
	.long	0xbf13c800
	.long	0xbf184000
	.long	0xbf1ca000
	.long	0xbf20f000
	.long	0xbf252800
	.long	0xbf295000
	.long	0xbf2d6800
	/* Log_tbl_L */
	.align	64
	.long	0x80000000
	.long	0xb726c39e
	.long	0x3839e7fe
	.long	0xb7528ae5
	.long	0x377891d5
	.long	0xb8297c10
	.long	0x37cf8f58
	.long	0x3852b186
	.long	0x35838656
	.long	0xb80c36af
	.long	0x38235454
	.long	0xb862bae1
	.long	0x37e87bc7
	.long	0x37848150
	.long	0x37202511
	.long	0xb74e1b05
	.long	0x385c1340
	.long	0xb8777bcd
	.long	0x36038656
	.long	0xb7d40984
	.long	0xb80f5faf
	.long	0xb8254b4c
	.long	0xb865c84a
	.long	0x37f0b42d
	.long	0xb83ebce1
	.long	0xb83c2513
	.long	0x37a332c4
	.long	0x3779654f
	.long	0x38602f73
	.long	0x367449f8
	.long	0xb7b4996f
	.long	0xb800986b
	/* One */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* SmallThreshold */
	.align	64
	.long	0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000, 0x39800000
	/* Threshold */
	.align	64
	.long	0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000, 0x5f000000
	/* LargeThreshold */
	.align	64
	.long	0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff
	/* ca1 */
	.align	64
	.long	0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE, 0xbe2AA5DE
	/* c2s */
	.align	64
	.long	0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000, 0x3ec00000
	/* c1s */
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* AddB5 */
	.align	64
	.long	0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000, 0x00020000
	/* RcpBitMask */
	.align	64
	.long	0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000
	/* OneEighth */
	.align	64
	.long	0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000, 0x3e000000
	/* Four */
	.align	64
	.long	0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000, 0x40800000
	/* poly_coeff3 */
	.align	64
	.long	0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810
	/* poly_coeff2 */
	.align	64
	.long	0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e
	/* poly_coeff1 */
	.align	64
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000
	/* L2H = log(2)_high */
	.align	64
	.long	0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000
	/* L2L = log(2)_low */
	.align	64
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4
	.align	64
	.type	__svml_sacosh_data_internal_avx512, @object
	.size	__svml_sacosh_data_internal_avx512, .-__svml_sacosh_data_internal_avx512
