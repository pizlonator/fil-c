/* Copy memory block and return pointer to beginning of destination block
   For Intel 80x86, x>=6.
   This file is part of the GNU C Library.
   Copyright (C) 2003-2026 Free Software Foundation, Inc.

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
#include "asm-syntax.h"

#define PARMS	4+4	/* one spilled register */
#define RTN	PARMS

	.text

#define DEST	RTN
#define SRC	DEST+4
#define LEN	SRC+4

#if defined SHARED && IS_IN (libc)
ENTRY_CHK (__memmove_chk)
	movl	12(%esp), %eax
	cmpl	%eax, 16(%esp)
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END_CHK (__memmove_chk)
libc_hidden_builtin_def (__memmove_chk)
#endif

ENTRY (memmove)

	pushl	%edi
	cfi_adjust_cfa_offset (4)

	movl	LEN(%esp), %ecx
	movl	DEST(%esp), %edi
	cfi_rel_offset (edi, 0)
	movl	%esi, %edx
	movl	SRC(%esp), %esi
	cfi_register (esi, edx)

	movl	%edi, %eax
	subl	%esi, %eax
	cmpl	%eax, %ecx
	ja	3f

	cld
	shrl	$1, %ecx
	jnc	1f
	movsb
1:	shrl	$1, %ecx
	jnc	2f
	movsw
2:	rep
	movsl
	movl	%edx, %esi
	cfi_restore (esi)
	movl	DEST(%esp), %eax

	popl	%edi
	cfi_adjust_cfa_offset (-4)
	cfi_restore (edi)

	ret

	cfi_adjust_cfa_offset (4)
	cfi_rel_offset (edi, 0)
	cfi_register (esi, edx)

	/* Backward copying.  */
3:	std
	leal	-1(%edi, %ecx), %edi
	leal	-1(%esi, %ecx), %esi
	shrl	$1, %ecx
	jnc	1f
	movsb
1:	subl	$1, %edi
	subl	$1, %esi
	shrl	$1, %ecx
	jnc	2f
	movsw
2:	subl	$2, %edi
	subl	$2, %esi
	rep
	movsl
	movl	%edx, %esi
	cfi_restore (esi)
	movl	DEST(%esp), %eax

	cld
	popl	%edi
	cfi_adjust_cfa_offset (-4)
	cfi_restore (edi)

	ret
END (memmove)
libc_hidden_builtin_def (memmove)
