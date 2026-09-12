/* Cancellable syscall wrapper.  Linux/x86_64 version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <descr-const.h>

/* long int [rax] __syscall_cancel_arch (volatile int *cancelhandling [%rdi],
					 __syscall_arg_t nr   [%rsi],
					 __syscall_arg_t arg1 [%rdx],
					 __syscall_arg_t arg2 [%rcx],
					 __syscall_arg_t arg3 [%r8],
					 __syscall_arg_t arg4 [%r9],
					 __syscall_arg_t arg5 [SP+8],
					 __syscall_arg_t arg6 [SP+16])  */

ENTRY (__syscall_cancel_arch)
	.globl __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	/* if (*cancelhandling & CANCELED_BITMASK)
	     __syscall_do_cancel()  */
	mov    (%rdi),%eax
	testb  $TCB_CANCELED_BITMASK, (%rdi)
	jne    __syscall_do_cancel

	/* Issue a 6 argument syscall, the nr [%rax] being the syscall
	   number.  */
	mov    %rdi,%r11
	mov    %rsi,%rax
	mov    %rdx,%rdi
	mov    %rcx,%rsi
	mov    %r8,%rdx
	mov    %r9,%r10
	mov    8(%rsp),%r8
	mov    16(%rsp),%r9
	mov    %r11,8(%rsp)
	syscall

	.globl __syscall_cancel_arch_end
__syscall_cancel_arch_end:
	ret
END (__syscall_cancel_arch)
