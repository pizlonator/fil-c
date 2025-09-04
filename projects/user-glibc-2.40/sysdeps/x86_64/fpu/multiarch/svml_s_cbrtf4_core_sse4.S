/* Function cbrtf vectorized with SSE4.
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
#define _sP1				528
#define _sMantissaMask			544
#define _sMantissaMask1			560
#define _sExpMask			576
#define _sExpMask1			592
#define _iRcpIndexMask			608
#define _iBExpMask			624
#define _iSignMask			640
#define _iBias				656
#define _iOne				672
#define _i555				688
#define _iAbsMask			704
#define _iSubConst			720
#define _iCmpConst			736

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_cbrtf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/*
	 * Load constants
	 * Reciprocal index calculation
	 */
	movaps	%xmm0, %xmm2
	movdqu	_iRcpIndexMask+__svml_scbrt_data_internal(%rip), %xmm3
	psrld	$16, %xmm2
	pand	%xmm2, %xmm3

	/* Load reciprocal value */
	lea	__svml_scbrt_data_internal(%rip), %rdx
	pshufd	$1, %xmm3, %xmm5

	/* Get signed biased exponent */
	psrld	$7, %xmm2
	movd	%xmm3, %eax
	movd	%xmm5, %ecx

	/* Get absolute biased exponent */
	movdqu	_iBExpMask+__svml_scbrt_data_internal(%rip), %xmm15

	/*
	 * Calculate exponent/3
	 * i555Exp=(2^{12}-1)/3*exponent
	 */
	movdqu	_i555+__svml_scbrt_data_internal(%rip), %xmm14
	pand	%xmm2, %xmm15
	movslq	%eax, %rax
	movdqa	%xmm14, %xmm5
	movslq	%ecx, %rcx
	psrlq	$32, %xmm14
	pmuludq	%xmm15, %xmm5
	movd	(%rdx, %rax), %xmm4
	movd	(%rdx, %rcx), %xmm6
	punpckldq %xmm6, %xmm4
	movdqa	%xmm15, %xmm6
	psrlq	$32, %xmm15
	pmuludq	%xmm14, %xmm15
	pshufd	$2, %xmm3, %xmm7
	psllq	$32, %xmm15
	pshufd	$3, %xmm3, %xmm8
	movd	%xmm7, %esi
	movd	%xmm8, %edi

	/* Argument reduction */
	movups	_sMantissaMask+__svml_scbrt_data_internal(%rip), %xmm12
	movups	_sMantissaMask1+__svml_scbrt_data_internal(%rip), %xmm11
	andps	%xmm0, %xmm12
	pand	.FLT_17(%rip), %xmm5
	andps	%xmm0, %xmm11
	movslq	%esi, %rsi
	por	%xmm15, %xmm5
	movslq	%edi, %rdi

	/* Get K (exponent=3*k+j) */
	psrld	$12, %xmm5
	orps	_sExpMask+__svml_scbrt_data_internal(%rip), %xmm12
	orps	_sExpMask1+__svml_scbrt_data_internal(%rip), %xmm11
	psubd	_iOne+__svml_scbrt_data_internal(%rip), %xmm6

	/* r=y-y` */
	subps	%xmm11, %xmm12

	/* Get J */
	psubd	%xmm5, %xmm6
	movdqu	_iAbsMask+__svml_scbrt_data_internal(%rip), %xmm1
	psubd	%xmm5, %xmm6
	movd	(%rdx, %rsi), %xmm10
	pand	%xmm0, %xmm1
	movd	(%rdx, %rdi), %xmm9
	psubd	%xmm5, %xmm6
	punpckldq %xmm9, %xmm10

	/* Get 128*J */
	pslld	$7, %xmm6
	punpcklqdq %xmm10, %xmm4

	/*
	 * iCbrtIndex=4*l+128*j
	 * Zero index if callout expected
	 */
	paddd	%xmm6, %xmm3
	psubd	_iSubConst+__svml_scbrt_data_internal(%rip), %xmm1
	pcmpgtd	_iCmpConst+__svml_scbrt_data_internal(%rip), %xmm1

	/* r=(y-y`)*rcp_table(y`) */
	mulps	%xmm12, %xmm4
	movmskps %xmm1, %eax

	/* Biased exponent-1 */
	movdqu	_iSignMask+__svml_scbrt_data_internal(%rip), %xmm13
	pandn	%xmm3, %xmm1

	/*
	 * Add 2/3*(bias-1)+1 to (k+1/3*(bias-1))
	 * Attach sign to exponent
	 */
	movdqu	_iBias+__svml_scbrt_data_internal(%rip), %xmm12
	pand	%xmm13, %xmm2
	paddd	%xmm5, %xmm12

	/* Load Cbrt table Hi & Lo values */
	movd	%xmm1, %r8d
	por	%xmm2, %xmm12
	pshufd	$1, %xmm1, %xmm2
	pslld	$23, %xmm12
	pshufd	$2, %xmm1, %xmm7
	pshufd	$3, %xmm1, %xmm1
	movd	%xmm2, %r9d
	movd	%xmm7, %r10d
	movd	%xmm1, %r11d

	/* Polynomial:    p1+r*(p2*r+r*(p3+r*p4)) */
	movups	_sP2+__svml_scbrt_data_internal(%rip), %xmm11
	mulps	%xmm4, %xmm11
	movslq	%r8d, %r8
	addps	_sP1+__svml_scbrt_data_internal(%rip), %xmm11
	movslq	%r9d, %r9
	movslq	%r10d, %r10
	movslq	%r11d, %r11
	movd	128(%rdx, %r8), %xmm10
	movd	128(%rdx, %r9), %xmm3
	movd	128(%rdx, %r10), %xmm9
	movd	128(%rdx, %r11), %xmm8
	punpckldq %xmm3, %xmm10
	punpckldq %xmm8, %xmm9
	punpcklqdq %xmm9, %xmm10

	/* sCbrtHi *= 2^k */
	mulps	%xmm10, %xmm12

	/* T`*r */
	mulps	%xmm12, %xmm4

	/* (T`*r)*P */
	mulps	%xmm4, %xmm11

	/*
	 * T`*r*P+D`
	 * result = T`+(T`*r*P+D`)
	 */
	addps	%xmm11, %xmm12
	testl	%eax, %eax

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 eax xmm0 xmm12

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm12, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm12, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 eax

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
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm12

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm12

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	cbrtf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_cbrtf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_scbrt_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _sRcp[32][1];
	__declspec(align(16)) VUINT32 _sCbrtHL[96][1];
	__declspec(align(16)) VUINT32 _sP2[4][1];
	__declspec(align(16)) VUINT32 _sP1[4][1];
	__declspec(align(16)) VUINT32 _sMantissaMask[4][1];
	__declspec(align(16)) VUINT32 _sMantissaMask1[4][1];
	__declspec(align(16)) VUINT32 _sExpMask[4][1];
	__declspec(align(16)) VUINT32 _sExpMask1[4][1];
	__declspec(align(16)) VUINT32 _iRcpIndexMask[4][1];
	__declspec(align(16)) VUINT32 _iBExpMask[4][1];
	__declspec(align(16)) VUINT32 _iSignMask[4][1];
	__declspec(align(16)) VUINT32 _iBias[4][1];
	__declspec(align(16)) VUINT32 _iOne[4][1];
	__declspec(align(16)) VUINT32 _i555[4][1];
	__declspec(align(16)) VUINT32 _iAbsMask[4][1];
	__declspec(align(16)) VUINT32 _iSubConst[4][1];
	__declspec(align(16)) VUINT32 _iCmpConst[4][1];
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
	.align	16
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
	.align	16
	.long	0xBDE3A962, 0xBDE3A962, 0xBDE3A962, 0xBDE3A962 /* _sP2 */
	.align	16
	.long	0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91, 0x3EAAAC91 /* _sP1 */
	.align	16
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff /* _sMantissaMask (EXP_MSK3) */
	.align	16
	.long	0x007e0000, 0x007e0000, 0x007e0000, 0x007e0000 /* _sMantissaMask1 (SIG_MASK) */
	.align	16
	.long	0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000 /* _sExpMask  (EXP_MASK) */
	.align	16
	.long	0xBF820000, 0xBF820000, 0xBF820000, 0xBF820000 /* _sExpMask1 (EXP_MASK2) */
	.align	16
	.long	0x0000007c, 0x0000007c, 0x0000007c, 0x0000007c /* _iRcpIndexMask */
	.align	16
	.long	0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff /* _iBExpMask */
	.align	16
	.long	0x00000100, 0x00000100, 0x00000100, 0x00000100 /* _iSignMask */
	.align	16
	.long	0x00000055, 0x00000055, 0x00000055, 0x00000055 /* _iBias */
	.align	16
	.long	0x00000001, 0x00000001, 0x00000001, 0x00000001 /* _iOne */
	.align	16
	.long	0x00000555, 0x00000555, 0x00000555, 0x00000555 /* _i555 */
	.align	16
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _iAbsMask */
	.align	16
	.long	0x80800000, 0x80800000, 0x80800000, 0x80800000 /* _iSubConst */
	.align	16
	.long	0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF /* _iCmpConst */
	.align	16
	.type	__svml_scbrt_data_internal, @object
	.size	__svml_scbrt_data_internal, .-__svml_scbrt_data_internal
	.align	16

.FLT_17:
	.long	0xffffffff, 0x00000000, 0xffffffff, 0x00000000
	.type	.FLT_17, @object
	.size	.FLT_17, 16
