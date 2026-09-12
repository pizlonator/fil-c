/* Return a thread's cpu clockid.  Generic version.
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
#include <time.h>

#include <pt-internal.h>
#include <shlib-compat.h>

int
__pthread_getcpuclockid (pthread_t thread, clockid_t *clock)
{
#ifdef CLOCK_THREAD_CPUTIME_ID
  *clock = CLOCK_THREAD_CPUTIME_ID;
  return 0;
#else
  return ENOSYS;
#endif
}

stub_warning (pthread_getcpuclockid)
versioned_symbol (libc, __pthread_getcpuclockid, pthread_getcpuclockid, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libpthread, __pthread_getcpuclockid, pthread_getcpuclockid, GLIBC_2_12);
#endif
