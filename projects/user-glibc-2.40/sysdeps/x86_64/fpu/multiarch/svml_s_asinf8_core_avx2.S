/* Function asinf vectorized with AVX2.
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
#define OneHalf				32
#define SmallNorm			64
#define One				96
#define Two				128
#define sqrt_coeff			160
#define poly_coeff			224
#define Pi2H				384

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_asinf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	vmovups	__svml_sasin_data_internal(%rip), %ymm5
	vmovups	OneHalf+__svml_sasin_data_internal(%rip), %ymm9
	vmovups	One+__svml_sasin_data_internal(%rip), %ymm6
	vmovaps	%ymm0, %ymm4

	/* x = |arg| */
	vandps	%ymm4, %ymm5, %ymm3

	/* Y = 0.5 - 0.5*x */
	vmovaps	%ymm9, %ymm12
	vfnmadd231ps %ymm3, %ymm9, %ymm12

	/* x^2 */
	vmulps	%ymm3, %ymm3, %ymm7
	vcmplt_oqps %ymm3, %ymm6, %ymm8

	/* SQ ~ -2*sqrt(Y) */
	vcmplt_oqps SmallNorm+__svml_sasin_data_internal(%rip), %ymm12, %ymm10
	vminps	%ymm12, %ymm7, %ymm1
	vaddps	%ymm12, %ymm12, %ymm15
	vcmpnlt_uqps %ymm9, %ymm3, %ymm0
	vrsqrtps %ymm12, %ymm11
	vmovups	poly_coeff+64+__svml_sasin_data_internal(%rip), %ymm7
	vmulps	%ymm1, %ymm1, %ymm6
	vmovups	sqrt_coeff+__svml_sasin_data_internal(%rip), %ymm9
	vfmadd213ps poly_coeff+96+__svml_sasin_data_internal(%rip), %ymm1, %ymm7
	vmovmskps %ymm8, %edx

	/* polynomial */
	vmovups	poly_coeff+__svml_sasin_data_internal(%rip), %ymm8
	vandnps	%ymm11, %ymm10, %ymm13
	vmulps	%ymm13, %ymm13, %ymm14
	vfmadd213ps poly_coeff+32+__svml_sasin_data_internal(%rip), %ymm1, %ymm8
	vandnps	%ymm4, %ymm5, %ymm2
	vmulps	%ymm15, %ymm13, %ymm5
	vfmsub213ps Two+__svml_sasin_data_internal(%rip), %ymm14, %ymm15
	vfmadd213ps %ymm7, %ymm6, %ymm8
	vfmadd213ps sqrt_coeff+32+__svml_sasin_data_internal(%rip), %ymm15, %ymm9
	vmulps	%ymm15, %ymm5, %ymm15
	vfmadd213ps poly_coeff+128+__svml_sasin_data_internal(%rip), %ymm1, %ymm8
	vfmsub213ps %ymm5, %ymm15, %ymm9
	vmulps	%ymm8, %ymm1, %ymm1
	vblendvps %ymm0, %ymm9, %ymm3, %ymm3
	vfmadd213ps %ymm3, %ymm3, %ymm1
	vandps	Pi2H+__svml_sasin_data_internal(%rip), %ymm0, %ymm0
	vaddps	%ymm1, %ymm0, %ymm10
	vxorps	%ymm2, %ymm10, %ymm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm4

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
	vmovups	%ymm4, 32(%rsp)
	vmovups	%ymm0, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -96; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa0, 0xff, 0xff, 0xff, 0x22
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
	vmovups	64(%rsp), %ymm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -96; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	32(%rsp, %r14, 4), %xmm0
	call	asinf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_asinf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sasin_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 AbsMask[8][1];
	__declspec(align(32)) VUINT32 OneHalf[8][1];
	__declspec(align(32)) VUINT32 SmallNorm[8][1];
	__declspec(align(32)) VUINT32 One[8][1];
	__declspec(align(32)) VUINT32 Two[8][1];
	__declspec(align(32)) VUINT32 sqrt_coeff[2][8][1];
	__declspec(align(32)) VUINT32 poly_coeff[5][8][1];
	__declspec(align(32)) VUINT32 Pi2H[8][1];
} __svml_sasin_data_internal;
#endif
__svml_sasin_data_internal:
	/* AbsMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* OneHalf */
	.align	32
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* SmallNorm */
	.align	32
	.long	0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000
	/* One */
	.align	32
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* Two */
	.align	32
	.long	0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000
	/* sqrt_coeff[2] */
	.align	32
	.long	0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004 /* sqrt_coeff2 */
	.long	0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001 /* sqrt_coeff1 */
	/* poly_coeff[5] */
	.align	32
	.long	0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07 /* poly_coeff5 */
	.long	0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B /* poly_coeff4 */
	.long	0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4 /* poly_coeff3 */
	.long	0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12 /* poly_coeff2 */
	.long	0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF /* poly_coeff1 */
	/* Pi2H */
	.align	32
	.long	0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB
	.align	32
	.type	__svml_sasin_data_internal, @object
	.size	__svml_sasin_data_internal, .-__svml_sasin_data_internal
