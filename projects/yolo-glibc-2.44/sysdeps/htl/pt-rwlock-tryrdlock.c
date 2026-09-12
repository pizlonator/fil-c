/* Try to acquire a rwlock for reading.  Generic version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <pthread.h>
#include <assert.h>
#include <pt-internal.h>
#include <shlib-compat.h>

/* Try to acquire RWLOCK.  */
int
__pthread_rwlock_tryrdlock (struct __pthread_rwlock *rwlock)
{
  __pthread_spin_wait (&rwlock->__lock);
  if (__pthread_spin_trylock (&rwlock->__held) == 0)
    /* Successfully acquired the lock.  */
    {
      assert (rwlock->__readerqueue == 0);
      assert (rwlock->__writerqueue == 0);
      assert (rwlock->__readers == 0);

      rwlock->__readers = 1;
      __pthread_spin_unlock (&rwlock->__lock);
      return 0;
    }
  else
    /* Lock is held, but is held by a reader?  */
  if (rwlock->__readers > 0)
    {
      assert (rwlock->__readerqueue == 0);
      rwlock->__readers++;
      __pthread_spin_unlock (&rwlock->__lock);
      return 0;
    }

  /* The lock is busy.  */

  __pthread_spin_unlock (&rwlock->__lock);

  return EBUSY;
}
libc_hidden_def (__pthread_rwlock_tryrdlock)
versioned_symbol (libc, __pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock, GLIBC_2_42);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_42)
compat_symbol (libpthread, __pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock, GLIBC_2_12);
#endif
