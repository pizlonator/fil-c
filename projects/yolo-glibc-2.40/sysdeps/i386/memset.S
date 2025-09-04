/* memset with REP MOVSB/STOSB
   Copyright (C) 2015-2024 Free Software Foundation, Inc.
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

#define CFI_PUSH(REG)						\
  cfi_adjust_cfa_offset (4);					\
  cfi_rel_offset (REG, 0)

#define CFI_POP(REG)						\
  cfi_adjust_cfa_offset (-4);					\
  cfi_restore (REG)

#define PUSH(REG)	pushl REG; CFI_PUSH (REG)
#define POP(REG)	popl REG; CFI_POP (REG)

#define STR1  8
#define STR2  STR1+4
#define N     STR2+4

	.text
#if defined SHARED && IS_IN (libc)
ENTRY (__memset_chk)
	movl	12(%esp), %eax
	cmpl	%eax, 16(%esp)
	jb	HIDDEN_JUMPTARGET (__chk_fail)
END (__memset_chk)
libc_hidden_builtin_def (__memset_chk)
#endif
ENTRY (memset)
	PUSH    (%edi)
	movl	N(%esp), %ecx
	movl	STR1(%esp), %edi
	movzbl	STR2(%esp), %eax
	mov	%edi, %edx
	rep	stosb
	mov	%edx, %eax
	POP     (%edi)
	ret
END (memset)

libc_hidden_builtin_def (memset)
