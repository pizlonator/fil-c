/* Function log2 vectorized with AVX2.
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
 *    log2(x) = k - log2(Rcp) + poly_approximation(R)
 *       log2(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_dlog2_data_internal
 */
#define Log_HA_table			0
#define Log_LA_table			8224
#define poly_coeff			12352
#define ExpMask				12512
#define Two10				12544
#define MinNorm				12576
#define MaxNorm				12608
#define HalfMask			12640
#define One				12672
#define Threshold			12704
#define Bias				12736
#define Bias1				12768

/* Lookup bias for data table __svml_dlog2_data_internal.  */
#define Table_Lookup_Bias		-0x405fe0

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4v_log2_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp
	lea	Table_Lookup_Bias+__svml_dlog2_data_internal(%rip), %r8
	vmovapd	%ymm0, %ymm3

	/* preserve mantissa, set input exponent to 2^(-10) */
	vandpd	ExpMask+__svml_dlog2_data_internal(%rip), %ymm3, %ymm4
	vorpd	Two10+__svml_dlog2_data_internal(%rip), %ymm4, %ymm2

	/* reciprocal approximation good to at least 11 bits */
	vcvtpd2ps %ymm2, %xmm5

	/* exponent bits */
	vpsrlq	$20, %ymm3, %ymm7
	vmovupd	One+__svml_dlog2_data_internal(%rip), %ymm14
	vrcpps	%xmm5, %xmm6

	/* check range */
	vcmplt_oqpd MinNorm+__svml_dlog2_data_internal(%rip), %ymm3, %ymm11
	vcmpnle_uqpd MaxNorm+__svml_dlog2_data_internal(%rip), %ymm3, %ymm12
	vcvtps2pd %xmm6, %ymm9

	/* round reciprocal to nearest integer, will have 1+9 mantissa bits */
	vroundpd $0, %ymm9, %ymm1

	/* exponent */
	vmovupd	Threshold+__svml_dlog2_data_internal(%rip), %ymm9

	/*
	 * prepare table index
	 * table lookup
	 */
	vpsrlq	$40, %ymm1, %ymm15

	/* argument reduction */
	vfmsub213pd %ymm14, %ymm1, %ymm2

	/* polynomial */
	vmovupd	poly_coeff+__svml_dlog2_data_internal(%rip), %ymm14
	vcmplt_oqpd %ymm1, %ymm9, %ymm1
	vfmadd213pd poly_coeff+32+__svml_dlog2_data_internal(%rip), %ymm2, %ymm14
	vorpd	%ymm12, %ymm11, %ymm13
	vmulpd	%ymm2, %ymm2, %ymm12

	/* combine and get argument value range mask */
	vmovmskpd %ymm13, %eax
	vextractf128 $1, %ymm7, %xmm8
	vshufps	$221, %xmm8, %xmm7, %xmm10

	/* biased exponent in DP format */
	vcvtdq2pd %xmm10, %ymm0
	vandpd	Bias+__svml_dlog2_data_internal(%rip), %ymm1, %ymm10
	vorpd	Bias1+__svml_dlog2_data_internal(%rip), %ymm10, %ymm11
	vsubpd	%ymm11, %ymm0, %ymm1
	vmovupd	poly_coeff+64+__svml_dlog2_data_internal(%rip), %ymm0
	vfmadd213pd poly_coeff+96+__svml_dlog2_data_internal(%rip), %ymm2, %ymm0
	vmulpd	poly_coeff+128+__svml_dlog2_data_internal(%rip), %ymm2, %ymm2
	vfmadd213pd %ymm0, %ymm12, %ymm14
	vfmadd213pd %ymm2, %ymm12, %ymm14
	vextractf128 $1, %ymm15, %xmm6
	vmovd	%xmm15, %edx
	vmovd	%xmm6, %esi
	movslq	%edx, %rdx
	vpextrd	$2, %xmm15, %ecx
	movslq	%esi, %rsi
	vpextrd	$2, %xmm6, %edi
	movslq	%ecx, %rcx
	movslq	%edi, %rdi
	vmovsd	(%r8, %rdx), %xmm4
	vmovsd	(%r8, %rsi), %xmm7
	vmovhpd	(%r8, %rcx), %xmm4, %xmm5
	vmovhpd	(%r8, %rdi), %xmm7, %xmm8
	vinsertf128 $1, %xmm8, %ymm5, %ymm13

	/* reconstruction */
	vaddpd	%ymm14, %ymm13, %ymm0
	vaddpd	%ymm0, %ymm1, %ymm0
	testl	%eax, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 eax ymm0 ymm3

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
	vmovupd	%ymm3, 32(%rsp)
	vmovupd	%ymm0, 64(%rsp)
	# LOE rbx r12 r13 r14 r15 eax ymm0

	xorl	%edx, %edx
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -80; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xb0, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -88; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xa8, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r13d
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
	vmovupd	64(%rsp), %ymm0

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
	vmovsd	32(%rsp, %r14, 8), %xmm0
	call	log2@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 64(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN4v_log2_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_dlog2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 Log_HA_table[(1<<10)+2][2];
	__declspec(align(32)) VUINT32 Log_LA_table[(1<<9)+1][2];
	__declspec(align(32)) VUINT32 poly_coeff[5][4][2];
	__declspec(align(32)) VUINT32 ExpMask[4][2];
	__declspec(align(32)) VUINT32 Two10[4][2];
	__declspec(align(32)) VUINT32 MinNorm[4][2];
	__declspec(align(32)) VUINT32 MaxNorm[4][2];
	__declspec(align(32)) VUINT32 HalfMask[4][2];
	__declspec(align(32)) VUINT32 One[4][2];
	__declspec(align(32)) VUINT32 Threshold[4][2];
	__declspec(align(32)) VUINT32 Bias[4][2];
	__declspec(align(32)) VUINT32 Bias1[4][2];
} __svml_dlog2_data_internal;
#endif
__svml_dlog2_data_internal:
	/* Log_HA_table */
	.quad	0xc08ff00000000000, 0x0000000000000000
	.quad	0xc08ff0040038c920, 0x3d52bfc81744e999
	.quad	0xc08ff007ff0f0190, 0xbd59b2cedc63c895
	.quad	0xc08ff00bfc839e88, 0xbd28e365e6741d71
	.quad	0xc08ff00ff8979428, 0x3d4027998f69a77d
	.quad	0xc08ff013f34bd5a0, 0x3d5dd2cb33fe6a89
	.quad	0xc08ff017eca15518, 0xbd526514cdf2c019
	.quad	0xc08ff01be49903d8, 0xbd44bfeeba165e04
	.quad	0xc08ff01fdb33d218, 0xbd3fa79ee110cec3
	.quad	0xc08ff023d072af20, 0xbd4eebb642c7fd60
	.quad	0xc08ff027c4568948, 0x3d429b13d7093443
	.quad	0xc08ff02bb6e04de8, 0x3d50f346bd36551e
	.quad	0xc08ff02fa810e968, 0xbd5020bb662f1536
	.quad	0xc08ff03397e94750, 0x3d5de76b56340995
	.quad	0xc08ff037866a5218, 0x3d58065ff3304090
	.quad	0xc08ff03b7394f360, 0x3d561fc9322fb785
	.quad	0xc08ff03f5f6a13d0, 0x3d0abecd17d0d778
	.quad	0xc08ff04349ea9b28, 0xbd588f3ad0ce4d44
	.quad	0xc08ff04733177040, 0xbd4454ba4ac5f44d
	.quad	0xc08ff04b1af178f8, 0xbd556f78faaa0887
	.quad	0xc08ff04f01799a58, 0x3d49db8976de7469
	.quad	0xc08ff052e6b0b868, 0xbd5cdb6fce17ef00
	.quad	0xc08ff056ca97b668, 0xbd576de8c0412f09
	.quad	0xc08ff05aad2f76a0, 0x3d30142c7ec6475c
	.quad	0xc08ff05e8e78da70, 0xbd1e685afc26de72
	.quad	0xc08ff0626e74c260, 0xbd40b64c954078a3
	.quad	0xc08ff0664d240e10, 0xbd5fcde393462d7d
	.quad	0xc08ff06a2a879c48, 0xbd537245eeeecc53
	.quad	0xc08ff06e06a04ae8, 0x3d4ac306eb47b436
	.quad	0xc08ff071e16ef6e8, 0xbd5a1fd9d3758f6b
	.quad	0xc08ff075baf47c80, 0x3d2401fbaaa67e3c
	.quad	0xc08ff0799331b6f0, 0x3d4f8dbef47a4d53
	.quad	0xc08ff07d6a2780a8, 0x3d51215e0abb42d1
	.quad	0xc08ff0813fd6b340, 0x3d57ce6249eddb35
	.quad	0xc08ff08514402770, 0xbd38a803c7083a25
	.quad	0xc08ff088e764b528, 0x3d42218beba5073e
	.quad	0xc08ff08cb9453370, 0x3d447b66f1c6248f
	.quad	0xc08ff09089e27880, 0xbd53d9297847e995
	.quad	0xc08ff094593d59c8, 0xbd12b6979cc77aa9
	.quad	0xc08ff0982756abd0, 0xbd55308545ecd702
	.quad	0xc08ff09bf42f4260, 0xbd578fa97c3b936f
	.quad	0xc08ff09fbfc7f068, 0xbd41828408ce869d
	.quad	0xc08ff0a38a218808, 0x3d555da6ce7251a6
	.quad	0xc08ff0a7533cda88, 0xbd41f3cd14bfcb02
	.quad	0xc08ff0ab1b1ab878, 0xbd1f028da6bf1852
	.quad	0xc08ff0aee1bbf188, 0xbd4cf04de3267f54
	.quad	0xc08ff0b2a72154a8, 0xbd4556e47019db10
	.quad	0xc08ff0b66b4baff8, 0x3d1e7ba00b15fbe4
	.quad	0xc08ff0ba2e3bd0d0, 0x3d5bfde1c52c2f28
	.quad	0xc08ff0bdeff283b8, 0x3d48d63fe20ee5d6
	.quad	0xc08ff0c1b0709480, 0x3d57f551980838ff
	.quad	0xc08ff0c56fb6ce20, 0xbd4189091f293c81
	.quad	0xc08ff0c92dc5fae0, 0x3d4d549f05f06169
	.quad	0xc08ff0ccea9ee428, 0xbd5982466074e1e3
	.quad	0xc08ff0d0a64252b8, 0xbd5d30a6b16c0e4b
	.quad	0xc08ff0d460b10e80, 0xbd3138bf3b51a201
	.quad	0xc08ff0d819ebdea8, 0xbd454e680c0801d6
	.quad	0xc08ff0dbd1f389a8, 0x3d584db361385926
	.quad	0xc08ff0df88c8d520, 0xbd564f2252a82c03
	.quad	0xc08ff0e33e6c8610, 0xbd5c78c35ed5d034
	.quad	0xc08ff0e6f2df60a8, 0xbd52eb9f29ca3d75
	.quad	0xc08ff0eaa6222860, 0x3d5340c0c01b5ff8
	.quad	0xc08ff0ee58359fe8, 0x3d10c2acaffa64b6
	.quad	0xc08ff0f2091a8948, 0xbd3fced311301ebe
	.quad	0xc08ff0f5b8d1a5c8, 0x3d41ee5d591af30b
	.quad	0xc08ff0f9675bb5f0, 0x3d4873546b0e668c
	.quad	0xc08ff0fd14b97998, 0x3d5a99928177a119
	.quad	0xc08ff100c0ebafd8, 0x3d378ead132adcac
	.quad	0xc08ff1046bf31720, 0x3d51a538bc597d48
	.quad	0xc08ff10815d06d18, 0xbd540ee2f35efd7e
	.quad	0xc08ff10bbe846ec8, 0xbd59cf94753adacc
	.quad	0xc08ff10f660fd878, 0xbd5201a3d6862895
	.quad	0xc08ff1130c7365c0, 0x3d383e25d0822d03
	.quad	0xc08ff116b1afd180, 0xbd0b7389bbea8f7b
	.quad	0xc08ff11a55c5d5f0, 0xbd4df278087a6617
	.quad	0xc08ff11df8b62c98, 0xbd48daeb8ec01e26
	.quad	0xc08ff1219a818e50, 0x3d57c9312e0a14da
	.quad	0xc08ff1253b28b330, 0xbd5f0fbc0e4d507e
	.quad	0xc08ff128daac52c8, 0xbd222afdee008687
	.quad	0xc08ff12c790d23d8, 0x3d17c71747bcef8b
	.quad	0xc08ff130164bdc88, 0x3d5d69cfd051af50
	.quad	0xc08ff133b2693248, 0x3d59dff064e9433a
	.quad	0xc08ff1374d65d9e8, 0x3d4f71a30db3240b
	.quad	0xc08ff13ae7428788, 0xbd5e56afa9524606
	.quad	0xc08ff13e7fffeeb0, 0xbd44acd84e6f8518
	.quad	0xc08ff142179ec228, 0xbd519845ade5e121
	.quad	0xc08ff145ae1fb420, 0xbd5b3b4a38ddec70
	.quad	0xc08ff14943837620, 0xbd5ea4bb5bc137c7
	.quad	0xc08ff14cd7cab910, 0x3d5610f3bf8eb6ce
	.quad	0xc08ff1506af62d20, 0x3d57b1170d6184cf
	.quad	0xc08ff153fd0681f0, 0x3d5791a688a3660e
	.quad	0xc08ff1578dfc6678, 0x3d5d41ecf8abac2e
	.quad	0xc08ff15b1dd88908, 0x3cf0bd995d64d573
	.quad	0xc08ff15eac9b9758, 0xbd5e3653cd796d01
	.quad	0xc08ff1623a463e80, 0xbd597573005ef2d8
	.quad	0xc08ff165c6d92af0, 0xbd4ee222d6439c41
	.quad	0xc08ff16952550880, 0x3d5913b845e75950
	.quad	0xc08ff16cdcba8258, 0xbd558e7ba239077e
	.quad	0xc08ff170660a4328, 0x3d5a0e174a2cae66
	.quad	0xc08ff173ee44f4d8, 0x3d22b8db103db712
	.quad	0xc08ff177756b40d8, 0x3d5cc610480853c4
	.quad	0xc08ff17afb7dcfe0, 0xbd304a8bc84e5c0f
	.quad	0xc08ff17e807d4a28, 0x3d3639d185da5f7d
	.quad	0xc08ff182046a5738, 0xbd534705d06d788f
	.quad	0xc08ff18587459e10, 0xbd540d25b28a51fd
	.quad	0xc08ff189090fc510, 0xbd02d804afa7080a
	.quad	0xc08ff18c89c97200, 0x3d5f2a5d305818ba
	.quad	0xc08ff19009734a08, 0xbd3a602e9d05c3e4
	.quad	0xc08ff193880df1d0, 0xbd533d6fdcd54875
	.quad	0xc08ff197059a0d60, 0x3d24eaf0a9490202
	.quad	0xc08ff19a82184020, 0xbd5685666d98eb59
	.quad	0xc08ff19dfd892cf8, 0xbd509f8745f0868b
	.quad	0xc08ff1a177ed7630, 0xbd2dcba340a9d268
	.quad	0xc08ff1a4f145bd80, 0x3d4916fcd0331266
	.quad	0xc08ff1a86992a408, 0xbd548cd033a49073
	.quad	0xc08ff1abe0d4ca68, 0xbd5252f40e5df1a2
	.quad	0xc08ff1af570cd0a0, 0xbd541d623bd02248
	.quad	0xc08ff1b2cc3b5628, 0xbd258dc48235c071
	.quad	0xc08ff1b64060f9e0, 0xbd4b4bd8f02ed3f2
	.quad	0xc08ff1b9b37e5a28, 0x3d4e8d20a88cd0a2
	.quad	0xc08ff1bd259414c0, 0x3d3b669b6380bc55
	.quad	0xc08ff1c096a2c6e8, 0xbd45d54159d51094
	.quad	0xc08ff1c406ab0d58, 0x3d59f684ffbca44d
	.quad	0xc08ff1c775ad8428, 0x3d543b1b1d508399
	.quad	0xc08ff1cae3aac6f8, 0x3d5c30953a12fc6e
	.quad	0xc08ff1ce50a370d0, 0xbd1763b04f9aad5f
	.quad	0xc08ff1d1bc981c40, 0x3d573c6fa54f46c2
	.quad	0xc08ff1d527896338, 0x3d48ccfb9ffd7455
	.quad	0xc08ff1d89177df30, 0x3d42756f80d6f7ce
	.quad	0xc08ff1dbfa642910, 0xbd3c2bfbc353c5a5
	.quad	0xc08ff1df624ed940, 0x3d1d6064f5dc380b
	.quad	0xc08ff1e2c9388798, 0x3ce327c6b30711cf
	.quad	0xc08ff1e62f21cb70, 0x3d140aa9546525bc
	.quad	0xc08ff1e9940b3b98, 0xbd15c1ff43c21863
	.quad	0xc08ff1ecf7f56e60, 0x3d590ba680120498
	.quad	0xc08ff1f05ae0f988, 0x3d5390c6b62dff50
	.quad	0xc08ff1f3bcce7258, 0x3d4da0c90878457f
	.quad	0xc08ff1f71dbe6d90, 0x3d30697edc85b98c
	.quad	0xc08ff1fa7db17f70, 0x3d04d81188510a79
	.quad	0xc08ff1fddca83bb0, 0xbd5f2ddc983ce25c
	.quad	0xc08ff2013aa33598, 0x3d46c22f0fae6844
	.quad	0xc08ff20497a2ffd0, 0xbd53359b714c3d03
	.quad	0xc08ff207f3a82ca0, 0xbd4aefaa5524f88b
	.quad	0xc08ff20b4eb34dc0, 0x3d39bf4a4a73d01d
	.quad	0xc08ff20ea8c4f468, 0x3d44217befdb12e6
	.quad	0xc08ff21201ddb158, 0x3d5219b281d4b6f8
	.quad	0xc08ff21559fe14c8, 0xbd5e3b123373d370
	.quad	0xc08ff218b126ae88, 0xbd59b525a6edc3cb
	.quad	0xc08ff21c07580dd8, 0xbd4b494e7737c4dc
	.quad	0xc08ff21f5c92c180, 0xbd3989b7d67e3e54
	.quad	0xc08ff222b0d757d0, 0x3d486c8f098ad3cf
	.quad	0xc08ff22604265e98, 0x3d5254956d8e15b2
	.quad	0xc08ff22956806330, 0x3d3f14730a362959
	.quad	0xc08ff22ca7e5f278, 0xbd40e8ed02e32ea1
	.quad	0xc08ff22ff85798d8, 0xbd40fb2b9b1e0261
	.quad	0xc08ff23347d5e238, 0xbd5bfeb1e13c8bc3
	.quad	0xc08ff23696615a18, 0x3d5b891f041e037b
	.quad	0xc08ff239e3fa8b60, 0xbd36255027582bb9
	.quad	0xc08ff23d30a200a8, 0x3d56bb5a92a55361
	.quad	0xc08ff2407c5843f0, 0xbd31902fb4417244
	.quad	0xc08ff243c71dded8, 0xbd5a8a7c3c4a2cc6
	.quad	0xc08ff24710f35a88, 0xbd23be1be6941016
	.quad	0xc08ff24a59d93fa8, 0x3d55c85afafa1d46
	.quad	0xc08ff24da1d01668, 0xbd5b4b05a0adcbf1
	.quad	0xc08ff250e8d866a0, 0x3d134d191476f74b
	.quad	0xc08ff2542ef2b798, 0x3d5e78ce963395e1
	.quad	0xc08ff257741f9028, 0x3d3f9219a8f57c17
	.quad	0xc08ff25ab85f76c8, 0x3d5cfc6f47ac691b
	.quad	0xc08ff25dfbb2f168, 0x3d4ab3b720b5ca71
	.quad	0xc08ff2613e1a8598, 0x3d54a4ab99feb71a
	.quad	0xc08ff2647f96b868, 0xbd42daa69d79d724
	.quad	0xc08ff267c0280e88, 0xbd344d9115018f45
	.quad	0xc08ff26affcf0c28, 0xbd56673e143d2ac0
	.quad	0xc08ff26e3e8c3518, 0x3d3aac889e91c638
	.quad	0xc08ff2717c600ca8, 0x3d4cf65b41d006e7
	.quad	0xc08ff274b94b15c0, 0xbd4c821320391e76
	.quad	0xc08ff277f54dd2e8, 0x3d51abd6e2ddc2a1
	.quad	0xc08ff27b3068c620, 0xbd2f1bdd1264e703
	.quad	0xc08ff27e6a9c7110, 0xbd58437b4f032f15
	.quad	0xc08ff281a3e954f0, 0xbd4f8e063b069a7d
	.quad	0xc08ff284dc4ff288, 0x3d5276d0723a662a
	.quad	0xc08ff28813d0ca28, 0xbd5731f7c6d8f6eb
	.quad	0xc08ff28b4a6c5bd0, 0xbd58b587f08307ec
	.quad	0xc08ff28e80232708, 0x3d57f19a7a352baf
	.quad	0xc08ff291b4f5aae0, 0x3d570d99aff32790
	.quad	0xc08ff294e8e46610, 0x3d4efafaad4f59db
	.quad	0xc08ff2981befd6e0, 0xbd41eb1728371564
	.quad	0xc08ff29b4e187b38, 0x3d458465b4e080d7
	.quad	0xc08ff29e7f5ed088, 0x3d46acb4a035a820
	.quad	0xc08ff2a1afc353e0, 0xbd39fc68238dd5d3
	.quad	0xc08ff2a4df4681f0, 0x3d526d90c6750dde
	.quad	0xc08ff2a80de8d6f0, 0x3d48505c598278fd
	.quad	0xc08ff2ab3baacec0, 0x3d520fece8e148e8
	.quad	0xc08ff2ae688ce4d0, 0x3d14f7bf38646243
	.quad	0xc08ff2b1948f9430, 0xbd5aa5f693a627df
	.quad	0xc08ff2b4bfb35790, 0xbd4725d8e6280861
	.quad	0xc08ff2b7e9f8a930, 0x3d482e0765d44bda
	.quad	0xc08ff2bb136002e8, 0xbd523d745da75cde
	.quad	0xc08ff2be3be9de40, 0xbd32e50b4191ef73
	.quad	0xc08ff2c16396b448, 0xbd490856dfe073b2
	.quad	0xc08ff2c48a66fdb8, 0xbd512b526137db4d
	.quad	0xc08ff2c7b05b32e8, 0x3d5bfcdc71b36585
	.quad	0xc08ff2cad573cbb8, 0xbd2c24f2afddb377
	.quad	0xc08ff2cdf9b13fc0, 0xbd5ea60d06da12f6
	.quad	0xc08ff2d11d140630, 0xbd582f2f9e256dc5
	.quad	0xc08ff2d43f9c95d0, 0xbd4411c269523864
	.quad	0xc08ff2d7614b6508, 0xbd41107eeb7e1093
	.quad	0xc08ff2da8220e9e8, 0x3d5a4aa491710eda
	.quad	0xc08ff2dda21d9a10, 0x3d46e50a14550378
	.quad	0xc08ff2e0c141ead0, 0xbd4881e3bd846de9
	.quad	0xc08ff2e3df8e5118, 0xbd46d93437bd399d
	.quad	0xc08ff2e6fd034170, 0xbd5b4ef1e9713a4c
	.quad	0xc08ff2ea19a13010, 0x3d4a0e31ed25b3ef
	.quad	0xc08ff2ed356890b8, 0xbd5a7a560db90113
	.quad	0xc08ff2f05059d6f0, 0x3d51f5bb5f9072c9
	.quad	0xc08ff2f36a7575c0, 0x3d5ed5225350a585
	.quad	0xc08ff2f683bbdfe0, 0xbd1c9363d9e745db
	.quad	0xc08ff2f99c2d87b8, 0x3d329c788e376e0d
	.quad	0xc08ff2fcb3cadf40, 0xbd59eb5d29918de0
	.quad	0xc08ff2ffca945828, 0xbd4a86aac097a06b
	.quad	0xc08ff302e08a63b8, 0x3d541c2c97e8b4d1
	.quad	0xc08ff305f5ad72d8, 0x3d43c95dec31821b
	.quad	0xc08ff30909fdf620, 0xbd590abed3d72738
	.quad	0xc08ff30c1d7c5dd8, 0x3d4caefdad90e913
	.quad	0xc08ff30f302919d0, 0xbd4f7ed5e1dcb170
	.quad	0xc08ff312420499a0, 0x3d3c590edf8c3407
	.quad	0xc08ff315530f4c70, 0x3d5477d46ce838e1
	.quad	0xc08ff3186349a118, 0x3d5e4b00c511fa78
	.quad	0xc08ff31b72b40610, 0xbd54333e5a0c1658
	.quad	0xc08ff31e814ee990, 0x3d25300b88bfa10a
	.quad	0xc08ff3218f1ab958, 0xbd5bfbd520249ed7
	.quad	0xc08ff3249c17e2f0, 0x3d436b1cdba645b7
	.quad	0xc08ff327a846d368, 0xbd5cb667c2f86eaa
	.quad	0xc08ff32ab3a7f7a0, 0x3d5334d06a920d5f
	.quad	0xc08ff32dbe3bbbf8, 0xbd5407602ab64243
	.quad	0xc08ff330c8028ca0, 0xbd52b12c9cc82316
	.quad	0xc08ff333d0fcd560, 0x3d158d7dd801324b
	.quad	0xc08ff336d92b01a8, 0xbd38b55deae69564
	.quad	0xc08ff339e08d7ca0, 0x3d4a92d51dc43d43
	.quad	0xc08ff33ce724b110, 0x3d5455afbb5de008
	.quad	0xc08ff33fecf10970, 0x3d3b65694b6f87fb
	.quad	0xc08ff342f1f2efe8, 0xbd3afb8ccc1260eb
	.quad	0xc08ff345f62ace50, 0x3d59c98f7ec71b79
	.quad	0xc08ff348f9990e18, 0xbd5238294ff3846d
	.quad	0xc08ff34bfc3e1880, 0x3d4deba7087bbf7b
	.quad	0xc08ff34efe1a5650, 0xbd573e25d2d308e5
	.quad	0xc08ff351ff2e3020, 0xbd44bc302ffa76fb
	.quad	0xc08ff354ff7a0e20, 0xbd2cad65891df000
	.quad	0xc08ff357fefe5838, 0x3d4b4fe326c05a8a
	.quad	0xc08ff35afdbb75f8, 0x3d0fb5680f67649b
	.quad	0xc08ff35dfbb1cea8, 0xbd4af509a9977e57
	.quad	0xc08ff360f8e1c940, 0x3cea69221cfb0ad6
	.quad	0xc08ff363f54bcc60, 0x3d3d116c159fead5
	.quad	0xc08ff366f0f03e58, 0xbd5e64e8bff70d5e
	.quad	0xc08ff369ebcf8538, 0xbd5cc32ce5effb96
	.quad	0xc08ff36ce5ea06b8, 0x3d57bbe811e4fbda
	.quad	0xc08ff36fdf402830, 0xbcf46d4595033678
	.quad	0xc08ff372d7d24ec8, 0x3d4c4bbec857b9fc
	.quad	0xc08ff375cfa0df40, 0xbd59d3f339613a2d
	.quad	0xc08ff378c6ac3e28, 0x3d58408e1bcb4e24
	.quad	0xc08ff37bbcf4cfa0, 0x3d5fdb793dc8e643
	.quad	0xc08ff37eb27af788, 0xbd5f0d884b401f1e
	.quad	0xc08ff381a73f1988, 0xbd5a7ed37e2c50b4
	.quad	0xc08ff3849b4198e8, 0x3d5b14c1f630b2af
	.quad	0xc08ff3878e82d898, 0x3d505a9abef02aff
	.quad	0xc08ff38a81033b50, 0xbd4a9bbd51a7d1c4
	.quad	0xc08ff38d72c32380, 0x3d4783623464f80e
	.quad	0xc08ff39063c2f338, 0xbd0e2d78f68abcc7
	.quad	0xc08ff39354030c50, 0x3d3e604763e782cb
	.quad	0xc08ff3964383d048, 0xbd4514f0840b6f59
	.quad	0xc08ff3993245a060, 0xbd5488753d6035a4
	.quad	0xc08ff39c2048dd90, 0x3d5ccc099b5ff97d
	.quad	0xc08ff39f0d8de870, 0x3d454ada83325c69
	.quad	0xc08ff3a1fa152168, 0x3d1e4b27fb754eb1
	.quad	0xc08ff3a4e5dee890, 0x3d58c67819ead583
	.quad	0xc08ff3a7d0eb9da8, 0xbd536d02e85d644b
	.quad	0xc08ff3aabb3ba048, 0x3d5f510ab9e7c184
	.quad	0xc08ff3ada4cf4f98, 0x3d557bc5b296d5f5
	.quad	0xc08ff3b08da70a90, 0xbd48893b8f7f52c9
	.quad	0xc08ff3b375c32fe8, 0x3d5ca0b69a37d601
	.quad	0xc08ff3b65d241df0, 0xbd519c57fff86872
	.quad	0xc08ff3b943ca32d8, 0x3d048da0e3a8c3c3
	.quad	0xc08ff3bc29b5cc68, 0xbd5dd05e06ec07d0
	.quad	0xc08ff3bf0ee74840, 0x3d56c52a5c8015db
	.quad	0xc08ff3c1f35f0398, 0x3d54e1dba9930bed
	.quad	0xc08ff3c4d71d5b78, 0x3d2c5f679a7932b7
	.quad	0xc08ff3c7ba22aca0, 0xbd3f77628aa1aed8
	.quad	0xc08ff3cd7e03ac60, 0xbd5cc8a22f1d8591
	.quad	0xc08ff3d33f04e360, 0x3d4ae09463e13f6f
	.quad	0xc08ff3d8fd292dc8, 0x3d42736efbec3922
	.quad	0xc08ff3deb8736390, 0xbce0324f8d149b09
	.quad	0xc08ff3e470e65870, 0xbd52089e4b8dd900
	.quad	0xc08ff3ea2684dbf0, 0xbd5f8e9d5dea127f
	.quad	0xc08ff3efd951b970, 0xbd4b60d79db026b1
	.quad	0xc08ff3f5894fb828, 0x3d45ff1d6cea2c52
	.quad	0xc08ff3fb36819b38, 0x3d5d56022cd7f5b2
	.quad	0xc08ff400e0ea21a8, 0xbd58d63f09907b27
	.quad	0xc08ff406888c0690, 0xbd4ce6ea362f7ce0
	.quad	0xc08ff40c2d6a00f0, 0x3d519fc9ad2ef3ab
	.quad	0xc08ff411cf86c3c8, 0xbd55fc89e7b55f20
	.quad	0xc08ff4176ee4fe40, 0xbd53229ca791d9be
	.quad	0xc08ff41d0b875b88, 0x3d5e7733e6fb23d1
	.quad	0xc08ff422a57082e0, 0x3d5871413696b637
	.quad	0xc08ff4283ca317c0, 0x3d4b118aa7f493b9
	.quad	0xc08ff42dd121b9c8, 0x3d4bdf3692763b50
	.quad	0xc08ff43362ef04c8, 0x3d4867e17476dd63
	.quad	0xc08ff438f20d90c8, 0xbd5d49b741c778f3
	.quad	0xc08ff43e7e7ff228, 0x3d59ac35724f01e3
	.quad	0xc08ff4440848b968, 0xbd5251ccdc49432d
	.quad	0xc08ff4498f6a7388, 0x3d56cf153ebc9f07
	.quad	0xc08ff44f13e7a9b8, 0x3d503b7a697a659c
	.quad	0xc08ff45495c2e198, 0xbd5fa03da8acd872
	.quad	0xc08ff45a14fe9d38, 0xbd5e6cfb0b5c38fc
	.quad	0xc08ff45f919d5b08, 0x3d468b1f1269f1cf
	.quad	0xc08ff4650ba195e0, 0xbd313a3a8f72c0f3
	.quad	0xc08ff46a830dc528, 0x3d205d31eb8d2bd4
	.quad	0xc08ff46ff7e45cb8, 0xbd56cb8ddf5d4a90
	.quad	0xc08ff4756a27cd00, 0x3d272c2d46acdcbf
	.quad	0xc08ff47ad9da82e8, 0xbd4946efab7a989d
	.quad	0xc08ff48046fee800, 0xbd23fabe48cf933c
	.quad	0xc08ff485b1976268, 0x3d4f03b099d80f79
	.quad	0xc08ff48b19a654e0, 0x3d4fe0c35ab7e9b5
	.quad	0xc08ff4907f2e1ed0, 0xbd54b4843f34fe09
	.quad	0xc08ff495e2311c58, 0xbd5dfa6541236a64
	.quad	0xc08ff49b42b1a648, 0x3d56fd2c8c418cbb
	.quad	0xc08ff4a0a0b21218, 0x3d5e687ef208418a
	.quad	0xc08ff4a5fc34b210, 0x3d4a671ce14c5521
	.quad	0xc08ff4ab553bd540, 0x3d419d0202e3cd96
	.quad	0xc08ff4b0abc9c780, 0x3d576b941a895781
	.quad	0xc08ff4b5ffe0d170, 0xbd4ea96d88cd1a30
	.quad	0xc08ff4bb518338a0, 0x3d4d6b405bd43ba6
	.quad	0xc08ff4c0a0b33f60, 0xbcf03382150a56b7
	.quad	0xc08ff4c5ed7324f8, 0xbd400df96beb0937
	.quad	0xc08ff4cb37c52590, 0xbd5c161714cdebd5
	.quad	0xc08ff4d07fab7a48, 0xbd333e8eda1a8e79
	.quad	0xc08ff4d5c5285928, 0x3d53aba20381d59f
	.quad	0xc08ff4db083df530, 0xbd45e9b07af4e77c
	.quad	0xc08ff4e048ee7e70, 0xbd533cfdb78a8c41
	.quad	0xc08ff4e5873c21f0, 0xbd5d9b87f4d283f2
	.quad	0xc08ff4eac32909c8, 0xbd53a677deee97fa
	.quad	0xc08ff4effcb75d18, 0xbd5afd9f5dedc208
	.quad	0xc08ff4f533e94020, 0x3ce9dd794d20ab77
	.quad	0xc08ff4fa68c0d428, 0xbd5eeae84ba1cbf1
	.quad	0xc08ff4ff9b4037b0, 0xbd4f4451587282c8
	.quad	0xc08ff504cb698648, 0xbd4a1fa15087e717
	.quad	0xc08ff509f93ed8b0, 0xbd5f2f0042b9331a
	.quad	0xc08ff50f24c244e0, 0xbd2c2389f8e86341
	.quad	0xc08ff5144df5ddf0, 0xbd556fcb7b48f200
	.quad	0xc08ff51974dbb448, 0x3d43ba060aa69038
	.quad	0xc08ff51e9975d578, 0x3d477ef38ca20229
	.quad	0xc08ff523bbc64c60, 0x3d49bcaf1aa4168a
	.quad	0xc08ff528dbcf2120, 0xbd51c5609b60687e
	.quad	0xc08ff52df9925930, 0xbd51691708d22ce7
	.quad	0xc08ff5331511f750, 0x3d30d05c98ecb3d1
	.quad	0xc08ff5382e4ffb90, 0xbd423adb056dd244
	.quad	0xc08ff53d454e6368, 0xbd3663607042da50
	.quad	0xc08ff5425a0f29a8, 0x3d42655d3c6187a6
	.quad	0xc08ff5476c944680, 0xbd028c958ae09d20
	.quad	0xc08ff54c7cdfaf90, 0xbd436eaf17756653
	.quad	0xc08ff5518af357e8, 0x3d5fbbbee66f8d24
	.quad	0xc08ff55696d12ff0, 0xbd5d93b389497880
	.quad	0xc08ff55ba07b25b0, 0xbd43ff8ff777f337
	.quad	0xc08ff560a7f32488, 0xbcf3568803ec82a4
	.quad	0xc08ff565ad3b1560, 0xbd50c83eba5cc7ea
	.quad	0xc08ff56ab054deb0, 0x3d5becc2411500b7
	.quad	0xc08ff56fb1426458, 0xbd5dac964ffa8b83
	.quad	0xc08ff574b00587f0, 0x3d1d82f6cc82e69f
	.quad	0xc08ff579aca02878, 0xbd34767c0d40542c
	.quad	0xc08ff57ea7142298, 0xbd52d28e996ed2ce
	.quad	0xc08ff5839f635090, 0xbd432a85d337086d
	.quad	0xc08ff588958f8a38, 0x3d512b06ec20c7fd
	.quad	0xc08ff58d899aa500, 0xbd47e2147555e10b
	.quad	0xc08ff5927b867410, 0xbd4d84480a1b301d
	.quad	0xc08ff5976b54c830, 0x3d5622146f3a51bd
	.quad	0xc08ff59c59076fc8, 0x3d46d485c5f9c392
	.quad	0xc08ff5a144a03700, 0xbd4562714549f4fd
	.quad	0xc08ff5a62e20e7b8, 0x3d541ab67e365a63
	.quad	0xc08ff5ab158b4970, 0xbd5b0855668b2369
	.quad	0xc08ff5affae12188, 0x3d27de1bc2ed4dd8
	.quad	0xc08ff5b4de243300, 0x3d40f2592d5ed454
	.quad	0xc08ff5b9bf563ea8, 0xbd4ee2f8ba7b3e9e
	.quad	0xc08ff5be9e790320, 0xbd3c2214335c2164
	.quad	0xc08ff5c37b8e3cc8, 0x3d30745623ab1fd9
	.quad	0xc08ff5c85697a5d0, 0xbd326c8fb0ffde38
	.quad	0xc08ff5cd2f96f640, 0xbd4c83277493b0bc
	.quad	0xc08ff5d2068de3f8, 0x3d39bb1655e6e5ba
	.quad	0xc08ff5d6db7e22a8, 0x3d403170b47a5559
	.quad	0xc08ff5dbae6963e8, 0x3d5801ddf1edc325
	.quad	0xc08ff5e07f515728, 0x3d4b2704c46fe064
	.quad	0xc08ff5e54e37a9c8, 0x3d5a16e99ed6cd83
	.quad	0xc08ff5ea1b1e0700, 0xbd5353a3ac18c62f
	.quad	0xc08ff5eee6061810, 0x3d567c69c189f21a
	.quad	0xc08ff5f3aef18400, 0xbd50dd3220e0b0f2
	.quad	0xc08ff5f875e1eff0, 0xbd3ab64d80638db2
	.quad	0xc08ff5fd3ad8fee0, 0x3d3ec753439035aa
	.quad	0xc08ff601fdd851c8, 0xbd5e10415f5f5e74
	.quad	0xc08ff606bee187b0, 0xbd55f1048b113fae
	.quad	0xc08ff60b7df63d90, 0x3d1e94e4107406c8
	.quad	0xc08ff6103b180e60, 0xbd4e2eb5d0c36eb5
	.quad	0xc08ff614f6489330, 0x3d43ec5c714f709a
	.quad	0xc08ff619af896308, 0x3d519ec459b62a08
	.quad	0xc08ff61e66dc1300, 0xbd5b93d09dd6161d
	.quad	0xc08ff6231c423658, 0x3d5d72b849dd56be
	.quad	0xc08ff627cfbd5e38, 0xbd276b7e32659173
	.quad	0xc08ff62c814f1a08, 0x3d4fd918f2e7a6b9
	.quad	0xc08ff63130f8f730, 0x3d5609ba1dcc4c97
	.quad	0xc08ff635debc8138, 0xbd55cab233dbd84c
	.quad	0xc08ff63a8a9b41d8, 0xbd56778ab7aaabc9
	.quad	0xc08ff63f3496c0e0, 0x3d5b2791da49c370
	.quad	0xc08ff643dcb08438, 0x3d583063ef145f9c
	.quad	0xc08ff64882ea1000, 0xbd484e9cab375fb6
	.quad	0xc08ff64d2744e688, 0xbd5c430c95c374aa
	.quad	0xc08ff651c9c28848, 0xbd57a16d78490bb3
	.quad	0xc08ff6566a6473e8, 0xbd445d70374ea9ec
	.quad	0xc08ff65b092c2648, 0x3d5c9729142b9d4b
	.quad	0xc08ff65fa61b1a70, 0xbd4aaa179d032405
	.quad	0xc08ff6644132c9c0, 0xbd2a3ea300d173de
	.quad	0xc08ff668da74abc0, 0x3d57809438efb010
	.quad	0xc08ff66d71e23630, 0xbd5e9156720951d6
	.quad	0xc08ff672077cdd30, 0xbd5bab62e8462035
	.quad	0xc08ff6769b461310, 0xbd05113545431443
	.quad	0xc08ff67b2d3f4868, 0x3d5105eb0607e59b
	.quad	0xc08ff67fbd69ec18, 0xbd5e657842b37dc0
	.quad	0xc08ff6844bc76b68, 0x3d4ad1849705bc4c
	.quad	0xc08ff688d85931c8, 0xbd508b6f92b6e0d6
	.quad	0xc08ff68d6320a920, 0x3d48683cceb5fdfc
	.quad	0xc08ff691ec1f3990, 0xbd2c25ee290acbf5
	.quad	0xc08ff696735649a8, 0x3d58904932cd46d0
	.quad	0xc08ff69af8c73e38, 0xbd5c964167f0bfeb
	.quad	0xc08ff69f7c737a90, 0xbd43d66937fa06a9
	.quad	0xc08ff6a3fe5c6040, 0xbd54bc302ffa76fb
	.quad	0xc08ff6a87e834f50, 0x3d4609b1487f87a3
	.quad	0xc08ff6acfce9a618, 0xbd42c0d9af0400b1
	.quad	0xc08ff6b17990c170, 0x3d549a63973d262d
	.quad	0xc08ff6b5f479fc80, 0xbd28cde894aa0641
	.quad	0xc08ff6ba6da6b0f0, 0xbd5acef617609a34
	.quad	0xc08ff6bee51836d8, 0x3d4abb9ff3cf80b8
	.quad	0xc08ff6c35acfe4a8, 0xbd53dcfa1b7697f3
	.quad	0xc08ff6c7cecf0f68, 0x3d5bcdf4aea18a55
	.quad	0xc08ff6cc41170a70, 0x3d3cad29d4324038
	.quad	0xc08ff6d0b1a927b0, 0x3d56945f9cc2a565
	.quad	0xc08ff6d52086b780, 0x3d5d20dfc1c668a7
	.quad	0xc08ff6d98db108b8, 0x3d37f20a9bcbbe04
	.quad	0xc08ff6ddf92968b8, 0x3d1e0824a6e3a4d2
	.quad	0xc08ff6e262f12358, 0xbd469f07bf6322c7
	.quad	0xc08ff6e6cb0982f8, 0xbd5cc593afdbfaef
	.quad	0xc08ff6eb3173d080, 0xbd5ee68d555d7122
	.quad	0xc08ff6ef96315360, 0xbd144ee1d6a39124
	.quad	0xc08ff6f3f9435188, 0xbd40f2cb308bcd25
	.quad	0xc08ff6f85aab0f80, 0xbd5fd98ced08a73c
	.quad	0xc08ff6fcba69d068, 0x3d54f2f2a1ea8606
	.quad	0xc08ff7011880d5d0, 0xbd57818234572db7
	.quad	0xc08ff70574f16008, 0x3d52429e823a9a83
	.quad	0xc08ff709cfbcadd0, 0x3d5d6dc9bb81476c
	.quad	0xc08ff70e28e3fc90, 0x3d57d189e116bcb2
	.quad	0xc08ff71280688848, 0x3d0e18992809fd6d
	.quad	0xc08ff716d64b8b98, 0xbd3b48ac92b8549a
	.quad	0xc08ff71b2a8e3fb8, 0xbd4dcfa48040893b
	.quad	0xc08ff71f7d31dc88, 0x3d58d945b8e53ef1
	.quad	0xc08ff723ce379878, 0x3d4f80faef3e15ee
	.quad	0xc08ff7281da0a8b0, 0x3d53edc0fd40d18f
	.quad	0xc08ff72c6b6e40f0, 0xbd4bcac66e0be72f
	.quad	0xc08ff730b7a193b0, 0xbd44fcf96e2ec967
	.quad	0xc08ff735023bd208, 0x3d57e2ff34b08d86
	.quad	0xc08ff7394b3e2bb0, 0xbd4caedfb10b98dd
	.quad	0xc08ff73d92a9cf28, 0xbd55db1083e5ac6a
	.quad	0xc08ff741d87fe990, 0xbd580e83e6d54ed6
	.quad	0xc08ff7461cc1a6c0, 0x3d1688c83e1b0cba
	.quad	0xc08ff74a5f703138, 0xbd52c398c872b701
	.quad	0xc08ff74ea08cb240, 0xbd49aabc3683b259
	.quad	0xc08ff752e01851d0, 0x3d5ccba8de72495b
	.quad	0xc08ff7571e143688, 0xbd5981cf630f5793
	.quad	0xc08ff75b5a8185e8, 0xbd4f235844e01ebd
	.quad	0xc08ff75f95616410, 0xbd5047de7ba8ec62
	.quad	0xc08ff763ceb4f3f0, 0x3d5fa55e004d6562
	.quad	0xc08ff768067d5720, 0xbd49f386e521a80e
	.quad	0xc08ff76c3cbbae20, 0x3d3693551e62fe83
	.quad	0xc08ff77071711818, 0x3d4ba63b30b6c42c
	.quad	0xc08ff774a49eb300, 0x3d4c26523d32f573
	.quad	0xc08ff778d6459b98, 0x3d3b65e70806143a
	.quad	0xc08ff77d0666ed68, 0xbd5796d9c9f2c2cb
	.quad	0xc08ff7813503c2d0, 0x3d33267b004b912b
	.quad	0xc08ff785621d34e8, 0x3d1d5d8a23e33341
	.quad	0xc08ff7898db45ba8, 0x3d46c95233e60f40
	.quad	0xc08ff78db7ca4dd0, 0x3d362865acc8f43f
	.quad	0xc08ff791e06020f8, 0xbd10e8203e161511
	.quad	0xc08ff7960776e988, 0xbd5cafe4f4467eaa
	.quad	0xc08ff79a2d0fbac8, 0xbd520fddea9ea0cd
	.quad	0xc08ff79e512ba6d0, 0x3d5c53d3778dae46
	.quad	0xc08ff7a273cbbe80, 0xbd5f0f6f88490367
	.quad	0xc08ff7a694f111c0, 0x3d5601aa3f55ec11
	.quad	0xc08ff7aab49caf20, 0xbd4f1a8a2328a4c4
	.quad	0xc08ff7aed2cfa438, 0xbd4a3d5341c07d0e
	.quad	0xc08ff7b2ef8afd68, 0xbd5f4a1f4c525f31
	.quad	0xc08ff7b70acfc600, 0xbd4d594d77b3d775
	.quad	0xc08ff7bb249f0828, 0x3d2aef47e37e953b
	.quad	0xc08ff7bf3cf9ccf0, 0x3d501803b47dfba2
	.quad	0xc08ff7c353e11c50, 0x3d5ed5ec84e5745e
	.quad	0xc08ff7c76955fd20, 0xbd3de249bc9e7f96
	.quad	0xc08ff7cb7d597538, 0x3d5b5794341d1fdf
	.quad	0xc08ff7cf8fec8938, 0xbd519dbd08276359
	.quad	0xc08ff7d3a1103cd0, 0xbd450129b8038848
	.quad	0xc08ff7d7b0c59288, 0x3d348f00d3bb30fd
	.quad	0xc08ff7dbbf0d8bd8, 0xbd43529025720d8a
	.quad	0xc08ff7dfcbe92938, 0x3d5abdaa2b1955d7
	.quad	0xc08ff7e3d75969f8, 0xbd4e8837d4588a98
	.quad	0xc08ff7e7e15f4c80, 0x3d57a782a6df5a1f
	.quad	0xc08ff7ebe9fbce08, 0x3d304ba3eaa96bf1
	.quad	0xc08ff7eff12fead8, 0xbd47aab17b868a60
	.quad	0xc08ff7f3f6fc9e28, 0xbd5bd858693ba90a
	.quad	0xc08ff7f7fb62e230, 0x3d26abb2c547789a
	.quad	0xc08ff7fbfe63b010, 0xbd59d383d543b3f5
	.quad	0xc08ff80000000000, 0x8000000000000000
	/* Log_LA_table */
	.align	32
	.quad	0x0000000000000000
	.quad	0xbf670f83ff0a7565
	.quad	0xbf7709c46d7aac77
	.quad	0xbf8143068125dd0e
	.quad	0xbf86fe50b6ef0851
	.quad	0xbf8cb6c3abd14559
	.quad	0xbf91363117a97b0c
	.quad	0xbf940f9786685d29
	.quad	0xbf96e79685c2d22a
	.quad	0xbf99be2f7749acc2
	.quad	0xbf9c9363ba850f86
	.quad	0xbf9f6734acf8695a
	.quad	0xbfa11cd1d5133413
	.quad	0xbfa2855905ca70f6
	.quad	0xbfa3ed3094685a26
	.quad	0xbfa554592bb8cd58
	.quad	0xbfa6bad3758efd87
	.quad	0xbfa820a01ac754cb
	.quad	0xbfa985bfc3495194
	.quad	0xbfaaea3316095f72
	.quad	0xbfac4dfab90aab5f
	.quad	0xbfadb1175160f3b0
	.quad	0xbfaf1389833253a0
	.quad	0xbfb03aa8f8dc854c
	.quad	0xbfb0eb389fa29f9b
	.quad	0xbfb19b74069f5f0a
	.quad	0xbfb24b5b7e135a3d
	.quad	0xbfb2faef55ccb372
	.quad	0xbfb3aa2fdd27f1c3
	.quad	0xbfb4591d6310d85a
	.quad	0xbfb507b836033bb7
	.quad	0xbfb5b600a40bd4f3
	.quad	0xbfb663f6fac91316
	.quad	0xbfb7119b876bea86
	.quad	0xbfb7beee96b8a281
	.quad	0xbfb86bf07507a0c7
	.quad	0xbfb918a16e46335b
	.quad	0xbfb9c501cdf75872
	.quad	0xbfba7111df348494
	.quad	0xbfbb1cd1ecae66e7
	.quad	0xbfbbc84240adabba
	.quad	0xbfbc73632513bd4f
	.quad	0xbfbd1e34e35b82da
	.quad	0xbfbdc8b7c49a1ddb
	.quad	0xbfbe72ec117fa5b2
	.quad	0xbfbf1cd21257e18c
	.quad	0xbfbfc66a0f0b00a5
	.quad	0xbfc037da278f2870
	.quad	0xbfc08c588cda79e4
	.quad	0xbfc0e0b05ac848ed
	.quad	0xbfc134e1b489062e
	.quad	0xbfc188ecbd1d16be
	.quad	0xbfc1dcd197552b7b
	.quad	0xbfc2309065d29791
	.quad	0xbfc284294b07a640
	.quad	0xbfc2d79c6937efdd
	.quad	0xbfc32ae9e278ae1a
	.quad	0xbfc37e11d8b10f89
	.quad	0xbfc3d1146d9a8a64
	.quad	0xbfc423f1c2c12ea2
	.quad	0xbfc476a9f983f74d
	.quad	0xbfc4c93d33151b24
	.quad	0xbfc51bab907a5c8a
	.quad	0xbfc56df5328d58c5
	.quad	0xbfc5c01a39fbd688
	.quad	0xbfc6121ac74813cf
	.quad	0xbfc663f6fac91316
	.quad	0xbfc6b5aef4aae7dc
	.quad	0xbfc70742d4ef027f
	.quad	0xbfc758b2bb6c7b76
	.quad	0xbfc7a9fec7d05ddf
	.quad	0xbfc7fb27199df16d
	.quad	0xbfc84c2bd02f03b3
	.quad	0xbfc89d0d0ab430cd
	.quad	0xbfc8edcae8352b6c
	.quad	0xbfc93e6587910444
	.quad	0xbfc98edd077e70df
	.quad	0xbfc9df31868c11d5
	.quad	0xbfca2f632320b86b
	.quad	0xbfca7f71fb7bab9d
	.quad	0xbfcacf5e2db4ec94
	.quad	0xbfcb1f27d7bd7a80
	.quad	0xbfcb6ecf175f95e9
	.quad	0xbfcbbe540a3f036f
	.quad	0xbfcc0db6cdd94dee
	.quad	0xbfcc5cf77f860826
	.quad	0xbfccac163c770dc9
	.quad	0xbfccfb1321b8c400
	.quad	0xbfcd49ee4c325970
	.quad	0xbfcd98a7d8a605a7
	.quad	0xbfcde73fe3b1480f
	.quad	0xbfce35b689cd2655
	.quad	0xbfce840be74e6a4d
	.quad	0xbfced2401865df52
	.quad	0xbfcf205339208f27
	.quad	0xbfcf6e456567fe55
	.quad	0xbfcfbc16b902680a
	.quad	0xbfd004e3a7c97cbd
	.quad	0xbfd02baba24d0664
	.quad	0xbfd0526359bab1b3
	.quad	0xbfd0790adbb03009
	.quad	0xbfd09fa235ba2020
	.quad	0xbfd0c62975542a8f
	.quad	0xbfd0eca0a7e91e0b
	.quad	0xbfd11307dad30b76
	.quad	0xbfd1395f1b5b61a6
	.quad	0xbfd15fa676bb08ff
	.quad	0xbfd185ddfa1a7ed0
	.quad	0xbfd1ac05b291f070
	.quad	0xbfd1d21dad295632
	.quad	0xbfd1f825f6d88e13
	.quad	0xbfd21e1e9c877639
	.quad	0xbfd24407ab0e073a
	.quad	0xbfd269e12f346e2c
	.quad	0xbfd28fab35b32683
	.quad	0xbfd2b565cb3313b6
	.quad	0xbfd2db10fc4d9aaf
	.quad	0xbfd300acd58cbb10
	.quad	0xbfd32639636b2836
	.quad	0xbfd34bb6b2546218
	.quad	0xbfd37124cea4cded
	.quad	0xbfd39683c4a9ce9a
	.quad	0xbfd3bbd3a0a1dcfb
	.quad	0xbfd3e1146ebc9ff2
	.quad	0xbfd406463b1b0449
	.quad	0xbfd42b6911cf5465
	.quad	0xbfd4507cfedd4fc4
	.quad	0xbfd475820e3a4251
	.quad	0xbfd49a784bcd1b8b
	.quad	0xbfd4bf5fc36e8577
	.quad	0xbfd4e43880e8fb6a
	.quad	0xbfd509028ff8e0a2
	.quad	0xbfd52dbdfc4c96b3
	.quad	0xbfd5526ad18493ce
	.quad	0xbfd577091b3378cb
	.quad	0xbfd59b98e4de271c
	.quad	0xbfd5c01a39fbd688
	.quad	0xbfd5e48d25f62ab9
	.quad	0xbfd608f1b42948ae
	.quad	0xbfd62d47efe3ebee
	.quad	0xbfd6518fe4677ba7
	.quad	0xbfd675c99ce81f92
	.quad	0xbfd699f5248cd4b8
	.quad	0xbfd6be12866f820d
	.quad	0xbfd6e221cd9d0cde
	.quad	0xbfd7062305156d1d
	.quad	0xbfd72a1637cbc183
	.quad	0xbfd74dfb70a66388
	.quad	0xbfd771d2ba7efb3c
	.quad	0xbfd7959c202292f1
	.quad	0xbfd7b957ac51aac4
	.quad	0xbfd7dd0569c04bff
	.quad	0xbfd800a563161c54
	.quad	0xbfd82437a2ee70f7
	.quad	0xbfd847bc33d8618e
	.quad	0xbfd86b332056db01
	.quad	0xbfd88e9c72e0b226
	.quad	0xbfd8b1f835e0b642
	.quad	0xbfd8d54673b5c372
	.quad	0xbfd8f88736b2d4e8
	.quad	0xbfd91bba891f1709
	.quad	0xbfd93ee07535f967
	.quad	0xbfd961f90527409c
	.quad	0xbfd98504431717fc
	.quad	0xbfd9a802391e232f
	.quad	0xbfd9caf2f1498fa4
	.quad	0xbfd9edd6759b25e0
	.quad	0xbfda10acd0095ab4
	.quad	0xbfda33760a7f6051
	.quad	0xbfda56322edd3731
	.quad	0xbfda78e146f7bef4
	.quad	0xbfda9b835c98c70a
	.quad	0xbfdabe18797f1f49
	.quad	0xbfdae0a0a75ea862
	.quad	0xbfdb031befe06434
	.quad	0xbfdb258a5ca28608
	.quad	0xbfdb47ebf73882a1
	.quad	0xbfdb6a40c92b203f
	.quad	0xbfdb8c88dbf8867a
	.quad	0xbfdbaec439144dfd
	.quad	0xbfdbd0f2e9e79031
	.quad	0xbfdbf314f7d0f6ba
	.quad	0xbfdc152a6c24cae6
	.quad	0xbfdc3733502d04f8
	.quad	0xbfdc592fad295b56
	.quad	0xbfdc7b1f8c4f51a4
	.quad	0xbfdc9d02f6ca47b4
	.quad	0xbfdcbed9f5bb886a
	.quad	0xbfdce0a4923a587d
	.quad	0xbfdd0262d554051c
	.quad	0xbfdd2414c80bf27d
	.quad	0xbfdd45ba735baa4f
	.quad	0xbfdd6753e032ea0f
	.quad	0xbfdd88e11777b149
	.quad	0xbfddaa6222064fb9
	.quad	0xbfddcbd708b17359
	.quad	0xbfdded3fd442364c
	.quad	0xbfde0e9c8d782cbd
	.quad	0xbfde2fed3d097298
	.quad	0xbfde5131eba2b931
	.quad	0xbfde726aa1e754d2
	.quad	0xbfde939768714a32
	.quad	0xbfdeb4b847d15bce
	.quad	0xbfded5cd488f1732
	.quad	0xbfdef6d67328e220
	.quad	0xbfdf17d3d01407af
	.quad	0xbfdf38c567bcc541
	.quad	0xbfdf59ab4286576c
	.quad	0xbfdf7a8568cb06cf
	.quad	0xbfdf9b53e2dc34c4
	.quad	0xbfdfbc16b902680a
	.quad	0xbfdfdccdf37d594c
	.quad	0xbfdffd799a83ff9b
	.quad	0x3fdfe1e649bb6335
	.quad	0x3fdfc151b11b3640
	.quad	0x3fdfa0c8937e7d5d
	.quad	0x3fdf804ae8d0cd02
	.quad	0x3fdf5fd8a9063e35
	.quad	0x3fdf3f71cc1b629c
	.quad	0x3fdf1f164a15389a
	.quad	0x3fdefec61b011f85
	.quad	0x3fdede8136f4cbf1
	.quad	0x3fdebe47960e3c08
	.quad	0x3fde9e193073ac06
	.quad	0x3fde7df5fe538ab3
	.quad	0x3fde5dddf7e46e0a
	.quad	0x3fde3dd1156507de
	.quad	0x3fde1dcf4f1c1a9e
	.quad	0x3fddfdd89d586e2b
	.quad	0x3fddddecf870c4c1
	.quad	0x3fddbe0c58c3cff2
	.quad	0x3fdd9e36b6b825b1
	.quad	0x3fdd7e6c0abc3579
	.quad	0x3fdd5eac4d463d7e
	.quad	0x3fdd3ef776d43ff4
	.quad	0x3fdd1f4d7febf868
	.quad	0x3fdcffae611ad12b
	.quad	0x3fdce01a12f5d8d1
	.quad	0x3fdcc0908e19b7bd
	.quad	0x3fdca111cb2aa5c5
	.quad	0x3fdc819dc2d45fe4
	.quad	0x3fdc62346dca1dfe
	.quad	0x3fdc42d5c4c688b4
	.quad	0x3fdc2381c08baf4f
	.quad	0x3fdc043859e2fdb3
	.quad	0x3fdbe4f9899d326e
	.quad	0x3fdbc5c5489254cc
	.quad	0x3fdba69b8fa1ab02
	.quad	0x3fdb877c57b1b070
	.quad	0x3fdb686799b00be3
	.quad	0x3fdb495d4e9185f7
	.quad	0x3fdb2a5d6f51ff83
	.quad	0x3fdb0b67f4f46810
	.quad	0x3fdaec7cd882b46c
	.quad	0x3fdacd9c130dd53f
	.quad	0x3fdaaec59dadadbe
	.quad	0x3fda8ff971810a5e
	.quad	0x3fda713787ad97a5
	.quad	0x3fda527fd95fd8ff
	.quad	0x3fda33d25fcb1fac
	.quad	0x3fda152f142981b4
	.quad	0x3fd9f695efbbd0ef
	.quad	0x3fd9d806ebc9921c
	.quad	0x3fd9b98201a0f405
	.quad	0x3fd99b072a96c6b2
	.quad	0x3fd97c96600672ad
	.quad	0x3fd95e2f9b51f04e
	.quad	0x3fd93fd2d5e1bf1d
	.quad	0x3fd921800924dd3b
	.quad	0x3fd903372e90bee4
	.quad	0x3fd8e4f83fa145ee
	.quad	0x3fd8c6c335d8b966
	.quad	0x3fd8a8980abfbd32
	.quad	0x3fd88a76b7e549c6
	.quad	0x3fd86c5f36dea3dc
	.quad	0x3fd84e5181475449
	.quad	0x3fd8304d90c11fd3
	.quad	0x3fd812535ef3ff19
	.quad	0x3fd7f462e58e1688
	.quad	0x3fd7d67c1e43ae5c
	.quad	0x3fd7b89f02cf2aad
	.quad	0x3fd79acb8cf10390
	.quad	0x3fd77d01b66fbd37
	.quad	0x3fd75f417917e02c
	.quad	0x3fd7418acebbf18f
	.quad	0x3fd723ddb1346b65
	.quad	0x3fd7063a1a5fb4f2
	.quad	0x3fd6e8a004221b1f
	.quad	0x3fd6cb0f6865c8ea
	.quad	0x3fd6ad88411abfea
	.quad	0x3fd6900a8836d0d5
	.quad	0x3fd6729637b59418
	.quad	0x3fd6552b49986277
	.quad	0x3fd637c9b7e64dc2
	.quad	0x3fd61a717cac1983
	.quad	0x3fd5fd2291fc33cf
	.quad	0x3fd5dfdcf1eeae0e
	.quad	0x3fd5c2a096a135dc
	.quad	0x3fd5a56d7a370ded
	.quad	0x3fd5884396d90702
	.quad	0x3fd56b22e6b578e5
	.quad	0x3fd54e0b64003b70
	.quad	0x3fd530fd08f29fa7
	.quad	0x3fd513f7cfcb68ce
	.quad	0x3fd4f6fbb2cec598
	.quad	0x3fd4da08ac46495a
	.quad	0x3fd4bd1eb680e548
	.quad	0x3fd4a03dcbd2e1be
	.quad	0x3fd48365e695d797
	.quad	0x3fd466970128a987
	.quad	0x3fd449d115ef7d87
	.quad	0x3fd42d141f53b646
	.quad	0x3fd4106017c3eca3
	.quad	0x3fd3f3b4f9b3e939
	.quad	0x3fd3d712bf9c9def
	.quad	0x3fd3ba7963fc1f8f
	.quad	0x3fd39de8e1559f6f
	.quad	0x3fd3816132316520
	.quad	0x3fd364e2511cc821
	.quad	0x3fd3486c38aa29a8
	.quad	0x3fd32bfee370ee68
	.quad	0x3fd30f9a4c0d786d
	.quad	0x3fd2f33e6d2120f2
	.quad	0x3fd2d6eb4152324f
	.quad	0x3fd2baa0c34be1ec
	.quad	0x3fd29e5eedbe4a35
	.quad	0x3fd28225bb5e64a4
	.quad	0x3fd265f526e603cb
	.quad	0x3fd249cd2b13cd6c
	.quad	0x3fd22dadc2ab3497
	.quad	0x3fd21196e87473d1
	.quad	0x3fd1f588973c8747
	.quad	0x3fd1d982c9d52708
	.quad	0x3fd1bd857b14c146
	.quad	0x3fd1a190a5d674a0
	.quad	0x3fd185a444fa0a7b
	.quad	0x3fd169c05363f158
	.quad	0x3fd14de4cbfd373e
	.quad	0x3fd13211a9b38424
	.quad	0x3fd11646e7791469
	.quad	0x3fd0fa848044b351
	.quad	0x3fd0deca6f11b58b
	.quad	0x3fd0c318aedff3c0
	.quad	0x3fd0a76f3ab3c52c
	.quad	0x3fd08bce0d95fa38
	.quad	0x3fd070352293d724
	.quad	0x3fd054a474bf0eb7
	.quad	0x3fd0391bff2dbcf3
	.quad	0x3fd01d9bbcfa61d4
	.quad	0x3fd00223a943dc19
	.quad	0x3fcfcd677e5ac81d
	.quad	0x3fcf9697f3bd0ccf
	.quad	0x3fcf5fd8a9063e35
	.quad	0x3fcf29299496a889
	.quad	0x3fcef28aacd72231
	.quad	0x3fcebbfbe83901a6
	.quad	0x3fce857d3d361368
	.quad	0x3fce4f0ea2509008
	.quad	0x3fce18b00e13123d
	.quad	0x3fcde26177108d03
	.quad	0x3fcdac22d3e441d3
	.quad	0x3fcd75f41b31b6dd
	.quad	0x3fcd3fd543a4ad5c
	.quad	0x3fcd09c643f117f0
	.quad	0x3fccd3c712d31109
	.quad	0x3fcc9dd7a70ed160
	.quad	0x3fcc67f7f770a67e
	.quad	0x3fcc3227facce950
	.quad	0x3fcbfc67a7fff4cc
	.quad	0x3fcbc6b6f5ee1c9b
	.quad	0x3fcb9115db83a3dd
	.quad	0x3fcb5b844fb4b3ef
	.quad	0x3fcb2602497d5346
	.quad	0x3fcaf08fbfe15c51
	.quad	0x3fcabb2ca9ec7472
	.quad	0x3fca85d8feb202f7
	.quad	0x3fca5094b54d2828
	.quad	0x3fca1b5fc4e0b465
	.quad	0x3fc9e63a24971f46
	.quad	0x3fc9b123cba27ed3
	.quad	0x3fc97c1cb13c7ec1
	.quad	0x3fc94724cca657be
	.quad	0x3fc9123c1528c6ce
	.quad	0x3fc8dd62821404a9
	.quad	0x3fc8a8980abfbd32
	.quad	0x3fc873dca68b06f4
	.quad	0x3fc83f304cdc5aa7
	.quad	0x3fc80a92f5218acc
	.quad	0x3fc7d60496cfbb4c
	.quad	0x3fc7a18529635926
	.quad	0x3fc76d14a4601225
	.quad	0x3fc738b2ff50ccad
	.quad	0x3fc7046031c79f85
	.quad	0x3fc6d01c335dc9b5
	.quad	0x3fc69be6fbb3aa6f
	.quad	0x3fc667c08270b905
	.quad	0x3fc633a8bf437ce1
	.quad	0x3fc5ff9fa9e18595
	.quad	0x3fc5cba53a0762ed
	.quad	0x3fc597b967789d12
	.quad	0x3fc563dc29ffacb2
	.quad	0x3fc5300d796df33a
	.quad	0x3fc4fc4d4d9bb313
	.quad	0x3fc4c89b9e6807f5
	.quad	0x3fc494f863b8df35
	.quad	0x3fc46163957af02e
	.quad	0x3fc42ddd2ba1b4a9
	.quad	0x3fc3fa651e276158
	.quad	0x3fc3c6fb650cde51
	.quad	0x3fc3939ff859bf9f
	.quad	0x3fc36052d01c3dd7
	.quad	0x3fc32d13e4692eb7
	.quad	0x3fc2f9e32d5bfdd1
	.quad	0x3fc2c6c0a316a540
	.quad	0x3fc293ac3dc1a668
	.quad	0x3fc260a5f58c02bd
	.quad	0x3fc22dadc2ab3497
	.quad	0x3fc1fac39d5b280c
	.quad	0x3fc1c7e77dde33dc
	.quad	0x3fc195195c7d125b
	.quad	0x3fc162593186da70
	.quad	0x3fc12fa6f550f896
	.quad	0x3fc0fd02a03727ea
	.quad	0x3fc0ca6c2a9b6b41
	.quad	0x3fc097e38ce60649
	.quad	0x3fc06568bf8576b3
	.quad	0x3fc032fbbaee6d65
	.quad	0x3fc0009c779bc7b5
	.quad	0x3fbf9c95dc1d1165
	.quad	0x3fbf380e2d9ba4df
	.quad	0x3fbed3a1d4cdbebb
	.quad	0x3fbe6f50c2d9f754
	.quad	0x3fbe0b1ae8f2fd56
	.quad	0x3fbda700385788a2
	.quad	0x3fbd4300a2524d41
	.quad	0x3fbcdf1c1839ee74
	.quad	0x3fbc7b528b70f1c5
	.quad	0x3fbc17a3ed65b23c
	.quad	0x3fbbb4102f925394
	.quad	0x3fbb5097437cb58e
	.quad	0x3fbaed391ab6674e
	.quad	0x3fba89f5a6dc9acc
	.quad	0x3fba26ccd9981853
	.quad	0x3fb9c3bea49d3214
	.quad	0x3fb960caf9abb7ca
	.quad	0x3fb8fdf1ca8eea6a
	.quad	0x3fb89b33091d6fe8
	.quad	0x3fb8388ea739470a
	.quad	0x3fb7d60496cfbb4c
	.quad	0x3fb77394c9d958d5
	.quad	0x3fb7113f3259e07a
	.quad	0x3fb6af03c2603bd0
	.quad	0x3fb64ce26c067157
	.quad	0x3fb5eadb217198a3
	.quad	0x3fb588edd4d1ceaa
	.quad	0x3fb5271a78622a0f
	.quad	0x3fb4c560fe68af88
	.quad	0x3fb463c15936464e
	.quad	0x3fb4023b7b26ac9e
	.quad	0x3fb3a0cf56a06c4b
	.quad	0x3fb33f7cde14cf5a
	.quad	0x3fb2de4403ffd4b3
	.quad	0x3fb27d24bae824db
	.quad	0x3fb21c1ef55f06c2
	.quad	0x3fb1bb32a600549d
	.quad	0x3fb15a5fbf7270ce
	.quad	0x3fb0f9a634663add
	.quad	0x3fb09905f797047c
	.quad	0x3fb0387efbca869e
	.quad	0x3fafb02267a1ad2d
	.quad	0x3faeef792508b69d
	.quad	0x3fae2f02159384fe
	.quad	0x3fad6ebd1f1febfe
	.quad	0x3facaeaa27a02241
	.quad	0x3fabeec9151aac2e
	.quad	0x3fab2f19cdaa46dc
	.quad	0x3faa6f9c377dd31b
	.quad	0x3fa9b05038d84095
	.quad	0x3fa8f135b8107912
	.quad	0x3fa8324c9b914bc7
	.quad	0x3fa77394c9d958d5
	.quad	0x3fa6b50e297afcce
	.quad	0x3fa5f6b8a11c3c61
	.quad	0x3fa538941776b01e
	.quad	0x3fa47aa07357704f
	.quad	0x3fa3bcdd9b9f00f3
	.quad	0x3fa2ff4b77413dcb
	.quad	0x3fa241e9ed454683
	.quad	0x3fa184b8e4c56af8
	.quad	0x3fa0c7b844ef1795
	.quad	0x3fa00ae7f502c1c4
	.quad	0x3f9e9c8fb8a7a900
	.quad	0x3f9d23afc49139f9
	.quad	0x3f9bab2fdcb46ec7
	.quad	0x3f9a330fd028f75f
	.quad	0x3f98bb4f6e2bd536
	.quad	0x3f9743ee861f3556
	.quad	0x3f95ccece78a4a9e
	.quad	0x3f94564a62192834
	.quad	0x3f92e006c59c9c29
	.quad	0x3f916a21e20a0a45
	.quad	0x3f8fe9370ef68e1b
	.quad	0x3f8cfee70c5ce5dc
	.quad	0x3f8a15535d0bab34
	.quad	0x3f872c7ba20f7327
	.quad	0x3f84445f7cbc8fd2
	.quad	0x3f815cfe8eaec830
	.quad	0x3f7cecb0f3922091
	.quad	0x3f7720d9c06a835f
	.quad	0x3f715676c8c7a8c1
	.quad	0x3f671b0ea42e5fda
	.quad	0x3f57182a894b69c6
	.quad	0x8000000000000000
	/* poly_coeff[5] */
	.align	32
	.quad	0x3fd2776E996DA1D2, 0x3fd2776E996DA1D2, 0x3fd2776E996DA1D2, 0x3fd2776E996DA1D2 /* coeff5 */
	.quad	0xbfd715494C3E7C9B, 0xbfd715494C3E7C9B, 0xbfd715494C3E7C9B, 0xbfd715494C3E7C9B /* coeff4 */
	.quad	0x3fdEC709DC39E926, 0x3fdEC709DC39E926, 0x3fdEC709DC39E926, 0x3fdEC709DC39E926 /* coeff3 */
	.quad	0xbfe71547652B7CF8, 0xbfe71547652B7CF8, 0xbfe71547652B7CF8, 0xbfe71547652B7CF8 /* coeff2 */
	.quad	0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE, 0x3ff71547652B82FE /* coeff1 */
	/* ExpMask */
	.align	32
	.quad	0x000fffffffffffff, 0x000fffffffffffff, 0x000fffffffffffff, 0x000fffffffffffff
	/* Two10 */
	.align	32
	.quad	0x3f50000000000000, 0x3f50000000000000, 0x3f50000000000000, 0x3f50000000000000
	/* MinNorm */
	.align	32
	.quad	0x0010000000000000, 0x0010000000000000, 0x0010000000000000, 0x0010000000000000
	/* MaxNorm */
	.align	32
	.quad	0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff, 0x7fefffffffffffff
	/* HalfMask */
	.align	32
	.quad	0xfffffffffc000000, 0xfffffffffc000000, 0xfffffffffc000000, 0xfffffffffc000000
	/* One */
	.align	32
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* Threshold */
	.align	32
	.quad	0x4086a00000000000, 0x4086a00000000000, 0x4086a00000000000, 0x4086a00000000000
	/* Bias */
	.align	32
	.quad	0x408ff80000000000, 0x408ff80000000000, 0x408ff80000000000, 0x408ff80000000000
	/* Bias1 */
	.align	32
	.quad	0x408ff00000000000, 0x408ff00000000000, 0x408ff00000000000, 0x408ff00000000000
	.align	32
	.type	__svml_dlog2_data_internal, @object
	.size	__svml_dlog2_data_internal, .-__svml_dlog2_data_internal
