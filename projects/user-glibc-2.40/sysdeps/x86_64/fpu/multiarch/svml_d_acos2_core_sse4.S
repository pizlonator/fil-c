/* Function acos vectorized with SSE4.
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
 */

/* Offsets for data table __svml_dacos_data_internal
 */
#define SgnBit				0
#define OneHalf				16
#define SmallNorm			32
#define MOne				48
#define Two				64
#define sqrt_coeff			80
#define poly_coeff			144
#define PiH				336
#define Pi2H				352

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_acos_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm5
	movups	__svml_dacos_data_internal(%rip), %xmm3
	movups	OneHalf+__svml_dacos_data_internal(%rip), %xmm6

	/* x = -|arg| */
	movaps	%xmm3, %xmm4
	orps	%xmm5, %xmm4

	/* Y = 0.5 + 0.5*(-x) */
	movaps	%xmm6, %xmm7
	mulpd	%xmm4, %xmm7
	addpd	%xmm7, %xmm6

	/* S ~ 2*sqrt(Y) */
	cvtpd2ps %xmm6, %xmm9
	movlhps	%xmm9, %xmm9

	/* x^2 */
	movaps	%xmm4, %xmm0
	rsqrtps	%xmm9, %xmm10
	mulpd	%xmm4, %xmm0
	cvtps2pd %xmm10, %xmm11
	minpd	%xmm6, %xmm0
	movaps	%xmm6, %xmm1
	movaps	%xmm0, %xmm2
	cmpltpd	SmallNorm+__svml_dacos_data_internal(%rip), %xmm1
	cmpnltpd %xmm6, %xmm2
	addpd	%xmm6, %xmm6
	andnps	%xmm11, %xmm1
	movaps	%xmm0, %xmm11
	movaps	%xmm1, %xmm12
	andps	%xmm5, %xmm3
	mulpd	%xmm1, %xmm12
	mulpd	%xmm6, %xmm1
	mulpd	%xmm12, %xmm6
	mulpd	%xmm0, %xmm11
	subpd	Two+__svml_dacos_data_internal(%rip), %xmm6
	movups	sqrt_coeff+__svml_dacos_data_internal(%rip), %xmm13
	movaps	%xmm6, %xmm14
	mulpd	%xmm6, %xmm13
	mulpd	%xmm1, %xmm14
	addpd	sqrt_coeff+16+__svml_dacos_data_internal(%rip), %xmm13
	mulpd	%xmm6, %xmm13
	addpd	sqrt_coeff+32+__svml_dacos_data_internal(%rip), %xmm13
	mulpd	%xmm13, %xmm6

	/* polynomial */
	movups	poly_coeff+__svml_dacos_data_internal(%rip), %xmm15
	movaps	%xmm11, %xmm7
	mulpd	%xmm0, %xmm15
	addpd	sqrt_coeff+48+__svml_dacos_data_internal(%rip), %xmm6
	addpd	poly_coeff+16+__svml_dacos_data_internal(%rip), %xmm15
	mulpd	%xmm11, %xmm7
	mulpd	%xmm6, %xmm14
	mulpd	%xmm11, %xmm15
	subpd	%xmm14, %xmm1
	movups	MOne+__svml_dacos_data_internal(%rip), %xmm8
	andps	%xmm2, %xmm1

	/* NaN processed in special branch (so wind test passed) */
	cmpnlepd %xmm4, %xmm8
	movmskpd %xmm8, %edx

	/* X<X^2 iff X<0 */
	movaps	%xmm5, %xmm12
	movups	poly_coeff+32+__svml_dacos_data_internal(%rip), %xmm8
	movaps	%xmm2, %xmm13
	movups	poly_coeff+64+__svml_dacos_data_internal(%rip), %xmm6
	mulpd	%xmm0, %xmm8
	mulpd	%xmm0, %xmm6
	addpd	poly_coeff+48+__svml_dacos_data_internal(%rip), %xmm8
	addpd	poly_coeff+80+__svml_dacos_data_internal(%rip), %xmm6
	cmpltpd	%xmm0, %xmm12
	addpd	%xmm15, %xmm8
	mulpd	%xmm11, %xmm6
	mulpd	%xmm7, %xmm8
	movups	poly_coeff+96+__svml_dacos_data_internal(%rip), %xmm9
	mulpd	%xmm0, %xmm9
	addpd	poly_coeff+112+__svml_dacos_data_internal(%rip), %xmm9
	addpd	%xmm6, %xmm9
	movups	poly_coeff+128+__svml_dacos_data_internal(%rip), %xmm10
	movaps	%xmm2, %xmm6
	mulpd	%xmm0, %xmm10
	addpd	%xmm8, %xmm9
	addpd	poly_coeff+144+__svml_dacos_data_internal(%rip), %xmm10
	mulpd	%xmm11, %xmm9
	movups	poly_coeff+160+__svml_dacos_data_internal(%rip), %xmm14
	andnps	%xmm4, %xmm6
	addpd	%xmm9, %xmm10
	mulpd	%xmm0, %xmm14
	mulpd	%xmm10, %xmm11
	addpd	poly_coeff+176+__svml_dacos_data_internal(%rip), %xmm14
	addpd	%xmm11, %xmm14
	mulpd	%xmm0, %xmm14
	orps	%xmm1, %xmm6
	pxor	%xmm3, %xmm6
	mulpd	%xmm6, %xmm14
	movups	PiH+__svml_dacos_data_internal(%rip), %xmm0
	andps	%xmm2, %xmm0
	andnps	Pi2H+__svml_dacos_data_internal(%rip), %xmm13
	andps	%xmm12, %xmm0
	addpd	%xmm13, %xmm0
	addpd	%xmm14, %xmm6
	addpd	%xmm6, %xmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm5

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm5, 32(%rsp)
	movups	%xmm0, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 edx

	xorl	%eax, %eax
	movq	%r12, 16(%rsp)
	cfi_offset(12, -64)
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -72)
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	cfi_offset(14, -80)
	# LOE rbx rbp r15 r12d r13d

	/* Range mask
	 * bits check
	 */

