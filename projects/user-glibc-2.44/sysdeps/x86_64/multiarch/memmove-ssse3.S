/* memmove/memcpy/mempcpy optimized for aligned access with SSSE3.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#if ISA_SHOULD_BUILD (2)

# include <sysdep.h>
# ifndef MEMMOVE
#  define MEMMOVE	__memmove_ssse3
#  define MEMMOVE_CHK	__memmove_chk_ssse3
#  define MEMCPY	__memcpy_ssse3
#  define MEMCPY_CHK	__memcpy_chk_ssse3
#  define MEMPCPY	__mempcpy_ssse3
#  define MEMPCPY_CHK	__mempcpy_chk_ssse3
# endif

	.section .text.ssse3, "ax", @progbits
# if defined SHARED
ENTRY(MEMPCPY_CHK)
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET(__chk_fail)
END(MEMPCPY_CHK)
# endif

ENTRY(MEMPCPY)
	mov	%RDI_LP, %RAX_LP
	add	%RDX_LP, %RAX_LP
	jmp	L(start)
END(MEMPCPY)

# if defined SHARED
ENTRY(MEMMOVE_CHK)
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET(__chk_fail)
END(MEMMOVE_CHK)
# endif

ENTRY_P2ALIGN(MEMMOVE, 6)
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	movq	%rdi, %rax
L(start):
	cmpq	$16, %rdx
	jb	L(copy_0_15)

	/* These loads are always useful.  */
	movups	0(%rsi), %xmm0
	movups	-16(%rsi, %rdx), %xmm7
	cmpq	$32, %rdx
	ja	L(more_2x_vec)

	movups	%xmm0, 0(%rdi)
	movups	%xmm7, -16(%rdi, %rdx)
	ret

	.p2align 4,, 4
L(copy_0_15):
	cmpl	$4, %edx
	jb	L(copy_0_3)
	cmpl	$8, %edx
	jb	L(copy_4_7)
	movq	0(%rsi), %rcx
	movq	-8(%rsi, %rdx), %rsi
	movq	%rcx, 0(%rdi)
	movq	%rsi, -8(%rdi, %rdx)
	ret

	.p2align 4,, 4
L(copy_4_7):
	movl	0(%rsi), %ecx
	movl	-4(%rsi, %rdx), %esi
	movl	%ecx, 0(%rdi)
	movl	%esi, -4(%rdi, %rdx)
	ret

	.p2align 4,, 4
L(copy_0_3):
	decl	%edx
	jl	L(copy_0_0)
	movb	(%rsi), %cl
	je	L(copy_1_1)

	movzwl	-1(%rsi, %rdx), %esi
	movw	%si, -1(%rdi, %rdx)
L(copy_1_1):
	movb	%cl, (%rdi)
L(copy_0_0):
	ret

	.p2align 4,, 4
L(copy_4x_vec):
	movups	16(%rsi), %xmm1
	movups	-32(%rsi, %rdx), %xmm2

	movups	%xmm0, 0(%rdi)
	movups	%xmm1, 16(%rdi)
	movups	%xmm2, -32(%rdi, %rdx)
	movups	%xmm7, -16(%rdi, %rdx)
L(nop):
	ret

	.p2align 4
L(more_2x_vec):
	cmpq	$64, %rdx
	jbe	L(copy_4x_vec)

	/* We use rcx later to get alignr value.  */
	movq	%rdi, %rcx

	/* Backward copy for overlap + dst > src for memmove safety.  */
	subq	%rsi, %rcx
	cmpq	%rdx, %rcx
	jb	L(copy_backward)

	/* Load tail.  */

	/* -16(%rsi, %rdx) already loaded into xmm7.  */
	movups	-32(%rsi, %rdx), %xmm8
	movups	-48(%rsi, %rdx), %xmm9

	/* Get misalignment.  */
	andl	$0xf, %ecx

	movq	%rsi, %r9
	addq	%rcx, %rsi
	andq	$-16, %rsi
	/* Get first vec for `palignr`.  */
	movaps	(%rsi), %xmm1

	/* We have loaded (%rsi) so safe to do this store before the
	   loop.  */
	movups	%xmm0, (%rdi)

	cmp	__x86_shared_non_temporal_threshold(%rip), %rdx
	ja	L(large_memcpy)

L(loop_fwd):
	leaq	-64(%rdi, %rdx), %r8
	andq	$-16, %rdi
	movl	$48, %edx

	leaq	L(loop_fwd_start)(%rip), %r9
	sall	$6, %ecx
	addq	%r9, %rcx
	jmp	* %rcx

	.p2align 4,, 8
L(copy_backward):
	testq	%rcx, %rcx
	jz	L(nop)

	/* Preload tail.  */

	/* (%rsi) already loaded into xmm0.  */
	movups	16(%rsi), %xmm4
	movups	32(%rsi), %xmm5

	movq	%rdi, %r8
	subq	%rdi, %rsi
	leaq	-49(%rdi, %rdx), %rdi
	andq	$-16, %rdi
	addq	%rdi, %rsi
	andq	$-16, %rsi

	movaps	48(%rsi), %xmm6


	leaq	L(loop_bkwd_start)(%rip), %r9
	andl	$0xf, %ecx
	sall	$6, %ecx
	addq	%r9, %rcx
	jmp	* %rcx

	.p2align 4,, 8
