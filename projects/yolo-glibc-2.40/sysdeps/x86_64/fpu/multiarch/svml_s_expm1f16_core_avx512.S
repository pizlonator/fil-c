/* Function expm1f vectorized with AVX-512.
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
 *   After computing exp(x) in high-low parts, an accurate computation is performed to obtain exp(x)-1
 *   Typical exp() implementation, except that:
 *    - tables are small (32 elements), allowing for fast gathers
 *    - all arguments processed in the main path
 *        - final VSCALEF assists branch-free design (correct overflow/underflow and special case responses)
 *        - a VAND is used to ensure the reduced argument |R|<2, even for large inputs
 *        - RZ mode used to avoid overflow to +/-Inf for x*log2(e); helps with special case handling
 *
 *
 */

/* Offsets for data table __svml_sexpm1_data_internal_avx512
 */
#define Exp_tbl_H			0
#define Exp_tbl_L			128
#define L2E				256
#define Shifter				320
#define Threshold			384
#define SgnMask				448
#define L2H				512
#define L2L				576
#define EMask				640
#define poly_coeff3			704
#define poly_coeff2			768
#define One				832

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_expm1f_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	L2E+__svml_sexpm1_data_internal_avx512(%rip), %zmm5
	vmovups	Shifter+__svml_sexpm1_data_internal_avx512(%rip), %zmm3
	vmovups	L2H+__svml_sexpm1_data_internal_avx512(%rip), %zmm8
	vmovups	L2L+__svml_sexpm1_data_internal_avx512(%rip), %zmm4
	vmovups	__svml_sexpm1_data_internal_avx512(%rip), %zmm6

	/* polynomial */
	vmovups	poly_coeff3+__svml_sexpm1_data_internal_avx512(%rip), %zmm9
	vmovups	poly_coeff2+__svml_sexpm1_data_internal_avx512(%rip), %zmm12
	vmovups	Exp_tbl_L+__svml_sexpm1_data_internal_avx512(%rip), %zmm11
	vmovups	Threshold+__svml_sexpm1_data_internal_avx512(%rip), %zmm2

	/* Th - 1 */
	vmovups	One+__svml_sexpm1_data_internal_avx512(%rip), %zmm14
	vmovaps	%zmm0, %zmm1

	/* 2^(52-5)*1.5 + x * log2(e) */
	vfmadd213ps {rn-sae}, %zmm3, %zmm1, %zmm5
	vcmpps	$29, {sae}, %zmm2, %zmm1, %k0

	/* Z0 ~ x*log2(e), rounded to 5 fractional bits */
	vsubps	{rn-sae}, %zmm3, %zmm5, %zmm7
	vpermt2ps Exp_tbl_H+64+__svml_sexpm1_data_internal_avx512(%rip), %zmm5, %zmm6
	vpermt2ps Exp_tbl_L+64+__svml_sexpm1_data_internal_avx512(%rip), %zmm5, %zmm11
	vandps	SgnMask+__svml_sexpm1_data_internal_avx512(%rip), %zmm1, %zmm0

	/* R = x - Z0*log(2) */
	vfnmadd213ps {rn-sae}, %zmm1, %zmm7, %zmm8

	/* scale Th */
	vscalefps {rn-sae}, %zmm7, %zmm6, %zmm2
	vfnmadd231ps {rn-sae}, %zmm7, %zmm4, %zmm8
	kmovw	%k0, %edx

	/* ensure |R|<2 even for special cases */
	vandps	EMask+__svml_sexpm1_data_internal_avx512(%rip), %zmm8, %zmm13
	vsubps	{rn-sae}, %zmm14, %zmm2, %zmm8
	vmulps	{rn-sae}, %zmm13, %zmm13, %zmm10
	vfmadd231ps {rn-sae}, %zmm13, %zmm9, %zmm12

	/* Tlr + R+ R2*Poly */
	vfmadd213ps {rn-sae}, %zmm11, %zmm10, %zmm12
	vaddps	{rn-sae}, %zmm13, %zmm12, %zmm15

	/* (Th-1)+Th*(Tlr + R+ R*Poly) */
	vfmadd213ps {rn-sae}, %zmm8, %zmm15, %zmm2
	vorps	%zmm0, %zmm2, %zmm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1

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
	vmovups	%zmm1, 64(%rsp)
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
	call	expm1f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_expm1f_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sexpm1_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Exp_tbl_H[32][1];
	__declspec(align(64)) VUINT32 Exp_tbl_L[32][1];
	__declspec(align(64)) VUINT32 L2E[16][1];
	__declspec(align(64)) VUINT32 Shifter[16][1];
	__declspec(align(64)) VUINT32 Threshold[16][1];
	__declspec(align(64)) VUINT32 SgnMask[16][1];
	__declspec(align(64)) VUINT32 L2H[16][1];
	__declspec(align(64)) VUINT32 L2L[16][1];
	__declspec(align(64)) VUINT32 EMask[16][1];
	__declspec(align(64)) VUINT32 poly_coeff3[16][1];
	__declspec(align(64)) VUINT32 poly_coeff2[16][1];
	__declspec(align(64)) VUINT32 One[16][1];
} __svml_sexpm1_data_internal_avx512;
#endif
__svml_sexpm1_data_internal_avx512:
	/* Exp_tbl_H */
	.long	0x3f800000, 0x3f82cd87, 0x3f85aac3, 0x3f88980f
	.long	0x3f8b95c2, 0x3f8ea43a, 0x3f91c3d3, 0x3f94f4f0
	.long	0x3f9837f0, 0x3f9b8d3a, 0x3f9ef532, 0x3fa27043
	.long	0x3fa5fed7, 0x3fa9a15b, 0x3fad583f, 0x3fb123f6
	.long	0x3fb504f3, 0x3fb8fbaf, 0x3fbd08a4, 0x3fc12c4d
	.long	0x3fc5672a, 0x3fc9b9be, 0x3fce248c, 0x3fd2a81e
	.long	0x3fd744fd, 0x3fdbfbb8, 0x3fe0ccdf, 0x3fe5b907
	.long	0x3feac0c7, 0x3fefe4ba, 0x3ff5257d, 0x3ffa83b3
	/* Exp_tbl_L */
	.align	64
	.long	0x00000000, 0xb34a3a0a, 0x3346cb6a, 0xb36ed17e
	.long	0xb24e0611, 0xb3517dd9, 0x334b2482, 0xb31586de
	.long	0x33092801, 0xb2e6f467, 0x331b85f2, 0x3099b6f1
	.long	0xb3051aa8, 0xb2e2a0da, 0xb2006c56, 0xb3365942
	.long	0x329302ae, 0x32c595dc, 0xb302e5a2, 0xb28e10a1
	.long	0x31b3d0e5, 0xb31a472b, 0x31d1daf2, 0xb305bf64
	.long	0xb27ce182, 0xb2f26443, 0xb1b4b0da, 0xb1da8a8f
	.long	0xb1d290be, 0xb2d5b899, 0x31b0a147, 0xb2156afc
	/* log2(e) */
	.align	64
	.long	0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B, 0x3fB8AA3B
	/* Shifter=2^(23-5)*1.5 */
	.align	64
	.long	0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000, 0x48c00000
	/* Threshold */
	.align	64
	.long	0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B, 0x42AD496B
	/* Sgn */
	.align	64
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
	/* L2H = log(2)_high */
	.align	64
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	/* L2L = log(2)_low */
	.align	64
	.long	0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308, 0xb102e308
	/* EMask */
	.align	64
	.long	0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff
	/* poly_coeff3 */
	.align	64
	.long	0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3, 0x3e2AABF3
	/* poly_coeff2 */
	.align	64
	.long	0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6, 0x3f0000F6
	/* One */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	.align	64
	.type	__svml_sexpm1_data_internal_avx512, @object
	.size	__svml_sexpm1_data_internal_avx512, .-__svml_sexpm1_data_internal_avx512
