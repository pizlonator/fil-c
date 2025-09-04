/* memrchr optimized with 256-bit EVEX instructions.
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
   <https://www.gnu.org/licenses/>.  */

#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# include <sysdep.h>

# ifndef VEC_SIZE
#  include "x86-evex256-vecs.h"
# endif

# include "reg-macros.h"

# ifndef MEMRCHR
#  define MEMRCHR	__memrchr_evex
# endif

# define PAGE_SIZE	4096
# define VMATCH	VMM(0)

	.section SECTION(.text), "ax", @progbits
ENTRY_P2ALIGN(MEMRCHR, 6)
# ifdef __ILP32__
	/* Clear upper bits.  */
	and	%RDX_LP, %RDX_LP
# else
	test	%RDX_LP, %RDX_LP
# endif
	jz	L(zero_0)

	/* Get end pointer. Minus one for three reasons. 1) It is
	   necessary for a correct page cross check and 2) it correctly
	   sets up end ptr to be subtract by lzcnt aligned. 3) it is a
	   necessary step in aligning ptr.  */
	leaq	-1(%rdi, %rdx), %rax
	vpbroadcastb %esi, %VMATCH

	/* Check if we can load 1x VEC without cross a page.  */
	testl	$(PAGE_SIZE - VEC_SIZE), %eax
	jz	L(page_cross)

	/* Don't use rax for pointer here because EVEX has better
	   encoding with offset % VEC_SIZE == 0.  */
	vpcmpeqb (VEC_SIZE * -1)(%rdi, %rdx), %VMATCH, %k0
	KMOV	%k0, %VRCX

	/* If rcx is zero then lzcnt -> VEC_SIZE.  NB: there is a
	   already a dependency between rcx and rsi so no worries about
	   false-dep here.  */
	lzcnt	%VRCX, %VRSI
	/* If rdx <= rsi then either 1) rcx was non-zero (there was a
	   match) but it was out of bounds or 2) rcx was zero and rdx
	   was <= VEC_SIZE so we are done scanning.  */
	cmpq	%rsi, %rdx
	/* NB: Use branch to return zero/non-zero.  Common usage will
	   branch on result of function (if return is null/non-null).
	   This branch can be used to predict the ensuing one so there
	   is no reason to extend the data-dependency with cmovcc.  */
	jbe	L(zero_0)

	/* If rcx is zero then len must be > RDX, otherwise since we
	   already tested len vs lzcnt(rcx) (in rsi) we are good to
	   return this match.  */
	test	%VRCX, %VRCX
	jz	L(more_1x_vec)
	subq	%rsi, %rax
	ret

	/* Fits in aligning bytes of first cache line for VEC_SIZE ==
	   32.  */
# if VEC_SIZE == 32
	.p2align 4,, 2
L(zero_0):
	xorl	%eax, %eax
	ret
# endif

	.p2align 4,, 10
L(more_1x_vec):
	/* Align rax (pointer to string).  */
	andq	$-VEC_SIZE, %rax
L(page_cross_continue):
	/* Recompute length after aligning.  */
	subq	%rdi, %rax

	cmpq	$(VEC_SIZE * 2), %rax
	ja	L(more_2x_vec)

L(last_2x_vec):
	vpcmpeqb (VEC_SIZE * -1)(%rdi, %rax), %VMATCH, %k0
	KMOV	%k0, %VRCX

	test	%VRCX, %VRCX
	jnz	L(ret_vec_x0_test)

	/* If VEC_SIZE == 64 need to subtract because lzcntq won't
	   implicitly add VEC_SIZE to match position.  */
# if VEC_SIZE == 64
	subl	$VEC_SIZE, %eax
# else
	cmpb	$VEC_SIZE, %al
# endif
	jle	L(zero_2)

	/* We adjusted rax (length) for VEC_SIZE == 64 so need separate
	   offsets.  */
# if VEC_SIZE == 64
	vpcmpeqb (VEC_SIZE * -1)(%rdi, %rax), %VMATCH, %k0
