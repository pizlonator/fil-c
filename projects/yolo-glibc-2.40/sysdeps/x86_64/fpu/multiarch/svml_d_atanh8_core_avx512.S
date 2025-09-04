/* Function atanh vectorized with AVX-512.
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
 *   Compute atanh(x) as 0.5 * log((1 + x)/(1 - x))
 *   using small lookup table that map to AVX-512 permute instructions
 *
 *   Special cases:
 *
 *   atanh(0)  = 0
 *   atanh(+1) = +INF
 *   atanh(-1) = -INF
 *   atanh(x)  = NaN if |x| > 1, or if x is a NaN or INF
 *
 */

/* Offsets for data table __svml_datanh_data_internal_avx512
 */
#define Log_tbl_H			0
#define Log_tbl_L			128
#define One				256
#define AbsMask				320
#define AddB5				384
#define RcpBitMask			448
#define poly_coeff8			512
#define poly_coeff7			576
#define poly_coeff6			640
#define poly_coeff5			704
#define poly_coeff4			768
#define poly_coeff3			832
#define poly_coeff2			896
#define poly_coeff1			960
#define poly_coeff0			1024
#define Half				1088
#define L2H				1152
#define L2L				1216

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_atanh_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	One+__svml_datanh_data_internal_avx512(%rip), %zmm15

	/* round reciprocals to 1+4b mantissas */
	vmovups	AddB5+__svml_datanh_data_internal_avx512(%rip), %zmm6
	vmovups	RcpBitMask+__svml_datanh_data_internal_avx512(%rip), %zmm9
	vmovaps	%zmm0, %zmm2
	vandpd	AbsMask+__svml_datanh_data_internal_avx512(%rip), %zmm2, %zmm13

	/* 1+y */
	vaddpd	{rn-sae}, %zmm15, %zmm13, %zmm0

	/* 1-y */
	vsubpd	{rn-sae}, %zmm13, %zmm15, %zmm4
	vxorpd	%zmm13, %zmm2, %zmm1

	/* Yp_high */
	vsubpd	{rn-sae}, %zmm15, %zmm0, %zmm7

	/* -Ym_high */
	vsubpd	{rn-sae}, %zmm15, %zmm4, %zmm12

	/* RcpP ~ 1/Yp */
	vrcp14pd %zmm0, %zmm3

	/* RcpM ~ 1/Ym */
	vrcp14pd %zmm4, %zmm5

	/* input outside (-1, 1) ? */
	vcmppd	$21, {sae}, %zmm15, %zmm13, %k0
	vpaddq	%zmm6, %zmm3, %zmm11
	vpaddq	%zmm6, %zmm5, %zmm10

	/* Yp_low */
	vsubpd	{rn-sae}, %zmm7, %zmm13, %zmm8
	vandpd	%zmm9, %zmm11, %zmm14
	vandpd	%zmm9, %zmm10, %zmm3

	/* Ym_low */
	vaddpd	{rn-sae}, %zmm12, %zmm13, %zmm12

	/* Reduced argument: Rp = (RcpP*Yp - 1)+RcpP*Yp_low */
	vfmsub213pd {rn-sae}, %zmm15, %zmm14, %zmm0

	/* Reduced argument: Rm = (RcpM*Ym - 1)+RcpM*Ym_low */
	vfmsub231pd {rn-sae}, %zmm3, %zmm4, %zmm15

	/* exponents */
	vgetexppd {sae}, %zmm14, %zmm5
	vgetexppd {sae}, %zmm3, %zmm4

	/* Table lookups */
	vmovups	__svml_datanh_data_internal_avx512(%rip), %zmm9
	vmovups	Log_tbl_H+64+__svml_datanh_data_internal_avx512(%rip), %zmm13
	vmovups	Log_tbl_L+__svml_datanh_data_internal_avx512(%rip), %zmm7
	vfmadd231pd {rn-sae}, %zmm14, %zmm8, %zmm0
	vfnmadd231pd {rn-sae}, %zmm3, %zmm12, %zmm15

	/* Prepare table index */
	vpsrlq	$48, %zmm14, %zmm11
	vpsrlq	$48, %zmm3, %zmm8
	vmovups	Log_tbl_L+64+__svml_datanh_data_internal_avx512(%rip), %zmm14

	/* polynomials */
	vmovups	poly_coeff8+__svml_datanh_data_internal_avx512(%rip), %zmm3

	/* Km-Kp */
	vsubpd	{rn-sae}, %zmm5, %zmm4, %zmm5
	vmovups	poly_coeff7+__svml_datanh_data_internal_avx512(%rip), %zmm4
	kmovw	%k0, %edx
	vmovaps	%zmm11, %zmm10
	vmovaps	%zmm4, %zmm6
	vpermi2pd %zmm13, %zmm9, %zmm10
	vpermi2pd %zmm14, %zmm7, %zmm11
	vpermt2pd %zmm13, %zmm8, %zmm9
	vpermt2pd %zmm14, %zmm8, %zmm7
	vmovups	poly_coeff6+__svml_datanh_data_internal_avx512(%rip), %zmm8
	vfmadd231pd {rn-sae}, %zmm0, %zmm3, %zmm6
	vfmadd231pd {rn-sae}, %zmm15, %zmm3, %zmm4
	vmovups	poly_coeff3+__svml_datanh_data_internal_avx512(%rip), %zmm13
	vmovups	poly_coeff2+__svml_datanh_data_internal_avx512(%rip), %zmm14
	vfmadd213pd {rn-sae}, %zmm8, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm8, %zmm15, %zmm4
	vmovups	poly_coeff0+__svml_datanh_data_internal_avx512(%rip), %zmm8
	vsubpd	{rn-sae}, %zmm11, %zmm7, %zmm12

	/* table values */
	vsubpd	{rn-sae}, %zmm10, %zmm9, %zmm3
	vmovups	poly_coeff5+__svml_datanh_data_internal_avx512(%rip), %zmm7
	vmovups	poly_coeff4+__svml_datanh_data_internal_avx512(%rip), %zmm9

	/* K*L2H + Th */
	vmovups	L2H+__svml_datanh_data_internal_avx512(%rip), %zmm10

	/* K*L2L + Tl */
	vmovups	L2L+__svml_datanh_data_internal_avx512(%rip), %zmm11
	vfmadd213pd {rn-sae}, %zmm7, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm7, %zmm15, %zmm4
	vmovups	poly_coeff1+__svml_datanh_data_internal_avx512(%rip), %zmm7
	vfmadd231pd {rn-sae}, %zmm5, %zmm10, %zmm3
	vfmadd213pd {rn-sae}, %zmm12, %zmm11, %zmm5
	vfmadd213pd {rn-sae}, %zmm9, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm9, %zmm15, %zmm4
	vfmadd213pd {rn-sae}, %zmm13, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm13, %zmm15, %zmm4
	vfmadd213pd {rn-sae}, %zmm14, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm14, %zmm15, %zmm4
	vfmadd213pd {rn-sae}, %zmm7, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm7, %zmm15, %zmm4
	vfmadd213pd {rn-sae}, %zmm8, %zmm0, %zmm6
	vfmadd213pd {rn-sae}, %zmm8, %zmm15, %zmm4

	/* (K*L2L + Tl) + Rp*PolyP */
	vfmadd213pd {rn-sae}, %zmm5, %zmm0, %zmm6
	vorpd	Half+__svml_datanh_data_internal_avx512(%rip), %zmm1, %zmm0

	/* (K*L2L + Tl) + Rp*PolyP -Rm*PolyM */
	vfnmadd213pd {rn-sae}, %zmm6, %zmm15, %zmm4
	vaddpd	{rn-sae}, %zmm4, %zmm3, %zmm1
	vmulpd	{rn-sae}, %zmm0, %zmm1, %zmm0
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
	call	atanh@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 128(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8v_atanh_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_datanh_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Log_tbl_H[16][2];
	__declspec(align(64)) VUINT32 Log_tbl_L[16][2];
	__declspec(align(64)) VUINT32 One[8][2];
	__declspec(align(64)) VUINT32 AbsMask[8][2];
	__declspec(align(64)) VUINT32 AddB5[8][2];
	__declspec(align(64)) VUINT32 RcpBitMask[8][2];
	__declspec(align(64)) VUINT32 poly_coeff8[8][2];
	__declspec(align(64)) VUINT32 poly_coeff7[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 poly_coeff1[8][2];
	__declspec(align(64)) VUINT32 poly_coeff0[8][2];
	__declspec(align(64)) VUINT32 Half[8][2];
	__declspec(align(64)) VUINT32 L2H[8][2];
	__declspec(align(64)) VUINT32 L2L[8][2];
} __svml_datanh_data_internal_avx512;
#endif
__svml_datanh_data_internal_avx512:
	/* Log_tbl_H */
	.quad	0x0000000000000000
	.quad	0x3faf0a30c0100000
	.quad	0x3fbe27076e2a0000
	.quad	0x3fc5ff3070a80000
	.quad	0x3fcc8ff7c79b0000
	.quad	0x3fd1675cabab8000
	.quad	0x3fd4618bc21c8000
	.quad	0x3fd739d7f6bc0000
	.quad	0x3fd9f323ecbf8000
	.quad	0x3fdc8ff7c79a8000
	.quad	0x3fdf128f5faf0000
	.quad	0x3fe0be72e4254000
	.quad	0x3fe1e85f5e704000
	.quad	0x3fe307d7334f0000
	.quad	0x3fe41d8fe8468000
	.quad	0x3fe52a2d265bc000
	/* Log_tbl_L */
	.align	64
	.quad	0x0000000000000000
	.quad	0x3d662a6617cc9717
	.quad	0x3d6e5cbd3d50fffc
	.quad	0xbd6b0b0de3077d7e
	.quad	0xbd697794f689f843
	.quad	0x3d630701ce63eab9
	.quad	0xbd609ec17a426426
	.quad	0xbd67fcb18ed9d603
	.quad	0x3d584bf2b68d766f
	.quad	0x3d5a21ac25d81ef3
	.quad	0x3d3bb2cd720ec44c
	.quad	0xbd657d49676844cc
	.quad	0x3d1a07bd8b34be7c
	.quad	0x3d60be1fb590a1f5
	.quad	0xbd5aa33736867a17
	.quad	0x3d46abb9df22bc57
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* AbsMask */
	.align	64
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff
	/* AddB5 */
	.align	64
	.quad	0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000, 0x0000800000000000
	/* RcpBitMask */
	.align	64
	.quad	0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000, 0xffff000000000000
	/* poly_coeff8 */
	.align	64
	.quad	0x3fbc81dd40d38142, 0x3fbc81dd40d38142, 0x3fbc81dd40d38142, 0x3fbc81dd40d38142, 0x3fbc81dd40d38142, 0x3fbc81dd40d38142, 0x3fbc81dd40d38142, 0x3fbc81dd40d38142
	/* poly_coeff7 */
	.align	64
	.quad	0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70, 0xbfc0073cb82e8b70
	/* poly_coeff6 */
	.align	64
	.quad	0x3fc2492298ffdae8, 0x3fc2492298ffdae8, 0x3fc2492298ffdae8, 0x3fc2492298ffdae8, 0x3fc2492298ffdae8, 0x3fc2492298ffdae8, 0x3fc2492298ffdae8, 0x3fc2492298ffdae8
	/* poly_coeff5 */
	.align	64
	.quad	0xbfc55553f871e5c5, 0xbfc55553f871e5c5, 0xbfc55553f871e5c5, 0xbfc55553f871e5c5, 0xbfc55553f871e5c5, 0xbfc55553f871e5c5, 0xbfc55553f871e5c5, 0xbfc55553f871e5c5
	/* poly_coeff4 */
	.align	64
	.quad	0x3fc9999999cd394a, 0x3fc9999999cd394a, 0x3fc9999999cd394a, 0x3fc9999999cd394a, 0x3fc9999999cd394a, 0x3fc9999999cd394a, 0x3fc9999999cd394a, 0x3fc9999999cd394a
	/* poly_coeff3 */
	.align	64
	.quad	0xbfd00000000c2a01, 0xbfd00000000c2a01, 0xbfd00000000c2a01, 0xbfd00000000c2a01, 0xbfd00000000c2a01, 0xbfd00000000c2a01, 0xbfd00000000c2a01, 0xbfd00000000c2a01
	/* poly_coeff2 */
	.align	64
	.quad	0x3fd5555555555462, 0x3fd5555555555462, 0x3fd5555555555462, 0x3fd5555555555462, 0x3fd5555555555462, 0x3fd5555555555462, 0x3fd5555555555462, 0x3fd5555555555462
	/* poly_coeff1 */
	.align	64
	.quad	0xbfdfffffffffffc5, 0xbfdfffffffffffc5, 0xbfdfffffffffffc5, 0xbfdfffffffffffc5, 0xbfdfffffffffffc5, 0xbfdfffffffffffc5, 0xbfdfffffffffffc5, 0xbfdfffffffffffc5
	/* poly_coeff0 */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* Half */
	.align	64
	.quad	0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000
	/* L2H = log(2)_high */
	.align	64
	.quad	0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000, 0x3fe62E42FEFA0000
	/* L2L = log(2)_low */
	.align	64
	.quad	0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000, 0x3d7cf79abc9e0000
	.align	64
	.type	__svml_datanh_data_internal_avx512, @object
	.size	__svml_datanh_data_internal_avx512, .-__svml_datanh_data_internal_avx512
