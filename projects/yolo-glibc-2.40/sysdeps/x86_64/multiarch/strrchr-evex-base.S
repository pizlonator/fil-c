/* Implementation for strrchr using evex256 and evex512.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   <https://www.gnu.org/licenses/>.  */

#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# include <sysdep.h>

# ifdef USE_AS_WCSRCHR
#  if VEC_SIZE == 64
#   define RCX_M	cx
#   define KORTEST_M	kortestw
#  else
#   define RCX_M	cl
#   define KORTEST_M	kortestb
#  endif

#  define SHIFT_REG	VRCX
#  define CHAR_SIZE	4
#  define VPCMP		vpcmpd
#  define VPMIN		vpminud
#  define VPTESTN	vptestnmd
#  define VPTEST	vptestmd
#  define VPBROADCAST	vpbroadcastd
#  define VPCMPEQ	vpcmpeqd

# else
#  if VEC_SIZE == 64
#   define SHIFT_REG	VRCX
#  else
#   define SHIFT_REG	VRDI
#  endif
#  define CHAR_SIZE	1
#  define VPCMP		vpcmpb
#  define VPMIN		vpminub
#  define VPTESTN	vptestnmb
#  define VPTEST	vptestmb
#  define VPBROADCAST	vpbroadcastb
#  define VPCMPEQ	vpcmpeqb

#  define RCX_M		VRCX
#  define KORTEST_M	KORTEST
# endif

# if VEC_SIZE == 32 || (defined USE_AS_WCSRCHR)
#  define SHIFT_R(cnt, val)	shrx cnt, val, val
# else
#  define SHIFT_R(cnt, val)	shr %cl, val
# endif

# define VMATCH		VMM(0)
# define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)
# define PAGE_SIZE	4096

	.section SECTION(.text), "ax", @progbits
	/* Aligning entry point to 64 byte, provides better performance for
	   one vector length string.  */
ENTRY_P2ALIGN(STRRCHR, 6)
	movl	%edi, %eax
	/* Broadcast CHAR to VMATCH.  */
	VPBROADCAST %esi, %VMATCH

	andl	$(PAGE_SIZE - 1), %eax
	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
	jg	L(cross_page_boundary)
L(page_cross_continue):
	VMOVU	(%rdi), %VMM(1)
	/* k0 has a 1 for each zero CHAR in YMM1.  */
	VPTESTN	%VMM(1), %VMM(1), %k0
	KMOV	%k0, %VGPR(rsi)
	test	%VGPR(rsi), %VGPR(rsi)
	jz	L(aligned_more)
	/* fallthrough: zero CHAR in first VEC.  */

	/* K1 has a 1 for each search CHAR match in VEC(1).  */
	VPCMPEQ	%VMATCH, %VMM(1), %k1
	KMOV	%k1, %VGPR(rax)
	/* Build mask up until first zero CHAR (used to mask of
	   potential search CHAR matches past the end of the string).  */
	blsmsk	%VGPR(rsi), %VGPR(rsi)
	/* Use `and` here to remove any out of bounds matches so we can
	   do a reverse scan on `rax` to find the last match.  */
	and	%VGPR(rsi), %VGPR(rax)
	jz	L(ret0)
	/* Get last match.  */
	bsr	%VGPR(rax), %VGPR(rax)
# ifdef USE_AS_WCSRCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	addq	%rdi, %rax
# endif
L(ret0):
	ret

	/* Returns for first vec x1/x2/x3 have hard coded backward
	   search path for earlier matches.  */
	.p2align 4,, 6
L(first_vec_x1):
	VPCMPEQ	%VMATCH, %VMM(2), %k1
	KMOV	%k1, %VGPR(rax)
	blsmsk	%VGPR(rcx), %VGPR(rcx)
	/* eax non-zero if search CHAR in range.  */
	and	%VGPR(rcx), %VGPR(rax)
	jnz	L(first_vec_x1_return)

	/* fallthrough: no match in YMM2 then need to check for earlier
	   matches (in YMM1).  */
	.p2align 4,, 4
