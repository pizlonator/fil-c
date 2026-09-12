/* pthread_barrierattr_init.  Generic version.
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
#include <pt-internal.h>
#include <shlib-compat.h>

int
__pthread_barrierattr_init (pthread_barrierattr_t *attr)
{
  ASSERT_TYPE_SIZE (pthread_barrierattr_t, __SIZEOF_PTHREAD_BARRIERATTR_T);

  *attr = __pthread_default_barrierattr;
  return 0;
}
libc_hidden_def (__pthread_barrierattr_init)
versioned_symbol (libc, __pthread_barrierattr_init, pthread_barrierattr_init, GLIBC_2_42);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_42)
compat_symbol (libpthread, __pthread_barrierattr_init, pthread_barrierattr_init, GLIBC_2_12);
#endif
