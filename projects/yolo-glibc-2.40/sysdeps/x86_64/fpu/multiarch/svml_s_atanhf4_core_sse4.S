/* Function atanhf vectorized with SSE4.
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
 *
 *   Special cases:
 *
 *   atanh(0)  = 0
 *   atanh(+1) = +INF
 *   atanh(-1) = -INF
 *   atanh(x)  = NaN if |x| > 1, or if x is a NaN or INF
 *
 */

/* Offsets for data table __svml_satanh_data_internal_avx512. Ordered
   by use in the function. On cold-starts this might help the
   prefetcher. Possibly a better idea is to interleave start/end so
   that the prefetcher is less likely to detect a stream and pull
   irrelivant lines into cache.  */
#define sOne				0
#define SgnMask				16
#define sTopMask12			32
#define iBrkValue			48
#define iOffExpoMask			64
#define sPoly				80
#define sLn2				208
#define TinyRange			224

#include <sysdep.h>
#define ATANHF_DATA(x)			((x)+__svml_satanh_data_internal)

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_atanhf_sse4)
	movaps	%xmm0, %xmm5

	/* Load constants including One = 1 */
	movups	ATANHF_DATA(sOne)(%rip), %xmm4
	movaps	%xmm5, %xmm3

	/* Strip off the sign, so treat X as positive until right at the end */
	movups	ATANHF_DATA(SgnMask)(%rip), %xmm1
	movaps	%xmm4, %xmm2
	andps	%xmm1, %xmm0
	movaps	%xmm4, %xmm10
	movups	ATANHF_DATA(sTopMask12)(%rip), %xmm11
	movaps	%xmm4, %xmm14
	movaps	%xmm11, %xmm9


	/*
	 * Compute V = 2 * X trivially, and UHi + U_lo = 1 - X in two pieces,
	 * the upper part UHi being <= 12 bits long. Then we have
	 * atanh(X) = 1/2 * log((1 + X) / (1 - X)) = 1/2 * log1p(V / (UHi + ULo)).
	 */
	movaps	%xmm0, %xmm6
	mulps	%xmm5, %xmm3
	subps	%xmm0, %xmm2
	addps	%xmm0, %xmm6
	subps	%xmm2, %xmm10
	addps	%xmm5, %xmm3
	subps	%xmm0, %xmm10
	andps	%xmm2, %xmm9


	/*
	 * Check whether |X| < 1, in which case we use the main function.
	 * Otherwise set the rangemask so that the callout will get used.
	 * Note that this will also use the callout for NaNs since not(NaN < 1).
	 */
	rcpps	%xmm9, %xmm7
	subps	%xmm9, %xmm2
	andps	%xmm11, %xmm7


	/*
	 * Split V as well into upper 12 bits and lower part, so that we can get
	 * a preliminary quotient estimate without rounding error.
	 */
	andps	%xmm6, %xmm11
	mulps	%xmm7, %xmm9
	addps	%xmm2, %xmm10
	subps	%xmm11, %xmm6

	/* Hence get initial quotient estimate QHi + QLo = R * VHi + R * VLo */
	mulps	%xmm7, %xmm11
	mulps	%xmm7, %xmm10
	subps	%xmm9, %xmm14
	mulps	%xmm6, %xmm7
	subps	%xmm10, %xmm14

	/* Compute D = E + E^2 */
	movaps	%xmm14, %xmm13
	movaps	%xmm4, %xmm8
	mulps	%xmm14, %xmm13

	/* reduction: compute r,n */
	movdqu	ATANHF_DATA(iBrkValue)(%rip), %xmm9
	addps	%xmm13, %xmm14

	/*
	 * Compute R * (VHi + VLo) * (1 + E + E^2)
	 * = R *  (VHi + VLo) * (1 + D)
	 * = QHi + (QHi * D + QLo + QLo * D)
	 */
	movaps	%xmm14, %xmm2
	mulps	%xmm7, %xmm14
	mulps	%xmm11, %xmm2
	addps	%xmm14, %xmm7
	movdqu	ATANHF_DATA(iOffExpoMask)(%rip), %xmm12
	movaps	%xmm4, %xmm14

	/* Record the sign for eventual reincorporation. */
	addps	%xmm7, %xmm2


	/*
	 * Now finally accumulate the high and low parts of the
	 * argument to log1p, H + L, with a final compensated summation.
	 */
	movaps	%xmm2, %xmm6
	andnps	%xmm5, %xmm1
	movaps	%xmm4, %xmm7
	/* Or the sign bit in with the tiny result to handle atanh(-0) correctly */
	addps	%xmm11, %xmm6
	maxps	%xmm6, %xmm7
	minps	%xmm6, %xmm8
	subps	%xmm6, %xmm11
	movaps	%xmm7, %xmm10
	addps	%xmm8, %xmm10
	addps	%xmm11, %xmm2
	subps	%xmm10, %xmm7
	psubd	%xmm9, %xmm10
	addps	%xmm8, %xmm7
	pand	%xmm10, %xmm12
	psrad	$23, %xmm10
	cvtdq2ps %xmm10, %xmm13
	addps	%xmm7, %xmm2

	/* final reconstruction */
	pslld	$23, %xmm10
	paddd	%xmm9, %xmm12
	psubd	%xmm10, %xmm14

	/* polynomial evaluation */
	subps	%xmm4, %xmm12
	mulps	%xmm14, %xmm2
	movups	ATANHF_DATA(sPoly+0)(%rip), %xmm7
	addps	%xmm12, %xmm2
	mulps	%xmm2, %xmm7


	/* Finally, halve the result and reincorporate the sign */
	addps	ATANHF_DATA(sPoly+16)(%rip), %xmm7
	mulps	%xmm2, %xmm7
	addps	ATANHF_DATA(sPoly+32)(%rip), %xmm7
	mulps	%xmm2, %xmm7
	addps	ATANHF_DATA(sPoly+48)(%rip), %xmm7
	mulps	%xmm2, %xmm7
	addps	ATANHF_DATA(sPoly+64)(%rip), %xmm7
	mulps	%xmm2, %xmm7
	addps	ATANHF_DATA(sPoly+80)(%rip), %xmm7
	mulps	%xmm2, %xmm7
	addps	ATANHF_DATA(sPoly+96)(%rip), %xmm7
	mulps	%xmm2, %xmm7
	movaps	ATANHF_DATA(sPoly+112)(%rip), %xmm6
	addps	%xmm6, %xmm7
	mulps	%xmm2, %xmm7
	mulps	%xmm2, %xmm7
	mulps	ATANHF_DATA(sLn2)(%rip), %xmm13
	/* We can build `sHalf` with `sPoly & sOne`.  */
	andps	%xmm4, %xmm6
	orps	%xmm1, %xmm3
	xorps	%xmm6, %xmm1

	addps	%xmm2, %xmm7
	addps	%xmm13, %xmm7
	mulps	%xmm7, %xmm1

	/* Finish check of NaNs.  */
	cmpleps	%xmm0, %xmm4
	movmskps %xmm4, %edx
	cmpltps	ATANHF_DATA(TinyRange)(%rip), %xmm0

	andps	%xmm0, %xmm3
	andnps	%xmm1, %xmm0
	orps	%xmm3, %xmm0

	testl	%edx, %edx
	/* Go to special inputs processing branch.  */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 xmm0
	/* No registers to restore on fast path.  */
	ret


	/* Cold case. edx has 1s where there was a special value that
	   needs to be handled by a atanhf call. Optimize for code size
	   more so than speed here. */
