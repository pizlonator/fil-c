/* Function hypot vectorized with AVX2.
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
 *      HIGH LEVEL OVERVIEW
 *
 *      Calculate z = (x*x+y*y)
 *      Calculate reciplicle sqrt (z)
 *      Calculate error = z*(rsqrt(z)*rsqrt(z)) - 1
 *      Calculate fixing part p with polynom
 *      Fix answer with sqrt(z) = z * rsqrt(z) + error * p * z
 *
 *      ALGORITHM DETAILS
 *
 *    Multiprecision branch for _HA_ only
 *      Remove sigm from both arguments
 *      Find maximum (_x) and minimum (_y) (by abs value) between arguments
 *      Split _x int _a and _b for multiprecision
 *      If _x >> _y we will we will not split _y for multiprecision
 *      all _y will be put into lower part (_d) and higher part (_c = 0)
 *      Fixing _hilo_mask for the case _x >> _y
 *      Split _y into _c and _d for multiprecision with fixed mask
 *
 *      compute Hi and Lo parts of _z = _x*_x + _y*_y
 *
 *      _zHi = _a*_a + _c*_c
 *      _zLo = (_x + _a)*_b + _d*_y + _d*_c
 *      _z = _zHi + _zLo
 *
 *    No multiprecision branch for _LA_ and _EP_
 *      _z = _VARG1 * _VARG1 + _VARG2 * _VARG2
 *
 *    Check _z exponent to be within borders [3BC ; 441] else goto Callout
 *
 *    _s  ~ 1.0/sqrt(_z)
 *    _s2 ~ 1.0/(sqrt(_z)*sqrt(_z)) ~ 1.0/_z = (1.0/_z + O)
 *    _e[rror]  =  (1.0/_z + O) * _z - 1.0
 *    calculate fixing part _p
 *    _p = (((_POLY_C5*_e + _POLY_C4)*_e +_POLY_C3)*_e +_POLY_C2)*_e + _POLY_C1
 *    some parts of polynom are skipped for lower flav
 *
 *    result = _z * (1.0/sqrt(_z) + O) + _p * _e[rror] * _z
 *
 *
 */

/* Offsets for data table __svml_dhypot_data_internal
 */
