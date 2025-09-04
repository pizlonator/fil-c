/* Function log10 vectorized with AVX-512.
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
 *    log10(x) = k*log10(2.0) - log10(Rcp) + poly_approximation(R)
 *       log10(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_dlog10_data_internal_avx512
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
#define L2				832

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_log10_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovaps	%zmm0, %zmm7
	vgetmantpd $8, {sae}, %zmm7, %zmm6
	vmovups	One+__svml_dlog10_data_internal_avx512(%rip), %zmm3
	vmovups	poly_coeff5+__svml_dlog10_data_internal_avx512(%rip), %zmm12
	vmovups	poly_coeff3+__svml_dlog10_data_internal_avx512(%rip), %zmm13

	/* Start polynomial evaluation */
	vmovups	poly_coeff9+__svml_dlog10_data_internal_avx512(%rip), %zmm10
	vmovups	poly_coeff8+__svml_dlog10_data_internal_avx512(%rip), %zmm1
	vmovups	poly_coeff7+__svml_dlog10_data_internal_avx512(%rip), %zmm11
	vmovups	poly_coeff6+__svml_dlog10_data_internal_avx512(%rip), %zmm14

	/* Prepare exponent correction: DblRcp<0.75? */
	vmovups	C075+__svml_dlog10_data_internal_avx512(%rip), %zmm2

	/* Table lookup */
	vmovups	__svml_dlog10_data_internal_avx512(%rip), %zmm5

	/* GetExp(x) */
	vgetexppd {sae}, %zmm7, %zmm0

	/* DblRcp ~ 1/Mantissa */
	vrcp14pd %zmm6, %zmm8

	/* x<=0? */
	vfpclasspd $94, %zmm7, %k0

	/* round DblRcp to 4 fractional bits (RN mode, no Precision exception) */
	vrndscalepd $88, {sae}, %zmm8, %zmm4
	vmovups	poly_coeff4+__svml_dlog10_data_internal_avx512(%rip), %zmm8
	kmovw	%k0, %edx

	/* Reduced argument: R = DblRcp*Mantissa - 1 */
	vfmsub213pd {rn-sae}, %zmm3, %zmm4, %zmm6
	vcmppd	$17, {sae}, %zmm2, %zmm4, %k1
	vfmadd231pd {rn-sae}, %zmm6, %zmm12, %zmm8
	vmovups	poly_coeff2+__svml_dlog10_data_internal_avx512(%rip), %zmm12
	vfmadd231pd {rn-sae}, %zmm6, %zmm10, %zmm1
	vfmadd231pd {rn-sae}, %zmm6, %zmm11, %zmm14
	vmovups	poly_coeff1+__svml_dlog10_data_internal_avx512(%rip), %zmm2

	/* R^2 */
	vmulpd	{rn-sae}, %zmm6, %zmm6, %zmm15
	vfmadd231pd {rn-sae}, %zmm6, %zmm13, %zmm12

	/* Prepare table index */
	vpsrlq	$48, %zmm4, %zmm9

	/* add 1 to Expon if DblRcp<0.75 */
	vaddpd	{rn-sae}, %zmm3, %zmm0, %zmm0{%k1}
	vmulpd	{rn-sae}, %zmm15, %zmm15, %zmm13
	vfmadd213pd {rn-sae}, %zmm14, %zmm15, %zmm1
	vfmadd213pd {rn-sae}, %zmm12, %zmm15, %zmm8
	vpermt2pd Log_tbl+64+__svml_dlog10_data_internal_avx512(%rip), %zmm9, %zmm5

	/* polynomial */
	vfmadd213pd {rn-sae}, %zmm8, %zmm13, %zmm1
	vfmadd213pd {rn-sae}, %zmm2, %zmm6, %zmm1
	vfmadd213pd {rn-sae}, %zmm5, %zmm1, %zmm6
	vmovups	L2+__svml_dlog10_data_internal_avx512(%rip), %zmm1
	vfmadd213pd {rn-sae}, %zmm6, %zmm1, %zmm0
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
	call	log10@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_log10_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dlog10_data_internal_avx512_typedef
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
	__declspec(align(64)) VUINT32 L2[8][2];
} __svml_dlog10_data_internal_avx512;
#endif
__svml_dlog10_data_internal_avx512:
	/* Log_tbl */
	.quad	0x0000000000000000
	.quad	0xbf9af5f92b00e610
	.quad	0xbfaa30a9d609efea
	.quad	0xbfb31b3055c47118
	.quad	0xbfb8cf183886480d
	.quad	0xbfbe3bc1ab0e19fe
	.quad	0xbfc1b3e71ec94f7b
	.quad	0xbfc42c7e7fe3fc02
	.quad	0x3fbffbfc2bbc7803
	.quad	0x3fbb721cd17157e3
	.quad	0x3fb715d0ce367afc
	.quad	0x3fb2e3a740b7800f
	.quad	0x3fadb11ed766abf4
	.quad	0x3fa5e3966b7e9295
	.quad	0x3f9cb38fccd8bfdb
	.quad	0x3f8c3d0837784c41
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* 0.75 */
	.align	64
	.quad	0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000, 0x3fe8000000000000
	/* poly_coeff9 */
	.align	64
	.quad	0x3fa8c2d828480370, 0x3fa8c2d828480370, 0x3fa8c2d828480370, 0x3fa8c2d828480370, 0x3fa8c2d828480370, 0x3fa8c2d828480370, 0x3fa8c2d828480370, 0x3fa8c2d828480370
	/* poly_coeff8 */
	.align	64
	.quad	0xbfabd80d96029814, 0xbfabd80d96029814, 0xbfabd80d96029814, 0xbfabd80d96029814, 0xbfabd80d96029814, 0xbfabd80d96029814, 0xbfabd80d96029814, 0xbfabd80d96029814
	/* poly_coeff7 */
	.align	64
	.quad	0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2, 0x3fafc3f6f38b58a2
	/* poly_coeff6 */
	.align	64
	.quad	0xbfb287a63464dc80, 0xbfb287a63464dc80, 0xbfb287a63464dc80, 0xbfb287a63464dc80, 0xbfb287a63464dc80, 0xbfb287a63464dc80, 0xbfb287a63464dc80, 0xbfb287a63464dc80
	/* poly_coeff5 */
	.align	64
	.quad	0x3fb63c62777f27d9, 0x3fb63c62777f27d9, 0x3fb63c62777f27d9, 0x3fb63c62777f27d9, 0x3fb63c62777f27d9, 0x3fb63c62777f27d9, 0x3fb63c62777f27d9, 0x3fb63c62777f27d9
	/* poly_coeff4 */
	.align	64
	.quad	0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3, 0xbfbbcb7b153c06a3
	/* poly_coeff3 */
	.align	64
	.quad	0x3fc287a7636f428c, 0x3fc287a7636f428c, 0x3fc287a7636f428c, 0x3fc287a7636f428c, 0x3fc287a7636f428c, 0x3fc287a7636f428c, 0x3fc287a7636f428c, 0x3fc287a7636f428c
	/* poly_coeff2 */
	.align	64
	.quad	0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db, 0xbfcbcb7b1526e4db
	/* poly_coeff1 */
	.align	64
	.quad	0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e, 0x3fdbcb7b1526e50e
	/* L2 */
	.align	64
	.quad	0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff
	.align	64
	.type	__svml_dlog10_data_internal_avx512, @object
	.size	__svml_dlog10_data_internal_avx512, .-__svml_dlog10_data_internal_avx512
