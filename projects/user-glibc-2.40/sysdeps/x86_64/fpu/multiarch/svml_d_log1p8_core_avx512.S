/* Function log1p vectorized with AVX-512.
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
 *    1+x = 2^k*(xh + xl) is computed in high-low parts; xh in [1, 2)
 *    Get short reciprocal approximation Rcp ~ 1/xh
 *    R = (Rcp*xh - 1.0) + Rcp*xl
 *    log1p(x) = k*log(2.0) - log(Rcp) + poly(R)
 *       log(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_dlog1p_data_internal_avx512
 */
#define Log_tbl				0
#define One				128
#define SgnMask				192
#define C075				256
#define poly_coeff9			320
#define poly_coeff8			384
#define poly_coeff7			448
#define poly_coeff6			512
#define poly_coeff5			576
#define poly_coeff4			640
#define poly_coeff3			704
#define poly_coeff2			768
#define L2				832

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_log1p_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	One+__svml_dlog1p_data_internal_avx512(%rip), %zmm7
	vmovups	SgnMask+__svml_dlog1p_data_internal_avx512(%rip), %zmm14
	vmovaps	%zmm0, %zmm9
	vaddpd	{rn-sae}, %zmm9, %zmm7, %zmm11
	vandpd	%zmm14, %zmm9, %zmm8

	/* compute 1+x as high, low parts */
	vmaxpd	{sae}, %zmm9, %zmm7, %zmm10
	vminpd	{sae}, %zmm9, %zmm7, %zmm12

	/* GetMant(x), normalized to [1, 2) for x>=0, NaN for x<0 */
	vgetmantpd $8, {sae}, %zmm11, %zmm6

	/* GetExp(x) */
	vgetexppd {sae}, %zmm11, %zmm5
	vsubpd	{rn-sae}, %zmm10, %zmm11, %zmm13

	/* DblRcp ~ 1/Mantissa */
	vrcp14pd %zmm6, %zmm15

	/* Start polynomial evaluation */
	vmovups	poly_coeff9+__svml_dlog1p_data_internal_avx512(%rip), %zmm10
	vmovups	poly_coeff7+__svml_dlog1p_data_internal_avx512(%rip), %zmm11

	/* Xl */
	vsubpd	{rn-sae}, %zmm13, %zmm12, %zmm2
	vxorpd	%zmm14, %zmm5, %zmm3

	/* round DblRcp to 4 fractional bits (RN mode, no Precision exception) */
	vrndscalepd $88, {sae}, %zmm15, %zmm4
	vmovups	poly_coeff5+__svml_dlog1p_data_internal_avx512(%rip), %zmm12
	vmovups	poly_coeff6+__svml_dlog1p_data_internal_avx512(%rip), %zmm14
	vmovups	poly_coeff3+__svml_dlog1p_data_internal_avx512(%rip), %zmm13

	/* Xl*2^(-Expon) */
	vscalefpd {rn-sae}, %zmm3, %zmm2, %zmm1

	/* Reduced argument: R = DblRcp*(Mantissa+Xl) - 1 */
	vfmsub213pd {rn-sae}, %zmm7, %zmm4, %zmm6
	vmovups	__svml_dlog1p_data_internal_avx512(%rip), %zmm3

	/*
	 * Table lookup
	 * Prepare exponent correction: DblRcp<0.75?
	 */
	vmovups	C075+__svml_dlog1p_data_internal_avx512(%rip), %zmm2

	/* Prepare table index */
	vpsrlq	$48, %zmm4, %zmm0
	vfmadd231pd {rn-sae}, %zmm4, %zmm1, %zmm6
	vmovups	poly_coeff8+__svml_dlog1p_data_internal_avx512(%rip), %zmm1
	vcmppd	$17, {sae}, %zmm2, %zmm4, %k1
	vcmppd	$4, {sae}, %zmm6, %zmm6, %k0
	vfmadd231pd {rn-sae}, %zmm6, %zmm10, %zmm1
	vmovups	poly_coeff4+__svml_dlog1p_data_internal_avx512(%rip), %zmm10
	vfmadd231pd {rn-sae}, %zmm6, %zmm11, %zmm14
	vmovups	L2+__svml_dlog1p_data_internal_avx512(%rip), %zmm4
	vpermt2pd Log_tbl+64+__svml_dlog1p_data_internal_avx512(%rip), %zmm0, %zmm3

	/* add 1 to Expon if DblRcp<0.75 */
	vaddpd	{rn-sae}, %zmm7, %zmm5, %zmm5{%k1}

	/* R^2 */
	vmulpd	{rn-sae}, %zmm6, %zmm6, %zmm0
	vfmadd231pd {rn-sae}, %zmm6, %zmm12, %zmm10
	vmovups	poly_coeff2+__svml_dlog1p_data_internal_avx512(%rip), %zmm12
	vmulpd	{rn-sae}, %zmm0, %zmm0, %zmm15
	vfmadd231pd {rn-sae}, %zmm6, %zmm13, %zmm12
	vfmadd213pd {rn-sae}, %zmm14, %zmm0, %zmm1
	kmovw	%k0, %edx
	vfmadd213pd {rn-sae}, %zmm12, %zmm0, %zmm10

	/* polynomial */
	vfmadd213pd {rn-sae}, %zmm10, %zmm15, %zmm1
	vfmadd213pd {rn-sae}, %zmm6, %zmm0, %zmm1
	vaddpd	{rn-sae}, %zmm1, %zmm3, %zmm6
	vfmadd213pd {rn-sae}, %zmm6, %zmm4, %zmm5
	vorpd	%zmm8, %zmm5, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm9

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
	vmovups	%zmm9, 64(%rsp)
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
	call	log1p@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_log1p_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dlog1p_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Log_tbl[16][2];
	__declspec(align(64)) VUINT32 One[8][2];
	__declspec(align(64)) VUINT32 SgnMask[8][2];
	__declspec(align(64)) VUINT32 C075[8][2];
	__declspec(align(64)) VUINT32 poly_coeff9[8][2];
	__declspec(align(64)) VUINT32 poly_coeff8[8][2];
	__declspec(align(64)) VUINT32 poly_coeff7[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 L2[8][2];
} __svml_dlog1p_data_internal_avx512;
#endif
__svml_dlog1p_data_internal_avx512:
	/* Log_tbl */
	.quad	0x0000000000000000
	.quad	0xbfaf0a30c01162a6
	.quad	0xbfbe27076e2af2e6
	.quad	0xbfc5ff3070a793d4
	.quad	0xbfcc8ff7c79a9a22
	.quad	0xbfd1675cababa60e
	.quad	0xbfd4618bc21c5ec2
	.quad	0xbfd739d7f6bbd007
	.quad	0x3fd269621134db92
	.quad	0x3fcf991c6cb3b379
	.quad	0x3fca93ed3c8ad9e3
	.quad	0x3fc5bf406b543db2
	.quad	0x3fc1178e8227e47c
	.quad	0x3fb9335e5d594989
	.quad	0x3fb08598b59e3a07
	.quad	0x3fa0415d89e74444
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* SgnMask */
	.align	64
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000
	/* C075 0.75 */
	.align	64
	.quad	0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000
	/* poly_coeff9 */
	.align	64
	.quad	0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70, 0x3fbC81CD309D7C70
	/* poly_coeff8 */
	.align	64
	.quad	0xbfc007357E93AF62, 0xbfc007357E93AF62, 0xbfc007357E93AF62, 0xbfc007357E93AF62, 0xbfc007357E93AF62, 0xbfc007357E93AF62, 0xbfc007357E93AF62, 0xbfc007357E93AF62
	/* poly_coeff7 */
	.align	64
	.quad	0x3fc249229CEE81EF, 0x3fc249229CEE81EF, 0x3fc249229CEE81EF, 0x3fc249229CEE81EF, 0x3fc249229CEE81EF, 0x3fc249229CEE81EF, 0x3fc249229CEE81EF, 0x3fc249229CEE81EF
	/* poly_coeff6 */
	.align	64
	.quad	0xbfc55553FB28DB06, 0xbfc55553FB28DB06, 0xbfc55553FB28DB06, 0xbfc55553FB28DB06, 0xbfc55553FB28DB06, 0xbfc55553FB28DB06, 0xbfc55553FB28DB06, 0xbfc55553FB28DB06
	/* poly_coeff5 */
	.align	64
	.quad	0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C, 0x3fc9999999CC9F5C
	/* poly_coeff4 */
	.align	64
	.quad	0xbfd00000000C05BD, 0xbfd00000000C05BD, 0xbfd00000000C05BD, 0xbfd00000000C05BD, 0xbfd00000000C05BD, 0xbfd00000000C05BD, 0xbfd00000000C05BD, 0xbfd00000000C05BD
	/* poly_coeff3 */
	.align	64
	.quad	0x3fd5555555555466, 0x3fd5555555555466, 0x3fd5555555555466, 0x3fd5555555555466, 0x3fd5555555555466, 0x3fd5555555555466, 0x3fd5555555555466, 0x3fd5555555555466
	/* poly_coeff2 */
	.align	64
	.quad	0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6, 0xbfdFFFFFFFFFFFC6
	/* L2 = log(2) */
	.align	64
	.quad	0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF, 0x3fe62E42FEFA39EF
	.align	64
	.type	__svml_dlog1p_data_internal_avx512, @object
	.size	__svml_dlog1p_data_internal_avx512, .-__svml_dlog1p_data_internal_avx512
