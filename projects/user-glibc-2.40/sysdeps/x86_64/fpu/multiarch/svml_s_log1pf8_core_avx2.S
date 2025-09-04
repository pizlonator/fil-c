/* Function log1pf vectorized with AVX2.
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
 *    1+x = 2^k*(xh + xl) is computed in high-low parts; xh in [1, 2)
 *    Get short reciprocal approximation Rcp ~ 1/xh
 *    R = (Rcp*xh - 1.0) + Rcp*xl
 *    log1p(x) = k*log(2.0) - log(Rcp) + poly(R)
 *       log(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_slog1p_data_internal
 */
#define SgnMask				0
#define sOne				32
#define sPoly				64
#define iHiDelta			320
#define iLoRange			352
#define iBrkValue			384
#define iOffExpoMask			416
#define sLn2				448

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_log1pf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	vmovups	sOne+__svml_slog1p_data_internal(%rip), %ymm2

	/* reduction: compute r, n */
	vmovups	iBrkValue+__svml_slog1p_data_internal(%rip), %ymm13
	vmovups	SgnMask+__svml_slog1p_data_internal(%rip), %ymm4
	vmovups	iLoRange+__svml_slog1p_data_internal(%rip), %ymm8
	vmovaps	%ymm0, %ymm3

	/* compute 1+x as high, low parts */
	vmaxps	%ymm3, %ymm2, %ymm5
	vminps	%ymm3, %ymm2, %ymm6
	vaddps	%ymm6, %ymm5, %ymm10
	vpsubd	%ymm13, %ymm10, %ymm11

	/* check argument value ranges */
	vpaddd	iHiDelta+__svml_slog1p_data_internal(%rip), %ymm10, %ymm9
	vsubps	%ymm10, %ymm5, %ymm7
	vpsrad	$23, %ymm11, %ymm14
	vpand	iOffExpoMask+__svml_slog1p_data_internal(%rip), %ymm11, %ymm12
	vpslld	$23, %ymm14, %ymm15
	vcvtdq2ps %ymm14, %ymm0
	vpsubd	%ymm15, %ymm2, %ymm14
	vandnps	%ymm3, %ymm4, %ymm1
	vaddps	%ymm7, %ymm6, %ymm4
	vpaddd	%ymm13, %ymm12, %ymm6
	vmulps	%ymm4, %ymm14, %ymm7

	/* polynomial evaluation */
	vsubps	%ymm2, %ymm6, %ymm2
	vpcmpgtd %ymm9, %ymm8, %ymm5
	vmovups	sPoly+224+__svml_slog1p_data_internal(%rip), %ymm8
	vaddps	%ymm2, %ymm7, %ymm9
	vfmadd213ps sPoly+192+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vfmadd213ps sPoly+160+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vfmadd213ps sPoly+128+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vfmadd213ps sPoly+96+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vfmadd213ps sPoly+64+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vfmadd213ps sPoly+32+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vfmadd213ps sPoly+__svml_slog1p_data_internal(%rip), %ymm9, %ymm8
	vmulps	%ymm8, %ymm9, %ymm10
	vfmadd213ps %ymm9, %ymm9, %ymm10

	/* final reconstruction */
	vfmadd132ps sLn2+__svml_slog1p_data_internal(%rip), %ymm10, %ymm0

	/* combine and get argument value range mask */
	vmovmskps %ymm5, %edx
	vorps	%ymm1, %ymm0, %ymm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm3

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
	vmovups	%ymm3, 32(%rsp)
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
	call	log1pf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_log1pf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_slog1p_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 SgnMask[8][1];
	__declspec(align(32)) VUINT32 sOne[8][1];
	__declspec(align(32)) VUINT32 sPoly[8][8][1];
	__declspec(align(32)) VUINT32 iHiDelta[8][1];
	__declspec(align(32)) VUINT32 iLoRange[8][1];
	__declspec(align(32)) VUINT32 iBrkValue[8][1];
	__declspec(align(32)) VUINT32 iOffExpoMask[8][1];
	__declspec(align(32)) VUINT32 sLn2[8][1];
} __svml_slog1p_data_internal;
#endif
__svml_slog1p_data_internal:
	/* SgnMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* sOne = SP 1.0 */
	.align	32
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* sPoly[] = SP polynomial */
	.align	32
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000 /* -5.0000000000000000000000000e-01 P0 */
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94 /* 3.3333265781402587890625000e-01 P1 */
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e /* -2.5004237890243530273437500e-01 P2 */
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190 /* 2.0007920265197753906250000e-01 P3 */
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37 /* -1.6472326219081878662109375e-01 P4 */
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12 /* 1.4042308926582336425781250e-01 P5 */
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3 /* -1.5122179687023162841796875e-01 P6 */
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed /* 1.3820238411426544189453125e-01 P7 */
	/* iHiDelta = SP 80000000-7f000000 */
	.align	32
	.long	0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000
	/* iLoRange = SP 00800000+iHiDelta */
	.align	32
	.long	0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000
	/* iBrkValue = SP 2/3 */
	.align	32
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	32
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sLn2 = SP ln(2) */
	.align	32
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.align	32
	.type	__svml_slog1p_data_internal, @object
	.size	__svml_slog1p_data_internal, .-__svml_slog1p_data_internal
