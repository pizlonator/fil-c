/* Function cbrt vectorized with AVX2.
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
 *   x=2^{3*k+j} * 1.b1 b2 ... b5 b6 ... b52
 *   Let r=(x*2^{-3k-j} - 1.b1 b2 ... b5 1)* rcp[b1 b2 ..b5],
 *   where rcp[b1 b2 .. b5]=1/(1.b1 b2 b3 b4 b5 1) in double precision
 *   cbrt(2^j * 1. b1 b2 .. b5 1) is approximated as T[j][b1..b5]+D[j][b1..b5]
 *   (T stores the high 53 bits, D stores the low order bits)
 *   Result=2^k*T+(2^k*T*r)*P+2^k*D
 *   where P=p1+p2*r+..+p8*r^7
 *
 */

/* Offsets for data table __svml_dcbrt_data_internal
 */
#define _dRcp				0
#define _dCbrtHiLo			256
#define _dA7				1024
#define _dA6				1056
#define _dA5				1088
#define _dA4				1120
#define _dA3				1152
#define _dA2				1184
#define _dA1				1216
#define _dNeg65Div64			1248
#define _dSgnf6Mask			1280
#define _dNegOne			1312
#define _dMantissaMask			1344
#define _lExpHiMask			1376
#define _lExpLoMask			1408
#define _l1556				1440
#define _iRcpIndexMask			1472
#define _iAbsMask			1504
#define _iSignMask			1536
#define _iBias				1568
#define _iSub				1600
#define _iCmp				1632

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN4v_cbrt_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$96, %rsp

	/* Load 1/(1+iRcpIndex/32+1/64) reciprocal table value */
	lea	__svml_dcbrt_data_internal(%rip), %rax
	vmovapd	%ymm0, %ymm5

	/*
	 * Declarations
	 * Load constants
	 * Get iX - high part of argument
	 */
	vextractf128 $1, %ymm5, %xmm6

	/* Calculate CbrtIndex */
	vpsrlq	$52, %ymm5, %ymm15
	vshufps	$221, %xmm6, %xmm5, %xmm4

	/* Calculate Rcp table index */
	vandps	_iRcpIndexMask+__svml_dcbrt_data_internal(%rip), %xmm4, %xmm10
	vpsrld	$12, %xmm10, %xmm3
	vmovd	%xmm3, %ecx

	/* If the exponent field is zero - go to callout to process denormals */
	vandps	_iAbsMask+__svml_dcbrt_data_internal(%rip), %xmm4, %xmm7

	/* Compute 2^k */
	vpsrld	$20, %xmm4, %xmm4
	vpsubd	_iSub+__svml_dcbrt_data_internal(%rip), %xmm7, %xmm8
	vandps	_lExpLoMask+__svml_dcbrt_data_internal(%rip), %ymm15, %ymm0
	vpmuludq _l1556+__svml_dcbrt_data_internal(%rip), %ymm0, %ymm6
	vpextrd	$2, %xmm3, %edi
	movslq	%ecx, %rcx
	vpextrd	$1, %xmm3, %esi
	movslq	%edi, %rdi
	vpextrd	$3, %xmm3, %r8d
	movslq	%esi, %rsi
	movslq	%r8d, %r8
	vpcmpgtd _iCmp+__svml_dcbrt_data_internal(%rip), %xmm8, %xmm9
	vmovsd	(%rax, %rcx), %xmm11
	vmovmskps %xmm9, %edx
	vmovsd	(%rax, %rdi), %xmm13
	vmovhpd	(%rax, %rsi), %xmm11, %xmm12
	vmovhpd	(%rax, %r8), %xmm13, %xmm14
	vextractf128 $1, %ymm6, %xmm7
	vshufps	$136, %xmm7, %xmm6, %xmm8
	vmovups	__VUNPACK_ODD_ind1.613.0.1(%rip), %ymm7
	vextractf128 $1, %ymm0, %xmm1
	vshufps	$136, %xmm1, %xmm0, %xmm9
	vpsrld	$14, %xmm8, %xmm1
	vpsubd	%xmm1, %xmm9, %xmm10
	vpaddd	%xmm1, %xmm1, %xmm11

	/*
	 * VAND( L, l2k, = l2k, lExpHiMask );
	 * Argument reduction Z
	 */
	vandpd	_dMantissaMask+__svml_dcbrt_data_internal(%rip), %ymm5, %ymm9
	vinsertf128 $1, %xmm14, %ymm12, %ymm2
	vpsubd	%xmm11, %xmm10, %xmm12
	vpslld	$8, %xmm12, %xmm13
	vpaddd	%xmm13, %xmm3, %xmm15

	/* Load cbrt(2^j*(1+iRcpIndex/32+1/64)) Hi & Lo values */
	vmovd	%xmm15, %r9d
	vpextrd	$2, %xmm15, %r11d
	movslq	%r9d, %r9
	vpextrd	$1, %xmm15, %r10d
	movslq	%r11d, %r11
	vpextrd	$3, %xmm15, %ecx
	movslq	%r10d, %r10
	movslq	%ecx, %rcx
	vmovsd	256(%rax, %r9), %xmm3
	vmovsd	256(%rax, %r11), %xmm0
	vandpd	_dSgnf6Mask+__svml_dcbrt_data_internal(%rip), %ymm5, %ymm10
	vmovhpd	256(%rax, %r10), %xmm3, %xmm14
	vmovhpd	256(%rax, %rcx), %xmm0, %xmm3
	vorpd	_dNegOne+__svml_dcbrt_data_internal(%rip), %ymm9, %ymm11
	vorpd	_dNeg65Div64+__svml_dcbrt_data_internal(%rip), %ymm10, %ymm12
	vsubpd	%ymm12, %ymm11, %ymm13
	vmulpd	%ymm13, %ymm2, %ymm2
	vinsertf128 $1, %xmm3, %ymm14, %ymm0
	vpand	_iSignMask+__svml_dcbrt_data_internal(%rip), %xmm4, %xmm3
	vpor	_iBias+__svml_dcbrt_data_internal(%rip), %xmm3, %xmm4
	vpaddd	%xmm1, %xmm4, %xmm1
	vpslld	$20, %xmm1, %xmm6

	/* Polynomial */
	vmovupd	_dA7+__svml_dcbrt_data_internal(%rip), %ymm1
	vfmadd213pd _dA6+__svml_dcbrt_data_internal(%rip), %ymm2, %ymm1
	vfmadd213pd _dA5+__svml_dcbrt_data_internal(%rip), %ymm2, %ymm1
	vfmadd213pd _dA4+__svml_dcbrt_data_internal(%rip), %ymm2, %ymm1
	vfmadd213pd _dA3+__svml_dcbrt_data_internal(%rip), %ymm2, %ymm1
	vfmadd213pd _dA2+__svml_dcbrt_data_internal(%rip), %ymm2, %ymm1
	vfmadd213pd _dA1+__svml_dcbrt_data_internal(%rip), %ymm2, %ymm1
	vpermps	%ymm6, %ymm7, %ymm8
	vandps	__VUNPACK_ODD_mask.613.0.1(%rip), %ymm8, %ymm14

	/* THi*2^k, TLo*2^k */
	vmulpd	%ymm14, %ymm0, %ymm0

	/* THi*2^k*Z */
	vmulpd	%ymm0, %ymm2, %ymm2

	/* Final reconstruction */
	vmulpd	%ymm2, %ymm1, %ymm3
	vaddpd	%ymm3, %ymm0, %ymm0
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm5

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
	vmovupd	%ymm5, 32(%rsp)
	vmovupd	%ymm0, 64(%rsp)
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
	call	cbrt@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovsd	%xmm0, 64(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx r15 r12d r13d
END(_ZGVdN4v_cbrt_avx2)
	.section .rodata, "a"
	.align	32

__VUNPACK_ODD_ind1.613.0.1:
	.rept	3
	.long	0
	.endr
	.long	1
	.long	0
	.long	2
	.long	0
	.long	3
	.align	32

__VUNPACK_ODD_mask.613.0.1:
	.long	0
	.long	-1
	.long	0
	.long	-1
	.long	0
	.long	-1
	.long	0
	.long	-1

	.section .rodata, "a"
	.align	32

#ifdef __svml_dcbrt_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 _dRcp[32][2];
	__declspec(align(32)) VUINT32 _dCbrtHiLo[96][2];
	__declspec(align(32)) VUINT32 _dA7[4][2];
	__declspec(align(32)) VUINT32 _dA6[4][2];
	__declspec(align(32)) VUINT32 _dA5[4][2];
	__declspec(align(32)) VUINT32 _dA4[4][2];
	__declspec(align(32)) VUINT32 _dA3[4][2];
	__declspec(align(32)) VUINT32 _dA2[4][2];
	__declspec(align(32)) VUINT32 _dA1[4][2];
	__declspec(align(32)) VUINT32 _dNeg65Div64[4][2];
	__declspec(align(32)) VUINT32 _dSgnf6Mask[4][2];
	__declspec(align(32)) VUINT32 _dNegOne[4][2];
	__declspec(align(32)) VUINT32 _dMantissaMask[4][2];
	__declspec(align(32)) VUINT32 _lExpHiMask[4][2];
	__declspec(align(32)) VUINT32 _lExpLoMask[4][2];
	__declspec(align(32)) VUINT32 _l1556[4][2];
	__declspec(align(32)) VUINT32 _iRcpIndexMask[8][1];
	__declspec(align(32)) VUINT32 _iAbsMask[8][1];
	__declspec(align(32)) VUINT32 _iSignMask[8][1];
	__declspec(align(32)) VUINT32 _iBias[8][1];
	__declspec(align(32)) VUINT32 _iSub[8][1];
	__declspec(align(32)) VUINT32 _iCmp[8][1];
} __svml_dcbrt_data_internal;
#endif
__svml_dcbrt_data_internal:
	/* _dRcp */
	.quad	0xBFEF81F81F81F820 /* (1/(1+0/32+1/64)) = -.984615 */
	.quad	0xBFEE9131ABF0B767 /* (1/(1+1/32+1/64)) = -.955224 */
	.quad	0xBFEDAE6076B981DB /* (1/(1+2/32+1/64)) = -.927536 */
	.quad	0xBFECD85689039B0B /* (1/(1+3/32+1/64)) = -.901408 */
	.quad	0xBFEC0E070381C0E0 /* (1/(1+4/32+1/64)) = -.876712 */
	.quad	0xBFEB4E81B4E81B4F /* (1/(1+5/32+1/64)) = -.853333 */
	.quad	0xBFEA98EF606A63BE /* (1/(1+6/32+1/64)) = -.831169 */
	.quad	0xBFE9EC8E951033D9 /* (1/(1+7/32+1/64)) = -.810127 */
	.quad	0xBFE948B0FCD6E9E0 /* (1/(1+8/32+1/64)) = -.790123 */
	.quad	0xBFE8ACB90F6BF3AA /* (1/(1+9/32+1/64)) = -.771084 */
	.quad	0xBFE8181818181818 /* (1/(1+10/32+1/64)) = -.752941 */
	.quad	0xBFE78A4C8178A4C8 /* (1/(1+11/32+1/64)) = -.735632 */
	.quad	0xBFE702E05C0B8170 /* (1/(1+12/32+1/64)) = -.719101 */
	.quad	0xBFE6816816816817 /* (1/(1+13/32+1/64)) = -.703297 */
	.quad	0xBFE6058160581606 /* (1/(1+14/32+1/64)) = -.688172 */
	.quad	0xBFE58ED2308158ED /* (1/(1+15/32+1/64)) = -.673684 */
	.quad	0xBFE51D07EAE2F815 /* (1/(1+16/32+1/64)) = -.659794 */
	.quad	0xBFE4AFD6A052BF5B /* (1/(1+17/32+1/64)) = -.646465 */
	.quad	0xBFE446F86562D9FB /* (1/(1+18/32+1/64)) = -.633663 */
	.quad	0xBFE3E22CBCE4A902 /* (1/(1+19/32+1/64)) = -.621359 */
	.quad	0xBFE3813813813814 /* (1/(1+20/32+1/64)) = -.609524 */
	.quad	0xBFE323E34A2B10BF /* (1/(1+21/32+1/64)) = -.598131 */
	.quad	0xBFE2C9FB4D812CA0 /* (1/(1+22/32+1/64)) = -.587156 */
	.quad	0xBFE27350B8812735 /* (1/(1+23/32+1/64)) = -.576577 */
	.quad	0xBFE21FB78121FB78 /* (1/(1+24/32+1/64)) = -.566372 */
	.quad	0xBFE1CF06ADA2811D /* (1/(1+25/32+1/64)) = -.556522 */
	.quad	0xBFE1811811811812 /* (1/(1+26/32+1/64)) = -.547009 */
	.quad	0xBFE135C81135C811 /* (1/(1+27/32+1/64)) = -.537815 */
	.quad	0xBFE0ECF56BE69C90 /* (1/(1+28/32+1/64)) = -.528926 */
	.quad	0xBFE0A6810A6810A7 /* (1/(1+29/32+1/64)) = -.520325 */
	.quad	0xBFE0624DD2F1A9FC /* (1/(1+30/32+1/64)) = -.512 */
	.quad	0xBFE0204081020408 /* (1/(1+31/32+1/64)) = -.503937 */
	/* _dCbrtHiLo */
	.align	32
	.quad	0x3FF01539221D4C97 /* HI((2^0*(1+0/32+1/64))^(1/3)) = 1.005181 */
	.quad	0x3FF03F06771A2E33 /* HI((2^0*(1+1/32+1/64))^(1/3)) = 1.015387 */
	.quad	0x3FF06800E629D671 /* HI((2^0*(1+2/32+1/64))^(1/3)) = 1.025391 */
	.quad	0x3FF090328731DEB2 /* HI((2^0*(1+3/32+1/64))^(1/3)) = 1.035204 */
	.quad	0x3FF0B7A4B1BD64AC /* HI((2^0*(1+4/32+1/64))^(1/3)) = 1.044835 */
	.quad	0x3FF0DE601024FB87 /* HI((2^0*(1+5/32+1/64))^(1/3)) = 1.054291 */
	.quad	0x3FF1046CB0597000 /* HI((2^0*(1+6/32+1/64))^(1/3)) = 1.06358 */
	.quad	0x3FF129D212A9BA9B /* HI((2^0*(1+7/32+1/64))^(1/3)) = 1.07271 */
	.quad	0x3FF14E9736CDAF38 /* HI((2^0*(1+8/32+1/64))^(1/3)) = 1.081687 */
	.quad	0x3FF172C2A772F507 /* HI((2^0*(1+9/32+1/64))^(1/3)) = 1.090518 */
	.quad	0x3FF1965A848001D3 /* HI((2^0*(1+10/32+1/64))^(1/3)) = 1.099207 */
	.quad	0x3FF1B9648C38C55D /* HI((2^0*(1+11/32+1/64))^(1/3)) = 1.107762 */
	.quad	0x3FF1DBE6236A0C45 /* HI((2^0*(1+12/32+1/64))^(1/3)) = 1.116186 */
	.quad	0x3FF1FDE45CBB1F9F /* HI((2^0*(1+13/32+1/64))^(1/3)) = 1.124485 */
	.quad	0x3FF21F63FF409042 /* HI((2^0*(1+14/32+1/64))^(1/3)) = 1.132664 */
	.quad	0x3FF240698C6746E5 /* HI((2^0*(1+15/32+1/64))^(1/3)) = 1.140726 */
	.quad	0x3FF260F9454BB99B /* HI((2^0*(1+16/32+1/64))^(1/3)) = 1.148675 */
	.quad	0x3FF281172F8E7073 /* HI((2^0*(1+17/32+1/64))^(1/3)) = 1.156516 */
	.quad	0x3FF2A0C719B4B6D0 /* HI((2^0*(1+18/32+1/64))^(1/3)) = 1.164252 */
	.quad	0x3FF2C00C9F2263EC /* HI((2^0*(1+19/32+1/64))^(1/3)) = 1.171887 */
	.quad	0x3FF2DEEB2BB7FB78 /* HI((2^0*(1+20/32+1/64))^(1/3)) = 1.179423 */
	.quad	0x3FF2FD65FF1EFBBC /* HI((2^0*(1+21/32+1/64))^(1/3)) = 1.186865 */
	.quad	0x3FF31B802FCCF6A2 /* HI((2^0*(1+22/32+1/64))^(1/3)) = 1.194214 */
	.quad	0x3FF3393CADC50708 /* HI((2^0*(1+23/32+1/64))^(1/3)) = 1.201474 */
	.quad	0x3FF3569E451E4C2A /* HI((2^0*(1+24/32+1/64))^(1/3)) = 1.208647 */
	.quad	0x3FF373A7A0554CDE /* HI((2^0*(1+25/32+1/64))^(1/3)) = 1.215736 */
	.quad	0x3FF3905B4A6D76CE /* HI((2^0*(1+26/32+1/64))^(1/3)) = 1.222743 */
	.quad	0x3FF3ACBBB0E756B6 /* HI((2^0*(1+27/32+1/64))^(1/3)) = 1.229671 */
	.quad	0x3FF3C8CB258FA340 /* HI((2^0*(1+28/32+1/64))^(1/3)) = 1.236522 */
	.quad	0x3FF3E48BE02AC0CE /* HI((2^0*(1+29/32+1/64))^(1/3)) = 1.243297 */
	.quad	0x3FF4000000000000 /* HI((2^0*(1+30/32+1/64))^(1/3)) = 1.25 */
	.quad	0x3FF41B298D47800E /* HI((2^0*(1+31/32+1/64))^(1/3)) = 1.256631 */
	.quad	0x3FF443604B34D9B2 /* HI((2^1*(1+0/32+1/64))^(1/3)) = 1.266449 */
	.quad	0x3FF4780B20906571 /* HI((2^1*(1+1/32+1/64))^(1/3)) = 1.279307 */
	.quad	0x3FF4ABAC3EE06706 /* HI((2^1*(1+2/32+1/64))^(1/3)) = 1.291912 */
	.quad	0x3FF4DE505DA66B8D /* HI((2^1*(1+3/32+1/64))^(1/3)) = 1.304276 */
	.quad	0x3FF51003420A5C07 /* HI((2^1*(1+4/32+1/64))^(1/3)) = 1.316409 */
	.quad	0x3FF540CFD6FD11C1 /* HI((2^1*(1+5/32+1/64))^(1/3)) = 1.328323 */
	.quad	0x3FF570C04260716B /* HI((2^1*(1+6/32+1/64))^(1/3)) = 1.340027 */
	.quad	0x3FF59FDDF7A45F38 /* HI((2^1*(1+7/32+1/64))^(1/3)) = 1.35153 */
	.quad	0x3FF5CE31C83539DF /* HI((2^1*(1+8/32+1/64))^(1/3)) = 1.36284 */
	.quad	0x3FF5FBC3F20966A4 /* HI((2^1*(1+9/32+1/64))^(1/3)) = 1.373966 */
	.quad	0x3FF6289C2C8F1B70 /* HI((2^1*(1+10/32+1/64))^(1/3)) = 1.384915 */
	.quad	0x3FF654C1B4316DCF /* HI((2^1*(1+11/32+1/64))^(1/3)) = 1.395693 */
	.quad	0x3FF6803B54A34E44 /* HI((2^1*(1+12/32+1/64))^(1/3)) = 1.406307 */
	.quad	0x3FF6AB0F72182659 /* HI((2^1*(1+13/32+1/64))^(1/3)) = 1.416763 */
	.quad	0x3FF6D544118C08BC /* HI((2^1*(1+14/32+1/64))^(1/3)) = 1.427067 */
	.quad	0x3FF6FEDEE0388D4A /* HI((2^1*(1+15/32+1/64))^(1/3)) = 1.437224 */
	.quad	0x3FF727E53A4F645E /* HI((2^1*(1+16/32+1/64))^(1/3)) = 1.44724 */
	.quad	0x3FF7505C31104114 /* HI((2^1*(1+17/32+1/64))^(1/3)) = 1.457119 */
	.quad	0x3FF77848904CD549 /* HI((2^1*(1+18/32+1/64))^(1/3)) = 1.466866 */
	.quad	0x3FF79FAEE36B2534 /* HI((2^1*(1+19/32+1/64))^(1/3)) = 1.476485 */
	.quad	0x3FF7C69379F4605B /* HI((2^1*(1+20/32+1/64))^(1/3)) = 1.48598 */
	.quad	0x3FF7ECFA6BBCA391 /* HI((2^1*(1+21/32+1/64))^(1/3)) = 1.495356 */
	.quad	0x3FF812E79CAE7EB9 /* HI((2^1*(1+22/32+1/64))^(1/3)) = 1.504615 */
	.quad	0x3FF8385EC043C71D /* HI((2^1*(1+23/32+1/64))^(1/3)) = 1.513762 */
	.quad	0x3FF85D635CB41B9D /* HI((2^1*(1+24/32+1/64))^(1/3)) = 1.5228 */
	.quad	0x3FF881F8CDE083DB /* HI((2^1*(1+25/32+1/64))^(1/3)) = 1.531731 */
	.quad	0x3FF8A6224802B8A8 /* HI((2^1*(1+26/32+1/64))^(1/3)) = 1.54056 */
	.quad	0x3FF8C9E2DA25E5E4 /* HI((2^1*(1+27/32+1/64))^(1/3)) = 1.549289 */
	.quad	0x3FF8ED3D706E1010 /* HI((2^1*(1+28/32+1/64))^(1/3)) = 1.55792 */
	.quad	0x3FF91034D632B6DF /* HI((2^1*(1+29/32+1/64))^(1/3)) = 1.566457 */
	.quad	0x3FF932CBB7F0CF2D /* HI((2^1*(1+30/32+1/64))^(1/3)) = 1.574901 */
	.quad	0x3FF95504A517BF3A /* HI((2^1*(1+31/32+1/64))^(1/3)) = 1.583256 */
	.quad	0x3FF987AF34F8BB19 /* HI((2^2*(1+0/32+1/64))^(1/3)) = 1.595626 */
	.quad	0x3FF9CA0A8337B317 /* HI((2^2*(1+1/32+1/64))^(1/3)) = 1.611826 */
	.quad	0x3FFA0B1709CC13D5 /* HI((2^2*(1+2/32+1/64))^(1/3)) = 1.627708 */
	.quad	0x3FFA4AE4CE6419ED /* HI((2^2*(1+3/32+1/64))^(1/3)) = 1.643285 */
	.quad	0x3FFA8982A5567031 /* HI((2^2*(1+4/32+1/64))^(1/3)) = 1.658572 */
	.quad	0x3FFAC6FE500AB570 /* HI((2^2*(1+5/32+1/64))^(1/3)) = 1.673582 */
	.quad	0x3FFB036497A15A17 /* HI((2^2*(1+6/32+1/64))^(1/3)) = 1.688328 */
	.quad	0x3FFB3EC164671755 /* HI((2^2*(1+7/32+1/64))^(1/3)) = 1.702821 */
	.quad	0x3FFB791FD288C46F /* HI((2^2*(1+8/32+1/64))^(1/3)) = 1.717071 */
	.quad	0x3FFBB28A44693BE4 /* HI((2^2*(1+9/32+1/64))^(1/3)) = 1.731089 */
	.quad	0x3FFBEB0A72EB6E31 /* HI((2^2*(1+10/32+1/64))^(1/3)) = 1.744883 */
	.quad	0x3FFC22A97BF5F697 /* HI((2^2*(1+11/32+1/64))^(1/3)) = 1.758462 */
	.quad	0x3FFC596FEF6AF983 /* HI((2^2*(1+12/32+1/64))^(1/3)) = 1.771835 */
	.quad	0x3FFC8F65DAC655A3 /* HI((2^2*(1+13/32+1/64))^(1/3)) = 1.785009 */
	.quad	0x3FFCC492D38CE8D9 /* HI((2^2*(1+14/32+1/64))^(1/3)) = 1.797992 */
	.quad	0x3FFCF8FE00B19367 /* HI((2^2*(1+15/32+1/64))^(1/3)) = 1.810789 */
	.quad	0x3FFD2CAE230F8709 /* HI((2^2*(1+16/32+1/64))^(1/3)) = 1.823408 */
	.quad	0x3FFD5FA99D15208F /* HI((2^2*(1+17/32+1/64))^(1/3)) = 1.835855 */
	.quad	0x3FFD91F679B6E505 /* HI((2^2*(1+18/32+1/64))^(1/3)) = 1.848135 */
	.quad	0x3FFDC39A72BF2302 /* HI((2^2*(1+19/32+1/64))^(1/3)) = 1.860255 */
	.quad	0x3FFDF49AF68C1570 /* HI((2^2*(1+20/32+1/64))^(1/3)) = 1.872218 */
	.quad	0x3FFE24FD2D4C23B8 /* HI((2^2*(1+21/32+1/64))^(1/3)) = 1.884031 */
	.quad	0x3FFE54C5FDC5EC73 /* HI((2^2*(1+22/32+1/64))^(1/3)) = 1.895697 */
	.quad	0x3FFE83FA11B81DBB /* HI((2^2*(1+23/32+1/64))^(1/3)) = 1.907221 */
	.quad	0x3FFEB29DD9DBAF25 /* HI((2^2*(1+24/32+1/64))^(1/3)) = 1.918608 */
	.quad	0x3FFEE0B59191D374 /* HI((2^2*(1+25/32+1/64))^(1/3)) = 1.929861 */
	.quad	0x3FFF0E454245E4BF /* HI((2^2*(1+26/32+1/64))^(1/3)) = 1.940984 */
	.quad	0x3FFF3B50C68A9DD3 /* HI((2^2*(1+27/32+1/64))^(1/3)) = 1.951981 */
	.quad	0x3FFF67DBCCF922DC /* HI((2^2*(1+28/32+1/64))^(1/3)) = 1.962856 */
	.quad	0x3FFF93E9DAD7A4A6 /* HI((2^2*(1+29/32+1/64))^(1/3)) = 1.973612 */
	.quad	0x3FFFBF7E4E8CC9CB /* HI((2^2*(1+30/32+1/64))^(1/3)) = 1.984251 */
	.quad	0x3FFFEA9C61E47CD3 /* HI((2^2*(1+31/32+1/64))^(1/3)) = 1.994778 */
	.align	32
	.quad	0x3F93750AD588F115, 0x3F93750AD588F115, 0x3F93750AD588F115, 0x3F93750AD588F115 /* _dA7 */
	.align	32
	.quad	0xBF98090D6221A247, 0xBF98090D6221A247, 0xBF98090D6221A247, 0xBF98090D6221A247 /* _dA6 */
	.align	32
	.quad	0x3F9EE7113506AC12, 0x3F9EE7113506AC12, 0x3F9EE7113506AC12, 0x3F9EE7113506AC12 /* _dA5 */
	.align	32
	.quad	0xBFA511E8D2B3183B, 0xBFA511E8D2B3183B, 0xBFA511E8D2B3183B, 0xBFA511E8D2B3183B /* _dA4 */
	.align	32
	.quad	0x3FAF9ADD3C0CA458, 0x3FAF9ADD3C0CA458, 0x3FAF9ADD3C0CA458, 0x3FAF9ADD3C0CA458 /* _dA3 */
	.align	32
	.quad	0xBFBC71C71C71C71C, 0xBFBC71C71C71C71C, 0xBFBC71C71C71C71C, 0xBFBC71C71C71C71C /* _dA2 */
	.align	32
	.quad	0x3FD5555555555555, 0x3FD5555555555555, 0x3FD5555555555555, 0x3FD5555555555555 /* _dA1 */
	.align	32
	.quad	0xBFF0400000000000, 0xBFF0400000000000, 0xBFF0400000000000, 0xBFF0400000000000 /* _dNeg65Div64 */
	.align	32
	.quad	0x000FC00000000000, 0x000FC00000000000, 0x000FC00000000000, 0x000FC00000000000 /* _dSgnf6Mask */
	.align	32
	.quad	0xBFF0000000000000, 0xBFF0000000000000, 0xBFF0000000000000, 0xBFF0000000000000 /* _dNegOne */
	.align	32
	.quad	0x000FFFFFFFFFFFFF, 0x000FFFFFFFFFFFFF, 0x000FFFFFFFFFFFFF, 0x000FFFFFFFFFFFFF /* _dMantissaMask */
	.align	32
	.quad	0xFFF0000000000000, 0xFFF0000000000000, 0xFFF0000000000000, 0xFFF0000000000000 /* _lExpHiMask */
	.align	32
	.quad	0x00000000000007FF, 0x00000000000007FF, 0x00000000000007FF, 0x00000000000007FF /* _lExpLoMask */
	.align	32
	.quad	0x0000000000001556, 0x0000000000001556, 0x0000000000001556, 0x0000000000001556 /* _l1556 */
	.align	32
	.long	0x000F8000, 0x000F8000, 0x000F8000, 0x000F8000, 0x000F8000, 0x000F8000, 0x000F8000, 0x000F8000 /* _iRcpIndexMask */
	.align	32
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF /* _iAbsMask */
	.align	32
	.long	0x00000800, 0x00000800, 0x00000800, 0x00000800, 0x00000800, 0x00000800, 0x00000800, 0x00000800 /* _iSignMask */
	.align	32
	.long	0x000002AA, 0x000002AA, 0x000002AA, 0x000002AA, 0x000002AA, 0x000002AA, 0x000002AA, 0x000002AA /* _iBias */
	.align	32
	.long	0x80100000, 0x80100000, 0x80100000, 0x80100000, 0x80100000, 0x80100000, 0x80100000, 0x80100000 /* _iSub */
	.align	32
	.long	0xffdfffff, 0xffdfffff, 0xffdfffff, 0xffdfffff, 0xffdfffff, 0xffdfffff, 0xffdfffff, 0xffdfffff /* _iCmp */
	.align	32
	.type	__svml_dcbrt_data_internal, @object
	.size	__svml_dcbrt_data_internal, .-__svml_dcbrt_data_internal