# else
	vpcmpeqb (VEC_SIZE * -2)(%rdi, %rax), %VMATCH, %k0
# endif
	KMOV	%k0, %VRCX
	/* NB: 64-bit lzcnt. This will naturally add 32 to position for
	   VEC_SIZE == 32.  */
	lzcntq	%rcx, %rcx
	subl	%ecx, %eax
	ja	L(first_vec_x1_ret)
	/* If VEC_SIZE == 64 put L(zero_0) here as we can't fit in the
	   first cache line (this is the second cache line).  */
# if VEC_SIZE == 64
L(zero_0):
# endif
L(zero_2):
	xorl	%eax, %eax
	ret

	/* NB: Fits in aligning bytes before next cache line for
	   VEC_SIZE == 32.  For VEC_SIZE == 64 this is attached to
	   L(first_vec_x0_test).  */
# if VEC_SIZE == 32
L(first_vec_x1_ret):
	leaq	-1(%rdi, %rax), %rax
	ret
# endif

	.p2align 4,, 6
L(ret_vec_x0_test):
	lzcnt	%VRCX, %VRCX
	subl	%ecx, %eax
	jle	L(zero_2)
# if VEC_SIZE == 64
	/* Reuse code at the end of L(ret_vec_x0_test) as we can't fit
	   L(first_vec_x1_ret) in the same cache line as its jmp base
	   so we might as well save code size.  */
L(first_vec_x1_ret):
# endif
	leaq	-1(%rdi, %rax), %rax
	ret

	.p2align 4,, 6
L(loop_last_4x_vec):
	/* Compute remaining length.  */
	subl	%edi, %eax
L(last_4x_vec):
	cmpl	$(VEC_SIZE * 2), %eax
	jle	L(last_2x_vec)
# if VEC_SIZE == 32
	/* Only align for VEC_SIZE == 32.  For VEC_SIZE == 64 we need
	   the spare bytes to align the loop properly.  */
	.p2align 4,, 10
# endif
L(more_2x_vec):

	/* Length > VEC_SIZE * 2 so check the first 2x VEC for match and
	   return if either hit.  */
	vpcmpeqb (VEC_SIZE * -1)(%rdi, %rax), %VMATCH, %k0
	KMOV	%k0, %VRCX

	test	%VRCX, %VRCX
	jnz	L(first_vec_x0)

	vpcmpeqb (VEC_SIZE * -2)(%rdi, %rax), %VMATCH, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(first_vec_x1)

	/* Need no matter what.  */
	vpcmpeqb (VEC_SIZE * -3)(%rdi, %rax), %VMATCH, %k0
	KMOV	%k0, %VRCX

	/* Check if we are near the end.  */
	subq	$(VEC_SIZE * 4), %rax
	ja	L(more_4x_vec)

	test	%VRCX, %VRCX
	jnz	L(first_vec_x2_test)

	/* Adjust length for final check and check if we are at the end.
	 */
	addl	$(VEC_SIZE * 1), %eax
	jle	L(zero_1)

	vpcmpeqb (VEC_SIZE * -1)(%rdi, %rax), %VMATCH, %k0
	KMOV	%k0, %VRCX

	lzcnt	%VRCX, %VRCX
	subl	%ecx, %eax
	ja	L(first_vec_x3_ret)
L(zero_1):
	xorl	%eax, %eax
	ret
L(first_vec_x3_ret):
	leaq	-1(%rdi, %rax), %rax
	ret

	.p2align 4,, 6
L(first_vec_x2_test):
	/* Must adjust length before check.  */
	subl	$-(VEC_SIZE * 2 - 1), %eax
	lzcnt	%VRCX, %VRCX
	subl	%ecx, %eax
	jl	L(zero_4)
	addq	%rdi, %rax
	ret


	.p2align 4,, 10