L(first_vec_x0_test):
	VPCMPEQ	%VMATCH, %VMM(1), %k1
	KMOV	%k1, %VGPR(rax)
	test	%VGPR(rax), %VGPR(rax)
	jz	L(ret1)
	bsr	%VGPR(rax), %VGPR(rax)
# ifdef USE_AS_WCSRCHR
	leaq	(%rsi, %rax, CHAR_SIZE), %rax
# else
	addq	%rsi, %rax
# endif
L(ret1):
	ret

	.p2align 4,, 10
L(first_vec_x3):
	VPCMPEQ	%VMATCH, %VMM(4), %k1
	KMOV	%k1, %VGPR(rax)
	blsmsk	%VGPR(rcx), %VGPR(rcx)
	/* If no search CHAR match in range check YMM1/YMM2/YMM3.  */
	and	%VGPR(rcx), %VGPR(rax)
	jz	L(first_vec_x1_or_x2)
	bsr	%VGPR(rax), %VGPR(rax)
	leaq	(VEC_SIZE * 3)(%rdi, %rax, CHAR_SIZE), %rax
	ret
	.p2align 4,, 4

L(first_vec_x2):
	VPCMPEQ	%VMATCH, %VMM(3), %k1
	KMOV	%k1, %VGPR(rax)
	blsmsk	%VGPR(rcx), %VGPR(rcx)
	/* Check YMM3 for last match first. If no match try YMM2/YMM1.  */
	and	%VGPR(rcx), %VGPR(rax)
	jz	L(first_vec_x0_x1_test)
	bsr	%VGPR(rax), %VGPR(rax)
	leaq	(VEC_SIZE * 2)(%r8, %rax, CHAR_SIZE), %rax
	ret

	.p2align 4,, 6
L(first_vec_x0_x1_test):
	VPCMPEQ	%VMATCH, %VMM(2), %k1
	KMOV	%k1, %VGPR(rax)
	/* Check YMM2 for last match first. If no match try YMM1.  */
	test	%VGPR(rax), %VGPR(rax)
	jz	L(first_vec_x0_test)
	.p2align 4,, 4
L(first_vec_x1_return):
	bsr	%VGPR(rax), %VGPR(rax)
	leaq	(VEC_SIZE)(%r8, %rax, CHAR_SIZE), %rax
	ret

	.p2align 4,, 12
L(aligned_more):
	/* Need to keep original pointer incase VEC(1) has last match.  */
	movq	%rdi, %rsi
	andq	$-VEC_SIZE, %rdi

	VMOVU	VEC_SIZE(%rdi), %VMM(2)
	VPTESTN	%VMM(2), %VMM(2), %k0
	KMOV	%k0, %VRCX
	movq	%rdi, %r8
	test	%VRCX, %VRCX
	jnz	L(first_vec_x1)

	VMOVU	(VEC_SIZE * 2)(%rdi), %VMM(3)
	VPTESTN	%VMM(3), %VMM(3), %k0
	KMOV	%k0, %VRCX

	test	%VRCX, %VRCX
	jnz	L(first_vec_x2)

	VMOVU	(VEC_SIZE * 3)(%rdi), %VMM(4)
	VPTESTN	%VMM(4), %VMM(4), %k0
	KMOV	%k0, %VRCX

	/* Intentionally use 64-bit here.  EVEX256 version needs 1-byte
	   padding for efficient nop before loop alignment.  */
	test	%rcx, %rcx
	jnz	L(first_vec_x3)

	andq	$-(VEC_SIZE * 2), %rdi
	.p2align 4