L(SPECIAL_VALUES_BRANCH):
	# LOE rbx rdx rbp r12 r13 r14 r15 xmm0 xmm5
	/* Stack coming in 16-byte aligned. Set 8-byte misaligned so on
       call entry will be 16-byte aligned. */
	subq	$56, %rsp
	cfi_def_cfa_offset(64)
	movups	%xmm0, 24(%rsp)
	movups	%xmm5, 40(%rsp)

	/* Use rbx/rbp for callee save registers as they get short
       encoding for many instructions (as compared with r12/r13). */
	movq	%rbx, (%rsp)
	cfi_offset(rbx, -64)
	movq	%rbp, 8(%rsp)
	cfi_offset(rbp, -56)
	/* edx has 1s where there was a special value that needs to be handled
	   by a tanhf call.  */
	movl	%edx, %ebx
L(SPECIAL_VALUES_LOOP):
	# LOE rbx rbp r12 r13 r14 r15
	/* use rbp as index for special value that is saved across calls to
	   tanhf. We technically don't need a callee save register here as offset
	   to rsp is always [0, 12] so we can restore rsp by realigning to 64.
	   Essentially the tradeoff is 1 extra save/restore vs 2 extra instructions
	   in the loop.  */
	xorl	%ebp, %ebp
	bsfl	%ebx, %ebp

	/* Scalar math function call to process special input.  */
	movss	40(%rsp, %rbp, 4), %xmm0
	call	atanhf@PLT
	/* No good way to avoid the store-forwarding fault this will cause on
	   return. `lfence` avoids the SF fault but at greater cost as it
	   serialized stack/callee save restoration.  */
	movss	%xmm0, 24(%rsp, %rbp, 4)

	leal	-1(%rbx), %eax
	andl	%eax, %ebx
	jnz	L(SPECIAL_VALUES_LOOP)
	# LOE r12 r13 r14 r15
	/* All results have been written to 24(%rsp).  */
	movups	24(%rsp), %xmm0
	movq	(%rsp), %rbx
	cfi_restore(rbx)
	movq	8(%rsp), %rbp
	cfi_restore(rbp)
	addq	$56, %rsp
	cfi_def_cfa_offset(8)
	ret
