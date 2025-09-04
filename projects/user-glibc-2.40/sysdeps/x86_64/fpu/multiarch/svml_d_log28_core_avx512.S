/* Function log2 vectorized with AVX-512.
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
 *    Get short reciprocal approximation Rcp ~ 1/mantissa(x)
 *    R = Rcp*x - 1.0
 *    log2(x) = k - log2(Rcp) + poly_approximation(R)
 *       log2(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_dlog2_data_internal_avx512
 */
#define Log_tbl				0
#define One				128
#define C075				192
#define poly_coeff9			256
#define poly_coeff8			320
#define poly_coeff7			384
#define poly_coeff6			448
#define poly_coeff5			512
#define poly_coeff4			576
#define poly_coeff3			640
#define poly_coeff2			704
#define poly_coeff1			768

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_log2_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovaps	%zmm0, %zmm7
	vgetmantpd $8, {sae}, %zmm7, %zmm6
	vmovups	One+__svml_dlog2_data_internal_avx512(%rip), %zmm2
	vmovups	poly_coeff5+__svml_dlog2_data_internal_avx512(%rip), %zmm12
	vmovups	poly_coeff3+__svml_dlog2_data_internal_avx512(%rip), %zmm13

	/* Start polynomial evaluation */
	vmovups	poly_coeff9+__svml_dlog2_data_internal_avx512(%rip), %zmm10
	vmovups	poly_coeff8+__svml_dlog2_data_internal_avx512(%rip), %zmm0
	vmovups	poly_coeff7+__svml_dlog2_data_internal_avx512(%rip), %zmm11
	vmovups	poly_coeff6+__svml_dlog2_data_internal_avx512(%rip), %zmm14

	/* Prepare exponent correction: DblRcp<0.75? */
	vmovups	C075+__svml_dlog2_data_internal_avx512(%rip), %zmm1

	/* Table lookup */
	vmovups	__svml_dlog2_data_internal_avx512(%rip), %zmm4

	/* GetExp(x) */
	vgetexppd {sae}, %zmm7, %zmm5

	/* DblRcp ~ 1/Mantissa */
	vrcp14pd %zmm6, %zmm8

	/* x<=0? */
	vfpclasspd $94, %zmm7, %k0

	/* round DblRcp to 4 fractional bits (RN mode, no Precision exception) */
	vrndscalepd $88, {sae}, %zmm8, %zmm3
	vmovups	poly_coeff4+__svml_dlog2_data_internal_avx512(%rip), %zmm8
	kmovw	%k0, %edx

	/* Reduced argument: R = DblRcp*Mantissa - 1 */
	vfmsub213pd {rn-sae}, %zmm2, %zmm3, %zmm6
	vcmppd	$17, {sae}, %zmm1, %zmm3, %k1
	vfmadd231pd {rn-sae}, %zmm6, %zmm12, %zmm8
	vmovups	poly_coeff2+__svml_dlog2_data_internal_avx512(%rip), %zmm12
	vfmadd231pd {rn-sae}, %zmm6, %zmm10, %zmm0
	vfmadd231pd {rn-sae}, %zmm6, %zmm11, %zmm14
	vmovups	poly_coeff1+__svml_dlog2_data_internal_avx512(%rip), %zmm1

	/* R^2 */
	vmulpd	{rn-sae}, %zmm6, %zmm6, %zmm15
	vfmadd231pd {rn-sae}, %zmm6, %zmm13, %zmm12

	/* Prepare table index */
	vpsrlq	$48, %zmm3, %zmm9

	/* add 1 to Expon if DblRcp<0.75 */
	vaddpd	{rn-sae}, %zmm2, %zmm5, %zmm5{%k1}
	vmulpd	{rn-sae}, %zmm15, %zmm15, %zmm13
	vfmadd213pd {rn-sae}, %zmm14, %zmm15, %zmm0
	vfmadd213pd {rn-sae}, %zmm12, %zmm15, %zmm8
	vpermt2pd Log_tbl+64+__svml_dlog2_data_internal_avx512(%rip), %zmm9, %zmm4

	/* polynomial */
	vfmadd213pd {rn-sae}, %zmm8, %zmm13, %zmm0
	vfmadd213pd {rn-sae}, %zmm1, %zmm6, %zmm0
	vfmadd213pd {rn-sae}, %zmm4, %zmm0, %zmm6
	vaddpd	{rn-sae}, %zmm6, %zmm5, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm7

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
	vmovups	%zmm7, 64(%rsp)
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
	call	log2@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_log2_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dlog2_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Log_tbl[16][2];
	__declspec(align(64)) VUINT32 One[8][2];
	__declspec(align(64)) VUINT32 C075[8][2];
	__declspec(align(64)) VUINT32 poly_coeff9[8][2];
	__declspec(align(64)) VUINT32 poly_coeff8[8][2];
	__declspec(align(64)) VUINT32 poly_coeff7[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 poly_coeff1[8][2];
} __svml_dlog2_data_internal_avx512;
#endif
__svml_dlog2_data_internal_avx512:
	/* Log_tbl */
	.quad	0x0000000000000000
	.quad	0xbfb663f6fac91316
	.quad	0xbfc5c01a39fbd688
	.quad	0xbfcfbc16b902680a
	.quad	0xbfd49a784bcd1b8b
	.quad	0xbfd91bba891f1709
	.quad	0xbfdd6753e032ea0f
	.quad	0xbfe0c10500d63aa6
	.quad	0x3fda8ff971810a5e
	.quad	0x3fd6cb0f6865c8ea
	.quad	0x3fd32bfee370ee68
	.quad	0x3fcf5fd8a9063e35
	.quad	0x3fc8a8980abfbd32
	.quad	0x3fc22dadc2ab3497
	.quad	0x3fb7d60496cfbb4c
	.quad	0x3fa77394c9d958d5
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* C075 0.75 */
	.align	64
	.quad	0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000
	/* poly_coeff9 */
	.align	64
	.quad	0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12, 0x3fc4904bda0e1d12
	/* poly_coeff8 */
	.align	64
	.quad	0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce, 0xbfc71fb84deb5cce
	/* poly_coeff7 */
	.align	64
	.quad	0x3fca617351818613, 0x3fca617351818613, 0x3fca617351818613, 0x3fca617351818613, 0x3fca617351818613, 0x3fca617351818613, 0x3fca617351818613, 0x3fca617351818613
	/* poly_coeff6 */
	.align	64
	.quad	0xbfcec707e4e3144c, 0xbfcec707e4e3144c, 0xbfcec707e4e3144c, 0xbfcec707e4e3144c, 0xbfcec707e4e3144c, 0xbfcec707e4e3144c, 0xbfcec707e4e3144c, 0xbfcec707e4e3144c
	/* poly_coeff5 */
	.align	64
	.quad	0x3fd2776c5114d91a, 0x3fd2776c5114d91a, 0x3fd2776c5114d91a, 0x3fd2776c5114d91a, 0x3fd2776c5114d91a, 0x3fd2776c5114d91a, 0x3fd2776c5114d91a, 0x3fd2776c5114d91a
	/* poly_coeff4 */
	.align	64
	.quad	0xbfd71547653d0f8d, 0xbfd71547653d0f8d, 0xbfd71547653d0f8d, 0xbfd71547653d0f8d, 0xbfd71547653d0f8d, 0xbfd71547653d0f8d, 0xbfd71547653d0f8d, 0xbfd71547653d0f8d
	/* poly_coeff3 */
	.align	64
	.quad	0x3fdec709dc3a029f, 0x3fdec709dc3a029f, 0x3fdec709dc3a029f, 0x3fdec709dc3a029f, 0x3fdec709dc3a029f, 0x3fdec709dc3a029f, 0x3fdec709dc3a029f, 0x3fdec709dc3a029f
	/* poly_coeff2 */
	.align	64
	.quad	0xbfe71547652b82d4, 0xbfe71547652b82d4, 0xbfe71547652b82d4, 0xbfe71547652b82d4, 0xbfe71547652b82d4, 0xbfe71547652b82d4, 0xbfe71547652b82d4, 0xbfe71547652b82d4
	/* poly_coeff1 */
	.align	64
	.quad	0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe, 0x3ff71547652b82fe
	.align	64
	.type	__svml_dlog2_data_internal_avx512, @object
	.size	__svml_dlog2_data_internal_avx512, .-__svml_dlog2_data_internal_avx512