L(RANGEMASK_CHECK):
	btl	%r12d, %r13d

	/* Call scalar math function */
	jc	L(SCALAR_MATH_CALL)
	# LOE rbx rbp r15 r12d r13d

	/* Special inputs
	 * processing loop
	 */

L(SPECIAL_VALUES_LOOP):
	incl	%r12d
	cmpl	$2, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm0

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	call	acos@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN2v_acos_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dacos_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 SgnBit[2][2];
	__declspec(align(16)) VUINT32 OneHalf[2][2];
	__declspec(align(16)) VUINT32 SmallNorm[2][2];
	__declspec(align(16)) VUINT32 MOne[2][2];
	__declspec(align(16)) VUINT32 Two[2][2];
	__declspec(align(16)) VUINT32 sqrt_coeff[4][2][2];
	__declspec(align(16)) VUINT32 poly_coeff[12][2][2];
	__declspec(align(16)) VUINT32 PiH[2][2];
	__declspec(align(16)) VUINT32 Pi2H[2][2];
} __svml_dacos_data_internal;
#endif
__svml_dacos_data_internal:
	/* SgnBit */
	.quad	0x8000000000000000, 0x8000000000000000
	/* OneHalf */
	.align	16
	.quad	0x3fe0000000000000, 0x3fe0000000000000
	/* SmallNorm */
	.align	16
	.quad	0x3000000000000000, 0x3000000000000000
	/* MOne */
	.align	16
	.quad	0xbff0000000000000, 0xbff0000000000000
	/* Two */
	.align	16
	.quad	0x4000000000000000, 0x4000000000000000
	/* sqrt_coeff[4] */
	.align	16
	.quad	0xbf918000993B24C3, 0xbf918000993B24C3 /* sqrt_coeff4 */
	.quad	0x3fa400006F70D42D, 0x3fa400006F70D42D /* sqrt_coeff3 */
	.quad	0xbfb7FFFFFFFFFE97, 0xbfb7FFFFFFFFFE97 /* sqrt_coeff2 */
	.quad	0x3fcFFFFFFFFFFF9D, 0x3fcFFFFFFFFFFF9D /* sqrt_coeff1 */
	/* poly_coeff[12] */
	.align	16
	.quad	0x3fa07520C70EB909, 0x3fa07520C70EB909 /* poly_coeff12 */
	.quad	0xbf90FB17F7DBB0ED, 0xbf90FB17F7DBB0ED /* poly_coeff11 */
	.quad	0x3f943F44BFBC3BAE, 0x3f943F44BFBC3BAE /* poly_coeff10 */
	.quad	0x3f7A583395D45ED5, 0x3f7A583395D45ED5 /* poly_coeff9 */
	.quad	0x3f88F8DC2AFCCAD6, 0x3f88F8DC2AFCCAD6 /* poly_coeff8 */
	.quad	0x3f8C6DBBCB88BD57, 0x3f8C6DBBCB88BD57 /* poly_coeff7 */
	.quad	0x3f91C6DCF538AD2E, 0x3f91C6DCF538AD2E /* poly_coeff6 */
	.quad	0x3f96E89CEBDEFadd, 0x3f96E89CEBDEFadd /* poly_coeff5 */
	.quad	0x3f9F1C72E13AD8BE, 0x3f9F1C72E13AD8BE /* poly_coeff4 */
	.quad	0x3fa6DB6DB3B445F8, 0x3fa6DB6DB3B445F8 /* poly_coeff3 */
	.quad	0x3fb333333337E0DE, 0x3fb333333337E0DE /* poly_coeff2 */
	.quad	0x3fc555555555529C, 0x3fc555555555529C /* poly_coeff1 */
	/* PiH */
	.align	16
	.quad	0x400921fb54442d18, 0x400921fb54442d18
	/* Pi2H */
	.align	16
	.quad	0x3ff921fb54442d18, 0x3ff921fb54442d18
	.align	16
	.type	__svml_dacos_data_internal, @object
	.size	__svml_dacos_data_internal, .-__svml_dacos_data_internal
