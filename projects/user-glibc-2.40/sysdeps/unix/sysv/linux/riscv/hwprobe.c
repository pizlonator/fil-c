/* RISC-V hardware feature probing support on Linux
   Copyright (C) 2024 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sys/syscall.h>
#include <sys/hwprobe.h>
#include <sysdep.h>
#include <sysdep-vdso.h>

int __riscv_hwprobe (struct riscv_hwprobe *pairs, size_t pair_count,
		     size_t cpu_count, unsigned long int *cpus,
		     unsigned int flags)
{
  int r;

  r = INTERNAL_VSYSCALL (riscv_hwprobe, 5, pairs, pair_count,
                         cpu_count, cpus, flags);

  /* Negate negative errno values to match pthreads API. */
  return -r;
}
libc_hidden_def (__riscv_hwprobe)