END(_ZGVbN4v_atanhf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_satanh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct{
	__declspec(align(16)) VUINT32 sOne[4][1];
	__declspec(align(16)) VUINT32 SgnMask[4][1];
	__declspec(align(16)) VUINT32 sTopMask12[4][1];
	__declspec(align(16)) VUINT32 iBrkValue[4][1];
	__declspec(align(16)) VUINT32 iOffExpoMask[4][1];
	__declspec(align(16)) VUINT32 sPoly[8][4][1];
	__declspec(align(16)) VUINT32 sLn2[4][1];
	__declspec(align(16)) VUINT32 TinyRange[4][1];
} __svml_satanh_data_internal;
#endif

__svml_satanh_data_internal:
	/* sOne = SP 1.0 */
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* SgnMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* sTopMask12 */
	.align	16
	.long	0xFFFFF000, 0xFFFFF000, 0xFFFFF000, 0xFFFFF000
	/* iBrkValue = SP 2/3 */
	.align	16
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask ==*/
	.align	16
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff

	/* sPoly[] = SP polynomial */
	.align	16
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed /* 1.3820238411426544189453125e-01 P7 */
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3 /* -1.5122179687023162841796875e-01 P6 */
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12 /* 1.4042308926582336425781250e-01 P5 */
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37 /* -1.6472326219081878662109375e-01 P4 */
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190 /* 2.0007920265197753906250000e-01 P3 */
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e /* -2.5004237890243530273437500e-01 P2 */
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94 /* 3.3333265781402587890625000e-01 P1 */
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000 /* -5.0000000000000000000000000e-01 P0 */

	/* sLn2 = SP ln(2) */
	.align	16
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	/* TinyRange */
	.align	16
	.long	0x0C000000, 0x0C000000, 0x0C000000, 0x0C000000
	.align	16
	.type	__svml_satanh_data_internal, @object
	.size	__svml_satanh_data_internal, .-__svml_satanh_data_internal
