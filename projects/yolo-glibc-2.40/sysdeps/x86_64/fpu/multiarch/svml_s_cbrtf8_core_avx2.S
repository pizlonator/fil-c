/* Function cbrtf vectorized with AVX2.
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
 *     x=2^{3*k+j} * 1.b1 b2 ... b5 b6 ... b52
 *     Let r=(x*2^{-3k-j} - 1.b1 b2 ... b5 1)* rcp[b1 b2 ..b5],
 *     where rcp[b1 b2 .. b5]=1/(1.b1 b2 b3 b4 b5 1) in single precision
 *     cbrtf(2^j * 1. b1 b2 .. b5 1) is approximated as T[j][b1..b5]+D[j][b1..b5]
 *     (T stores the high 24 bits, D stores the low order bits)
 *     Result=2^k*T+(2^k*T*r)*P+2^k*D
 *      where P=p1+p2*r+..
 *
 */

/* Offsets for data table __svml_scbrt_data_internal
 */
#define _sRcp				0
#define _sCbrtHL			128
#define _sP2				512
#define _sP1				544
#define _sMantissaMask			576
#define _sMantissaMask1			608
#define _sExpMask			640
#define _sExpMask1			672
#define _iRcpIndexMask			704
#define _iBExpMask			736
#define _iSignMask			768
#define _iBias				800
#define _iOne				832
#define _i555				864
#define _iAbsMask			896
#define _iSubConst			928
#define _iCmpConst			960

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_cbrtf_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/* Load reciprocal value */
	lea	__svml_scbrt_data_internal(%rip), %rdx
	vmovaps	%ymm0, %ymm5

	/*
	 * Load constants
	 * Reciprocal index calculation
	 */
	vpsrld	$16, %ymm5, %ymm3
	vpand	_iRcpIndexMask+__svml_scbrt_data_internal(%rip), %ymm3, %ymm4
	vextractf128 $1, %ymm4, %xmm15
	vmovd	%xmm4, %eax
	vmovd	%xmm15, %r8d
	vpextrd	$1, %xmm15, %r9d
	vpextrd	$2, %xmm15, %r10d
	vpextrd	$3, %xmm15, %r11d
	movslq	%r8d, %r8
	movslq	%r9d, %r9
	movslq	%r10d, %r10
	movslq	%r11d, %r11
	vpextrd	$1, %xmm4, %ecx
	vpextrd	$2, %xmm4, %esi
	vpextrd	$3, %xmm4, %edi
	movslq	%eax, %rax
	movslq	%ecx, %rcx
	movslq	%esi, %rsi
	movslq	%edi, %rdi
	vmovd	(%rdx, %r8), %xmm13
	vmovd	(%rdx, %r9), %xmm14
	vmovd	(%rdx, %r10), %xmm1
	vmovd	(%rdx, %r11), %xmm0
	vpunpckldq %xmm14, %xmm13, %xmm2
	vpunpckldq %xmm0, %xmm1, %xmm13

	/* Get signed biased exponent */
	vpsrld	$7, %ymm3, %ymm0
	vmovd	(%rdx, %rax), %xmm6
	vmovd	(%rdx, %rcx), %xmm7
	vmovd	(%rdx, %rsi), %xmm8
	vmovd	(%rdx, %rdi), %xmm9
	vpunpckldq %xmm7, %xmm6, %xmm10
	vpunpckldq %xmm9, %xmm8, %xmm11
	vpunpcklqdq %xmm11, %xmm10, %xmm12
	vpunpcklqdq %xmm13, %xmm2, %xmm6
	vandps	_iAbsMask+__svml_scbrt_data_internal(%rip), %ymm5, %ymm3

	/* Argument reduction */
	vandps	_sMantissaMask+__svml_scbrt_data_internal(%rip), %ymm5, %ymm8
	vandps	_sMantissaMask1+__svml_scbrt_data_internal(%rip), %ymm5, %ymm9
	vpsubd	_iSubConst+__svml_scbrt_data_internal(%rip), %ymm3, %ymm7
	vorps	_sExpMask+__svml_scbrt_data_internal(%rip), %ymm8, %ymm10
	vorps	_sExpMask1+__svml_scbrt_data_internal(%rip), %ymm9, %ymm11

	/* r=y-y` */
	vsubps	%ymm11, %ymm10, %ymm15

	/* Biased exponent-1 */
	vpand	_iSignMask+__svml_scbrt_data_internal(%rip), %ymm0, %ymm8
	vpcmpgtd _iCmpConst+__svml_scbrt_data_internal(%rip), %ymm7, %ymm2
	vmovmskps %ymm2, %eax
	vinsertf128 $1, %xmm6, %ymm12, %ymm14

	/* Get absolute biased exponent */
	vpand	_iBExpMask+__svml_scbrt_data_internal(%rip), %ymm0, %ymm6

	/* r=(y-y`)*rcp_table(y`) */
	vmulps	%ymm15, %ymm14, %ymm1
	vpsubd	_iOne+__svml_scbrt_data_internal(%rip), %ymm6, %ymm10

	/*
	 * Calculate exponent/3
	 * i555Exp=(2^{12}-1)/3*exponent
	 */
	vpmulld	_i555+__svml_scbrt_data_internal(%rip), %ymm6, %ymm3

	/* Get K (exponent=3*k+j) */
	vpsrld	$12, %ymm3, %ymm13

	/* Get J */
	vpsubd	%ymm13, %ymm10, %ymm11

	/* Add 2/3*(bias-1)+1 to (k+1/3*(bias-1)) */
	vpaddd	_iBias+__svml_scbrt_data_internal(%rip), %ymm13, %ymm7
	vpsubd	%ymm13, %ymm11, %ymm12

	/* Attach sign to exponent */
	vpor	%ymm8, %ymm7, %ymm9
	vpsubd	%ymm13, %ymm12, %ymm14
	vpslld	$23, %ymm9, %ymm0

	/* Get 128*J */
	vpslld	$7, %ymm14, %ymm15

	/* iCbrtIndex=4*l+128*j */
	vpaddd	%ymm15, %ymm4, %ymm4

	/* Zero index if callout expected */
	vpandn	%ymm4, %ymm2, %ymm4

	/* Load Cbrt table Hi & Lo values */
	vmovd	%xmm4, %ecx
	vextractf128 $1, %ymm4, %xmm13
	vpextrd	$1, %xmm4, %esi
	movslq	%ecx, %rcx
	movslq	%esi, %rsi
	vmovd	%xmm13, %r9d
	vmovd	128(%rdx, %rcx), %xmm2
	vpextrd	$2, %xmm4, %edi
	vpextrd	$3, %xmm4, %r8d
	vmovd	128(%rdx, %rsi), %xmm3
	vpextrd	$1, %xmm13, %r10d
	vpextrd	$2, %xmm13, %ecx
	vpextrd	$3, %xmm13, %esi
	movslq	%edi, %rdi
	movslq	%r8d, %r8
	movslq	%r9d, %r9
	movslq	%r10d, %r10
	movslq	%ecx, %rcx
	movslq	%esi, %rsi
	vmovd	128(%rdx, %rdi), %xmm6
	vmovd	128(%rdx, %r8), %xmm7
	vmovd	128(%rdx, %r9), %xmm11
	vmovd	128(%rdx, %r10), %xmm12
	vmovd	128(%rdx, %rcx), %xmm14
	vmovd	128(%rdx, %rsi), %xmm15
	vpunpckldq %xmm3, %xmm2, %xmm8
	vpunpckldq %xmm7, %xmm6, %xmm9
	vpunpckldq %xmm12, %xmm11, %xmm4
	vpunpckldq %xmm15, %xmm14, %xmm11
	vpunpcklqdq %xmm9, %xmm8, %xmm10
	vpunpcklqdq %xmm11, %xmm4, %xmm2
	vinsertf128 $1, %xmm2, %ymm10, %ymm3

	/* sCbrtHi *= 2^k */
	vmulps	%ymm3, %ymm0, %ymm2

	/* Polynomial:    p1+r*(p2*r+r*(p3+r*p4)) */
	vmovups	_sP2+__svml_scbrt_data_internal(%rip), %ymm0
	vfmadd213ps _sP1+__svml_scbrt_data_internal(%rip), %ymm1, %ymm0

	/* T`*r */
	vmulps	%ymm2, %ymm1, %ymm1

	/* (T`*r)*P */
	vmulps	%ymm1, %ymm0, %ymm0

	/*
	 * T`*r*P+D`
	 * result = T`+(T`*r*P+D`)
	 */
	vaddps	%ymm0, %ymm2, %ymm0
	testl	%eax, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 eax ymm0 ymm5

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
	vmovups	%ymm5, 32(%rsp)
	vmovups	%ymm0, 64(%rsp)
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
	call	cbrtf@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN8v_cbrtf_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_scbrt_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _sRcp[32][1];
	__declspec(align(32)) VUINT32 _sCbrtHL[96][1];
	__declspec(align(32)) VUINT32 _sP2[8][1];
	__declspec(align(32)) VUINT32 _sP1[8][1];
	__declspec(align(32)) VUINT32 _sMantissaMask[8][1];
	__declspec(align(32)) VUINT32 _sMantissaMask1[8][1];
	__declspec(align(32)) VUINT32 _sExpMask[8][1];
	__declspec(align(32)) VUINT32 _sExpMask1[8][1];
	__declspec(align(32)) VUINT32 _iRcpIndexMask[8][1];
	__declspec(align(32)) VUINT32 _iBExpMask[8][1];
	__declspec(align(32)) VUINT32 _iSignMask[8][1];
	__declspec(align(32)) VUINT32 _iBias[8][1];
	__declspec(align(32)) VUINT32 _iOne[8][1];
	__declspec(align(32)) VUINT32 _i555[8][1];
	__declspec(align(32)) VUINT32 _iAbsMask[8][1];
	__declspec(align(32)) VUINT32 _iSubConst[8][1];
	__declspec(align(32)) VUINT32 _iCmpConst[8][1];
} __svml_scbrt_data_internal;
#endif
__svml_scbrt_data_internal:
	/* _sRcp */
	.long	0xBF7C0FC1 /* (1/(1+0/32+1/64)) = -.984615 */
	.long	0xBF74898D /* (1/(1+1/32+1/64)) = -.955224 */
	.long	0xBF6D7304 /* (1/(1+2/32+1/64)) = -.927536 */
	.long	0xBF66C2B4 /* (1/(1+3/32+1/64)) = -.901408 */
	.long	0xBF607038 /* (1/(1+4/32+1/64)) = -.876712 */
	.long	0xBF5A740E /* (1/(1+5/32+1/64)) = -.853333 */
	.long	0xBF54C77B /* (1/(1+6/32+1/64)) = -.831169 */
	.long	0xBF4F6475 /* (1/(1+7/32+1/64)) = -.810127 */
	.long	0xBF4A4588 /* (1/(1+8/32+1/64)) = -.790123 */
	.long	0xBF4565C8 /* (1/(1+9/32+1/64)) = -.771084 */
	.long	0xBF40C0C1 /* (1/(1+10/32+1/64)) = -.752941 */
	.long	0xBF3C5264 /* (1/(1+11/32+1/64)) = -.735632 */
	.long	0xBF381703 /* (1/(1+12/32+1/64)) = -.719101 */
	.long	0xBF340B41 /* (1/(1+13/32+1/64)) = -.703297 */
	.long	0xBF302C0B /* (1/(1+14/32+1/64)) = -.688172 */
	.long	0xBF2C7692 /* (1/(1+15/32+1/64)) = -.673684 */
	.long	0xBF28E83F /* (1/(1+16/32+1/64)) = -.659794 */
	.long	0xBF257EB5 /* (1/(1+17/32+1/64)) = -.646465 */
	.long	0xBF2237C3 /* (1/(1+18/32+1/64)) = -.633663 */
	.long	0xBF1F1166 /* (1/(1+19/32+1/64)) = -.621359 */
	.long	0xBF1C09C1 /* (1/(1+20/32+1/64)) = -.609524 */
	.long	0xBF191F1A /* (1/(1+21/32+1/64)) = -.598131 */
	.long	0xBF164FDA /* (1/(1+22/32+1/64)) = -.587156 */
	.long	0xBF139A86 /* (1/(1+23/32+1/64)) = -.576577 */
	.long	0xBF10FDBC /* (1/(1+24/32+1/64)) = -.566372 */
	.long	0xBF0E7835 /* (1/(1+25/32+1/64)) = -.556522 */
	.long	0xBF0C08C1 /* (1/(1+26/32+1/64)) = -.547009 */
	.long	0xBF09AE41 /* (1/(1+27/32+1/64)) = -.537815 */
	.long	0xBF0767AB /* (1/(1+28/32+1/64)) = -.528926 */
	.long	0xBF053408 /* (1/(1+29/32+1/64)) = -.520325 */
	.long	0xBF03126F /* (1/(1+30/32+1/64)) = -.512 */
	.long	0xBF010204 /* (1/(1+31/32+1/64)) = -.503937 */
	/* _sCbrtHL */
	.align	32
	.long	0x3F80A9C9 /* HI((2^0*(1+0/32+1/64))^(1/3)) = 1.005181 */
	.long	0x3F81F833 /* HI((2^0*(1+1/32+1/64))^(1/3)) = 1.015387 */
	.long	0x3F834007 /* HI((2^0*(1+2/32+1/64))^(1/3)) = 1.025391 */
	.long	0x3F848194 /* HI((2^0*(1+3/32+1/64))^(1/3)) = 1.035204 */
	.long	0x3F85BD25 /* HI((2^0*(1+4/32+1/64))^(1/3)) = 1.044835 */
	.long	0x3F86F300 /* HI((2^0*(1+5/32+1/64))^(1/3)) = 1.054291 */
	.long	0x3F882365 /* HI((2^0*(1+6/32+1/64))^(1/3)) = 1.06358 */
	.long	0x3F894E90 /* HI((2^0*(1+7/32+1/64))^(1/3)) = 1.07271 */
	.long	0x3F8A74B9 /* HI((2^0*(1+8/32+1/64))^(1/3)) = 1.081687 */
	.long	0x3F8B9615 /* HI((2^0*(1+9/32+1/64))^(1/3)) = 1.090518 */
	.long	0x3F8CB2D4 /* HI((2^0*(1+10/32+1/64))^(1/3)) = 1.099207 */
	.long	0x3F8DCB24 /* HI((2^0*(1+11/32+1/64))^(1/3)) = 1.107762 */
	.long	0x3F8EDF31 /* HI((2^0*(1+12/32+1/64))^(1/3)) = 1.116186 */
	.long	0x3F8FEF22 /* HI((2^0*(1+13/32+1/64))^(1/3)) = 1.124485 */
	.long	0x3F90FB1F /* HI((2^0*(1+14/32+1/64))^(1/3)) = 1.132664 */
	.long	0x3F92034C /* HI((2^0*(1+15/32+1/64))^(1/3)) = 1.140726 */
	.long	0x3F9307CA /* HI((2^0*(1+16/32+1/64))^(1/3)) = 1.148675 */
	.long	0x3F9408B9 /* HI((2^0*(1+17/32+1/64))^(1/3)) = 1.156516 */
	.long	0x3F950638 /* HI((2^0*(1+18/32+1/64))^(1/3)) = 1.164252 */
	.long	0x3F960064 /* HI((2^0*(1+19/32+1/64))^(1/3)) = 1.171887 */
	.long	0x3F96F759 /* HI((2^0*(1+20/32+1/64))^(1/3)) = 1.179423 */
	.long	0x3F97EB2F /* HI((2^0*(1+21/32+1/64))^(1/3)) = 1.186865 */
	.long	0x3F98DC01 /* HI((2^0*(1+22/32+1/64))^(1/3)) = 1.194214 */
	.long	0x3F99C9E5 /* HI((2^0*(1+23/32+1/64))^(1/3)) = 1.201474 */
	.long	0x3F9AB4F2 /* HI((2^0*(1+24/32+1/64))^(1/3)) = 1.208647 */
	.long	0x3F9B9D3D /* HI((2^0*(1+25/32+1/64))^(1/3)) = 1.215736 */
	.long	0x3F9C82DA /* HI((2^0*(1+26/32+1/64))^(1/3)) = 1.222743 */
	.long	0x3F9D65DD /* HI((2^0*(1+27/32+1/64))^(1/3)) = 1.229671 */
	.long	0x3F9E4659 /* HI((2^0*(1+28/32+1/64))^(1/3)) = 1.236522 */
	.long	0x3F9F245F /* HI((2^0*(1+29/32+1/64))^(1/3)) = 1.243297 */
	.long	0x3FA00000 /* HI((2^0*(1+30/32+1/64))^(1/3)) = 1.25 */
	.long	0x3FA0D94C /* HI((2^0*(1+31/32+1/64))^(1/3)) = 1.256631 */
	.long	0x3FA21B02 /* HI((2^1*(1+0/32+1/64))^(1/3)) = 1.266449 */
	.long	0x3FA3C059 /* HI((2^1*(1+1/32+1/64))^(1/3)) = 1.279307 */
	.long	0x3FA55D61 /* HI((2^1*(1+2/32+1/64))^(1/3)) = 1.291912 */
	.long	0x3FA6F282 /* HI((2^1*(1+3/32+1/64))^(1/3)) = 1.304276 */
	.long	0x3FA8801A /* HI((2^1*(1+4/32+1/64))^(1/3)) = 1.316409 */
	.long	0x3FAA067E /* HI((2^1*(1+5/32+1/64))^(1/3)) = 1.328323 */
	.long	0x3FAB8602 /* HI((2^1*(1+6/32+1/64))^(1/3)) = 1.340027 */
	.long	0x3FACFEEF /* HI((2^1*(1+7/32+1/64))^(1/3)) = 1.35153 */
	.long	0x3FAE718E /* HI((2^1*(1+8/32+1/64))^(1/3)) = 1.36284 */
	.long	0x3FAFDE1F /* HI((2^1*(1+9/32+1/64))^(1/3)) = 1.373966 */
	.long	0x3FB144E1 /* HI((2^1*(1+10/32+1/64))^(1/3)) = 1.384915 */
	.long	0x3FB2A60D /* HI((2^1*(1+11/32+1/64))^(1/3)) = 1.395692 */
	.long	0x3FB401DA /* HI((2^1*(1+12/32+1/64))^(1/3)) = 1.406307 */
	.long	0x3FB5587B /* HI((2^1*(1+13/32+1/64))^(1/3)) = 1.416763 */
	.long	0x3FB6AA20 /* HI((2^1*(1+14/32+1/64))^(1/3)) = 1.427067 */
	.long	0x3FB7F6F7 /* HI((2^1*(1+15/32+1/64))^(1/3)) = 1.437224 */
	.long	0x3FB93F29 /* HI((2^1*(1+16/32+1/64))^(1/3)) = 1.44724 */
	.long	0x3FBA82E1 /* HI((2^1*(1+17/32+1/64))^(1/3)) = 1.457119 */
	.long	0x3FBBC244 /* HI((2^1*(1+18/32+1/64))^(1/3)) = 1.466866 */
	.long	0x3FBCFD77 /* HI((2^1*(1+19/32+1/64))^(1/3)) = 1.476485 */
	.long	0x3FBE349B /* HI((2^1*(1+20/32+1/64))^(1/3)) = 1.48598 */
	.long	0x3FBF67D3 /* HI((2^1*(1+21/32+1/64))^(1/3)) = 1.495356 */
	.long	0x3FC0973C /* HI((2^1*(1+22/32+1/64))^(1/3)) = 1.504615 */
	.long	0x3FC1C2F6 /* HI((2^1*(1+23/32+1/64))^(1/3)) = 1.513762 */
	.long	0x3FC2EB1A /* HI((2^1*(1+24/32+1/64))^(1/3)) = 1.5228 */
	.long	0x3FC40FC6 /* HI((2^1*(1+25/32+1/64))^(1/3)) = 1.531731 */
	.long	0x3FC53112 /* HI((2^1*(1+26/32+1/64))^(1/3)) = 1.54056 */
	.long	0x3FC64F16 /* HI((2^1*(1+27/32+1/64))^(1/3)) = 1.549289 */
	.long	0x3FC769EB /* HI((2^1*(1+28/32+1/64))^(1/3)) = 1.55792 */
	.long	0x3FC881A6 /* HI((2^1*(1+29/32+1/64))^(1/3)) = 1.566457 */
	.long	0x3FC9965D /* HI((2^1*(1+30/32+1/64))^(1/3)) = 1.574901 */
	.long	0x3FCAA825 /* HI((2^1*(1+31/32+1/64))^(1/3)) = 1.583256 */
	.long	0x3FCC3D79 /* HI((2^2*(1+0/32+1/64))^(1/3)) = 1.595626 */
	.long	0x3FCE5054 /* HI((2^2*(1+1/32+1/64))^(1/3)) = 1.611826 */
	.long	0x3FD058B8 /* HI((2^2*(1+2/32+1/64))^(1/3)) = 1.627707 */
	.long	0x3FD25726 /* HI((2^2*(1+3/32+1/64))^(1/3)) = 1.643285 */
	.long	0x3FD44C15 /* HI((2^2*(1+4/32+1/64))^(1/3)) = 1.658572 */
	.long	0x3FD637F2 /* HI((2^2*(1+5/32+1/64))^(1/3)) = 1.673582 */
	.long	0x3FD81B24 /* HI((2^2*(1+6/32+1/64))^(1/3)) = 1.688328 */
	.long	0x3FD9F60B /* HI((2^2*(1+7/32+1/64))^(1/3)) = 1.702821 */
	.long	0x3FDBC8FE /* HI((2^2*(1+8/32+1/64))^(1/3)) = 1.717071 */
	.long	0x3FDD9452 /* HI((2^2*(1+9/32+1/64))^(1/3)) = 1.731089 */
	.long	0x3FDF5853 /* HI((2^2*(1+10/32+1/64))^(1/3)) = 1.744883 */
	.long	0x3FE1154B /* HI((2^2*(1+11/32+1/64))^(1/3)) = 1.758462 */
	.long	0x3FE2CB7F /* HI((2^2*(1+12/32+1/64))^(1/3)) = 1.771835 */
	.long	0x3FE47B2E /* HI((2^2*(1+13/32+1/64))^(1/3)) = 1.785009 */
	.long	0x3FE62496 /* HI((2^2*(1+14/32+1/64))^(1/3)) = 1.797992 */
	.long	0x3FE7C7F0 /* HI((2^2*(1+15/32+1/64))^(1/3)) = 1.810789 */
	.long	0x3FE96571 /* HI((2^2*(1+16/32+1/64))^(1/3)) = 1.823408 */
	.long	0x3FEAFD4C /* HI((2^2*(1+17/32+1/64))^(1/3)) = 1.835855 */
	.long	0x3FEC8FB3 /* HI((2^2*(1+18/32+1/64))^(1/3)) = 1.848135 */
	.long	0x3FEE1CD3 /* HI((2^2*(1+19/32+1/64))^(1/3)) = 1.860255 */
	.long	0x3FEFA4D7 /* HI((2^2*(1+20/32+1/64))^(1/3)) = 1.872218 */
	.long	0x3FF127E9 /* HI((2^2*(1+21/32+1/64))^(1/3)) = 1.88403 */
	.long	0x3FF2A62F /* HI((2^2*(1+22/32+1/64))^(1/3)) = 1.895697 */
	.long	0x3FF41FD0 /* HI((2^2*(1+23/32+1/64))^(1/3)) = 1.907221 */
	.long	0x3FF594EE /* HI((2^2*(1+24/32+1/64))^(1/3)) = 1.918607 */
	.long	0x3FF705AC /* HI((2^2*(1+25/32+1/64))^(1/3)) = 1.929861 */
	.long	0x3FF8722A /* HI((2^2*(1+26/32+1/64))^(1/3)) = 1.940984 */
	.long	0x3FF9DA86 /* HI((2^2*(1+27/32+1/64))^(1/3)) = 1.951981 */
	.long	0x3FFB3EDE /* HI((2^2*(1+28/32+1/64))^(1/3)) = 1.962856 */
	.long	0x3FFC9F4E /* HI((2^2*(1+29/32+1/64))^(1/3)) = 1.973612 */
	.long	0x3FFDFBF2 /* HI((2^2*(1+30/32+1/64))^(1/3)) = 1.984251 */
	.long	0x3FFF54E3 /* HI((2^2*(1+31/32+1/64))^(1/3)) = 1.994778 */
	.align	32
	.long	0xBDE3A962, 0xBDE3A962, 0xBDE3A962, 0xBDE3A962, 0xBDE3A962, 0xBDE3A962, 0xBDE3A962, 0xBDE3A962 /* _sP2 */
	.align	32
	.long	0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91 /* _sP1 */
	.align	32
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff /* _sMantissaMask (EXP_MSK3) */
	.align	32
	.long	0x007e0000, 0x007e0000, 0x007e0000, 0x007e0000, 0x007e0000, 0x007e0000, 0x007e0000, 0x007e0000 /* _sMantissaMask1 (SIG_MASK) */
	.align	32
	.long	0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000 /* _sExpMask  (EXP_MASK) */
	.align	32
	.long	0xBF820000, 0xBF820000, 0xBF820000, 0xBF820000, 0xBF820000, 0xBF820000, 0xBF820000, 0xBF820000 /* _sExpMask1 (EXP_MASK2) */
	.align	32
	.long	0x0000007c, 0x0000007c, 0x0000007c, 0x0000007c, 0x0000007c, 0x0000007c, 0x0000007c, 0x0000007c /* _iRcpIndexMask */
	.align	32
	.long	0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff /* _iBExpMask */
	.align	32
	.long	0x00000100, 0x00000100, 0x00000100, 0x00000100, 0x00000100, 0x00000100, 0x00000100, 0x00000100 /* _iSignMask */
	.align	32
	.long	0x00000055, 0x00000055, 0x00000055, 0x00000055, 0x00000055, 0x00000055, 0x00000055, 0x00000055 /* _iBias */
	.align	32
	.long	0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 /* _iOne */
	.align	32
	.long	0x00000555, 0x00000555, 0x00000555, 0x00000555, 0x00000555, 0x00000555, 0x00000555, 0x00000555 /* _i555 */
	.align	32
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	32
	.long	0x80800000, 0x80800000, 0x80800000, 0x80800000, 0x80800000, 0x80800000, 0x80800000, 0x80800000 /* _iSubConst */
	.align	32
	.long	0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF /* _iCmpConst */
	.align	32
	.type	__svml_scbrt_data_internal, @object
	.size	__svml_scbrt_data_internal, .-__svml_scbrt_data_internal
