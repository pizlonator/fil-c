/* Function atanhf vectorized with AVX2.
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
   by use in the function. On cold-starts this might hhelp the
   prefetcher. Possibly a better idea is to interleave start/end so
   that the prefetcher is less likely to detect a stream and pull
   irrelivant lines into cache.  */
#define SgnMask				0
#define sOne				32
#define sTopMask12			64
#define TinyRange			96
#define iBrkValue			128
#define iOffExpoMask			160
#define sPoly				192
#define sLn2				448
#define sHalf				480

#include <sysdep.h>
#define ATANHF_DATA(x)			((x)+__svml_satanh_data_internal)

	.section .text.avx2, "ax", @progbits
ENTRY(_ZGVdN8v_atanhf_avx2)
	/* Strip off the sign, so treat X as positive until right at the end */
	vmovaps	ATANHF_DATA(SgnMask)(%rip), %ymm2
	vandps	%ymm2, %ymm0, %ymm3
	/* Load constants including One = 1 */
	vmovups	ATANHF_DATA(sOne)(%rip), %ymm5
	vsubps	%ymm3, %ymm5, %ymm1
	vmovups	ATANHF_DATA(sTopMask12)(%rip), %ymm4

	vrcpps	%ymm1, %ymm7
	vsubps	%ymm1, %ymm5, %ymm9
	vandps	%ymm4, %ymm7, %ymm6
	vsubps	%ymm3, %ymm9, %ymm7

	/* No need to split sU when FMA is available */
	vfnmadd213ps %ymm5, %ymm6, %ymm1
	vmovaps	%ymm0, %ymm8
	vfmadd213ps %ymm0, %ymm0, %ymm0
	vfnmadd231ps %ymm6, %ymm7, %ymm1

	/*
	 * Check whether |X| < 1, in which case we use the main function.
	 * Otherwise set the rangemask so that the callout will get used.
	 * Note that this will also use the callout for NaNs since not(NaN < 1).
	 */
	vcmpnlt_uqps %ymm5, %ymm3, %ymm14
	vcmplt_oqps ATANHF_DATA(TinyRange)(%rip), %ymm3, %ymm15

	/*
	 * Compute V = 2 * X trivially, and UHi + U_lo = 1 - X in two pieces,
	 * the upper part UHi being <= 12 bits long. Then we have
	 * atanh(X) = 1/2 * log((1 + X) / (1 - X)) = 1/2 * log1p(V / (UHi + ULo)).
	 */
	vaddps	%ymm3, %ymm3, %ymm3

	/*
	 * Split V as well into upper 12 bits and lower part, so that we can get
	 * a preliminary quotient estimate without rounding error.
	 */
	vandps	%ymm4, %ymm3, %ymm4
	vsubps	%ymm4, %ymm3, %ymm7

	/* Hence get initial quotient estimate QHi + QLo = R * VHi + R * VLo */
	vmulps	%ymm4, %ymm6, %ymm4

	/* Compute D = E + E^2 */
	vfmadd213ps %ymm1, %ymm1, %ymm1

	/* Record the sign for eventual reincorporation.  */
	vandnps	%ymm8, %ymm2, %ymm3

	/* Or the sign bit in with the tiny result to handle atanh(-0) correctly */
	vorps	%ymm3, %ymm0, %ymm13
	vmulps	%ymm7, %ymm6, %ymm2

	/*
	 * Compute R * (VHi + VLo) * (1 + E + E^2)
	 * = R *  (VHi + VLo) * (1 + D)
	 * = QHi + (QHi * D + QLo + QLo * D)
	 */

	/*
	 * If less precision is acceptable the `vmulps %ymm1, %ymm4, %ymm9;
	 * vaddps %ymm1, %ymm9, %ymm1` can be replaced with
	 * `vfmadd231ps %ymm1, %ymm4, %ymm4`.
	 */
	vmulps	%ymm1, %ymm4, %ymm6
	vfmadd213ps %ymm2, %ymm2, %ymm1
	vaddps	%ymm1, %ymm6, %ymm1

	/*
	 * Now finally accumulate the high and low parts of the
	 * argument to log1p, H + L, with a final compensated summation.
	 */
	vaddps	%ymm1, %ymm4, %ymm2

	/* reduction: compute r, n */
	vmovups	ATANHF_DATA(iBrkValue)(%rip), %ymm9

	/*
	 * Now we feed into the log1p code, using H in place of _VARG1 and
	 * later incorporating L into the reduced argument.
	 * compute 1+x as high, low parts
	 */
	vmaxps	%ymm2, %ymm5, %ymm0
	vminps	%ymm2, %ymm5, %ymm6

	/* This is needed for rounding (see `vaddps %ymm1, %ymm4, %ymm2`).  */
	vsubps	%ymm2, %ymm4, %ymm2
	vaddps	%ymm6, %ymm0, %ymm4
	vpsubd	%ymm9, %ymm4, %ymm7
	vsubps	%ymm4, %ymm0, %ymm4
	vaddps	%ymm2, %ymm1, %ymm2
	vmovaps	ATANHF_DATA(iOffExpoMask)(%rip), %ymm1

	vandps	%ymm1, %ymm7, %ymm0
	vaddps	%ymm4, %ymm6, %ymm4
	vandnps	%ymm7, %ymm1, %ymm6
	vmovups	ATANHF_DATA(sPoly+0)(%rip), %ymm1
	vpaddd	%ymm9, %ymm0, %ymm0
	vaddps	%ymm4, %ymm2, %ymm4
	vpsubd	%ymm6, %ymm5, %ymm6

	/* polynomial evaluation */
	vsubps	%ymm5, %ymm0, %ymm2
	vfmadd231ps %ymm4, %ymm6, %ymm2
	vfmadd213ps ATANHF_DATA(sPoly+32)(%rip), %ymm2, %ymm1
	vfmadd213ps ATANHF_DATA(sPoly+64)(%rip), %ymm2, %ymm1
	vfmadd213ps ATANHF_DATA(sPoly+96)(%rip), %ymm2, %ymm1
	vfmadd213ps ATANHF_DATA(sPoly+128)(%rip), %ymm2, %ymm1
	vfmadd213ps ATANHF_DATA(sPoly+160)(%rip), %ymm2, %ymm1
	vfmadd213ps ATANHF_DATA(sPoly+192)(%rip), %ymm2, %ymm1
	vfmadd213ps ATANHF_DATA(sPoly+224)(%rip), %ymm2, %ymm1

	vmulps	%ymm1, %ymm2, %ymm1
	vfmadd213ps %ymm2, %ymm2, %ymm1

	/* final reconstruction */
	vpsrad	$23, %ymm7, %ymm6
	vcvtdq2ps %ymm6, %ymm2
	vfmadd132ps ATANHF_DATA(sLn2)(%rip), %ymm1, %ymm2

	/* Finally, halve the result and reincorporate the sign */
	vxorps	ATANHF_DATA(sHalf)(%rip), %ymm3, %ymm3
	vmulps	%ymm2, %ymm3, %ymm2
	vmovmskps %ymm14, %edx
	testl	%edx, %edx

	vblendvps %ymm15, %ymm13, %ymm2, %ymm0
	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rdx r12 r13 r14 r15 ymm0
	/* No registers to restore on fast path.  */
	ret


	/* Cold case. edx has 1s where there was a special value that
	   needs to be handled by a atanhf call. Optimize for code size
	   more so than speed here. */
