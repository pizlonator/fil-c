/* memmove/memcpy/mempcpy with unaligned load/store and rep movsb
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

/* memmove/memcpy/mempcpy is implemented as:
   1. Use overlapping load and store to avoid branch.
   2. Load all sources into registers and store them together to avoid
      possible address overlap between source and destination.
   3. If size is 8 * VEC_SIZE or less, load all sources into registers
      and store them together.
   4. If address of destination > address of source, backward copy
      4 * VEC_SIZE at a time with unaligned load and aligned store.
      Load the first 4 * VEC and last VEC before the loop and store
      them after the loop to support overlapping addresses.
   5. Otherwise, forward copy 4 * VEC_SIZE at a time with unaligned
      load and aligned store.  Load the last 4 * VEC and first VEC
      before the loop and store them after the loop to support
      overlapping addresses.
   6. On machines with ERMS feature, if size greater than equal or to
      __x86_rep_movsb_threshold and less than
      __x86_rep_movsb_stop_threshold, then REP MOVSB will be used.
   7. If size >= __x86_shared_non_temporal_threshold and there is no
      overlap between destination and source, use non-temporal store
      instead of aligned store copying from either 2 or 4 pages at
      once.
   8. For point 7) if size < 16 * __x86_shared_non_temporal_threshold
      and source and destination do not page alias, copy from 2 pages
      at once using non-temporal stores. Page aliasing in this case is
      considered true if destination's page alignment - sources' page
      alignment is less than 8 * VEC_SIZE.
   9. If size >= 16 * __x86_shared_non_temporal_threshold or source
      and destination do page alias copy from 4 pages at once using
      non-temporal stores.  */

#include <sysdep.h>

#ifndef MEMCPY_SYMBOL
# define MEMCPY_SYMBOL(p,s)		MEMMOVE_SYMBOL(p, s)
#endif

#ifndef MEMPCPY_SYMBOL
# define MEMPCPY_SYMBOL(p,s)		MEMMOVE_SYMBOL(p, s)
#endif

#ifndef MEMMOVE_CHK_SYMBOL
# define MEMMOVE_CHK_SYMBOL(p,s)	MEMMOVE_SYMBOL(p, s)
#endif

#ifndef VZEROUPPER
# if VEC_SIZE > 16
#  define VZEROUPPER vzeroupper
# else
#  define VZEROUPPER
# endif
#endif

/* Whether to align before movsb. Ultimately we want 64 byte
   align and not worth it to load 4x VEC for VEC_SIZE == 16.  */
#define ALIGN_MOVSB	(VEC_SIZE > 16)
/* Number of bytes to align movsb to.  */
#define MOVSB_ALIGN_TO	64

#define SMALL_MOV_SIZE	(MOV_SIZE <= 4)
#define LARGE_MOV_SIZE	(MOV_SIZE > 4)

#if SMALL_MOV_SIZE + LARGE_MOV_SIZE != 1
# error MOV_SIZE Unknown
#endif

#if LARGE_MOV_SIZE
# define SMALL_SIZE_OFFSET	(4)
#else
# define SMALL_SIZE_OFFSET	(0)
#endif

#ifndef PAGE_SIZE
# define PAGE_SIZE 4096
#endif

#if PAGE_SIZE != 4096
# error Unsupported PAGE_SIZE
#endif

#ifndef LOG_PAGE_SIZE
# define LOG_PAGE_SIZE 12
#endif

#if PAGE_SIZE != (1 << LOG_PAGE_SIZE)
# error Invalid LOG_PAGE_SIZE
#endif

/* Byte per page for large_memcpy inner loop.  */
#if VEC_SIZE == 64
# define LARGE_LOAD_SIZE (VEC_SIZE * 2)
#else
# define LARGE_LOAD_SIZE (VEC_SIZE * 4)
#endif

/* Amount to shift __x86_shared_non_temporal_threshold by for
   bound for memcpy_large_4x. This is essentially use to to
   indicate that the copy is far beyond the scope of L3
   (assuming no user config x86_non_temporal_threshold) and to
   use a more aggressively unrolled loop.  NB: before
   increasing the value also update initialization of
   x86_non_temporal_threshold.  */
#ifndef LOG_4X_MEMCPY_THRESH
# define LOG_4X_MEMCPY_THRESH 4
#endif

/* Avoid short distance rep movsb only with non-SSE vector.  */
#ifndef AVOID_SHORT_DISTANCE_REP_MOVSB
# define AVOID_SHORT_DISTANCE_REP_MOVSB (VEC_SIZE > 16)
#else
# define AVOID_SHORT_DISTANCE_REP_MOVSB 0
#endif

#ifndef PREFETCH
# define PREFETCH(addr) prefetcht0 addr
#endif

/* Assume 64-byte prefetch size.  */
#ifndef PREFETCH_SIZE
# define PREFETCH_SIZE 64
#endif

#define PREFETCHED_LOAD_SIZE (VEC_SIZE * 4)

#if PREFETCH_SIZE == 64
# if PREFETCHED_LOAD_SIZE == PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((offset)base)
# elif PREFETCHED_LOAD_SIZE == 2 * PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((offset)base); \
	PREFETCH ((offset + dir * PREFETCH_SIZE)base)
