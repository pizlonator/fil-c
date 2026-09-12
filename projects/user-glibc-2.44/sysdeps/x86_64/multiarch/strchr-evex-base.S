/* Placeholder function, not used by any processor at the moment.
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

/* UNUSED. Exists purely as reference implementation.  */

#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# include <sysdep.h>

# ifdef USE_AS_WCSCHR
#  define CHAR_REG	esi
#  define CHAR_SIZE	4
#  define VPBROADCAST   vpbroadcastd
#  define VPCMP		vpcmpd
#  define VPCMPNE	vpcmpneqd
#  define VPMINU	vpminud
#  define VPTEST	vptestmd
#  define VPTESTN	vptestnmd
# else
#  define CHAR_REG	sil
#  define CHAR_SIZE	1
#  define VPBROADCAST   vpbroadcastb
#  define VPCMP		vpcmpb
#  define VPCMPNE	vpcmpneqb
#  define VPMINU	vpminub
#  define VPTEST	vptestmb
#  define VPTESTN	vptestnmb
# endif

# define PAGE_SIZE	4096
# define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)
# define VEC_MATCH_MASK ((1 << CHAR_PER_VEC) - 1)

	.section SECTION(.text), "ax", @progbits
/* Aligning entry point to 64 byte, provides better performance for
   one vector length string.  */
ENTRY_P2ALIGN (STRCHR, 6)

	/* Broadcast CHAR to VMM(0).  */
	VPBROADCAST %esi, %VMM(0)
	movl	%edi, %eax
	sall	$20,%eax
	cmpl	$((PAGE_SIZE - VEC_SIZE) << 20), %eax
	ja	L(page_cross)

	VMOVU	(%rdi), %VMM(1)
	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPTEST	%VMM(1), %VMM(1), %k0{%k1}
	KMOV	%k0, %VRAX
	/* Compare [w]char for null, mask bit will be set for match.  */

# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jz	L(align_more)

	bsf	%VRAX, %VRAX

# ifdef USE_AS_WCSCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	add	%rdi, %rax
# endif
# ifndef USE_AS_STRCHRNUL
	cmp	(%rax), %CHAR_REG
	jne	L(zero)
	ret
L(zero):
	xorl	%eax, %eax
# endif
	ret

L(ret_vec_x3):
	subq	$-VEC_SIZE, %rdi
L(ret_vec_x2):
	subq	$-VEC_SIZE, %rdi
L(ret_vec_x1):
	bsf     %VRAX, %VRAX
# ifdef USE_AS_WCSCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	add	%rdi, %rax
# endif

# ifndef USE_AS_STRCHRNUL
	cmp	(%rax), %CHAR_REG
	jne	L(zero)
# endif
	ret

L(page_cross):
	mov	%rdi, %rax
	movl	%edi, %ecx
# ifdef USE_AS_WCSCHR
	/* Calculate number of compare result bits to be skipped for
	   wide string alignment adjustment.  */
	andl	$(VEC_SIZE - 1), %ecx
	sarl	$2, %ecx
# endif
	/* ecx contains number of w[char] to be skipped as a result
	   of address alignment.  */
	andq    $-VEC_SIZE, %rax

	VMOVA	(%rax), %VMM(1)
	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPTEST	%VMM(1), %VMM(1), %k0{%k1}
	KMOV	%k0, %VRAX
	sar	%cl, %VRAX
#ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
#else
	inc	%VRAX
#endif
	/* Ignore number of character for alignment adjustment.  */
	jz	L(align_more)

	bsf	%VRAX, %VRAX
# ifdef USE_AS_WCSCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	addq	%rdi, %rax
# endif

# ifndef USE_AS_STRCHRNUL
	cmp	(%rax), %CHAR_REG
	jne	L(zero)
# endif
	ret

L(align_more):
	/* Align rax to VEC_SIZE.  */
	andq	$-VEC_SIZE, %rdi

	/* Loop unroll 4 times for 4 vector loop.  */
	VMOVA	VEC_SIZE(%rdi), %VMM(1)
	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPTEST	%VMM(1), %VMM(1), %k0{%k1}

	/* Increment rdi by vector size for further comparison and
	   return.  */
	subq	$-VEC_SIZE, %rdi
	KMOV	%k0, %VRAX

# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jnz	L(ret_vec_x1)

	VMOVA	VEC_SIZE(%rdi), %VMM(1)
	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPTEST	%VMM(1), %VMM(1), %k0{%k1}
	KMOV	%k0, %VRAX
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jnz	L(ret_vec_x2)

	VMOVA	(VEC_SIZE * 2)(%rdi), %VMM(1)
	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPTEST	%VMM(1), %VMM(1), %k0{%k1}
	KMOV	%k0, %VRAX
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jnz	L(ret_vec_x3)

	VMOVA	(VEC_SIZE * 3)(%rdi), %VMM(1)
	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPTEST	%VMM(1), %VMM(1), %k0{%k1}
	KMOV	%k0, %VRDX
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRDX
# else
	inc	%VRDX
# endif
	jnz	L(ret_vec_x4)


	/* Align address to VEC_SIZE * 4 for loop.  */
	andq	$-(VEC_SIZE * 4), %rdi
L(loop):
	/* VPMINU and VPCMP combination provide better performance as
	   compared to alternative combinations.  */
	VMOVA	(VEC_SIZE * 4)(%rdi), %VMM(1)
	VMOVA	(VEC_SIZE * 5)(%rdi), %VMM(2)
	VMOVA	(VEC_SIZE * 6)(%rdi), %VMM(3)
	VMOVA	(VEC_SIZE * 7)(%rdi), %VMM(4)

	VPCMPNE	%VMM(1), %VMM(0), %k1
	VPCMPNE	%VMM(2), %VMM(0), %k2

	VPMINU	%VMM(2), %VMM(1), %VMM(2)

	VPCMPNE	%VMM(3), %VMM(0), %k3{%k1}
	VPCMPNE	%VMM(4), %VMM(0), %k4{%k2}

	VPMINU	%VMM(4), %VMM(3), %VMM(4)
	VPMINU	%VMM(2), %VMM(4), %VMM(4){%k3}{z}

	VPTEST	%VMM(4), %VMM(4), %k5{%k4}

	KMOV	%k5, %VRDX
	subq	$-(VEC_SIZE * 4), %rdi
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRDX
# else
	inc	%VRDX
# endif
	jz	L(loop)

	VPTEST	%VMM(1), %VMM(1), %k0{%k1}
	KMOV	%k0, %VRAX
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jnz	L(ret_vec_x1)

	VPTEST	%VMM(2), %VMM(2), %k0{%k2}
	KMOV	%k0, %VRAX
	/* At this point, if k1 is non zero, null char must be in the
	   second vector.  */
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jnz	L(ret_vec_x2)

	VPTEST	%VMM(3), %VMM(3), %k0{%k3}
	KMOV	%k0, %VRAX
# ifdef USE_AS_WCSCHR
	sub	$VEC_MATCH_MASK, %VRAX
# else
	inc	%VRAX
# endif
	jnz	L(ret_vec_x3)
	/* At this point null [w]char must be in the fourth vector so no
	   need to check.  */

L(ret_vec_x4):
	bsf	%VRDX, %VRDX
	leaq	(VEC_SIZE * 3)(%rdi, %rdx, CHAR_SIZE), %rax
# ifndef USE_AS_STRCHRNUL
	cmp	(%rax), %CHAR_REG
	jne	L(zero_2)
# endif
	ret

# ifndef USE_AS_STRCHRNUL
L(zero_2):
	xor	%eax, %eax
	ret
# endif
END (STRCHR)
#endif
