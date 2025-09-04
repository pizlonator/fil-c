/* Function asinhf vectorized with AVX2.
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
 *   Compute asinh(x) as log(x + sqrt(x*x + 1))
 *
 *   Special cases:
 *
 *   asinh(NaN) = quiet NaN, and raise invalid exception
 *   asinh(INF) = that INF
 *   asinh(0)   = that 0
 *
 */

/* Offsets for data table __svml_sasinh_data_internal
 */
#define SgnMask				0
#define sOne				32
#define sPoly				64
#define iBrkValue			320
#define iOffExpoMask			352
#define sBigThreshold			384
#define sC2				416
#define sC3				448
#define sHalf				480
#define sLargestFinite			512
#define sLittleThreshold		544
#define sSign				576
#define sThirtyOne			608
#define sTopMask8			640
#define XScale				672
#define sLn2				704

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_asinhf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	vmovaps	%ymm0, %ymm9

	/* Load the constant 1 and a sign mask */
	vmovups	sOne+__svml_sasinh_data_internal(%rip), %ymm8

	/* No need to split X when FMA is available in hardware. */
	vmulps	%ymm9, %ymm9, %ymm5
	vmovups	sTopMask8+__svml_sasinh_data_internal(%rip), %ymm1

	/*
	 * Finally, express Y + W = X^2 + 1 accurately where Y has <= 8 bits.
	 * If |X| <= 1 then |XHi| <= 1 and so |X2Hi| <= 1, so we can treat 1
	 * as the dominant component in the compensated summation. Otherwise,
	 * if |X| >= 1, then since X2Hi only has 22 significant bits, the basic
	 * addition will be exact anyway until we get to |X| >= 2^24. But by
	 * that time the log function is well-conditioned enough that the
	 * rounding error doesn't matter. Hence we can treat 1 as dominant even
	 * if it literally isn't.
	 */
	vaddps	%ymm5, %ymm8, %ymm13
	vandps	%ymm1, %ymm13, %ymm2
	vmovaps	%ymm9, %ymm4
	vsubps	%ymm13, %ymm8, %ymm11
	vsubps	%ymm2, %ymm13, %ymm15

	/*
	 * Compute R = 1/sqrt(Y + W) * (1 + d)
	 * Force R to <= 8 significant bits.
	 * This means that R * Y and R^2 * Y are exactly representable.
	 */
	vrsqrtps %ymm2, %ymm0
	vfmsub213ps %ymm5, %ymm9, %ymm4
	vaddps	%ymm11, %ymm5, %ymm12

	/*
	 * Get the absolute value of the input, since we will exploit antisymmetry
	 * and mostly assume X >= 0 in the core computation
	 */
	vandps	SgnMask+__svml_sasinh_data_internal(%rip), %ymm9, %ymm6

	/*
	 * Check whether the input is finite, by checking |X| <= MaxFloat
	 * Otherwise set the rangemask so that the callout will get used.
	 * Note that this will also use the callout for NaNs since not(NaN <= MaxFloat)
	 */
	vcmpnle_uqps sLargestFinite+__svml_sasinh_data_internal(%rip), %ymm6, %ymm10
	vaddps	%ymm12, %ymm4, %ymm14

	/*
	 * Unfortunately, we can still be in trouble if |X| <= 2^-5, since
	 * the absolute error 2^-(7+24)-ish in sqrt(1 + X^2) gets scaled up
	 * by 1/X and comes close to our threshold. Hence if |X| <= 2^-4,
	 * perform an alternative computation
	 * sqrt(1 + X^2) - 1 = X^2/2 - X^4/8 + X^6/16
	 * X2 = X^2
	 */
	vaddps	%ymm4, %ymm5, %ymm4

	/*
	 * The following computation can go wrong for very large X, basically
	 * because X^2 overflows. But for large X we have
	 * asinh(X) / log(2 X) - 1 =~= 1/(4 * X^2), so for X >= 2^30
	 * we can just later stick X back into the log and tweak up the exponent.
	 * Actually we scale X by 2^-30 and tweak the exponent up by 31,
	 * to stay in the safe range for the later log computation.
	 * Compute a flag now telling us when do do this.
	 */
	vcmplt_oqps sBigThreshold+__svml_sasinh_data_internal(%rip), %ymm6, %ymm7
	vaddps	%ymm15, %ymm14, %ymm3

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
	vmovups	sC3+__svml_sasinh_data_internal(%rip), %ymm12
	vmovmskps %ymm10, %edx
	vandps	%ymm1, %ymm0, %ymm10

	/*
	 * Compute S = (Y/sqrt(Y + W)) * (1 + d)
	 * and T = (W/sqrt(Y + W)) * (1 + d)
	 * so that S + T = sqrt(Y + W) * (1 + d)
	 * S is exact, and the rounding error in T is OK.
	 */
	vmulps	%ymm10, %ymm2, %ymm15
	vmulps	%ymm3, %ymm10, %ymm14
	vmovups	sHalf+__svml_sasinh_data_internal(%rip), %ymm3
	vsubps	%ymm8, %ymm15, %ymm0

	/*
	 * Obtain sqrt(1 + X^2) - 1 in two pieces
	 * sqrt(1 + X^2) - 1
	 * = sqrt(Y + W) - 1
	 * = (S + T) * (1 + Corr) - 1
	 * = [S - 1] + [T + (S + T) * Corr]
	 * We need a compensated summation for the last part. We treat S - 1
	 * as the larger part; it certainly is until about X < 2^-4, and in that
	 * case, the error is affordable since X dominates over sqrt(1 + X^2) - 1
	 * Final sum is dTmp5 (hi) + dTmp7 (lo)
	 */
	vaddps	%ymm14, %ymm15, %ymm13

	/*
	 * Compute e = -(2 * d + d^2)
	 * The first FMR is exact, and the rounding error in the other is acceptable
	 * since d and e are ~ 2^-8
	 */
	vmovaps	%ymm8, %ymm11
	vfnmadd231ps %ymm15, %ymm10, %ymm11
	vfnmadd231ps %ymm14, %ymm10, %ymm11
	vfmadd213ps sC2+__svml_sasinh_data_internal(%rip), %ymm11, %ymm12
	vfmadd213ps %ymm3, %ymm11, %ymm12
	vmulps	%ymm12, %ymm11, %ymm1

	/* Now multiplex the two possible computations */
	vcmple_oqps sLittleThreshold+__svml_sasinh_data_internal(%rip), %ymm6, %ymm11
	vfmadd213ps %ymm14, %ymm13, %ymm1
	vaddps	%ymm0, %ymm1, %ymm2
	vsubps	%ymm2, %ymm0, %ymm10

	/* sX2over2 = X^2/2 */
	vmulps	%ymm4, %ymm3, %ymm0
	vaddps	%ymm10, %ymm1, %ymm1

	/* sX4over4 = X^4/4 */
	vmulps	%ymm0, %ymm0, %ymm5

	/* sX46 = -X^4/4 + X^6/8 */
	vfmsub231ps %ymm0, %ymm5, %ymm5

	/* sX46over2 = -X^4/8 + x^6/16 */
	vmulps	%ymm5, %ymm3, %ymm3
	vaddps	%ymm3, %ymm0, %ymm5
	vblendvps %ymm11, %ymm5, %ymm2, %ymm2
	vsubps	%ymm5, %ymm0, %ymm4

	/*
	 * Now do another compensated sum to add |X| + [sqrt(1 + X^2) - 1].
	 * It's always safe to assume |X| is larger.
	 * This is the final 2-part argument to the log1p function
	 */
	vaddps	%ymm2, %ymm6, %ymm14

	/*
	 * Now resume the main code.
	 * reduction: compute r, n
	 */
	vmovups	iBrkValue+__svml_sasinh_data_internal(%rip), %ymm5
	vaddps	%ymm4, %ymm3, %ymm10

	/*
	 * Now we feed into the log1p code, using H in place of _VARG1 and
	 * also adding L into Xl.
	 * compute 1+x as high, low parts
	 */
	vmaxps	%ymm14, %ymm8, %ymm15
	vminps	%ymm14, %ymm8, %ymm0
	vblendvps %ymm11, %ymm10, %ymm1, %ymm12
	vsubps	%ymm14, %ymm6, %ymm1
	vaddps	%ymm0, %ymm15, %ymm3

	/* Now multiplex to the case X = 2^-30 * input, Xl = sL = 0 in the "big" case. */
	vmulps	XScale+__svml_sasinh_data_internal(%rip), %ymm6, %ymm6
	vaddps	%ymm1, %ymm2, %ymm13
	vsubps	%ymm3, %ymm15, %ymm15
	vaddps	%ymm13, %ymm12, %ymm1
	vaddps	%ymm15, %ymm0, %ymm2
	vblendvps %ymm7, %ymm3, %ymm6, %ymm0
	vaddps	%ymm2, %ymm1, %ymm4
	vpsubd	%ymm5, %ymm0, %ymm1
	vpsrad	$23, %ymm1, %ymm6
	vpand	iOffExpoMask+__svml_sasinh_data_internal(%rip), %ymm1, %ymm2
	vmovups	sPoly+224+__svml_sasinh_data_internal(%rip), %ymm1
	vpslld	$23, %ymm6, %ymm10
	vpaddd	%ymm5, %ymm2, %ymm13
	vcvtdq2ps %ymm6, %ymm0
	vpsubd	%ymm10, %ymm8, %ymm12

	/* polynomial evaluation */
	vsubps	%ymm8, %ymm13, %ymm8

	/* Add 31 to the exponent in the "large" case to get log(2 * input) */
	vaddps	sThirtyOne+__svml_sasinh_data_internal(%rip), %ymm0, %ymm3
	vandps	%ymm7, %ymm4, %ymm11
	vmulps	%ymm12, %ymm11, %ymm14
	vblendvps %ymm7, %ymm0, %ymm3, %ymm0
	vaddps	%ymm8, %ymm14, %ymm2
	vfmadd213ps sPoly+192+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vfmadd213ps sPoly+160+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vfmadd213ps sPoly+128+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vfmadd213ps sPoly+96+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vfmadd213ps sPoly+64+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vfmadd213ps sPoly+32+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vfmadd213ps sPoly+__svml_sasinh_data_internal(%rip), %ymm2, %ymm1
	vmulps	%ymm1, %ymm2, %ymm4
	vfmadd213ps %ymm2, %ymm2, %ymm4

	/* final reconstruction */
	vfmadd132ps sLn2+__svml_sasinh_data_internal(%rip), %ymm4, %ymm0

	/* Finally, reincorporate the original sign. */
	vandps	sSign+__svml_sasinh_data_internal(%rip), %ymm9, %ymm7
	vxorps	%ymm0, %ymm7, %ymm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm9

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
	vmovups	%ymm9, 32(%rsp)
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
	call	asinhf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_asinhf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_sasinh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 SgnMask[8][1];
	__declspec(align(32)) VUINT32 sOne[8][1];
	__declspec(align(32)) VUINT32 sPoly[8][8][1];
	__declspec(align(32)) VUINT32 iBrkValue[8][1];
	__declspec(align(32)) VUINT32 iOffExpoMask[8][1];
	__declspec(align(32)) VUINT32 sBigThreshold[8][1];
	__declspec(align(32)) VUINT32 sC2[8][1];
	__declspec(align(32)) VUINT32 sC3[8][1];
	__declspec(align(32)) VUINT32 sHalf[8][1];
	__declspec(align(32)) VUINT32 sLargestFinite[8][1];
	__declspec(align(32)) VUINT32 sLittleThreshold[8][1];
	__declspec(align(32)) VUINT32 sSign[8][1];
	__declspec(align(32)) VUINT32 sThirtyOne[8][1];
	__declspec(align(32)) VUINT32 sTopMask8[8][1];
	__declspec(align(32)) VUINT32 XScale[8][1];
	__declspec(align(32)) VUINT32 sLn2[8][1];
} __svml_sasinh_data_internal;
#endif
__svml_sasinh_data_internal:
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
	/* sLittleThreshold */
	.align	32
	.long	0x3D800000, 0x3D800000, 0x3D800000, 0x3D800000, 0x3D800000, 0x3D800000, 0x3D800000, 0x3D800000
	/* sSign */
	.align	32
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
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
	.type	__svml_sasinh_data_internal, @object
	.size	__svml_sasinh_data_internal, .-__svml_sasinh_data_internal