# elif PREFETCHED_LOAD_SIZE == 4 * PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((offset)base); \
	PREFETCH ((offset + dir * PREFETCH_SIZE)base); \
	PREFETCH ((offset + dir * PREFETCH_SIZE * 2)base); \
	PREFETCH ((offset + dir * PREFETCH_SIZE * 3)base)
# else
#   error Unsupported PREFETCHED_LOAD_SIZE!
# endif
#else
# error Unsupported PREFETCH_SIZE!
#endif

#if LARGE_LOAD_SIZE == (VEC_SIZE * 2)
# define LOAD_ONE_SET(base, offset, vec0, vec1, ...) \
	VMOVU	(offset)base, vec0; \
	VMOVU	((offset) + VEC_SIZE)base, vec1;
# define STORE_ONE_SET(base, offset, vec0, vec1, ...) \
	VMOVNT  vec0, (offset)base; \
	VMOVNT  vec1, ((offset) + VEC_SIZE)base;
#elif LARGE_LOAD_SIZE == (VEC_SIZE * 4)
# define LOAD_ONE_SET(base, offset, vec0, vec1, vec2, vec3) \
	VMOVU	(offset)base, vec0; \
	VMOVU	((offset) + VEC_SIZE)base, vec1; \
	VMOVU	((offset) + VEC_SIZE * 2)base, vec2; \
	VMOVU	((offset) + VEC_SIZE * 3)base, vec3;
# define STORE_ONE_SET(base, offset, vec0, vec1, vec2, vec3) \
	VMOVNT	vec0, (offset)base; \
	VMOVNT	vec1, ((offset) + VEC_SIZE)base; \
	VMOVNT	vec2, ((offset) + VEC_SIZE * 2)base; \
	VMOVNT	vec3, ((offset) + VEC_SIZE * 3)base;
#else
# error Invalid LARGE_LOAD_SIZE
#endif

#ifndef SECTION
# error SECTION is not defined!
#endif

	.section SECTION(.text),"ax",@progbits
#if defined SHARED && IS_IN (libc)
ENTRY (MEMMOVE_CHK_SYMBOL (__mempcpy_chk, unaligned))
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (MEMMOVE_CHK_SYMBOL (__mempcpy_chk, unaligned))
#endif

ENTRY (MEMPCPY_SYMBOL (__mempcpy, unaligned))
	mov	%RDI_LP, %RAX_LP
	add	%RDX_LP, %RAX_LP
	jmp	L(start)
END (MEMPCPY_SYMBOL (__mempcpy, unaligned))

#if defined SHARED && IS_IN (libc)
ENTRY (MEMMOVE_CHK_SYMBOL (__memmove_chk, unaligned))
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (MEMMOVE_CHK_SYMBOL (__memmove_chk, unaligned))
#endif

ENTRY (MEMMOVE_SYMBOL (__memmove, unaligned))
	movq	%rdi, %rax
L(start):
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	cmp	$VEC_SIZE, %RDX_LP
	jb	L(less_vec)
	/* Load regardless.  */
	VMOVU	(%rsi), %VMM(0)
	cmp	$(VEC_SIZE * 2), %RDX_LP
	ja	L(more_2x_vec)
	/* From VEC and to 2 * VEC.  No branch when size == VEC_SIZE.  */
	VMOVU	-VEC_SIZE(%rsi,%rdx), %VMM(1)
	VMOVU	%VMM(0), (%rdi)
	VMOVU	%VMM(1), -VEC_SIZE(%rdi,%rdx)
#if !(defined USE_MULTIARCH && IS_IN (libc))
	ZERO_UPPER_VEC_REGISTERS_RETURN
#else
	VZEROUPPER_RETURN
#endif
#if defined USE_MULTIARCH && IS_IN (libc)
END (MEMMOVE_SYMBOL (__memmove, unaligned))

# ifdef SHARED
ENTRY (MEMMOVE_CHK_SYMBOL (__mempcpy_chk, unaligned_erms))
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (MEMMOVE_CHK_SYMBOL (__mempcpy_chk, unaligned_erms))
# endif

ENTRY (MEMMOVE_SYMBOL (__mempcpy, unaligned_erms))
	mov	%RDI_LP, %RAX_LP
	add	%RDX_LP, %RAX_LP
	jmp	L(start_erms)
END (MEMMOVE_SYMBOL (__mempcpy, unaligned_erms))

# ifdef SHARED
ENTRY (MEMMOVE_CHK_SYMBOL (__memmove_chk, unaligned_erms))
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (MEMMOVE_CHK_SYMBOL (__memmove_chk, unaligned_erms))
# endif

ENTRY_P2ALIGN (MEMMOVE_SYMBOL (__memmove, unaligned_erms), 6)
	movq	%rdi, %rax
L(start_erms):
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	cmp	$VEC_SIZE, %RDX_LP
	jb	L(less_vec)
	/* Load regardless.  */
	VMOVU	(%rsi), %VMM(0)
	cmp	$(VEC_SIZE * 2), %RDX_LP
	ja	L(movsb_more_2x_vec)
	/* From VEC and to 2 * VEC.  No branch when size == VEC_SIZE.
	 */
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(1)
	VMOVU	%VMM(0), (%rdi)
	VMOVU	%VMM(1), -VEC_SIZE(%rdi, %rdx)
