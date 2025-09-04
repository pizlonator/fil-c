/* Function atanhf vectorized with AVX-512.
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
 *   Compute atanh(x) as 0.5 * log((1 + x)/(1 - x))
 *   using small lookup table that map to AVX-512 permute instructions
 *
 *   Special cases:
 *
 *   atanh(0)  = 0
 *   atanh(+1) = +INF
 *   atanh(-1) = -INF
 *   atanh(x)  = NaN if |x| > 1, or if x is a NaN or INF
 *
 */

/* Offsets for data table __svml_satanh_data_internal_avx512 and
   __svml_satanh_data_internal_avx512_al64. Ordered by use in the
   function. On cold-starts this might help the prefetcher. Possibly
   a better idea is to interleave start/end so that the prefetcher is
   less likely to detect a stream and pull irrelivant lines into
   cache.  */

/* Offset into __svml_satanh_data_internal_avx512. 4-byte aligned as
   the memory is broadcast to {1to16}.  */
#define AbsMask				0

/* Offset into __svml_satanh_data_internal_avx512_al64. The full value
   is used here.  */
#define One				0
#define AddB5				64
#define RcpBitMask			128
#define Log_tbl_L_lo			192
#define Log_tbl_L_hi			256
#define Log_tbl_H_lo			320
#define Log_tbl_H_hi			384
#define L2H				448
#define L2L				512
#define poly_coeff3			576
#define poly_coeff2			640
#define poly_coeff1			704

#include <sysdep.h>

#define ATANHF_DATA(x)			((x)+__svml_satanh_data_internal_avx512_al64)

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_atanhf_skx)
	vandps	AbsMask+__svml_satanh_data_internal_avx512(%rip){1to16}, %zmm0, %zmm6
	vmovups	ATANHF_DATA(One)(%rip), %zmm4

	/* 1+y */
	vaddps	{rn-sae}, %zmm4, %zmm6, %zmm9

	/* 1-y */
	vsubps	{rn-sae}, %zmm6, %zmm4, %zmm8

	/* round reciprocals to 1+5b mantissas */
	vmovups	ATANHF_DATA(AddB5)(%rip), %zmm14
	vmovups	ATANHF_DATA(RcpBitMask)(%rip), %zmm1

	/* RcpP ~ 1/Yp */
	vrcp14ps %zmm9, %zmm12

	/* RcpM ~ 1/Ym */
	vrcp14ps %zmm8, %zmm13

	/* Yp_high */
	vsubps	{rn-sae}, %zmm4, %zmm9, %zmm2

	/* -Ym_high */
	vsubps	{rn-sae}, %zmm4, %zmm8, %zmm5


	/* input outside (-1, 1) ? */
	vpaddd	%zmm14, %zmm12, %zmm15
	vpaddd	%zmm14, %zmm13, %zmm12

	/* Yp_low */
	vsubps	{rn-sae}, %zmm2, %zmm6, %zmm3
	vandps	%zmm1, %zmm15, %zmm7
	vandps	%zmm1, %zmm12, %zmm12

	/* Ym_low */
	vaddps	{rn-sae}, %zmm5, %zmm6, %zmm5

	/* Reduced argument: Rp = (RcpP*Yp - 1)+RcpP*Yp_low */
	vfmsub213ps {rn-sae}, %zmm4, %zmm7, %zmm9

	/* Reduced argument: Rm = (RcpM*Ym - 1)+RcpM*Ym_low */
	vfmsub213ps {rn-sae}, %zmm4, %zmm12, %zmm8

	vmovups	ATANHF_DATA(Log_tbl_L_lo)(%rip), %zmm10
	vmovups	ATANHF_DATA(Log_tbl_L_hi)(%rip), %zmm13

	/* exponents */
	vfmadd231ps {rn-sae}, %zmm7, %zmm3, %zmm9
	vgetexpps {sae}, %zmm7, %zmm15


	/* Table lookups */
	vfnmadd231ps {rn-sae}, %zmm12, %zmm5, %zmm8
	vgetexpps {sae}, %zmm12, %zmm14


	/* Prepare table index */
	vpsrld	$18, %zmm7, %zmm3
	vpsrld	$18, %zmm12, %zmm2
	vmovups	ATANHF_DATA(Log_tbl_H_lo)(%rip), %zmm11
	vmovups	ATANHF_DATA(Log_tbl_H_hi)(%rip), %zmm7
	/* Km-Kp */

	vmovaps	%zmm3, %zmm5
	vpermi2ps %zmm13, %zmm10, %zmm3
	vpermt2ps %zmm13, %zmm2, %zmm10
	vpermi2ps %zmm7, %zmm11, %zmm5
	vpermt2ps %zmm7, %zmm2, %zmm11
	vsubps	{rn-sae}, %zmm15, %zmm14, %zmm1
	vsubps	{rn-sae}, %zmm3, %zmm10, %zmm7

	/* K*L2H + Th */
	vmovups	ATANHF_DATA(L2H)(%rip), %zmm2

	/* K*L2L + Tl */
	vmovups	ATANHF_DATA(L2L)(%rip), %zmm3

	/* table values */
	vsubps	{rn-sae}, %zmm5, %zmm11, %zmm5
	vfmadd231ps {rn-sae}, %zmm1, %zmm2, %zmm5
	vfmadd213ps {rn-sae}, %zmm7, %zmm3, %zmm1
	/* polynomials */
	vmovups	ATANHF_DATA(poly_coeff3)(%rip), %zmm7
	vmovups	ATANHF_DATA(poly_coeff2)(%rip), %zmm10
	vmovaps	%zmm10, %zmm14
	vfmadd231ps {rn-sae}, %zmm9, %zmm7, %zmm10
	vfmadd231ps {rn-sae}, %zmm8, %zmm7, %zmm14
	vmovups	ATANHF_DATA(poly_coeff1)(%rip), %zmm12
	vfmadd213ps {rn-sae}, %zmm12, %zmm9, %zmm10
	vfmadd213ps {rn-sae}, %zmm12, %zmm8, %zmm14
	vfmadd213ps {rn-sae}, %zmm4, %zmm9, %zmm10
	vfmadd213ps {rn-sae}, %zmm4, %zmm8, %zmm14

	/* (K*L2L + Tl) + Rp*PolyP */
	vfmadd213ps {rn-sae}, %zmm1, %zmm9, %zmm10

	/* zmm12 = zmm12 & (zmm4 | zmm0).  */
	vpternlogq $0xe0, %zmm0, %zmm4, %zmm12

	/* (K*L2L + Tl) + Rp*PolyP -Rm*PolyM */
	vfnmadd213ps {rn-sae}, %zmm5, %zmm8, %zmm14
	vaddps	{rn-sae}, %zmm14, %zmm10, %zmm8

	vcmpps	$21, {sae}, %zmm4, %zmm6, %k0
	kmovw	%k0, %edx
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 zmm0 zmm8 zmm12
	vmulps	{rn-sae}, %zmm12, %zmm8, %zmm0

	/* No register to restore on fast path.  */
	ret

	/* Cold case. edx has 1s where there was a special value that
	   needs to be handled by a atanhf call. Optimize for code size
	   more so than speed here. */
