/* pthread_spin_trylock -- trylock a spin lock.  Generic version.
   Copyright (C) 2012-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <atomic.h>
#include "pthreadP.h"
#include <shlib-compat.h>

int
__pthread_spin_trylock (pthread_spinlock_t *lock)
{
  if (atomic_exchange_acquire (lock, 1) == 0)
    return 0;

  return EBUSY;
}
versioned_symbol (libc, __pthread_spin_trylock, pthread_spin_trylock,
		  GLIBC_2_34);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_2, GLIBC_2_34)
compat_symbol (libpthread, __pthread_spin_trylock, pthread_spin_trylock,
	       GLIBC_2_2);
#endif