L(return_vzeroupper):
# if VEC_SIZE > 16
	ZERO_UPPER_VEC_REGISTERS_RETURN
# else
	ret
# endif
#endif

#if LARGE_MOV_SIZE
	/* If LARGE_MOV_SIZE this fits in the aligning bytes between the
	   ENTRY block and L(less_vec).  */
	.p2align 4,, 8
L(between_4_7):
	/* From 4 to 7.  No branch when size == 4.  */
	movl	(%rsi), %ecx
	movl	(%rsi, %rdx), %esi
	movl	%ecx, (%rdi)
	movl	%esi, (%rdi, %rdx)
	ret
#endif

	.p2align 4
L(less_vec):
	/* Less than 1 VEC.  */
#if VEC_SIZE != 16 && VEC_SIZE != 32 && VEC_SIZE != 64
# error Unsupported VEC_SIZE!
#endif
#if VEC_SIZE > 32
	cmpl	$32, %edx
	jae	L(between_32_63)
#endif
#if VEC_SIZE > 16
	cmpl	$16, %edx
	jae	L(between_16_31)
#endif
	cmpl	$8, %edx
	jae	L(between_8_15)
#if SMALL_MOV_SIZE
	cmpl	$4, %edx
#else
	subq	$4, %rdx
#endif
	jae	L(between_4_7)
	cmpl	$(1 - SMALL_SIZE_OFFSET), %edx
	jl	L(copy_0)
	movb	(%rsi), %cl
	je	L(copy_1)
	movzwl	(-2 + SMALL_SIZE_OFFSET)(%rsi, %rdx), %esi
	movw	%si, (-2 + SMALL_SIZE_OFFSET)(%rdi, %rdx)
L(copy_1):
	movb	%cl, (%rdi)
L(copy_0):
	ret

#if SMALL_MOV_SIZE
	.p2align 4,, 8
L(between_4_7):
	/* From 4 to 7.  No branch when size == 4.  */
	movl	-4(%rsi, %rdx), %ecx
	movl	(%rsi), %esi
	movl	%ecx, -4(%rdi, %rdx)
	movl	%esi, (%rdi)
	ret
#endif

#if VEC_SIZE > 16
	/* From 16 to 31.  No branch when size == 16.  */
	.p2align 4,, 8
L(between_16_31):
	vmovdqu	(%rsi), %xmm0
	vmovdqu	-16(%rsi, %rdx), %xmm1
	vmovdqu	%xmm0, (%rdi)
	vmovdqu	%xmm1, -16(%rdi, %rdx)
	/* No ymm registers have been touched.  */
	ret
#endif

#if VEC_SIZE > 32
	.p2align 4,, 10
L(between_32_63):
	/* From 32 to 63.  No branch when size == 32.  */
	VMOVU	(%rsi), %VMM_256(0)
	VMOVU	-32(%rsi, %rdx), %VMM_256(1)
	VMOVU	%VMM_256(0), (%rdi)
	VMOVU	%VMM_256(1), -32(%rdi, %rdx)
	VZEROUPPER_RETURN
#endif

	.p2align 4,, 10
L(between_8_15):
	/* From 8 to 15.  No branch when size == 8.  */
	movq	-8(%rsi, %rdx), %rcx
	movq	(%rsi), %rsi
	movq	%rsi, (%rdi)
	movq	%rcx, -8(%rdi, %rdx)
	ret

	.p2align 4,, 10
L(last_4x_vec):
	/* Copy from 2 * VEC + 1 to 4 * VEC, inclusively.  */

	/* VEC(0) and VEC(1) have already been loaded.  */
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(2)
	VMOVU	-(VEC_SIZE * 2)(%rsi, %rdx), %VMM(3)
	VMOVU	%VMM(0), (%rdi)
	VMOVU	%VMM(1), VEC_SIZE(%rdi)
	VMOVU	%VMM(2), -VEC_SIZE(%rdi, %rdx)
	VMOVU	%VMM(3), -(VEC_SIZE * 2)(%rdi, %rdx)
	VZEROUPPER_RETURN

	.p2align 4
#if defined USE_MULTIARCH && IS_IN (libc)
L(movsb_more_2x_vec):
	cmp	__x86_rep_movsb_threshold(%rip), %RDX_LP
	ja	L(movsb)
