/* Invoke the brk system call.  Sparc version.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifdef __arch64__
# define SYSCALL_NUM "0x6d"
#else
# define SYSCALL_NUM "0x10"
#endif

static inline void *
__brk_call (void *addr)
{
  register long int g1 asm ("g1") = __NR_brk;
  register long int o0 asm ("o0") = (long int) addr;
  asm volatile ("ta " SYSCALL_NUM
		: "=r"(o0)
		: "r"(g1), "0"(o0)
		: "cc");
  return (void *) o0;
}