L(SPECIAL_VALUES_BRANCH):
	# LOE rbx rdx r12 r13 r14 r15 zmm0 zmm8 zmm12
    /* Use r13 to save/restore the stack. This allows us to use rbp as
       callee save register saving code size. */
	pushq	%r13
	cfi_adjust_cfa_offset(8)
	cfi_offset(r13, -16)
	/* Need to callee save registers to preserve state across tanhf calls.
	 */
	pushq	%rbx
	cfi_adjust_cfa_offset(8)
	cfi_offset(rbx, -24)
	pushq	%rbp
	cfi_adjust_cfa_offset(8)
	cfi_offset(rbp, -32)
	movq	%rsp, %r13
	cfi_def_cfa_register(r13)

	/* Align stack and make room for 2x zmm vectors.  */
	andq	$-64, %rsp
	addq	$-128, %rsp
	vmulps	{rn-sae}, %zmm12, %zmm8, %zmm1
	vmovaps	%zmm1, (%rsp)
	vmovaps	%zmm0, 64(%rsp)
	vzeroupper

	/* edx has 1s where there was a special value that needs to be handled
	   by a atanhf call.  */
	movl	%edx, %ebx
L(SPECIAL_VALUES_LOOP):
	# LOE rbx rbp r12 r13 r14 r15
	/* use rbp as index for special value that is saved across calls to
	   atanhf. We technically don't need a callee save register here as offset
	   to rsp is always [0, 56] so we can restore rsp by realigning to 64.
	   Essentially the tradeoff is 1 extra save/restore vs 2 extra instructions
	   in the loop. Realigning also costs more code size.  */
	xorl	%ebp, %ebp
	tzcntl	%ebx, %ebp

	/* Scalar math function call to process special input.  */
	vmovss	64(%rsp, %rbp, 4), %xmm0
	call	atanhf@PLT

	/* No good way to avoid the store-forwarding fault this will cause on
	   return. `lfence` avoids the SF fault but at greater cost as it
	   serialized stack/callee save restoration.  */
	vmovss	%xmm0, (%rsp, %rbp, 4)

	blsrl   %ebx, %ebx
	jnz	L(SPECIAL_VALUES_LOOP)
	# LOE r12 r13 r14 r15

	/* All results have been written to (%rsp).  */
	vmovaps	(%rsp), %zmm0
	/* Restore rsp.  */
	movq	%r13, %rsp
	cfi_def_cfa_register(rsp)
	/* Restore callee save registers.  */
	popq	%rbp
	cfi_adjust_cfa_offset(-8)
	cfi_restore(rbp)
	popq	%rbx
	cfi_adjust_cfa_offset(-8)
	cfi_restore(rbp)
	popq	%r13
	cfi_adjust_cfa_offset(-8)
	cfi_restore(r13)
	ret
END(_ZGVeN16v_atanhf_skx)

	.section .rodata, "a"
	.align	4