#endif
L(more_2x_vec):
	/* More than 2 * VEC and there may be overlap between
	   destination and source.  */
	cmpq	$(VEC_SIZE * 8), %rdx
	ja	L(more_8x_vec)
	/* Load VEC(1) regardless. VEC(0) has already been loaded.  */
	VMOVU	VEC_SIZE(%rsi), %VMM(1)
	cmpq	$(VEC_SIZE * 4), %rdx
	jbe	L(last_4x_vec)
	/* Copy from 4 * VEC + 1 to 8 * VEC, inclusively.  */
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(3)
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(4)
	VMOVU	-(VEC_SIZE * 2)(%rsi, %rdx), %VMM(5)
	VMOVU	-(VEC_SIZE * 3)(%rsi, %rdx), %VMM(6)
	VMOVU	-(VEC_SIZE * 4)(%rsi, %rdx), %VMM(7)
	VMOVU	%VMM(0), (%rdi)
	VMOVU	%VMM(1), VEC_SIZE(%rdi)
	VMOVU	%VMM(2), (VEC_SIZE * 2)(%rdi)
	VMOVU	%VMM(3), (VEC_SIZE * 3)(%rdi)
	VMOVU	%VMM(4), -VEC_SIZE(%rdi, %rdx)
	VMOVU	%VMM(5), -(VEC_SIZE * 2)(%rdi, %rdx)
	VMOVU	%VMM(6), -(VEC_SIZE * 3)(%rdi, %rdx)
	VMOVU	%VMM(7), -(VEC_SIZE * 4)(%rdi, %rdx)
	VZEROUPPER_RETURN

	.p2align 4,, 4
L(more_8x_vec):
	movq	%rdi, %rcx
	subq	%rsi, %rcx
	/* Go to backwards temporal copy if overlap no matter what as
	   backward REP MOVSB is slow and we don't want to use NT stores if
	   there is overlap.  */
	cmpq	%rdx, %rcx
	/* L(more_8x_vec_backward_check_nop) checks for src == dst.  */
	jb	L(more_8x_vec_backward_check_nop)
	/* Check if non-temporal move candidate.  */
#if (defined USE_MULTIARCH || VEC_SIZE == 16) && IS_IN (libc)
	/* Check non-temporal store threshold.  */
	cmp	__x86_shared_non_temporal_threshold(%rip), %RDX_LP
	ja	L(large_memcpy_2x)
#endif
	/* To reach this point there cannot be overlap and dst > src. So
	   check for overlap and src > dst in which case correctness
	   requires forward copy. Otherwise decide between backward/forward
	   copy depending on address aliasing.  */

	/* Entry if rdx is greater than __x86_rep_movsb_stop_threshold
	   but less than __x86_shared_non_temporal_threshold.  */
L(more_8x_vec_check):
	/* rcx contains dst - src. Add back length (rdx).  */
	leaq	(%rcx, %rdx), %r8
	/* If r8 has different sign than rcx then there is overlap so we
	   must do forward copy.  */
	xorq	%rcx, %r8
	/* Isolate just sign bit of r8.  */
	shrq	$63, %r8
	/* Get 4k difference dst - src.  */
	andl	$(PAGE_SIZE - 256), %ecx
	/* If r8 is non-zero must do forward for correctness. Otherwise
	   if ecx is non-zero there is 4k False Alaising so do backward
	   copy.  */
	addl	%r8d, %ecx
	jz	L(more_8x_vec_backward)

	/* if rdx is greater than __x86_shared_non_temporal_threshold
	   but there is overlap, or from short distance movsb.  */
L(more_8x_vec_forward):
	/* Load first and last 4 * VEC to support overlapping addresses.
	 */

	/* First vec was already loaded into VEC(0).  */
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(5)
	VMOVU	-(VEC_SIZE * 2)(%rsi, %rdx), %VMM(6)
	/* Save beginning of dst.  */
	movq	%rdi, %rcx
	/* Align dst to VEC_SIZE - 1.  */
	orq	$(VEC_SIZE - 1), %rdi
	VMOVU	-(VEC_SIZE * 3)(%rsi, %rdx), %VMM(7)
	VMOVU	-(VEC_SIZE * 4)(%rsi, %rdx), %VMM(8)

	/* Subtract dst from src. Add back after dst aligned.  */
	subq	%rcx, %rsi
	/* Finish aligning dst.  */
	incq	%rdi
	/* Restore src adjusted with new value for aligned dst.  */
	addq	%rdi, %rsi
	/* Store end of buffer minus tail in rdx.  */
	leaq	(VEC_SIZE * -4)(%rcx, %rdx), %rdx

	/* Dont use multi-byte nop to align.  */
	.p2align 4,, 11
L(loop_4x_vec_forward):
	/* Copy 4 * VEC a time forward.  */
	VMOVU	(%rsi), %VMM(1)
	VMOVU	VEC_SIZE(%rsi), %VMM(2)
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(3)
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(4)
	subq	$-(VEC_SIZE * 4), %rsi
	VMOVA	%VMM(1), (%rdi)
	VMOVA	%VMM(2), VEC_SIZE(%rdi)
	VMOVA	%VMM(3), (VEC_SIZE * 2)(%rdi)
	VMOVA	%VMM(4), (VEC_SIZE * 3)(%rdi)
	subq	$-(VEC_SIZE * 4), %rdi
	cmpq	%rdi, %rdx
	ja	L(loop_4x_vec_forward)
	/* Store the last 4 * VEC.  */
	VMOVU	%VMM(5), (VEC_SIZE * 3)(%rdx)
	VMOVU	%VMM(6), (VEC_SIZE * 2)(%rdx)
	VMOVU	%VMM(7), VEC_SIZE(%rdx)
	VMOVU	%VMM(8), (%rdx)
	/* Store the first VEC.  */
	VMOVU	%VMM(0), (%rcx)
	/* Keep L(nop_backward) target close to jmp for 2-byte encoding.
	 */