L(large_memcpy):
	movups	-64(%r9, %rdx), %xmm10
	movups	-80(%r9, %rdx), %xmm11

	/* Check if src and dst overlap. If they do use cacheable
	   writes to potentially gain positive interference between
	   the loads during the memmove.  */
	subq	%rdi, %r9
	cmpq	%rdx, %r9
	jb	L(loop_fwd)

	sall	$5, %ecx
	leal	(%rcx, %rcx, 2), %r8d
	leaq	-96(%rdi, %rdx), %rcx
	andq	$-16, %rdi
	leaq	L(large_loop_fwd_start)(%rip), %rdx
	addq	%r8, %rdx
	jmp	* %rdx


	/* Instead of a typical jump table all 16 loops are exactly
	   64-bytes in size. So, we can just jump to first loop + r8 *
	   64. Before modifying any loop ensure all their sizes match!
	 */
	.p2align 6
L(loop_fwd_start):
L(loop_fwd_0x0):
	movaps	16(%rsi), %xmm1
	movaps	32(%rsi), %xmm2
	movaps	48(%rsi), %xmm3
	movaps	%xmm1, 16(%rdi)
	movaps	%xmm2, 32(%rdi)
	movaps	%xmm3, 48(%rdi)
	addq	%rdx, %rdi
	addq	%rdx, %rsi
	cmpq	%rdi, %r8
	ja	L(loop_fwd_0x0)
L(end_loop_fwd):
	movups	%xmm9, 16(%r8)
	movups	%xmm8, 32(%r8)
	movups	%xmm7, 48(%r8)
	ret

	/* Exactly 64 bytes if `jmp L(end_loop_fwd)` is long encoding.
	   60 bytes otherwise.  */
# define ALIGNED_LOOP_FWD(align_by);	\
	.p2align 6;	\
