/* Function atan2f vectorized with AVX-512.
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
#define sONE				64
#define sSIGN_MASK			128
#define sABS_MASK			192
#define sPIO2				256
#define sPI				320
#define sPC8				384
#define sPC7				448
#define sPC6				512
#define sPC5				576
#define sPC4				640
#define sPC3				704
#define sPC2				768
#define sPC1				832
#define sPC0				896
#define iCHK_WORK_SUB			960
#define iCHK_WORK_CMP			1024

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16vv_atan2f_skx)
	pushq	%rbp
	cfi_def_cfa_offset(16)
	movq	%rsp, %rbp
	cfi_def_cfa(6, 16)
	cfi_offset(6, -16)
	andq	$-64, %rsp
	subq	$256, %rsp
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
	vmovups	sABS_MASK+__svml_satan2_data_internal(%rip), %zmm6
	vmovups	sONE+__svml_satan2_data_internal(%rip), %zmm3

	/* Testing on working interval. */
	vmovups	iCHK_WORK_SUB+__svml_satan2_data_internal(%rip), %zmm9
	vmovups	iCHK_WORK_CMP+__svml_satan2_data_internal(%rip), %zmm14

	/*
	 * 1) If y<x then a= y, b=x, PIO2=0
	 * 2) If y>x then a=-x, b=y, PIO2=Pi/2
	 */
	vmovups	sPIO2+__svml_satan2_data_internal(%rip), %zmm4
	vpternlogd $255, %zmm13, %zmm13, %zmm13
	vmovaps	%zmm1, %zmm8
	vandps	%zmm6, %zmm8, %zmm2
	vandps	%zmm6, %zmm0, %zmm1
	vorps	sSIGN_MASK+__svml_satan2_data_internal(%rip), %zmm2, %zmm5
	vpsubd	%zmm9, %zmm2, %zmm10
	vpsubd	%zmm9, %zmm1, %zmm12
	vxorps	%zmm2, %zmm8, %zmm7
	vxorps	%zmm1, %zmm0, %zmm6
	vcmpps	$17, {sae}, %zmm2, %zmm1, %k1
	vpcmpgtd %zmm10, %zmm14, %k2
	vpcmpgtd %zmm12, %zmm14, %k3
	vmovups	sPC6+__svml_satan2_data_internal(%rip), %zmm14
	vblendmps %zmm1, %zmm5, %zmm11{%k1}
	vblendmps %zmm2, %zmm1, %zmm5{%k1}
	vxorps	%zmm4, %zmm4, %zmm4{%k1}

	/*
	 * Division a/b.
	 * Enabled when FMA is available and
	 * performance is better with NR iteration
	 */
	vrcp14ps %zmm5, %zmm15
	vfnmadd231ps {rn-sae}, %zmm5, %zmm15, %zmm3
	vfmadd213ps {rn-sae}, %zmm15, %zmm3, %zmm15
	vmulps	{rn-sae}, %zmm15, %zmm11, %zmm3
	vfnmadd231ps {rn-sae}, %zmm5, %zmm3, %zmm11
	vfmadd213ps {rn-sae}, %zmm3, %zmm11, %zmm15
	vmovups	sPC8+__svml_satan2_data_internal(%rip), %zmm11
	vpternlogd $255, %zmm3, %zmm3, %zmm3

	/* Polynomial. */
	vmulps	{rn-sae}, %zmm15, %zmm15, %zmm9
	vpandnd	%zmm10, %zmm10, %zmm13{%k2}
	vmulps	{rn-sae}, %zmm9, %zmm9, %zmm10
	vfmadd231ps {rn-sae}, %zmm10, %zmm11, %zmm14
	vmovups	sPC5+__svml_satan2_data_internal(%rip), %zmm11
	vpandnd	%zmm12, %zmm12, %zmm3{%k3}
	vpord	%zmm3, %zmm13, %zmm3
	vmovups	sPC4+__svml_satan2_data_internal(%rip), %zmm13
	vmovups	sPC7+__svml_satan2_data_internal(%rip), %zmm12
	vptestmd %zmm3, %zmm3, %k0
	vfmadd213ps {rn-sae}, %zmm13, %zmm10, %zmm14
	vfmadd231ps {rn-sae}, %zmm10, %zmm12, %zmm11
	vmovups	sPC3+__svml_satan2_data_internal(%rip), %zmm12
	vmovups	sPC2+__svml_satan2_data_internal(%rip), %zmm13

	/*  Special branch for fast (vector) processing of zero arguments  */
	kortestw %k0, %k0
	vfmadd213ps {rn-sae}, %zmm12, %zmm10, %zmm11
	vmovups	sPC1+__svml_satan2_data_internal(%rip), %zmm12
	vfmadd213ps {rn-sae}, %zmm13, %zmm10, %zmm14
	vmovups	sPC0+__svml_satan2_data_internal(%rip), %zmm13
	vfmadd213ps {rn-sae}, %zmm12, %zmm10, %zmm11
	vfmadd213ps {rn-sae}, %zmm13, %zmm10, %zmm14
	vfmadd213ps {rn-sae}, %zmm14, %zmm9, %zmm11

	/* Reconstruction. */
	vfmadd213ps {rn-sae}, %zmm4, %zmm15, %zmm11

	/* if x<0, sPI = Pi, else sPI =0 */
	vmovups	__svml_satan2_data_internal(%rip), %zmm15
	vorps	%zmm7, %zmm11, %zmm9
	vcmpps	$18, {sae}, %zmm15, %zmm8, %k4
	vmovups	sPI+__svml_satan2_data_internal(%rip), %zmm11
	vaddps	{rn-sae}, %zmm11, %zmm9, %zmm9{%k4}
	vorps	%zmm6, %zmm9, %zmm10

	/* Go to auxiliary branch */
	jne	L(AUX_BRANCH)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm1 zmm2 zmm3 zmm4 zmm5 zmm6 zmm7 zmm8 zmm10 zmm11

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
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm8 zmm10

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	vmovaps	%zmm10, %zmm0
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
	vmovups	%zmm0, 64(%rsp)
	vmovups	%zmm8, 128(%rsp)
	vmovups	%zmm10, 192(%rsp)
	# LOE rbx r12 r13 r14 r15 edx zmm10

	xorl	%eax, %eax
	# LOE rbx r12 r13 r14 r15 eax edx

	vzeroupper
	movq	%r12, 16(%rsp)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
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
	cmpl	$16, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	vmovups	192(%rsp), %zmm10

	/* Go to exit */
	jmp	L(EXIT)
	/*  DW_CFA_expression: r12 (r12) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -240; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0c, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x10, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r13 (r13) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -248; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0d, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x08, 0xff, 0xff, 0xff, 0x22
	/*  DW_CFA_expression: r14 (r14) (DW_OP_lit8; DW_OP_minus; DW_OP_const4s: -64; DW_OP_and; DW_OP_const4s: -256; DW_OP_plus)  */
	.cfi_escape 0x10, 0x0e, 0x0e, 0x38, 0x1c, 0x0d, 0xc0, 0xff, 0xff, 0xff, 0x1a, 0x0d, 0x00, 0xff, 0xff, 0xff, 0x22
	# LOE rbx r12 r13 r14 r15 zmm10

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	vmovss	64(%rsp, %r14, 4), %xmm0
	vmovss	128(%rsp, %r14, 4), %xmm1
	call	atan2f@PLT
	# LOE rbx r14 r15 r12d r13d xmm0

	vmovss	%xmm0, 192(%rsp, %r14, 4)

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
	vmovups	__svml_satan2_data_internal(%rip), %zmm9

	/* Check if both X & Y are not NaNs:  iXYnotNAN */
	vcmpps	$3, {sae}, %zmm8, %zmm8, %k1
	vcmpps	$3, {sae}, %zmm0, %zmm0, %k2
	vpcmpd	$4, %zmm9, %zmm2, %k3
	vpcmpd	$4, %zmm9, %zmm1, %k4

	/*
	 *  Path for zero arguments (at least one of both)
	 * Check if both args are zeros (den. is zero)
	 */
	vcmpps	$4, {sae}, %zmm9, %zmm5, %k5

	/* Res = sign(Y)*(X<0)?(PIO2+PI):PIO2 */
	vpcmpgtd %zmm8, %zmm9, %k6
	vpternlogd $255, %zmm14, %zmm14, %zmm14
	vpternlogd $255, %zmm12, %zmm12, %zmm12
	vpternlogd $255, %zmm13, %zmm13, %zmm13
	vpandnd	%zmm2, %zmm2, %zmm14{%k3}
	vpternlogd $255, %zmm2, %zmm2, %zmm2
	vpandnd	%zmm1, %zmm1, %zmm2{%k4}
	vpord	%zmm2, %zmm14, %zmm15
	vpternlogd $255, %zmm2, %zmm2, %zmm2
	vpandnd	%zmm5, %zmm5, %zmm2{%k5}

	/* Set sPIO2 to zero if den. is zero */
	vpandnd	%zmm4, %zmm2, %zmm4
	vpandd	%zmm2, %zmm9, %zmm5
	vpord	%zmm5, %zmm4, %zmm2
	vorps	%zmm7, %zmm2, %zmm7
	vaddps	{rn-sae}, %zmm11, %zmm7, %zmm7{%k6}
	vorps	%zmm6, %zmm7, %zmm6
	vpandnd	%zmm8, %zmm8, %zmm12{%k1}
	vpandnd	%zmm0, %zmm0, %zmm13{%k2}
	vandps	%zmm13, %zmm12, %zmm12

	/* Check if at least on of Y or Y is zero and not NaN: iAXAYZEROnotNAN */
	vpandd	%zmm12, %zmm15, %zmm1

	/* Exclude from previous callout mask zero (and not NaN) arguments */
	vpandnd	%zmm3, %zmm1, %zmm3

	/* Go to callout */
	vptestmd %zmm3, %zmm3, %k0
	kmovw	%k0, %edx

	/* Merge results from main and spec path */
	vpandnd	%zmm10, %zmm1, %zmm10
	vpandd	%zmm1, %zmm6, %zmm11
	vpord	%zmm11, %zmm10, %zmm10

	/* Return to main vector processing path */
	jmp	L(AUX_BRANCH_RETURN)
	# LOE rbx r12 r13 r14 r15 edx zmm0 zmm8 zmm10
