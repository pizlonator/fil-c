/* Function asinf vectorized with AVX-512.
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
 *      SelMask = (|x| >= 0.5) ? 1 : 0;
 *      R = SelMask ? sqrt(0.5 - 0.5*|x|) : |x|
 *      asin(x) = (SelMask ? (Pi/2 - 2*Poly(R)) : Poly(R))*(-1)^sign(x)
 *
 *
 */

/* Offsets for data table __svml_sasin_data_internal
 */
#define AbsMask				0
#define OneHalf				64
#define SmallNorm			128
#define One				192
#define Two				256
#define sqrt_coeff_1			320
#define sqrt_coeff_2			384
#define poly_coeff_1			448
#define poly_coeff_2			512
#define poly_coeff_3			576
#define poly_coeff_4			640
#define poly_coeff_5			704
#define Pi2H				768

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_asinf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	__svml_sasin_data_internal(%rip), %zmm4
	vmovups	OneHalf+__svml_sasin_data_internal(%rip), %zmm6

	/* SQ ~ -2*sqrt(Y) */
	vmovups	SmallNorm+__svml_sasin_data_internal(%rip), %zmm8
	vmovups	Two+__svml_sasin_data_internal(%rip), %zmm12
	vmovups	sqrt_coeff_1+__svml_sasin_data_internal(%rip), %zmm13
	vmovups	One+__svml_sasin_data_internal(%rip), %zmm7
	vmovaps	%zmm0, %zmm3

	/* x = |arg| */
	vandps	%zmm3, %zmm4, %zmm2
	vandnps	%zmm3, %zmm4, %zmm1

	/* x^2 */
	vmulps	{rn-sae}, %zmm2, %zmm2, %zmm5
	vcmpps	$17, {sae}, %zmm2, %zmm7, %k0
	vcmpps	$21, {sae}, %zmm6, %zmm2, %k2
	vmovups	poly_coeff_2+__svml_sasin_data_internal(%rip), %zmm7
	kmovw	%k0, %edx

	/* Y = 0.5 - 0.5*x */
	vmovaps	%zmm6, %zmm9
	vfnmadd231ps {rn-sae}, %zmm2, %zmm6, %zmm9
	vmovups	poly_coeff_5+__svml_sasin_data_internal(%rip), %zmm6
	vrsqrt14ps %zmm9, %zmm10
	vcmpps	$17, {sae}, %zmm8, %zmm9, %k1
	vminps	{sae}, %zmm9, %zmm5, %zmm0
	vmovups	sqrt_coeff_2+__svml_sasin_data_internal(%rip), %zmm8
	vmovups	poly_coeff_4+__svml_sasin_data_internal(%rip), %zmm5
	vxorps	%zmm10, %zmm10, %zmm10{%k1}
	vaddps	{rn-sae}, %zmm9, %zmm9, %zmm14
	vmulps	{rn-sae}, %zmm10, %zmm10, %zmm11
	vmulps	{rn-sae}, %zmm10, %zmm14, %zmm4
	vfmsub213ps {rn-sae}, %zmm12, %zmm11, %zmm14
	vmulps	{rn-sae}, %zmm14, %zmm4, %zmm15
	vfmadd231ps {rn-sae}, %zmm14, %zmm13, %zmm8
	vmovups	poly_coeff_3+__svml_sasin_data_internal(%rip), %zmm14

	/* polynomial */
	vmovups	poly_coeff_1+__svml_sasin_data_internal(%rip), %zmm13
	vfmsub213ps {rn-sae}, %zmm4, %zmm15, %zmm8
	vfmadd231ps {rn-sae}, %zmm0, %zmm14, %zmm5
	vfmadd231ps {rn-sae}, %zmm0, %zmm13, %zmm7
	vmulps	{rn-sae}, %zmm0, %zmm0, %zmm15
	vblendmps %zmm8, %zmm2, %zmm2{%k2}
	vfmadd213ps {rn-sae}, %zmm5, %zmm15, %zmm7
	vfmadd213ps {rn-sae}, %zmm6, %zmm0, %zmm7
	vmulps	{rn-sae}, %zmm0, %zmm7, %zmm9
	vmovups	Pi2H+__svml_sasin_data_internal(%rip), %zmm0
	vfmadd213ps {rn-sae}, %zmm2, %zmm2, %zmm9
	vaddps	{rn-sae}, %zmm0, %zmm9, %zmm9{%k2}
	vxorps	%zmm1, %zmm9, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm3

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
	vmovups	%zmm3, 64(%rsp)
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
	call	asinf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_asinf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sasin_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 AbsMask[16][1];
	__declspec(align(64)) VUINT32 OneHalf[16][1];
	__declspec(align(64)) VUINT32 SmallNorm[16][1];
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 Two[16][1];
	__declspec(align(64)) VUINT32 sqrt_coeff[2][16][1];
	__declspec(align(64)) VUINT32 poly_coeff[5][16][1];
	__declspec(align(64)) VUINT32 Pi2H[16][1];
} __svml_sasin_data_internal;
#endif
__svml_sasin_data_internal:
	/* AbsMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* OneHalf */
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* SmallNorm */
	.align	64
	.long	0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000
	/* One */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* Two */
	.align	64
	.long	0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000
	/* sqrt_coeff[2] */
	.align	64
	.long	0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004 /* sqrt_coeff2 */
	.long	0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001 /* sqrt_coeff1 */
	/* poly_coeff[5] */
	.align	64
	.long	0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07 /* poly_coeff5 */
	.long	0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B /* poly_coeff4 */
	.long	0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4 /* poly_coeff3 */
	.long	0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12 /* poly_coeff2 */
	.long	0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF /* poly_coeff1 */
	/* Pi2H */
	.align	64
	.long	0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB
	.align	64
	.type	__svml_sasin_data_internal, @object
	.size	__svml_sasin_data_internal, .-__svml_sasin_data_internal
