/* C11 threads call once implementation.
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

void
__call_once (once_flag *flag, void (*func)(void))
{
  _Static_assert (sizeof (once_flag) == sizeof (pthread_once_t),
		  "sizeof (once_flag) != sizeof (pthread_once_t)");
  _Static_assert (alignof (once_flag) == alignof (pthread_once_t),
		  "alignof (once_flag) != alignof (pthread_once_t)");
  __pthread_once ((pthread_once_t *) flag, func);
}
versioned_symbol (libc, __call_once, call_once, C11_THREADS_IN_LIBC);
#if OTHER_SHLIB_COMPAT (libpthread, C11_THREADS_INTRODUCED, C11_THREADS_IN_LIBC)
compat_symbol (libpthread, __call_once, call_once, C11_THREADS_INTRODUCED);
#endif
