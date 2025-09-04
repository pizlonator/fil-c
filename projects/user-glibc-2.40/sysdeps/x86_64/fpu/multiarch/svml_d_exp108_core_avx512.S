/* Function exp10 vectorized with AVX-512.
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
 *   Typical exp10() implementation, except that:
 *    - tables are small (16 elements), allowing for fast gathers
 *    - all arguments processed in the main path
 *        - final VSCALEF assists branch-free design (correct overflow/underflow and special case responses)
 *        - a VAND is used to ensure the reduced argument |R|<2, even for large inputs
 *        - RZ mode used to avoid overflow to +/-Inf for x*log2(e); helps with special case handling
 *        - SAE used to avoid spurious flag settings
 *
 */

/* Offsets for data table __svml_dexp10_data_internal_avx512
 */
#define Exp_tbl_H			0
#define L2E				128
#define Shifter				192
#define L2H				256
#define L2L				320
#define EMask				384
#define poly_coeff6			448
#define poly_coeff5			512
#define poly_coeff4			576
#define poly_coeff3			640
#define poly_coeff2			704
#define poly_coeff1			768
#define AbsMask				832
#define Threshold			896

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_exp10_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	L2E+__svml_dexp10_data_internal_avx512(%rip), %zmm4
	vmovups	Shifter+__svml_dexp10_data_internal_avx512(%rip), %zmm2
	vmovups	L2H+__svml_dexp10_data_internal_avx512(%rip), %zmm5
	vmovups	L2L+__svml_dexp10_data_internal_avx512(%rip), %zmm3

	/* polynomial */
	vmovups	poly_coeff6+__svml_dexp10_data_internal_avx512(%rip), %zmm6
	vmovups	poly_coeff4+__svml_dexp10_data_internal_avx512(%rip), %zmm7
	vmovups	poly_coeff3+__svml_dexp10_data_internal_avx512(%rip), %zmm9
	vmovups	poly_coeff2+__svml_dexp10_data_internal_avx512(%rip), %zmm8
	vmovups	poly_coeff1+__svml_dexp10_data_internal_avx512(%rip), %zmm11
	vmovups	Threshold+__svml_dexp10_data_internal_avx512(%rip), %zmm14
	vmovaps	%zmm0, %zmm1

	/* 2^(52-4)*1.5 + x * log2(e) */
	vfmadd213pd {rz-sae}, %zmm2, %zmm1, %zmm4
	vandpd	AbsMask+__svml_dexp10_data_internal_avx512(%rip), %zmm1, %zmm13

	/* Z0 ~ x*log2(e), rounded down to 4 fractional bits */
	vsubpd	{rn-sae}, %zmm2, %zmm4, %zmm0

	/* Table lookup: Th */
	vmovups	__svml_dexp10_data_internal_avx512(%rip), %zmm2
	vcmppd	$29, {sae}, %zmm14, %zmm13, %k0

	/* R = x - Z0*log(2) */
	vfnmadd213pd {rn-sae}, %zmm1, %zmm0, %zmm5
	vpermt2pd Exp_tbl_H+64+__svml_dexp10_data_internal_avx512(%rip), %zmm4, %zmm2
	kmovw	%k0, %edx
	vfnmadd231pd {rn-sae}, %zmm0, %zmm3, %zmm5
	vmovups	poly_coeff5+__svml_dexp10_data_internal_avx512(%rip), %zmm3

	/* ensure |R|<2 even for special cases */
	vandpd	EMask+__svml_dexp10_data_internal_avx512(%rip), %zmm5, %zmm12
	vmulpd	{rn-sae}, %zmm12, %zmm12, %zmm10
	vmulpd	{rn-sae}, %zmm12, %zmm2, %zmm15
	vfmadd231pd {rn-sae}, %zmm12, %zmm6, %zmm3
	vfmadd231pd {rn-sae}, %zmm12, %zmm7, %zmm9
	vfmadd231pd {rn-sae}, %zmm12, %zmm8, %zmm11
	vfmadd213pd {rn-sae}, %zmm9, %zmm10, %zmm3
	vfmadd213pd {rn-sae}, %zmm11, %zmm10, %zmm3
	vfmadd213pd {rn-sae}, %zmm2, %zmm15, %zmm3
	vscalefpd {rn-sae}, %zmm0, %zmm3, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1

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
	vmovups	%zmm1, 64(%rsp)
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
	call	exp10@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_exp10_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dexp10_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Exp_tbl_H[16][2];
	__declspec(align(64)) VUINT32 L2E[8][2];
	__declspec(align(64)) VUINT32 Shifter[8][2];
	__declspec(align(64)) VUINT32 L2H[8][2];
	__declspec(align(64)) VUINT32 L2L[8][2];
	__declspec(align(64)) VUINT32 EMask[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 poly_coeff1[8][2];
	__declspec(align(64)) VUINT32 AbsMask[8][2];
	__declspec(align(64)) VUINT32 Threshold[8][2];
} __svml_dexp10_data_internal_avx512;
#endif
__svml_dexp10_data_internal_avx512:
	/* Exp_tbl_H */
	.quad	0x3ff0000000000000
	.quad	0x3ff0b5586cf9890f
	.quad	0x3ff172b83c7d517b
	.quad	0x3ff2387a6e756238
	.quad	0x3ff306fe0a31b715
	.quad	0x3ff3dea64c123422
	.quad	0x3ff4bfdad5362a27
	.quad	0x3ff5ab07dd485429
	.quad	0x3ff6a09e667f3bcd
	.quad	0x3ff7a11473eb0187
	.quad	0x3ff8ace5422aa0db
	.quad	0x3ff9c49182a3f090
	.quad	0x3ffae89f995ad3ad
	.quad	0x3ffc199bdd85529c
	.quad	0x3ffd5818dcfba487
	.quad	0x3ffea4afa2a490da
	/* log2(e) */
	.align	64
	.quad	0x400A934F0979A371, 0x400A934F0979A371, 0x400A934F0979A371, 0x400A934F0979A371, 0x400A934F0979A371, 0x400A934F0979A371, 0x400A934F0979A371, 0x400A934F0979A371
	/* Shifter=2^(52-4)*1.5 */
	.align	64
	.quad	0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0
	/* L2H = log(2)_high */
	.align	64
	.quad	0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff, 0x3fd34413509f79ff
	/* L2L = log(2)_low */
	.align	64
	.quad	0xbc49dc1da994fd21, 0xbc49dc1da994fd21, 0xbc49dc1da994fd21, 0xbc49dc1da994fd21, 0xbc49dc1da994fd21, 0xbc49dc1da994fd21, 0xbc49dc1da994fd21, 0xbc49dc1da994fd21
	/* EMask */
	.align	64
	.quad	0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff
	/* poly_coeff6 */
	.align	64
	.quad	0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020, 0x3fcb137ed8ac2020
	/* poly_coeff5 */
	.align	64
	.quad	0x3fe141a8e24f9424, 0x3fe141a8e24f9424, 0x3fe141a8e24f9424, 0x3fe141a8e24f9424, 0x3fe141a8e24f9424, 0x3fe141a8e24f9424, 0x3fe141a8e24f9424, 0x3fe141a8e24f9424
	/* poly_coeff4 */
	.align	64
	.quad	0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d, 0x3ff2bd77a0926c9d
	/* poly_coeff3 */
	.align	64
	.quad	0x40004705908704c8, 0x40004705908704c8, 0x40004705908704c8, 0x40004705908704c8, 0x40004705908704c8, 0x40004705908704c8, 0x40004705908704c8, 0x40004705908704c8
	/* poly_coeff2 */
	.align	64
	.quad	0x40053524c73dfe25, 0x40053524c73dfe25, 0x40053524c73dfe25, 0x40053524c73dfe25, 0x40053524c73dfe25, 0x40053524c73dfe25, 0x40053524c73dfe25, 0x40053524c73dfe25
	/* poly_coeff1 */
	.align	64
	.quad	0x40026bb1bbb554c2, 0x40026bb1bbb554c2, 0x40026bb1bbb554c2, 0x40026bb1bbb554c2, 0x40026bb1bbb554c2, 0x40026bb1bbb554c2, 0x40026bb1bbb554c2, 0x40026bb1bbb554c2
	/* AbsMask */
	.align	64
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff
	/* Threshold */
	.align	64
	.quad	0x40733A7146F72A41, 0x40733A7146F72A41, 0x40733A7146F72A41, 0x40733A7146F72A41, 0x40733A7146F72A41, 0x40733A7146F72A41, 0x40733A7146F72A41, 0x40733A7146F72A41
	.align	64
	.type	__svml_dexp10_data_internal_avx512, @object
	.size	__svml_dexp10_data_internal_avx512, .-__svml_dexp10_data_internal_avx512
