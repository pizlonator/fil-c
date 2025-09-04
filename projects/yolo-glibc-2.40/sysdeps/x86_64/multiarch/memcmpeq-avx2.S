/* __memcmpeq optimized with AVX2.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

#if ISA_SHOULD_BUILD (3)

/* __memcmpeq is implemented as:
   1. Use ymm vector compares when possible. The only case where
      vector compares is not possible for when size < VEC_SIZE
      and loading from either s1 or s2 would cause a page cross.
   2. Use xmm vector compare when size >= 8 bytes.
   3. Optimistically compare up to first 4 * VEC_SIZE one at a
      to check for early mismatches. Only do this if its guaranteed the
      work is not wasted.
   4. If size is 8 * VEC_SIZE or less, unroll the loop.
   5. Compare 4 * VEC_SIZE at a time with the aligned first memory
      area.
   6. Use 2 vector compares when size is 2 * VEC_SIZE or less.
   7. Use 4 vector compares when size is 4 * VEC_SIZE or less.
   8. Use 8 vector compares when size is 8 * VEC_SIZE or less.  */

# include <sysdep.h>

# ifndef MEMCMPEQ
#  define MEMCMPEQ	__memcmpeq_avx2
# endif

# define VPCMPEQ	vpcmpeqb

# ifndef VZEROUPPER
#  define VZEROUPPER	vzeroupper
# endif

# ifndef SECTION
#  define SECTION(p)	p##.avx
# endif

# define VEC_SIZE 32
# define PAGE_SIZE	4096

	.section SECTION(.text), "ax", @progbits
ENTRY_P2ALIGN (MEMCMPEQ, 6)
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	cmp	$VEC_SIZE, %RDX_LP
	jb	L(less_vec)

	/* From VEC to 2 * VEC.  No branch when size == VEC_SIZE.  */
	vmovdqu	(%rsi), %ymm1
	VPCMPEQ	(%rdi), %ymm1, %ymm1
	vpmovmskb %ymm1, %eax
	incl	%eax
	jnz	L(return_neq0)
	cmpq	$(VEC_SIZE * 2), %rdx
	jbe	L(last_1x_vec)

	/* Check second VEC no matter what.  */
	vmovdqu	VEC_SIZE(%rsi), %ymm2
	VPCMPEQ	VEC_SIZE(%rdi), %ymm2, %ymm2
	vpmovmskb %ymm2, %eax
	/* If all 4 VEC where equal eax will be all 1s so incl will overflow
	   and set zero flag.  */
	incl	%eax
	jnz	L(return_neq0)

	/* Less than 4 * VEC.  */
	cmpq	$(VEC_SIZE * 4), %rdx
	jbe	L(last_2x_vec)

	/* Check third and fourth VEC no matter what.  */
	vmovdqu	(VEC_SIZE * 2)(%rsi), %ymm3
	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %ymm3, %ymm3
	vpmovmskb %ymm3, %eax
	incl	%eax
	jnz	L(return_neq0)

	vmovdqu	(VEC_SIZE * 3)(%rsi), %ymm4
	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %ymm4, %ymm4
	vpmovmskb %ymm4, %eax
	incl	%eax
	jnz	L(return_neq0)

	/* Go to 4x VEC loop.  */
	cmpq	$(VEC_SIZE * 8), %rdx
	ja	L(more_8x_vec)

	/* Handle remainder of size = 4 * VEC + 1 to 8 * VEC without any
	   branches.  */

	/* Adjust rsi and rdi to avoid indexed address mode. This end up
	   saving a 16 bytes of code, prevents unlamination, and bottlenecks in
	   the AGU.  */
	addq	%rdx, %rsi
	vmovdqu	-(VEC_SIZE * 4)(%rsi), %ymm1
	vmovdqu	-(VEC_SIZE * 3)(%rsi), %ymm2
	addq	%rdx, %rdi

	VPCMPEQ	-(VEC_SIZE * 4)(%rdi), %ymm1, %ymm1
	VPCMPEQ	-(VEC_SIZE * 3)(%rdi), %ymm2, %ymm2

	vmovdqu	-(VEC_SIZE * 2)(%rsi), %ymm3
	VPCMPEQ	-(VEC_SIZE * 2)(%rdi), %ymm3, %ymm3
	vmovdqu	-VEC_SIZE(%rsi), %ymm4
	VPCMPEQ	-VEC_SIZE(%rdi), %ymm4, %ymm4

	/* Reduce VEC0 - VEC4.  */
	vpand	%ymm1, %ymm2, %ymm2
	vpand	%ymm3, %ymm4, %ymm4
	vpand	%ymm2, %ymm4, %ymm4
	vpmovmskb %ymm4, %eax
	incl	%eax
