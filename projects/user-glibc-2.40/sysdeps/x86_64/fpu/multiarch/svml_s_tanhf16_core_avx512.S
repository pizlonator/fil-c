/* Function tanhf vectorized with AVX-512.
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
 *   NOTE: Since the hyperbolic tangent function is odd
 *         (tanh(x) = -tanh(-x)), below algorithm deals with the absolute
 *         value of the argument |x|: tanh(x) = sign(x) * tanh(|x|)
 *
 *   We use a table lookup method to compute tanh(|x|).
 *   The basic idea is to split the input range into a number of subintervals
 *   and to approximate tanh(.) with a polynomial on each of them.
 *
 *   IEEE SPECIAL CONDITIONS:
 *   x = [+, -]0, r = [+, -]0
 *   x = +Inf,   r = +1
 *   x = -Inf,   r = -1
 *   x = QNaN,   r = QNaN
 *   x = SNaN,   r = QNaN
 *
 *
 *   ALGORITHM DETAILS
 *   We handle special values in a callout function, aside from main path
 *   computations. "Special" for this algorithm are:
 *   INF, NAN, |x| > HUGE_THRESHOLD
 *
 *
 *   Main path computations are organized as follows:
 *   Actually we split the interval [0, SATURATION_THRESHOLD)
 *   into a number of subintervals.  On each subinterval we approximate tanh(.)
 *   with a minimax polynomial of pre-defined degree. Polynomial coefficients
 *   are computed beforehand and stored in table. We also use
 *
 *       y := |x| + B,
 *
 *   here B depends on subinterval and is used to make argument
 *   closer to zero.
 *   We also add large fake interval [SATURATION_THRESHOLD, HUGE_THRESHOLD],
 *   where 1.0 + 0.0*y + 0.0*y^2 ... coefficients are stored - just to
 *   preserve main path computation logic but return 1.0 for all arguments.
 *
 *   Hence reconstruction looks as follows:
 *   we extract proper polynomial and range reduction coefficients
 *        (Pj and B), corresponding to subinterval, to which |x| belongs,
 *        and return
 *
 *       r := sign(x) * (P0 + P1 * y + ... + Pn * y^n)
 *
 *   NOTE: we use multiprecision technique to multiply and sum the first
 *         K terms of the polynomial. So Pj, j = 0..K are stored in
 *         table each as a pair of target precision numbers (Pj and PLj) to
 *         achieve wider than target precision.
 *
 *
 */

/* Offsets for data table __svml_stanh_data_internal_avx512. Ordered
   by use in the function. On cold-starts this might help the
   prefetcher. Possibly a better idea is to interleave start/end so
   that the prefetcher is less likely to detect a stream and pull
   irrelivant lines into cache.  */

/* Offsets for data table __svml_stanh_data_internal. 4 bytes each.
 */
#define _iExpMantMask_UISA		0
#define _iMinIdxOfsMask_UISA		4
#define _iMaxIdxMask_UISA		8
#define _iExpMask			12

/* Offsets for data table __svml_stanh_data_internal_al64. 64 bytes
   each.  */
#define _sC_lo				0
#define _sC_hi				64
#define _sP7_lo				128
#define _sP7_hi				192
#define _sSignMask			256
#define _sP6_lo				320
#define _sP6_hi				384
#define _sP5_lo				448
#define _sP5_hi				512
#define _sP4_lo				576
#define _sP4_hi				640
#define _sP3_lo				704
#define _sP3_hi				768
#define _sP2_lo				832
#define _sP2_hi				896
#define _sP0_lo				960
#define _sP0_hi				1024

