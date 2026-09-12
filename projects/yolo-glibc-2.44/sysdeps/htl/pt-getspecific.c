/* pthread_getspecific.  Hurd version.
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

void *
__pthread_getspecific (pthread_key_t key)
{
  struct __pthread *self;

  if (key < 0 || key >= __pthread_key_count)
    return NULL;

  self = _pthread_self ();

  if (self->thread_specifics == NULL)
    {
      if (key >= PTHREAD_STATIC_KEYS)
	return NULL;
      return self->static_thread_specifics[key];
    }

  if (key >= self->thread_specifics_size)
    return 0;

  return self->thread_specifics[key];
}
libc_hidden_def (__pthread_getspecific)
versioned_symbol (libc, __pthread_getspecific, pthread_getspecific, GLIBC_2_42);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_42)
compat_symbol (libpthread, __pthread_getspecific, pthread_getspecific, GLIBC_2_12);
#endif
