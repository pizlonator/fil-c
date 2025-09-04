/* Legacy shadow stack code.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

	.text
	.globl	legacy
	.type	legacy, @function
legacy:
	.cfi_startproc
#ifdef __x86_64__
	movq	(%rsp), %rax
	addq	$8, %rsp
	jmp	*%rax
#else
	movl	(%esp), %eax
	addl	$4, %esp
	jmp	*%eax
#endif
	.cfi_endproc
	.size	legacy, .-legacy
	.section	.note.GNU-stack,"",@progbits