#include <sysdep.h>
#define TANHF_DATA(x)			((x)+__svml_stanh_data_internal_al64)
#define TANHF_DATA_UNALIGNED(x)		((x)+__svml_stanh_data_internal)

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_tanhf_skx)
	/* Here huge arguments, INF and NaNs are filtered out to callout. */
	vpandd	TANHF_DATA_UNALIGNED(_iExpMantMask_UISA)(%rip){1to16}, %zmm0, %zmm1
	vpsubd	TANHF_DATA_UNALIGNED(_iMinIdxOfsMask_UISA)(%rip){1to16}, %zmm1, %zmm2

	/* Selection arguments between [0, 0x03e00000] into zmm3.  */
	vpxord	%zmm3, %zmm3, %zmm3
	vpmaxsd	%zmm3, %zmm2, %zmm3
	vpminsd	TANHF_DATA_UNALIGNED(_iMaxIdxMask_UISA)(%rip){1to16}, %zmm3, %zmm3

	/* Setup permute indices in zmm3.  */
	vpsrld	$21, %zmm3, %zmm3

	/* Store if there are any special cases in k1.  */
	vpcmpd	$6, TANHF_DATA_UNALIGNED(_iExpMask)(%rip){1to16}, %zmm1, %k1

	vmovaps	TANHF_DATA(_sC_lo)(%rip), %zmm5
	vpermt2ps TANHF_DATA(_sC_hi)(%rip), %zmm3, %zmm5

	vmovaps	TANHF_DATA(_sP7_lo)(%rip), %zmm2
	vpermt2ps TANHF_DATA(_sP7_hi)(%rip), %zmm3, %zmm2

	/* Store absolute values of inputs in zmm1.  */
	vmovaps	TANHF_DATA(_sSignMask)(%rip), %zmm4
	vandnps	%zmm0, %zmm4, %zmm1
	vsubps	{rn-sae}, %zmm5, %zmm1, %zmm1

	vmovaps	TANHF_DATA(_sP6_lo)(%rip), %zmm5
	vpermt2ps TANHF_DATA(_sP6_hi)(%rip), %zmm3, %zmm5

	vmovaps	TANHF_DATA(_sP5_lo)(%rip), %zmm6
	vpermt2ps TANHF_DATA(_sP5_hi)(%rip), %zmm3, %zmm6

	vfmadd213ps {rn-sae}, %zmm5, %zmm1, %zmm2
	vfmadd213ps {rn-sae}, %zmm6, %zmm1, %zmm2

	vmovaps	TANHF_DATA(_sP4_lo)(%rip), %zmm7
	vpermt2ps TANHF_DATA(_sP4_hi)(%rip), %zmm3, %zmm7

	vmovaps	TANHF_DATA(_sP3_lo)(%rip), %zmm8
	vpermt2ps TANHF_DATA(_sP3_hi)(%rip), %zmm3, %zmm8

	vfmadd213ps {rn-sae}, %zmm7, %zmm1, %zmm2
	vfmadd213ps {rn-sae}, %zmm8, %zmm1, %zmm2

	vmovaps	TANHF_DATA(_sP2_lo)(%rip), %zmm9
	vpermt2ps TANHF_DATA(_sP2_hi)(%rip), %zmm3, %zmm9

	vmovaps	TANHF_DATA(_sP0_lo)(%rip), %zmm10
	vpermt2ps TANHF_DATA(_sP0_hi)(%rip), %zmm3, %zmm10

	vfmadd213ps {rn-sae}, %zmm9, %zmm1, %zmm2
	vfmadd213ps {rn-sae}, %zmm10, %zmm1, %zmm2

	kmovw	%k1, %edx
	testl	%edx, %edx

	/* Go to special inputs processing branch.  */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx r12 r13 r14 r15 zmm0 zmm2 zmm4
	/* Wait until after branch of write over zmm0.  */
	vpternlogd $0xec, %zmm4, %zmm2, %zmm0

	/* No stack restoration on the fastpath.  */
	ret

	/* Cold case. edx has 1s where there was a special value that
	   needs to be handled by a tanhf call. Optimize for code size
	   more so than speed here. */
L(SPECIAL_VALUES_BRANCH):
	# LOE rbx rdx r12 r13 r14 r15 zmm0 zmm2 zmm4
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

	/* Save original input (zmm0 unchanged up to this point).  */
	vmovaps	%zmm0, 64(%rsp)
	/* Save all already computed inputs.  */
	vpternlogd $0xec, %zmm4, %zmm2, %zmm0
	vmovaps	%zmm0, (%rsp)

	vzeroupper

	/* edx has 1s where there was a special value that needs to be handled
	   by a tanhf call.  */
	movl	%edx, %ebx
L(SPECIAL_VALUES_LOOP):
	# LOE rbx rbp r12 r13 r14 r15
	/* use rbp as index for special value that is saved across calls to
	   tanhf. We technically don't need a callee save register here as offset
	   to rsp is always [0, 56] so we can restore rsp by realigning to 64.
	   Essentially the tradeoff is 1 extra save/restore vs 2 extra instructions
	   in the loop. Realigning also costs more code size.  */
	xorl	%ebp, %ebp
	tzcntl	%ebx, %ebp

	/* Scalar math function call to process special input.  */
	vmovss	64(%rsp, %rbp, 4), %xmm0
	call	tanhf@PLT

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
END(_ZGVeN16v_tanhf_skx)

	.section .rodata, "a"
	.align	16
