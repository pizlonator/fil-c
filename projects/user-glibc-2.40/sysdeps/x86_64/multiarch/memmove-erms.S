/* memcpy/mempcpy/memmove implement with rep movsb
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


#include <sysdep.h>

#if defined USE_MULTIARCH && IS_IN (libc)
	.text
ENTRY (__mempcpy_chk_erms)
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (__mempcpy_chk_erms)

/* Only used to measure performance of REP MOVSB.  */
ENTRY (__mempcpy_erms)
	mov	%RDI_LP, %RAX_LP
	/* Skip zero length.  */
	test	%RDX_LP, %RDX_LP
	jz	2f
	add	%RDX_LP, %RAX_LP
	jmp	L(start_movsb)
END (__mempcpy_erms)

ENTRY (__memmove_chk_erms)
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (__memmove_chk_erms)

ENTRY (__memmove_erms)
	movq	%rdi, %rax
	/* Skip zero length.  */
	test	%RDX_LP, %RDX_LP
	jz	2f
L(start_movsb):
	mov	%RDX_LP, %RCX_LP
	cmp	%RSI_LP, %RDI_LP
	jb	1f
	/* Source == destination is less common.  */
	je	2f
	lea	(%rsi,%rcx), %RDX_LP
	cmp	%RDX_LP, %RDI_LP
	jb	L(movsb_backward)
1:
	rep movsb
2:
	ret
L(movsb_backward):
	leaq	-1(%rdi,%rcx), %rdi
	leaq	-1(%rsi,%rcx), %rsi
	std
	rep movsb
	cld
	ret
END (__memmove_erms)
strong_alias (__memmove_erms, __memcpy_erms)
strong_alias (__memmove_chk_erms, __memcpy_chk_erms)
#endif