L(first_aligned_loop):
	/* Preserve VEC(1), VEC(2), VEC(3), and VEC(4) until we can
	   gurantee they don't store a match.  */
	VMOVA	(VEC_SIZE * 4)(%rdi), %VMM(5)
	VMOVA	(VEC_SIZE * 5)(%rdi), %VMM(6)

	VPCMP	$4, %VMM(5), %VMATCH, %k2
	VPCMP	$4, %VMM(6), %VMATCH, %k3{%k2}

	VPMIN	%VMM(5), %VMM(6), %VMM(7)

	VPTEST	%VMM(7), %VMM(7), %k1{%k3}
	subq	$(VEC_SIZE * -2), %rdi
	KORTEST_M %k1, %k1
	jc	L(first_aligned_loop)

	VPTESTN	%VMM(7), %VMM(7), %k1
	KMOV	%k1, %VRDX
	test	%VRDX, %VRDX
	jz	L(second_aligned_loop_prep)

	KORTEST_M %k3, %k3
	jnc	L(return_first_aligned_loop)

	.p2align 4,, 6
L(first_vec_x1_or_x2_or_x3):
	VPCMPEQ	%VMM(4), %VMATCH, %k4
	KMOV	%k4, %VRAX
	test	%VRAX, %VRAX
	jz	L(first_vec_x1_or_x2)
	bsr	%VRAX, %VRAX
	leaq	(VEC_SIZE * 3)(%r8, %rax, CHAR_SIZE), %rax
	ret

	.p2align 4,, 8
L(return_first_aligned_loop):
	VPTESTN	%VMM(5), %VMM(5), %k0
	KMOV	%k0, %VRCX
	blsmsk	%VRCX, %VRCX
	jnc	L(return_first_new_match_first)
	blsmsk	%VRDX, %VRDX
	VPCMPEQ	%VMM(6), %VMATCH, %k0
	KMOV	%k0, %VRAX
	addq	$VEC_SIZE, %rdi
	and	%VRDX, %VRAX
	jnz	L(return_first_new_match_ret)
	subq	$VEC_SIZE, %rdi
L(return_first_new_match_first):
	KMOV	%k2, %VRAX
# ifdef USE_AS_WCSRCHR
	xorl	$((1 << CHAR_PER_VEC)- 1), %VRAX
	and	%VRCX, %VRAX
# else
	andn	%VRCX, %VRAX, %VRAX
# endif
	jz	L(first_vec_x1_or_x2_or_x3)
L(return_first_new_match_ret):
	bsr	%VRAX, %VRAX
	leaq	(VEC_SIZE * 2)(%rdi, %rax, CHAR_SIZE), %rax
	ret

	.p2align 4,, 10
L(first_vec_x1_or_x2):
	VPCMPEQ	%VMM(3), %VMATCH, %k3
	KMOV	%k3, %VRAX
	test	%VRAX, %VRAX
	jz	L(first_vec_x0_x1_test)
	bsr	%VRAX, %VRAX
	leaq	(VEC_SIZE * 2)(%r8, %rax, CHAR_SIZE), %rax
	ret

	.p2align 4
	/* We can throw away the work done for the first 4x checks here
	   as we have a later match. This is the 'fast' path persay.  */
L(second_aligned_loop_prep):
L(second_aligned_loop_set_furthest_match):
	movq	%rdi, %rsi
	VMOVA	%VMM(5), %VMM(7)
	VMOVA	%VMM(6), %VMM(8)
	.p2align 4
L(second_aligned_loop):
	VMOVU	(VEC_SIZE * 4)(%rdi), %VMM(5)
	VMOVU	(VEC_SIZE * 5)(%rdi), %VMM(6)
	VPCMP	$4, %VMM(5), %VMATCH, %k2
	VPCMP	$4, %VMM(6), %VMATCH, %k3{%k2}

	VPMIN	%VMM(5), %VMM(6), %VMM(4)

	VPTEST	%VMM(4), %VMM(4), %k1{%k3}
	subq	$(VEC_SIZE * -2), %rdi
	KMOV	%k1, %VRCX
	inc	%RCX_M
	jz	L(second_aligned_loop)
	VPTESTN	%VMM(4), %VMM(4), %k1
	KMOV	%k1, %VRDX
	test	%VRDX, %VRDX
	jz	L(second_aligned_loop_set_furthest_match)

	KORTEST_M %k3, %k3
	jnc	L(return_new_match)
	/* branch here because there is a significant advantage interms
	   of output dependency chance in using edx.  */