#ifdef __svml_satanh_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct{
	__declspec(align(4)) VUINT32 AbsMask[1][1];
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 AddB5[16][1];
	__declspec(align(64)) VUINT32 RcpBitMask[16][1];
	__declspec(align(64)) VUINT32 Log_tbl_L_lo[16][1];
	__declspec(align(64)) VUINT32 Log_tbl_L_hi[16][1];
	__declspec(align(64)) VUINT32 Log_tbl_H_lo[16][1];
	__declspec(align(64)) VUINT32 Log_tbl_H_hi[16][1];
	__declspec(align(64)) VUINT32 L2H[16][1];
	__declspec(align(64)) VUINT32 L2L[16][1];
	__declspec(align(64)) VUINT32 poly_coeff3[16][1];
	__declspec(align(64)) VUINT32 poly_coeff2[16][1];
	__declspec(align(64)) VUINT32 poly_coeff1[16][1];
} __svml_satanh_data_internal_avx512;
#endif
__svml_satanh_data_internal_avx512:
	/* Leave this at front so we can potentially save space due to
	   smaller alignment constraint.  */
	.align	4
    /* AbsMask */
	.long	0x7fffffff
	.align	64
__svml_satanh_data_internal_avx512_al64:
	/* One */
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* AddB5 */
	.align	64
	.long	0x00020000, 0x00020000, 0x00020000, 0x00020000
	.long	0x00020000, 0x00020000, 0x00020000, 0x00020000
	.long	0x00020000, 0x00020000, 0x00020000, 0x00020000
	.long	0x00020000, 0x00020000, 0x00020000, 0x00020000
	/* RcpBitMask */
	.align	64
	.long	0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000
	.long	0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000
	.long	0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000
	.long	0xfffc0000, 0xfffc0000, 0xfffc0000, 0xfffc0000
	/* Log_tbl_L_lo */
	.align	64
	.long	0x00000000
	.long	0x3726c39e
	.long	0x38a30c01
	.long	0x37528ae5
	.long	0x38e0edc5
	.long	0xb8ab41f8
	.long	0xb7cf8f58
	.long	0x3896a73d
	.long	0xb5838656
	.long	0x380c36af
	.long	0xb8235454
	.long	0x3862bae1
	.long	0x38c5e10e
	.long	0x38dedfac
	.long	0x38ebfb5e
	.long	0xb8e63c9f
	/* Log_tbl_L_hi */
	.align	64
	.long	0xb85c1340
	.long	0x38777bcd
	.long	0xb6038656
	.long	0x37d40984
	.long	0xb8b85028
	.long	0xb8ad5a5a
	.long	0x3865c84a
	.long	0x38c3d2f5
	.long	0x383ebce1
	.long	0xb8a1ed76
	.long	0xb7a332c4
	.long	0xb779654f
	.long	0xb8602f73
	.long	0x38f85db0
	.long	0x37b4996f
	.long	0xb8bfb3ca
	/* Log_tbl_H_lo */
	.align	64
	.long	0x00000000
	.long	0x3cfc0000
	.long	0x3d780000
	.long	0x3db78000
	.long	0x3df10000
	.long	0x3e14c000
	.long	0x3e300000
	.long	0x3e4a8000
	.long	0x3e648000
	.long	0x3e7dc000
	.long	0x3e8b4000
	.long	0x3e974000
	.long	0x3ea30000
	.long	0x3eae8000
	.long	0x3eb9c000
	.long	0x3ec4e000
	/* Log_tbl_H_hi */
	.align	64
	.long	0x3ecfa000
	.long	0x3eda2000
	.long	0x3ee48000
	.long	0x3eeea000
	.long	0x3ef8a000
	.long	0x3f013000
	.long	0x3f05f000
	.long	0x3f0aa000
	.long	0x3f0f4000
	.long	0x3f13d000
	.long	0x3f184000
	.long	0x3f1ca000
	.long	0x3f20f000
	.long	0x3f252000
	.long	0x3f295000
	.long	0x3f2d7000
	/* L2H = log(2)_high */
	.align	64
	.long	0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000
	.long	0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000
	.long	0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000
	.long	0x3f317000, 0x3f317000, 0x3f317000, 0x3f317000
	/* L2L = log(2)_low */
	.align	64
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4
	.long	0x3805fdf4, 0x3805fdf4, 0x3805fdf4, 0x3805fdf4
	/* poly_coeff3 */
	.align	64
	.long	0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810
	.long	0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810
	.long	0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810
	.long	0xbe800810, 0xbe800810, 0xbe800810, 0xbe800810
	/* poly_coeff2 */
	.align	64
	.long	0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e
	.long	0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e
	.long	0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e
	.long	0x3eaab11e, 0x3eaab11e, 0x3eaab11e, 0x3eaab11e
	/* poly_coeff1 */
	.align	64
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000
	.align	64
	.type	__svml_satanh_data_internal_avx512_al64, @object
	.size	__svml_satanh_data_internal_avx512_al64, .-__svml_satanh_data_internal_avx512_al64
	.type	__svml_satanh_data_internal_avx512, @object
	.size	__svml_satanh_data_internal_avx512, .-__svml_satanh_data_internal_avx512