#ifdef __svml_stanh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct
	{
	__declspec(align(4)) VUINT32 _iExpMantMask_UISA[1][1];
	__declspec(align(4)) VUINT32 _iMinIdxOfsMask_UISA[1][1];
	__declspec(align(4)) VUINT32 _iMaxIdxMask_UISA[1][1];
	__declspec(align(4)) VUINT32 _iExpMask[1][1];
	__declspec(align(64)) VUINT32 _sC_lo[16][1];
	__declspec(align(64)) VUINT32 _sC_hi[16][1];
	__declspec(align(64)) VUINT32 _sP7_lo[16][1];
	__declspec(align(64)) VUINT32 _sP7_hi[16][1];
	__declspec(align(64)) VUINT32 _sSignMask[16][1];
	__declspec(align(64)) VUINT32 _sP6_lo[16][1];
	__declspec(align(64)) VUINT32 _sP6_hi[16][1];
	__declspec(align(64)) VUINT32 _sP5_lo[16][1];
	__declspec(align(64)) VUINT32 _sP5_hi[16][1];
	__declspec(align(64)) VUINT32 _sP4_lo[16][1];
	__declspec(align(64)) VUINT32 _sP4_hi[16][1];
	__declspec(align(64)) VUINT32 _sP3_lo[16][1];
	__declspec(align(64)) VUINT32 _sP3_hi[16][1];
	__declspec(align(64)) VUINT32 _sP2_lo[16][1];
	__declspec(align(64)) VUINT32 _sP2_hi[16][1];
	__declspec(align(64)) VUINT32 _sP0_lo[16][1];
	__declspec(align(64)) VUINT32 _sP0_hi[16][1];
} __svml_stanh_data_internal;
#endif

__svml_stanh_data_internal:
	.align	4
	/* _iExpMantMask_UISA */
	.long	0x7fe00000

	.align	4
	/* _iMinIdxOfsMask_UISA */
	.long	0x3d400000

	.align	4
	/* _iMaxIdxMask_UISA */
	.long	0x03e00000

	.align	4
	/* _iExpMask */
	.long	0x7f000000

	.align	64