L(nop_backward):
	VZEROUPPER_RETURN

	.p2align 4,, 8
L(more_8x_vec_backward_check_nop):
	/* rcx contains dst - src. Test for dst == src to skip all of
	   memmove.  */
	testq	%rcx, %rcx
	jz	L(nop_backward)
L(more_8x_vec_backward):
	/* Load the first 4 * VEC and last VEC to support overlapping
	   addresses.  */

	/* First vec was also loaded into VEC(0).  */
	VMOVU	VEC_SIZE(%rsi), %VMM(5)
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(6)
	/* Beginning of region for 4x backward copy stored in rcx.  */
	leaq	(VEC_SIZE * -4 + -1)(%rdi, %rdx), %rcx
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(7)
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(8)
	/* Subtract dst from src. Add back after dst aligned.  */
	subq	%rdi, %rsi
	/* Align dst.  */
	andq	$-(VEC_SIZE), %rcx
	/* Restore src.  */
	addq	%rcx, %rsi

	/* Don't use multi-byte nop to align.  */
	.p2align 4,, 11
L(loop_4x_vec_backward):
	/* Copy 4 * VEC a time backward.  */
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(1)
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	(VEC_SIZE * 1)(%rsi), %VMM(3)
	VMOVU	(VEC_SIZE * 0)(%rsi), %VMM(4)
	addq	$(VEC_SIZE * -4), %rsi
	VMOVA	%VMM(1), (VEC_SIZE * 3)(%rcx)
	VMOVA	%VMM(2), (VEC_SIZE * 2)(%rcx)
	VMOVA	%VMM(3), (VEC_SIZE * 1)(%rcx)
	VMOVA	%VMM(4), (VEC_SIZE * 0)(%rcx)
	addq	$(VEC_SIZE * -4), %rcx
	cmpq	%rcx, %rdi
	jb	L(loop_4x_vec_backward)
	/* Store the first 4 * VEC.  */
	VMOVU	%VMM(0), (%rdi)
	VMOVU	%VMM(5), VEC_SIZE(%rdi)
	VMOVU	%VMM(6), (VEC_SIZE * 2)(%rdi)
	VMOVU	%VMM(7), (VEC_SIZE * 3)(%rdi)
	/* Store the last VEC.  */
	VMOVU	%VMM(8), -VEC_SIZE(%rdx, %rdi)
	VZEROUPPER_RETURN

#if defined USE_MULTIARCH && IS_IN (libc)
	/* L(skip_short_movsb_check) is only used with ERMS. Not for
	   FSRM.  */
	.p2align 5,, 16
# if ALIGN_MOVSB
L(skip_short_movsb_check):
#  if MOVSB_ALIGN_TO > VEC_SIZE
	VMOVU	VEC_SIZE(%rsi), %VMM(1)
#  endif
#  if MOVSB_ALIGN_TO > (VEC_SIZE * 2)
#   error Unsupported MOVSB_ALIGN_TO
#  endif
	/* If CPU does not have FSRM two options for aligning. Align src
	   if dst and src 4k alias. Otherwise align dst.  */
	testl	$(PAGE_SIZE - 512), %ecx
	jnz	L(movsb_align_dst)
	/* Fall through. dst and src 4k alias. It's better to align src
	   here because the bottleneck will be loads dues to the false
	   dependency on dst.  */

	/* rcx already has dst - src.  */
	movq	%rcx, %r9
	/* Add src to len. Subtract back after src aligned. -1 because
	   src is initially aligned to MOVSB_ALIGN_TO - 1.  */
	leaq	-1(%rsi, %rdx), %rcx
	/* Inclusively align src to MOVSB_ALIGN_TO - 1.  */
	orq	$(MOVSB_ALIGN_TO - 1), %rsi
	/* Restore dst and len adjusted with new values for aligned dst.
	 */
	leaq	1(%rsi, %r9), %rdi
	subq	%rsi, %rcx
	/* Finish aligning src.  */
	incq	%rsi

	rep	movsb

	VMOVU	%VMM(0), (%r8)
#  if MOVSB_ALIGN_TO > VEC_SIZE
	VMOVU	%VMM(1), VEC_SIZE(%r8)
#  endif
	VZEROUPPER_RETURN
# endif

	.p2align 4,, 12
L(movsb):
	movq	%rdi, %rcx
	subq	%rsi, %rcx
	/* Go to backwards temporal copy if overlap no matter what as
	   backward REP MOVSB is slow and we don't want to use NT stores if
	   there is overlap.  */
	cmpq	%rdx, %rcx
	/* L(more_8x_vec_backward_check_nop) checks for src == dst.  */
	jb	L(more_8x_vec_backward_check_nop)
# if ALIGN_MOVSB
	/* Save dest for storing aligning VECs later.  */
	movq	%rdi, %r8
# endif
	/* If above __x86_rep_movsb_stop_threshold most likely is
	   candidate for NT moves as well.  */
	cmp	__x86_rep_movsb_stop_threshold(%rip), %RDX_LP
	jae	L(large_memcpy_2x_check)
