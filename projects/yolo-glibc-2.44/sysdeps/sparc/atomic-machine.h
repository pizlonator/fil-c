/* Atomic operations.  Sparc version.
   Copyright (C) 2019-2026 Free Software Foundation, Inc.
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

#ifndef _SPARC_ATOMIC_MACHINE_H
#define _SPARC_ATOMIC_MACHINE_H

#ifdef __sparc_v9__
# define atomic_full_barrier() \
  __asm __volatile ("membar #LoadLoad | #LoadStore"			      \
		    " | #StoreLoad | #StoreStore" : : : "memory")
# define atomic_read_barrier() \
  __asm __volatile ("membar #LoadLoad | #LoadStore" : : : "memory")
# define atomic_write_barrier() \
  __asm __volatile ("membar #LoadStore | #StoreStore" : : : "memory")

extern void __cpu_relax (void);
# define atomic_spin_nop() __cpu_relax ()
#endif

#endif
