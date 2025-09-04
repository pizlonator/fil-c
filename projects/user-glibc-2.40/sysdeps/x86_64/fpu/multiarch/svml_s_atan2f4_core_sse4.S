/* Function atan2f vectorized with SSE4.
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
 *      For    0.0    <= x <=  7.0/16.0: atan(x) = atan(0.0) + atan(s), where s=(x-0.0)/(1.0+0.0*x)
 *      For  7.0/16.0 <= x <= 11.0/16.0: atan(x) = atan(0.5) + atan(s), where s=(x-0.5)/(1.0+0.5*x)
 *      For 11.0/16.0 <= x <= 19.0/16.0: atan(x) = atan(1.0) + atan(s), where s=(x-1.0)/(1.0+1.0*x)
 *      For 19.0/16.0 <= x <= 39.0/16.0: atan(x) = atan(1.5) + atan(s), where s=(x-1.5)/(1.0+1.5*x)
 *      For 39.0/16.0 <= x <=    inf   : atan(x) = atan(inf) + atan(s), where s=-1.0/x
 *      Where atan(s) ~= s+s^3*Poly11(s^2) on interval |s|<7.0/0.16.
 *
 *
 */

/* Offsets for data table __svml_satan2_data_internal
 */
#define sZERO				0
#define sSIGN_MASK			16
#define sABS_MASK			32
#define sPIO2				48
#define sPI				64
#define sPC8				80
#define sPC7				96
#define sPC6				112
#define sPC5				128
#define sPC4				144
#define sPC3				160
#define sPC2				176
#define sPC1				192
#define sPC0				208
#define iCHK_WORK_SUB			224
#define iCHK_WORK_CMP			240

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4vv_atan2f_sse4)
	subq	$88, %rsp
	cfi_def_cfa_offset(96)
	movaps	%xmm0, %xmm12

	/*
	 * #define NO_VECTOR_ZERO_ATAN2_ARGS
	 *  Declarations
	 * Variables
	 * Constants
	 *  The end of declarations
	 *  Implementation
	 * Arguments signs
	 */
	movups	sABS_MASK+__svml_satan2_data_internal(%rip), %xmm10
	movaps	%xmm1, %xmm13
	movaps	%xmm10, %xmm11
	andps	%xmm12, %xmm10
	andps	%xmm13, %xmm11
	movaps	%xmm10, %xmm7
	cmpltps	%xmm11, %xmm7

	/*
	 * 1) If y<x then a= y, b=x, PIO2=0
	 * 2) If y>x then a=-x, b=y, PIO2=Pi/2
	 */
	movups	sSIGN_MASK+__svml_satan2_data_internal(%rip), %xmm6
	movaps	%xmm7, %xmm0
	orps	%xmm11, %xmm6
	movaps	%xmm10, %xmm4
	andnps	%xmm6, %xmm0
	movaps	%xmm7, %xmm6
	movaps	%xmm11, %xmm5
	andps	%xmm7, %xmm4
	andnps	%xmm10, %xmm6
	andps	%xmm7, %xmm5
	orps	%xmm4, %xmm0
	orps	%xmm5, %xmm6

	/* Division a/b. */
	divps	%xmm6, %xmm0

	/* Testing on working interval. */
	movdqu	iCHK_WORK_SUB+__svml_satan2_data_internal(%rip), %xmm14
	movaps	%xmm11, %xmm15
	movaps	%xmm10, %xmm3
	psubd	%xmm14, %xmm15
	psubd	%xmm14, %xmm3
	movdqa	%xmm15, %xmm1
	movdqu	iCHK_WORK_CMP+__svml_satan2_data_internal(%rip), %xmm2
	movdqa	%xmm3, %xmm14
	pcmpgtd	%xmm2, %xmm1
	pcmpeqd	%xmm2, %xmm15
	pcmpgtd	%xmm2, %xmm14
	pcmpeqd	%xmm2, %xmm3
	por	%xmm15, %xmm1
	por	%xmm3, %xmm14
	por	%xmm14, %xmm1

	/* Polynomial. */
	movaps	%xmm0, %xmm14
	mulps	%xmm0, %xmm14
	movaps	%xmm13, %xmm4
	movmskps %xmm1, %ecx
	movaps	%xmm14, %xmm15
	movaps	%xmm11, %xmm9
	mulps	%xmm14, %xmm15
	pxor	%xmm13, %xmm9
	movups	sPC8+__svml_satan2_data_internal(%rip), %xmm2
	movaps	%xmm10, %xmm8
	mulps	%xmm15, %xmm2
	pxor	%xmm12, %xmm8
	movups	sPC7+__svml_satan2_data_internal(%rip), %xmm3
	xorl	%edx, %edx
	mulps	%xmm15, %xmm3
	addps	sPC6+__svml_satan2_data_internal(%rip), %xmm2
	mulps	%xmm15, %xmm2
	addps	sPC5+__svml_satan2_data_internal(%rip), %xmm3
	mulps	%xmm15, %xmm3
	addps	sPC4+__svml_satan2_data_internal(%rip), %xmm2
	mulps	%xmm15, %xmm2
	addps	sPC3+__svml_satan2_data_internal(%rip), %xmm3
	mulps	%xmm15, %xmm3
	addps	sPC2+__svml_satan2_data_internal(%rip), %xmm2
	mulps	%xmm2, %xmm15
	addps	sPC1+__svml_satan2_data_internal(%rip), %xmm3
	mulps	%xmm3, %xmm14
	addps	sPC0+__svml_satan2_data_internal(%rip), %xmm15

	/* if x<0, sPI = Pi, else sPI =0 */
	movups	__svml_satan2_data_internal(%rip), %xmm5
	xorl	%eax, %eax
	andnps	sPIO2+__svml_satan2_data_internal(%rip), %xmm7
	addps	%xmm14, %xmm15
	cmpleps	%xmm5, %xmm4

	/* Reconstruction. */
	mulps	%xmm15, %xmm0
	andps	sPI+__svml_satan2_data_internal(%rip), %xmm4
	addps	%xmm7, %xmm0
	orps	%xmm9, %xmm0
	addps	%xmm4, %xmm0
	orps	%xmm8, %xmm0

	/*  Special branch for fast (vector) processing of zero arguments  */
	testl	%ecx, %ecx

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0 xmm1 xmm4 xmm5 xmm6 xmm7 xmm8 xmm9 xmm10 xmm11 xmm12 xmm13

	/* Return from auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH_RETURN):
	/*
	 *  Special branch for fast (vector) processing of zero arguments
	 *  The end of implementation
	 */
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0 xmm12 xmm13

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	addq	$88, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(96)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm12, 32(%rsp)
	movups	%xmm13, 48(%rsp)
	movups	%xmm0, 64(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0

	movq	%r12, 16(%rsp)
	cfi_offset(12, -80)
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -88)
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	cfi_offset(14, -96)
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
	movups	64(%rsp), %xmm0

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -80)
	cfi_offset(13, -88)
	cfi_offset(14, -96)
	# LOE rbx rbp r12 r13 r14 r15 xmm0

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	movss	48(%rsp, %r14, 4), %xmm1
	call	atan2f@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 64(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	cfi_restore(12)
	cfi_restore(13)
	cfi_restore(14)
	# LOE rbx rbp r15 r12d r13d

	/* Auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH):
	/* Check if both X & Y are not NaNs:  iXYnotNAN */
	movaps	%xmm13, %xmm3
	movaps	%xmm12, %xmm2
	cmpordps %xmm13, %xmm3
	cmpordps %xmm12, %xmm2

	/*
	 *  Path for zero arguments (at least one of both)
	 * Check if both args are zeros (den. is zero)
	 */
	cmpeqps	%xmm5, %xmm6

	/* Check if at least on of Y or Y is zero: iAXAYZERO */
	pcmpeqd	%xmm5, %xmm11
	pcmpeqd	%xmm5, %xmm10
	andps	%xmm2, %xmm3
	por	%xmm10, %xmm11

	/* Check if at least on of Y or Y is zero and not NaN: iAXAYZEROnotNAN */
	andps	%xmm3, %xmm11

	/* Exclude from previous callout mask zero (and not NaN) arguments */
	movaps	%xmm11, %xmm10
	pandn	%xmm1, %xmm10

	/* Set sPIO2 to zero if den. is zero */
	movaps	%xmm6, %xmm1
	andnps	%xmm7, %xmm1
	andps	%xmm5, %xmm6
	orps	%xmm6, %xmm1

	/* Res = sign(Y)*(X<0)?(PIO2+PI):PIO2 */
	pcmpgtd	%xmm13, %xmm5
	orps	%xmm9, %xmm1
	andps	%xmm4, %xmm5

	/* Merge results from main and spec path */
	movaps	%xmm11, %xmm4
	addps	%xmm5, %xmm1

	/* Go to callout */
	movmskps %xmm10, %edx
	orps	%xmm8, %xmm1
	andnps	%xmm0, %xmm4
	andps	%xmm11, %xmm1
	movaps	%xmm4, %xmm0
	orps	%xmm1, %xmm0

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx rbp r12 r13 r14 r15 eax edx xmm0 xmm12 xmm13
END(_ZGVbN4vv_atan2f_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_satan2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 sZERO[4][1];
	__declspec(align(16)) VUINT32 sSIGN_MASK[4][1];
	__declspec(align(16)) VUINT32 sABS_MASK[4][1];
	__declspec(align(16)) VUINT32 sPIO2[4][1];
	__declspec(align(16)) VUINT32 sPI[4][1];
	__declspec(align(16)) VUINT32 sPC8[4][1];
	__declspec(align(16)) VUINT32 sPC7[4][1];
	__declspec(align(16)) VUINT32 sPC6[4][1];
	__declspec(align(16)) VUINT32 sPC5[4][1];
	__declspec(align(16)) VUINT32 sPC4[4][1];
	__declspec(align(16)) VUINT32 sPC3[4][1];
	__declspec(align(16)) VUINT32 sPC2[4][1];
	__declspec(align(16)) VUINT32 sPC1[4][1];
	__declspec(align(16)) VUINT32 sPC0[4][1];
	__declspec(align(16)) VUINT32 iCHK_WORK_SUB[4][1];
	__declspec(align(16)) VUINT32 iCHK_WORK_CMP[4][1];
} __svml_satan2_data_internal;
#endif
__svml_satan2_data_internal:
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000 // sZERO
	.align	16
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000 // sSIGN_MASK
	.align	16
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF // sABS_MASK
	.align	16
	.long	0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB // sPIO2
	.align	16
	.long	0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB // sPI
	.align	16
	.long	0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0 // sA08
	.align	16
	.long	0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631 // sA07
	.align	16
	.long	0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384 // sA06
	.align	16
	.long	0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629 // sA05
	.align	16
	.long	0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474 // sA04
	.align	16
	.long	0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8 // sA03
	.align	16
	.long	0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F // sA02
	.align	16
	.long	0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49 // sA01
	.align	16
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 // sA00
	.align	16
	.long	0x81000000, 0x81000000, 0x81000000, 0x81000000 // iCHK_WORK_SUB
	.align	16
	.long	0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000 // iCHK_WORK_CMP
	.align	16
	.type	__svml_satan2_data_internal, @object
	.size	__svml_satan2_data_internal, .-__svml_satan2_data_internal
