/* Function acosf vectorized with SSE4.
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
#define OneHalf				16
#define SmallNorm			32
#define MOne				48
#define Two				64
#define sqrt_coeff			80
#define poly_coeff			112
#define Pi2H				192
#define PiH				208

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_acosf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/* X<X^2 iff X<0 */
	movaps	%xmm0, %xmm14

	/*
	 * 2*sqrt(X) ~ Sh - Sl  (to 24+ bits)
	 * SQ ~ 2*sqrt(X)
	 */
	movups	__svml_sacos_data_internal(%rip), %xmm3
	movups	OneHalf+__svml_sacos_data_internal(%rip), %xmm5

	/* x = -|arg| */
	movaps	%xmm3, %xmm4
	orps	%xmm0, %xmm4

	/* Y = 0.5 + 0.5*(-x) */
	movaps	%xmm5, %xmm6
	mulps	%xmm4, %xmm6

	/* x^2 */
	movaps	%xmm4, %xmm13
	mulps	%xmm4, %xmm13
	addps	%xmm6, %xmm5

	/* SQ ~ 2*sqrt(Y) */
	rsqrtps	%xmm5, %xmm8
	minps	%xmm5, %xmm13
	movaps	%xmm5, %xmm2
	movaps	%xmm13, %xmm1
	cmpltps	SmallNorm+__svml_sacos_data_internal(%rip), %xmm2
	cmpnltps %xmm5, %xmm1
	cmpltps	%xmm13, %xmm14
	addps	%xmm5, %xmm5
	andnps	%xmm8, %xmm2
	movaps	%xmm13, %xmm11
	movaps	%xmm2, %xmm9
	movaps	%xmm1, %xmm6
	mulps	%xmm2, %xmm9
	andnps	%xmm4, %xmm6
	mulps	%xmm5, %xmm2
	mulps	%xmm13, %xmm11
	mulps	%xmm9, %xmm5
	movups	sqrt_coeff+__svml_sacos_data_internal(%rip), %xmm10
	andps	%xmm0, %xmm3

	/* polynomial */
	movups	poly_coeff+__svml_sacos_data_internal(%rip), %xmm12
	movaps	%xmm1, %xmm15
	mulps	%xmm13, %xmm12
	subps	Two+__svml_sacos_data_internal(%rip), %xmm5
	mulps	%xmm5, %xmm10
	addps	poly_coeff+16+__svml_sacos_data_internal(%rip), %xmm12
	mulps	%xmm2, %xmm5
	mulps	%xmm11, %xmm12
	addps	sqrt_coeff+16+__svml_sacos_data_internal(%rip), %xmm10
	mulps	%xmm5, %xmm10
	movups	poly_coeff+32+__svml_sacos_data_internal(%rip), %xmm5
	subps	%xmm10, %xmm2
	mulps	%xmm13, %xmm5
	movups	MOne+__svml_sacos_data_internal(%rip), %xmm7
	andps	%xmm1, %xmm2
	cmpnleps %xmm4, %xmm7
	addps	poly_coeff+48+__svml_sacos_data_internal(%rip), %xmm5
	movmskps %xmm7, %edx
	orps	%xmm2, %xmm6
	addps	%xmm12, %xmm5
	mulps	%xmm13, %xmm5
	pxor	%xmm3, %xmm6
	movups	PiH+__svml_sacos_data_internal(%rip), %xmm7
	andps	%xmm1, %xmm7
	addps	poly_coeff+64+__svml_sacos_data_internal(%rip), %xmm5
	mulps	%xmm13, %xmm5
	andps	%xmm14, %xmm7
	mulps	%xmm6, %xmm5
	andnps	Pi2H+__svml_sacos_data_internal(%rip), %xmm15
	addps	%xmm5, %xmm6
	addps	%xmm15, %xmm7
	addps	%xmm6, %xmm7
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm7

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm7, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm7, 48(%rsp)
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
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm7

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm7

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	acosf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_acosf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sacos_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 SgnBit[4][1];
	__declspec(align(16)) VUINT32 OneHalf[4][1];
	__declspec(align(16)) VUINT32 SmallNorm[4][1];
	__declspec(align(16)) VUINT32 MOne[4][1];
	__declspec(align(16)) VUINT32 Two[4][1];
	__declspec(align(16)) VUINT32 sqrt_coeff[2][4][1];
	__declspec(align(16)) VUINT32 poly_coeff[5][4][1];
	__declspec(align(16)) VUINT32 Pi2H[4][1];
	__declspec(align(16)) VUINT32 PiH[4][1];
} __svml_sacos_data_internal;
#endif
__svml_sacos_data_internal:
	/* SgnBit */
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000
	/* OneHalf */
	.align	16
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* SmallNorm */
	.align	16
	.long	0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000
	/* MOne */
	.align	16
	.long	0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
	/* Two */
	.align	16
	.long	0x40000000, 0x40000000, 0x40000000, 0x40000000
	/* sqrt_coeff[2] */
	.align	16
	.long	0xbdC00004, 0xbdC00004, 0xbdC00004, 0xbdC00004 /* sqrt_coeff2 */
	.long	0x3e800001, 0x3e800001, 0x3e800001, 0x3e800001 /* sqrt_coeff1 */
	/* poly_coeff[5] */
	.align	16
	.long	0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07, 0x3d2EDC07 /* poly_coeff5 */
	.long	0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B, 0x3CC32A6B /* poly_coeff4 */
	.long	0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4, 0x3d3A9AB4 /* poly_coeff3 */
	.long	0x3d997C12, 0x3d997C12, 0x3d997C12, 0x3d997C12 /* poly_coeff2 */
	.long	0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF, 0x3e2AAAFF /* poly_coeff1 */
	/* Pi2H */
	.align	16
	.long	0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB
	/* PiH */
	.align	16
	.long	0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB
	.align	16
	.type	__svml_sacos_data_internal, @object
	.size	__svml_sacos_data_internal, .-__svml_sacos_data_internal