L(first_vec_x0):
	bsr	%VRCX, %VRCX
	leaq	(VEC_SIZE * -1)(%rdi, %rax), %rax
	addq	%rcx, %rax
	ret

	/* Fits unobtrusively here.  */
L(zero_4):
	xorl	%eax, %eax
	ret

	.p2align 4,, 10
L(first_vec_x1):
	bsr	%VRCX, %VRCX
	leaq	(VEC_SIZE * -2)(%rdi, %rax), %rax
	addq	%rcx, %rax
	ret

	.p2align 4,, 8
L(first_vec_x3):
	bsr	%VRCX, %VRCX
	addq	%rdi, %rax
	addq	%rcx, %rax
	ret

	.p2align 4,, 6
L(first_vec_x2):
	bsr	%VRCX, %VRCX
	leaq	(VEC_SIZE * 1)(%rdi, %rax), %rax
	addq	%rcx, %rax
	ret

	.p2align 4,, 2
L(more_4x_vec):
	test	%VRCX, %VRCX
	jnz	L(first_vec_x2)

	vpcmpeqb (%rdi, %rax), %VMATCH, %k0
	KMOV	%k0, %VRCX

	test	%VRCX, %VRCX
	jnz	L(first_vec_x3)

	/* Check if near end before re-aligning (otherwise might do an
	   unnecessary loop iteration).  */
	cmpq	$(VEC_SIZE * 4), %rax
	jbe	L(last_4x_vec)


	/* NB: We setup the loop to NOT use index-address-mode for the
	   buffer.  This costs some instructions & code size but avoids
	   stalls due to unlaminated micro-fused instructions (as used
	   in the loop) from being forced to issue in the same group
	   (essentially narrowing the backend width).  */

	/* Get endptr for loop in rdx. NB: Can't just do while rax > rdi
	   because lengths that overflow can be valid and break the
	   comparison.  */
# if VEC_SIZE == 64
	/* Use rdx as intermediate to compute rax, this gets us imm8
	   encoding which just allows the L(more_4x_vec) block to fit
	   in 1 cache-line.  */
	leaq	(VEC_SIZE * 4)(%rdi), %rdx
	leaq	(VEC_SIZE * -1)(%rdx, %rax), %rax

	/* No evex machine has partial register stalls. This can be
	   replaced with: `andq $(VEC_SIZE * -4), %rax/%rdx` if that
	   changes.  */
	xorb	%al, %al
	xorb	%dl, %dl
# else
	leaq	(VEC_SIZE * 3)(%rdi, %rax), %rax
	andq	$(VEC_SIZE * -4), %rax
	leaq	(VEC_SIZE * 4)(%rdi), %rdx
	andq	$(VEC_SIZE * -4), %rdx
# endif


	.p2align 4
L(loop_4x_vec):
	/* NB: We could do the same optimization here as we do for
	   memchr/rawmemchr by using VEX encoding in the loop for access
	   to VEX vpcmpeqb + vpternlogd.  Since memrchr is not as hot as
	   memchr it may not be worth the extra code size, but if the
	   need arises it an easy ~15% perf improvement to the loop.  */

	cmpq	%rdx, %rax
	je	L(loop_last_4x_vec)
	/* Store 1 were not-equals and 0 where equals in k1 (used to
	   mask later on).  */
	vpcmpb	$4, (VEC_SIZE * -1)(%rax), %VMATCH, %k1

	/* VEC(2/3) will have zero-byte where we found a CHAR.  */
	vpxorq	(VEC_SIZE * -2)(%rax), %VMATCH, %VMM(2)
	vpxorq	(VEC_SIZE * -3)(%rax), %VMATCH, %VMM(3)
	vpcmpeqb (VEC_SIZE * -4)(%rax), %VMATCH, %k4

	/* Combine VEC(2/3) with min and maskz with k1 (k1 has zero bit
	   where CHAR is found and VEC(2/3) have zero-byte where CHAR
	   is found.  */
	vpminub	%VMM(2), %VMM(3), %VMM(3){%k1}{z}
	vptestnmb %VMM(3), %VMM(3), %k2

	addq	$-(VEC_SIZE * 4), %rax

	/* Any 1s and we found CHAR.  */
	KORTEST %k2, %k4
	jz	L(loop_4x_vec)


	/* K1 has non-matches for first VEC. inc; jz will overflow rcx
	   iff all bytes where non-matches.  */
	KMOV	%k1, %VRCX
	inc	%VRCX
	jnz	L(first_vec_x0_end)

	vptestnmb %VMM(2), %VMM(2), %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(first_vec_x1_end)
	KMOV	%k2, %VRCX

	/* Separate logic for VEC_SIZE == 64 and VEC_SIZE == 32 for
	   returning last 2x VEC. For VEC_SIZE == 64 we test each VEC
	   individually, for VEC_SIZE == 32 we combine them in a single
	   64-bit GPR.  */
