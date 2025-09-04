/* Function log10 vectorized with SSE4.
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
 *    Get short reciprocal approximation Rcp ~ 1/mantissa(x)
 *    R = Rcp*x - 1.0
 *    log10(x) = k*log10(2.0) - log10(Rcp) + poly_approximation(R)
 *       log10(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_dlog10_data_internal
 */
#define Log_HA_table			0
#define Log_LA_table			4112
#define poly_coeff			8224
#define ExpMask				8304
#define Two10				8320
#define MinNorm				8336
#define MaxNorm				8352
#define HalfMask			8368
#define One				8384
#define Threshold			8400
#define Bias				8416
#define Bias1				8432
#define L2				8448

/* Lookup bias for data table __svml_dlog10_data_internal.  */
#define Table_Lookup_Bias		-0x406ff0

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_log10_sse4)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$64, %rsp

	/* exponent bits */
	movaps	%xmm0, %xmm5

	/* preserve mantissa, set input exponent to 2^(-10) */
	movups	ExpMask+__svml_dlog10_data_internal(%rip), %xmm1
	psrlq	$20, %xmm5
	andps	%xmm0, %xmm1
	lea	Table_Lookup_Bias+__svml_dlog10_data_internal(%rip), %rsi
	orps	Two10+__svml_dlog10_data_internal(%rip), %xmm1

	/* check range */
	movaps	%xmm0, %xmm8

	/* reciprocal approximation good to at least 11 bits */
	cvtpd2ps %xmm1, %xmm2
	cmpltpd	MinNorm+__svml_dlog10_data_internal(%rip), %xmm8
	movlhps	%xmm2, %xmm2
	movaps	%xmm0, %xmm7
	rcpps	%xmm2, %xmm3
	cmpnlepd MaxNorm+__svml_dlog10_data_internal(%rip), %xmm7
	cvtps2pd %xmm3, %xmm12

	/* round reciprocal to nearest integer, will have 1+9 mantissa bits */
	movups	.FLT_12(%rip), %xmm4
	orps	%xmm7, %xmm8
	addpd	%xmm4, %xmm12

	/* combine and get argument value range mask */
	movmskpd %xmm8, %edx

	/* argument reduction */
	movups	HalfMask+__svml_dlog10_data_internal(%rip), %xmm9
	subpd	%xmm4, %xmm12
	andps	%xmm1, %xmm9

	/*
	 * prepare table index
	 * table lookup
	 */
	movaps	%xmm12, %xmm10
	subpd	%xmm9, %xmm1
	mulpd	%xmm12, %xmm9
	mulpd	%xmm12, %xmm1
	subpd	One+__svml_dlog10_data_internal(%rip), %xmm9
	addpd	%xmm9, %xmm1

	/* polynomial */
	movups	poly_coeff+__svml_dlog10_data_internal(%rip), %xmm14
	psrlq	$40, %xmm10
	mulpd	%xmm1, %xmm14
	movd	%xmm10, %eax
	pshufd	$2, %xmm10, %xmm11
	movaps	%xmm1, %xmm10
	movups	poly_coeff+32+__svml_dlog10_data_internal(%rip), %xmm15
	mulpd	%xmm1, %xmm10
	addpd	poly_coeff+16+__svml_dlog10_data_internal(%rip), %xmm14
	mulpd	%xmm1, %xmm15
	mulpd	%xmm10, %xmm14
	addpd	poly_coeff+48+__svml_dlog10_data_internal(%rip), %xmm15
	movd	%xmm11, %ecx

	/* exponent*log(2.0) */
	movups	Threshold+__svml_dlog10_data_internal(%rip), %xmm13
	addpd	%xmm14, %xmm15
	cmpltpd	%xmm12, %xmm13
	mulpd	%xmm15, %xmm10
	pshufd	$221, %xmm5, %xmm6
	movups	poly_coeff+64+__svml_dlog10_data_internal(%rip), %xmm11

	/* biased exponent in DP format */
	cvtdq2pd %xmm6, %xmm3
	mulpd	%xmm1, %xmm11
	andps	Bias+__svml_dlog10_data_internal(%rip), %xmm13
	orps	Bias1+__svml_dlog10_data_internal(%rip), %xmm13
	subpd	%xmm13, %xmm3
	addpd	%xmm10, %xmm11
	mulpd	L2+__svml_dlog10_data_internal(%rip), %xmm3
	movslq	%eax, %rax
	movslq	%ecx, %rcx
	movsd	(%rsi, %rax), %xmm2
	movhpd	(%rsi, %rcx), %xmm2

	/* reconstruction */
	addpd	%xmm11, %xmm2
	addpd	%xmm2, %xmm3
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx xmm0 xmm3

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm3, %xmm0
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
	movups	%xmm0, 32(%rsp)
	movups	%xmm3, 48(%rsp)
	# LOE rbx r12 r13 r14 r15 edx

	xorl	%eax, %eax
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -48; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -56; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -64; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x22
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
	cmpl	$2, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm3

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -48; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -56; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -64; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 xmm3

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	call	log10@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVbN2v_log10_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dlog10_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 Log_HA_table[(1<<9)+2][2];
	__declspec(align(16)) VUINT32 Log_LA_table[(1<<9)+1][2];
	__declspec(align(16)) VUINT32 poly_coeff[5][2][2];
	__declspec(align(16)) VUINT32 ExpMask[2][2];
	__declspec(align(16)) VUINT32 Two10[2][2];
	__declspec(align(16)) VUINT32 MinNorm[2][2];
	__declspec(align(16)) VUINT32 MaxNorm[2][2];
	__declspec(align(16)) VUINT32 HalfMask[2][2];
	__declspec(align(16)) VUINT32 One[2][2];
	__declspec(align(16)) VUINT32 Threshold[2][2];
	__declspec(align(16)) VUINT32 Bias[2][2];
	__declspec(align(16)) VUINT32 Bias1[2][2];
	__declspec(align(16)) VUINT32 L2[2][2];
} __svml_dlog10_data_internal;
#endif
__svml_dlog10_data_internal:
	/* Log_HA_table */
	.quad	0xc0733a7146f6b080, 0xbe1e707ce619c200
	.quad	0xc0733a7547771970, 0xbe1e79c6c06d6f51
	.quad	0xc0733a7945aacb70, 0xbe1e78e225fad29c
	.quad	0xc0733a7d41946970, 0xbe1e76d607f9693b
	.quad	0xc0733a813b3691f0, 0xbe1e7704b3e0685b
	.quad	0xc0733a853293df00, 0xbe1e79c1216a27fa
	.quad	0xc0733a8927aee660, 0xbe1e76dce5734a81
	.quad	0xc0733a8d1a8a3920, 0xbe1e782ee2ca4dba
	.quad	0xc0733a910b286430, 0xbe1e7812d1a0a61f
	.quad	0xc0733a94f98bf010, 0xbe1e77e1b5ecbc61
	.quad	0xc0733a98e5b76100, 0xbe1e76635cac1586
	.quad	0xc0733a9ccfad36f0, 0xbe1e7638f7968f32
	.quad	0xc0733aa0b76feda0, 0xbe1e7840ee76e365
	.quad	0xc0733aa49d01fcb0, 0xbe1e79f3fd01907e
	.quad	0xc0733aa88065d7a0, 0xbe1e77bbb3a9c38a
	.quad	0xc0733aac619dedb0, 0xbe1e7742719bf41d
	.quad	0xc0733ab040acaa20, 0xbe1e79bcedaf79cb
	.quad	0xc0733ab41d947450, 0xbe1e762d63cb7ca0
	.quad	0xc0733ab7f857af50, 0xbe1e77a07be83403
	.quad	0xc0733abbd0f8ba80, 0xbe1e7763ff836ad0
	.quad	0xc0733abfa779f130, 0xbe1e7737720ead39
	.quad	0xc0733ac37bddaad0, 0xbe1e7776a08e55e7
	.quad	0xc0733ac74e263af0, 0xbe1e793e3c52dd36
	.quad	0xc0733acb1e55f160, 0xbe1e788a94695051
	.quad	0xc0733aceec6f1a10, 0xbe1e76508114a813
	.quad	0xc0733ad2b873fd20, 0xbe1e76909457d23e
	.quad	0xc0733ad68266df10, 0xbe1e7664a24f9ca4
	.quad	0xc0733ada4a4a0090, 0xbe1e7a07b3d44b18
	.quad	0xc0733ade101f9ee0, 0xbe1e76d87594704d
	.quad	0xc0733ae1d3e9f340, 0xbe1e79563595a182
	.quad	0xc0733ae595ab33b0, 0xbe1e771880c3c6ab
	.quad	0xc0733ae955659250, 0xbe1e78c171f517d4
	.quad	0xc0733aed131b3df0, 0xbe1e77eac3874666
	.quad	0xc0733af0cece61b0, 0xbe1e790db479d8f6
	.quad	0xc0733af488812550, 0xbe1e7965d1aa5c90
	.quad	0xc0733af84035ad10, 0xbe1e78ceb398ba47
	.quad	0xc0733afbf5ee19c0, 0xbe1e779cc0dcb5aa
	.quad	0xc0733affa9ac88c0, 0xbe1e7871053953ed
	.quad	0xc0733b035b731420, 0xbe1e7a082cffa71a
	.quad	0xc0733b070b43d2a0, 0xbe1e7904b4382fad
	.quad	0xc0733b0ab920d790, 0xbe1e79b458d0b4f3
	.quad	0xc0733b0e650c3310, 0xbe1e79d0ded414c6
	.quad	0xc0733b120f07f200, 0xbe1e763c357a1943
	.quad	0xc0733b15b7161dd0, 0xbe1e78b80ba6daaa
	.quad	0xc0733b195d38bd00, 0xbe1e7998e23b8ffd
	.quad	0xc0733b1d0171d2c0, 0xbe1e7974aa65ee8c
	.quad	0xc0733b20a3c35f20, 0xbe1e76ccfde752ab
	.quad	0xc0733b24442f5ef0, 0xbe1e77b4ff19debb
	.quad	0xc0733b27e2b7cc10, 0xbe1e7772ee478542
	.quad	0xc0733b2b7f5e9d30, 0xbe1e781d81b58b44
	.quad	0xc0733b2f1a25c600, 0xbe1e78350d967565
	.quad	0xc0733b32b30f3720, 0xbe1e783888e48152
	.quad	0xc0733b364a1cde30, 0xbe1e78367bf7c111
	.quad	0xc0733b39df50a5d0, 0xbe1e7959e57ca47d
	.quad	0xc0733b3d72ac75c0, 0xbe1e777322423222
	.quad	0xc0733b41043232b0, 0xbe1e767ce42a60aa
	.quad	0xc0733b4493e3be70, 0xbe1e781d445aea19
	.quad	0xc0733b4821c2f800, 0xbe1e7922fca18e18
	.quad	0xc0733b4badd1bb80, 0xbe1e76fed3d40647
	.quad	0xc0733b4f3811e210, 0xbe1e793948c9eabc
	.quad	0xc0733b52c0854240, 0xbe1e76e487656b8c
	.quad	0xc0733b56472daf90, 0xbe1e780ab2f71223
	.quad	0xc0733b59cc0cfaf0, 0xbe1e77189120b09c
	.quad	0xc0733b5d4f24f270, 0xbe1e7644a0343a12
	.quad	0xc0733b60d0776160, 0xbe1e78f2a3e4733d
	.quad	0xc0733b6450061080, 0xbe1e7913b2f73ae5
	.quad	0xc0733b67cdd2c5c0, 0xbe1e7882d08393b5
	.quad	0xc0733b6b49df4470, 0xbe1e765e1b209979
	.quad	0xc0733b6ec42d4d20, 0xbe1e785c9c4620d4
	.quad	0xc0733b75b394f240, 0xbe1e78878cd0e956
	.quad	0xc0733b7c9c178630, 0xbe1e789a4112d90b
	.quad	0xc0733b837dc2b0f0, 0xbe1e79050b8a1766
	.quad	0xc0733b8a58a3f220, 0xbe1e7790dffc47aa
	.quad	0xc0733b912cc8a180, 0xbe1e77174593b06a
	.quad	0xc0733b97fa3defb0, 0xbe1e7677de2d2ecc
	.quad	0xc0733b9ec110e6b0, 0xbe1e76cff477ca18
	.quad	0xc0733ba5814e6a80, 0xbe1e78f8644dec7b
	.quad	0xc0733bac3b0339d0, 0xbe1e764e1361788d
	.quad	0xc0733bb2ee3bee30, 0xbe1e78c913e738de
	.quad	0xc0733bb99b04fd30, 0xbe1e76666f5bddaa
	.quad	0xc0733bc0416ab850, 0xbe1e77e87cbd8ab6
	.quad	0xc0733bc6e1794e10, 0xbe1e76f18ba1c966
	.quad	0xc0733bcd7b3cca10, 0xbe1e777c9461b8db
	.quad	0xc0733bd40ec115d0, 0xbe1e78b78526ffac
	.quad	0xc0733bda9c11f920, 0xbe1e7942abecfede
	.quad	0xc0733be1233b1aa0, 0xbe1e76d8a684fd8c
	.quad	0xc0733be7a4480010, 0xbe1e79622b539ac9
	.quad	0xc0733bee1f440f30, 0xbe1e7978e7cc20ea
	.quad	0xc0733bf4943a8de0, 0xbe1e765c9c9de825
	.quad	0xc0733bfb0336a290, 0xbe1e775d8b138ee2
	.quad	0xc0733c016c435500, 0xbe1e78bf33465c2f
	.quad	0xc0733c07cf6b8e80, 0xbe1e78164f7cc441
	.quad	0xc0733c0e2cba1a50, 0xbe1e7824e64d0b23
	.quad	0xc0733c148439a630, 0xbe1e78373ae7dd81
	.quad	0xc0733c1ad5f4c2c0, 0xbe1e7704513e0afe
	.quad	0xc0733c2121f5e3d0, 0xbe1e7914aa84200f
	.quad	0xc0733c2768476110, 0xbe1e76b1cde25cf6
	.quad	0xc0733c2da8f37600, 0xbe1e796120e3862d
	.quad	0xc0733c33e40442e0, 0xbe1e78ec836d7e7b
	.quad	0xc0733c3a1983cca0, 0xbe1e77fb13b7dabb
	.quad	0xc0733c40497bfd70, 0xbe1e783c6fcb2404
	.quad	0xc0733c4673f6a530, 0xbe1e7628bb93dce8
	.quad	0xc0733c4c98fd7990, 0xbe1e7857a47b5001
	.quad	0xc0733c52b89a16d0, 0xbe1e76708dc2831f
	.quad	0xc0733c58d2d5ffa0, 0xbe1e77b6038651f1
	.quad	0xc0733c5ee7ba9de0, 0xbe1e792e855bb5b2
	.quad	0xc0733c64f75142d0, 0xbe1e776cacd5c105
	.quad	0xc0733c6b01a32740, 0xbe1e77f8a8011315
	.quad	0xc0733c7106b96c30, 0xbe1e765cf3efcfde
	.quad	0xc0733c77069d1ad0, 0xbe1e78d837d2efac
	.quad	0xc0733c7d01572530, 0xbe1e78b615cf772c
	.quad	0xc0733c82f6f06640, 0xbe1e7650bbbd7a25
	.quad	0xc0733c88e771a220, 0xbe1e78bcf3495872
	.quad	0xc0733c8ed2e386c0, 0xbe1e792266832e84
	.quad	0xc0733c94b94eabd0, 0xbe1e79c1c3c2ca52
	.quad	0xc0733c9a9abb9340, 0xbe1e78aa61e5807d
	.quad	0xc0733ca07732a970, 0xbe1e7620fc4cf156
	.quad	0xc0733ca64ebc4570, 0xbe1e76b914a832c5
	.quad	0xc0733cac2160a970, 0xbe1e79227f72020e
	.quad	0xc0733cb1ef280300, 0xbe1e77ac972cc008
	.quad	0xc0733cb7b81a6b10, 0xbe1e798089be41f4
	.quad	0xc0733cbd7c3fe6a0, 0xbe1e77942ae037fe
	.quad	0xc0733cc33ba06690, 0xbe1e7956ae6463d9
	.quad	0xc0733cc8f643c850, 0xbe1e7918a50c7942
	.quad	0xc0733cceac31d5d0, 0xbe1e78308eeab604
	.quad	0xc0733cd45d7245e0, 0xbe1e76dd4ea88445
	.quad	0xc0733cda0a0cbc60, 0xbe1e77e7c1aa5909
	.quad	0xc0733cdfb208caa0, 0xbe1e7804b9d20e54
	.quad	0xc0733ce5556def70, 0xbe1e78f88e99d49c
	.quad	0xc0733ceaf4439780, 0xbe1e787d74682d68
	.quad	0xc0733cf08e911d80, 0xbe1e76edc24fe6e7
	.quad	0xc0733cf6245dca50, 0xbe1e79b347ec86d2
	.quad	0xc0733cfbb5b0d580, 0xbe1e797cceb2c39b
	.quad	0xc0733d0142916530, 0xbe1e783adbdc6aa1
	.quad	0xc0733d06cb068e70, 0xbe1e76e4c20e3d9e
	.quad	0xc0733d0c4f175570, 0xbe1e77070bf3cf61
	.quad	0xc0733d11cecaadc0, 0xbe1e781c43502734
	.quad	0xc0733d174a277a80, 0xbe1e78b11268ea72
	.quad	0xc0733d1cc1348e90, 0xbe1e7754b83bfc7d
	.quad	0xc0733d2233f8acb0, 0xbe1e7756c29bf5e9
	.quad	0xc0733d27a27a87d0, 0xbe1e7952fc1d9333
	.quad	0xc0733d2d0cc0c350, 0xbe1e778c76ae6077
	.quad	0xc0733d3272d1f2e0, 0xbe1e7a1896ba8f43
	.quad	0xc0733d37d4b49b30, 0xbe1e76dafdf432d8
	.quad	0xc0733d3d326f3180, 0xbe1e795330184013
	.quad	0xc0733d428c081c80, 0xbe1e763cc774d30f
	.quad	0xc0733d47e185b3d0, 0xbe1e77030a779c0a
	.quad	0xc0733d4d32ee40b0, 0xbe1e7908af2a2d7e
	.quad	0xc0733d528047fe00, 0xbe1e78c4953b797d
	.quad	0xc0733d57c9991850, 0xbe1e78b43b096579
	.quad	0xc0733d5d0ee7ae30, 0xbe1e7824ae0a4804
	.quad	0xc0733d625039d040, 0xbe1e79d2b2fbb740
	.quad	0xc0733d678d958190, 0xbe1e7662de59a1a6
	.quad	0xc0733d6cc700b760, 0xbe1e76b251d59aaa
	.quad	0xc0733d71fc8159b0, 0xbe1e7a00cfd1f487
	.quad	0xc0733d772e1d4360, 0xbe1e77f4d246167e
	.quad	0xc0733d7c5bda4200, 0xbe1e767a4ee8e6fc
	.quad	0xc0733d8185be1640, 0xbe1e777ccf0a8aed
	.quad	0xc0733d86abce7420, 0xbe1e767d7e279ada
	.quad	0xc0733d8bce1102d0, 0xbe1e7a05cef4bb90
	.quad	0xc0733d90ec8b5d40, 0xbe1e78f75369be5b
	.quad	0xc0733d96074311d0, 0xbe1e77b9612e8c8a
	.quad	0xc0733d9b1e3da2b0, 0xbe1e794518b9adeb
	.quad	0xc0733da031808620, 0xbe1e7810626fb934
	.quad	0xc0733da541112650, 0xbe1e76d87223fa6d
	.quad	0xc0733daa4cf4e1a0, 0xbe1e794c5e7ca3b5
	.quad	0xc0733daf55310af0, 0xbe1e789856ef816f
	.quad	0xc0733db459cae970, 0xbe1e77d2004effbd
	.quad	0xc0733db95ac7b8f0, 0xbe1e78467d31eb9c
	.quad	0xc0733dbe582caa00, 0xbe1e79aaa4e25787
	.quad	0xc0733dc351fee220, 0xbe1e762de8f107bf
	.quad	0xc0733dc848437b90, 0xbe1e7670670a63fe
	.quad	0xc0733dcd3aff85d0, 0xbe1e795ca237c6cc
	.quad	0xc0733dd22a3805b0, 0xbe1e77e55c53c1d9
	.quad	0xc0733dd715f1f520, 0xbe1e78a806213ac4
	.quad	0xc0733ddbfe3243b0, 0xbe1e77743a2bc615
	.quad	0xc0733de0e2fdd660, 0xbe1e78b8b45b0b7d
	.quad	0xc0733de5c4598800, 0xbe1e78d635f2f4b9
	.quad	0xc0733deaa24a2920, 0xbe1e7758c396a11e
	.quad	0xc0733def7cd48020, 0xbe1e7a17a8cc454c
	.quad	0xc0733df453fd49a0, 0xbe1e783caa73f616
	.quad	0xc0733df927c93820, 0xbe1e7932cfa29664
	.quad	0xc0733dfdf83cf490, 0xbe1e777d265c72a6
	.quad	0xc0733e02c55d1e10, 0xbe1e7775e7c03c60
	.quad	0xc0733e078f2e4a40, 0xbe1e79f65d52d232
	.quad	0xc0733e0c55b50570, 0xbe1e76e7e7464b4e
	.quad	0xc0733e1118f5d250, 0xbe1e77be81cad877
	.quad	0xc0733e15d8f52a80, 0xbe1e79dd25b5fb3a
	.quad	0xc0733e1a95b77e80, 0xbe1e78e45f1418ef
	.quad	0xc0733e1f4f4135a0, 0xbe1e78eb7289505b
	.quad	0xc0733e240596ae50, 0xbe1e78a468c07cad
	.quad	0xc0733e28b8bc3e20, 0xbe1e776b558a4009
	.quad	0xc0733e2d68b631d0, 0xbe1e77412eb9941e
	.quad	0xc0733e321588cd80, 0xbe1e76b2853f845e
	.quad	0xc0733e36bf384cb0, 0xbe1e76aa7184273c
	.quad	0xc0733e3b65c8e260, 0xbe1e7832027f78fa
	.quad	0xc0733e40093eb930, 0xbe1e7a1c7da131f5
	.quad	0xc0733e44a99df380, 0xbe1e76a0bc2ae4bc
	.quad	0xc0733e4946eaab30, 0xbe1e78dff13b6f5d
	.quad	0xc0733e4de128f250, 0xbe1e765a226dea2c
	.quad	0xc0733e52785cd290, 0xbe1e78509b989111
	.quad	0xc0733e570c8a4de0, 0xbe1e7916a4e9803d
	.quad	0xc0733e5b9db55e30, 0xbe1e7950c15758cc
	.quad	0xc0733e602be1f5a0, 0xbe1e7922ba1ad420
	.quad	0xc0733e64b713fe90, 0xbe1e794cbaabcef6
	.quad	0xc0733e693f4f5bc0, 0xbe1e7837bf883fed
	.quad	0xc0733e6dc497e850, 0xbe1e76f198ddbbdf
	.quad	0xc0733e7246f177d0, 0xbe1e7a18c1067764
	.quad	0xc0733e76c65fd6a0, 0xbe1e76b845a8fd9d
	.quad	0xc0733e7b42e6c970, 0xbe1e7714012df506
	.quad	0xc0733e7fbc8a0de0, 0xbe1e7765612922cd
	.quad	0xc0733e84334d5a50, 0xbe1e7688f5424a00
	.quad	0xc0733e88a7345df0, 0xbe1e769d011f6663
	.quad	0xc0733e8d1842c0e0, 0xbe1e79914acbfaf7
	.quad	0xc0733e91867c2460, 0xbe1e79a85e189bd7
	.quad	0xc0733e95f1e422a0, 0xbe1e79ea7c726432
	.quad	0xc0733e9a5a7e4f10, 0xbe1e768a6fbb8e6e
	.quad	0xc0733e9ec04e3620, 0xbe1e793c75bcc9fc
	.quad	0xc0733ea323575dd0, 0xbe1e797f78da13d4
	.quad	0xc0733ea7839d4550, 0xbe1e78d8c9cda978
	.quad	0xc0733eabe1236540, 0xbe1e77028d480fff
	.quad	0xc0733eb03bed2fa0, 0xbe1e7a0d0f74ff7c
	.quad	0xc0733eb493fe1040, 0xbe1e76732e8a35fb
	.quad	0xc0733eb8e9596c30, 0xbe1e77220caeabeb
	.quad	0xc0733ebd3c02a260, 0xbe1e797438b645ef
	.quad	0xc0733ec18bfd0b80, 0xbe1e79207c5fd6e8
	.quad	0xc0733ec5d94bf9f0, 0xbe1e781c7df8f946
	.quad	0xc0733eca23f2b9f0, 0xbe1e76736284e2db
	.quad	0xc0733ece6bf49190, 0xbe1e7a109cc0c3f5
	.quad	0xc0733ed2b154c120, 0xbe1e767f14a16d50
	.quad	0xc0733ed6f4168290, 0xbe1e789cd22acaf0
	.quad	0xc0733edb343d0a40, 0xbe1e764355ca28ad
	.quad	0xc0733edf71cb8660, 0xbe1e79e4c7a81c45
	.quad	0xc0733ee3acc51fb0, 0xbe1e761e26b644c2
	.quad	0xc0733ee7e52cf8c0, 0xbe1e793e9f8fbdd3
	.quad	0xc0733eec1b062ed0, 0xbe1e78c432991c20
	.quad	0xc0733ef04e53d940, 0xbe1e78cdd025f4d8
	.quad	0xc0733ef47f1909f0, 0xbe1e778310c6446e
	.quad	0xc0733ef8ad58cd20, 0xbe1e7871af3d6e17
	.quad	0xc0733efcd91629b0, 0xbe1e77e0e906f697
	.quad	0xc0733f01025420f0, 0xbe1e7a1ae9b27892
	.quad	0xc0733f052915af00, 0xbe1e76ac64c88f9d
	.quad	0xc0733f094d5dca60, 0xbe1e779a815589c4
	.quad	0xc0733f0d6f2f6480, 0xbe1e788f39a4864c
	.quad	0xc0733f118e8d6980, 0xbe1e79fc51263525
	.quad	0xc0733f15ab7ac060, 0xbe1e783501f19e90
	.quad	0xc0733f19c5fa4ae0, 0xbe1e767e82c327ab
	.quad	0xc0733f1dde0ee5a0, 0xbe1e7a1785d66123
	.quad	0xc0733f21f3bb6870, 0xbe1e7936d07203da
	.quad	0xc0733f260702a5e0, 0xbe1e7a010a7ac699
	.quad	0xc0733f2a17e76bb0, 0xbe1e7975e4e16312
	.quad	0xc0733f2e266c82b0, 0xbe1e7654b5422330
	.quad	0xc0733f323294aeb0, 0xbe1e77f8a4909d35
	.quad	0xc0733f363c62aee0, 0xbe1e792c8e30d226
	.quad	0xc0733f3a43d93da0, 0xbe1e76f6ac67a1ff
	.quad	0xc0733f3e48fb1070, 0xbe1e775c2e97715a
	.quad	0xc0733f424bcad840, 0xbe1e781cd54ae100
	/* Log_LA_table */
	.align	16
	.quad	0x0000000000000000
	.quad	0xbf4bc48a867884b7
	.quad	0xbf5bbd9e9482af09
	.quad	0xbf64c9096b94befd
	.quad	0xbf6bafd47221ed26
	.quad	0xbf714999e2ad8ea6
	.quad	0xbf74b99563d2a1bd
	.quad	0xbf7827de6b310350
	.quad	0xbf7b9476a4fcd10f
	.quad	0xbf7eff5fbaf25781
	.quad	0xbf81344daa2d7553
	.quad	0xbf82e8158b08d957
	.quad	0xbf849b0851443684
	.quad	0xbf864d26cce610dd
	.quad	0xbf87fe71ccc4e6b0
	.quad	0xbf89aeea1e897fdf
	.quad	0xbf8b5e908eb13790
	.quad	0xbf8d0d65e890405a
	.quad	0xbf8ebb6af653e2ee
	.quad	0xbf90345040825bad
	.quad	0xbf910a83a8446c78
	.quad	0xbf91e05015d30a71
	.quad	0xbf92b5b5ec0209d3
	.quad	0xbf938ab58d173e91
	.quad	0xbf945f4f5acb8be0
	.quad	0xbf953383b64bf13f
	.quad	0xbf960753003a94ef
	.quad	0xbf96dabd98afcc05
	.quad	0xbf97adc3df3b1ff8
	.quad	0xbf98806632e451d0
	.quad	0xbf9952a4f22c5ae9
	.quad	0xbf9a24807b0e6b5c
	.quad	0xbf9af5f92b00e610
	.quad	0xbf9bc70f5ef65a77
	.quad	0xbf9c97c3735e7c0a
	.quad	0xbf9d6815c4271775
	.quad	0xbf9e3806acbd058f
	.quad	0xbf9f0796880d1c19
	.quad	0xbf9fd6c5b0851c4c
	.quad	0xbfa052ca400a4f9b
	.quad	0xbfa0ba01a8170000
	.quad	0xbfa121093ce3a205
	.quad	0xbfa187e12aad8077
	.quad	0xbfa1ee899d74a03e
	.quad	0xbfa25502c0fc314c
	.quad	0xbfa2bb4cc0cafe8d
	.quad	0xbfa32167c82bdcda
	.quad	0xbfa38754022e18e2
	.quad	0xbfa3ed1199a5e425
	.quad	0xbfa452a0b92cc0ec
	.quad	0xbfa4b8018b21ed4f
	.quad	0xbfa51d3439aacd4a
	.quad	0xbfa58238eeb353da
	.quad	0xbfa5e70fd3ee6b34
	.quad	0xbfa64bb912d65c07
	.quad	0xbfa6b034d4ad33df
	.quad	0xbfa71483427d2a99
	.quad	0xbfa778a4851906f3
	.quad	0xbfa7dc98c51c8242
	.quad	0xbfa840602aecab3d
	.quad	0xbfa8a3fadeb847f4
	.quad	0xbfa90769087836e4
	.quad	0xbfa96aaacfefcf3c
	.quad	0xbfa9cdc05cad4042
	.quad	0xbfaa30a9d609efea
	.quad	0xbfaa9367632ad897
	.quad	0xbfaaf5f92b00e610
	.quad	0xbfab585f544951a4
	.quad	0xbfabba9a058dfd84
	.quad	0xbfac1ca96525cf56
	.quad	0xbfac7e8d993509f9
	.quad	0xbface046c7ada68d
	.quad	0xbfad41d5164facb4
	.quad	0xbfada338aaa98a0c
	.quad	0xbfae0471aa1868f5
	.quad	0xbfae658039c88690
	.quad	0xbfaec6647eb58808
	.quad	0xbfaf271e9daacf20
	.quad	0xbfaf87aebb43ce06
	.quad	0xbfafe814fbec5a77
	.quad	0xbfb02428c1f08016
	.quad	0xbfb054323b97a948
	.quad	0xbfb08426fcdb1ee7
	.quad	0xbfb0b40717932b96
	.quad	0xbfb0e3d29d81165e
	.quad	0xbfb11389a04f4a2e
	.quad	0xbfb1432c31917d08
	.quad	0xbfb172ba62c4d6de
	.quad	0xbfb1a23445501816
	.quad	0xbfb1d199ea83bfbe
	.quad	0xbfb200eb639a3173
	.quad	0xbfb23028c1b7daed
	.quad	0xbfb25f5215eb594a
	.quad	0xbfb28e67712d9dfc
	.quad	0xbfb2bd68e4621371
	.quad	0xbfb2ec568056c16f
	.quad	0xbfb31b3055c47118
	.quad	0xbfb349f6754ed0b4
	.quad	0xbfb378a8ef84971e
	.quad	0xbfb3a747d4dfa6f5
	.quad	0xbfb3d5d335c53179
	.quad	0xbfb4044b2285d925
	.quad	0xbfb432afab5dd3ff
	.quad	0xbfb46100e0750da1
	.quad	0xbfb48f3ed1df48fb
	.quad	0xbfb4bd698f9c41cf
	.quad	0xbfb4eb812997cde4
	.quad	0xbfb51985afa9fdfd
	.quad	0xbfb5477731973e85
	.quad	0xbfb57555bf1077f5
	.quad	0xbfb5a32167b32f02
	.quad	0xbfb5d0da3b09a47e
	.quad	0xbfb5fe80488af4fd
	.quad	0xbfb62c139f9b3837
	.quad	0xbfb659944f8ba02d
	.quad	0xbfb68702679a980a
	.quad	0xbfb6b45df6f3e2c9
	.quad	0xbfb6e1a70cb0b99a
	.quad	0xbfb70eddb7d7ea07
	.quad	0xbfb73c02075df3e5
	.quad	0xbfb769140a2526fd
	.quad	0xbfb79613cefdc07d
	.quad	0xbfb7c30164a60836
	.quad	0xbfb7efdcd9ca6d8f
	.quad	0xbfb81ca63d05a44a
	.quad	0xbfb8495d9ce0c10c
	.quad	0xbfb8760307d355ab
	.quad	0xbfb8a2968c438d41
	.quad	0xbfb8cf183886480d
	.quad	0xbfb8fb881adf3713
	.quad	0xbfb927e64180f790
	.quad	0xbfb95432ba8d2e2f
	.quad	0xbfb9806d9414a209
	.quad	0xbfb9ac96dc175776
	.quad	0xbfb9d8aea084aa9c
	.quad	0xbfba04b4ef3b69d8
	.quad	0xbfba30a9d609efea
	.quad	0xbfba5c8d62ae3dec
	.quad	0xbfba885fa2d6151e
	.quad	0xbfbab420a41f1076
	.quad	0xbfbadfd07416be07
	.quad	0xbfbb0b6f203ab82c
	.quad	0xbfbb36fcb5f8be8a
	.quad	0xbfbb627942aecedd
	.quad	0xbfbb8de4d3ab3d98
	.quad	0xbfbbb93f762cce4f
	.quad	0xbfbbe4893762cbf7
	.quad	0xbfbc0fc2246d20f5
	.quad	0xbfbc3aea4a5c6eff
	.quad	0xbfbc6601b63226cb
	.quad	0xbfbc910874e09f98
	.quad	0xbfbcbbfe934b2e81
	.quad	0xbfbce6e41e463da5
	.quad	0xbfbd11b92297632b
	.quad	0xbfbd3c7dacf5780b
	.quad	0xbfbd6731ca08aeb9
	.quad	0xbfbd91d5866aa99c
	.quad	0xbfbdbc68eea6915b
	.quad	0xbfbde6ec0f392b05
	.quad	0xbfbe115ef490ee07
	.quad	0xbfbe3bc1ab0e19fe
	.quad	0xbfbe66143f02cc5d
	.quad	0xbfbe9056bcb315e8
	.quad	0xbfbeba893055100b
	.quad	0xbfbee4aba610f204
	.quad	0xbfbf0ebe2a0125eb
	.quad	0xbfbf38c0c8325d86
	.quad	0xbfbf62b38ca3a706
	.quad	0xbfbf8c9683468191
	.quad	0xbfbfb669b7fef1a8
	.quad	0xbfbfe02d36a3956d
	.quad	0xbfc004f0857edc5c
	.quad	0xbfc019c2a064b486
	.quad	0xbfc02e8cf1dac4b8
	.quad	0xbfc0434f7fb1f307
	.quad	0xbfc0580a4fb4a3df
	.quad	0xbfc06cbd67a6c3b6
	.quad	0xbfc08168cd45d0a9
	.quad	0xbfc0960c8648e406
	.quad	0xbfc0aaa89860bbcf
	.quad	0xbfc0bf3d0937c41c
	.quad	0xbfc0d3c9de722078
	.quad	0xbfc0e84f1dadb526
	.quad	0xbfc0fccccc823059
	.quad	0xbfc11142f0811357
	.quad	0xbfc125b18f35bb8e
	.quad	0xbfc13a18ae256b99
	.quad	0xbfc14e7852cf5430
	.quad	0xbfc162d082ac9d10
	.quad	0xbfc1772143306dc6
	.quad	0xbfc18b6a99c7f679
	.quad	0xbfc19fac8bda7897
	.quad	0xbfc1b3e71ec94f7b
	.quad	0xbfc1c81a57eff8fd
	.quad	0xbfc1dc463ca41df8
	.quad	0xbfc1f06ad2359abd
	.quad	0xbfc204881dee8777
	.quad	0xbfc2189e25134081
	.quad	0xbfc22cacece26ead
	.quad	0xbfc240b47a950f79
	.quad	0xbfc254b4d35e7d3c
	.quad	0xbfc268adfc6c773e
	.quad	0xbfc27c9ffae729c1
	.quad	0xbfc2908ad3f13603
	.quad	0xbfc2a46e8ca7ba2a
	.quad	0xbfc2b84b2a225923
	.quad	0xbfc2cc20b1734279
	.quad	0xbfc2dfef27a73a18
	.quad	0xbfc2f3b691c5a001
	.quad	0xbfc30776f4d077f7
	.quad	0xbfc31b3055c47118
	.quad	0xbfc32ee2b998ed6e
	.quad	0xbfc3428e2540096d
	.quad	0x3fc331f403985097
	.quad	0x3fc31e56798a910a
	.quad	0x3fc30abfd8f333b6
	.quad	0x3fc2f7301cf4e87b
	.quad	0x3fc2e3a740b7800f
	.quad	0x3fc2d0253f67e4cb
	.quad	0x3fc2bcaa14381386
	.quad	0x3fc2a935ba5f1479
	.quad	0x3fc295c82d18f434
	.quad	0x3fc2826167a6bc9c
	.quad	0x3fc26f01654e6df6
	.quad	0x3fc25ba8215af7fc
	.quad	0x3fc24855971c3307
	.quad	0x3fc23509c1e6d937
	.quad	0x3fc221c49d147fb3
	.quad	0x3fc20e8624038fed
	.quad	0x3fc1fb4e521740f4
	.quad	0x3fc1e81d22b790d4
	.quad	0x3fc1d4f291513e01
	.quad	0x3fc1c1ce9955c0c6
	.quad	0x3fc1aeb1363b44c8
	.quad	0x3fc19b9a637ca295
	.quad	0x3fc1888a1c995931
	.quad	0x3fc175805d1587c1
	.quad	0x3fc1627d2079e731
	.quad	0x3fc14f806253c3ed
	.quad	0x3fc13c8a1e34f7a0
	.quad	0x3fc1299a4fb3e306
	.quad	0x3fc116b0f26b67bb
	.quad	0x3fc103ce01fae223
	.quad	0x3fc0f0f17a062353
	.quad	0x3fc0de1b56356b04
	.quad	0x3fc0cb4b9235619a
	.quad	0x3fc0b88229b71227
	.quad	0x3fc0a5bf186fe483
	.quad	0x3fc093025a19976c
	.quad	0x3fc0804bea723aa9
	.quad	0x3fc06d9bc53c2941
	.quad	0x3fc05af1e63e03b4
	.quad	0x3fc0484e4942aa43
	.quad	0x3fc035b0ea19373b
	.quad	0x3fc02319c494f951
	.quad	0x3fc01088d48d6e03
	.quad	0x3fbffbfc2bbc7803
	.quad	0x3fbfd6f308ce5b52
	.quad	0x3fbfb1f6381856f4
	.quad	0x3fbf8d05b16a6d47
	.quad	0x3fbf68216c9cc727
	.quad	0x3fbf4349618fa91a
	.quad	0x3fbf1e7d882b689a
	.quad	0x3fbef9bdd860616b
	.quad	0x3fbed50a4a26eafc
	.quad	0x3fbeb062d57f4de8
	.quad	0x3fbe8bc77271b97a
	.quad	0x3fbe6738190e394c
	.quad	0x3fbe42b4c16caaf3
	.quad	0x3fbe1e3d63acb3ba
	.quad	0x3fbdf9d1f7f5b674
	.quad	0x3fbdd5727676c959
	.quad	0x3fbdb11ed766abf4
	.quad	0x3fbd8cd71303bd26
	.quad	0x3fbd689b2193f133
	.quad	0x3fbd446afb64c7e5
	.quad	0x3fbd204698cb42bd
	.quad	0x3fbcfc2df223db2d
	.quad	0x3fbcd820ffd278f3
	.quad	0x3fbcb41fba42686d
	.quad	0x3fbc902a19e65111
	.quad	0x3fbc6c4017382bea
	.quad	0x3fbc4861aab93a23
	.quad	0x3fbc248eccf1fba6
	.quad	0x3fbc00c7767225cb
	.quad	0x3fbbdd0b9fd09a10
	.quad	0x3fbbb95b41ab5ce6
	.quad	0x3fbb95b654a78c87
	.quad	0x3fbb721cd17157e3
	.quad	0x3fbb4e8eb0bbf58f
	.quad	0x3fbb2b0beb419ad0
	.quad	0x3fbb079479c372ad
	.quad	0x3fbae4285509950b
	.quad	0x3fbac0c775e2fde6
	.quad	0x3fba9d71d5258484
	.quad	0x3fba7a276badd2c8
	.quad	0x3fba56e8325f5c87
	.quad	0x3fba33b4222456f1
	.quad	0x3fba108b33edb005
	.quad	0x3fb9ed6d60b30612
	.quad	0x3fb9ca5aa1729f45
	.quad	0x3fb9a752ef316149
	.quad	0x3fb9845642fac8f0
	.quad	0x3fb9616495e0e1e8
	.quad	0x3fb93e7de0fc3e80
	.quad	0x3fb91ba21d6bef77
	.quad	0x3fb8f8d144557bdf
	.quad	0x3fb8d60b4ee4d901
	.quad	0x3fb8b350364c6257
	.quad	0x3fb8909ff3c4d191
	.quad	0x3fb86dfa808d36a0
	.quad	0x3fb84b5fd5eaefd8
	.quad	0x3fb828cfed29a215
	.quad	0x3fb8064abf9b30f1
	.quad	0x3fb7e3d04697b704
	.quad	0x3fb7c1607b7d7e32
	.quad	0x3fb79efb57b0f803
	.quad	0x3fb77ca0d49cb608
	.quad	0x3fb75a50ebb1624a
	.quad	0x3fb7380b9665b7c8
	.quad	0x3fb715d0ce367afc
	.quad	0x3fb6f3a08ca67270
	.quad	0x3fb6d17acb3e5f5e
	.quad	0x3fb6af5f838cf654
	.quad	0x3fb68d4eaf26d7ee
	.quad	0x3fb66b4847a68997
	.quad	0x3fb6494c46ac6e4d
	.quad	0x3fb6275aa5debf81
	.quad	0x3fb605735ee985f1
	.quad	0x3fb5e3966b7e9295
	.quad	0x3fb5c1c3c5557799
	.quad	0x3fb59ffb662b815c
	.quad	0x3fb57e3d47c3af7b
	.quad	0x3fb55c8963e6adeb
	.quad	0x3fb53adfb462ce16
	.quad	0x3fb51940330c000b
	.quad	0x3fb4f7aad9bbcbaf
	.quad	0x3fb4d61fa2514a00
	.quad	0x3fb4b49e86b11e5f
	.quad	0x3fb4932780c56fe2
	.quad	0x3fb471ba8a7de2b7
	.quad	0x3fb450579dcf9186
	.quad	0x3fb42efeb4b506e9
	.quad	0x3fb40dafc92e36e2
	.quad	0x3fb3ec6ad5407868
	.quad	0x3fb3cb2fd2f67ef1
	.quad	0x3fb3a9febc60540a
	.quad	0x3fb388d78b9350ff
	.quad	0x3fb367ba3aaa1883
	.quad	0x3fb346a6c3c49066
	.quad	0x3fb3259d2107db54
	.quad	0x3fb3049d4c9e52a0
	.quad	0x3fb2e3a740b7800f
	.quad	0x3fb2c2baf78817b7
	.quad	0x3fb2a1d86b49f1e2
	.quad	0x3fb280ff963c04fc
	.quad	0x3fb2603072a25f82
	.quad	0x3fb23f6afac6220a
	.quad	0x3fb21eaf28f57941
	.quad	0x3fb1fdfcf7839804
	.quad	0x3fb1dd5460c8b16f
	.quad	0x3fb1bcb55f21f307
	.quad	0x3fb19c1fecf17ee0
	.quad	0x3fb17b94049e65d0
	.quad	0x3fb15b11a094a1aa
	.quad	0x3fb13a98bb450f81
	.quad	0x3fb11a294f2569f6
	.quad	0x3fb0f9c356b04389
	.quad	0x3fb0d966cc6500fa
	.quad	0x3fb0b913aac7d3a7
	.quad	0x3fb098c9ec61b3ff
	.quad	0x3fb078898bc05bf4
	.quad	0x3fb0585283764178
	.quad	0x3fb03824ce1a9101
	.quad	0x3fb0180066492817
	.quad	0x3fafefca8d451fd6
	.quad	0x3fafafa6d397efdb
	.quad	0x3faf6f9594de60f0
	.quad	0x3faf2f96c6754aee
	.quad	0x3faeefaa5dc2b239
	.quad	0x3faeafd05035bd3b
	.quad	0x3fae70089346a9e6
	.quad	0x3fae30531c76c34a
	.quad	0x3fadf0afe1505738
	.quad	0x3fadb11ed766abf4
	.quad	0x3fad719ff455f5f7
	.quad	0x3fad32332dc34dbd
	.quad	0x3facf2d8795ca5a5
	.quad	0x3facb38fccd8bfdb
	.quad	0x3fac74591df72456
	.quad	0x3fac3534628016dd
	.quad	0x3fabf62190448d22
	.quad	0x3fabb7209d1e24e5
	.quad	0x3fab78317eef1a29
	.quad	0x3fab39542ba23d73
	.quad	0x3faafa88992aea19
	.quad	0x3faabbcebd84fca0
	.quad	0x3faa7d268eb4c924
	.quad	0x3faa3e9002c711d2
	.quad	0x3faa000b0fd0fd6b
	.quad	0x3fa9c197abf00dd7
	.quad	0x3fa98335cd4a16c3
	.quad	0x3fa944e56a0d3450
	.quad	0x3fa906a6786fc1cb
	.quad	0x3fa8c878eeb05074
	.quad	0x3fa88a5cc3159e53
	.quad	0x3fa84c51ebee8d15
	.quad	0x3fa80e585f9218fc
	.quad	0x3fa7d070145f4fd7
	.quad	0x3fa7929900bd4809
	.quad	0x3fa754d31b1b179c
	.quad	0x3fa7171e59efcb5f
	.quad	0x3fa6d97ab3ba5e10
	.quad	0x3fa69be81f01af99
	.quad	0x3fa65e6692547c4e
	.quad	0x3fa620f604495440
	.quad	0x3fa5e3966b7e9295
	.quad	0x3fa5a647be9a54f6
	.quad	0x3fa56909f44a72fe
	.quad	0x3fa52bdd034475b8
	.quad	0x3fa4eec0e2458f30
	.quad	0x3fa4b1b588129203
	.quad	0x3fa474baeb77e904
	.quad	0x3fa437d103498eec
	.quad	0x3fa3faf7c663060e
	.quad	0x3fa3be2f2ba7501f
	.quad	0x3fa381772a00e604
	.quad	0x3fa344cfb861afae
	.quad	0x3fa30838cdc2fbfd
	.quad	0x3fa2cbb2612578b4
	.quad	0x3fa28f3c69912a74
	.quad	0x3fa252d6de1564c1
	.quad	0x3fa21681b5c8c213
	.quad	0x3fa1da3ce7c91bf8
	.quad	0x3fa19e086b3b8333
	.quad	0x3fa161e4374c37f4
	.quad	0x3fa125d0432ea20e
	.quad	0x3fa0e9cc861d4944
	.quad	0x3fa0add8f759cd95
	.quad	0x3fa071f58e2cdf9b
	.quad	0x3fa0362241e638ec
	.quad	0x3f9ff4be13b92920
	.quad	0x3f9f7d57badb4ee8
	.quad	0x3f9f061167fc31e8
	.quad	0x3f9e8eeb09f2f6cb
	.quad	0x3f9e17e48fa48962
	.quad	0x3f9da0fde8038de9
	.quad	0x3f9d2a3702105259
	.quad	0x3f9cb38fccd8bfdb
	.quad	0x3f9c3d0837784c41
	.quad	0x3f9bc6a03117eb97
	.quad	0x3f9b5057a8ee01ce
	.quad	0x3f9ada2e8e3e546f
	.quad	0x3f9a6424d059fc68
	.quad	0x3f99ee3a5e9f57e8
	.quad	0x3f99786f2879fc53
	.quad	0x3f9902c31d62a843
	.quad	0x3f988d362cdf359e
	.quad	0x3f9817c846828bbd
	.quad	0x3f97a27959ec91aa
	.quad	0x3f972d4956ca2067
	.quad	0x3f96b8382cd4f551
	.quad	0x3f964345cbd3a491
	.quad	0x3f95ce7223998b98
	.quad	0x3f9559bd2406c3ba
	.quad	0x3f94e526bd0814d1
	.quad	0x3f9470aede96e7f2
	.quad	0x3f93fc5578b93a38
	.quad	0x3f93881a7b818f9e
	.quad	0x3f9313fdd70ee5e8
	.quad	0x3f929fff7b8ca79d
	.quad	0x3f922c1f59329f1b
	.quad	0x3f91b85d6044e9ae
	.quad	0x3f9144b98113eac0
	.quad	0x3f90d133abfc3f1b
	.quad	0x3f905dcbd166b033
	.quad	0x3f8fd503c3904f1d
	.quad	0x3f8eeeab9b43445d
	.quad	0x3f8e088f0b004827
	.quad	0x3f8d22adf3f9579d
	.quad	0x3f8c3d0837784c41
	.quad	0x3f8b579db6dec358
	.quad	0x3f8a726e53a6056e
	.quad	0x3f898d79ef5eedf0
	.quad	0x3f88a8c06bb1d2f4
	.quad	0x3f87c441aa5e6d15
	.quad	0x3f86dffd8d3bbf70
	.quad	0x3f85fbf3f637ffc5
	.quad	0x3f851824c7587eb0
	.quad	0x3f84348fe2b99002
	.quad	0x3f8351352a8e733f
	.quad	0x3f826e1481213c2e
	.quad	0x3f818b2dc8d2bb91
	.quad	0x3f80a880e41a67f6
	.quad	0x3f7f8c1b6b0c8d4e
	.quad	0x3f7dc7a83f75a96d
	.quad	0x3f7c03a80ae5e054
	.quad	0x3f7a401a92ff827e
	.quad	0x3f787cff9d9147a5
	.quad	0x3f76ba56f09621bc
	.quad	0x3f74f8205235102d
	.quad	0x3f73365b88c0f347
	.quad	0x3f7175085ab85ff0
	.quad	0x3f6f684d1d8ae702
	.quad	0x3f6be76bd77b4fc3
	.quad	0x3f68676c71434fb9
	.quad	0x3f64e84e793a474a
	.quad	0x3f616a117e0d4b30
	.quad	0x3f5bd96a1d7d9cbc
	.quad	0x3f54e071754c98ba
	.quad	0x3f4bd27045bfd025
	.quad	0x3f3bcef518e29612
	.quad	0x8000000000000000
	/* poly_coeff[5] */
	.align	16
	.quad	0x3fb63C65231FBD16, 0x3fb63C65231FBD16 /* coeff5 */
	.quad	0xbfbBCB7D4EFBE80B, 0xbfbBCB7D4EFBE80B /* coeff4 */
	.quad	0x3fc287A7636F341E, 0x3fc287A7636F341E /* coeff3 */
	.quad	0xbfcBCB7B1526DE36, 0xbfcBCB7B1526DE36 /* coeff2 */
	.quad	0x3fdBCB7B1526E50E, 0x3fdBCB7B1526E50E /* coeff1 */
	/* ExpMask */
	.align	16
	.quad	0x000fffffffffffff, 0x000fffffffffffff
	/* Two10 */
	.align	16
	.quad	0x3f50000000000000, 0x3f50000000000000
	/* MinNorm */
	.align	16
	.quad	0x0010000000000000, 0x0010000000000000
	/* MaxNorm */
	.align	16
	.quad	0x7fefffffffffffff, 0x7fefffffffffffff
	/* HalfMask */
	.align	16
	.quad	0xfffffffffc000000, 0xfffffffffc000000
	/* One */
	.align	16
	.quad	0x3ff0000000000000, 0x3ff0000000000000
	/* Threshold */
	.align	16
	.quad	0x4086a00000000000, 0x4086a00000000000
	/* Bias */
	.align	16
	.quad	0x408ff80000000000, 0x408ff80000000000
	/* Bias1 */
	.align	16
	.quad	0x408ff00000000000, 0x408ff00000000000
	/* L2 */
	.align	16
	.quad	0x3fd34413509f79ff, 0x3fd34413509f79ff
	.align	16
	.type	__svml_dlog10_data_internal, @object
	.size	__svml_dlog10_data_internal, .-__svml_dlog10_data_internal
	.space	48, 0x00
	.align	16

.FLT_12:
	.long	0x00000000, 0x43380000, 0x00000000, 0x43380000
	.type	.FLT_12, @object
	.size	.FLT_12, 16
