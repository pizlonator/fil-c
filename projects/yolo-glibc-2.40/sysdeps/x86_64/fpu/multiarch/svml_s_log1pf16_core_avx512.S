/* Function log1pf vectorized with AVX-512.
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
#define sOne				64
#define sPoly_1				128
#define sPoly_2				192
#define sPoly_3				256
#define sPoly_4				320
#define sPoly_5				384
#define sPoly_6				448
#define sPoly_7				512
#define sPoly_8				576
#define iHiDelta			640
#define iLoRange			704
#define iBrkValue			768
#define iOffExpoMask			832
#define sLn2				896

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_log1pf_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	sOne+__svml_slog1p_data_internal(%rip), %zmm2

	/* reduction: compute r, n */
	vmovups	iBrkValue+__svml_slog1p_data_internal(%rip), %zmm12
	vmovups	SgnMask+__svml_slog1p_data_internal(%rip), %zmm4
	vmovaps	%zmm0, %zmm3

	/* compute 1+x as high, low parts */
	vmaxps	{sae}, %zmm3, %zmm2, %zmm5
	vminps	{sae}, %zmm3, %zmm2, %zmm7
	vandnps	%zmm3, %zmm4, %zmm1
	vpternlogd $255, %zmm4, %zmm4, %zmm4
	vaddps	{rn-sae}, %zmm7, %zmm5, %zmm9
	vpsubd	%zmm12, %zmm9, %zmm10
	vsubps	{rn-sae}, %zmm9, %zmm5, %zmm6

	/* check argument value ranges */
	vpaddd	iHiDelta+__svml_slog1p_data_internal(%rip), %zmm9, %zmm8
	vpsrad	$23, %zmm10, %zmm13
	vmovups	sPoly_5+__svml_slog1p_data_internal(%rip), %zmm9
	vpcmpd	$5, iLoRange+__svml_slog1p_data_internal(%rip), %zmm8, %k1
	vpslld	$23, %zmm13, %zmm14
	vaddps	{rn-sae}, %zmm7, %zmm6, %zmm15
	vcvtdq2ps {rn-sae}, %zmm13, %zmm0
	vpsubd	%zmm14, %zmm2, %zmm13
	vmovups	sPoly_8+__svml_slog1p_data_internal(%rip), %zmm7
	vmovups	sPoly_1+__svml_slog1p_data_internal(%rip), %zmm14
	vmulps	{rn-sae}, %zmm13, %zmm15, %zmm6
	vpandd	iOffExpoMask+__svml_slog1p_data_internal(%rip), %zmm10, %zmm11
	vpaddd	%zmm12, %zmm11, %zmm5
	vmovups	sPoly_4+__svml_slog1p_data_internal(%rip), %zmm10
	vmovups	sPoly_3+__svml_slog1p_data_internal(%rip), %zmm11
	vmovups	sPoly_2+__svml_slog1p_data_internal(%rip), %zmm12

	/* polynomial evaluation */
	vsubps	{rn-sae}, %zmm2, %zmm5, %zmm2
	vaddps	{rn-sae}, %zmm6, %zmm2, %zmm15
	vmovups	sPoly_7+__svml_slog1p_data_internal(%rip), %zmm2
	vfmadd231ps {rn-sae}, %zmm15, %zmm7, %zmm2
	vpandnd	%zmm8, %zmm8, %zmm4{%k1}
	vmovups	sPoly_6+__svml_slog1p_data_internal(%rip), %zmm8

	/* combine and get argument value range mask */
	vptestmd %zmm4, %zmm4, %k0
	vfmadd213ps {rn-sae}, %zmm8, %zmm15, %zmm2
	kmovw	%k0, %edx
	vfmadd213ps {rn-sae}, %zmm9, %zmm15, %zmm2
	vfmadd213ps {rn-sae}, %zmm10, %zmm15, %zmm2
	vfmadd213ps {rn-sae}, %zmm11, %zmm15, %zmm2
	vfmadd213ps {rn-sae}, %zmm12, %zmm15, %zmm2
	vfmadd213ps {rn-sae}, %zmm14, %zmm15, %zmm2
	vmulps	{rn-sae}, %zmm15, %zmm2, %zmm4
	vfmadd213ps {rn-sae}, %zmm15, %zmm15, %zmm4

	/* final reconstruction */
	vmovups	sLn2+__svml_slog1p_data_internal(%rip), %zmm15
	vfmadd213ps {rn-sae}, %zmm4, %zmm15, %zmm0
	vorps	%zmm1, %zmm0, %zmm0
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
	call	log1pf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_log1pf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_slog1p_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 SgnMask[16][1];
	__declspec(align(64)) VUINT32 sOne[16][1];
	__declspec(align(64)) VUINT32 sPoly[8][16][1];
	__declspec(align(64)) VUINT32 iHiDelta[16][1];
	__declspec(align(64)) VUINT32 iLoRange[16][1];
	__declspec(align(64)) VUINT32 iBrkValue[16][1];
	__declspec(align(64)) VUINT32 iOffExpoMask[16][1];
	__declspec(align(64)) VUINT32 sLn2[16][1];
} __svml_slog1p_data_internal;
#endif
__svml_slog1p_data_internal:
	/* SgnMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* sOne = SP 1.0 */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* sPoly[] = SP polynomial */
	.align	64
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000 /* -5.0000000000000000000000000e-01 P0 */
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94 /* 3.3333265781402587890625000e-01 P1 */
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e /* -2.5004237890243530273437500e-01 P2 */
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190 /* 2.0007920265197753906250000e-01 P3 */
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37 /* -1.6472326219081878662109375e-01 P4 */
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12 /* 1.4042308926582336425781250e-01 P5 */
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3 /* -1.5122179687023162841796875e-01 P6 */
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed /* 1.3820238411426544189453125e-01 P7 */
	/* iHiDelta = SP 80000000-7f000000 */
	.align	64
	.long	0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000
	/* iLoRange = SP 00800000+iHiDelta */
	.align	64
	.long	0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000, 0x01800000
	/* iBrkValue = SP 2/3 */
	.align	64
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	64
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sLn2 = SP ln(2) */
	.align	64
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.align	64
	.type	__svml_slog1p_data_internal, @object
	.size	__svml_slog1p_data_internal, .-__svml_slog1p_data_internal