END(_ZGVeN16vv_atan2f_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_satan2_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 sZERO[16][1];
	__declspec(align(64)) VUINT32 sONE[16][1];
	__declspec(align(64)) VUINT32 sSIGN_MASK[16][1];
	__declspec(align(64)) VUINT32 sABS_MASK[16][1];
	__declspec(align(64)) VUINT32 sPIO2[16][1];
	__declspec(align(64)) VUINT32 sPI[16][1];
	__declspec(align(64)) VUINT32 sPC8[16][1];
	__declspec(align(64)) VUINT32 sPC7[16][1];
	__declspec(align(64)) VUINT32 sPC6[16][1];
	__declspec(align(64)) VUINT32 sPC5[16][1];
	__declspec(align(64)) VUINT32 sPC4[16][1];
	__declspec(align(64)) VUINT32 sPC3[16][1];
	__declspec(align(64)) VUINT32 sPC2[16][1];
	__declspec(align(64)) VUINT32 sPC1[16][1];
	__declspec(align(64)) VUINT32 sPC0[16][1];
	__declspec(align(64)) VUINT32 iCHK_WORK_SUB[16][1];
	__declspec(align(64)) VUINT32 iCHK_WORK_CMP[16][1];
} __svml_satan2_data_internal;
#endif
__svml_satan2_data_internal:
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 // sZERO
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 // sONE
	.align	64
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000 // sSIGN_MASK
	.align	64
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF // sABS_MASK
	.align	64
	.long	0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB // sPIO2
	.align	64
	.long	0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x40490FDB // sPI
	.align	64
	.long	0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0 // sA08
	.align	64
	.long	0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631 // sA07
	.align	64
	.long	0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384 // sA06
	.align	64
	.long	0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629 // sA05
	.align	64
	.long	0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474 // sA04
	.align	64
	.long	0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8 // sA03
	.align	64
	.long	0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F // sA02
	.align	64
	.long	0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49 // sA01
	.align	64
	.long	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000 // sA00
	.align	64
	.long	0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000, 0x81000000 // iCHK_WORK_SUB
	.align	64
	.long	0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000, 0xFC000000 // iCHK_WORK_CMP
	.align	64
	.type	__svml_satan2_data_internal, @object
	.size	__svml_satan2_data_internal, .-__svml_satan2_data_internal
