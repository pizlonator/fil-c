/* Startup code for statically linked Hurd/x86_64 binaries.
   Copyright (C) 1998-2024 Free Software Foundation, Inc.
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
	.globl _start
_start:

	leaq __memcpy_sse2_unaligned(%rip), %rax
	movq %rax, memcpy@GOTPCREL(%rip)
	leaq __strlen_sse2(%rip), %rax
	movq %rax, strlen@GOTPCREL(%rip)

	movq %rsp, %rdi
	call _hurd_stack_setup
	xorq %rdx, %rdx
	jmp _start1

#define _start _start1
#include <sysdeps/x86_64/start.S>
