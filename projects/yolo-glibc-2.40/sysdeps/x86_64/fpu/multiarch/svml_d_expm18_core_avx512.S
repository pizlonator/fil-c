/* Function expm1 vectorized with AVX-512.
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
 *   After computing exp(x) in high-low parts, an accurate computation is performed to obtain exp(x)-1
 *   Typical exp() implementation, except that:
 *    - tables are small (16 elements), allowing for fast gathers
 *    - all arguments processed in the main path
 *        - final VSCALEF assists branch-free design (correct overflow/underflow and special case responses)
 *        - a VAND is used to ensure the reduced argument |R|<2, even for large inputs
 *        - RZ mode used to avoid overflow to +/-Inf for x*log2(e); helps with special case handling
 *
 *
 */

/* Offsets for data table __svml_dexpm1_data_internal_avx512
 */
#define Exp_tbl_H			0
#define Exp_tbl_L			128
#define L2E				256
#define Shifter				320
#define Threshold			384
#define SgnMask				448
#define L2H				512
#define L2L				576
#define ZThres				640
#define EMask				704
#define poly_coeff7			768
#define poly_coeff6			832
#define poly_coeff5			896
#define poly_coeff4			960
#define poly_coeff3			1024
#define poly_coeff2			1088
#define One				1152

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_expm1_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	L2E+__svml_dexpm1_data_internal_avx512(%rip), %zmm6
	vmovups	Shifter+__svml_dexpm1_data_internal_avx512(%rip), %zmm4
	vmovups	L2H+__svml_dexpm1_data_internal_avx512(%rip), %zmm11
	vmovups	L2L+__svml_dexpm1_data_internal_avx512(%rip), %zmm5
	vmovups	Threshold+__svml_dexpm1_data_internal_avx512(%rip), %zmm3
	vmovups	poly_coeff5+__svml_dexpm1_data_internal_avx512(%rip), %zmm13
	vmovups	poly_coeff4+__svml_dexpm1_data_internal_avx512(%rip), %zmm15

	/* polynomial */
	vmovups	poly_coeff7+__svml_dexpm1_data_internal_avx512(%rip), %zmm12

	/* set Z0=max(Z0, -128.0) */
	vmovups	ZThres+__svml_dexpm1_data_internal_avx512(%rip), %zmm8
	vmovups	poly_coeff3+__svml_dexpm1_data_internal_avx512(%rip), %zmm14
	vmovups	__svml_dexpm1_data_internal_avx512(%rip), %zmm9
	vmovaps	%zmm0, %zmm2

	/* 2^(52-4)*1.5 + x * log2(e) */
	vfmadd213pd {rn-sae}, %zmm4, %zmm2, %zmm6
	vmovups	Exp_tbl_L+__svml_dexpm1_data_internal_avx512(%rip), %zmm0
	vcmppd	$21, {sae}, %zmm3, %zmm2, %k0

	/* Z0 ~ x*log2(e), rounded to 4 fractional bits */
	vsubpd	{rn-sae}, %zmm4, %zmm6, %zmm7
	vpermt2pd Exp_tbl_H+64+__svml_dexpm1_data_internal_avx512(%rip), %zmm6, %zmm9
	vpermt2pd Exp_tbl_L+64+__svml_dexpm1_data_internal_avx512(%rip), %zmm6, %zmm0
	vandpd	SgnMask+__svml_dexpm1_data_internal_avx512(%rip), %zmm2, %zmm1

	/* R = x - Z0*log(2) */
	vfnmadd213pd {rn-sae}, %zmm2, %zmm7, %zmm11
	vmaxpd	{sae}, %zmm8, %zmm7, %zmm10
	vfnmadd231pd {rn-sae}, %zmm7, %zmm5, %zmm11
	kmovw	%k0, %edx

	/* ensure |R|<2 even for special cases */
	vandpd	EMask+__svml_dexpm1_data_internal_avx512(%rip), %zmm11, %zmm3
	vmovups	poly_coeff6+__svml_dexpm1_data_internal_avx512(%rip), %zmm11

	/* scale Th */
	vscalefpd {rn-sae}, %zmm10, %zmm9, %zmm4
	vfmadd231pd {rn-sae}, %zmm3, %zmm13, %zmm15
	vfmadd231pd {rn-sae}, %zmm3, %zmm12, %zmm11
	vmovups	poly_coeff2+__svml_dexpm1_data_internal_avx512(%rip), %zmm12
	vmulpd	{rn-sae}, %zmm3, %zmm3, %zmm13
	vfmadd231pd {rn-sae}, %zmm3, %zmm14, %zmm12
	vfmadd213pd {rn-sae}, %zmm15, %zmm13, %zmm11
	vfmadd213pd {rn-sae}, %zmm12, %zmm13, %zmm11

	/* Tlr + R+ R*Poly */
	vfmadd213pd {rn-sae}, %zmm0, %zmm13, %zmm11

	/* Th - 1 */
	vmovups	One+__svml_dexpm1_data_internal_avx512(%rip), %zmm0
	vaddpd	{rn-sae}, %zmm3, %zmm11, %zmm14
	vsubpd	{rn-sae}, %zmm0, %zmm4, %zmm15

	/* (Th-1)+Th*(Tlr + R+ R*Poly) */
	vfmadd213pd {rn-sae}, %zmm15, %zmm14, %zmm4
	vorpd	%zmm1, %zmm4, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm2

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
	vmovups	%zmm2, 64(%rsp)
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
	call	expm1@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_expm1_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dexpm1_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Exp_tbl_H[16][2];
	__declspec(align(64)) VUINT32 Exp_tbl_L[16][2];
	__declspec(align(64)) VUINT32 L2E[8][2];
	__declspec(align(64)) VUINT32 Shifter[8][2];
	__declspec(align(64)) VUINT32 Threshold[8][2];
	__declspec(align(64)) VUINT32 SgnMask[8][2];
	__declspec(align(64)) VUINT32 L2H[8][2];
	__declspec(align(64)) VUINT32 L2L[8][2];
	__declspec(align(64)) VUINT32 ZThres[8][2];
	__declspec(align(64)) VUINT32 EMask[8][2];
	__declspec(align(64)) VUINT32 poly_coeff7[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 One[8][2];
} __svml_dexpm1_data_internal_avx512;
#endif
__svml_dexpm1_data_internal_avx512:
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
	/* Exp_tbl_L */
	.align	64
	.quad	0x0000000000000000
	.quad	0x3c979aa65d837b6d
	.quad	0xbc801b15eaa59348
	.quad	0x3c968efde3a8a894
	.quad	0x3c834d754db0abb6
	.quad	0x3c859f48a72a4c6d
	.quad	0x3c7690cebb7aafb0
	.quad	0x3c9063e1e21c5409
	.quad	0xbc93b3efbf5e2228
	.quad	0xbc7b32dcb94da51d
	.quad	0x3c8db72fc1f0eab4
	.quad	0x3c71affc2b91ce27
	.quad	0x3c8c1a7792cb3387
	.quad	0x3c736eae30af0cb3
	.quad	0x3c74a385a63d07a7
	.quad	0xbc8ff7128fd391f0
	/* log2(e) */
	.align	64
	.quad	0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE
	/* Shifter=2^(52-4)*1.5 */
	.align	64
	.quad	0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0, 0x42f8000000003ff0
	/* Threshold */
	.align	64
	.quad	0x40861DA04CBAFE44, 0x40861DA04CBAFE44, 0x40861DA04CBAFE44, 0x40861DA04CBAFE44, 0x40861DA04CBAFE44, 0x40861DA04CBAFE44, 0x40861DA04CBAFE44, 0x40861DA04CBAFE44
	/* Sgn */
	.align	64
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000
	/* L2H = log(2)_high */
	.align	64
	.quad	0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef, 0x3fe62e42fefa39ef
	/* L2L = log(2)_low */
	.align	64
	.quad	0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f, 0x3c7abc9e3b39803f
	/* ZThres */
	.align	64
	.quad	0xc060000000000000, 0xc060000000000000, 0xc060000000000000, 0xc060000000000000, 0xc060000000000000, 0xc060000000000000, 0xc060000000000000, 0xc060000000000000
	/* EMask */
	.align	64
	.quad	0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff, 0xbfffffffffffffff
	/* poly_coeff7 */
	.align	64
	.quad	0x3f2a020410303d8a, 0x3f2a020410303d8a, 0x3f2a020410303d8a, 0x3f2a020410303d8a, 0x3f2a020410303d8a, 0x3f2a020410303d8a, 0x3f2a020410303d8a, 0x3f2a020410303d8a
	/* poly_coeff6 */
	.align	64
	.quad	0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f, 0x3f56c1c38e164a2f
	/* poly_coeff5 */
	.align	64
	.quad	0x3f81111110865214, 0x3f81111110865214, 0x3f81111110865214, 0x3f81111110865214, 0x3f81111110865214, 0x3f81111110865214, 0x3f81111110865214, 0x3f81111110865214
	/* poly_coeff4 */
	.align	64
	.quad	0x3fa5555554ad3d06, 0x3fa5555554ad3d06, 0x3fa5555554ad3d06, 0x3fa5555554ad3d06, 0x3fa5555554ad3d06, 0x3fa5555554ad3d06, 0x3fa5555554ad3d06, 0x3fa5555554ad3d06
	/* poly_coeff3 */
	.align	64
	.quad	0x3fc5555555555656, 0x3fc5555555555656, 0x3fc5555555555656, 0x3fc5555555555656, 0x3fc5555555555656, 0x3fc5555555555656, 0x3fc5555555555656, 0x3fc5555555555656
	/* poly_coeff2 */
	.align	64
	.quad	0x3fe00000000000a2, 0x3fe00000000000a2, 0x3fe00000000000a2, 0x3fe00000000000a2, 0x3fe00000000000a2, 0x3fe00000000000a2, 0x3fe00000000000a2, 0x3fe00000000000a2
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	.align	64
	.type	__svml_dexpm1_data_internal_avx512, @object
	.size	__svml_dexpm1_data_internal_avx512, .-__svml_dexpm1_data_internal_avx512
