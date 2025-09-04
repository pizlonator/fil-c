/* Function asinhf vectorized with AVX-512.
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
 *   Compute asinh(x) as log(x + sqrt(x*x + 1))
 *   using RSQRT instructions for starting the
 *   square root approximation, and small table lookups for log
 *   that map to AVX-512 permute instructions
 *
 *   Special cases:
 *
 *   asinh(NaN) = quiet NaN, and raise invalid exception
 *   asinh(INF) = that INF
 *   asinh(0)   = that 0
 *
 */

/* Offsets for data table __svml_sasinh_data_internal_avx512
 */
#define Log_tbl_H			0
#define Log_tbl_L			128
#define One				256
#define AbsMask				320
#define SmallThreshold			384
#define Threshold			448
#define LargeThreshold			512
#define ca1				576
#define c2s				640
#define c1s				704
#define AddB5				768
#define RcpBitMask			832
#define OneEighth			896
#define Four				960
#define poly_coeff3			1024
#define poly_coeff2			1088
#define poly_coeff1			1152
#define L2H				1216
#define L2L				1280

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_asinhf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovaps	%zmm0, %zmm10

	/* x^2 */
	vmulps	{rn-sae}, %zmm10, %zmm10, %zmm0
	vmovups	One+__svml_sasinh_data_internal_avx512(%rip), %zmm2

	/* polynomial computation for small inputs */
	vmovups	ca1+__svml_sasinh_data_internal_avx512(%rip), %zmm1

	/* not a very small input ? */
	vmovups	SmallThreshold+__svml_sasinh_data_internal_avx512(%rip), %zmm11

	/* 1+x^2 */
	vaddps	{rn-sae}, %zmm2, %zmm0, %zmm7

	/* |input| */
	vandps	AbsMask+__svml_sasinh_data_internal_avx512(%rip), %zmm10, %zmm12

	/* A=max(x^2, 1); */
	vmaxps	{sae}, %zmm0, %zmm2, %zmm14
	vrsqrt14ps %zmm7, %zmm8

	/* B=min(x^2, 1); */
	vminps	{sae}, %zmm0, %zmm2, %zmm15
	vcmpps	$21, {sae}, %zmm11, %zmm12, %k2

	/* B_high */
	vsubps	{rn-sae}, %zmm14, %zmm7, %zmm9

	/* sign bit */
	vxorps	%zmm10, %zmm12, %zmm13

	/* Sh ~sqrt(1+x^2) */
	vmulps	{rn-sae}, %zmm8, %zmm7, %zmm6
	vmovups	LargeThreshold+__svml_sasinh_data_internal_avx512(%rip), %zmm14

	/* B_low */
	vsubps	{rn-sae}, %zmm9, %zmm15, %zmm3

	/* Sh+x */
	vaddps	{rn-sae}, %zmm12, %zmm6, %zmm15

	/* (Yh*R0)_low */
	vfmsub213ps {rn-sae}, %zmm6, %zmm8, %zmm7
	vmulps	{rn-sae}, %zmm1, %zmm0, %zmm9
	vcmpps	$22, {sae}, %zmm14, %zmm12, %k0
	vmovups	c1s+__svml_sasinh_data_internal_avx512(%rip), %zmm1

	/* polynomial computation for small inputs */
	vfmadd213ps {rn-sae}, %zmm12, %zmm12, %zmm9
	kmovw	%k0, %edx

	/* (x^2)_low */
	vmovaps	%zmm10, %zmm4
	vfmsub213ps {rn-sae}, %zmm0, %zmm10, %zmm4

	/* Yl = (x^2)_low + B_low */
	vaddps	{rn-sae}, %zmm4, %zmm3, %zmm5

	/* rel. error term: Eh=1-Sh*R0 */
	vmovaps	%zmm2, %zmm0
	vfnmadd231ps {rn-sae}, %zmm6, %zmm8, %zmm0

	/* Sl = (Yh*R0)_low+(R0*Yl) */
	vfmadd213ps {rn-sae}, %zmm7, %zmm8, %zmm5

	/* very large inputs ? */
	vmovups	Threshold+__svml_sasinh_data_internal_avx512(%rip), %zmm7

	/* rel. error term: Eh=(1-Sh*R0)-Sl*R0 */
	vfnmadd231ps {rn-sae}, %zmm5, %zmm8, %zmm0

	/* sqrt(1+x^2) ~ Sh + Sl + Sh*Eh*poly_s */
	vmovups	c2s+__svml_sasinh_data_internal_avx512(%rip), %zmm8
	vcmpps	$21, {sae}, %zmm7, %zmm12, %k1

	/* Sh*Eh */
	vmulps	{rn-sae}, %zmm0, %zmm6, %zmm4
	vfmadd231ps {rn-sae}, %zmm0, %zmm8, %zmm1

	/* Sl + Sh*Eh*poly_s */
	vfmadd213ps {rn-sae}, %zmm5, %zmm1, %zmm4

	/* Xh */
	vsubps	{rn-sae}, %zmm6, %zmm15, %zmm5

	/* fixup for very large inputs */
	vmovups	OneEighth+__svml_sasinh_data_internal_avx512(%rip), %zmm6

	/* Xin0+Sl+Sh*Eh*poly_s ~ x+sqrt(1+x^2) */
	vaddps	{rn-sae}, %zmm4, %zmm15, %zmm3

	/* Xl */
	vsubps	{rn-sae}, %zmm5, %zmm12, %zmm5

	/* Sl_high */
	vsubps	{rn-sae}, %zmm15, %zmm3, %zmm0
	vmulps	{rn-sae}, %zmm6, %zmm12, %zmm3{%k1}

	/* -K*L2H + Th */
	vmovups	L2H+__svml_sasinh_data_internal_avx512(%rip), %zmm15

	/* Sl_l */
	vsubps	{rn-sae}, %zmm0, %zmm4, %zmm1
	vrcp14ps %zmm3, %zmm6

	/* Table lookups */
	vmovups	__svml_sasinh_data_internal_avx512(%rip), %zmm0

	/* Xin_low */
	vaddps	{rn-sae}, %zmm5, %zmm1, %zmm7

	/* round reciprocal to 1+4b mantissas */
	vpaddd	AddB5+__svml_sasinh_data_internal_avx512(%rip), %zmm6, %zmm4
	vmovups	poly_coeff1+__svml_sasinh_data_internal_avx512(%rip), %zmm5
	vandps	RcpBitMask+__svml_sasinh_data_internal_avx512(%rip), %zmm4, %zmm8

	/* fixup for very large inputs */
	vxorps	%zmm7, %zmm7, %zmm7{%k1}

	/* polynomial */
	vmovups	poly_coeff3+__svml_sasinh_data_internal_avx512(%rip), %zmm4

	/* reduced argument for log(): (Rcp*Xin-1)+Rcp*Xin_low */
	vfmsub231ps {rn-sae}, %zmm8, %zmm3, %zmm2
	vmovups	Four+__svml_sasinh_data_internal_avx512(%rip), %zmm3

	/* exponents */
	vgetexpps {sae}, %zmm8, %zmm1

	/* Prepare table index */
	vpsrld	$18, %zmm8, %zmm14
	vfmadd231ps {rn-sae}, %zmm8, %zmm7, %zmm2
	vmovups	poly_coeff2+__svml_sasinh_data_internal_avx512(%rip), %zmm7
	vsubps	{rn-sae}, %zmm3, %zmm1, %zmm1{%k1}
	vpermt2ps Log_tbl_H+64+__svml_sasinh_data_internal_avx512(%rip), %zmm14, %zmm0
	vmovups	Log_tbl_L+__svml_sasinh_data_internal_avx512(%rip), %zmm3
	vfmadd231ps {rn-sae}, %zmm2, %zmm4, %zmm7
	vfnmadd231ps {rn-sae}, %zmm1, %zmm15, %zmm0

	/* R^2 */
	vmulps	{rn-sae}, %zmm2, %zmm2, %zmm6
	vfmadd213ps {rn-sae}, %zmm5, %zmm2, %zmm7
	vpermt2ps Log_tbl_L+64+__svml_sasinh_data_internal_avx512(%rip), %zmm14, %zmm3

	/* -K*L2L + Tl */
	vmovups	L2L+__svml_sasinh_data_internal_avx512(%rip), %zmm14
	vfnmadd213ps {rn-sae}, %zmm3, %zmm14, %zmm1

	/* Tl + R^2*Poly */
	vfmadd213ps {rn-sae}, %zmm1, %zmm6, %zmm7

	/* R+Tl + R^2*Poly */
	vaddps	{rn-sae}, %zmm2, %zmm7, %zmm2
	vaddps	{rn-sae}, %zmm2, %zmm0, %zmm9{%k2}
	vxorps	%zmm13, %zmm9, %zmm0
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
	call	asinhf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_asinhf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sasinh_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Log_tbl_H[32][1];
	__declspec(align(64)) VUINT32 Log_tbl_L[32][1];
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 AbsMask[16][1];
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
} __svml_sasinh_data_internal_avx512;
#endif
__svml_sasinh_data_internal_avx512:
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
	/* AbsMask */
	.align	64
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* SmallThreshold */
	.align	64
	.long	0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000, 0x3c800000
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
	.type	__svml_sasinh_data_internal_avx512, @object
	.size	__svml_sasinh_data_internal_avx512, .-__svml_sasinh_data_internal_avx512
