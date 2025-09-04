/* Types for registers for sys/procfs.h.  OpenRISC version.
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

#ifndef _SYS_PROCFS_H
# error "Never include <bits/procfs.h> directly; use <sys/procfs.h> instead."
#endif

#include <sys/ucontext.h>

#define ELF_NGREG __NGREG

/* Type for a general-purpose register.  */
typedef unsigned long int elf_greg_t;

/* And the array of general-purpose registers.  We could have used `struct
   user_regs' directly, but tradition says that the register set is an array,
   which does have some peculiar semantics, so leave it that way.
   GDB uses this for prtrace GETREGSET, on OpenRISC the regset contains 32
   gprs the PC and the SR, 34 longs.  */
typedef elf_greg_t elf_gregset_t[34];

/* Register set for the floating-point registers.  */
typedef elf_greg_t elf_fpregset_t[32];