# if VEC_SIZE == 64
	test	%VRCX, %VRCX
	jnz	L(first_vec_x2_end)
	KMOV	%k4, %VRCX
# else
	/* Combine last 2 VEC matches for VEC_SIZE == 32. If rcx (from
	   VEC(3)) is zero (no CHAR in VEC(3)) then it won't affect the
	   result in rsi (from VEC(4)). If rcx is non-zero then CHAR in
	   VEC(3) and bsrq will use that position.  */
	KMOV	%k4, %VRSI
	salq	$32, %rcx
	orq	%rsi, %rcx
# endif
	bsrq	%rcx, %rcx
	addq	%rcx, %rax
	ret

	.p2align 4,, 4
L(first_vec_x0_end):
	/* rcx has 1s at non-matches so we need to `not` it. We used
	   `inc` to test if zero so use `neg` to complete the `not` so
	   the last 1 bit represent a match.  NB: (-x + 1 == ~x).  */
	neg	%VRCX
	bsr	%VRCX, %VRCX
	leaq	(VEC_SIZE * 3)(%rcx, %rax), %rax
	ret

	.p2align 4,, 10
L(first_vec_x1_end):
	bsr	%VRCX, %VRCX
	leaq	(VEC_SIZE * 2)(%rcx, %rax), %rax
	ret

# if VEC_SIZE == 64
	/* Since we can't combine the last 2x VEC for VEC_SIZE == 64
	   need return label for it.  */
	.p2align 4,, 4
L(first_vec_x2_end):
	bsr	%VRCX, %VRCX
	leaq	(VEC_SIZE * 1)(%rcx, %rax), %rax
	ret
# endif


	.p2align 4,, 4
L(page_cross):
	/* only lower bits of eax[log2(VEC_SIZE):0] are set so we can
	   use movzbl to get the amount of bytes we are checking here.
	 */
	movzbl	%al, %ecx
	andq	$-VEC_SIZE, %rax
	vpcmpeqb (%rax), %VMATCH, %k0
	KMOV	%k0, %VRSI

	/* eax was comptued as %rdi + %rdx - 1 so need to add back 1
	   here.  */
	leal	1(%rcx), %r8d

	/* Invert ecx to get shift count for byte matches out of range.
	 */
	notl	%ecx
	shlx	%VRCX, %VRSI, %VRSI

	/* if r8 < rdx then the entire [buf, buf + len] is handled in
	   the page cross case.  NB: we can't use the trick here we use
	   in the non page-cross case because we aren't checking full
	   VEC_SIZE.  */
	cmpq	%r8, %rdx
	ja	L(page_cross_check)
	lzcnt	%VRSI, %VRSI
	subl	%esi, %edx
	ja	L(page_cross_ret)
	xorl	%eax, %eax
	ret

L(page_cross_check):
	test	%VRSI, %VRSI
	jz	L(page_cross_continue)

	lzcnt	%VRSI, %VRSI
	subl	%esi, %edx
L(page_cross_ret):
	leaq	-1(%rdi, %rdx), %rax
	ret
END(MEMRCHR)
#endif