L(loop_fwd_ ## align_by):	\
	movaps	16(%rsi), %xmm0;	\
	movaps	32(%rsi), %xmm2;	\
	movaps	48(%rsi), %xmm3;	\
	movaps	%xmm3, %xmm4;	\
	palignr	$align_by, %xmm2, %xmm3;	\
	palignr	$align_by, %xmm0, %xmm2;	\
	palignr	$align_by, %xmm1, %xmm0;	\
	movaps	%xmm4, %xmm1;	\
	movaps	%xmm0, 16(%rdi);	\
	movaps	%xmm2, 32(%rdi);	\
	movaps	%xmm3, 48(%rdi);	\
	addq	%rdx, %rdi;	\
	addq	%rdx, %rsi;	\
	cmpq	%rdi, %r8;	\
	ja	L(loop_fwd_ ## align_by);	\
	jmp	L(end_loop_fwd);

	/* Must be in descending order.  */
	ALIGNED_LOOP_FWD (0xf)
	ALIGNED_LOOP_FWD (0xe)
	ALIGNED_LOOP_FWD (0xd)
	ALIGNED_LOOP_FWD (0xc)
	ALIGNED_LOOP_FWD (0xb)
	ALIGNED_LOOP_FWD (0xa)
	ALIGNED_LOOP_FWD (0x9)
	ALIGNED_LOOP_FWD (0x8)
	ALIGNED_LOOP_FWD (0x7)
	ALIGNED_LOOP_FWD (0x6)
	ALIGNED_LOOP_FWD (0x5)
	ALIGNED_LOOP_FWD (0x4)
	ALIGNED_LOOP_FWD (0x3)
	ALIGNED_LOOP_FWD (0x2)
	ALIGNED_LOOP_FWD (0x1)

	.p2align 6
L(large_loop_fwd_start):
L(large_loop_fwd_0x0):
	movaps	16(%rsi), %xmm1
	movaps	32(%rsi), %xmm2
	movaps	48(%rsi), %xmm3
	movaps	64(%rsi), %xmm4
	movaps	80(%rsi), %xmm5
	movntps	%xmm1, 16(%rdi)
	movntps	%xmm2, 32(%rdi)
	movntps	%xmm3, 48(%rdi)
	movntps	%xmm4, 64(%rdi)
	movntps	%xmm5, 80(%rdi)
	addq	$80, %rdi
	addq	$80, %rsi
	cmpq	%rdi, %rcx
	ja	L(large_loop_fwd_0x0)

	/* Ensure no icache line split on tail.  */
	.p2align 4
L(end_large_loop_fwd):
	sfence
	movups	%xmm11, 16(%rcx)
	movups	%xmm10, 32(%rcx)
	movups	%xmm9, 48(%rcx)
	movups	%xmm8, 64(%rcx)
	movups	%xmm7, 80(%rcx)
	ret


	/* Size > 64 bytes and <= 96 bytes. 32-byte align between ensure
	   96-byte spacing between each.  */
# define ALIGNED_LARGE_LOOP_FWD(align_by);	\
	.p2align 5;	\
L(large_loop_fwd_ ## align_by):	\
	movaps	16(%rsi), %xmm0;	\
	movaps	32(%rsi), %xmm2;	\
	movaps	48(%rsi), %xmm3;	\
	movaps	64(%rsi), %xmm4;	\
	movaps	80(%rsi), %xmm5;	\
	movaps	%xmm5, %xmm6;	\
	palignr	$align_by, %xmm4, %xmm5;	\
	palignr	$align_by, %xmm3, %xmm4;	\
	palignr	$align_by, %xmm2, %xmm3;	\
	palignr	$align_by, %xmm0, %xmm2;	\
	palignr	$align_by, %xmm1, %xmm0;	\
	movaps	%xmm6, %xmm1;	\
	movntps	%xmm0, 16(%rdi);	\
	movntps	%xmm2, 32(%rdi);	\
	movntps	%xmm3, 48(%rdi);	\
	movntps	%xmm4, 64(%rdi);	\
	movntps	%xmm5, 80(%rdi);	\
	addq	$80, %rdi;	\
	addq	$80, %rsi;	\
	cmpq	%rdi, %rcx;	\
	ja	L(large_loop_fwd_ ## align_by);	\
	jmp	L(end_large_loop_fwd);

	/* Must be in descending order.  */
	ALIGNED_LARGE_LOOP_FWD (0xf)
	ALIGNED_LARGE_LOOP_FWD (0xe)
	ALIGNED_LARGE_LOOP_FWD (0xd)
	ALIGNED_LARGE_LOOP_FWD (0xc)
	ALIGNED_LARGE_LOOP_FWD (0xb)
	ALIGNED_LARGE_LOOP_FWD (0xa)
	ALIGNED_LARGE_LOOP_FWD (0x9)
	ALIGNED_LARGE_LOOP_FWD (0x8)
	ALIGNED_LARGE_LOOP_FWD (0x7)
	ALIGNED_LARGE_LOOP_FWD (0x6)
	ALIGNED_LARGE_LOOP_FWD (0x5)
	ALIGNED_LARGE_LOOP_FWD (0x4)
	ALIGNED_LARGE_LOOP_FWD (0x3)
	ALIGNED_LARGE_LOOP_FWD (0x2)
	ALIGNED_LARGE_LOOP_FWD (0x1)


	.p2align 6
L(loop_bkwd_start):
L(loop_bkwd_0x0):
	movaps	32(%rsi), %xmm1
	movaps	16(%rsi), %xmm2
	movaps	0(%rsi), %xmm3
	movaps	%xmm1, 32(%rdi)
	movaps	%xmm2, 16(%rdi)
	movaps	%xmm3, 0(%rdi)
	subq	$48, %rdi
	subq	$48, %rsi
	cmpq	%rdi, %r8
	jb	L(loop_bkwd_0x0)
L(end_loop_bkwd):
	movups	%xmm7, -16(%r8, %rdx)
	movups	%xmm0, 0(%r8)
	movups	%xmm4, 16(%r8)
	movups	%xmm5, 32(%r8)

	ret


	/* Exactly 64 bytes if `jmp L(end_loop_bkwd)` is long encoding.
	   60 bytes otherwise.  */
# define ALIGNED_LOOP_BKWD(align_by);	\
	.p2align 6;	\
L(loop_bkwd_ ## align_by):	\
	movaps	32(%rsi), %xmm1;	\
	movaps	16(%rsi), %xmm2;	\
	movaps	0(%rsi), %xmm3;	\
	palignr	$align_by, %xmm1, %xmm6;	\
	palignr	$align_by, %xmm2, %xmm1;	\
	palignr	$align_by, %xmm3, %xmm2;	\
	movaps	%xmm6, 32(%rdi);	\
	movaps	%xmm1, 16(%rdi);	\
	movaps	%xmm2, 0(%rdi);	\
	subq	$48, %rdi;	\
	subq	$48, %rsi;	\
	movaps	%xmm3, %xmm6;	\
	cmpq	%rdi, %r8;	\
	jb	L(loop_bkwd_ ## align_by);	\
	jmp	L(end_loop_bkwd);

	/* Must be in descending order.  */
	ALIGNED_LOOP_BKWD (0xf)
	ALIGNED_LOOP_BKWD (0xe)
	ALIGNED_LOOP_BKWD (0xd)
	ALIGNED_LOOP_BKWD (0xc)
	ALIGNED_LOOP_BKWD (0xb)
	ALIGNED_LOOP_BKWD (0xa)
	ALIGNED_LOOP_BKWD (0x9)
	ALIGNED_LOOP_BKWD (0x8)
	ALIGNED_LOOP_BKWD (0x7)
	ALIGNED_LOOP_BKWD (0x6)
	ALIGNED_LOOP_BKWD (0x5)
	ALIGNED_LOOP_BKWD (0x4)
	ALIGNED_LOOP_BKWD (0x3)
	ALIGNED_LOOP_BKWD (0x2)
	ALIGNED_LOOP_BKWD (0x1)
END(MEMMOVE)

strong_alias (MEMMOVE, MEMCPY)
# if defined SHARED
strong_alias (MEMMOVE_CHK, MEMCPY_CHK)
# endif
#endif
