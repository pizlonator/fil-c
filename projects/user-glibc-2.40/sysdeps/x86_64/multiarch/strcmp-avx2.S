/* strcmp/wcscmp/strncmp/wcsncmp optimized with AVX2.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

# ifndef STRCMP_ISA
#  define STRCMP_ISA	_avx2
# endif

# include "strcmp-naming.h"

# include <sysdep.h>

# if defined USE_AS_STRCASECMP_L
#  include "locale-defines.h"
# endif

# ifndef STRCMP
#  define STRCMP	__strcmp_avx2
# endif

# define PAGE_SIZE	4096

	/* VEC_SIZE = Number of bytes in a ymm register.  */
# define VEC_SIZE	32

# define VMOVU	vmovdqu
# define VMOVA	vmovdqa

# ifdef USE_AS_WCSCMP
	/* Compare packed dwords.  */
#  define VPCMPEQ	vpcmpeqd
	/* Compare packed dwords and store minimum.  */
#  define VPMINU	vpminud
	/* 1 dword char == 4 bytes.  */
#  define SIZE_OF_CHAR	4
# else
	/* Compare packed bytes.  */
#  define VPCMPEQ	vpcmpeqb
	/* Compare packed bytes and store minimum.  */
#  define VPMINU	vpminub
	/* 1 byte char == 1 byte.  */
#  define SIZE_OF_CHAR	1
# endif

# ifdef USE_AS_STRNCMP
#  define LOOP_REG	r9d
#  define LOOP_REG64	r9

#  define OFFSET_REG8	r9b
#  define OFFSET_REG	r9d
#  define OFFSET_REG64	r9
# else
#  define LOOP_REG	edx
#  define LOOP_REG64	rdx

#  define OFFSET_REG8	dl
#  define OFFSET_REG	edx
#  define OFFSET_REG64	rdx
# endif

# ifndef VZEROUPPER
#  define VZEROUPPER	vzeroupper
# endif

# if defined USE_AS_STRNCMP
#  define VEC_OFFSET	0
# else
#  define VEC_OFFSET	(-VEC_SIZE)
# endif

# ifdef USE_AS_STRCASECMP_L
#  define BYTE_LOOP_REG	OFFSET_REG
# else
#  define BYTE_LOOP_REG	ecx
# endif

# ifdef USE_AS_STRCASECMP_L
#  ifdef USE_AS_STRNCMP
#   define LOCALE_REG	rcx
#   define LOCALE_REG_LP	RCX_LP
#  else
#   define LOCALE_REG	rdx
#   define LOCALE_REG_LP	RDX_LP
#  endif
# endif

# define xmmZERO	xmm15
# define ymmZERO	ymm15

# define LCASE_MIN_ymm	%ymm10
# define LCASE_MAX_ymm	%ymm11
# define CASE_ADD_ymm	%ymm12

# define LCASE_MIN_xmm	%xmm10
# define LCASE_MAX_xmm	%xmm11
# define CASE_ADD_xmm	%xmm12

	/* r11 is never use elsewhere so this is safe to maintain.  */
# define TOLOWER_BASE	%r11

# ifndef SECTION
#  define SECTION(p)	p##.avx
# endif

# ifdef USE_AS_STRCASECMP_L
#  define REG(x, y) x ## y
#  define TOLOWER(reg1_in, reg1_out, reg2_in, reg2_out, ext)			\
	vpaddb	REG(LCASE_MIN_, ext), reg1_in, REG(%ext, 8);				\
	vpaddb	REG(LCASE_MIN_, ext), reg2_in, REG(%ext, 9);				\
	vpcmpgtb REG(LCASE_MAX_, ext), REG(%ext, 8), REG(%ext, 8);			\
	vpcmpgtb REG(LCASE_MAX_, ext), REG(%ext, 9), REG(%ext, 9);			\
	vpandn	REG(CASE_ADD_, ext), REG(%ext, 8), REG(%ext, 8);			\
	vpandn	REG(CASE_ADD_, ext), REG(%ext, 9), REG(%ext, 9);			\
	vpaddb	REG(%ext, 8), reg1_in, reg1_out;							\
	vpaddb	REG(%ext, 9), reg2_in, reg2_out

#  define TOLOWER_gpr(src, dst)	movl (TOLOWER_BASE, src, 4), dst
#  define TOLOWER_ymm(...)	TOLOWER(__VA_ARGS__, ymm)
#  define TOLOWER_xmm(...)	TOLOWER(__VA_ARGS__, xmm)

#  define CMP_R1_R2(s1_reg, s2_reg, scratch_reg, reg_out, ext)			\
	TOLOWER	(s1_reg, scratch_reg, s2_reg, s2_reg, ext);					\
	VPCMPEQ	scratch_reg, s2_reg, reg_out

#  define CMP_R1_S2(s1_reg, s2_mem, scratch_reg, reg_out, ext)			\
	VMOVU	s2_mem, reg_out;											\
	CMP_R1_R2(s1_reg, reg_out, scratch_reg, reg_out, ext)

#  define CMP_R1_R2_ymm(...) CMP_R1_R2(__VA_ARGS__, ymm)
#  define CMP_R1_R2_xmm(...) CMP_R1_R2(__VA_ARGS__, xmm)

#  define CMP_R1_S2_ymm(...) CMP_R1_S2(__VA_ARGS__, ymm)
#  define CMP_R1_S2_xmm(...) CMP_R1_S2(__VA_ARGS__, xmm)

# else
#  define TOLOWER_gpr(...)
#  define TOLOWER_ymm(...)
#  define TOLOWER_xmm(...)

#  define CMP_R1_R2_ymm(s1_reg, s2_reg, scratch_reg, reg_out)			\
	VPCMPEQ	s2_reg, s1_reg, reg_out

