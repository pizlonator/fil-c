/* memset implement with rep stosb
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
ENTRY (__memset_chk_erms)
	cmp	%RDX_LP, %RCX_LP
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (__memset_chk_erms)

/* Only used to measure performance of REP STOSB.  */
ENTRY (__memset_erms)
	/* Skip zero length.  */
	test	%RDX_LP, %RDX_LP
	jz	 L(stosb_return_zero)
	mov	%RDX_LP, %RCX_LP
	movzbl	%sil, %eax
	mov	%RDI_LP, %RDX_LP
	rep stosb
	mov	%RDX_LP, %RAX_LP
	ret
L(stosb_return_zero):
	movq	%rdi, %rax
	ret
END (__memset_erms)
#endif