L(return_neq0):
L(return_vzeroupper):
	ZERO_UPPER_VEC_REGISTERS_RETURN

	/* NB: p2align 5 here will ensure the L(loop_4x_vec) is also 32 byte
	   aligned.  */
	.p2align 5
L(less_vec):
	/* Check if one or less char. This is necessary for size = 0 but is
	   also faster for size = 1.  */
	cmpl	$1, %edx
	jbe	L(one_or_less)

	/* Check if loading one VEC from either s1 or s2 could cause a page
	   cross. This can have false positives but is by far the fastest
	   method.  */
	movl	%edi, %eax
	orl	%esi, %eax
	andl	$(PAGE_SIZE - 1), %eax
	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
	jg	L(page_cross_less_vec)

	/* No page cross possible.  */
	vmovdqu	(%rsi), %ymm2
	VPCMPEQ	(%rdi), %ymm2, %ymm2
	vpmovmskb %ymm2, %eax
	incl	%eax
	/* Result will be zero if s1 and s2 match. Otherwise first set bit
	   will be first mismatch.  */
	bzhil	%edx, %eax, %eax
	VZEROUPPER_RETURN

	/* Relatively cold but placing close to L(less_vec) for 2 byte jump
	   encoding.  */
	.p2align 4
L(one_or_less):
	jb	L(zero)
	movzbl	(%rsi), %ecx
	movzbl	(%rdi), %eax
	subl	%ecx, %eax
	/* No ymm register was touched.  */
	ret
	/* Within the same 16 byte block is L(one_or_less).  */
L(zero):
	xorl	%eax, %eax
	ret

	.p2align 4
L(last_1x_vec):
	vmovdqu	-(VEC_SIZE * 1)(%rsi, %rdx), %ymm1
	VPCMPEQ	-(VEC_SIZE * 1)(%rdi, %rdx), %ymm1, %ymm1
	vpmovmskb %ymm1, %eax
	incl	%eax
	VZEROUPPER_RETURN

	.p2align 4
L(last_2x_vec):
	vmovdqu	-(VEC_SIZE * 2)(%rsi, %rdx), %ymm1
	VPCMPEQ	-(VEC_SIZE * 2)(%rdi, %rdx), %ymm1, %ymm1
	vmovdqu	-(VEC_SIZE * 1)(%rsi, %rdx), %ymm2
	VPCMPEQ	-(VEC_SIZE * 1)(%rdi, %rdx), %ymm2, %ymm2
	vpand	%ymm1, %ymm2, %ymm2
	vpmovmskb %ymm2, %eax
	incl	%eax
	VZEROUPPER_RETURN

	.p2align 4
L(more_8x_vec):
	/* Set end of s1 in rdx.  */
	leaq	-(VEC_SIZE * 4)(%rdi, %rdx), %rdx
	/* rsi stores s2 - s1. This allows loop to only update one pointer.
	 */
	subq	%rdi, %rsi
	/* Align s1 pointer.  */
	andq	$-VEC_SIZE, %rdi
	/* Adjust because first 4x vec where check already.  */
	subq	$-(VEC_SIZE * 4), %rdi
	.p2align 4
