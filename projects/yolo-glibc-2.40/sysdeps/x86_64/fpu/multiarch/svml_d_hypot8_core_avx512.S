/* Function hypot vectorized with AVX-512.
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
#define _dAbsMask			0
#define _lExpBound_uisa			64
#define _lExpBound			128
#define _dHalf				192

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8vv_hypot_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$256, %rsp
	vgetexppd {sae}, %zmm0, %zmm2
	vgetexppd {sae}, %zmm1, %zmm3
	vmovups	_dHalf+__svml_dhypot_data_internal(%rip), %zmm9
	vmaxpd	{sae}, %zmm3, %zmm2, %zmm4
	vmulpd	{rn-sae}, %zmm0, %zmm0, %zmm2
	vandpd	_dAbsMask+__svml_dhypot_data_internal(%rip), %zmm4, %zmm5
	vfmadd231pd {rn-sae}, %zmm1, %zmm1, %zmm2

	/* Select exponent bound so that no scaling is needed */
	vpcmpq	$5, _lExpBound_uisa+__svml_dhypot_data_internal(%rip), %zmm5, %k0
	vrsqrt14pd %zmm2, %zmm6
	kmovw	%k0, %edx
	vmulpd	{rn-sae}, %zmm6, %zmm2, %zmm7
	vmulpd	{rn-sae}, %zmm6, %zmm9, %zmm8
	vfnmadd231pd {rn-sae}, %zmm7, %zmm8, %zmm9
	vfmadd231pd {rn-sae}, %zmm9, %zmm8, %zmm8
	vfmadd213pd {rn-sae}, %zmm7, %zmm7, %zmm9
	vfnmadd231pd {rn-sae}, %zmm9, %zmm9, %zmm2
	vfmadd213pd {rn-sae}, %zmm9, %zmm8, %zmm2

	/*  The end of implementation  */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1 zmm2

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm2, %zmm0
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
	vmovups	%zmm0, 64(%rsp)
	vmovups	%zmm1, 128(%rsp)
	vmovups	%zmm2, 192(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm2

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
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
	vmovups	192(%rsp), %zmm2

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm2

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovsd	64(%rsp, %r14, 8), %xmm0
	vmovsd	128(%rsp, %r14, 8), %xmm1
	call	hypot@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 192(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN8vv_hypot_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dhypot_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _dAbsMask[8][2];
	__declspec(align(64)) VUINT32 _lExpBound_uisa[8][2];
	__declspec(align(64)) VUINT32 _lExpBound[8][2];
	__declspec(align(64)) VUINT32 _dHalf[8][2];
} __svml_dhypot_data_internal;
#endif
__svml_dhypot_data_internal:
	/* legacy algorithm */
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff /* _dAbsMask */
	/* fma based algorithm*/
	.align	64
	.quad	0x407ff00000000000, 0x407ff00000000000, 0x407ff00000000000, 0x407ff00000000000, 0x407ff00000000000, 0x407ff00000000000, 0x407ff00000000000, 0x407ff00000000000 /* _lExpBound_uisa */
	.align	64
	.quad	0x404f800000000000, 0x404f800000000000, 0x404f800000000000, 0x404f800000000000, 0x404f800000000000, 0x404f800000000000, 0x404f800000000000, 0x404f800000000000 /* _lExpBound */
	.align	64
	.quad	0x3FE0000000000000, 0x3FE0000000000000, 0x3FE0000000000000, 0x3FE0000000000000, 0x3FE0000000000000, 0x3FE0000000000000, 0x3FE0000000000000, 0x3FE0000000000000 /* _dHalf */
	.align	64
	.type	__svml_dhypot_data_internal, @object
	.size	__svml_dhypot_data_internal, .-__svml_dhypot_data_internal