L(SPECIAL_VALUES_BRANCH):
	# LOE rbx rdx r12 r13 r14 r15 ymm0 ymm8
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

	/* Align stack and make room for 2x ymm vectors.  */
	andq	$-32, %rsp
	addq	$-64, %rsp

	/* Save all already computed inputs.  */
	vmovups	%ymm0, (%rsp)
	/* Save original input (ymm8 unchanged up to this point).  */
	vmovups	%ymm8, 32(%rsp)

	vzeroupper

	/* edx has 1s where there was a special value that needs to be handled
	   by a atanhf call.  */
	movl	%edx, %ebx
L(SPECIAL_VALUES_LOOP):
	# LOE rbx rbp r12 r13 r14 r15
	/* use rbp as index for special value that is saved across calls to
	   atanhf. We technically don't need a callee save register here as offset
	   to rsp is always [0, 28] so we can restore rsp by realigning to 64.
	   Essentially the tradeoff is 1 extra save/restore vs 2 extra instructions
	   in the loop. Realigning also costs more code size.  */
	xorl	%ebp, %ebp
	tzcntl	%ebx, %ebp

	/* Scalar math function call to process special input.  */
	vmovss	32(%rsp, %rbp, 4), %xmm0
	call	atanhf@PLT

	/* No good way to avoid the store-forwarding fault this will cause on
	   return. `lfence` avoids the SF fault but at greater cost as it
	   serialized stack/callee save restoration.  */
	vmovss	%xmm0, (%rsp, %rbp, 4)

	blsrl   %ebx, %ebx
	jnz	L(SPECIAL_VALUES_LOOP)
	# LOE r12 r13 r14 r15


	/* All results have been written to (%rsp).  */
	vmovups	(%rsp), %ymm0
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
END(_ZGVdN8v_atanhf_avx2)

	.section .rodata, "a"
	.align	32
#ifdef __svml_satanh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct{
	__declspec(align(32)) VUINT32 SgnMask[8][1];
	__declspec(align(32)) VUINT32 sOne[8][1];
	__declspec(align(32)) VUINT32 sTopMask12[8][1];
	__declspec(align(32)) VUINT32 TinyRange[8][1];
	__declspec(align(32)) VUINT32 iBrkValue[8][1];
	__declspec(align(32)) VUINT32 iOffExpoMask[8][1];
	__declspec(align(32)) VUINT32 sPoly[8][8][1];
	__declspec(align(32)) VUINT32 sLn2[8][1];
	__declspec(align(32)) VUINT32 sHalf[8][1];
} __svml_satanh_data_internal;
#endif
__svml_satanh_data_internal:
	/* SgnMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* sOne = SP 1.0 */
	.align	32
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* sTopMask12 */
	.align	32
	.long	0xFFFFF000, 0xFFFFF000, 0xFFFFF000, 0xFFFFF000
	.long	0xFFFFF000, 0xFFFFF000, 0xFFFFF000, 0xFFFFF000
	/* TinyRange */
	.align	32
	.long	0x0C000000, 0x0C000000, 0x0C000000, 0x0C000000
	.long	0x0C000000, 0x0C000000, 0x0C000000, 0x0C000000
	/* iBrkValue = SP 2/3 */
	.align	32
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	32
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sPoly[] = SP polynomial */
	.align	32
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed /* 1.3820238411426544189453125e-01 P7 */
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3 /* -1.5122179687023162841796875e-01 P6 */
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12 /* 1.4042308926582336425781250e-01 P5 */
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37 /* -1.6472326219081878662109375e-01 P4 */
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190 /* 2.0007920265197753906250000e-01 P3 */
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e /* -2.5004237890243530273437500e-01 P2 */
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94 /* 3.3333265781402587890625000e-01 P1 */
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000 /* -5.0000000000000000000000000e-01 P0 */
	/* sLn2 = SP ln(2) */
	.align	32
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	/* sHalf */
	.align	32
	.long	0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000
	.long	0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000
	.align	32
	.type	__svml_satanh_data_internal, @object
	.size	__svml_satanh_data_internal, .-__svml_satanh_data_internal
