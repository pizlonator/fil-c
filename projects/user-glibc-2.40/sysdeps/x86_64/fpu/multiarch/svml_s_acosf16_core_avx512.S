/* Function acosf vectorized with AVX-512.
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
#define OneHalf				64
#define SmallNorm			128
#define MOne				192
#define Two				256
#define sqrt_coeff_1			320
#define sqrt_coeff_2			384
#define poly_coeff_1			448
#define poly_coeff_2			512
#define poly_coeff_3			576
#define poly_coeff_4			640
#define poly_coeff_5			704
#define Pi2H				768
#define PiH				832

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_acosf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	__svml_sacos_data_internal(%rip), %zmm5
	vmovups	OneHalf+__svml_sacos_data_internal(%rip), %zmm6

	/* SQ ~ 2*sqrt(Y) */
	vmovups	SmallNorm+__svml_sacos_data_internal(%rip), %zmm9
	vmovups	MOne+__svml_sacos_data_internal(%rip), %zmm8
	vmovups	Two+__svml_sacos_data_internal(%rip), %zmm12
	vmovups	sqrt_coeff_1+__svml_sacos_data_internal(%rip), %zmm13
	vmovaps	%zmm0, %zmm4

	/* x = -|arg| */
	vorps	%zmm4, %zmm5, %zmm3
	vandps	%zmm4, %zmm5, %zmm2
	vmovups	sqrt_coeff_2+__svml_sacos_data_internal(%rip), %zmm0

	/* Y = 0.5 + 0.5*(-x) */
	vfmadd231ps {rn-sae}, %zmm3, %zmm6, %zmm6

	/* x^2 */
	vmulps	{rn-sae}, %zmm3, %zmm3, %zmm7
	vrsqrt14ps %zmm6, %zmm10
	vcmpps	$17, {sae}, %zmm9, %zmm6, %k1
	vcmpps	$22, {sae}, %zmm3, %zmm8, %k0
	vmovups	poly_coeff_4+__svml_sacos_data_internal(%rip), %zmm9
	vminps	{sae}, %zmm6, %zmm7, %zmm1
	vmovups	poly_coeff_3+__svml_sacos_data_internal(%rip), %zmm7
	vxorps	%zmm10, %zmm10, %zmm10{%k1}
	vaddps	{rn-sae}, %zmm6, %zmm6, %zmm14
	vmulps	{rn-sae}, %zmm1, %zmm1, %zmm8
	vmulps	{rn-sae}, %zmm10, %zmm10, %zmm11
	vmulps	{rn-sae}, %zmm10, %zmm14, %zmm5
	vcmpps	$21, {sae}, %zmm6, %zmm1, %k4

	/* X<X^2 iff X<0 */
	vcmpps	$17, {sae}, %zmm1, %zmm4, %k2

	/* polynomial */
	vmovups	poly_coeff_1+__svml_sacos_data_internal(%rip), %zmm6
	vfmsub213ps {rn-sae}, %zmm12, %zmm11, %zmm14
	vmovups	poly_coeff_2+__svml_sacos_data_internal(%rip), %zmm11
	vfmadd231ps {rn-sae}, %zmm1, %zmm7, %zmm9
	vmovups	poly_coeff_5+__svml_sacos_data_internal(%rip), %zmm10
	vmovups	Pi2H+__svml_sacos_data_internal(%rip), %zmm12
	vfmadd231ps {rn-sae}, %zmm14, %zmm13, %zmm0
	vfmadd231ps {rn-sae}, %zmm1, %zmm6, %zmm11
	vmulps	{rn-sae}, %zmm14, %zmm5, %zmm15
	vfmadd213ps {rn-sae}, %zmm9, %zmm8, %zmm11
	vxorps	%zmm12, %zmm12, %zmm12{%k4}
	vfnmadd213ps {rn-sae}, %zmm5, %zmm15, %zmm0
	vfmadd213ps {rn-sae}, %zmm10, %zmm1, %zmm11
	kmovw	%k0, %edx
	vmulps	{rn-sae}, %zmm1, %zmm11, %zmm13
	vblendmps %zmm0, %zmm3, %zmm0{%k4}
	vxorps	%zmm2, %zmm0, %zmm1
	kandw	%k4, %k2, %k3
	vfmadd213ps {rn-sae}, %zmm1, %zmm1, %zmm13
	vorps	PiH+__svml_sacos_data_internal(%rip), %zmm12, %zmm12{%k3}
	vaddps	{rn-sae}, %zmm13, %zmm12, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm4

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
	vmovups	%zmm4, 64(%rsp)
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
	call	acosf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_acosf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sacos_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 SgnBit[16][1];
	__declspec(align(64)) VUINT32 OneHalf[16][1];
	__declspec(align(64)) VUINT32 SmallNorm[16][1];
	__declspec(align(64)) VUINT32 MOne[16][1];
	__declspec(align(64)) VUINT32 Two[16][1];
	__declspec(align(64)) VUINT32 sqrt_coeff[2][16][1];
	__declspec(align(64)) VUINT32 poly_coeff[5][16][1];
	__declspec(align(64)) VUINT32 Pi2H[16][1];
	__declspec(align(64)) VUINT32 PiH[16][1];
} __svml_sacos_data_internal;
#endif
__svml_sacos_data_internal:
	/* SgnBit */
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
	/* OneHalf */
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* SmallNorm */
	.align	64
	.long	0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000
	/* MOne */
	.align	64
	.long	0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
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
	/* PiH */
	.align	64
	.long	0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB
	.align	64
	.type	__svml_sacos_data_internal, @object
	.size	__svml_sacos_data_internal, .-__svml_sacos_data_internal