# if AVOID_SHORT_DISTANCE_REP_MOVSB || ALIGN_MOVSB
	/* Only avoid short movsb if CPU has FSRM.  */
#  if X86_STRING_CONTROL_AVOID_SHORT_DISTANCE_REP_MOVSB < 256
	testb	$X86_STRING_CONTROL_AVOID_SHORT_DISTANCE_REP_MOVSB, __x86_string_control(%rip)
#  else
	testl	$X86_STRING_CONTROL_AVOID_SHORT_DISTANCE_REP_MOVSB, __x86_string_control(%rip)
#  endif
	jz	L(skip_short_movsb_check)
#  if AVOID_SHORT_DISTANCE_REP_MOVSB
	/* Avoid "rep movsb" if RCX, the distance between source and
	   destination, is N*4GB + [1..63] with N >= 0.  */

	/* ecx contains dst - src. Early check for backward copy
	   conditions means only case of slow movsb with src = dst + [0,
	   63] is ecx in [-63, 0]. Use unsigned comparison with -64 check
	   for that case.  */
	cmpl	$-64, %ecx
	ja	L(more_8x_vec_forward)
#  endif
# endif
# if ALIGN_MOVSB
#  if MOVSB_ALIGN_TO > VEC_SIZE
	VMOVU	VEC_SIZE(%rsi), %VMM(1)
#  endif
#  if MOVSB_ALIGN_TO > (VEC_SIZE * 2)
#   error Unsupported MOVSB_ALIGN_TO
#  endif
	/* Fall through means cpu has FSRM. In that case exclusively
	   align destination.  */
L(movsb_align_dst):
	/* Subtract dst from src. Add back after dst aligned.  */
	subq	%rdi, %rsi
	/* Exclusively align dst to MOVSB_ALIGN_TO (64).  */
	addq	$(MOVSB_ALIGN_TO - 1), %rdi
	/* Add dst to len. Subtract back after dst aligned.  */
	leaq	(%r8, %rdx), %rcx
	/* Finish aligning dst.  */
	andq	$-(MOVSB_ALIGN_TO), %rdi
	/* Restore src and len adjusted with new values for aligned dst.
	 */
	addq	%rdi, %rsi
	subq	%rdi, %rcx

	rep	movsb

	/* Store VECs loaded for aligning.  */
	VMOVU	%VMM(0), (%r8)
#  if MOVSB_ALIGN_TO > VEC_SIZE
	VMOVU	%VMM(1), VEC_SIZE(%r8)
#  endif
	VZEROUPPER_RETURN
# else	/* !ALIGN_MOVSB.  */
L(skip_short_movsb_check):
	mov	%RDX_LP, %RCX_LP
	rep	movsb
	ret
# endif
#endif

	.p2align 4,, 10
#if (defined USE_MULTIARCH || VEC_SIZE == 16) && IS_IN (libc)
L(large_memcpy_2x_check):
	/* Entry from L(large_memcpy_2x) has a redundant load of
	   __x86_shared_non_temporal_threshold(%rip). L(large_memcpy_2x)
	   is only use for the non-erms memmove which is generally less
	   common.  */
L(large_memcpy_2x):
	mov	__x86_shared_non_temporal_threshold(%rip), %R11_LP
	cmp	%R11_LP, %RDX_LP
	jb	L(more_8x_vec_check)
	/* To reach this point it is impossible for dst > src and
	   overlap. Remaining to check is src > dst and overlap. rcx
	   already contains dst - src. Negate rcx to get src - dst. If
	   length > rcx then there is overlap and forward copy is best.  */
	negq	%rcx
	cmpq	%rcx, %rdx
	ja	L(more_8x_vec_forward)

	/* Cache align destination. First store the first 64 bytes then
	   adjust alignments.  */

	/* First vec was also loaded into VEC(0).  */
# if VEC_SIZE < 64
	VMOVU	VEC_SIZE(%rsi), %VMM(1)
#  if VEC_SIZE < 32
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(3)
#  endif
# endif
	VMOVU	%VMM(0), (%rdi)
# if VEC_SIZE < 64
	VMOVU	%VMM(1), VEC_SIZE(%rdi)
#  if VEC_SIZE < 32
	VMOVU	%VMM(2), (VEC_SIZE * 2)(%rdi)
	VMOVU	%VMM(3), (VEC_SIZE * 3)(%rdi)