#define _dHiLoMask			0
#define _dAbsMask			32
#define _dOne				64
#define _POLY_C5			96
#define _POLY_C4			128
#define _POLY_C3			160
#define _POLY_C2			192
#define _POLY_C1			224
#define _LowBoundary			256
#define _HighBoundary			288

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4vv_hypot_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$128, %rsp
	vmovapd	%ymm1, %ymm2
	vmovapd	%ymm0, %ymm1

	/*
	 *  Defines
	 *  Implementation
	 * Multiprecision branch for _HA_ only
	 * _z = _VARG1 * _VARG1 + _VARG2 * _VARG2
	 */
	vmulpd	%ymm1, %ymm1, %ymm0

	/*
	 * calculate fixing part _p
	 * _p = (((_POLY_C5*_e + _POLY_C4)*_e +_POLY_C3)*_e +_POLY_C2)*_e + _POLY_C1
	 * some parts of polynom are skipped for lower flav
	 */
	vmovupd	_POLY_C4+__svml_dhypot_data_internal(%rip), %ymm15
	vmovups	_LowBoundary+__svml_dhypot_data_internal(%rip), %xmm4
	vfmadd231pd %ymm2, %ymm2, %ymm0

	/*
	 * _s  ~ 1.0/sqrt(_z)
	 * _s2 ~ 1.0/(sqrt(_z)*sqrt(_z)) ~ 1.0/_z
	 */
	vcvtpd2ps %ymm0, %xmm12

	/* Check _z exponent to be within borders [3BC ; 441] else goto Callout */
	vextractf128 $1, %ymm0, %xmm3
	vrsqrtps %xmm12, %xmm13
	vshufps	$221, %xmm3, %xmm0, %xmm5
	vcvtps2pd %xmm13, %ymm3
	vpcmpgtd %xmm5, %xmm4, %xmm6
	vpcmpgtd _HighBoundary+__svml_dhypot_data_internal(%rip), %xmm5, %xmm7
	vpor	%xmm7, %xmm6, %xmm9
	vpshufd	$80, %xmm9, %xmm8
	vmulpd	%ymm3, %ymm3, %ymm14
	vpshufd	$250, %xmm9, %xmm10

	/* _e[rror]  ~  (1.0/_z + O) * _z - 1.0 */
	vfmsub213pd _dOne+__svml_dhypot_data_internal(%rip), %ymm0, %ymm14
	vfmadd213pd _POLY_C3+__svml_dhypot_data_internal(%rip), %ymm14, %ymm15
	vfmadd213pd _POLY_C2+__svml_dhypot_data_internal(%rip), %ymm14, %ymm15
	vfmadd213pd _POLY_C1+__svml_dhypot_data_internal(%rip), %ymm14, %ymm15

	/* result = _z * (1.0/sqrt(_z) + O) + _p * _e[rror] * _z */
	vmulpd	%ymm15, %ymm14, %ymm14
	vmulpd	%ymm14, %ymm3, %ymm15
	vmulpd	%ymm15, %ymm0, %ymm4
	vfmadd213pd %ymm4, %ymm3, %ymm0
	vinsertf128 $1, %xmm10, %ymm8, %ymm11
	vmovmskpd %ymm11, %edx

	/*  The end of implementation  */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm1 ymm2

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
	vmovupd	%ymm1, 32(%rsp)
	vmovupd	%ymm2, 64(%rsp)
	vmovupd	%ymm0, 96(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm0

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
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
	vmovupd	96(%rsp), %ymm0

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	32(%rsp, %r14, 8), %xmm0
	vmovsd	64(%rsp, %r14, 8), %xmm1
	call	hypot@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 96(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN4vv_hypot_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_dhypot_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _dHiLoMask[4][2];
	__declspec(align(32)) VUINT32 _dAbsMask[4][2];
	__declspec(align(32)) VUINT32 _dOne[4][2];
	__declspec(align(32)) VUINT32 _POLY_C5[4][2];
	__declspec(align(32)) VUINT32 _POLY_C4[4][2];
	__declspec(align(32)) VUINT32 _POLY_C3[4][2];
	__declspec(align(32)) VUINT32 _POLY_C2[4][2];
	__declspec(align(32)) VUINT32 _POLY_C1[4][2];
	__declspec(align(32)) VUINT32 _LowBoundary[8][1];
	__declspec(align(32)) VUINT32 _HighBoundary[8][1];
} __svml_dhypot_data_internal;
#endif
__svml_dhypot_data_internal:
	/* legacy algorithm */
	.quad	0xffffc00000000000, 0xffffc00000000000, 0xffffc00000000000, 0xffffc00000000000 /* _dHiLoMask */
	.align	32
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff /* _dAbsMask */
	.align	32
	.quad	0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000, 0x3FF0000000000000 /* _dOne */
	.align	32
	.quad	0xBFCF800000000000, 0xBFCF800000000000, 0xBFCF800000000000, 0xBFCF800000000000 /* _POLY_C5 */
	.align	32
	.quad	0x3FD1800000000000, 0x3FD1800000000000, 0x3FD1800000000000, 0x3FD1800000000000 /* _POLY_C4 */
	.align	32
	.quad	0xBFD4000000000000, 0xBFD4000000000000, 0xBFD4000000000000, 0xBFD4000000000000 /* _POLY_C3 */
	.align	32
	.quad	0x3FD8000000000000, 0x3FD8000000000000, 0x3FD8000000000000, 0x3FD8000000000000 /* _POLY_C2 */
	.align	32
	.quad	0xBFE0000000000000, 0xBFE0000000000000, 0xBFE0000000000000, 0xBFE0000000000000 /* _POLY_C1 */
	.align	32
	.long	0x3BC00000, 0x3BC00000, 0x3BC00000, 0x3BC00000, 0x3BC00000, 0x3BC00000, 0x3BC00000, 0x3BC00000 /* _LowBoundary */
	.align	32
	.long	0x44100000, 0x44100000, 0x44100000, 0x44100000, 0x44100000, 0x44100000, 0x44100000, 0x44100000 /* _HighBoundary */
	.align	32
	.type	__svml_dhypot_data_internal, @object
	.size	__svml_dhypot_data_internal, .-__svml_dhypot_data_internal
