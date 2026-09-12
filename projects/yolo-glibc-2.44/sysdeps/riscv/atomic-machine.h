/* Low-level functions for atomic operations. RISC-V version.
   Copyright (C) 2014-2026 Free Software Foundation, Inc.
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

#ifndef _LINUX_RISCV_BITS_ATOMIC_H
#define _LINUX_RISCV_BITS_ATOMIC_H 1

#ifdef __riscv_atomic

/* Miscellaneous.  */

# define asm_amo(which, ordering, mem, value) ({ 		\
  __atomic_check_size (mem);					\
  typeof (*mem) __tmp; 						\
  if (sizeof (__tmp) == 4)					\
    asm volatile (which ".w" ordering "\t%0, %z2, %1"		\
		  : "=r" (__tmp), "+A" (* (mem))		\
		  : "rJ" (value));				\
  else if (sizeof (__tmp) == 8)					\
    asm volatile (which ".d" ordering "\t%0, %z2, %1"		\
		  : "=r" (__tmp), "+A" (* (mem))		\
		  : "rJ" (value));				\
  else								\
    abort ();							\
  __tmp; })

# define atomic_max(mem, value) asm_amo ("amomaxu", ".aq", mem, value)
# define atomic_min(mem, value) asm_amo ("amominu", ".aq", mem, value)

#else /* __riscv_atomic */
# error "ISAs that do not subsume the A extension are not supported"
#endif /* !__riscv_atomic */

/* Execute a PAUSE hint when spinning.  */
#define atomic_spin_nop() __asm(".insn i 0x0f, 0, x0, x0, 0x010")

#endif /* bits/atomic.h */