#  endif
# endif

	/* Adjust source, destination, and size.  */
	movq	%rdi, %r8
	andq	$63, %r8
	/* Get the negative of offset for alignment.  */
	subq	$64, %r8
	/* Adjust source.  */
	subq	%r8, %rsi
	/* Adjust destination which should be aligned now.  */
	subq	%r8, %rdi
	/* Adjust length.  */
	addq	%r8, %rdx

	/* Test if source and destination addresses will alias. If they
	   do the larger pipeline in large_memcpy_4x alleviated the
	   performance drop.  */

	/* ecx contains -(dst - src). not ecx will return dst - src - 1
	   which works for testing aliasing.  */
	notl	%ecx
	movq	%rdx, %r10
	testl	$(PAGE_SIZE - VEC_SIZE * 8), %ecx
	jz	L(large_memcpy_4x)

	/* r11 has __x86_shared_non_temporal_threshold.  Shift it left
	   by LOG_4X_MEMCPY_THRESH to get L(large_memcpy_4x) threshold.
	 */
	shlq	$LOG_4X_MEMCPY_THRESH, %r11
	cmp	%r11, %rdx
	jae	L(large_memcpy_4x)

	/* edx will store remainder size for copying tail.  */
	andl	$(PAGE_SIZE * 2 - 1), %edx
	/* r10 stores outer loop counter.  */
	shrq	$(LOG_PAGE_SIZE + 1), %r10
	/* Copy 4x VEC at a time from 2 pages.  */
	.p2align 4
L(loop_large_memcpy_2x_outer):
	/* ecx stores inner loop counter.  */
	movl	$(PAGE_SIZE / LARGE_LOAD_SIZE), %ecx