__svml_stanh_data_internal_al64:
	.align	64
	/* _sC_lo */
	.long	0x00000000, 0x3d700000, 0x3d900000, 0x3db00000
	.long	0x3dd00000, 0x3df00000, 0x3e100000, 0x3e300000
	.long	0x3e500000, 0x3e700000, 0x3e900000, 0x3eb00000
	.long	0x3ed00000, 0x3ef00000, 0x3f100000, 0x3f300000

	.align	64
	/* _sC_hi */
	.long	0x3f500000, 0x3f700000, 0x3f900000, 0x3fb00000
	.long	0x3fd00000, 0x3ff00000, 0x40100000, 0x40300000
	.long	0x40500000, 0x40700000, 0x40900000, 0x40b00000
	.long	0x40d00000, 0x40f00000, 0x41100000, 0x00000000

	.align	64
	/* _sP7_lo */
	.long	0xbc0e2f66, 0x460bda12, 0x43d638ef, 0xc3e11c3e
	.long	0xc2baa4e9, 0xc249da2d, 0xc1859b82, 0x40dd5b57
	.long	0x40494640, 0x40c730a8, 0xbf0f160e, 0x3e30e76f
	.long	0xbea81387, 0xbdb26a1c, 0xbd351e57, 0xbb4c01a0

	.align	64
	/* _sP7_hi */
	.long	0x3c1d7bfb, 0x3c722cd1, 0x3c973f1c, 0x3c33a31b
	.long	0x3b862ef4, 0x3a27b3d0, 0xba3b5907, 0xba0efc22
	.long	0xb97f9f0f, 0xb8c8af50, 0xb7bdddfb, 0xb64f2950
	.long	0xb4e085b1, 0xb3731dfa, 0xb15a1f04, 0x00000000

	.align	64
	/* _sSignMask */
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000

	.align	64
	/* _sP6_lo */
	.long	0x3e0910e9, 0x43761143, 0x4165ecdc, 0xc190f756
	.long	0xc08c097d, 0xc02ba813, 0xbf7f6bda, 0x3f2b1dc0
	.long	0x3ece105d, 0x3f426a94, 0xbadb0dc4, 0x3da43b17
	.long	0xbd51ab88, 0xbcaea23d, 0xbd3b6d8d, 0xbd6caaad

	.align	64
	/* _sP6_hi */
	.long	0xbd795bed, 0xbd5fddda, 0xbd038f3b, 0xbc1cad63
	.long	0x3abb4766, 0x3b95f10b, 0x3b825873, 0x3afaea66
	.long	0x3a49f878, 0x39996bf3, 0x388f3e6c, 0x371bb0e3
	.long	0x35a8a5e6, 0x34369b17, 0x322487b0, 0x00000000

	.align	64
	/* _sP5_lo */
	.long	0xb76dd6b9, 0xbe1c276d, 0x3c1dcf2f, 0x3dc1a78d
	.long	0x3d96f985, 0x3da2b61b, 0x3dc13397, 0x3dd2f670
	.long	0x3df48a0a, 0x3e06c5a8, 0x3e1a3aba, 0x3e27c405
	.long	0x3e2e78d0, 0x3e2c3e44, 0x3e1d3097, 0x3df4a8f4

	.align	64
	/* _sP5_hi */
	.long	0x3da38508, 0x3d31416a, 0x3b562657, 0xbcaeeac9
	.long	0xbcce9419, 0xbcaaeac4, 0xbc49e7d0, 0xbba71ddd
	.long	0xbb003b0e, 0xba3f9a05, 0xb92c08a7, 0xb7ba9232
	.long	0xb64a0b0f, 0xb4dac169, 0xb2ab78ac, 0x00000000

	.align	64
	/* _sP4_lo */
	.long	0xbeaaaaa5, 0xbeab0612, 0xbea7f01f, 0xbea4e120
	.long	0xbea387b7, 0xbea15962, 0xbe9d57f7, 0xbe976b5a
	.long	0xbe90230d, 0xbe880dff, 0xbe7479b3, 0xbe4c3d88
	.long	0xbe212482, 0xbdeb8cba, 0xbd5e78ad, 0x3c6b5e6e

	.align	64
	/* _sP4_hi */
	.long	0x3d839143, 0x3dc21ee1, 0x3de347af, 0x3dcbec96
	.long	0x3d99ef2d, 0x3d542ea1, 0x3cdde701, 0x3c2cca67
	.long	0x3b81cb27, 0x3ac073a1, 0x39ac3032, 0x383a94d9
	.long	0x36ca081d, 0x355abd4c, 0x332b3cb6, 0x00000000

	.align	64
	/* _sP3_lo */
	.long	0xb0343c7b, 0xbd6ee69d, 0xbd8f0da7, 0xbdae477d
	.long	0xbdcd2a1f, 0xbdeba80d, 0xbe0c443b, 0xbe293cf3
	.long	0xbe44f282, 0xbe5f3651, 0xbe81c7c0, 0xbe96d7ca
	.long	0xbea7fb8e, 0xbeb50e9e, 0xbec12efe, 0xbec4be92

	.align	64
	/* _sP3_hi */
	.long	0xbebce070, 0xbead510e, 0xbe8ef7d6, 0xbe4b8704
	.long	0xbe083237, 0xbdaf7449, 0xbd2e1ec4, 0xbc83bf06
	.long	0xbbc3e0b5, 0xbb10aadc, 0xba0157db, 0xb88c18f2
	.long	0xb717b096, 0xb5a43bae, 0xb383012c, 0x00000000

	.align	64
	/* _sP2_lo */
	.long	0x3f800000, 0x3f7f1f84, 0x3f7ebd11, 0x3f7e1e5f
	.long	0x3f7d609f, 0x3f7c842d, 0x3f7b00e5, 0x3f789580
	.long	0x3f75b8ad, 0x3f726fd9, 0x3f6cc59b, 0x3f63fb92
	.long	0x3f59ff97, 0x3f4f11d7, 0x3f3d7573, 0x3f24f360

	.align	64
	/* _sP2_hi */
	.long	0x3f0cbfe7, 0x3eec1a69, 0x3eb0a801, 0x3e6753a2
	.long	0x3e132f1a, 0x3db7e7d3, 0x3d320845, 0x3c84d3d4
	.long	0x3bc477b7, 0x3b10d3da, 0x3a01601e, 0x388c1a3b
	.long	0x3717b0da, 0x35a43bce, 0x338306c6, 0x00000000

	.align	64
	/* _sP0_lo */
	.long	0x00000000, 0x3d6fb9c9, 0x3d8fc35f, 0x3daf9169
	.long	0x3dcf49ab, 0x3deee849, 0x3e0f0ee8, 0x3e2e4984
	.long	0x3e4d2f8e, 0x3e6bb32e, 0x3e8c51cd, 0x3ea96163
	.long	0x3ec543f1, 0x3edfd735, 0x3f028438, 0x3f18abf0

	.align	64
	/* _sP0_hi */
	.long	0x3f2bc480, 0x3f3bec1c, 0x3f4f2e5b, 0x3f613c53
	.long	0x3f6ce37d, 0x3f743c4f, 0x3f7a5feb, 0x3f7dea85
	.long	0x3f7f3b3d, 0x3f7fb78c, 0x3f7fefd4, 0x3f7ffdd0
	.long	0x3f7fffb4, 0x3f7ffff6, 0x3f7fffff, 0x3f800000

	.align	64
	.type	__svml_stanh_data_internal_al64, @object
	.size	__svml_stanh_data_internal_al64, .-__svml_stanh_data_internal_al64
	.type	__svml_stanh_data_internal, @object
	.size	__svml_stanh_data_internal, .-__svml_stanh_data_internal