L(loop_4x_vec):
	/* rsi has s2 - s1 so get correct address by adding s1 (in rdi).  */
	vmovdqu	(%rsi, %rdi), %ymm1
	VPCMPEQ	(%rdi), %ymm1, %ymm1

	vmovdqu	VEC_SIZE(%rsi, %rdi), %ymm2
	VPCMPEQ	VEC_SIZE(%rdi), %ymm2, %ymm2

	vmovdqu	(VEC_SIZE * 2)(%rsi, %rdi), %ymm3
	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %ymm3, %ymm3

	vmovdqu	(VEC_SIZE * 3)(%rsi, %rdi), %ymm4
	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %ymm4, %ymm4

	vpand	%ymm1, %ymm2, %ymm2
	vpand	%ymm3, %ymm4, %ymm4
	vpand	%ymm2, %ymm4, %ymm4
	vpmovmskb %ymm4, %eax
	incl	%eax
	jnz	L(return_neq1)
	subq	$-(VEC_SIZE * 4), %rdi
	/* Check if s1 pointer at end.  */
	cmpq	%rdx, %rdi
	jb	L(loop_4x_vec)

	vmovdqu	(VEC_SIZE * 3)(%rsi, %rdx), %ymm4
	VPCMPEQ	(VEC_SIZE * 3)(%rdx), %ymm4, %ymm4
	subq	%rdx, %rdi
	/* rdi has 4 * VEC_SIZE - remaining length.  */
	cmpl	$(VEC_SIZE * 3), %edi
	jae	L(8x_last_1x_vec)
	/* Load regardless of branch.  */
	vmovdqu	(VEC_SIZE * 2)(%rsi, %rdx), %ymm3
	VPCMPEQ	(VEC_SIZE * 2)(%rdx), %ymm3, %ymm3
	cmpl	$(VEC_SIZE * 2), %edi
	jae	L(8x_last_2x_vec)
	/* Check last 4 VEC.  */
	vmovdqu	VEC_SIZE(%rsi, %rdx), %ymm1
	VPCMPEQ	VEC_SIZE(%rdx), %ymm1, %ymm1

	vmovdqu	(%rsi, %rdx), %ymm2
	VPCMPEQ	(%rdx), %ymm2, %ymm2

	vpand	%ymm3, %ymm4, %ymm4
	vpand	%ymm1, %ymm2, %ymm3
L(8x_last_2x_vec):
	vpand	%ymm3, %ymm4, %ymm4
L(8x_last_1x_vec):
	vpmovmskb %ymm4, %eax
	/* Restore s1 pointer to rdi.  */
	incl	%eax
L(return_neq1):
	VZEROUPPER_RETURN

	/* Relatively cold case as page cross are unexpected.  */
	.p2align 4
L(page_cross_less_vec):
	cmpl	$16, %edx
	jae	L(between_16_31)
	cmpl	$8, %edx
	ja	L(between_9_15)
	cmpl	$4, %edx
	jb	L(between_2_3)
	/* From 4 to 8 bytes.  No branch when size == 4.  */
	movl	(%rdi), %eax
	subl	(%rsi), %eax
	movl	-4(%rdi, %rdx), %ecx
	movl	-4(%rsi, %rdx), %edi
	subl	%edi, %ecx
	orl	%ecx, %eax
	ret

	.p2align 4,, 8
L(between_16_31):
	/* From 16 to 31 bytes.  No branch when size == 16.  */

	/* Safe to use xmm[0, 15] as no vzeroupper is needed so RTM safe.
	 */
	vmovdqu	(%rsi), %xmm1
	vpcmpeqb (%rdi), %xmm1, %xmm1
	vmovdqu	-16(%rsi, %rdx), %xmm2
	vpcmpeqb -16(%rdi, %rdx), %xmm2, %xmm2
	vpand	%xmm1, %xmm2, %xmm2
	vpmovmskb %xmm2, %eax
	notw	%ax
	/* No ymm register was touched.  */
	ret

	.p2align 4,, 8
L(between_9_15):
	/* From 9 to 15 bytes.  */
	movq	(%rdi), %rax
	subq	(%rsi), %rax
	movq	-8(%rdi, %rdx), %rcx
	movq	-8(%rsi, %rdx), %rdi
	subq	%rdi, %rcx
	orq	%rcx, %rax
	/* edx is guaranteed to be a non-zero int.  */
	cmovnz	%edx, %eax
	ret

	/* Don't align. This is cold and aligning here will cause code
	   to spill into next cache line.  */
L(between_2_3):
	/* From 2 to 3 bytes.  No branch when size == 2.  */
	movzwl	(%rdi), %eax
	movzwl	(%rsi), %ecx
	subl	%ecx, %eax
	movzbl	-1(%rdi, %rdx), %ecx
	/* All machines that support evex will insert a "merging uop"
	   avoiding any serious partial register stalls.  */
	subb	-1(%rsi, %rdx), %cl
	orl	%ecx, %eax
	/* No ymm register was touched.  */
	ret

    /* 2 Bytes from next cache line. */
END (MEMCMPEQ)
#endif
