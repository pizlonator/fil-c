/* C11 threads mutex initialization implementation.
   Copyright (C) 2018-2026 Free Software Foundation, Inc.
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

#include <stdalign.h>
#include <shlib-compat.h>

#include "thrd_priv.h"
#include <c11-thread.h>

int
__mtx_init (mtx_t *mutex, int type)
{
  _Static_assert (sizeof (mtx_t) == sizeof (pthread_mutex_t),
		  "sizeof (mtx_t) != sizeof (pthread_mutex_t)");
  _Static_assert (alignof (mtx_t) == alignof (pthread_mutex_t),
		  "alignof (mtx_t) != alignof (pthread_mutex_t)");

  pthread_mutexattr_t attr;

  __pthread_mutexattr_init (&attr);

  /* Another possible solution would be to set the flags directly in
     mutex object. */
  switch (type)
  {
    case mtx_plain | mtx_recursive:
    case mtx_timed | mtx_recursive:
      __pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
      break;
    case mtx_plain:
    case mtx_timed: /* No difference between both in standard */
    default:
      __pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_NORMAL);
      break;
  }

  int err_code = __pthread_mutex_init ((pthread_mutex_t *) mutex, &attr);
  /* pthread_mutexattr_destroy implementation is a noop.  */
  return thrd_err_map (err_code);
}
versioned_symbol (libc, __mtx_init, mtx_init, C11_THREADS_IN_LIBC);
#if OTHER_SHLIB_COMPAT (libpthread, C11_THREADS_INTRODUCED, C11_THREADS_IN_LIBC)
compat_symbol (libpthread, __mtx_init, mtx_init, C11_THREADS_INTRODUCED);
#endif
