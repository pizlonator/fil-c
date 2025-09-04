/* Function exp10f vectorized with AVX-512.
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
 *   Typical exp10() implementation, except that:
 *    - tables are small (16 elements), allowing for fast gathers
 *    - all arguments processed in the main path
 *        - final VSCALEF assists branch-free design (correct overflow/underflow and special case responses)
 *        - a VAND is used to ensure the reduced argument |R|<2, even for large inputs
 *        - RZ mode used to avoid overflow to +/-Inf for x*log2(e); helps with special case handling
 *        - SAE used to avoid spurious flag settings
 *
 */

/* Offsets for data table __svml_sexp10_data_internal_avx512
 */
#define Exp_tbl_L			0
#define Exp_tbl_H			128
#define L2E				256
#define Shifter				320
#define L2H				384
#define L2L				448
#define EMask				512
#define AbsMask				576
#define Threshold			640
#define poly_coeff2			704
#define poly_coeff1			768

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_exp10f_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$192, %rsp
	vmovups	L2E+__svml_sexp10_data_internal_avx512(%rip), %zmm2
	vmovups	Shifter+__svml_sexp10_data_internal_avx512(%rip), %zmm1
	vmovups	L2H+__svml_sexp10_data_internal_avx512(%rip), %zmm5
	vmovups	L2L+__svml_sexp10_data_internal_avx512(%rip), %zmm4

	/* ensure |R|<2 even for special cases */
	vmovups	EMask+__svml_sexp10_data_internal_avx512(%rip), %zmm6
	vmovups	poly_coeff2+__svml_sexp10_data_internal_avx512(%rip), %zmm9

	/* 2^(52-4)*1.5 + x * log2(e) */
	vfmadd213ps {rz-sae}, %zmm1, %zmm0, %zmm2
	vmovups	poly_coeff1+__svml_sexp10_data_internal_avx512(%rip), %zmm10
	vmovups	__svml_sexp10_data_internal_avx512(%rip), %zmm8
	vmovups	Exp_tbl_H+__svml_sexp10_data_internal_avx512(%rip), %zmm15
	vmovups	Threshold+__svml_sexp10_data_internal_avx512(%rip), %zmm13
	vpsrld	$5, %zmm2, %zmm3

	/* Z0 ~ x*log2(e), rounded down to 6 fractional bits */
	vsubps	{rn-sae}, %zmm1, %zmm2, %zmm1
	vpermt2ps Exp_tbl_L+64+__svml_sexp10_data_internal_avx512(%rip), %zmm2, %zmm8
	vpermt2ps Exp_tbl_H+64+__svml_sexp10_data_internal_avx512(%rip), %zmm3, %zmm15
	vandps	AbsMask+__svml_sexp10_data_internal_avx512(%rip), %zmm0, %zmm12

	/* R = x - Z0*log(2) */
	vfnmadd213ps {rn-sae}, %zmm0, %zmm1, %zmm5
	vcmpps	$29, {sae}, %zmm13, %zmm12, %k0
	vfnmadd231ps {rn-sae}, %zmm1, %zmm4, %zmm5
	kmovw	%k0, %edx
	vrangeps $2, {sae}, %zmm6, %zmm5, %zmm11
	vfmadd231ps {rn-sae}, %zmm11, %zmm9, %zmm10
	vmulps	{rn-sae}, %zmm11, %zmm10, %zmm14

	/* x!=0? */
	vpxord	%zmm7, %zmm7, %zmm7
	vcmpps	$4, {sae}, %zmm7, %zmm0, %k1

	/* Th*Tl */
	vmulps	{rn-sae}, %zmm8, %zmm15, %zmm15{%k1}
	vfmadd213ps {rn-sae}, %zmm15, %zmm14, %zmm15
	vscalefps {rn-sae}, %zmm1, %zmm15, %zmm1
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm1, %zmm0
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
	# LOE rbx r12 r13 r14 r15 edx zmm1

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
	vmovups	128(%rsp), %zmm1

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -176; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x50, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -184; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x48, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -192; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x40, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm1

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	64(%rsp, %r14, 4), %xmm0
	call	exp10f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 128(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVeN16v_exp10f_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_sexp10_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 Exp_tbl_L[32][1];
	__declspec(align(64)) VUINT32 Exp_tbl_H[32][1];
	__declspec(align(64)) VUINT32 L2E[16][1];
	__declspec(align(64)) VUINT32 Shifter[16][1];
	__declspec(align(64)) VUINT32 L2H[16][1];
	__declspec(align(64)) VUINT32 L2L[16][1];
	__declspec(align(64)) VUINT32 EMask[16][1];
	__declspec(align(64)) VUINT32 AbsMask[16][1];
	__declspec(align(64)) VUINT32 Threshold[16][1];
	__declspec(align(64)) VUINT32 poly_coeff2[16][1];
	__declspec(align(64)) VUINT32 poly_coeff1[16][1];
} __svml_sexp10_data_internal_avx512;
#endif
__svml_sexp10_data_internal_avx512:
	/* Exp_tbl_L */
	.long	0x3f800001, 0x3f801631, 0x3f802c65, 0x3f80429d
	.long	0x3f8058d9, 0x3f806f18, 0x3f80855c, 0x3f809ba3
	.long	0x3f80b1ee, 0x3f80c83d, 0x3f80de90, 0x3f80f4e7
	.long	0x3f810b42, 0x3f8121a0, 0x3f813803, 0x3f814e69
	.long	0x3f8164d3, 0x3f817b41, 0x3f8191b3, 0x3f81a829
	.long	0x3f81bea2, 0x3f81d520, 0x3f81eba2, 0x3f820227
	.long	0x3f8218b0, 0x3f822f3d, 0x3f8245cf, 0x3f825c64
	.long	0x3f8272fd, 0x3f828999, 0x3f82a03a, 0x3f82b6df
	/* Exp_tbl_H */
	.align	64
	.long	0x3f800000, 0x3f82cd87, 0x3f85aac3, 0x3f88980f
	.long	0x3f8b95c2, 0x3f8ea43a, 0x3f91c3d3, 0x3f94f4f0
	.long	0x3f9837f0, 0x3f9b8d3a, 0x3f9ef532, 0x3fa27043
	.long	0x3fa5fed7, 0x3fa9a15b, 0x3fad583f, 0x3fb123f6
	.long	0x3fb504f3, 0x3fb8fbaf, 0x3fbd08a4, 0x3fc12c4d
	.long	0x3fc5672a, 0x3fc9b9be, 0x3fce248c, 0x3fd2a81e
	.long	0x3fd744fd, 0x3fdbfbb8, 0x3fe0ccdf, 0x3fe5b907
	.long	0x3feac0c7, 0x3fefe4ba, 0x3ff5257d, 0x3ffa83b3
	/* log2(10) */
	.align	64
	.long	0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78, 0x40549A78
	/* Shifter=2^(23-10)*1.5 */
	.align	64
	.long	0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000, 0x46400000
	/* L2H = log(2)_high */
	.align	64
	.long	0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b
	/* L2L = log(2)_low */
	.align	64
	.long	0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860, 0xb2760860
	/* EMask */
	.align	64
	.long	0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000
	/* AbsMask */
	.align	64
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* Threshold */
	.align	64
	.long	0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818, 0x4217B818
	/* poly_coeff2 */
	.align	64
	.long	0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA, 0x4029B7DA
	/* poly_coeff1 */
	.align	64
	.long	0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D, 0x40135D8D
	.align	64
	.type	__svml_sexp10_data_internal_avx512, @object
	.size	__svml_sexp10_data_internal_avx512, .-__svml_sexp10_data_internal_avx512
