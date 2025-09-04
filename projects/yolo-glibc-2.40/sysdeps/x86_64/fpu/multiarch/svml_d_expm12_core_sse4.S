/* Function expm1 vectorized with SSE4.
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
 *    N = (int)(x*2^k/log(2.0)), R = x - N*log(2)/2^k
 *    exp(x) = 2^(N/2^k) * poly(R) is computed in high-low parts
 *    expm1(x) = exp(x)-1 is then obtained via multi-precision computation
 *
 *
 */

/* Offsets for data table __svml_dexpm1_data_internal
 */
#define Expm1_HA_table			0
#define poly_coeff			2048
#define Log2e				2112
#define L2H				2128
#define L2L				2144
#define ExpAddConst			2160
#define IndexMask			2176
#define ExpMask				2192
#define MOne				2208
#define AbsMask				2224
#define Threshold			2240
#define L2				2256

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_expm1_sse4)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$64, %rsp
	movaps	%xmm0, %xmm2
	movups	Log2e+__svml_dexpm1_data_internal(%rip), %xmm7
	lea	__svml_dexpm1_data_internal(%rip), %rsi
	mulpd	%xmm0, %xmm7
	movups	.FLT_10(%rip), %xmm3
	addpd	%xmm3, %xmm7
	subpd	%xmm3, %xmm7

	/* argument reduction */
	movups	L2H+__svml_dexpm1_data_internal(%rip), %xmm4
	mulpd	%xmm7, %xmm4
	movups	L2L+__svml_dexpm1_data_internal(%rip), %xmm5
	mulpd	%xmm7, %xmm5
	subpd	%xmm4, %xmm2
	subpd	%xmm5, %xmm2

	/* polynomial */
	movups	poly_coeff+__svml_dexpm1_data_internal(%rip), %xmm12
	movaps	%xmm2, %xmm14
	mulpd	%xmm2, %xmm12
	mulpd	%xmm2, %xmm14
	addpd	poly_coeff+16+__svml_dexpm1_data_internal(%rip), %xmm12
	movups	ExpAddConst+__svml_dexpm1_data_internal(%rip), %xmm15
	addpd	%xmm7, %xmm15
	mulpd	%xmm14, %xmm12
	movups	poly_coeff+32+__svml_dexpm1_data_internal(%rip), %xmm13
	mulpd	%xmm2, %xmm13

	/* table lookup */
	movdqu	IndexMask+__svml_dexpm1_data_internal(%rip), %xmm8
	pand	%xmm15, %xmm8
	movups	AbsMask+__svml_dexpm1_data_internal(%rip), %xmm1
	pshufd	$2, %xmm8, %xmm9
	movaps	%xmm1, %xmm6
	movd	%xmm8, %eax
	andps	%xmm0, %xmm6
	movd	%xmm9, %ecx
	andnps	%xmm0, %xmm1
	movdqu	ExpMask+__svml_dexpm1_data_internal(%rip), %xmm11
	pand	%xmm11, %xmm15
	cmpnlepd Threshold+__svml_dexpm1_data_internal(%rip), %xmm6
	addpd	poly_coeff+48+__svml_dexpm1_data_internal(%rip), %xmm13
	movmskpd %xmm6, %edx
	psllq	$41, %xmm15

	/* T-1 */
	movups	MOne+__svml_dexpm1_data_internal(%rip), %xmm4
	movslq	%eax, %rax
	movslq	%ecx, %rcx
	addpd	%xmm12, %xmm13
	movups	(%rsi, %rax), %xmm3
	movups	(%rsi, %rcx), %xmm10
	movaps	%xmm3, %xmm6
	unpckhpd %xmm10, %xmm3

	/* Th1 = (Th-1) + Tl */
	mulpd	%xmm15, %xmm3
	mulpd	%xmm13, %xmm14
	unpcklpd %xmm10, %xmm6
	orps	%xmm15, %xmm6
	addpd	%xmm4, %xmm6
	addpd	%xmm14, %xmm2
	addpd	%xmm3, %xmm6

	/* T = Th+Tl */
	movaps	%xmm6, %xmm5
	subpd	%xmm4, %xmm5
	mulpd	%xmm5, %xmm2
	addpd	%xmm2, %xmm6
	orps	%xmm1, %xmm6
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx xmm0 xmm6

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm6, %xmm0
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
	movups	%xmm6, 48(%rsp)
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
	movups	48(%rsp), %xmm6

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -48; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xd0, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -56; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc8, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -64; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 xmm6

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	call	expm1@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVbN2v_expm1_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dexpm1_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 Expm1_HA_table[(1<<8)][2];
	__declspec(align(16)) VUINT32 poly_coeff[4][2][2];
	__declspec(align(16)) VUINT32 Log2e[2][2];
	__declspec(align(16)) VUINT32 L2H[2][2];
	__declspec(align(16)) VUINT32 L2L[2][2];
	__declspec(align(16)) VUINT32 ExpAddConst[2][2];
	__declspec(align(16)) VUINT32 IndexMask[2][2];
	__declspec(align(16)) VUINT32 ExpMask[2][2];
	__declspec(align(16)) VUINT32 MOne[2][2];
	__declspec(align(16)) VUINT32 AbsMask[2][2];
	__declspec(align(16)) VUINT32 Threshold[2][2];
	__declspec(align(16)) VUINT32 L2[2][2];
} __svml_dexpm1_data_internal;
#endif
__svml_dexpm1_data_internal:
	/* Expm1_HA_table */
	.quad	0x0000000000000000, 0x0000000000000000
	.quad	0x0000163da8000000, 0x3e3fb33356d84a67
	.quad	0x00002c9a40000000, 0xbe3887f9f1190835
	.quad	0x00004315e8000000, 0x3e1b9fe12f5ce3e7
	.quad	0x000059b0d0000000, 0x3e48ac2ba1d73e2a
	.quad	0x0000706b28000000, 0x3e3ddf6ddc6dc404
	.quad	0x0000874518000000, 0x3e1d66f20230d7c9
	.quad	0x00009e3ec8000000, 0x3e46379c1a290f03
	.quad	0x0000b55870000000, 0xbe4833b784eb3a37
	.quad	0x0000cc9228000000, 0x3e4b923fba03db83
	.quad	0x0000e3ec30000000, 0x3e469e8d10103a17
	.quad	0x0000fb66b0000000, 0xbdb2ce50dcdf6e22
	.quad	0x00011301d0000000, 0x3df25b50a4ebbf1b
	.quad	0x00012abdc0000000, 0x3e1b0c72fee4aeb5
	.quad	0x0001429ab0000000, 0xbe356d2204cbefe7
	.quad	0x00015a98c8000000, 0x3e24b1ca24901aae
	.quad	0x000172b840000000, 0xbe4c15742919041c
	.quad	0x00018af938000000, 0x3e2191bd3777ee17
	.quad	0x0001a35be8000000, 0x3e4b7e5ba9e5b4c8
	.quad	0x0001bbe088000000, 0xbe4fdd19632a70c7
	.quad	0x0001d48730000000, 0x3e368b9aa7805b80
	.quad	0x0001ed5020000000, 0x3e47e6c8e5c40d00
	.quad	0x0002063b88000000, 0x3e18a3358ee3bac1
	.quad	0x00021f4990000000, 0x3e37ddc962552fd3
	.quad	0x0002387a70000000, 0xbe38a9dc7993e052
	.quad	0x000251ce50000000, 0xbe135670329f5521
	.quad	0x00026b4568000000, 0xbe40ec1916d42cc6
	.quad	0x000284dfe0000000, 0x3e3f5638096cf15d
	.quad	0x00029e9df8000000, 0xbe470108f69ed175
	.quad	0x0002b87fd0000000, 0x3e2b5b31ffbbd48d
	.quad	0x0002d285a8000000, 0xbe31bfcf4bff6e2b
	.quad	0x0002ecafa8000000, 0x3e33e2f5611ca0f4
	.quad	0x000306fe08000000, 0x3e418db8a96f46ad
	.quad	0x0003217100000000, 0xbe4d993e76563187
	.quad	0x00033c08b0000000, 0x3e4320b7fa64e431
	.quad	0x000356c560000000, 0xbe1b5803cdae772e
	.quad	0x000371a738000000, 0xbe28aac6ab1d7560
	.quad	0x00038cae70000000, 0xbe47d13cd3d2b1a8
	.quad	0x0003a7db38000000, 0xbe48d30048af21b7
	.quad	0x0003c32dc0000000, 0x3e489d47242000f9
	.quad	0x0003dea650000000, 0xbe4f6e5eee525f6f
	.quad	0x0003fa4508000000, 0xbe4a9bff22fa047f
	.quad	0x0004160a20000000, 0x3e3f72e29f84325c
	.quad	0x000431f5d8000000, 0x3e350a896dc70444
	.quad	0x00044e0860000000, 0x3e18624b40c4dbd0
	.quad	0x00046a41f0000000, 0xbe4717fd446d7686
	.quad	0x000486a2b8000000, 0xbe41f6197f61f2e2
	.quad	0x0004a32af0000000, 0x3e2afa7bcce5b17a
	.quad	0x0004bfdad8000000, 0xbe464eaec715e343
	.quad	0x0004dcb298000000, 0x3e3fddd0d63b36ef
	.quad	0x0004f9b278000000, 0xbe362d35952cc275
	.quad	0x000516daa0000000, 0x3e467b320e0897a9
	.quad	0x0005342b58000000, 0xbe362b07e20f57c4
	.quad	0x000551a4c8000000, 0x3e42ec9076297631
	.quad	0x00056f4738000000, 0xbe34ad8259913500
	.quad	0x00058d12d8000000, 0xbe4b41c016d6a1ea
	.quad	0x0005ab07e0000000, 0xbe45bd5eb539b67f
	.quad	0x0005c92688000000, 0x3e42ca35b80e258e
	.quad	0x0005e76f18000000, 0xbe4296f5bc8b20da
	.quad	0x000605e1b8000000, 0x3e376dc08b076f59
	.quad	0x0006247eb0000000, 0x3e0d2ac258f87d03
	.quad	0x0006434638000000, 0xbe4999e701c483c7
	.quad	0x0006623880000000, 0x3e42a91124893ecf
	.quad	0x00068155d8000000, 0xbe4d9ab467bf1d47
	.quad	0x0006a09e68000000, 0xbe380c4336f74d05
	.quad	0x0006c01278000000, 0xbe47a12a08944ab3
	.quad	0x0006dfb240000000, 0xbe4cd72e886ef8ea
	.quad	0x0006ff7df8000000, 0x3e3519483cf87e1b
	.quad	0x00071f75e8000000, 0x3e2d8bee7ba46e1e
	.quad	0x00073f9a48000000, 0x3e24b02e77ab934a
	.quad	0x00075feb58000000, 0xbe3bd98374091656
	.quad	0x0007806950000000, 0xbe00d1604f328fec
	.quad	0x0007a11470000000, 0x3e4f580c36bea881
	.quad	0x0007c1ed00000000, 0x3e330c1327c49334
	.quad	0x0007e2f338000000, 0xbe330b19defa2fd4
	.quad	0x0008042758000000, 0xbe4e0f2f724f90cc
	.quad	0x0008258998000000, 0x3e34cce128acf88b
	.quad	0x0008471a48000000, 0xbe3dc385331ad094
	.quad	0x000868d998000000, 0x3e4a2497640720ed
	.quad	0x00088ac7d8000000, 0x3e38a669966530bd
	.quad	0x0008ace540000000, 0x3e415506dadd3e2b
	.quad	0x0008cf3218000000, 0xbe34abb7410d55e3
	.quad	0x0008f1ae98000000, 0x3e31577362b98274
	.quad	0x0009145b08000000, 0x3e4c8ffe2c4530da
	.quad	0x00093737b0000000, 0x3e29b8bc9e8a0388
	.quad	0x00095a44c8000000, 0x3e4e4290774da41b
	.quad	0x00097d82a0000000, 0xbe00d8d83a30b6f8
	.quad	0x0009a0f170000000, 0x3e2940f737462137
	.quad	0x0009c49180000000, 0x3e451f8480e3e236
	.quad	0x0009e86318000000, 0x3e3e323231824ca8
	.quad	0x000a0c6678000000, 0x3e4aef2b2594d6d4
	.quad	0x000a309bf0000000, 0xbe4dae966539f470
	.quad	0x000a5503b0000000, 0x3e41f12ae45a1225
	.quad	0x000a799e10000000, 0x3e49859ac3796fd9
	.quad	0x000a9e6b58000000, 0xbe44301205e0a6de
	.quad	0x000ac36bc0000000, 0xbe0606431f9234cb
	.quad	0x000ae89f98000000, 0x3e35ad3ad5e8734d
	.quad	0x000b0e0728000000, 0x3e38db66590842ad
	.quad	0x000b33a2b8000000, 0x3e13c57ebdaff43a
	.quad	0x000b597290000000, 0xbe40d536338e3bf7
	.quad	0x000b7f76f0000000, 0x3e47daf237553d84
	.quad	0x000ba5b030000000, 0x3e2420c930819679
	.quad	0x000bcc1e90000000, 0x3e12f074891ee83d
	.quad	0x000bf2c258000000, 0x3e4eb8f0442046b8
	.quad	0x000c199be0000000, 0xbe43d56b1eeef9a7
	.quad	0x000c40ab60000000, 0xbd87c2c975903ef8
	.quad	0x000c67f130000000, 0xbe3a82eb4b5dec80
	.quad	0x000c8f6d98000000, 0xbe4fc8c257729a1e
	.quad	0x000cb720e0000000, 0xbe48837cb757e1a1
	.quad	0x000cdf0b58000000, 0xbe4511e031dd83b5
	.quad	0x000d072d48000000, 0x3e403c4bdc687918
	.quad	0x000d2f8708000000, 0x3deb13e315bc2473
	.quad	0x000d5818e0000000, 0xbe4822dbc6d12fd3
	.quad	0x000d80e318000000, 0xbe3367c68447b063
	.quad	0x000da9e600000000, 0x3e4ed9942b84600d
	.quad	0x000dd321f0000000, 0x3e480da3025b4aef
	.quad	0x000dfc9730000000, 0x3e4bdcdaf5cb4656
	.quad	0x000e264618000000, 0xbe4852f6baf6c4f0
	.quad	0x000e502ee8000000, 0xbe1d30027630bb40
	.quad	0x000e7a51f8000000, 0x3e4e3a641a5aa459
	.quad	0x000ea4afa0000000, 0x3e452486cc2c7b9d
	.quad	0x000ecf4830000000, 0xbe438cc07b927e77
	.quad	0x000efa1bf0000000, 0xbe39ea5d888e02de
	.quad	0x000f252b38000000, 0xbe2288ad162f2d20
	.quad	0x000f507658000000, 0x3e4b722a033a7c26
	.quad	0x000f7bfdb0000000, 0xbe431a0f63b7625a
	.quad	0x000fa7c180000000, 0x3e39e90d82e90a7e
	.quad	0x000fd3c228000000, 0x3e4c7b8f884badd2
	/* poly_coeff[4] */
	.align	16
	.quad	0x3f81111168877F38, 0x3f81111168877F38 /* coeff5 */
	.quad	0x3fa55555C2A9C0F3, 0x3fa55555C2A9C0F3 /* coeff4 */
	.quad	0x3fc555555555541D, 0x3fc555555555541D /* coeff3 */
	.quad	0x3fdFFFFFFFFFFE5C, 0x3fdFFFFFFFFFFE5C /* coeff2 */
	/* Log2e */
	.align	16
	.quad	0x40671547652B82FE, 0x40671547652B82FE
	/* L2H */
	.align	16
	.quad	0x3f762e42fef80000, 0x3f762e42fef80000
	/* L2L */
	.align	16
	.quad	0x3d41cf79abc9e3b4, 0x3d41cf79abc9e3b4
	/* ExpAddConst */
	.align	16
	.quad	0x42f80000001ff800, 0x42f80000001ff800
	/* IndexMask */
	.align	16
	.quad	0x00000000000007f0, 0x00000000000007f0
	/* ExpMask */
	.align	16
	.quad	0x00000000003ff800, 0x00000000003ff800
	/* MOne */
	.align	16
	.quad	0xbff0000000000000, 0xbff0000000000000
	/* AbsMask */
	.align	16
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff
	/* Threshold */
	.align	16
	.quad	0x40861DA04CBAFE43, 0x40861DA04CBAFE43
	/* L2 */
	.align	16
	.quad	0x3f762e42fefa39ef, 0x3f762e42fefa39ef
	.align	16
	.type	__svml_dexpm1_data_internal, @object
	.size	__svml_dexpm1_data_internal, .-__svml_dexpm1_data_internal
	.align	16

.FLT_10:
	.long	0x00000000, 0x43380000, 0x00000000, 0x43380000
	.type	.FLT_10, @object
	.size	.FLT_10, 16
