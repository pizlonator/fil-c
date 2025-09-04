/* Function acosh vectorized with AVX-512.
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

/* Offsets for data table __svml_dacosh_data_internal_avx512
 */
#define Log_tbl_H			0
#define Log_tbl_L			128
#define One				256
#define SmallThreshold			320
#define Threshold			384
#define LargeThreshold			448
#define ca2				512
#define ca1				576
#define c4s				640
#define c3s				704
#define c2s				768
#define c1s				832
#define AddB5				896
#define RcpBitMask			960
#define OneEighth			1024
#define Four				1088
#define poly_coeff9			1152
#define poly_coeff8			1216
#define poly_coeff7			1280
#define poly_coeff6			1344
#define poly_coeff5			1408
#define poly_coeff4			1472
#define poly_coeff3			1536
#define poly_coeff2			1600
#define poly_coeff1			1664
#define L2H				1728
#define L2L				1792

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_acosh_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	One+__svml_dacosh_data_internal_avx512(%rip), %zmm5

	/* polynomial computation for small inputs */
	vmovups	ca2+__svml_dacosh_data_internal_avx512(%rip), %zmm13
	vmovups	ca1+__svml_dacosh_data_internal_avx512(%rip), %zmm14

	/*
	 * sqrt(1+x^2) ~ Sh + Sl + Sh*Eh*poly_s
	 * poly_s = c1+c2*Eh+c3*Eh^2
	 */
	vmovups	c4s+__svml_dacosh_data_internal_avx512(%rip), %zmm1
	vmovups	c2s+__svml_dacosh_data_internal_avx512(%rip), %zmm2
	vmovups	c1s+__svml_dacosh_data_internal_avx512(%rip), %zmm6

	/* very large inputs ? */
	vmovups	Threshold+__svml_dacosh_data_internal_avx512(%rip), %zmm15

	/* out of range inputs? */
	vmovups	LargeThreshold+__svml_dacosh_data_internal_avx512(%rip), %zmm3

	/* not a very small input ? */
	vmovups	SmallThreshold+__svml_dacosh_data_internal_avx512(%rip), %zmm10
	vmovaps	%zmm0, %zmm12

	/* x^2 - 1 */
	vmovaps	%zmm5, %zmm11
	vfmsub231pd {rn-sae}, %zmm12, %zmm12, %zmm11
	vcmppd	$21, {sae}, %zmm15, %zmm12, %k2
	vcmppd	$22, {sae}, %zmm3, %zmm12, %k0
	vcmppd	$18, {sae}, %zmm5, %zmm12, %k1
	vrsqrt14pd %zmm11, %zmm4
	vcmppd	$21, {sae}, %zmm10, %zmm11, %k3
	vfmadd231pd {rn-sae}, %zmm11, %zmm13, %zmm14
	vmovups	c3s+__svml_dacosh_data_internal_avx512(%rip), %zmm13

	/* Sh ~sqrt(-1+x^2) */
	vmulpd	{rn-sae}, %zmm4, %zmm11, %zmm9
	vmulpd	{rn-sae}, %zmm11, %zmm14, %zmm8

	/* Sh+x */
	vaddpd	{rn-sae}, %zmm12, %zmm9, %zmm15

	/* Shh */
	vsubpd	{rn-sae}, %zmm12, %zmm15, %zmm14

	/* (Yh*R0)_low */
	vmovaps	%zmm11, %zmm0
	korw	%k0, %k1, %k0

	/* rel. error term: Eh=1-Sh*R0 */
	vmovaps	%zmm5, %zmm7
	vfmsub213pd {rn-sae}, %zmm9, %zmm4, %zmm0
	vfnmadd231pd {rn-sae}, %zmm9, %zmm4, %zmm7

	/* rel. error term: Eh=(1-Sh*R0)-Sl*R0 */
	vfnmadd231pd {rn-sae}, %zmm0, %zmm4, %zmm7

	/* Shl */
	vsubpd	{rn-sae}, %zmm14, %zmm9, %zmm4
	vmovups	poly_coeff7+__svml_dacosh_data_internal_avx512(%rip), %zmm14
	vfmadd231pd {rn-sae}, %zmm7, %zmm1, %zmm13
	vfmadd213pd {rn-sae}, %zmm2, %zmm7, %zmm13
	vfmadd213pd {rn-sae}, %zmm6, %zmm7, %zmm13

	/* Sh*Eh */
	vmulpd	{rn-sae}, %zmm7, %zmm9, %zmm7

	/* Sl + Sh*Eh*poly_s */
	vfmadd213pd {rn-sae}, %zmm0, %zmm13, %zmm7

	/* polynomials */
	vmovups	poly_coeff9+__svml_dacosh_data_internal_avx512(%rip), %zmm13

	/* polynomial computation for small inputs */
	vaddpd	{rn-sae}, %zmm7, %zmm9, %zmm0

	/* Xin0+Sl+Sh*Eh*poly_s ~ x+sqrt(1+x^2) */
	vaddpd	{rn-sae}, %zmm7, %zmm15, %zmm6
	vfmadd231pd {rn-sae}, %zmm0, %zmm8, %zmm0

	/* fixup for very large inputs */
	vmovups	OneEighth+__svml_dacosh_data_internal_avx512(%rip), %zmm8

	/* Sl_high */
	vsubpd	{rn-sae}, %zmm15, %zmm6, %zmm9
	vmovups	poly_coeff6+__svml_dacosh_data_internal_avx512(%rip), %zmm15
	vmulpd	{rn-sae}, %zmm8, %zmm12, %zmm6{%k2}

	/* Sl_l */
	vsubpd	{rn-sae}, %zmm9, %zmm7, %zmm3
	vrcp14pd %zmm6, %zmm1

	/* Xin_low */
	vaddpd	{rn-sae}, %zmm4, %zmm3, %zmm7

	/* Table lookups */
	vmovups	__svml_dacosh_data_internal_avx512(%rip), %zmm3

	/* round reciprocal to 1+4b mantissas */
	vpaddq	AddB5+__svml_dacosh_data_internal_avx512(%rip), %zmm1, %zmm2

	/* fixup for very large inputs */
	vxorpd	%zmm7, %zmm7, %zmm7{%k2}
	vmovups	poly_coeff8+__svml_dacosh_data_internal_avx512(%rip), %zmm1
	vandpd	RcpBitMask+__svml_dacosh_data_internal_avx512(%rip), %zmm2, %zmm8
	vmovups	Log_tbl_L+__svml_dacosh_data_internal_avx512(%rip), %zmm2

	/* Prepare table index */
	vpsrlq	$48, %zmm8, %zmm9

	/* reduced argument for log(): (Rcp*Xin-1)+Rcp*Xin_low */
	vfmsub231pd {rn-sae}, %zmm8, %zmm6, %zmm5

	/* exponents */
	vgetexppd {sae}, %zmm8, %zmm4
	vmovups	Four+__svml_dacosh_data_internal_avx512(%rip), %zmm6
	vpermt2pd Log_tbl_H+64+__svml_dacosh_data_internal_avx512(%rip), %zmm9, %zmm3
	vpermt2pd Log_tbl_L+64+__svml_dacosh_data_internal_avx512(%rip), %zmm9, %zmm2
	vsubpd	{rn-sae}, %zmm6, %zmm4, %zmm4{%k2}
	vfmadd231pd {rn-sae}, %zmm8, %zmm7, %zmm5
	vmovups	poly_coeff5+__svml_dacosh_data_internal_avx512(%rip), %zmm6
	vmovups	poly_coeff4+__svml_dacosh_data_internal_avx512(%rip), %zmm7

	/* -K*L2H + Th */
	vmovups	L2H+__svml_dacosh_data_internal_avx512(%rip), %zmm8

	/* -K*L2L + Tl */
	vmovups	L2L+__svml_dacosh_data_internal_avx512(%rip), %zmm9
	vfmadd231pd {rn-sae}, %zmm5, %zmm13, %zmm1
	vmovups	poly_coeff2+__svml_dacosh_data_internal_avx512(%rip), %zmm13
	vfnmadd231pd {rn-sae}, %zmm4, %zmm8, %zmm3
	vfnmadd213pd {rn-sae}, %zmm2, %zmm9, %zmm4
	vfmadd213pd {rn-sae}, %zmm14, %zmm5, %zmm1
	vmovups	poly_coeff3+__svml_dacosh_data_internal_avx512(%rip), %zmm2
	vmovups	poly_coeff1+__svml_dacosh_data_internal_avx512(%rip), %zmm14
	vfmadd213pd {rn-sae}, %zmm15, %zmm5, %zmm1

	/* R^2 */
	vmulpd	{rn-sae}, %zmm5, %zmm5, %zmm15
	vfmadd213pd {rn-sae}, %zmm6, %zmm5, %zmm1
	vfmadd213pd {rn-sae}, %zmm7, %zmm5, %zmm1
	vfmadd213pd {rn-sae}, %zmm2, %zmm5, %zmm1
	vfmadd213pd {rn-sae}, %zmm13, %zmm5, %zmm1
	vfmadd213pd {rn-sae}, %zmm14, %zmm5, %zmm1

	/* Tl + R^2*Poly */
	vfmadd213pd {rn-sae}, %zmm4, %zmm15, %zmm1

	/* R+Tl + R^2*Poly */
	vaddpd	{rn-sae}, %zmm5, %zmm1, %zmm5
	vaddpd	{rn-sae}, %zmm5, %zmm3, %zmm0{%k3}

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 k0 zmm0 zmm12

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
	vmovups	%zmm12, 64(%rsp)
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
	call	acosh@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_acosh_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dacosh_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Log_tbl_H[16][2];
	__declspec(align(64)) VUINT32 Log_tbl_L[16][2];
	__declspec(align(64)) VUINT32 One[8][2];
	__declspec(align(64)) VUINT32 SmallThreshold[8][2];
	__declspec(align(64)) VUINT32 Threshold[8][2];
	__declspec(align(64)) VUINT32 LargeThreshold[8][2];
	__declspec(align(64)) VUINT32 ca2[8][2];
	__declspec(align(64)) VUINT32 ca1[8][2];
	__declspec(align(64)) VUINT32 c4s[8][2];
	__declspec(align(64)) VUINT32 c3s[8][2];
	__declspec(align(64)) VUINT32 c2s[8][2];
	__declspec(align(64)) VUINT32 c1s[8][2];
	__declspec(align(64)) VUINT32 AddB5[8][2];
	__declspec(align(64)) VUINT32 RcpBitMask[8][2];
	__declspec(align(64)) VUINT32 OneEighth[8][2];
	__declspec(align(64)) VUINT32 Four[8][2];
	__declspec(align(64)) VUINT32 poly_coeff9[8][2];
	__declspec(align(64)) VUINT32 poly_coeff8[8][2];
	__declspec(align(64)) VUINT32 poly_coeff7[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 poly_coeff1[8][2];
	__declspec(align(64)) VUINT32 L2H[8][2];
	__declspec(align(64)) VUINT32 L2L[8][2];
} __svml_dacosh_data_internal_avx512;
#endif
__svml_dacosh_data_internal_avx512:
	/* Log_tbl_H */
	.quad	0x0000000000000000
	.quad	0xbfaf0a30c0120000
	.quad	0xbfbe27076e2b0000
	.quad	0xbfc5ff3070a78000
	.quad	0xbfcc8ff7c79a8000
	.quad	0xbfd1675cababc000
	.quad	0xbfd4618bc21c4000
	.quad	0xbfd739d7f6bbc000
	.quad	0xbfd9f323ecbf8000
	.quad	0xbfdc8ff7c79a8000
	.quad	0xbfdf128f5faf0000
	.quad	0xbfe0be72e4252000
	.quad	0xbfe1e85f5e704000
	.quad	0xbfe307d7334f2000
	.quad	0xbfe41d8fe8468000
	.quad	0xbfe52a2d265bc000
	/* Log_tbl_L */
	.align	64
	.quad	0x0000000000000000
	.quad	0x3d53ab33d066d1d2
	.quad	0x3d2a342c2af0003c
	.quad	0xbd43d3c873e20a07
	.quad	0xbd4a21ac25d81ef3
	.quad	0x3d59f1fc63382a8f
	.quad	0xbd5ec27d0b7b37b3
	.quad	0xbd50069ce24c53fb
	.quad	0xbd584bf2b68d766f
	.quad	0xbd5a21ac25d81ef3
	.quad	0xbd3bb2cd720ec44c
	.quad	0xbd55056d312f7668
	.quad	0xbd1a07bd8b34be7c
	.quad	0x3d5e83c094debc15
	.quad	0x3d5aa33736867a17
	.quad	0xbd46abb9df22bc57
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* SmallThreshold */
	.align	64
	.quad	0x3ef0000000000000, 0x3ef0000000000000, 0x3ef0000000000000, 0x3ef0000000000000, 0x3ef0000000000000, 0x3ef0000000000000, 0x3ef0000000000000, 0x3ef0000000000000
	/* Threshold */
	.align	64
	.quad	0x5fe0000000000000, 0x5fe0000000000000, 0x5fe0000000000000, 0x5fe0000000000000, 0x5fe0000000000000, 0x5fe0000000000000, 0x5fe0000000000000, 0x5fe0000000000000
	/* LargeThreshold */
	.align	64
	.quad	0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff
	/* ca2 */
	.align	64
	.quad	0x3fb333220eaf02e7, 0x3fb333220eaf02e7, 0x3fb333220eaf02e7, 0x3fb333220eaf02e7, 0x3fb333220eaf02e7, 0x3fb333220eaf02e7, 0x3fb333220eaf02e7, 0x3fb333220eaf02e7
	/* ca1 */
	.align	64
	.quad	0xbfc5555555521e7e, 0xbfc5555555521e7e, 0xbfc5555555521e7e, 0xbfc5555555521e7e, 0xbfc5555555521e7e, 0xbfc5555555521e7e, 0xbfc5555555521e7e, 0xbfc5555555521e7e
	/* c4s */
	.align	64
	.quad	0x3fd1800001943612, 0x3fd1800001943612, 0x3fd1800001943612, 0x3fd1800001943612, 0x3fd1800001943612, 0x3fd1800001943612, 0x3fd1800001943612, 0x3fd1800001943612
	/* c3s */
	.align	64
	.quad	0x3fd40000013b0000, 0x3fd40000013b0000, 0x3fd40000013b0000, 0x3fd40000013b0000, 0x3fd40000013b0000, 0x3fd40000013b0000, 0x3fd40000013b0000, 0x3fd40000013b0000
	/* c2s */
	.align	64
	.quad	0x3fd8000000000000, 0x3fd8000000000000, 0x3fd8000000000000, 0x3fd8000000000000, 0x3fd8000000000000, 0x3fd8000000000000, 0x3fd8000000000000, 0x3fd8000000000000
	/* c1s */
	.align	64
	.quad	0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000
	/* AddB5 */
	.align	64
	.quad	0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000
	/* RcpBitMask */
	.align	64
	.quad	0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000
	/* OneEighth */
	.align	64
	.quad	0x3fc0000000000000, 0x3fc0000000000000, 0x3fc0000000000000, 0x3fc0000000000000, 0x3fc0000000000000, 0x3fc0000000000000, 0x3fc0000000000000, 0x3fc0000000000000
	/* Four */
	.align	64
	.quad	0x4010000000000000, 0x4010000000000000, 0x4010000000000000, 0x4010000000000000, 0x4010000000000000, 0x4010000000000000, 0x4010000000000000, 0x4010000000000000
	/* poly_coeff9 */
	.align	64
	.quad	0xbfb9a9b040214368, 0xbfb9a9b040214368, 0xbfb9a9b040214368, 0xbfb9a9b040214368, 0xbfb9a9b040214368, 0xbfb9a9b040214368, 0xbfb9a9b040214368, 0xbfb9a9b040214368
	/* poly_coeff8 */
	.align	64
	.quad	0x3fbc80666e249778, 0x3fbc80666e249778, 0x3fbc80666e249778, 0x3fbc80666e249778, 0x3fbc80666e249778, 0x3fbc80666e249778, 0x3fbc80666e249778, 0x3fbc80666e249778
	/* poly_coeff7 */
	.align	64
	.quad	0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9, 0xbfbffffb8a054bc9
	/* poly_coeff6 */
	.align	64
	.quad	0x3fc24922f71256f1, 0x3fc24922f71256f1, 0x3fc24922f71256f1, 0x3fc24922f71256f1, 0x3fc24922f71256f1, 0x3fc24922f71256f1, 0x3fc24922f71256f1, 0x3fc24922f71256f1
	/* poly_coeff5 */
	.align	64
	.quad	0xbfc55555559ba736, 0xbfc55555559ba736, 0xbfc55555559ba736, 0xbfc55555559ba736, 0xbfc55555559ba736, 0xbfc55555559ba736, 0xbfc55555559ba736, 0xbfc55555559ba736
	/* poly_coeff4 */
	.align	64
	.quad	0x3fc9999999be77af, 0x3fc9999999be77af, 0x3fc9999999be77af, 0x3fc9999999be77af, 0x3fc9999999be77af, 0x3fc9999999be77af, 0x3fc9999999be77af, 0x3fc9999999be77af
	/* poly_coeff3 */
	.align	64
	.quad	0xbfcffffffffffc65, 0xbfcffffffffffc65, 0xbfcffffffffffc65, 0xbfcffffffffffc65, 0xbfcffffffffffc65, 0xbfcffffffffffc65, 0xbfcffffffffffc65, 0xbfcffffffffffc65
	/* poly_coeff2 */
	.align	64
	.quad	0x3fd55555555554c1, 0x3fd55555555554c1, 0x3fd55555555554c1, 0x3fd55555555554c1, 0x3fd55555555554c1, 0x3fd55555555554c1, 0x3fd55555555554c1, 0x3fd55555555554c1
	/* poly_coeff1 */
	.align	64
	.quad	0xbfe0000000000000, 0xbfe0000000000000, 0xbfe0000000000000, 0xbfe0000000000000, 0xbfe0000000000000, 0xbfe0000000000000, 0xbfe0000000000000, 0xbfe0000000000000
	/* L2H = log(2)_high */
	.align	64
	.quad	0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000
	/* L2L = log(2)_low */
	.align	64
	.quad	0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000
	.align	64
	.type	__svml_dacosh_data_internal_avx512, @object
	.size	__svml_dacosh_data_internal_avx512, .-__svml_dacosh_data_internal_avx512
