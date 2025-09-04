/* Function acoshf vectorized with AVX2.
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
 *   Compute acosh(x) as log(x + sqrt(x*x - 1))
 *
 *   Special cases:
 *
 *   acosh(NaN)  = quiet NaN, and raise invalid exception
 *   acosh(-INF) = NaN
 *   acosh(+INF) = +INF
 *   acosh(x)    = NaN if x < 1
 *   acosh(1)    = +0
 *
 */

/* Offsets for data table __svml_sacosh_data_internal
 */
#define sOne				0
#define sPoly				32
#define iBrkValue			288
#define iOffExpoMask			320
#define sBigThreshold			352
#define sC2				384
#define sC3				416
#define sHalf				448
#define sLargestFinite			480
#define sThirtyOne			512
#define sTopMask8			544
#define XScale				576
#define sLn2				608

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_acoshf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/* Load constants, always including One = 1 */
	vmovups	sOne+__svml_sacosh_data_internal(%rip), %ymm2

	/* Finally, express Y + W = U * V accurately where Y has <= 8 bits */
	vmovups	sTopMask8+__svml_sacosh_data_internal(%rip), %ymm9

	/*
	 * Now       1 / (1 + d)
	 * = 1 / (1 + (sqrt(1 - e) - 1))
	 * = 1 / sqrt(1 - e)
	 * = 1 + 1/2 * e + 3/8 * e^2 + 5/16 * e^3 + 35/128 * e^4 + ...
	 * So compute the first three nonconstant terms of that, so that
	 * we have a relative correction (1 + Corr) to apply to S etc.
	 * C1 = 1/2
	 * C2 = 3/8
	 * C3 = 5/16
	 */
	vmovups	sC3+__svml_sacosh_data_internal(%rip), %ymm14
	vmovaps	%ymm0, %ymm3
	vmovaps	%ymm2, %ymm7
	vfmsub231ps %ymm3, %ymm3, %ymm7

	/*
	 * Check that 1 < X < +inf; otherwise go to the callout function.
	 * We need the callout for X = 1 to avoid division by zero below.
	 * This test ensures that callout handles NaN and either infinity.
	 */
	vcmpnle_uqps sLargestFinite+__svml_sacosh_data_internal(%rip), %ymm3, %ymm4
	vcmpngt_uqps %ymm2, %ymm3, %ymm5

	/*
	 * The following computation can go wrong for very large X, e.g.
	 * the X^2 - 1 = U * V can overflow. But for large X we have
	 * acosh(X) / log(2 X) - 1 =~= 1/(4 * X^2), so for X >= 2^30
	 * we can just later stick X back into the log and tweak up the exponent.
	 * Actually we scale X by 2^-30 and tweak the exponent up by 31,
	 * to stay in the safe range for the later log computation.
	 * Compute a flag now telling us when to do this.
	 */
	vcmplt_oqps sBigThreshold+__svml_sacosh_data_internal(%rip), %ymm3, %ymm1
	vandps	%ymm9, %ymm7, %ymm10

	/*
	 * Compute R = 1/sqrt(Y + W) * (1 + d)
	 * Force R to <= 8 significant bits.
	 * This means that R * Y and R^2 * Y are exactly representable.
	 */
	vrsqrtps %ymm10, %ymm8
	vsubps	%ymm10, %ymm7, %ymm11
	vandps	%ymm9, %ymm8, %ymm12

	/*
	 * Compute S = (Y/sqrt(Y + W)) * (1 + d)
	 * and T = (W/sqrt(Y + W)) * (1 + d)
	 * so that S + T = sqrt(Y + W) * (1 + d)
	 * S is exact, and the rounding error in T is OK.
	 */
	vmulps	%ymm12, %ymm10, %ymm15
	vmulps	%ymm11, %ymm12, %ymm0

	/* Now multiplex to the case X = 2^-30 * input, Xl = 0 in the "big" case. */
	vmulps	XScale+__svml_sacosh_data_internal(%rip), %ymm3, %ymm11

	/*
	 * Compute e = -(2 * d + d^2)
	 * The first FMR is exact, and the rounding error in the other is acceptable
	 * since d and e are ~ 2^-8
	 */
	vmovaps	%ymm2, %ymm13
	vfnmadd231ps %ymm15, %ymm12, %ymm13
	vfnmadd231ps %ymm0, %ymm12, %ymm13
	vfmadd213ps sC2+__svml_sacosh_data_internal(%rip), %ymm13, %ymm14
	vfmadd213ps sHalf+__svml_sacosh_data_internal(%rip), %ymm13, %ymm14
	vmulps	%ymm14, %ymm13, %ymm7
	vorps	%ymm5, %ymm4, %ymm6

	/*
	 * For low-accuracy versions, the computation can be done
	 * just as U + ((S + T) + (S + T) * Corr)
	 */
	vaddps	%ymm0, %ymm15, %ymm5

	/* sU is needed later on */
	vsubps	%ymm2, %ymm3, %ymm4
	vfmadd213ps %ymm5, %ymm7, %ymm5
	vmovmskps %ymm6, %edx
	vaddps	%ymm5, %ymm4, %ymm6

	/*
	 * Now resume the main code.
	 * reduction: compute r, n
	 */
	vmovups	iBrkValue+__svml_sacosh_data_internal(%rip), %ymm4

	/*
	 * Now we feed into the log1p code, using H in place of _VARG1 and
	 * also adding L into Xl.
	 * compute 1+x as high, low parts
	 */
	vmaxps	%ymm6, %ymm2, %ymm8
	vminps	%ymm6, %ymm2, %ymm9
	vaddps	%ymm9, %ymm8, %ymm12
	vblendvps %ymm1, %ymm12, %ymm11, %ymm14
	vsubps	%ymm12, %ymm8, %ymm10
	vpsubd	%ymm4, %ymm14, %ymm15
	vaddps	%ymm10, %ymm9, %ymm13
	vpand	iOffExpoMask+__svml_sacosh_data_internal(%rip), %ymm15, %ymm14
	vpsrad	$23, %ymm15, %ymm15
	vpaddd	%ymm4, %ymm14, %ymm8
	vpslld	$23, %ymm15, %ymm5
	vmovups	sPoly+224+__svml_sacosh_data_internal(%rip), %ymm4
	vcvtdq2ps %ymm15, %ymm0
	vpsubd	%ymm5, %ymm2, %ymm7

	/* polynomial evaluation */
	vsubps	%ymm2, %ymm8, %ymm2

	/* Add 31 to the exponent in the "large" case to get log(2 * input) */
	vaddps	sThirtyOne+__svml_sacosh_data_internal(%rip), %ymm0, %ymm5
	vandps	%ymm1, %ymm13, %ymm6
	vmulps	%ymm7, %ymm6, %ymm9
	vblendvps %ymm1, %ymm0, %ymm5, %ymm0
	vaddps	%ymm2, %ymm9, %ymm2
	vfmadd213ps sPoly+192+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vfmadd213ps sPoly+160+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vfmadd213ps sPoly+128+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vfmadd213ps sPoly+96+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vfmadd213ps sPoly+64+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vfmadd213ps sPoly+32+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vfmadd213ps sPoly+__svml_sacosh_data_internal(%rip), %ymm2, %ymm4
	vmulps	%ymm4, %ymm2, %ymm6
	vfmadd213ps %ymm2, %ymm2, %ymm6

	/* final reconstruction */
	vfmadd132ps sLn2+__svml_sacosh_data_internal(%rip), %ymm6, %ymm0
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
	call	acoshf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_acoshf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sacosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 sOne[8][1];
	__declspec(align(32)) VUINT32 sPoly[8][8][1];
	__declspec(align(32)) VUINT32 iBrkValue[8][1];
	__declspec(align(32)) VUINT32 iOffExpoMask[8][1];
	__declspec(align(32)) VUINT32 sBigThreshold[8][1];
	__declspec(align(32)) VUINT32 sC2[8][1];
	__declspec(align(32)) VUINT32 sC3[8][1];
	__declspec(align(32)) VUINT32 sHalf[8][1];
	__declspec(align(32)) VUINT32 sLargestFinite[8][1];
	__declspec(align(32)) VUINT32 sThirtyOne[8][1];
	__declspec(align(32)) VUINT32 sTopMask8[8][1];
	__declspec(align(32)) VUINT32 XScale[8][1];
	__declspec(align(32)) VUINT32 sLn2[8][1];
} __svml_sacosh_data_internal;
#endif
__svml_sacosh_data_internal:
	/* sOne = SP 1.0 */
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
	/* iBrkValue = SP 2/3 */
	.align	32
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	32
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sBigThreshold */
	.align	32
	.long	0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000
	/* sC2 */
	.align	32
	.long	0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000
	/* sC3 */
	.align	32
	.long	0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000
	/* sHalf */
	.align	32
	.long	0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000
	/* sLargestFinite */
	.align	32
	.long	0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF
	/* sThirtyOne */
	.align	32
	.long	0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000
	/* sTopMask8 */
	.align	32
	.long	0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000
	/* XScale */
	.align	32
	.long	0x30800000, 0x30800000, 0x30800000, 0x30800000, 0x30800000, 0x30800000, 0x30800000, 0x30800000
	/* sLn2 = SP ln(2) */
	.align	32
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.align	32
	.type	__svml_sacosh_data_internal, @object
	.size	__svml_sacosh_data_internal, .-__svml_sacosh_data_internal
