/* Function asinf vectorized with SSE4.
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
#define OneHalf				16
#define SmallNorm			32
#define One				48
#define Two				64
#define sqrt_coeff			80
#define poly_coeff			112
#define Pi2H				192

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_asinf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm2
	movups	__svml_sasin_data_internal(%rip), %xmm1
	movups	OneHalf+__svml_sasin_data_internal(%rip), %xmm5

	/* x = |arg| */
	movaps	%xmm1, %xmm0
	andps	%xmm2, %xmm0

	/* Y = 0.5 - 0.5*x */
	movaps	%xmm5, %xmm3
	mulps	%xmm0, %xmm3
	movaps	%xmm5, %xmm8

	/* x^2 */
	movaps	%xmm0, %xmm14
	movaps	%xmm0, %xmm15
	mulps	%xmm0, %xmm14
	subps	%xmm3, %xmm8
	cmpnltps %xmm5, %xmm15

	/* SQ ~ -2*sqrt(Y) */
	rsqrtps	%xmm8, %xmm6
	minps	%xmm8, %xmm14
	movaps	%xmm8, %xmm9
	movaps	%xmm14, %xmm10
	cmpltps	SmallNorm+__svml_sasin_data_internal(%rip), %xmm9
	mulps	%xmm14, %xmm10
	addps	%xmm8, %xmm8
	andnps	%xmm6, %xmm9
	movaps	%xmm15, %xmm3
	movaps	%xmm9, %xmm7
	andnps	%xmm0, %xmm3
	mulps	%xmm9, %xmm7
	andnps	%xmm2, %xmm1
	mulps	%xmm8, %xmm9
	mulps	%xmm7, %xmm8

	/* polynomial */
	movups	poly_coeff+__svml_sasin_data_internal(%rip), %xmm11
	mulps	%xmm14, %xmm11
	subps	Two+__svml_sasin_data_internal(%rip), %xmm8
	movups	poly_coeff+32+__svml_sasin_data_internal(%rip), %xmm12
	mulps	%xmm14, %xmm12
	addps	poly_coeff+16+__svml_sasin_data_internal(%rip), %xmm11
	mulps	%xmm10, %xmm11
	addps	poly_coeff+48+__svml_sasin_data_internal(%rip), %xmm12
	movups	sqrt_coeff+__svml_sasin_data_internal(%rip), %xmm13
	addps	%xmm11, %xmm12
	mulps	%xmm8, %xmm13
	mulps	%xmm9, %xmm8
	mulps	%xmm14, %xmm12
	addps	sqrt_coeff+16+__svml_sasin_data_internal(%rip), %xmm13
	addps	poly_coeff+64+__svml_sasin_data_internal(%rip), %xmm12
	mulps	%xmm8, %xmm13
	mulps	%xmm12, %xmm14
	subps	%xmm9, %xmm13
	andps	%xmm15, %xmm13
	orps	%xmm13, %xmm3
	mulps	%xmm3, %xmm14
	movups	One+__svml_sasin_data_internal(%rip), %xmm4
	addps	%xmm14, %xmm3
	cmpltps	%xmm0, %xmm4
	movups	Pi2H+__svml_sasin_data_internal(%rip), %xmm0
	andps	%xmm15, %xmm0
	movmskps %xmm4, %edx
	addps	%xmm3, %xmm0
	pxor	%xmm1, %xmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm2

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
	movups	%xmm2, 32(%rsp)
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
	movss	32(%rsp, %r14, 4), %xmm0
	call	asinf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_asinf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sasin_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 AbsMask[4][1];
	__declspec(align(16)) VUINT32 OneHalf[4][1];
	__declspec(align(16)) VUINT32 SmallNorm[4][1];
	__declspec(align(16)) VUINT32 One[4][1];
	__declspec(align(16)) VUINT32 Two[4][1];
	__declspec(align(16)) VUINT32 sqrt_coeff[2][4][1];
	__declspec(align(16)) VUINT32 poly_coeff[5][4][1];
	__declspec(align(16)) VUINT32 Pi2H[4][1];
} __svml_sasin_data_internal;
#endif
__svml_sasin_data_internal:
	/* AbsMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* OneHalf */
	.align	16
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* SmallNorm */
	.align	16
	.long	0x2f800000, 0x2f800000, 0x2f800000, 0x2f800000
	/* One */
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
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
	.align	16
	.type	__svml_sasin_data_internal, @object
	.size	__svml_sasin_data_internal, .-__svml_sasin_data_internal
