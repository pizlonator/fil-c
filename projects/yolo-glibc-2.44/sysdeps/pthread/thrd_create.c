/* C11 threads thread creation implementation.
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

#include "thrd_priv.h"
#include <shlib-compat.h>
#include <c11-thread.h>

int
__thrd_create (thrd_t *thr, thrd_start_t func, void *arg)
{
  _Static_assert (sizeof (thrd_t) == sizeof (pthread_t),
		  "sizeof (thrd_t) != sizeof (pthread_t)");

  int err_code = __pthread_create (thr, ATTR_C11_THREAD,
				   (void* (*) (void*))func, arg);
  return thrd_err_map (err_code);
}
versioned_symbol (libc, __thrd_create, thrd_create, C11_THREADS_IN_LIBC);
#if OTHER_SHLIB_COMPAT (libpthread, C11_THREADS_INTRODUCED, C11_THREADS_IN_LIBC)
compat_symbol (libpthread, __thrd_create, thrd_create, C11_THREADS_INTRODUCED);
#endif
