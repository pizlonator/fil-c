/* Function atan2f vectorized with AVX2.
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
#define sSIGN_MASK			32
#define sABS_MASK			64
#define sPIO2				96
#define sPI				128
#define sPC8				160
#define sPC7				192
#define sPC6				224
#define sPC5				256
#define sPC4				288
#define sPC3				320
#define sPC2				352
#define sPC1				384
#define sPC0				416
#define iCHK_WORK_SUB			448
#define iCHK_WORK_CMP			480

#include <sysdep.h>

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8vv_atan2f_avx2)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-32, %rsp
	subq	$128, %rsp
	xorl	%edx, %edx

	/*
	 * #define NO_VECTOR_ZERO_ATAN2_ARGS
	 *  Declarations
	 * Variables
	 * Constants
	 *  The end of declarations
	 *  Implementation
	 * Arguments signs
	 */
	vmovups	sABS_MASK+__svml_satan2_data_internal(%rip), %ymm2

	/* Testing on working interval. */
	vmovups	iCHK_WORK_SUB+__svml_satan2_data_internal(%rip), %ymm15
	vmovups	iCHK_WORK_CMP+__svml_satan2_data_internal(%rip), %ymm9

	/* if x<0, sPI = Pi, else sPI =0 */
	vmovups	__svml_satan2_data_internal(%rip), %ymm5
	vmovaps	%ymm1, %ymm7
	vandps	%ymm2, %ymm7, %ymm13
	vandps	%ymm2, %ymm0, %ymm12
	vcmplt_oqps %ymm13, %ymm12, %ymm4
	vcmple_oqps %ymm5, %ymm7, %ymm6
	vpsubd	%ymm15, %ymm13, %ymm10
	vpsubd	%ymm15, %ymm12, %ymm8

	/*
	 * 1) If y<x then a= y, b=x, PIO2=0
	 * 2) If y>x then a=-x, b=y, PIO2=Pi/2
	 */
	vorps	sSIGN_MASK+__svml_satan2_data_internal(%rip), %ymm13, %ymm3
	vblendvps %ymm4, %ymm12, %ymm3, %ymm14
	vblendvps %ymm4, %ymm13, %ymm12, %ymm3

	/* Division a/b. */
	vdivps	%ymm3, %ymm14, %ymm11
	vpcmpgtd %ymm9, %ymm10, %ymm14
	vpcmpeqd %ymm9, %ymm10, %ymm15
	vpor	%ymm15, %ymm14, %ymm10
	vmovups	sPC7+__svml_satan2_data_internal(%rip), %ymm15
	vpcmpgtd %ymm9, %ymm8, %ymm14
	vpcmpeqd %ymm9, %ymm8, %ymm8
	vpor	%ymm8, %ymm14, %ymm9
	vmovups	sPC8+__svml_satan2_data_internal(%rip), %ymm14
	vpor	%ymm9, %ymm10, %ymm10

	/* Polynomial. */
	vmulps	%ymm11, %ymm11, %ymm9
	vmulps	%ymm9, %ymm9, %ymm8
	vfmadd213ps sPC6+__svml_satan2_data_internal(%rip), %ymm8, %ymm14
	vfmadd213ps sPC5+__svml_satan2_data_internal(%rip), %ymm8, %ymm15
	vfmadd213ps sPC4+__svml_satan2_data_internal(%rip), %ymm8, %ymm14
	vfmadd213ps sPC3+__svml_satan2_data_internal(%rip), %ymm8, %ymm15
	vfmadd213ps sPC2+__svml_satan2_data_internal(%rip), %ymm8, %ymm14
	vfmadd213ps sPC1+__svml_satan2_data_internal(%rip), %ymm8, %ymm15
	vfmadd213ps sPC0+__svml_satan2_data_internal(%rip), %ymm8, %ymm14
	vfmadd213ps %ymm14, %ymm9, %ymm15
	vandnps	sPIO2+__svml_satan2_data_internal(%rip), %ymm4, %ymm4

	/* Reconstruction. */
	vfmadd213ps %ymm4, %ymm11, %ymm15
	vxorps	%ymm13, %ymm7, %ymm1
	vandps	sPI+__svml_satan2_data_internal(%rip), %ymm6, %ymm6
	vorps	%ymm1, %ymm15, %ymm11
	vaddps	%ymm11, %ymm6, %ymm8
	vmovmskps %ymm10, %eax
	vxorps	%ymm12, %ymm0, %ymm2
	vorps	%ymm2, %ymm8, %ymm9

	/*  Special branch for fast (vector) processing of zero arguments  */
	testl	%eax, %eax

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm1 ymm2 ymm3 ymm4 ymm5 ymm6 ymm7 ymm9 ymm10 ymm12 ymm13

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
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm7 ymm9

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%ymm9, %ymm0
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
	vmovups	%ymm0, 32(%rsp)
	vmovups	%ymm7, 64(%rsp)
	vmovups	%ymm9, 96(%rsp)
	# LOE rbx r12 r13 r14 r15 edx ymm9

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
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
	vmovups	96(%rsp), %ymm9

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -112; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x90, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -120; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x88, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -32; DW_OP_and; DW_OP_const4s: -128; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xe0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x80, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 ymm9

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	32(%rsp, %r14, 4), %xmm0
	vmovss	64(%rsp, %r14, 4), %xmm1
	call	atan2f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 96(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	cfi_restore(12)
	cfi_restore(13)
	cfi_restore(14)
	# LOE rbx r15 r12d r13d

	/* Auxiliary branch
	 * for out of main path inputs
	 */

L(AUX_BRANCH):
	/* Check if at least on of Y or Y is zero: iAXAYZERO */
	vpcmpeqd %ymm5, %ymm13, %ymm13
	vpcmpeqd %ymm5, %ymm12, %ymm12

	/* Check if both X & Y are not NaNs:  iXYnotNAN */
	vcmpordps %ymm7, %ymm7, %ymm11
	vcmpordps %ymm0, %ymm0, %ymm14

	/*
	 *  Path for zero arguments (at least one of both)
	 * Check if both args are zeros (den. is zero)
	 */
	vcmpeqps %ymm5, %ymm3, %ymm3
	vpor	%ymm12, %ymm13, %ymm15

	/* Set sPIO2 to zero if den. is zero */
	vblendvps %ymm3, %ymm5, %ymm4, %ymm4
	vandps	%ymm14, %ymm11, %ymm8

	/* Check if at least on of Y or Y is zero and not NaN: iAXAYZEROnotNAN */
	vpand	%ymm8, %ymm15, %ymm8

	/* Res = sign(Y)*(X<0)?(PIO2+PI):PIO2 */
	vpcmpgtd %ymm7, %ymm5, %ymm5
	vorps	%ymm1, %ymm4, %ymm1
	vandps	%ymm6, %ymm5, %ymm6
	vaddps	%ymm6, %ymm1, %ymm1

	/* Exclude from previous callout mask zero (and not NaN) arguments */
	vpandn	%ymm10, %ymm8, %ymm10
	vorps	%ymm2, %ymm1, %ymm2

	/* Go to callout */
	vmovmskps %ymm10, %edx

	/* Merge results from main and spec path */
	vblendvps %ymm8, %ymm2, %ymm9, %ymm9

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx r12 r13 r14 r15 edx ymm0 ymm7 ymm9
END(_ZGVdN8vv_atan2f_avx2)

	.section .rodata, "a"
	.align	32

#ifdef __svml_satan2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(32)) VUINT32 sZERO[8][1];
	__declspec(align(32)) VUINT32 sSIGN_MASK[8][1];
	__declspec(align(32)) VUINT32 sABS_MASK[8][1];
	__declspec(align(32)) VUINT32 sPIO2[8][1];
	__declspec(align(32)) VUINT32 sPI[8][1];
	__declspec(align(32)) VUINT32 sPC8[8][1];
	__declspec(align(32)) VUINT32 sPC7[8][1];
	__declspec(align(32)) VUINT32 sPC6[8][1];
	__declspec(align(32)) VUINT32 sPC5[8][1];
	__declspec(align(32)) VUINT32 sPC4[8][1];
	__declspec(align(32)) VUINT32 sPC3[8][1];
	__declspec(align(32)) VUINT32 sPC2[8][1];
	__declspec(align(32)) VUINT32 sPC1[8][1];
	__declspec(align(32)) VUINT32 sPC0[8][1];
	__declspec(align(32)) VUINT32 iCHK_WORK_SUB[8][1];
	__declspec(align(32)) VUINT32 iCHK_WORK_CMP[8][1];
} __svml_satan2_data_internal;
#endif
__svml_satan2_data_internal:
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 // sZERO
	.align	32
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000 // sSIGN_MASK
	.align	32
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF // sABS_MASK
	.align	32
	.long	0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB // sPIO2
	.align	32
	.long	0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB // sPI
	.align	32
	.long	0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0 // sA08
	.align	32
	.long	0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631 // sA07
	.align	32
	.long	0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384 // sA06
	.align	32
	.long	0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629 // sA05
	.align	32
	.long	0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474 // sA04
	.align	32
	.long	0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8 // sA03
	.align	32
	.long	0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F // sA02
	.align	32
	.long	0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49 // sA01
	.align	32
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 // sA00
	.align	32
	.long	0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000 // iCHK_WORK_SUB
	.align	32
	.long	0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000 // iCHK_WORK_CMP
	.align	32
	.type	__svml_satan2_data_internal, @object
	.size	__svml_satan2_data_internal, .-__svml_satan2_data_internal
