/* Function asin vectorized with AVX2.
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
 */

/* Offsets for data table __svml_dasin_data_internal
 */
#define AbsMask				0
#define OneHalf				32
#define SmallNorm			64
#define One				96
#define Two				128
#define sqrt_coeff			160
#define poly_coeff			288
#define Pi2H				672

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4v_asin_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	vmovupd	__svml_dasin_data_internal(%rip), %ymm6
	vmovupd	OneHalf+__svml_dasin_data_internal(%rip), %ymm10
	vmovupd	One+__svml_dasin_data_internal(%rip), %ymm8
	vmovapd	%ymm0, %ymm5

	/* x = |arg| */
	vandpd	%ymm5, %ymm6, %ymm4

	/* Y = 0.5 - 0.5*x */
	vmovapd	%ymm10, %ymm15
	vfnmadd231pd %ymm4, %ymm10, %ymm15

	/* x^2 */
	vmulpd	%ymm4, %ymm4, %ymm7
	vcmplt_oqpd %ymm4, %ymm8, %ymm9

	/* S ~ -2*sqrt(Y) */
	vcmplt_oqpd SmallNorm+__svml_dasin_data_internal(%rip), %ymm15, %ymm13
	vminpd	%ymm15, %ymm7, %ymm2
	vaddpd	%ymm15, %ymm15, %ymm7
	vcmpnlt_uqpd %ymm10, %ymm4, %ymm1
	vcvtpd2ps %ymm15, %xmm11
	vmovupd	poly_coeff+64+__svml_dasin_data_internal(%rip), %ymm10
	vmulpd	%ymm2, %ymm2, %ymm15
	vrsqrtps %xmm11, %xmm12
	vmovupd	poly_coeff+192+__svml_dasin_data_internal(%rip), %ymm11
	vfmadd213pd poly_coeff+96+__svml_dasin_data_internal(%rip), %ymm2, %ymm10
	vcvtps2pd %xmm12, %ymm14
	vmulpd	%ymm15, %ymm15, %ymm12
	vfmadd213pd poly_coeff+224+__svml_dasin_data_internal(%rip), %ymm2, %ymm11
	vandnpd	%ymm14, %ymm13, %ymm0
	vandnpd	%ymm5, %ymm6, %ymm3
	vmulpd	%ymm0, %ymm0, %ymm6
	vmovupd	poly_coeff+128+__svml_dasin_data_internal(%rip), %ymm13
	vmovupd	poly_coeff+256+__svml_dasin_data_internal(%rip), %ymm14
	vfmadd213pd poly_coeff+160+__svml_dasin_data_internal(%rip), %ymm2, %ymm13
	vfmadd213pd poly_coeff+288+__svml_dasin_data_internal(%rip), %ymm2, %ymm14
	vfmadd213pd %ymm11, %ymm15, %ymm13
	vmovmskpd %ymm9, %edx
	vmulpd	%ymm7, %ymm0, %ymm9
	vfmsub213pd Two+__svml_dasin_data_internal(%rip), %ymm6, %ymm7

	/* polynomial */
	vmovupd	poly_coeff+__svml_dasin_data_internal(%rip), %ymm6
	vmovupd	sqrt_coeff+__svml_dasin_data_internal(%rip), %ymm0
	vmulpd	%ymm7, %ymm9, %ymm8
	vfmadd213pd poly_coeff+32+__svml_dasin_data_internal(%rip), %ymm2, %ymm6
	vfmadd213pd sqrt_coeff+32+__svml_dasin_data_internal(%rip), %ymm7, %ymm0
	vfmadd213pd %ymm10, %ymm15, %ymm6
	vmovupd	poly_coeff+320+__svml_dasin_data_internal(%rip), %ymm10
	vfmadd213pd sqrt_coeff+64+__svml_dasin_data_internal(%rip), %ymm7, %ymm0
	vfmadd213pd %ymm13, %ymm12, %ymm6
	vfmadd213pd poly_coeff+352+__svml_dasin_data_internal(%rip), %ymm2, %ymm10
	vfmadd213pd sqrt_coeff+96+__svml_dasin_data_internal(%rip), %ymm7, %ymm0
	vfmadd213pd %ymm14, %ymm15, %ymm6
	vfmsub213pd %ymm9, %ymm8, %ymm0
	vfmadd213pd %ymm10, %ymm15, %ymm6
	vblendvpd %ymm1, %ymm0, %ymm4, %ymm4
	vmulpd	%ymm6, %ymm2, %ymm2
	vfmadd213pd %ymm4, %ymm4, %ymm2
	vandpd	Pi2H+__svml_dasin_data_internal(%rip), %ymm1, %ymm1
	vaddpd	%ymm2, %ymm1, %ymm0
	vxorpd	%ymm3, %ymm0, %ymm0
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
	vmovupd	%ymm5, 32(%rsp)
	vmovupd	%ymm0, 64(%rsp)
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
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	vmovupd	64(%rsp), %ymm0

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
	vmovsd	32(%rsp, %r14, 8), %xmm0
	call	asin@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 64(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN4v_asin_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_dasin_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 AbsMask[4][2];
	__declspec(align(32)) VUINT32 OneHalf[4][2];
	__declspec(align(32)) VUINT32 SmallNorm[4][2];
	__declspec(align(32)) VUINT32 One[4][2];
	__declspec(align(32)) VUINT32 Two[4][2];
	__declspec(align(32)) VUINT32 sqrt_coeff[4][4][2];
	__declspec(align(32)) VUINT32 poly_coeff[12][4][2];
	__declspec(align(32)) VUINT32 Pi2H[4][2];
} __svml_dasin_data_internal;
#endif
__svml_dasin_data_internal:
	/* AbsMask */
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff
	/* OneHalf */
	.align	32
	.quad	0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000, 0x3fe0000000000000
	/* SmallNorm */
	.align	32
	.quad	0x3000000000000000, 0x3000000000000000, 0x3000000000000000, 0x3000000000000000
	/* One */
	.align	32
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* Two */
	.align	32
	.quad	0x4000000000000000, 0x4000000000000000, 0x4000000000000000, 0x4000000000000000
	/* sqrt_coeff[4] */
	.align	32
	.quad	0xbf918000993B24C3, 0xbf918000993B24C3, 0xbf918000993B24C3, 0xbf918000993B24C3 /* sqrt_coeff4 */
	.quad	0x3fa400006F70D42D, 0x3fa400006F70D42D, 0x3fa400006F70D42D, 0x3fa400006F70D42D /* sqrt_coeff3 */
	.quad	0xbfb7FFFFFFFFFE97, 0xbfb7FFFFFFFFFE97, 0xbfb7FFFFFFFFFE97, 0xbfb7FFFFFFFFFE97 /* sqrt_coeff2 */
	.quad	0x3fcFFFFFFFFFFF9D, 0x3fcFFFFFFFFFFF9D, 0x3fcFFFFFFFFFFF9D, 0x3fcFFFFFFFFFFF9D /* sqrt_coeff1 */
	/* poly_coeff[12] */
	.align	32
	.quad	0x3fa07520C70EB909, 0x3fa07520C70EB909, 0x3fa07520C70EB909, 0x3fa07520C70EB909 /* poly_coeff12 */
	.quad	0xbf90FB17F7DBB0ED, 0xbf90FB17F7DBB0ED, 0xbf90FB17F7DBB0ED, 0xbf90FB17F7DBB0ED /* poly_coeff11 */
	.quad	0x3f943F44BFBC3BAE, 0x3f943F44BFBC3BAE, 0x3f943F44BFBC3BAE, 0x3f943F44BFBC3BAE /* poly_coeff10 */
	.quad	0x3f7A583395D45ED5, 0x3f7A583395D45ED5, 0x3f7A583395D45ED5, 0x3f7A583395D45ED5 /* poly_coeff9 */
	.quad	0x3f88F8DC2AFCCAD6, 0x3f88F8DC2AFCCAD6, 0x3f88F8DC2AFCCAD6, 0x3f88F8DC2AFCCAD6 /* poly_coeff8 */
	.quad	0x3f8C6DBBCB88BD57, 0x3f8C6DBBCB88BD57, 0x3f8C6DBBCB88BD57, 0x3f8C6DBBCB88BD57 /* poly_coeff7 */
	.quad	0x3f91C6DCF538AD2E, 0x3f91C6DCF538AD2E, 0x3f91C6DCF538AD2E, 0x3f91C6DCF538AD2E /* poly_coeff6 */
	.quad	0x3f96E89CEBDEFadd, 0x3f96E89CEBDEFadd, 0x3f96E89CEBDEFadd, 0x3f96E89CEBDEFadd /* poly_coeff5 */
	.quad	0x3f9F1C72E13AD8BE, 0x3f9F1C72E13AD8BE, 0x3f9F1C72E13AD8BE, 0x3f9F1C72E13AD8BE /* poly_coeff4 */
	.quad	0x3fa6DB6DB3B445F8, 0x3fa6DB6DB3B445F8, 0x3fa6DB6DB3B445F8, 0x3fa6DB6DB3B445F8 /* poly_coeff3 */
	.quad	0x3fb333333337E0DE, 0x3fb333333337E0DE, 0x3fb333333337E0DE, 0x3fb333333337E0DE /* poly_coeff2 */
	.quad	0x3fc555555555529C, 0x3fc555555555529C, 0x3fc555555555529C, 0x3fc555555555529C /* poly_coeff1 */
	/* Pi2H */
	.align	32
	.quad	0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18
	.align	32
	.type	__svml_dasin_data_internal, @object
	.size	__svml_dasin_data_internal, .-__svml_dasin_data_internal
