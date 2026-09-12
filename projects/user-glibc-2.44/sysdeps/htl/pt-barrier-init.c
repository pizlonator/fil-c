/* pthread_barrier_init.  Generic version.
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
#include <string.h>
#include <assert.h>
#include <shlib-compat.h>
#include <pt-internal.h>

int
__pthread_barrier_init (pthread_barrier_t *barrier,
		      const pthread_barrierattr_t *attr, unsigned count)
{
  ASSERT_TYPE_SIZE (pthread_barrier_t, __SIZEOF_PTHREAD_BARRIER_T);

  if (count == 0)
    return EINVAL;

  memset (barrier, 0, sizeof *barrier);

  barrier->__lock = PTHREAD_SPINLOCK_INITIALIZER;
  barrier->__pending = count;
  barrier->__count = count;

  if (attr == NULL
      || memcmp (attr, &__pthread_default_barrierattr, sizeof (*attr)) == 0)
    /* Use the default attributes.  */
    return 0;

  /* Non-default attributes.  */

  barrier->__attr = malloc (sizeof *attr);
  if (barrier->__attr == NULL)
    return ENOMEM;

  *barrier->__attr = *attr;
  return 0;
}
libc_hidden_def (__pthread_barrier_init)
versioned_symbol (libc, __pthread_barrier_init, pthread_barrier_init, GLIBC_2_42);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_42)
compat_symbol (libpthread, __pthread_barrier_init, pthread_barrier_init, GLIBC_2_12);
#endif
