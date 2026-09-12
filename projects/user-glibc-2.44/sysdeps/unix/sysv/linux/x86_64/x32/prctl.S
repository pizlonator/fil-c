/* The prctl system call.  Linux/x32 version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

/* Clear upper 32-bits in the last 4 arguments.  Since the first argument
   of prctl is int, leave it alone.  */
#undef	DO_CALL
#define DO_CALL(syscall_name, args, ulong_arg_1, ulong_arg_2) \
  movl %esi, %esi;			\
  movl %edx, %edx;			\
  movl %ecx, %r10d;			\
  movl %r8d, %r8d;			\
  movl $SYS_ify (syscall_name), %eax;	\
  syscall;

PSEUDO (__prctl, prctl, 5)
	ret
PSEUDO_END (__prctl)

libc_hidden_def (__prctl)
weak_alias (__prctl, prctl)
