/* memchr/wmemchr optimized with 256-bit EVEX instructions.
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
#include <sysdep.h>

#if ISA_SHOULD_BUILD (4)

# ifndef VEC_SIZE
#  include "x86-evex256-vecs.h"
# endif

# ifndef MEMCHR
#  define MEMCHR	__memchr_evex
# endif

# ifdef USE_AS_WMEMCHR
#  define PC_SHIFT_GPR	rcx
#  define VPTESTN	vptestnmd
#  define VPBROADCAST	vpbroadcastd
#  define VPMINU	vpminud
#  define VPCMP	vpcmpd
#  define VPCMPEQ	vpcmpeqd
#  define CHAR_SIZE	4

#  define USE_WIDE_CHAR
# else
#  define PC_SHIFT_GPR	rdi
#  define VPTESTN	vptestnmb
#  define VPBROADCAST	vpbroadcastb
#  define VPMINU	vpminub
#  define VPCMP	vpcmpb
#  define VPCMPEQ	vpcmpeqb
#  define CHAR_SIZE	1
# endif

# include "reg-macros.h"


/* If not in an RTM and VEC_SIZE != 64 (the VEC_SIZE = 64
   doesn't have VEX encoding), use VEX encoding in loop so we
   can use vpcmpeqb + vptern which is more efficient than the
   EVEX alternative.  */
# if defined USE_IN_RTM || VEC_SIZE == 64
#  undef COND_VZEROUPPER
#  undef VZEROUPPER_RETURN
#  undef VZEROUPPER

#  define COND_VZEROUPPER
#  define VZEROUPPER_RETURN	ret
#  define VZEROUPPER

#  define USE_TERN_IN_LOOP	0
# else
#  define USE_TERN_IN_LOOP	1
#  undef VZEROUPPER
#  define VZEROUPPER	vzeroupper
# endif

# if USE_TERN_IN_LOOP
	/* Resulting bitmask for vpmovmskb has 4-bits set for each wchar
	   so we don't want to multiply resulting index.  */
#  define TERN_CHAR_MULT	1

#  ifdef USE_AS_WMEMCHR
#   define TEST_END()	inc %VRCX
#  else
#   define TEST_END()	add %rdx, %rcx
#  endif
# else
#  define TERN_CHAR_MULT	CHAR_SIZE
#  define TEST_END()	KORTEST %k2, %k3
# endif

# if defined USE_AS_WMEMCHR || !USE_TERN_IN_LOOP
#  ifndef USE_AS_WMEMCHR
#   define GPR_X0_IS_RET	1
#  else
#   define GPR_X0_IS_RET	0
#  endif
#  define GPR_X0	rax
# else
#  define GPR_X0_IS_RET	0
#  define GPR_X0	rdx
# endif

# define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)

# if CHAR_PER_VEC == 64
#  define LAST_VEC_OFFSET	(VEC_SIZE * 3)
# else
#  define LAST_VEC_OFFSET	(VEC_SIZE * 2)
# endif
# if CHAR_PER_VEC >= 32
#  define MASK_GPR(...)	VGPR(__VA_ARGS__)
# elif CHAR_PER_VEC == 16
#  define MASK_GPR(reg)	VGPR_SZ(reg, 16)
# else
#  define MASK_GPR(reg)	VGPR_SZ(reg, 8)
# endif

# define VMATCH	VMM(0)
# define VMATCH_LO	VMM_lo(0)

# define PAGE_SIZE	4096


	.section SECTION(.text), "ax", @progbits
ENTRY_P2ALIGN (MEMCHR, 6)
	/* Check for zero length.  */
	test	%RDX_LP, %RDX_LP
	jz	L(zero_0)

# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	VPBROADCAST %esi, %VMATCH
	/* Check if we may cross page boundary with one vector load.  */
	movl	%edi, %eax
	andl	$(PAGE_SIZE - 1), %eax
	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
	ja	L(page_cross)

	VPCMPEQ	(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRAX
# ifndef USE_AS_WMEMCHR
	/* If rcx is zero then tzcnt -> CHAR_PER_VEC.  NB: there is a
	   already a dependency between rcx and rsi so no worries about
	   false-dep here.  */
	tzcnt	%VRAX, %VRSI
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
	test	%VRAX, %VRAX
	jz	L(more_1x_vec)
	leaq	(%rdi, %rsi), %rax
# else

	/* We can't use the `tzcnt` trick for wmemchr because CHAR_SIZE
	   > 1 so if rcx is tzcnt != CHAR_PER_VEC.  */
	cmpq	$CHAR_PER_VEC, %rdx
	ja	L(more_1x_vec)
	tzcnt	%VRAX, %VRAX
	cmpl	%eax, %edx
	jbe	L(zero_0)
L(first_vec_x0_ret):
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# endif
	ret

	/* Only fits in first cache line for VEC_SIZE == 32.  */
# if VEC_SIZE == 32
	.p2align 4,, 2
L(zero_0):
	xorl	%eax, %eax
	ret
# endif

	.p2align 4,, 9
L(more_1x_vec):
# ifdef USE_AS_WMEMCHR
	/* If wmemchr still need to test if there was a match in first
	   VEC.  Use bsf to test here so we can reuse
	   L(first_vec_x0_ret).  */
	bsf	%VRAX, %VRAX
	jnz	L(first_vec_x0_ret)
# endif

L(page_cross_continue):
# ifdef USE_AS_WMEMCHR
	/* We can't use end of the buffer to re-calculate length for
	   wmemchr as len * CHAR_SIZE may overflow.  */
	leaq	-(VEC_SIZE + CHAR_SIZE)(%rdi), %rax
	andq	$(VEC_SIZE * -1), %rdi
	subq	%rdi, %rax
	sarq	$2, %rax
	addq	%rdx, %rax
# else
	leaq	-(VEC_SIZE + 1)(%rdx, %rdi), %rax
	andq	$(VEC_SIZE * -1), %rdi
	subq	%rdi, %rax
# endif

	/* rax contains remaining length - 1.  -1 so we can get imm8
	   encoding in a few additional places saving code size.  */

	/* Needed regardless of remaining length.  */
	VPCMPEQ	VEC_SIZE(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRDX

	/* We cannot fold the above `sub %rdi, %rax` with the `cmp
	   $(CHAR_PER_VEC * 2), %rax` because its possible for a very
	   large length to overflow and cause the subtract to carry
	   despite length being above CHAR_PER_VEC * 2.  */
	cmpq	$(CHAR_PER_VEC * 2 - 1), %rax
	ja	L(more_2x_vec)
L(last_2x_vec):

	test	%VRDX, %VRDX
	jnz	L(first_vec_x1_check)

	/* Check the end of data.  NB: use 8-bit operations to save code
	   size.  We no longer need the full-width of eax and will
	   perform a write-only operation over eax so there will be no
	   partial-register stalls.  */
	subb	$(CHAR_PER_VEC * 1 - 1), %al
	jle	L(zero_0)

	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRCX
# ifdef USE_AS_WMEMCHR
	/* For wmemchr against we can't take advantage of tzcnt(0) ==
	   VEC_SIZE as CHAR_PER_VEC != VEC_SIZE.  */
	test	%VRCX, %VRCX
	jz	L(zero_0)
# endif
	tzcnt	%VRCX, %VRCX
	cmp	%cl, %al

	/* Same CFG for VEC_SIZE == 64 and VEC_SIZE == 32.  We give
	   fallthrough to L(zero_0) for VEC_SIZE == 64 here as there is
	   not enough space before the next cache line to fit the `lea`
	   for return.  */
# if VEC_SIZE == 64
	ja	L(first_vec_x2_ret)
L(zero_0):
	xorl	%eax, %eax
	ret
# else
	jbe	L(zero_0)
	leaq	(VEC_SIZE * 2)(%rdi, %rcx, CHAR_SIZE), %rax
	ret
# endif

	.p2align 4,, 5
L(first_vec_x1_check):
	bsf	%VRDX, %VRDX
	cmpb	%dl, %al
	jb	L(zero_4)
	leaq	(VEC_SIZE * 1)(%rdi, %rdx, CHAR_SIZE), %rax
	ret

	/* Fits at the end of the cache line here for VEC_SIZE == 32.
	 */
# if VEC_SIZE == 32
L(zero_4):
	xorl	%eax, %eax
	ret
# endif


	.p2align 4,, 4
L(first_vec_x2):
	bsf	%VRCX, %VRCX
L(first_vec_x2_ret):
	leaq	(VEC_SIZE * 2)(%rdi, %rcx, CHAR_SIZE), %rax
	ret

	/* Fits at the end of the cache line here for VEC_SIZE == 64.
	 */
# if VEC_SIZE == 64
L(zero_4):
	xorl	%eax, %eax
	ret
# endif

	.p2align 4,, 4
L(first_vec_x1):
	bsf	%VRDX, %VRDX
	leaq	(VEC_SIZE * 1)(%rdi, %rdx, CHAR_SIZE), %rax
	ret


	.p2align 4,, 5
L(more_2x_vec):
	/* Length > VEC_SIZE * 2 so check first 2x VEC before rechecking
	   length.  */


	/* Already computed matches for first VEC in rdx.  */
	test	%VRDX, %VRDX
	jnz	L(first_vec_x1)


	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(first_vec_x2)

	/* Needed regardless of next length check.  */
	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRCX

	/* Check if we are near the end.  */
	cmpq	$(CHAR_PER_VEC * 4 - 1), %rax
	ja	L(more_4x_vec)

	test	%VRCX, %VRCX
	jnz	L(first_vec_x3_check)

	/* Use 8-bit instructions to save code size.  We won't use full-
	   width eax again and will perform a write-only operation to
	   eax so no worries about partial-register stalls.  */
	subb	$(CHAR_PER_VEC * 3), %al
	jb	L(zero_2)
L(last_vec_check):
	VPCMPEQ	(VEC_SIZE * 4)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRCX
# ifdef USE_AS_WMEMCHR
	/* For wmemchr against we can't take advantage of tzcnt(0) ==
	   VEC_SIZE as CHAR_PER_VEC != VEC_SIZE.  */
	test	%VRCX, %VRCX
	jz	L(zero_2)
# endif
	tzcnt	%VRCX, %VRCX
	cmp	%cl, %al
	jae	L(first_vec_x4_ret)
L(zero_2):
	xorl	%eax, %eax
	ret

	/* Fits at the end of the cache line here for VEC_SIZE == 64.
	   For VEC_SIZE == 32 we put the return label at the end of
	   L(first_vec_x4).  */
# if VEC_SIZE == 64
L(first_vec_x4_ret):
	leaq	(VEC_SIZE * 4)(%rdi, %rcx, CHAR_SIZE), %rax
	ret
# endif

	.p2align 4,, 6
L(first_vec_x4):
	bsf	%VRCX, %VRCX
# if VEC_SIZE == 32
	/* Place L(first_vec_x4_ret) here as we can't fit it in the same
	   cache line as where it is called from so we might as well
	   save code size by reusing return of L(first_vec_x4).  */
L(first_vec_x4_ret):
# endif
	leaq	(VEC_SIZE * 4)(%rdi, %rcx, CHAR_SIZE), %rax
	ret

	.p2align 4,, 6
L(first_vec_x3_check):
	/* Need to adjust remaining length before checking.  */
	addb	$-(CHAR_PER_VEC * 2), %al
	bsf	%VRCX, %VRCX
	cmpb	%cl, %al
	jb	L(zero_2)
	leaq	(VEC_SIZE * 3)(%rdi, %rcx, CHAR_SIZE), %rax
	ret

	.p2align 4,, 6
L(first_vec_x3):
	bsf	%VRCX, %VRCX
	leaq	(VEC_SIZE * 3)(%rdi, %rcx, CHAR_SIZE), %rax
	ret

	.p2align 4,, 3
# if !USE_TERN_IN_LOOP
	.p2align 4,, 10
# endif
L(more_4x_vec):
	test	%VRCX, %VRCX
	jnz	L(first_vec_x3)

	VPCMPEQ	(VEC_SIZE * 4)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(first_vec_x4)

	subq	$-(VEC_SIZE * 5), %rdi
	subq	$(CHAR_PER_VEC * 8), %rax
	jb	L(last_4x_vec)

# ifdef USE_AS_WMEMCHR
	movl	%edi, %ecx
# else
	addq	%rdi, %rax
# endif


# if VEC_SIZE == 64
	/* use xorb to do `andq $-(VEC_SIZE * 4), %rdi`. No evex
	   processor has partial register stalls (all have merging
	   uop). If that changes this can be removed.  */
	xorb	%dil, %dil
# else
	andq	$-(VEC_SIZE * 4), %rdi
# endif

# ifdef USE_AS_WMEMCHR
	subl	%edi, %ecx
	sarl	$2, %ecx
	addq	%rcx, %rax
# else
	subq	%rdi, %rax
# endif



# if USE_TERN_IN_LOOP
	/* copy VMATCH to low ymm so we can use vpcmpeq which is not
	   encodable with EVEX registers.  NB: this is VEC_SIZE == 32
	   only as there is no way to encode vpcmpeq with zmm0-15.  */
	vmovdqa64 %VMATCH, %VMATCH_LO
# endif

	.p2align 4,, 11
L(loop_4x_vec):
	/* Two versions of the loop.  One that does not require
	   vzeroupper by not using ymmm0-15 and another does that
	   require vzeroupper because it uses ymmm0-15.  The reason why
	   ymm0-15 is used at all is because there is no EVEX encoding
	   vpcmpeq and with vpcmpeq this loop can be performed more
	   efficiently.  The non-vzeroupper version is safe for RTM
	   while the vzeroupper version should be preferred if RTM are
	   not supported.   Which loop version we use is determined by
	   USE_TERN_IN_LOOP.  */

# if USE_TERN_IN_LOOP
	/* Since vptern can only take 3x vectors fastest to do 1 vec
	   separately with EVEX vpcmp.  */
#  ifdef USE_AS_WMEMCHR
	/* vptern can only accept masks for epi32/epi64 so can only save
	   instruction using not equals mask on vptern with wmemchr.
	 */
	VPCMP	$4, (VEC_SIZE * 0)(%rdi), %VMATCH, %k1
#  else
	VPCMPEQ	(VEC_SIZE * 0)(%rdi), %VMATCH, %k1
#  endif
	/* Compare 3x with vpcmpeq and or them all together with vptern.
	 */
	VPCMPEQ	(VEC_SIZE * 1)(%rdi), %VMATCH_LO, %VMM_lo(2)
	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VMATCH_LO, %VMM_lo(3)
	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VMATCH_LO, %VMM_lo(4)
#  ifdef USE_AS_WMEMCHR
	/* This takes the not of or between VEC_lo(2), VEC_lo(3),
	   VEC_lo(4) as well as combines result from VEC(0) with zero
	   mask.  */
	vpternlogd $1, %VMM_lo(2), %VMM_lo(3), %VMM_lo(4){%k1}{z}
	vpmovmskb %VMM_lo(4), %VRCX
#  else
	/* 254 is mask for oring VEC_lo(2), VEC_lo(3), VEC_lo(4) into
	   VEC_lo(4).  */
	vpternlogd $254, %VMM_lo(2), %VMM_lo(3), %VMM_lo(4)
	vpmovmskb %VMM_lo(4), %VRCX
	KMOV	%k1, %edx
#  endif

# else
	/* Loop version that uses EVEX encoding.  */
	VPCMP	$4, (VEC_SIZE * 0)(%rdi), %VMATCH, %k1
	vpxorq	(VEC_SIZE * 1)(%rdi), %VMATCH, %VMM(2)
	vpxorq	(VEC_SIZE * 2)(%rdi), %VMATCH, %VMM(3)
	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VMATCH, %k3
	VPMINU	%VMM(2), %VMM(3), %VMM(3){%k1}{z}
	VPTESTN	%VMM(3), %VMM(3), %k2
# endif


	TEST_END ()
	jnz	L(loop_vec_ret)

	subq	$-(VEC_SIZE * 4), %rdi

	subq	$(CHAR_PER_VEC * 4), %rax
	jae	L(loop_4x_vec)

	/* COND_VZEROUPPER is vzeroupper if we use the VEX encoded loop.
	 */
	COND_VZEROUPPER

	.p2align 4,, 10
L(last_4x_vec):
	/* For CHAR_PER_VEC == 64 we don't need to mask as we use 8-bit
	   instructions on eax from here on out.  */
# if CHAR_PER_VEC != 64
	andl	$(CHAR_PER_VEC * 4 - 1), %eax
# endif
	VPCMPEQ	(VEC_SIZE * 0)(%rdi), %VMATCH, %k0
	subq	$(VEC_SIZE * 1), %rdi
	KMOV	%k0, %VRDX
	cmpb	$(CHAR_PER_VEC * 2 - 1), %al
	jbe	L(last_2x_vec)
	test	%VRDX, %VRDX
	jnz	L(last_vec_x1_novzero)

	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_x2_novzero)

	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VMATCH, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(first_vec_x3_check)

	subb	$(CHAR_PER_VEC * 3), %al
	jae	L(last_vec_check)

	xorl	%eax, %eax
	ret

# if defined USE_AS_WMEMCHR && USE_TERN_IN_LOOP
L(last_vec_x2_novzero):
	addq	$VEC_SIZE, %rdi
L(last_vec_x1_novzero):
	bsf	%VRDX, %VRDX
	leaq	(VEC_SIZE * 1)(%rdi, %rdx, CHAR_SIZE), %rax
	ret
# endif

# if CHAR_PER_VEC == 64
	/* Since we can't combine the last 2x VEC when CHAR_PER_VEC ==
	   64 it needs a separate return label.  */
	.p2align 4,, 4
L(last_vec_x2):
L(last_vec_x2_novzero):
	bsf	%VRDX, %VRDX
	leaq	(VEC_SIZE * 2)(%rdi, %rdx, TERN_CHAR_MULT), %rax
	ret
# endif

	.p2align 4,, 4
L(loop_vec_ret):
# if defined USE_AS_WMEMCHR || !USE_TERN_IN_LOOP
	KMOV	%k1, %VRAX
	inc	%MASK_GPR(rax)
# else
	test	%VRDX, %VRDX
# endif
	jnz	L(last_vec_x0)


# if USE_TERN_IN_LOOP
	vpmovmskb %VMM_lo(2), %VRDX
# else
	VPTESTN	%VMM(2), %VMM(2), %k1
	KMOV	%k1, %VRDX
# endif
	test	%VRDX, %VRDX
	jnz	L(last_vec_x1)


# if USE_TERN_IN_LOOP
	vpmovmskb %VMM_lo(3), %VRDX
# else
	KMOV	%k2, %VRDX
# endif

	/* No longer need any of the lo vecs (ymm0-15) so vzeroupper
	   (only if used VEX encoded loop).  */
	COND_VZEROUPPER

	/* Separate logic for CHAR_PER_VEC == 64 vs the rest.  For
	   CHAR_PER_VEC we test the last 2x VEC separately, for
	   CHAR_PER_VEC <= 32 we can combine the results from the 2x
	   VEC in a single GPR.  */
# if CHAR_PER_VEC == 64
#  if USE_TERN_IN_LOOP
#   error "Unsupported"
#  endif


	/* If CHAR_PER_VEC == 64 we can't combine the last two VEC.  */
	test	%VRDX, %VRDX
	jnz	L(last_vec_x2)
	KMOV	%k3, %VRDX
# else
	/* CHAR_PER_VEC <= 32 so we can combine the results from the
	   last 2x VEC.  */

#  if !USE_TERN_IN_LOOP
	KMOV	%k3, %VRCX
#  endif
	salq	$(VEC_SIZE / TERN_CHAR_MULT), %rcx
	addq	%rcx, %rdx
#  if !defined USE_AS_WMEMCHR || !USE_TERN_IN_LOOP
L(last_vec_x2_novzero):
#  endif
# endif
	bsf	%rdx, %rdx
	leaq	(LAST_VEC_OFFSET)(%rdi, %rdx, TERN_CHAR_MULT), %rax
	ret

	.p2align 4,, 8
L(last_vec_x1):
	COND_VZEROUPPER
# if !defined USE_AS_WMEMCHR || !USE_TERN_IN_LOOP
L(last_vec_x1_novzero):
# endif
	bsf	%VRDX, %VRDX
	leaq	(VEC_SIZE * 1)(%rdi, %rdx, TERN_CHAR_MULT), %rax
	ret


	.p2align 4,, 4
L(last_vec_x0):
	COND_VZEROUPPER
	bsf	%VGPR(GPR_X0), %VGPR(GPR_X0)
# if GPR_X0_IS_RET
	addq	%rdi, %rax
# else
	leaq	(%rdi, %GPR_X0, CHAR_SIZE), %rax
# endif
	ret

	.p2align 4,, 6
L(page_cross):
	/* Need to preserve eax to compute inbound bytes we are
	   checking.  */
# ifdef USE_AS_WMEMCHR
	movl	%eax, %ecx
# else
	xorl	%ecx, %ecx
	subl	%eax, %ecx
# endif

	xorq	%rdi, %rax
	VPCMPEQ	(PAGE_SIZE - VEC_SIZE)(%rax), %VMATCH, %k0
	KMOV	%k0, %VRAX

# ifdef USE_AS_WMEMCHR
	/* NB: Divide by CHAR_SIZE to shift out out of bounds bytes.  */
	shrl	$2, %ecx
	andl	$(CHAR_PER_VEC - 1), %ecx
# endif


	shrx	%VGPR(PC_SHIFT_GPR), %VRAX, %VRAX

# ifdef USE_AS_WMEMCHR
	negl	%ecx
# endif

	/* mask lower bits from ecx (negative eax) to get bytes till
	   next VEC.  */
	andl	$(CHAR_PER_VEC - 1), %ecx

	/* Check if VEC is entirely contained in the remainder of the
	   page.  */
	cmpq	%rcx, %rdx
	jbe	L(page_cross_ret)

	/* Length crosses the page so if rax is zero (no matches)
	   continue.  */
	test	%VRAX, %VRAX
	jz	L(page_cross_continue)

	/* if rdx > rcx then any match here must be in [buf:buf + len].
	 */
	tzcnt	%VRAX, %VRAX
# ifdef USE_AS_WMEMCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	addq	%rdi, %rax
# endif
	ret

	.p2align 4,, 2
L(page_cross_zero):
	xorl	%eax, %eax
	ret

	.p2align 4,, 4
L(page_cross_ret):
	/* Search is entirely contained in page cross case.  */
# ifdef USE_AS_WMEMCHR
	test	%VRAX, %VRAX
	jz	L(page_cross_zero)
# endif
	tzcnt	%VRAX, %VRAX
	cmpl	%eax, %edx
	jbe	L(page_cross_zero)
# ifdef USE_AS_WMEMCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	addq	%rdi, %rax
# endif
	ret
END (MEMCHR)
#endif