L(loop_large_memcpy_2x_inner):
	PREFETCH_ONE_SET(1, (%rsi), PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET(1, (%rsi), PREFETCHED_LOAD_SIZE * 2)
	PREFETCH_ONE_SET(1, (%rsi), PAGE_SIZE + PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET(1, (%rsi), PAGE_SIZE + PREFETCHED_LOAD_SIZE * 2)
	/* Load vectors from rsi.  */
	LOAD_ONE_SET((%rsi), 0, %VMM(0), %VMM(1), %VMM(2), %VMM(3))
	LOAD_ONE_SET((%rsi), PAGE_SIZE, %VMM(4), %VMM(5), %VMM(6), %VMM(7))
	subq	$-LARGE_LOAD_SIZE, %rsi
	/* Non-temporal store vectors to rdi.  */
	STORE_ONE_SET((%rdi), 0, %VMM(0), %VMM(1), %VMM(2), %VMM(3))
	STORE_ONE_SET((%rdi), PAGE_SIZE, %VMM(4), %VMM(5), %VMM(6), %VMM(7))
	subq	$-LARGE_LOAD_SIZE, %rdi
	decl	%ecx
	jnz	L(loop_large_memcpy_2x_inner)
	addq	$PAGE_SIZE, %rdi
	addq	$PAGE_SIZE, %rsi
	decq	%r10
	jne	L(loop_large_memcpy_2x_outer)
	sfence

	/* Check if only last 4 loads are needed.  */
	cmpl	$(VEC_SIZE * 4), %edx
	jbe	L(large_memcpy_2x_end)

	/* Handle the last 2 * PAGE_SIZE bytes.  */
L(loop_large_memcpy_2x_tail):
	/* Copy 4 * VEC a time forward with non-temporal stores.  */
	PREFETCH_ONE_SET (1, (%rsi), PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET (1, (%rdi), PREFETCHED_LOAD_SIZE)
	VMOVU	(%rsi), %VMM(0)
	VMOVU	VEC_SIZE(%rsi), %VMM(1)
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(3)
	subq	$-(VEC_SIZE * 4), %rsi
	addl	$-(VEC_SIZE * 4), %edx
	VMOVA	%VMM(0), (%rdi)
	VMOVA	%VMM(1), VEC_SIZE(%rdi)
	VMOVA	%VMM(2), (VEC_SIZE * 2)(%rdi)
	VMOVA	%VMM(3), (VEC_SIZE * 3)(%rdi)
	subq	$-(VEC_SIZE * 4), %rdi
	cmpl	$(VEC_SIZE * 4), %edx
	ja	L(loop_large_memcpy_2x_tail)

L(large_memcpy_2x_end):
	/* Store the last 4 * VEC.  */
	VMOVU	-(VEC_SIZE * 4)(%rsi, %rdx), %VMM(0)
	VMOVU	-(VEC_SIZE * 3)(%rsi, %rdx), %VMM(1)
	VMOVU	-(VEC_SIZE * 2)(%rsi, %rdx), %VMM(2)
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(3)

	VMOVU	%VMM(0), -(VEC_SIZE * 4)(%rdi, %rdx)
	VMOVU	%VMM(1), -(VEC_SIZE * 3)(%rdi, %rdx)
	VMOVU	%VMM(2), -(VEC_SIZE * 2)(%rdi, %rdx)
	VMOVU	%VMM(3), -VEC_SIZE(%rdi, %rdx)
	VZEROUPPER_RETURN

	.p2align 4
L(large_memcpy_4x):
	/* edx will store remainder size for copying tail.  */
	andl	$(PAGE_SIZE * 4 - 1), %edx
	/* r10 stores outer loop counter.  */
	shrq	$(LOG_PAGE_SIZE + 2), %r10
	/* Copy 4x VEC at a time from 4 pages.  */
	.p2align 4
L(loop_large_memcpy_4x_outer):
	/* ecx stores inner loop counter.  */
	movl	$(PAGE_SIZE / LARGE_LOAD_SIZE), %ecx
L(loop_large_memcpy_4x_inner):
	/* Only one prefetch set per page as doing 4 pages give more
	   time for prefetcher to keep up.  */
	PREFETCH_ONE_SET(1, (%rsi), PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET(1, (%rsi), PAGE_SIZE + PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET(1, (%rsi), PAGE_SIZE * 2 + PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET(1, (%rsi), PAGE_SIZE * 3 + PREFETCHED_LOAD_SIZE)
	/* Load vectors from rsi.  */
	LOAD_ONE_SET((%rsi), 0, %VMM(0), %VMM(1), %VMM(2), %VMM(3))
	LOAD_ONE_SET((%rsi), PAGE_SIZE, %VMM(4), %VMM(5), %VMM(6), %VMM(7))
	LOAD_ONE_SET((%rsi), PAGE_SIZE * 2, %VMM(8), %VMM(9), %VMM(10), %VMM(11))
	LOAD_ONE_SET((%rsi), PAGE_SIZE * 3, %VMM(12), %VMM(13), %VMM(14), %VMM(15))
	subq	$-LARGE_LOAD_SIZE, %rsi
	/* Non-temporal store vectors to rdi.  */
	STORE_ONE_SET((%rdi), 0, %VMM(0), %VMM(1), %VMM(2), %VMM(3))
	STORE_ONE_SET((%rdi), PAGE_SIZE, %VMM(4), %VMM(5), %VMM(6), %VMM(7))
	STORE_ONE_SET((%rdi), PAGE_SIZE * 2, %VMM(8), %VMM(9), %VMM(10), %VMM(11))
	STORE_ONE_SET((%rdi), PAGE_SIZE * 3, %VMM(12), %VMM(13), %VMM(14), %VMM(15))
	subq	$-LARGE_LOAD_SIZE, %rdi
	decl	%ecx
	jnz	L(loop_large_memcpy_4x_inner)
	addq	$(PAGE_SIZE * 3), %rdi
	addq	$(PAGE_SIZE * 3), %rsi
	decq	%r10
	jne	L(loop_large_memcpy_4x_outer)
	sfence
	/* Check if only last 4 loads are needed.  */
	cmpl	$(VEC_SIZE * 4), %edx
	jbe	L(large_memcpy_4x_end)

	/* Handle the last 4  * PAGE_SIZE bytes.  */
L(loop_large_memcpy_4x_tail):
	/* Copy 4 * VEC a time forward with non-temporal stores.  */
	PREFETCH_ONE_SET (1, (%rsi), PREFETCHED_LOAD_SIZE)
	PREFETCH_ONE_SET (1, (%rdi), PREFETCHED_LOAD_SIZE)
	VMOVU	(%rsi), %VMM(0)
	VMOVU	VEC_SIZE(%rsi), %VMM(1)
	VMOVU	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	(VEC_SIZE * 3)(%rsi), %VMM(3)
	subq	$-(VEC_SIZE * 4), %rsi
	addl	$-(VEC_SIZE * 4), %edx
	VMOVA	%VMM(0), (%rdi)
	VMOVA	%VMM(1), VEC_SIZE(%rdi)
	VMOVA	%VMM(2), (VEC_SIZE * 2)(%rdi)
	VMOVA	%VMM(3), (VEC_SIZE * 3)(%rdi)
	subq	$-(VEC_SIZE * 4), %rdi
	cmpl	$(VEC_SIZE * 4), %edx
	ja	L(loop_large_memcpy_4x_tail)

L(large_memcpy_4x_end):
	/* Store the last 4 * VEC.  */
	VMOVU	-(VEC_SIZE * 4)(%rsi, %rdx), %VMM(0)
	VMOVU	-(VEC_SIZE * 3)(%rsi, %rdx), %VMM(1)
	VMOVU	-(VEC_SIZE * 2)(%rsi, %rdx), %VMM(2)
	VMOVU	-VEC_SIZE(%rsi, %rdx), %VMM(3)

	VMOVU	%VMM(0), -(VEC_SIZE * 4)(%rdi, %rdx)
	VMOVU	%VMM(1), -(VEC_SIZE * 3)(%rdi, %rdx)
	VMOVU	%VMM(2), -(VEC_SIZE * 2)(%rdi, %rdx)
	VMOVU	%VMM(3), -VEC_SIZE(%rdi, %rdx)
	VZEROUPPER_RETURN
#endif
END (MEMMOVE_SYMBOL (__memmove, unaligned_erms))

#if IS_IN (libc)
# ifdef USE_MULTIARCH
strong_alias (MEMMOVE_SYMBOL (__memmove, unaligned_erms),
	      MEMMOVE_SYMBOL (__memcpy, unaligned_erms))
#  ifdef SHARED
strong_alias (MEMMOVE_SYMBOL (__memmove_chk, unaligned_erms),
	      MEMMOVE_SYMBOL (__memcpy_chk, unaligned_erms))
#  endif
# endif
# ifdef SHARED
strong_alias (MEMMOVE_CHK_SYMBOL (__memmove_chk, unaligned),
	      MEMMOVE_CHK_SYMBOL (__memcpy_chk, unaligned))
# endif
#endif
strong_alias (MEMMOVE_SYMBOL (__memmove, unaligned),
	      MEMCPY_SYMBOL (__memcpy, unaligned))
