/* Function cbrt vectorized with SSE4.
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
#define _dA6				1040
#define _dA5				1056
#define _dA4				1072
#define _dA3				1088
#define _dA2				1104
#define _dA1				1120
#define _dNeg65Div64			1136
#define _dSgnf6Mask			1152
#define _dNegOne			1168
#define _dMantissaMask			1184
#define _lExpHiMask			1200
#define _lExpLoMask			1216
#define _l1556				1232
#define _iRcpIndexMask			1248
#define _iAbsMask			1264
#define _iSignMask			1280
#define _iBias				1296
#define _iSub				1312
#define _iCmp				1328

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_cbrt_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/* Calculate CbrtIndex */
	movaps	%xmm0, %xmm10
	psrlq	$52, %xmm10

	/* Load 1/(1+iRcpIndex/32+1/64) reciprocal table value */
	lea	__svml_dcbrt_data_internal(%rip), %r8
	pand	_lExpLoMask+__svml_dcbrt_data_internal(%rip), %xmm10
	movdqu	_l1556+__svml_dcbrt_data_internal(%rip), %xmm9
	pmuludq	%xmm10, %xmm9

	/* If the exponent field is zero - go to callout to process denormals */
	movq	_iAbsMask+__svml_dcbrt_data_internal(%rip), %xmm7

	/* Calculate Rcp table index */
	movq	_iRcpIndexMask+__svml_dcbrt_data_internal(%rip), %xmm13

	/* Get iX - high part of argument */
	pshufd	$221, %xmm0, %xmm4

	/*
	 * Declarations
	 * Load constants
	 */
	movq	_iSignMask+__svml_dcbrt_data_internal(%rip), %xmm1
	pand	%xmm4, %xmm7
	pand	%xmm4, %xmm13

	/* Compute 2^k */
	psrld	$20, %xmm4
	movq	_iBias+__svml_dcbrt_data_internal(%rip), %xmm2
	pand	%xmm1, %xmm4
	pshufd	$136, %xmm9, %xmm15
	por	%xmm2, %xmm4
	psrld	$14, %xmm15
	psrld	$12, %xmm13
	paddd	%xmm15, %xmm4
	pxor	%xmm2, %xmm2
	pslld	$20, %xmm4
	movdqa	%xmm15, %xmm11
	movd	%xmm13, %edx
	paddd	%xmm15, %xmm11
	pshufd	$1, %xmm13, %xmm8
	punpckldq %xmm4, %xmm2

	/*
	 * VAND( L, l2k, = l2k, lExpHiMask );
	 * Argument reduction Z
	 */
	movups	_dMantissaMask+__svml_dcbrt_data_internal(%rip), %xmm1
	movups	_dSgnf6Mask+__svml_dcbrt_data_internal(%rip), %xmm4
	andps	%xmm0, %xmm1
	movd	%xmm8, %ecx
	andps	%xmm0, %xmm4
	orps	_dNegOne+__svml_dcbrt_data_internal(%rip), %xmm1
	orps	_dNeg65Div64+__svml_dcbrt_data_internal(%rip), %xmm4
	movslq	%edx, %rdx
	subpd	%xmm4, %xmm1
	movslq	%ecx, %rcx
	movsd	(%r8, %rdx), %xmm3
	movq	_iSub+__svml_dcbrt_data_internal(%rip), %xmm5
	psubd	%xmm5, %xmm7
	movhpd	(%r8, %rcx), %xmm3
	mulpd	%xmm1, %xmm3

	/* Polynomial */
	movups	_dA7+__svml_dcbrt_data_internal(%rip), %xmm5
	mulpd	%xmm3, %xmm5
	addpd	_dA6+__svml_dcbrt_data_internal(%rip), %xmm5
	mulpd	%xmm3, %xmm5
	addpd	_dA5+__svml_dcbrt_data_internal(%rip), %xmm5
	mulpd	%xmm3, %xmm5
	addpd	_dA4+__svml_dcbrt_data_internal(%rip), %xmm5
	mulpd	%xmm3, %xmm5
	addpd	_dA3+__svml_dcbrt_data_internal(%rip), %xmm5
	pshufd	$136, %xmm10, %xmm12
	psubd	%xmm15, %xmm12
	psubd	%xmm11, %xmm12
	mulpd	%xmm3, %xmm5
	pslld	$8, %xmm12
	paddd	%xmm12, %xmm13

	/* Load cbrt(2^j*(1+iRcpIndex/32+1/64)) Hi & Lo values */
	movd	%xmm13, %esi
	pshufd	$1, %xmm13, %xmm14
	movq	_iCmp+__svml_dcbrt_data_internal(%rip), %xmm6
	movd	%xmm14, %edi
	pcmpgtd	%xmm6, %xmm7
	movmskps %xmm7, %eax
	addpd	_dA2+__svml_dcbrt_data_internal(%rip), %xmm5
	movslq	%esi, %rsi
	movslq	%edi, %rdi
	mulpd	%xmm3, %xmm5
	movsd	256(%r8, %rsi), %xmm6
	movhpd	256(%r8, %rdi), %xmm6

	/* THi*2^k, TLo*2^k */
	mulpd	%xmm2, %xmm6
	addpd	_dA1+__svml_dcbrt_data_internal(%rip), %xmm5

	/* THi*2^k*Z */
	mulpd	%xmm6, %xmm3

	/* Final reconstruction */
	mulpd	%xmm3, %xmm5
	addpd	%xmm5, %xmm6
	andl	$3, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 eax xmm0 xmm6

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm6, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm6, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 eax xmm6

	xorl	%edx, %edx
	movq	%r12, 16(%rsp)
	cfi_offset(12, -64)
	movl	%edx, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -72)
	movl	%eax, %r13d
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
	cmpl	$2, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm6

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm6

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	call	cbrt@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN2v_cbrt_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dcbrt_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _dRcp[32][2];
	__declspec(align(16)) VUINT32 _dCbrtHiLo[96][2];
	__declspec(align(16)) VUINT32 _dA7[2][2];
	__declspec(align(16)) VUINT32 _dA6[2][2];
	__declspec(align(16)) VUINT32 _dA5[2][2];
	__declspec(align(16)) VUINT32 _dA4[2][2];
	__declspec(align(16)) VUINT32 _dA3[2][2];
	__declspec(align(16)) VUINT32 _dA2[2][2];
	__declspec(align(16)) VUINT32 _dA1[2][2];
	__declspec(align(16)) VUINT32 _dNeg65Div64[2][2];
	__declspec(align(16)) VUINT32 _dSgnf6Mask[2][2];
	__declspec(align(16)) VUINT32 _dNegOne[2][2];
	__declspec(align(16)) VUINT32 _dMantissaMask[2][2];
	__declspec(align(16)) VUINT32 _lExpHiMask[2][2];
	__declspec(align(16)) VUINT32 _lExpLoMask[2][2];
	__declspec(align(16)) VUINT32 _l1556[2][2];
	__declspec(align(16)) VUINT32 _iRcpIndexMask[4][1];
	__declspec(align(16)) VUINT32 _iAbsMask[4][1];
	__declspec(align(16)) VUINT32 _iSignMask[4][1];
	__declspec(align(16)) VUINT32 _iBias[4][1];
	__declspec(align(16)) VUINT32 _iSub[4][1];
	__declspec(align(16)) VUINT32 _iCmp[4][1];
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
	.align	16
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
	.align	16
	.quad	0x3F93750AD588F115, 0x3F93750AD588F115 /* _dA7 */
	.align	16
	.quad	0xBF98090D6221A247, 0xBF98090D6221A247 /* _dA6 */
	.align	16
	.quad	0x3F9EE7113506AC12, 0x3F9EE7113506AC12 /* _dA5 */
	.align	16
	.quad	0xBFA511E8D2B3183B, 0xBFA511E8D2B3183B /* _dA4 */
	.align	16
	.quad	0x3FAF9ADD3C0CA458, 0x3FAF9ADD3C0CA458 /* _dA3 */
	.align	16
	.quad	0xBFBC71C71C71C71C, 0xBFBC71C71C71C71C /* _dA2 */
	.align	16
	.quad	0x3FD5555555555555, 0x3FD5555555555555 /* _dA1 */
	.align	16
	.quad	0xBFF0400000000000, 0xBFF0400000000000 /* _dNeg65Div64 */
	.align	16
	.quad	0x000FC00000000000, 0x000FC00000000000 /* _dSgnf6Mask */
	.align	16
	.quad	0xBFF0000000000000, 0xBFF0000000000000 /* _dNegOne */
	.align	16
	.quad	0x000FFFFFFFFFFFFF, 0x000FFFFFFFFFFFFF /* _dMantissaMask */
	.align	16
	.quad	0xFFF0000000000000, 0xFFF0000000000000 /* _lExpHiMask */
	.align	16
	.quad	0x00000000000007FF, 0x00000000000007FF /* _lExpLoMask */
	.align	16
	.quad	0x0000000000001556, 0x0000000000001556 /* _l1556 */
	.align	16
	.long	0x000F8000, 0x000F8000, 0x000F8000, 0x000F8000 /* _iRcpIndexMask */
	.align	16
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF /* _iAbsMask */
	.align	16
	.long	0x00000800, 0x00000800, 0x00000800, 0x00000800 /* _iSignMask */
	.align	16
	.long	0x000002AA, 0x000002AA, 0x000002AA, 0x000002AA /* _iBias */
	.align	16
	.long	0x80100000, 0x80100000, 0x80100000, 0x80100000 /* _iSub */
	.align	16
	.long	0xffdfffff, 0xffdfffff, 0xffdfffff, 0xffdfffff /* _iCmp */
	.align	16
	.type	__svml_dcbrt_data_internal, @object
	.size	__svml_dcbrt_data_internal, .-__svml_dcbrt_data_internal
