/* Function acosf vectorized with AVX2.
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
 *      acos(|x|) = SelMask ? 2*Poly(R) : (Pi/2 - Poly(R))
 *      acos(x) = sign(x) ? (Pi - acos(|x|)) : acos(|x|)
 *
 *
 */

/* Offsets for data table __svml_sacos_data_internal
 */
#define SgnBit				0
#define OneHalf				32
#define SmallNorm			64
#define MOne				96
#define Two				128
#define sqrt_coeff			160
#define poly_coeff			224
#define Pi2H				384
#define PiH				416

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_acosf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/*
	 * 2*sqrt(X) ~ Sh - Sl  (to 24+ bits)
	 * SQ ~ 2*sqrt(X)
	 */
	vmovups	__svml_sacos_data_internal(%rip), %ymm6
	vmovups	OneHalf+__svml_sacos_data_internal(%rip), %ymm7
	vmovaps	%ymm0, %ymm5

	/* x = -|arg| */
	vorps	%ymm5, %ymm6, %ymm4

	/* Y = 0.5 + 0.5*(-x) */
	vfmadd231ps %ymm4, %ymm7, %ymm7

	/* x^2 */
	vmulps	%ymm4, %ymm4, %ymm8

	/* SQ ~ 2*sqrt(Y) */
	vmovups	sqrt_coeff+__svml_sacos_data_internal(%rip), %ymm0
	vcmpnge_uqps MOne+__svml_sacos_data_internal(%rip), %ymm4, %ymm9
	vcmplt_oqps SmallNorm+__svml_sacos_data_internal(%rip), %ymm7, %ymm10
	vminps	%ymm7, %ymm8, %ymm2
	vaddps	%ymm7, %ymm7, %ymm14
	vrsqrtps %ymm7, %ymm11
	vmovups	poly_coeff+64+__svml_sacos_data_internal(%rip), %ymm8
	vcmpnlt_uqps %ymm7, %ymm2, %ymm1
	vmulps	%ymm2, %ymm2, %ymm7
	vfmadd213ps poly_coeff+96+__svml_sacos_data_internal(%rip), %ymm2, %ymm8
	vmovmskps %ymm9, %edx

	/* polynomial */
	vmovups	poly_coeff+__svml_sacos_data_internal(%rip), %ymm9
	vandnps	%ymm11, %ymm10, %ymm12
	vmulps	%ymm12, %ymm12, %ymm13
	vfmadd213ps poly_coeff+32+__svml_sacos_data_internal(%rip), %ymm2, %ymm9

	/* X<X^2 iff X<0 */
	vcmplt_oqps %ymm2, %ymm5, %ymm10
	vfmadd213ps %ymm8, %ymm7, %ymm9
	vandps	%ymm5, %ymm6, %ymm3
	vmulps	%ymm14, %ymm12, %ymm6
	vfmsub213ps Two+__svml_sacos_data_internal(%rip), %ymm13, %ymm14
	vfmadd213ps poly_coeff+128+__svml_sacos_data_internal(%rip), %ymm2, %ymm9
	vfmadd213ps sqrt_coeff+32+__svml_sacos_data_internal(%rip), %ymm14, %ymm0
	vmulps	%ymm14, %ymm6, %ymm15
	vmulps	%ymm9, %ymm2, %ymm14
	vfnmadd213ps %ymm6, %ymm15, %ymm0
	vblendvps %ymm1, %ymm0, %ymm4, %ymm0
	vandps	PiH+__svml_sacos_data_internal(%rip), %ymm1, %ymm2
	vandnps	Pi2H+__svml_sacos_data_internal(%rip), %ymm1, %ymm12
	vxorps	%ymm3, %ymm0, %ymm1
	vfmadd213ps %ymm1, %ymm1, %ymm14
	vandps	%ymm10, %ymm2, %ymm11
	vaddps	%ymm12, %ymm11, %ymm13
	vaddps	%ymm14, %ymm13, %ymm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm5

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
	vmovups	%ymm5, 32(%rsp)
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
	call	acosf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_acosf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sacos_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 SgnBit[8][1];
	__declspec(align(32)) VUINT32 OneHalf[8][1];
	__declspec(align(32)) VUINT32 SmallNorm[8][1];
	__declspec(align(32)) VUINT32 MOne[8][1];
	__declspec(align(32)) VUINT32 Two[8][1];
	__declspec(align(32)) VUINT32 sqrt_coeff[2][8][1];
	__declspec(align(32)) VUINT32 poly_coeff[5][8][1];
	__declspec(align(32)) VUINT32 Pi2H[8][1];
	__declspec(align(32)) VUINT32 PiH[8][1];
} __svml_sacos_data_internal;
#endif
__svml_sacos_data_internal:
	/* SgnBit */
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
	/* OneHalf */
	.align	32
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* SmallNorm */
	.align	32
	.long	0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000
	/* MOne */
	.align	32
	.long	0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
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
	/* PiH */
	.align	32
	.long	0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB
	.align	32
	.type	__svml_sacos_data_internal, @object
	.size	__svml_sacos_data_internal, .-__svml_sacos_data_internal