#  define CMP_R1_R2_xmm(...) CMP_R1_R2_ymm(__VA_ARGS__)

#  define CMP_R1_S2_ymm(...) CMP_R1_R2_ymm(__VA_ARGS__)
#  define CMP_R1_S2_xmm(...) CMP_R1_R2_xmm(__VA_ARGS__)
# endif

/* Warning!
           wcscmp/wcsncmp have to use SIGNED comparison for elements.
           strcmp/strncmp have to use UNSIGNED comparison for elements.
*/

/* The main idea of the string comparison (byte or dword) using AVX2
   consists of comparing (VPCMPEQ) two ymm vectors. The latter can be on
   either packed bytes or dwords depending on USE_AS_WCSCMP. In order
   to check the null char, algorithm keeps the matched bytes/dwords,
   requiring two more AVX2 instructions (VPMINU and VPCMPEQ). In general,
   the costs of comparing VEC_SIZE bytes (32-bytes) are two VPCMPEQ and
   one VPMINU instructions, together with movdqu and testl instructions.
   Main loop (away from from page boundary) compares 4 vectors are a time,
   effectively comparing 4 x VEC_SIZE bytes (128 bytes) on each loop.

   The routine strncmp/wcsncmp (enabled by defining USE_AS_STRNCMP) logic
   is the same as strcmp, except that an a maximum offset is tracked.  If
   the maximum offset is reached before a difference is found, zero is
   returned.  */

	.section SECTION(.text), "ax", @progbits
	.align	16
	.type	STRCMP, @function
	.globl	STRCMP

# ifdef USE_AS_STRCASECMP_L
ENTRY (STRCASECMP)
	movq	__libc_tsd_LOCALE@gottpoff(%rip), %rax
	mov	%fs:(%rax), %LOCALE_REG_LP

	/* Either 1 or 5 bytes (depending if CET is enabled).  */
	.p2align 4
END (STRCASECMP)
	/* FALLTHROUGH to strcasecmp/strncasecmp_l.  */
# endif

	.p2align 4
STRCMP:
	cfi_startproc
	_CET_ENDBR
	CALL_MCOUNT

# if defined USE_AS_STRCASECMP_L
	/* We have to fall back on the C implementation for locales with
	   encodings not matching ASCII for single bytes.  */
#  if LOCALE_T___LOCALES != 0 || LC_CTYPE != 0
	mov	LOCALE_T___LOCALES + LC_CTYPE * LP_SIZE(%LOCALE_REG), %RAX_LP
#  else
	mov	(%LOCALE_REG), %RAX_LP
#  endif
	testb	$1, LOCALE_DATA_VALUES + _NL_CTYPE_NONASCII_CASE * SIZEOF_VALUES(%rax)
	jne	STRCASECMP_L_NONASCII
	leaq	_nl_C_LC_CTYPE_tolower + 128 * 4(%rip), TOLOWER_BASE
# endif

# ifdef USE_AS_STRNCMP
	/* Don't overwrite LOCALE_REG (rcx) until we have pass
	   L(one_or_less). Otherwise we might use the wrong locale in
	   the OVERFLOW_STRCMP (strcasecmp_l).  */
#  ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
#  endif
	cmp	$1, %RDX_LP
	/* Signed comparison intentional. We use this branch to also
	   test cases where length >= 2^63. These very large sizes can be
	   handled with strcmp as there is no way for that length to
	   actually bound the buffer.  */
	jle	L(one_or_less)
#  ifdef USE_AS_WCSCMP
	movq	%rdx, %rcx

	/* Multiplying length by sizeof(wchar_t) can result in overflow.
	   Check if that is possible. All cases where overflow are possible
	   are cases where length is large enough that it can never be a
	   bound on valid memory so just use wcscmp.  */
	shrq	$56, %rcx
	jnz	OVERFLOW_STRCMP

	leaq	(, %rdx, 4), %rdx
#  endif
# endif
	vpxor	%xmmZERO, %xmmZERO, %xmmZERO
# if defined USE_AS_STRCASECMP_L
	.section .rodata.cst32, "aM", @progbits, 32
	.align	32
L(lcase_min):
	.quad	0x3f3f3f3f3f3f3f3f
	.quad	0x3f3f3f3f3f3f3f3f
	.quad	0x3f3f3f3f3f3f3f3f
	.quad	0x3f3f3f3f3f3f3f3f
L(lcase_max):
	.quad	0x9999999999999999
	.quad	0x9999999999999999
	.quad	0x9999999999999999
	.quad	0x9999999999999999
L(case_add):
	.quad	0x2020202020202020
	.quad	0x2020202020202020
	.quad	0x2020202020202020
	.quad	0x2020202020202020
	.previous

	vmovdqa	L(lcase_min)(%rip), LCASE_MIN_ymm
	vmovdqa	L(lcase_max)(%rip), LCASE_MAX_ymm
	vmovdqa	L(case_add)(%rip), CASE_ADD_ymm
# endif
	movl	%edi, %eax
	orl	%esi, %eax
	sall	$20, %eax
	/* Check if s1 or s2 may cross a page  in next 4x VEC loads.  */
	cmpl	$((PAGE_SIZE -(VEC_SIZE * 4)) << 20), %eax
	ja	L(page_cross)

L(no_page_cross):
	/* Safe to compare 4x vectors.  */
	VMOVU	(%rdi), %ymm0
	/* 1s where s1 and s2 equal. Just VPCMPEQ if its not strcasecmp.
	   Otherwise converts ymm0 and load from rsi to lower. ymm2 is
	   scratch and ymm1 is the return.  */
	CMP_R1_S2_ymm (%ymm0, (%rsi), %ymm2, %ymm1)
	/* 1s at null CHAR.  */
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	/* 1s where s1 and s2 equal AND not null CHAR.  */
	vpandn	%ymm1, %ymm2, %ymm1

	/* All 1s -> keep going, any 0s -> return.  */
	vpmovmskb %ymm1, %ecx
