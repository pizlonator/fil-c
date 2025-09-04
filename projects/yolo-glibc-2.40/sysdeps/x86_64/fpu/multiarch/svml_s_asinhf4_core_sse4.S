/* Function asinhf vectorized with SSE4.
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
#define sOne				16
#define sPoly				32
#define iBrkValue			160
#define iOffExpoMask			176
#define sBigThreshold			192
#define sC2				208
#define sC3				224
#define sHalf				240
#define sLargestFinite			256
#define sLittleThreshold		272
#define sSign				288
#define sThirtyOne			304
#define sTopMask11			320
#define sTopMask8			336
#define XScale				352
#define sLn2				368

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_asinhf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm8

	/*
	 * Split X into high and low parts, XHi (<= 11 bits) and XLo (<= 13 bits)
	 * We could use either X or |X| here, but it doesn't seem to matter
	 */
	movups	sTopMask11+__svml_sasinh_data_internal(%rip), %xmm10
	movaps	%xmm8, %xmm2
	andps	%xmm8, %xmm10

	/*
	 * Compute X^2 = (XHi + XLo)^2 = XHi^2 + XLo * (X + XHi)
	 * The two parts are shifted off by around 11 bits. So even though
	 * the low bit will not in general be exact, it's near enough
	 */
	movaps	%xmm10, %xmm3
	subps	%xmm10, %xmm2
	mulps	%xmm10, %xmm3
	addps	%xmm8, %xmm10

	/* Load the constant 1 and a sign mask */
	movups	sOne+__svml_sasinh_data_internal(%rip), %xmm7

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
	movaps	%xmm7, %xmm11
	movaps	%xmm7, %xmm4
	movups	sTopMask8+__svml_sasinh_data_internal(%rip), %xmm12
	addps	%xmm3, %xmm11
	mulps	%xmm10, %xmm2
	subps	%xmm11, %xmm4
	movaps	%xmm12, %xmm0
	addps	%xmm3, %xmm4

	/*
	 * Unfortunately, we can still be in trouble if |X| <= 2^-5, since
	 * the absolute error 2^-(7+24)-ish in sqrt(1 + X^2) gets scaled up
	 * by 1/X and comes close to our threshold. Hence if |X| <= 2^-4,
	 * perform an alternative computation
	 * sqrt(1 + X^2) - 1 = X^2/2 - X^4/8 + X^6/16
	 * X2 = X^2
	 */
	addps	%xmm2, %xmm3
	addps	%xmm2, %xmm4
	andps	%xmm11, %xmm0

	/*
	 * Compute R = 1/sqrt(Y + W) * (1 + d)
	 * Force R to <= 8 significant bits.
	 * This means that R * Y and R^2 * Y are exactly representable.
	 */
	rsqrtps	%xmm0, %xmm14
	subps	%xmm0, %xmm11
	andps	%xmm12, %xmm14
	addps	%xmm11, %xmm4

	/*
	 * Compute S = (Y/sqrt(Y + W)) * (1 + d)
	 * and T = (W/sqrt(Y + W)) * (1 + d)
	 * so that S + T = sqrt(Y + W) * (1 + d)
	 * S is exact, and the rounding error in T is OK.
	 */
	mulps	%xmm14, %xmm0
	mulps	%xmm14, %xmm4

	/*
	 * Get the absolute value of the input, since we will exploit antisymmetry
	 * and mostly assume X >= 0 in the core computation
	 */
	movups	SgnMask+__svml_sasinh_data_internal(%rip), %xmm6

	/*
	 * Compute e = -(2 * d + d^2)
	 * The first FMR is exact, and the rounding error in the other is acceptable
	 * since d and e are ~ 2^-8
	 */
	movaps	%xmm14, %xmm13
	andps	%xmm8, %xmm6

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
	movaps	%xmm0, %xmm1

	/*
	 * Check whether the input is finite, by checking |X| <= MaxFloat
	 * Otherwise set the rangemask so that the callout will get used.
	 * Note that this will also use the callout for NaNs since not(NaN <= MaxFloat)
	 */
	movaps	%xmm6, %xmm9

	/*
	 * The following computation can go wrong for very large X, basically
	 * because X^2 overflows. But for large X we have
	 * asinh(X) / log(2 X) - 1 =~= 1/(4 * X^2), so for X >= 2^30
	 * we can just later stick X back into the log and tweak up the exponent.
	 * Actually we scale X by 2^-30 and tweak the exponent up by 31,
	 * to stay in the safe range for the later log computation.
	 * Compute a flag now telling us when do do this.
	 */
	movaps	%xmm6, %xmm5
	cmpnleps sLargestFinite+__svml_sasinh_data_internal(%rip), %xmm9
	cmpltps	sBigThreshold+__svml_sasinh_data_internal(%rip), %xmm5
	mulps	%xmm0, %xmm13
	addps	%xmm4, %xmm1
	subps	%xmm7, %xmm0
	mulps	%xmm4, %xmm14
	movmskps %xmm9, %edx
	movaps	%xmm7, %xmm9

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
	movups	sC3+__svml_sasinh_data_internal(%rip), %xmm15
	subps	%xmm13, %xmm9
	movups	sHalf+__svml_sasinh_data_internal(%rip), %xmm10
	subps	%xmm14, %xmm9

	/* sX2over2 = X^2/2 */
	mulps	%xmm10, %xmm3
	mulps	%xmm9, %xmm15

	/* sX46 = -X^4/4 + X^6/8 */
	movaps	%xmm3, %xmm2
	movaps	%xmm3, %xmm12

	/*
	 * Now do another compensated sum to add |X| + [sqrt(1 + X^2) - 1].
	 * It's always safe to assume |X| is larger.
	 * This is the final 2-part argument to the log1p function
	 */
	movaps	%xmm6, %xmm14
	addps	sC2+__svml_sasinh_data_internal(%rip), %xmm15
	mulps	%xmm9, %xmm15
	addps	%xmm10, %xmm15
	mulps	%xmm15, %xmm9
	mulps	%xmm1, %xmm9

	/* Now multiplex to the case X = 2^-30 * input, Xl = sL = 0 in the "big" case. */
	movups	XScale+__svml_sasinh_data_internal(%rip), %xmm15
	addps	%xmm9, %xmm4
	movaps	%xmm4, %xmm11
	addps	%xmm0, %xmm11
	subps	%xmm11, %xmm0
	addps	%xmm0, %xmm4

	/* sX4over4 = X^4/4 */
	movaps	%xmm3, %xmm0
	mulps	%xmm3, %xmm0
	mulps	%xmm0, %xmm2
	subps	%xmm0, %xmm2

	/*
	 * Now we feed into the log1p code, using H in place of _VARG1 and
	 * also adding L into Xl.
	 * compute 1+x as high, low parts
	 */
	movaps	%xmm7, %xmm0

	/* sX46over2 = -X^4/8 + x^6/16 */
	mulps	%xmm2, %xmm10
	movaps	%xmm7, %xmm2
	addps	%xmm10, %xmm12
	subps	%xmm12, %xmm3
	addps	%xmm3, %xmm10

	/* Now multiplex the two possible computations */
	movaps	%xmm6, %xmm3
	cmpleps	sLittleThreshold+__svml_sasinh_data_internal(%rip), %xmm3
	movaps	%xmm3, %xmm13
	andps	%xmm3, %xmm12
	andnps	%xmm11, %xmm13
	movaps	%xmm3, %xmm1
	orps	%xmm12, %xmm13
	andnps	%xmm4, %xmm1
	andps	%xmm3, %xmm10
	movaps	%xmm6, %xmm4
	orps	%xmm10, %xmm1
	addps	%xmm13, %xmm14
	mulps	%xmm15, %xmm6
	maxps	%xmm14, %xmm0
	minps	%xmm14, %xmm2
	subps	%xmm14, %xmm4
	movaps	%xmm0, %xmm3
	addps	%xmm4, %xmm13
	addps	%xmm2, %xmm3
	addps	%xmm13, %xmm1
	subps	%xmm3, %xmm0
	movaps	%xmm5, %xmm4
	andps	%xmm5, %xmm3
	andnps	%xmm6, %xmm4
	addps	%xmm0, %xmm2

	/*
	 * Now resume the main code.
	 * reduction: compute r, n
	 */
	movdqu	iBrkValue+__svml_sasinh_data_internal(%rip), %xmm6
	orps	%xmm3, %xmm4
	psubd	%xmm6, %xmm4
	movaps	%xmm7, %xmm0
	addps	%xmm2, %xmm1
	movdqu	iOffExpoMask+__svml_sasinh_data_internal(%rip), %xmm2
	pand	%xmm4, %xmm2
	psrad	$23, %xmm4
	cvtdq2ps %xmm4, %xmm3
	pslld	$23, %xmm4
	andps	%xmm5, %xmm1
	paddd	%xmm6, %xmm2
	psubd	%xmm4, %xmm0
	mulps	%xmm0, %xmm1

	/* polynomial evaluation */
	subps	%xmm7, %xmm2
	movups	sPoly+112+__svml_sasinh_data_internal(%rip), %xmm7
	addps	%xmm2, %xmm1
	mulps	%xmm1, %xmm7
	movaps	%xmm5, %xmm2

	/* Add 31 to the exponent in the "large" case to get log(2 * input) */
	movups	sThirtyOne+__svml_sasinh_data_internal(%rip), %xmm0
	addps	sPoly+96+__svml_sasinh_data_internal(%rip), %xmm7
	addps	%xmm3, %xmm0
	mulps	%xmm1, %xmm7
	andnps	%xmm0, %xmm2
	andps	%xmm5, %xmm3
	orps	%xmm3, %xmm2
	addps	sPoly+80+__svml_sasinh_data_internal(%rip), %xmm7

	/* final reconstruction */
	mulps	sLn2+__svml_sasinh_data_internal(%rip), %xmm2
	mulps	%xmm1, %xmm7

	/* Finally, reincorporate the original sign. */
	movups	sSign+__svml_sasinh_data_internal(%rip), %xmm0
	andps	%xmm8, %xmm0
	addps	sPoly+64+__svml_sasinh_data_internal(%rip), %xmm7
	mulps	%xmm1, %xmm7
	addps	sPoly+48+__svml_sasinh_data_internal(%rip), %xmm7
	mulps	%xmm1, %xmm7
	addps	sPoly+32+__svml_sasinh_data_internal(%rip), %xmm7
	mulps	%xmm1, %xmm7
	addps	sPoly+16+__svml_sasinh_data_internal(%rip), %xmm7
	mulps	%xmm1, %xmm7
	addps	sPoly+__svml_sasinh_data_internal(%rip), %xmm7
	mulps	%xmm1, %xmm7
	mulps	%xmm1, %xmm7
	addps	%xmm7, %xmm1
	addps	%xmm2, %xmm1
	pxor	%xmm1, %xmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm8

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
	movups	%xmm8, 32(%rsp)
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
	call	asinhf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_asinhf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sasinh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 SgnMask[4][1];
	__declspec(align(16)) VUINT32 sOne[4][1];
	__declspec(align(16)) VUINT32 sPoly[8][4][1];
	__declspec(align(16)) VUINT32 iBrkValue[4][1];
	__declspec(align(16)) VUINT32 iOffExpoMask[4][1];
	__declspec(align(16)) VUINT32 sBigThreshold[4][1];
	__declspec(align(16)) VUINT32 sC2[4][1];
	__declspec(align(16)) VUINT32 sC3[4][1];
	__declspec(align(16)) VUINT32 sHalf[4][1];
	__declspec(align(16)) VUINT32 sLargestFinite[4][1];
	__declspec(align(16)) VUINT32 sLittleThreshold[4][1];
	__declspec(align(16)) VUINT32 sSign[4][1];
	__declspec(align(16)) VUINT32 sThirtyOne[4][1];
	__declspec(align(16)) VUINT32 sTopMask11[4][1];
	__declspec(align(16)) VUINT32 sTopMask8[4][1];
	__declspec(align(16)) VUINT32 XScale[4][1];
	__declspec(align(16)) VUINT32 sLn2[4][1];
} __svml_sasinh_data_internal;
#endif
__svml_sasinh_data_internal:
	/* SgnMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* sOne = SP 1.0 */
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* sPoly[] = SP polynomial */
	.align	16
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000 /* -5.0000000000000000000000000e-01 P0 */
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94 /* 3.3333265781402587890625000e-01 P1 */
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e /* -2.5004237890243530273437500e-01 P2 */
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190 /* 2.0007920265197753906250000e-01 P3 */
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37 /* -1.6472326219081878662109375e-01 P4 */
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12 /* 1.4042308926582336425781250e-01 P5 */
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3 /* -1.5122179687023162841796875e-01 P6 */
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed /* 1.3820238411426544189453125e-01 P7 */
	/* iBrkValue = SP 2/3 */
	.align	16
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	16
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sBigThreshold */
	.align	16
	.long	0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000
	/* sC2 */
	.align	16
	.long	0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000
	/* sC3 */
	.align	16
	.long	0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000
	/* sHalf */
	.align	16
	.long	0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000
	/* sLargestFinite */
	.align	16
	.long	0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF
	/* sLittleThreshold */
	.align	16
	.long	0x3D800000, 0x3D800000, 0x3D800000, 0x3D800000
	/* sSign */
	.align	16
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000
	/* sThirtyOne */
	.align	16
	.long	0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000
	/* sTopMask11 */
	.align	16
	.long	0xFFFFE000, 0xFFFFE000, 0xFFFFE000, 0xFFFFE000
	/* sTopMask8 */
	.align	16
	.long	0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000
	/* XScale */
	.align	16
	.long	0x30800000, 0x30800000, 0x30800000, 0x30800000
	/* sLn2 = SP ln(2) */
	.align	16
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.align	16
	.type	__svml_sasinh_data_internal, @object
	.size	__svml_sasinh_data_internal, .-__svml_sasinh_data_internal