L(return_old_match):
	VPCMPEQ	%VMM(8), %VMATCH, %k0
	KMOV	%k0, %VRCX
	bsr	%VRCX, %VRCX
	jnz	L(return_old_match_ret)

	VPCMPEQ	%VMM(7), %VMATCH, %k0
	KMOV	%k0, %VRCX
	bsr	%VRCX, %VRCX
	subq	$VEC_SIZE, %rsi
L(return_old_match_ret):
	leaq	(VEC_SIZE * 3)(%rsi, %rcx, CHAR_SIZE), %rax
	ret

L(return_new_match):
	VPTESTN	%VMM(5), %VMM(5), %k0
	KMOV	%k0, %VRCX
	blsmsk	%VRCX, %VRCX
	jnc	L(return_new_match_first)
	dec	%VRDX
	VPCMPEQ	%VMM(6), %VMATCH, %k0
	KMOV	%k0, %VRAX
	addq	$VEC_SIZE, %rdi
	and	%VRDX, %VRAX
	jnz	L(return_new_match_ret)
	subq	$VEC_SIZE, %rdi
L(return_new_match_first):
	KMOV	%k2, %VRAX
# ifdef USE_AS_WCSRCHR
	xorl	$((1 << CHAR_PER_VEC)- 1), %VRAX
	and	%VRCX, %VRAX
# else
	andn	%VRCX, %VRAX, %VRAX
# endif
	jz	L(return_old_match)
L(return_new_match_ret):
	bsr	%VRAX, %VRAX
	leaq	(VEC_SIZE * 2)(%rdi, %rax, CHAR_SIZE), %rax
	ret

L(cross_page_boundary):
	/* eax contains all the page offset bits of src (rdi). `xor rdi,
	   rax` sets pointer will all page offset bits cleared so
	   offset of (PAGE_SIZE - VEC_SIZE) will get last aligned VEC
	   before page cross (guaranteed to be safe to read). Doing this
	   as opposed to `movq %rdi, %rax; andq $-VEC_SIZE, %rax` saves
	   a bit of code size.  */
	xorq	%rdi, %rax
	VMOVU	(PAGE_SIZE - VEC_SIZE)(%rax), %VMM(1)
	VPTESTN	%VMM(1), %VMM(1), %k0
	KMOV	%k0, %VRSI

	/* Shift out zero CHAR matches that are before the beginning of
	   src (rdi).  */
# if VEC_SIZE == 64 || (defined USE_AS_WCSRCHR)
	movl	%edi, %ecx
# endif
# ifdef USE_AS_WCSRCHR
	andl	$(VEC_SIZE - 1), %ecx
	shrl	$2, %ecx
# endif
	SHIFT_R	(%SHIFT_REG, %VRSI)
# if VEC_SIZE == 32 || (defined USE_AS_WCSRCHR)
	/* For strrchr-evex512 we use SHIFT_R as shr which will set zero
	   flag.  */
	test	%VRSI, %VRSI
# endif
	jz	L(page_cross_continue)

	/* Found zero CHAR so need to test for search CHAR.  */
	VPCMPEQ	%VMATCH, %VMM(1), %k1
	KMOV	%k1, %VRAX
	/* Shift out search CHAR matches that are before the beginning of
	   src (rdi).  */
	SHIFT_R	(%SHIFT_REG, %VRAX)
	/* Check if any search CHAR match in range.  */
	blsmsk	%VRSI, %VRSI
	and	%VRSI, %VRAX
	jz	L(ret2)
	bsr	%VRAX, %VRAX
# ifdef USE_AS_WCSRCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	addq	%rdi, %rax
# endif
L(ret2):
	ret
	/* 3 bytes from cache-line for evex.  */
	/* 0 bytes from cache-line for evex512.  */
END(STRRCHR)
#endif
