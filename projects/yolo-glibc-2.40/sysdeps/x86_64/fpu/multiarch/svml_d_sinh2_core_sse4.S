/* Function sinh vectorized with SSE4.
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
 *   Compute sinh(x) as (exp(x)-exp(-x))/2,
 *   where exp is calculated as
 *   exp(M*ln2 + ln2*(j/2^k) + r) = 2^M * 2^(j/2^k) * exp(r)
 *
 *   Special cases:
 *
 *   sinh(NaN) = quiet NaN, and raise invalid exception
 *   sinh(INF) = that INF
 *   sinh(x)   = x for subnormals
 *   sinh(x) overflows for big x and returns MAXLOG+log(2)
 *
 */

/* Offsets for data table __svml_dsinh_data_internal
 */
#define _dbInvLn2			0
#define _dbLn2hi			16
#define _dbLn2lo			32
#define _dSign				48
#define _dbT				64
#define _dbShifter			2112
#define _iDomainRange			2128
#define _dPC2				2144
#define _dPC3				2160
#define _dPC4				2176
#define _dPC5				2192
#define _lIndexMask			2208

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_sinh_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm2

	/*  Abs argument  */
	movups	_dSign+__svml_dsinh_data_internal(%rip), %xmm0
	lea	_dbT+8+__svml_dsinh_data_internal(%rip), %rsi
	andps	%xmm2, %xmm0
	movaps	%xmm0, %xmm1

	/*
	 *  Load argument
	 * dM = x*2^K/log(2) + RShifter
	 */
	movups	_dbInvLn2+__svml_dsinh_data_internal(%rip), %xmm10
	pxor	%xmm2, %xmm1
	mulpd	%xmm1, %xmm10
	movups	_dbShifter+__svml_dsinh_data_internal(%rip), %xmm5
	addpd	%xmm5, %xmm10

	/*
	 *  R
	 * dN = dM - RShifter
	 */
	movaps	%xmm10, %xmm7
	subpd	%xmm5, %xmm7

	/* dR = dX - dN*Log2_hi/2^K */
	movups	_dbLn2hi+__svml_dsinh_data_internal(%rip), %xmm6
	mulpd	%xmm7, %xmm6

	/* dR = (dX - dN*Log2_hi/2^K) - dN*Log2_lo/2^K */
	movups	_dbLn2lo+__svml_dsinh_data_internal(%rip), %xmm8
	mulpd	%xmm7, %xmm8

	/*
	 * Check for overflow\underflow
	 *
	 */
	pshufd	$221, %xmm1, %xmm4
	subpd	%xmm6, %xmm1
	subpd	%xmm8, %xmm1

	/* VLOAD_CONST( D, dPC[0],         TAB._dPC1 ); */
	movq	_iDomainRange+__svml_dsinh_data_internal(%rip), %xmm3
	pcmpgtd	%xmm3, %xmm4

	/* dR2 = dR^2 */
	movaps	%xmm1, %xmm3
	mulpd	%xmm1, %xmm3
	movmskps %xmm4, %edx

	/*
	 * sinh(r) = r*((a1=1)+r^2*(a3+r^2*a5)) = r + r*(r^2*(a3+r^2*a5)) ....
	 * dSinh_r = (a3+r^2*a5)
	 */
	movups	_dPC5+__svml_dsinh_data_internal(%rip), %xmm12

	/*
	 * poly(r) = (dG2+dG1)+dG3*sinh(dR)+dG1*sinh(dR)+(dG1+dG2)*dR2*(a2 +a4*dR2)
	 * dOut = (a2 +a4*dR2)
	 */
	movups	_dPC4+__svml_dsinh_data_internal(%rip), %xmm13
	mulpd	%xmm3, %xmm12
	mulpd	%xmm3, %xmm13
	addpd	_dPC3+__svml_dsinh_data_internal(%rip), %xmm12
	addpd	_dPC2+__svml_dsinh_data_internal(%rip), %xmm13

	/* dSinh_r = r^2*(a3+r^2*a5) */
	mulpd	%xmm3, %xmm12

	/* dOut = dR2*(a2 +a4*dR2) */
	mulpd	%xmm13, %xmm3

	/* dSinh_r = r + r*(r^2*(a3+r^2*a5)) */
	mulpd	%xmm1, %xmm12

	/*
	 *  Index and lookup
	 * j
	 */
	movups	_lIndexMask+__svml_dsinh_data_internal(%rip), %xmm9
	andps	%xmm10, %xmm9
	movd	%xmm9, %eax

	/* split j and N */
	pxor	%xmm9, %xmm10

	/*
	 *  G1, G2, G3: dTdif, dTn * 2^N, 2^(-N)
	 * lM now is an EXP(2^N)
	 */
	psllq	$45, %xmm10

	/*  */
	movaps	%xmm10, %xmm4
	pextrw	$4, %xmm9, %ecx
	addpd	%xmm12, %xmm1
	shll	$4, %eax
	shll	$4, %ecx
	movq	(%rax, %rsi), %xmm11
	movhpd	(%rcx, %rsi), %xmm11
	paddq	%xmm11, %xmm4

	/*  */
	psubq	%xmm10, %xmm11

	/* dG3 = dTn*2^N + dTn*2^-N */
	movdqa	%xmm4, %xmm14
	addpd	%xmm11, %xmm14

	/* dG2 = dTn*2^N - dTn*2^-N */
	subpd	%xmm11, %xmm4
	movq	-8(%rax, %rsi), %xmm15
	movhpd	-8(%rcx, %rsi), %xmm15
	paddq	%xmm10, %xmm15

	/* dG2 += dG1 */
	addpd	%xmm15, %xmm4

	/* dG1 += dG3 */
	addpd	%xmm14, %xmm15

	/* dOut = dG2*dR2*(a2 +a4*dR2) */
	mulpd	%xmm4, %xmm3

	/* dOut = dG1*sinh(dR)+dG2*dR2*(a2 +a4*dR2) */
	mulpd	%xmm15, %xmm1
	addpd	%xmm1, %xmm3

	/* dOut = dG2 + dG1*sinh(dR)+dG2*dR2*(a2 +a4*dR2) */
	addpd	%xmm3, %xmm4

	/*  Ret H  */
	orps	%xmm4, %xmm0
	andl	$3, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm2

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm2, 32(%rsp)
	movups	%xmm0, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0

	xorl	%eax, %eax
	movq	%r12, 16(%rsp)
	cfi_offset(12, -64)
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -72)
	movl	%edx, %r13d
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
	movups	48(%rsp), %xmm0

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movsd	32(%rsp, %r14, 8), %xmm0
	call	sinh@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movsd	%xmm0, 48(%rsp, %r14, 8)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN2v_sinh_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_dsinh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _dbInvLn2[2][2];
	__declspec(align(16)) VUINT32 _dbLn2hi[2][2];
	__declspec(align(16)) VUINT32 _dbLn2lo[2][2];
	__declspec(align(16)) VUINT32 _dSign[2][2]; // 0x8000000000000000
	__declspec(align(16)) VUINT32 _dbT[(1<<7)][2][2]; // precalc poly coeff
	__declspec(align(16)) VUINT32 _dbShifter[2][2];
	__declspec(align(16)) VUINT32 _iDomainRange[4][1];
	__declspec(align(16)) VUINT32 _dPC2[2][2];
	__declspec(align(16)) VUINT32 _dPC3[2][2];
	__declspec(align(16)) VUINT32 _dPC4[2][2];
	__declspec(align(16)) VUINT32 _dPC5[2][2];
	__declspec(align(16)) VUINT32 _lIndexMask[2][2];
} __svml_dsinh_data_internal;
#endif
__svml_dsinh_data_internal:
	.quad	0x3FF71547652B82FE, 0x3FF71547652B82FE /* _dbInvLn2 = 1/log(2) */
	.align	16
	.quad	0x3FE62E42FEFA0000, 0x3FE62E42FEFA0000 /* _dbLn2hi  = log(2) hi */
	.align	16
	.quad	0x3D7CF79ABC9E3B3A, 0x3D7CF79ABC9E3B3A /* _dbLn2lo  = log(2) lo */
	.align	16
	.quad	0x8000000000000000, 0x8000000000000000 /* _dSign */
	//_dbT
	.align	16
	.quad	0x0000000000000000, 0x3FE0000000000000 // 2^( 0 /128-1) - 2^(- 0 /128-1), 2^(- 0 /128-1)
	.quad	0x3F762E4A19BD1E74, 0x3FDFD3C22B8F71F1 // 2^( 1 /128-1) - 2^(- 1 /128-1), 2^(- 1 /128-1)
	.quad	0x3F862E5F6A0DFD36, 0x3FDFA7C1819E90D8 // 2^( 2 /128-1) - 2^(- 2 /128-1), 2^(- 2 /128-1)
	.quad	0x3F90A2E234040F5F, 0x3FDF7BFDAD9CBE14 // 2^( 3 /128-1) - 2^(- 3 /128-1), 2^(- 3 /128-1)
	.quad	0x3F962EB4ABCC5A81, 0x3FDF50765B6E4540 // 2^( 4 /128-1) - 2^(- 4 /128-1), 2^(- 4 /128-1)
	.quad	0x3F9BBAB1C5033244, 0x3FDF252B376BBA97 // 2^( 5 /128-1) - 2^(- 5 /128-1), 2^(- 5 /128-1)
	.quad	0x3FA0A372144EEB45, 0x3FDEFA1BEE615A27 // 2^( 6 /128-1) - 2^(- 6 /128-1), 2^(- 6 /128-1)
	.quad	0x3FA369AB3FFBF8B0, 0x3FDECF482D8E67F1 // 2^( 7 /128-1) - 2^(- 7 /128-1), 2^(- 7 /128-1)
	.quad	0x3FA63009BA740A2A, 0x3FDEA4AFA2A490DA // 2^( 8 /128-1) - 2^(- 8 /128-1), 2^(- 8 /128-1)
	.quad	0x3FA8F692D8EA1B5A, 0x3FDE7A51FBC74C83 // 2^( 9 /128-1) - 2^(- 9 /128-1), 2^(- 9 /128-1)
	.quad	0x3FABBD4BF0E31A6F, 0x3FDE502EE78B3FF6 // 2^( 10 /128-1) - 2^(- 10 /128-1), 2^(- 10 /128-1)
	.quad	0x3FAE843A5840286A, 0x3FDE264614F5A129 // 2^( 11 /128-1) - 2^(- 11 /128-1), 2^(- 11 /128-1)
	.quad	0x3FB0A5B1B2A46D0A, 0x3FDDFC97337B9B5F // 2^( 12 /128-1) - 2^(- 12 /128-1), 2^(- 12 /128-1)
	.quad	0x3FB20966375ABCDF, 0x3FDDD321F301B460 // 2^( 13 /128-1) - 2^(- 13 /128-1), 2^(- 13 /128-1)
	.quad	0x3FB36D3D65DCA4E8, 0x3FDDA9E603DB3285 // 2^( 14 /128-1) - 2^(- 14 /128-1), 2^(- 14 /128-1)
	.quad	0x3FB4D139EA06642A, 0x3FDD80E316C98398 // 2^( 15 /128-1) - 2^(- 15 /128-1), 2^(- 15 /128-1)
	.quad	0x3FB6355E6FFBF9BA, 0x3FDD5818DCFBA487 // 2^( 16 /128-1) - 2^(- 16 /128-1), 2^(- 16 /128-1)
	.quad	0x3FB799ADA42E4788, 0x3FDD2F87080D89F2 // 2^( 17 /128-1) - 2^(- 17 /128-1), 2^(- 17 /128-1)
	.quad	0x3FB8FE2A336035BC, 0x3FDD072D4A07897C // 2^( 18 /128-1) - 2^(- 18 /128-1), 2^(- 18 /128-1)
	.quad	0x3FBA62D6CAABD6B6, 0x3FDCDF0B555DC3FA // 2^( 19 /128-1) - 2^(- 19 /128-1), 2^(- 19 /128-1)
	.quad	0x3FBBC7B617878BAF, 0x3FDCB720DCEF9069 // 2^( 20 /128-1) - 2^(- 20 /128-1), 2^(- 20 /128-1)
	.quad	0x3FBD2CCAC7CB2A11, 0x3FDC8F6D9406E7B5 // 2^( 21 /128-1) - 2^(- 21 /128-1), 2^(- 21 /128-1)
	.quad	0x3FBE921789B52185, 0x3FDC67F12E57D14B // 2^( 22 /128-1) - 2^(- 22 /128-1), 2^(- 22 /128-1)
	.quad	0x3FBFF79F0BEFA2C7, 0x3FDC40AB5FFFD07A // 2^( 23 /128-1) - 2^(- 23 /128-1), 2^(- 23 /128-1)
	.quad	0x3FC0AEB1FECAE3A9, 0x3FDC199BDD85529C // 2^( 24 /128-1) - 2^(- 24 /128-1), 2^(- 24 /128-1)
	.quad	0x3FC161B4871C5CEC, 0x3FDBF2C25BD71E09 // 2^( 25 /128-1) - 2^(- 25 /128-1), 2^(- 25 /128-1)
	.quad	0x3FC214D876F26FD0, 0x3FDBCC1E904BC1D2 // 2^( 26 /128-1) - 2^(- 26 /128-1), 2^(- 26 /128-1)
	.quad	0x3FC2C81F2693816F, 0x3FDBA5B030A1064A // 2^( 27 /128-1) - 2^(- 27 /128-1), 2^(- 27 /128-1)
	.quad	0x3FC37B89EE88BEF7, 0x3FDB7F76F2FB5E47 // 2^( 28 /128-1) - 2^(- 28 /128-1), 2^(- 28 /128-1)
	.quad	0x3FC42F1A27A0B3CD, 0x3FDB59728DE5593A // 2^( 29 /128-1) - 2^(- 29 /128-1), 2^(- 29 /128-1)
	.quad	0x3FC4E2D12AF1E037, 0x3FDB33A2B84F15FB // 2^( 30 /128-1) - 2^(- 30 /128-1), 2^(- 30 /128-1)
	.quad	0x3FC596B051DD508D, 0x3FDB0E07298DB666 // 2^( 31 /128-1) - 2^(- 31 /128-1), 2^(- 31 /128-1)
	.quad	0x3FC64AB8F61134FA, 0x3FDAE89F995AD3AD // 2^( 32 /128-1) - 2^(- 32 /128-1), 2^(- 32 /128-1)
	.quad	0x3FC6FEEC718B79D1, 0x3FDAC36BBFD3F37A // 2^( 33 /128-1) - 2^(- 33 /128-1), 2^(- 33 /128-1)
	.quad	0x3FC7B34C1E9C607F, 0x3FDA9E6B5579FDBF // 2^( 34 /128-1) - 2^(- 34 /128-1), 2^(- 34 /128-1)
	.quad	0x3FC867D957E91912, 0x3FDA799E1330B358 // 2^( 35 /128-1) - 2^(- 35 /128-1), 2^(- 35 /128-1)
	.quad	0x3FC91C95786E5C72, 0x3FDA5503B23E255D // 2^( 36 /128-1) - 2^(- 36 /128-1), 2^(- 36 /128-1)
	.quad	0x3FC9D181DB83072F, 0x3FDA309BEC4A2D33 // 2^( 37 /128-1) - 2^(- 37 /128-1), 2^(- 37 /128-1)
	.quad	0x3FCA869FDCDAB512, 0x3FDA0C667B5DE565 // 2^( 38 /128-1) - 2^(- 38 /128-1), 2^(- 38 /128-1)
	.quad	0x3FCB3BF0D8885D4C, 0x3FD9E86319E32323 // 2^( 39 /128-1) - 2^(- 39 /128-1), 2^(- 39 /128-1)
	.quad	0x3FCBF1762B00EF69, 0x3FD9C49182A3F090 // 2^( 40 /128-1) - 2^(- 40 /128-1), 2^(- 40 /128-1)
	.quad	0x3FCCA731311DF0FB, 0x3FD9A0F170CA07BA // 2^( 41 /128-1) - 2^(- 41 /128-1), 2^(- 41 /128-1)
	.quad	0x3FCD5D2348201C09, 0x3FD97D829FDE4E50 // 2^( 42 /128-1) - 2^(- 42 /128-1), 2^(- 42 /128-1)
	.quad	0x3FCE134DCDB1FE3E, 0x3FD95A44CBC8520F // 2^( 43 /128-1) - 2^(- 43 /128-1), 2^(- 43 /128-1)
	.quad	0x3FCEC9B21FEA98EA, 0x3FD93737B0CDC5E5 // 2^( 44 /128-1) - 2^(- 44 /128-1), 2^(- 44 /128-1)
	.quad	0x3FCF80519D5001D3, 0x3FD9145B0B91FFC6 // 2^( 45 /128-1) - 2^(- 45 /128-1), 2^(- 45 /128-1)
	.quad	0x3FD01B96D26D026A, 0x3FD8F1AE99157736 // 2^( 46 /128-1) - 2^(- 46 /128-1), 2^(- 46 /128-1)
	.quad	0x3FD07723CAFA6331, 0x3FD8CF3216B5448C // 2^( 47 /128-1) - 2^(- 47 /128-1), 2^(- 47 /128-1)
	.quad	0x3FD0D2D06841B373, 0x3FD8ACE5422AA0DB // 2^( 48 /128-1) - 2^(- 48 /128-1), 2^(- 48 /128-1)
	.quad	0x3FD12E9D5A715381, 0x3FD88AC7D98A6699 // 2^( 49 /128-1) - 2^(- 49 /128-1), 2^(- 49 /128-1)
	.quad	0x3FD18A8B51F5C661, 0x3FD868D99B4492ED // 2^( 50 /128-1) - 2^(- 50 /128-1), 2^(- 50 /128-1)
	.quad	0x3FD1E69AFF7B04D7, 0x3FD8471A4623C7AD // 2^( 51 /128-1) - 2^(- 51 /128-1), 2^(- 51 /128-1)
	.quad	0x3FD242CD13EDD0F1, 0x3FD82589994CCE13 // 2^( 52 /128-1) - 2^(- 52 /128-1), 2^(- 52 /128-1)
	.quad	0x3FD29F22407D0A0C, 0x3FD80427543E1A12 // 2^( 53 /128-1) - 2^(- 53 /128-1), 2^(- 53 /128-1)
	.quad	0x3FD2FB9B369B0153, 0x3FD7E2F336CF4E62 // 2^( 54 /128-1) - 2^(- 54 /128-1), 2^(- 54 /128-1)
	.quad	0x3FD35838A7FECEC8, 0x3FD7C1ED0130C132 // 2^( 55 /128-1) - 2^(- 55 /128-1), 2^(- 55 /128-1)
	.quad	0x3FD3B4FB46A5A6CC, 0x3FD7A11473EB0187 // 2^( 56 /128-1) - 2^(- 56 /128-1), 2^(- 56 /128-1)
	.quad	0x3FD411E3C4D4302F, 0x3FD780694FDE5D3F // 2^( 57 /128-1) - 2^(- 57 /128-1), 2^(- 57 /128-1)
	.quad	0x3FD46EF2D517DAC8, 0x3FD75FEB564267C9 // 2^( 58 /128-1) - 2^(- 58 /128-1), 2^(- 58 /128-1)
	.quad	0x3FD4CC292A48369E, 0x3FD73F9A48A58174 // 2^( 59 /128-1) - 2^(- 59 /128-1), 2^(- 59 /128-1)
	.quad	0x3FD5298777884B96, 0x3FD71F75E8EC5F74 // 2^( 60 /128-1) - 2^(- 60 /128-1), 2^(- 60 /128-1)
	.quad	0x3FD5870E7047F1BC, 0x3FD6FF7DF9519484 // 2^( 61 /128-1) - 2^(- 61 /128-1), 2^(- 61 /128-1)
	.quad	0x3FD5E4BEC8452A1A, 0x3FD6DFB23C651A2F // 2^( 62 /128-1) - 2^(- 62 /128-1), 2^(- 62 /128-1)
	.quad	0x3FD64299338D7827, 0x3FD6C012750BDABF // 2^( 63 /128-1) - 2^(- 63 /128-1), 2^(- 63 /128-1)
	.quad	0x3FD6A09E667F3BCD, 0x3FD6A09E667F3BCD // 2^( 64 /128-1) - 2^(- 64 /128-1), 2^(- 64 /128-1)
	.quad	0x3FD6FECF15CB0C0B, 0x3FD68155D44CA973 // 2^( 65 /128-1) - 2^(- 65 /128-1), 2^(- 65 /128-1)
	.quad	0x3FD75D2BF6751239, 0x3FD6623882552225 // 2^( 66 /128-1) - 2^(- 66 /128-1), 2^(- 66 /128-1)
	.quad	0x3FD7BBB5BDD665E8, 0x3FD6434634CCC320 // 2^( 67 /128-1) - 2^(- 67 /128-1), 2^(- 67 /128-1)
	.quad	0x3FD81A6D219E6963, 0x3FD6247EB03A5585 // 2^( 68 /128-1) - 2^(- 68 /128-1), 2^(- 68 /128-1)
	.quad	0x3FD87952D7D426DF, 0x3FD605E1B976DC09 // 2^( 69 /128-1) - 2^(- 69 /128-1), 2^(- 69 /128-1)
	.quad	0x3FD8D86796D7AE49, 0x3FD5E76F15AD2148 // 2^( 70 /128-1) - 2^(- 70 /128-1), 2^(- 70 /128-1)
	.quad	0x3FD937AC156373C8, 0x3FD5C9268A5946B7 // 2^( 71 /128-1) - 2^(- 71 /128-1), 2^(- 71 /128-1)
	.quad	0x3FD997210A8DAEE4, 0x3FD5AB07DD485429 // 2^( 72 /128-1) - 2^(- 72 /128-1), 2^(- 72 /128-1)
	.quad	0x3FD9F6C72DC9BA68, 0x3FD58D12D497C7FD // 2^( 73 /128-1) - 2^(- 73 /128-1), 2^(- 73 /128-1)
	.quad	0x3FDA569F36E974EA, 0x3FD56F4736B527DA // 2^( 74 /128-1) - 2^(- 74 /128-1), 2^(- 74 /128-1)
	.quad	0x3FDAB6A9DE1EA215, 0x3FD551A4CA5D920F // 2^( 75 /128-1) - 2^(- 75 /128-1), 2^(- 75 /128-1)
	.quad	0x3FDB16E7DBFC4CA3, 0x3FD5342B569D4F82 // 2^( 76 /128-1) - 2^(- 76 /128-1), 2^(- 76 /128-1)
	.quad	0x3FDB7759E9782918, 0x3FD516DAA2CF6642 // 2^( 77 /128-1) - 2^(- 77 /128-1), 2^(- 77 /128-1)
	.quad	0x3FDBD800BFEBF932, 0x3FD4F9B2769D2CA7 // 2^( 78 /128-1) - 2^(- 78 /128-1), 2^(- 78 /128-1)
	.quad	0x3FDC38DD1916F025, 0x3FD4DCB299FDDD0D // 2^( 79 /128-1) - 2^(- 79 /128-1), 2^(- 79 /128-1)
	.quad	0x3FDC99EFAF1F1790, 0x3FD4BFDAD5362A27 // 2^( 80 /128-1) - 2^(- 80 /128-1), 2^(- 80 /128-1)
	.quad	0x3FDCFB393C92B539, 0x3FD4A32AF0D7D3DE // 2^( 81 /128-1) - 2^(- 81 /128-1), 2^(- 81 /128-1)
	.quad	0x3FDD5CBA7C69B19C, 0x3FD486A2B5C13CD0 // 2^( 82 /128-1) - 2^(- 82 /128-1), 2^(- 82 /128-1)
	.quad	0x3FDDBE742A06FF34, 0x3FD46A41ED1D0057 // 2^( 83 /128-1) - 2^(- 83 /128-1), 2^(- 83 /128-1)
	.quad	0x3FDE2067013A029D, 0x3FD44E086061892D // 2^( 84 /128-1) - 2^(- 84 /128-1), 2^(- 84 /128-1)
	.quad	0x3FDE8293BE3FFB87, 0x3FD431F5D950A897 // 2^( 85 /128-1) - 2^(- 85 /128-1), 2^(- 85 /128-1)
	.quad	0x3FDEE4FB1DC56E75, 0x3FD4160A21F72E2A // 2^( 86 /128-1) - 2^(- 86 /128-1), 2^(- 86 /128-1)
	.quad	0x3FDF479DDCE78F58, 0x3FD3FA4504AC801C // 2^( 87 /128-1) - 2^(- 87 /128-1), 2^(- 87 /128-1)
	.quad	0x3FDFAA7CB935ACFE, 0x3FD3DEA64C123422 // 2^( 88 /128-1) - 2^(- 88 /128-1), 2^(- 88 /128-1)
	.quad	0x3FE006CC38594EB1, 0x3FD3C32DC313A8E5 // 2^( 89 /128-1) - 2^(- 89 /128-1), 2^(- 89 /128-1)
	.quad	0x3FE03878E0EB1569, 0x3FD3A7DB34E59FF7 // 2^( 90 /128-1) - 2^(- 90 /128-1), 2^(- 90 /128-1)
	.quad	0x3FE06A44B5C74101, 0x3FD38CAE6D05D866 // 2^( 91 /128-1) - 2^(- 91 /128-1), 2^(- 91 /128-1)
	.quad	0x3FE09C3016A0D077, 0x3FD371A7373AA9CB // 2^( 92 /128-1) - 2^(- 92 /128-1), 2^(- 92 /128-1)
	.quad	0x3FE0CE3B63676360, 0x3FD356C55F929FF1 // 2^( 93 /128-1) - 2^(- 93 /128-1), 2^(- 93 /128-1)
	.quad	0x3FE10066FC47F240, 0x3FD33C08B26416FF // 2^( 94 /128-1) - 2^(- 94 /128-1), 2^(- 94 /128-1)
	.quad	0x3FE132B341AD8761, 0x3FD32170FC4CD831 // 2^( 95 /128-1) - 2^(- 95 /128-1), 2^(- 95 /128-1)
	.quad	0x3FE165209441F823, 0x3FD306FE0A31B715 // 2^( 96 /128-1) - 2^(- 96 /128-1), 2^(- 96 /128-1)
	.quad	0x3FE197AF54EE9EBB, 0x3FD2ECAFA93E2F56 // 2^( 97 /128-1) - 2^(- 97 /128-1), 2^(- 97 /128-1)
	.quad	0x3FE1CA5FE4DD1475, 0x3FD2D285A6E4030B // 2^( 98 /128-1) - 2^(- 98 /128-1), 2^(- 98 /128-1)
	.quad	0x3FE1FD32A577EC72, 0x3FD2B87FD0DAD990 // 2^( 99 /128-1) - 2^(- 99 /128-1), 2^(- 99 /128-1)
	.quad	0x3FE23027F86B6ED6, 0x3FD29E9DF51FDEE1 // 2^( 100 /128-1) - 2^(- 100 /128-1), 2^(- 100 /128-1)
	.quad	0x3FE263403FA65489, 0x3FD284DFE1F56381 // 2^( 101 /128-1) - 2^(- 101 /128-1), 2^(- 101 /128-1)
	.quad	0x3FE2967BDD5A8364, 0x3FD26B4565E27CDD // 2^( 102 /128-1) - 2^(- 102 /128-1), 2^(- 102 /128-1)
	.quad	0x3FE2C9DB33FDCAE9, 0x3FD251CE4FB2A63F // 2^( 103 /128-1) - 2^(- 103 /128-1), 2^(- 103 /128-1)
	.quad	0x3FE2FD5EA64AA180, 0x3FD2387A6E756238 // 2^( 104 /128-1) - 2^(- 104 /128-1), 2^(- 104 /128-1)
	.quad	0x3FE331069740E22F, 0x3FD21F49917DDC96 // 2^( 105 /128-1) - 2^(- 105 /128-1), 2^(- 105 /128-1)
	.quad	0x3FE364D36A268AE0, 0x3FD2063B88628CD6 // 2^( 106 /128-1) - 2^(- 106 /128-1), 2^(- 106 /128-1)
	.quad	0x3FE398C582887B27, 0x3FD1ED5022FCD91D // 2^( 107 /128-1) - 2^(- 107 /128-1), 2^(- 107 /128-1)
	.quad	0x3FE3CCDD443B3394, 0x3FD1D4873168B9AA // 2^( 108 /128-1) - 2^(- 108 /128-1), 2^(- 108 /128-1)
	.quad	0x3FE4011B135B9590, 0x3FD1BBE084045CD4 // 2^( 109 /128-1) - 2^(- 109 /128-1), 2^(- 109 /128-1)
	.quad	0x3FE4357F544FA3C1, 0x3FD1A35BEB6FCB75 // 2^( 110 /128-1) - 2^(- 110 /128-1), 2^(- 110 /128-1)
	.quad	0x3FE46A0A6BC742FD, 0x3FD18AF9388C8DEA // 2^( 111 /128-1) - 2^(- 111 /128-1), 2^(- 111 /128-1)
	.quad	0x3FE49EBCBEBCFBCA, 0x3FD172B83C7D517B // 2^( 112 /128-1) - 2^(- 112 /128-1), 2^(- 112 /128-1)
	.quad	0x3FE4D396B276BC6F, 0x3FD15A98C8A58E51 // 2^( 113 /128-1) - 2^(- 113 /128-1), 2^(- 113 /128-1)
	.quad	0x3FE50898AC869B96, 0x3FD1429AAEA92DE0 // 2^( 114 /128-1) - 2^(- 114 /128-1), 2^(- 114 /128-1)
	.quad	0x3FE53DC312CB9B7A, 0x3FD12ABDC06C31CC // 2^( 115 /128-1) - 2^(- 115 /128-1), 2^(- 115 /128-1)
	.quad	0x3FE573164B726DB6, 0x3FD11301D0125B51 // 2^( 116 /128-1) - 2^(- 116 /128-1), 2^(- 116 /128-1)
	.quad	0x3FE5A892BCF6379B, 0x3FD0FB66AFFED31B // 2^( 117 /128-1) - 2^(- 117 /128-1), 2^(- 117 /128-1)
	.quad	0x3FE5DE38CE215725, 0x3FD0E3EC32D3D1A2 // 2^( 118 /128-1) - 2^(- 118 /128-1), 2^(- 118 /128-1)
	.quad	0x3FE61408E60E2888, 0x3FD0CC922B7247F7 // 2^( 119 /128-1) - 2^(- 119 /128-1), 2^(- 119 /128-1)
	.quad	0x3FE64A036C27CC52, 0x3FD0B5586CF9890F // 2^( 120 /128-1) - 2^(- 120 /128-1), 2^(- 120 /128-1)
	.quad	0x3FE68028C82AEE2F, 0x3FD09E3ECAC6F383 // 2^( 121 /128-1) - 2^(- 121 /128-1), 2^(- 121 /128-1)
	.quad	0x3FE6B67962268C43, 0x3FD0874518759BC8 // 2^( 122 /128-1) - 2^(- 122 /128-1), 2^(- 122 /128-1)
	.quad	0x3FE6ECF5A27CBF28, 0x3FD0706B29DDF6DE // 2^( 123 /128-1) - 2^(- 123 /128-1), 2^(- 123 /128-1)
	.quad	0x3FE7239DF1E38286, 0x3FD059B0D3158574 // 2^( 124 /128-1) - 2^(- 124 /128-1), 2^(- 124 /128-1)
	.quad	0x3FE75A72B9657E51, 0x3FD04315E86E7F85 // 2^( 125 /128-1) - 2^(- 125 /128-1), 2^(- 125 /128-1)
	.quad	0x3FE791746262D0A8, 0x3FD02C9A3E778061 // 2^( 126 /128-1) - 2^(- 126 /128-1), 2^(- 126 /128-1)
	.quad	0x3FE7C8A35691D856, 0x3FD0163DA9FB3335 // 2^( 127 /128-1) - 2^(- 127 /128-1), 2^(- 127 /128-1)
	.align	16
	.quad	0x42C8000000000000, 0x42C8000000000000 /* _dbShifter = 1.5 * 2^(52-k) */
	.align	16
	.long	0x40861d99, 0x40861d99, 0x40861d99, 0x40861d99 /* _iDomainRange 0x40861d9ac12a3e85 =(1021*2^K-0.5)*log(2)/2^K -needed for quick exp */
	.align	16
	.quad	0x3FDFFFFFFFFFFDBD, 0x3FDFFFFFFFFFFDBD /* _dPC2 */
	.align	16
	.quad	0x3FC55555555554AD, 0x3FC55555555554AD /* _dPC3 */
	.align	16
	.quad	0x3FA55555CF16D299, 0x3FA55555CF16D299 /* _dPC4 */
	.align	16
	.quad	0x3F8111115712F425, 0x3F8111115712F425 /* _dPC5 */
	.align	16
	.quad	0x000000000000007f, 0x000000000000007f /* _lIndexMask */
	.align	16
	.type	__svml_dsinh_data_internal, @object
	.size	__svml_dsinh_data_internal, .-__svml_dsinh_data_internal