# ifdef USE_AS_STRNCMP
	cmpq	$VEC_SIZE, %rdx
	jbe	L(vec_0_test_len)
# endif

	/* All 1s represents all equals. incl will overflow to zero in
	   all equals case. Otherwise 1s will carry until position of first
	   mismatch.  */
	incl	%ecx
	jz	L(more_3x_vec)

	.p2align 4,, 4
L(return_vec_0):
	tzcntl	%ecx, %ecx
# ifdef USE_AS_WCSCMP
	movl	(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	(%rsi, %rcx), %edx
	je	L(ret0)
	setl	%al
	negl	%eax
	orl	$1, %eax
# else
	movzbl	(%rdi, %rcx), %eax
	movzbl	(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
# endif
L(ret0):
L(return_vzeroupper):
	ZERO_UPPER_VEC_REGISTERS_RETURN

# ifdef USE_AS_STRNCMP
	.p2align 4,, 8
L(vec_0_test_len):
	notl	%ecx
	bzhil	%edx, %ecx, %eax
	jnz	L(return_vec_0)
	/* Align if will cross fetch block.  */
	.p2align 4,, 2
L(ret_zero):
	xorl	%eax, %eax
	VZEROUPPER_RETURN

	.p2align 4,, 5
L(one_or_less):
#  ifdef USE_AS_STRCASECMP_L
	/* Set locale argument for strcasecmp.  */
	movq	%LOCALE_REG, %rdx
#  endif
	jb	L(ret_zero)
	/* 'nbe' covers the case where length is negative (large
	   unsigned).  */
	jnbe	OVERFLOW_STRCMP
#  ifdef USE_AS_WCSCMP
	movl	(%rdi), %edx
	xorl	%eax, %eax
	cmpl	(%rsi), %edx
	je	L(ret1)
	setl	%al
	negl	%eax
	orl	$1, %eax
#  else
	movzbl	(%rdi), %eax
	movzbl	(%rsi), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
#  endif
L(ret1):
	ret
# endif

	.p2align 4,, 10
L(return_vec_1):
	tzcntl	%ecx, %ecx
# ifdef USE_AS_STRNCMP
	/* rdx must be > CHAR_PER_VEC so save to subtract w.o fear of
	   overflow.  */
	addq	$-VEC_SIZE, %rdx
	cmpq	%rcx, %rdx
	jbe	L(ret_zero)
# endif
# ifdef USE_AS_WCSCMP
	movl	VEC_SIZE(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	VEC_SIZE(%rsi, %rcx), %edx
	je	L(ret2)
	setl	%al
	negl	%eax
	orl	$1, %eax
# else
	movzbl	VEC_SIZE(%rdi, %rcx), %eax
	movzbl	VEC_SIZE(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
# endif
L(ret2):
	VZEROUPPER_RETURN

	.p2align 4,, 10
# ifdef USE_AS_STRNCMP
L(return_vec_3):
	salq	$32, %rcx
# endif

L(return_vec_2):
# ifndef USE_AS_STRNCMP
	tzcntl	%ecx, %ecx
# else
	tzcntq	%rcx, %rcx
	cmpq	%rcx, %rdx
	jbe	L(ret_zero)
# endif

# ifdef USE_AS_WCSCMP
	movl	(VEC_SIZE * 2)(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	(VEC_SIZE * 2)(%rsi, %rcx), %edx
	je	L(ret3)
	setl	%al
	negl	%eax
	orl	$1, %eax
# else
	movzbl	(VEC_SIZE * 2)(%rdi, %rcx), %eax
	movzbl	(VEC_SIZE * 2)(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
# endif
L(ret3):
	VZEROUPPER_RETURN

# ifndef USE_AS_STRNCMP
	.p2align 4,, 10
L(return_vec_3):
	tzcntl	%ecx, %ecx
#  ifdef USE_AS_WCSCMP
	movl	(VEC_SIZE * 3)(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	(VEC_SIZE * 3)(%rsi, %rcx), %edx
	je	L(ret4)
	setl	%al
	negl	%eax
	orl	$1, %eax
#  else
	movzbl	(VEC_SIZE * 3)(%rdi, %rcx), %eax
	movzbl	(VEC_SIZE * 3)(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
#  endif
L(ret4):
	VZEROUPPER_RETURN
# endif

	.p2align 4,, 10
L(more_3x_vec):
	/* Safe to compare 4x vectors.  */
	VMOVU	VEC_SIZE(%rdi), %ymm0
	CMP_R1_S2_ymm (%ymm0, VEC_SIZE(%rsi), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_1)

# ifdef USE_AS_STRNCMP
	subq	$(VEC_SIZE * 2), %rdx
	jbe	L(ret_zero)
# endif

	VMOVU	(VEC_SIZE * 2)(%rdi), %ymm0
	CMP_R1_S2_ymm (%ymm0, (VEC_SIZE * 2)(%rsi), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_2)

	VMOVU	(VEC_SIZE * 3)(%rdi), %ymm0
	CMP_R1_S2_ymm (%ymm0, (VEC_SIZE * 3)(%rsi), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_3)

# ifdef USE_AS_STRNCMP
	cmpq	$(VEC_SIZE * 2), %rdx
	jbe	L(ret_zero)
# endif

# ifdef USE_AS_WCSCMP
	/* any non-zero positive value that doesn't inference with 0x1.
	 */
	movl	$2, %r8d

# else
	xorl	%r8d, %r8d
# endif

	/* The prepare labels are various entry points from the page
	   cross logic.  */
L(prepare_loop):

# ifdef USE_AS_STRNCMP
	/* Store N + (VEC_SIZE * 4) and place check at the beginning of
	   the loop.  */
	leaq	(VEC_SIZE * 2)(%rdi, %rdx), %rdx
# endif
L(prepare_loop_no_len):

	/* Align s1 and adjust s2 accordingly.  */
	subq	%rdi, %rsi
	andq	$-(VEC_SIZE * 4), %rdi
	addq	%rdi, %rsi

# ifdef USE_AS_STRNCMP
	subq	%rdi, %rdx
# endif

L(prepare_loop_aligned):
	/* eax stores distance from rsi to next page cross. These cases
	   need to be handled specially as the 4x loop could potentially
	   read memory past the length of s1 or s2 and across a page
	   boundary.  */
	movl	$-(VEC_SIZE * 4), %eax
	subl	%esi, %eax
	andl	$(PAGE_SIZE - 1), %eax

	/* Loop 4x comparisons at a time.  */
	.p2align 4
L(loop):

	/* End condition for strncmp.  */
# ifdef USE_AS_STRNCMP
	subq	$(VEC_SIZE * 4), %rdx
	jbe	L(ret_zero)
# endif

	subq	$-(VEC_SIZE * 4), %rdi
	subq	$-(VEC_SIZE * 4), %rsi

	/* Check if rsi loads will cross a page boundary.  */
	addl	$-(VEC_SIZE * 4), %eax
	jnb	L(page_cross_during_loop)

	/* Loop entry after handling page cross during loop.  */
L(loop_skip_page_cross_check):
	VMOVA	(VEC_SIZE * 0)(%rdi), %ymm0
	VMOVA	(VEC_SIZE * 1)(%rdi), %ymm2
	VMOVA	(VEC_SIZE * 2)(%rdi), %ymm4
	VMOVA	(VEC_SIZE * 3)(%rdi), %ymm6

	/* ymm1 all 1s where s1 and s2 equal. All 0s otherwise.  */
	CMP_R1_S2_ymm (%ymm0, (VEC_SIZE * 0)(%rsi), %ymm3, %ymm1)
	CMP_R1_S2_ymm (%ymm2, (VEC_SIZE * 1)(%rsi), %ymm5, %ymm3)
	CMP_R1_S2_ymm (%ymm4, (VEC_SIZE * 2)(%rsi), %ymm7, %ymm5)
	CMP_R1_S2_ymm (%ymm6, (VEC_SIZE * 3)(%rsi), %ymm13, %ymm7)

	/* If any mismatches or null CHAR then 0 CHAR, otherwise non-
	   zero.  */
	vpand	%ymm0, %ymm1, %ymm1


	vpand	%ymm2, %ymm3, %ymm3
	vpand	%ymm4, %ymm5, %ymm5
	vpand	%ymm6, %ymm7, %ymm7

	VPMINU	%ymm1, %ymm3, %ymm3
	VPMINU	%ymm5, %ymm7, %ymm7

	/* Reduce all 0 CHARs for the 4x VEC into ymm7.  */
	VPMINU	%ymm3, %ymm7, %ymm7

	/* If any 0 CHAR then done.  */
	VPCMPEQ	%ymm7, %ymmZERO, %ymm7
	vpmovmskb %ymm7, %LOOP_REG
	testl	%LOOP_REG, %LOOP_REG
	jz	L(loop)

	/* Find which VEC has the mismatch of end of string.  */
	VPCMPEQ	%ymm1, %ymmZERO, %ymm1
	vpmovmskb %ymm1, %ecx
	testl	%ecx, %ecx
	jnz	L(return_vec_0_end)


	VPCMPEQ	%ymm3, %ymmZERO, %ymm3
	vpmovmskb %ymm3, %ecx
	testl	%ecx, %ecx
	jnz	L(return_vec_1_end)

L(return_vec_2_3_end):
# ifdef USE_AS_STRNCMP
	subq	$(VEC_SIZE * 2), %rdx
	jbe	L(ret_zero_end)
# endif

	VPCMPEQ	%ymm5, %ymmZERO, %ymm5
	vpmovmskb %ymm5, %ecx
	testl	%ecx, %ecx
	jnz	L(return_vec_2_end)

	/* LOOP_REG contains matches for null/mismatch from the loop. If
	   VEC 0,1,and 2 all have no null and no mismatches then mismatch
	   must entirely be from VEC 3 which is fully represented by
	   LOOP_REG.  */
	tzcntl	%LOOP_REG, %LOOP_REG

# ifdef USE_AS_STRNCMP
	subl	$-(VEC_SIZE), %LOOP_REG
	cmpq	%LOOP_REG64, %rdx
	jbe	L(ret_zero_end)
# endif

# ifdef USE_AS_WCSCMP
	movl	(VEC_SIZE * 2 - VEC_OFFSET)(%rdi, %LOOP_REG64), %ecx
	xorl	%eax, %eax
	cmpl	(VEC_SIZE * 2 - VEC_OFFSET)(%rsi, %LOOP_REG64), %ecx
	je	L(ret5)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
# else
	movzbl	(VEC_SIZE * 2 - VEC_OFFSET)(%rdi, %LOOP_REG64), %eax
	movzbl	(VEC_SIZE * 2 - VEC_OFFSET)(%rsi, %LOOP_REG64), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
# endif
L(ret5):
	VZEROUPPER_RETURN

# ifdef USE_AS_STRNCMP
	.p2align 4,, 2
L(ret_zero_end):
	xorl	%eax, %eax
	VZEROUPPER_RETURN
# endif


	/* The L(return_vec_N_end) differ from L(return_vec_N) in that
	   they use the value of `r8` to negate the return value. This is
	   because the page cross logic can swap `rdi` and `rsi`.  */
	.p2align 4,, 10
# ifdef USE_AS_STRNCMP
L(return_vec_1_end):
	salq	$32, %rcx
# endif
L(return_vec_0_end):
# ifndef USE_AS_STRNCMP
	tzcntl	%ecx, %ecx
# else
	tzcntq	%rcx, %rcx
	cmpq	%rcx, %rdx
	jbe	L(ret_zero_end)
# endif

# ifdef USE_AS_WCSCMP
	movl	(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	(%rsi, %rcx), %edx
	je	L(ret6)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
# else
	movzbl	(%rdi, %rcx), %eax
	movzbl	(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
# endif
L(ret6):
	VZEROUPPER_RETURN

# ifndef USE_AS_STRNCMP
	.p2align 4,, 10
L(return_vec_1_end):
	tzcntl	%ecx, %ecx
#  ifdef USE_AS_WCSCMP
	movl	VEC_SIZE(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	VEC_SIZE(%rsi, %rcx), %edx
	je	L(ret7)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
#  else
	movzbl	VEC_SIZE(%rdi, %rcx), %eax
	movzbl	VEC_SIZE(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
#  endif
L(ret7):
	VZEROUPPER_RETURN
# endif

	.p2align 4,, 10
L(return_vec_2_end):
	tzcntl	%ecx, %ecx
# ifdef USE_AS_STRNCMP
	cmpq	%rcx, %rdx
	jbe	L(ret_zero_page_cross)
# endif
# ifdef USE_AS_WCSCMP
	movl	(VEC_SIZE * 2)(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	(VEC_SIZE * 2)(%rsi, %rcx), %edx
	je	L(ret11)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
# else
	movzbl	(VEC_SIZE * 2)(%rdi, %rcx), %eax
	movzbl	(VEC_SIZE * 2)(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
# endif
L(ret11):
	VZEROUPPER_RETURN


	/* Page cross in rsi in next 4x VEC.  */

	/* TODO: Improve logic here.  */
	.p2align 4,, 10
L(page_cross_during_loop):
	/* eax contains [distance_from_page - (VEC_SIZE * 4)].  */

	/* Optimistically rsi and rdi and both aligned inwhich case we
	   don't need any logic here.  */
	cmpl	$-(VEC_SIZE * 4), %eax
	/* Don't adjust eax before jumping back to loop and we will
	   never hit page cross case again.  */
	je	L(loop_skip_page_cross_check)

	/* Check if we can safely load a VEC.  */
	cmpl	$-(VEC_SIZE * 3), %eax
	jle	L(less_1x_vec_till_page_cross)

	VMOVA	(%rdi), %ymm0
	CMP_R1_S2_ymm (%ymm0, (%rsi), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_0_end)

	/* if distance >= 2x VEC then eax > -(VEC_SIZE * 2).  */
	cmpl	$-(VEC_SIZE * 2), %eax
	jg	L(more_2x_vec_till_page_cross)

	.p2align 4,, 4
L(less_1x_vec_till_page_cross):
	subl	$-(VEC_SIZE * 4), %eax
	/* Guaranteed safe to read from rdi - VEC_SIZE here. The only
	   concerning case is first iteration if incoming s1 was near start
	   of a page and s2 near end. If s1 was near the start of the page
	   we already aligned up to nearest VEC_SIZE * 4 so gurnateed safe
	   to read back -VEC_SIZE. If rdi is truly at the start of a page
	   here, it means the previous page (rdi - VEC_SIZE) has already
	   been loaded earlier so must be valid.  */
	VMOVU	-VEC_SIZE(%rdi, %rax), %ymm0
	CMP_R1_S2_ymm (%ymm0, -VEC_SIZE(%rsi, %rax), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx

	/* Mask of potentially valid bits. The lower bits can be out of
	   range comparisons (but safe regarding page crosses).  */
	movl	$-1, %r10d
	shlxl	%esi, %r10d, %r10d
	notl	%ecx

# ifdef USE_AS_STRNCMP
	cmpq	%rax, %rdx
	jbe	L(return_page_cross_end_check)
# endif
	movl	%eax, %OFFSET_REG
	addl	$(PAGE_SIZE - VEC_SIZE * 4), %eax

	andl	%r10d, %ecx
	jz	L(loop_skip_page_cross_check)

	.p2align 4,, 3
L(return_page_cross_end):
	tzcntl	%ecx, %ecx

# ifdef USE_AS_STRNCMP
	leal	-VEC_SIZE(%OFFSET_REG64, %rcx), %ecx
L(return_page_cross_cmp_mem):
# else
	addl	%OFFSET_REG, %ecx
# endif
# ifdef USE_AS_WCSCMP
	movl	VEC_OFFSET(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	VEC_OFFSET(%rsi, %rcx), %edx
	je	L(ret8)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
# else
	movzbl	VEC_OFFSET(%rdi, %rcx), %eax
	movzbl	VEC_OFFSET(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
# endif
L(ret8):
	VZEROUPPER_RETURN

# ifdef USE_AS_STRNCMP
	.p2align 4,, 10
L(return_page_cross_end_check):
	andl	%r10d, %ecx
	tzcntl	%ecx, %ecx
	leal	-VEC_SIZE(%rax, %rcx), %ecx
	cmpl	%ecx, %edx
	ja	L(return_page_cross_cmp_mem)
	xorl	%eax, %eax
	VZEROUPPER_RETURN
# endif


	.p2align 4,, 10
L(more_2x_vec_till_page_cross):
	/* If more 2x vec till cross we will complete a full loop
	   iteration here.  */

	VMOVU	VEC_SIZE(%rdi), %ymm0
	CMP_R1_S2_ymm (%ymm0, VEC_SIZE(%rsi), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_1_end)

# ifdef USE_AS_STRNCMP
	cmpq	$(VEC_SIZE * 2), %rdx
	jbe	L(ret_zero_in_loop_page_cross)
# endif

	subl	$-(VEC_SIZE * 4), %eax

	/* Safe to include comparisons from lower bytes.  */
	VMOVU	-(VEC_SIZE * 2)(%rdi, %rax), %ymm0
	CMP_R1_S2_ymm (%ymm0, -(VEC_SIZE * 2)(%rsi, %rax), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_page_cross_0)

	VMOVU	-(VEC_SIZE * 1)(%rdi, %rax), %ymm0
	CMP_R1_S2_ymm (%ymm0, -(VEC_SIZE * 1)(%rsi, %rax), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx
	jnz	L(return_vec_page_cross_1)

# ifdef USE_AS_STRNCMP
	/* Must check length here as length might proclude reading next
	   page.  */
	cmpq	%rax, %rdx
	jbe	L(ret_zero_in_loop_page_cross)
# endif

	/* Finish the loop.  */
	VMOVA	(VEC_SIZE * 2)(%rdi), %ymm4
	VMOVA	(VEC_SIZE * 3)(%rdi), %ymm6

	CMP_R1_S2_ymm (%ymm4, (VEC_SIZE * 2)(%rsi), %ymm7, %ymm5)
	CMP_R1_S2_ymm (%ymm6, (VEC_SIZE * 3)(%rsi), %ymm13, %ymm7)
	vpand	%ymm4, %ymm5, %ymm5
	vpand	%ymm6, %ymm7, %ymm7
	VPMINU	%ymm5, %ymm7, %ymm7
	VPCMPEQ	%ymm7, %ymmZERO, %ymm7
	vpmovmskb %ymm7, %LOOP_REG
	testl	%LOOP_REG, %LOOP_REG
	jnz	L(return_vec_2_3_end)

	/* Best for code size to include ucond-jmp here. Would be faster
	   if this case is hot to duplicate the L(return_vec_2_3_end) code
	   as fall-through and have jump back to loop on mismatch
	   comparison.  */
	subq	$-(VEC_SIZE * 4), %rdi
	subq	$-(VEC_SIZE * 4), %rsi
	addl	$(PAGE_SIZE - VEC_SIZE * 8), %eax
# ifdef USE_AS_STRNCMP
	subq	$(VEC_SIZE * 4), %rdx
	ja	L(loop_skip_page_cross_check)
L(ret_zero_in_loop_page_cross):
	xorl	%eax, %eax
	VZEROUPPER_RETURN
# else
	jmp	L(loop_skip_page_cross_check)
# endif


	.p2align 4,, 10
L(return_vec_page_cross_0):
	addl	$-VEC_SIZE, %eax
L(return_vec_page_cross_1):
	tzcntl	%ecx, %ecx
# ifdef USE_AS_STRNCMP
	leal	-VEC_SIZE(%rax, %rcx), %ecx
	cmpq	%rcx, %rdx
	jbe	L(ret_zero_in_loop_page_cross)
# else
	addl	%eax, %ecx
# endif

# ifdef USE_AS_WCSCMP
	movl	VEC_OFFSET(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	VEC_OFFSET(%rsi, %rcx), %edx
	je	L(ret9)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
# else
	movzbl	VEC_OFFSET(%rdi, %rcx), %eax
	movzbl	VEC_OFFSET(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
# endif
L(ret9):
	VZEROUPPER_RETURN


	.p2align 4,, 10
L(page_cross):
# ifndef USE_AS_STRNCMP
	/* If both are VEC aligned we don't need any special logic here.
	   Only valid for strcmp where stop condition is guaranteed to be
	   reachable by just reading memory.  */
	testl	$((VEC_SIZE - 1) << 20), %eax
	jz	L(no_page_cross)
# endif

	movl	%edi, %eax
	movl	%esi, %ecx
	andl	$(PAGE_SIZE - 1), %eax
	andl	$(PAGE_SIZE - 1), %ecx

	xorl	%OFFSET_REG, %OFFSET_REG

	/* Check which is closer to page cross, s1 or s2.  */
	cmpl	%eax, %ecx
	jg	L(page_cross_s2)

	/* The previous page cross check has false positives. Check for
	   true positive as page cross logic is very expensive.  */
	subl	$(PAGE_SIZE - VEC_SIZE * 4), %eax
	jbe	L(no_page_cross)

	/* Set r8 to not interfere with normal return value (rdi and rsi
	   did not swap).  */
# ifdef USE_AS_WCSCMP
	/* any non-zero positive value that doesn't inference with 0x1.
	 */
	movl	$2, %r8d
# else
	xorl	%r8d, %r8d
# endif

	/* Check if less than 1x VEC till page cross.  */
	subl	$(VEC_SIZE * 3), %eax
	jg	L(less_1x_vec_till_page)

	/* If more than 1x VEC till page cross, loop through safely
	   loadable memory until within 1x VEC of page cross.  */

	.p2align 4,, 10
L(page_cross_loop):

	VMOVU	(%rdi, %OFFSET_REG64), %ymm0
	CMP_R1_S2_ymm (%ymm0, (%rsi, %OFFSET_REG64), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx
	incl	%ecx

	jnz	L(check_ret_vec_page_cross)
	addl	$VEC_SIZE, %OFFSET_REG
# ifdef USE_AS_STRNCMP
	cmpq	%OFFSET_REG64, %rdx
	jbe	L(ret_zero_page_cross)
# endif
	addl	$VEC_SIZE, %eax
	jl	L(page_cross_loop)

	subl	%eax, %OFFSET_REG
	/* OFFSET_REG has distance to page cross - VEC_SIZE. Guaranteed
	   to not cross page so is safe to load. Since we have already
	   loaded at least 1 VEC from rsi it is also guaranteed to be
	   safe.  */

	VMOVU	(%rdi, %OFFSET_REG64), %ymm0
	CMP_R1_S2_ymm (%ymm0, (%rsi, %OFFSET_REG64), %ymm2, %ymm1)
	VPCMPEQ	%ymm0, %ymmZERO, %ymm2
	vpandn	%ymm1, %ymm2, %ymm1
	vpmovmskb %ymm1, %ecx

# ifdef USE_AS_STRNCMP
	leal	VEC_SIZE(%OFFSET_REG64), %eax
	cmpq	%rax, %rdx
	jbe	L(check_ret_vec_page_cross2)
	addq	%rdi, %rdx
# endif
	incl	%ecx
	jz	L(prepare_loop_no_len)

	.p2align 4,, 4
L(ret_vec_page_cross):
# ifndef USE_AS_STRNCMP
L(check_ret_vec_page_cross):
# endif
	tzcntl	%ecx, %ecx
	addl	%OFFSET_REG, %ecx
L(ret_vec_page_cross_cont):
# ifdef USE_AS_WCSCMP
	movl	(%rdi, %rcx), %edx
	xorl	%eax, %eax
	cmpl	(%rsi, %rcx), %edx
	je	L(ret12)
	setl	%al
	negl	%eax
	xorl	%r8d, %eax
# else
	movzbl	(%rdi, %rcx), %eax
	movzbl	(%rsi, %rcx), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %ecx)
	subl	%ecx, %eax
	xorl	%r8d, %eax
	subl	%r8d, %eax
# endif
L(ret12):
	VZEROUPPER_RETURN

# ifdef USE_AS_STRNCMP
	.p2align 4,, 10
L(check_ret_vec_page_cross2):
	incl	%ecx
L(check_ret_vec_page_cross):
	tzcntl	%ecx, %ecx
	addl	%OFFSET_REG, %ecx
	cmpq	%rcx, %rdx
	ja	L(ret_vec_page_cross_cont)
	.p2align 4,, 2
L(ret_zero_page_cross):
	xorl	%eax, %eax
	VZEROUPPER_RETURN
# endif

	.p2align 4,, 4
L(page_cross_s2):
	/* Ensure this is a true page cross.  */
	subl	$(PAGE_SIZE - VEC_SIZE * 4), %ecx
	jbe	L(no_page_cross)


	movl	%ecx, %eax
	movq	%rdi, %rcx
	movq	%rsi, %rdi
	movq	%rcx, %rsi

	/* set r8 to negate return value as rdi and rsi swapped.  */
# ifdef USE_AS_WCSCMP
	movl	$-4, %r8d
# else
	movl	$-1, %r8d
# endif
	xorl	%OFFSET_REG, %OFFSET_REG

	/* Check if more than 1x VEC till page cross.  */
	subl	$(VEC_SIZE * 3), %eax
	jle	L(page_cross_loop)

	.p2align 4,, 6
L(less_1x_vec_till_page):
	/* Find largest load size we can use.  */
	cmpl	$16, %eax
	ja	L(less_16_till_page)

	VMOVU	(%rdi), %xmm0
	CMP_R1_S2_xmm (%xmm0, (%rsi), %xmm2, %xmm1)
	VPCMPEQ	%xmm0, %xmmZERO, %xmm2
	vpandn	%xmm1, %xmm2, %xmm1
	vpmovmskb %ymm1, %ecx
	incw	%cx
	jnz	L(check_ret_vec_page_cross)
	movl	$16, %OFFSET_REG
# ifdef USE_AS_STRNCMP
	cmpq	%OFFSET_REG64, %rdx
	jbe	L(ret_zero_page_cross_slow_case0)
	subl	%eax, %OFFSET_REG
# else
	/* Explicit check for 16 byte alignment.  */
	subl	%eax, %OFFSET_REG
	jz	L(prepare_loop)
# endif

	VMOVU	(%rdi, %OFFSET_REG64), %xmm0
	CMP_R1_S2_xmm (%xmm0, (%rsi, %OFFSET_REG64), %xmm2, %xmm1)
	VPCMPEQ	%xmm0, %xmmZERO, %xmm2
	vpandn	%xmm1, %xmm2, %xmm1
	vpmovmskb %ymm1, %ecx
	incw	%cx
	jnz	L(check_ret_vec_page_cross)

# ifdef USE_AS_STRNCMP
	addl	$16, %OFFSET_REG
	subq	%OFFSET_REG64, %rdx
	jbe	L(ret_zero_page_cross_slow_case0)
	subq	$-(VEC_SIZE * 4), %rdx

	leaq	-(VEC_SIZE * 4)(%rdi, %OFFSET_REG64), %rdi
	leaq	-(VEC_SIZE * 4)(%rsi, %OFFSET_REG64), %rsi
# else
	leaq	(16 - VEC_SIZE * 4)(%rdi, %OFFSET_REG64), %rdi
	leaq	(16 - VEC_SIZE * 4)(%rsi, %OFFSET_REG64), %rsi
# endif
	jmp	L(prepare_loop_aligned)

# ifdef USE_AS_STRNCMP
	.p2align 4,, 2
L(ret_zero_page_cross_slow_case0):
	xorl	%eax, %eax
	ret
# endif


	.p2align 4,, 10
L(less_16_till_page):
	/* Find largest load size we can use.  */
	cmpl	$24, %eax
	ja	L(less_8_till_page)

	vmovq	(%rdi), %xmm0
	vmovq	(%rsi), %xmm1
	VPCMPEQ	%xmm0, %xmmZERO, %xmm2
	CMP_R1_R2_xmm (%xmm0, %xmm1, %xmm3, %xmm1)
	vpandn	%xmm1, %xmm2, %xmm1
	vpmovmskb %ymm1, %ecx
	incb	%cl
	jnz	L(check_ret_vec_page_cross)


# ifdef USE_AS_STRNCMP
	cmpq	$8, %rdx
	jbe	L(ret_zero_page_cross_slow_case0)
# endif
	movl	$24, %OFFSET_REG
	/* Explicit check for 16 byte alignment.  */
	subl	%eax, %OFFSET_REG



	vmovq	(%rdi, %OFFSET_REG64), %xmm0
	vmovq	(%rsi, %OFFSET_REG64), %xmm1
	VPCMPEQ	%xmm0, %xmmZERO, %xmm2
	CMP_R1_R2_xmm (%xmm0, %xmm1, %xmm3, %xmm1)
	vpandn	%xmm1, %xmm2, %xmm1
	vpmovmskb %ymm1, %ecx
	incb	%cl
	jnz	L(check_ret_vec_page_cross)

# ifdef USE_AS_STRNCMP
	addl	$8, %OFFSET_REG
	subq	%OFFSET_REG64, %rdx
	jbe	L(ret_zero_page_cross_slow_case0)
	subq	$-(VEC_SIZE * 4), %rdx

	leaq	-(VEC_SIZE * 4)(%rdi, %OFFSET_REG64), %rdi
	leaq	-(VEC_SIZE * 4)(%rsi, %OFFSET_REG64), %rsi
# else
	leaq	(8 - VEC_SIZE * 4)(%rdi, %OFFSET_REG64), %rdi
	leaq	(8 - VEC_SIZE * 4)(%rsi, %OFFSET_REG64), %rsi
# endif
	jmp	L(prepare_loop_aligned)


	.p2align 4,, 10
L(less_8_till_page):
# ifdef USE_AS_WCSCMP
	/* If using wchar then this is the only check before we reach
	   the page boundary.  */
	movl	(%rdi), %eax
	movl	(%rsi), %ecx
	cmpl	%ecx, %eax
	jnz	L(ret_less_8_wcs)
#  ifdef USE_AS_STRNCMP
	addq	%rdi, %rdx
	/* We already checked for len <= 1 so cannot hit that case here.
	 */
#  endif
	testl	%eax, %eax
	jnz	L(prepare_loop_no_len)
	ret

	.p2align 4,, 8
L(ret_less_8_wcs):
	setl	%OFFSET_REG8
	negl	%OFFSET_REG
	movl	%OFFSET_REG, %eax
	xorl	%r8d, %eax
	ret

# else

	/* Find largest load size we can use.  */
	cmpl	$28, %eax
	ja	L(less_4_till_page)

	vmovd	(%rdi), %xmm0
	vmovd	(%rsi), %xmm1
	VPCMPEQ	%xmm0, %xmmZERO, %xmm2
	CMP_R1_R2_xmm (%xmm0, %xmm1, %xmm3, %xmm1)
	vpandn	%xmm1, %xmm2, %xmm1
	vpmovmskb %ymm1, %ecx
	subl	$0xf, %ecx
	jnz	L(check_ret_vec_page_cross)

#  ifdef USE_AS_STRNCMP
	cmpq	$4, %rdx
	jbe	L(ret_zero_page_cross_slow_case1)
#  endif
	movl	$28, %OFFSET_REG
	/* Explicit check for 16 byte alignment.  */
	subl	%eax, %OFFSET_REG



	vmovd	(%rdi, %OFFSET_REG64), %xmm0
	vmovd	(%rsi, %OFFSET_REG64), %xmm1
	VPCMPEQ	%xmm0, %xmmZERO, %xmm2
	CMP_R1_R2_xmm (%xmm0, %xmm1, %xmm3, %xmm1)
	vpandn	%xmm1, %xmm2, %xmm1
	vpmovmskb %ymm1, %ecx
	subl	$0xf, %ecx
	jnz	L(check_ret_vec_page_cross)

#  ifdef USE_AS_STRNCMP
	addl	$4, %OFFSET_REG
	subq	%OFFSET_REG64, %rdx
	jbe	L(ret_zero_page_cross_slow_case1)
	subq	$-(VEC_SIZE * 4), %rdx

	leaq	-(VEC_SIZE * 4)(%rdi, %OFFSET_REG64), %rdi
	leaq	-(VEC_SIZE * 4)(%rsi, %OFFSET_REG64), %rsi
#  else
	leaq	(4 - VEC_SIZE * 4)(%rdi, %OFFSET_REG64), %rdi
	leaq	(4 - VEC_SIZE * 4)(%rsi, %OFFSET_REG64), %rsi
#  endif
	jmp	L(prepare_loop_aligned)

#  ifdef USE_AS_STRNCMP
	.p2align 4,, 2
L(ret_zero_page_cross_slow_case1):
	xorl	%eax, %eax
	ret
#  endif

	.p2align 4,, 10
L(less_4_till_page):
	subq	%rdi, %rsi
	/* Extremely slow byte comparison loop.  */
L(less_4_loop):
	movzbl	(%rdi), %eax
	movzbl	(%rsi, %rdi), %ecx
	TOLOWER_gpr (%rax, %eax)
	TOLOWER_gpr (%rcx, %BYTE_LOOP_REG)
	subl	%BYTE_LOOP_REG, %eax
	jnz	L(ret_less_4_loop)
	testl	%ecx, %ecx
	jz	L(ret_zero_4_loop)
#  ifdef USE_AS_STRNCMP
	decq	%rdx
	jz	L(ret_zero_4_loop)
#  endif
	incq	%rdi
	/* end condition is reach page boundary (rdi is aligned).  */
	testl	$31, %edi
	jnz	L(less_4_loop)
	leaq	-(VEC_SIZE * 4)(%rdi, %rsi), %rsi
	addq	$-(VEC_SIZE * 4), %rdi
#  ifdef USE_AS_STRNCMP
	subq	$-(VEC_SIZE * 4), %rdx
#  endif
	jmp	L(prepare_loop_aligned)

L(ret_zero_4_loop):
	xorl	%eax, %eax
	ret
L(ret_less_4_loop):
	xorl	%r8d, %eax
	subl	%r8d, %eax
	ret
# endif
	cfi_endproc
	.size	STRCMP, .-STRCMP
#endif
