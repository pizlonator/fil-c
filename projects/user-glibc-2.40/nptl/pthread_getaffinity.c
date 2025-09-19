/* Copyright (C) 2003-2024 Free Software Foundation, Inc.
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
#include <limits.h>
#include <pthreadP.h>
#include <string.h>
#include <sysdep.h>
#include <sys/param.h>
#include <sys/types.h>
#include <shlib-compat.h>
#include <pizlonated_syscalls.h>


int
__pthread_getaffinity_np (pthread_t th, size_t cpusetsize, cpu_set_t *cpuset)
{
  const struct pthread *pd = (const struct pthread *) th;

  int saved_errno = errno;
  int res = zsys_sched_getaffinity (pd->tid, MIN (INT_MAX, cpusetsize), cpuset);
  int result_errno = errno;
  errno = saved_errno;
  if (res < 0)
    return result_errno;

  return 0;
}
libc_hidden_def (__pthread_getaffinity_np)
versioned_symbol (libc, __pthread_getaffinity_np, pthread_getaffinity_np,
		  GLIBC_2_32);

#if SHLIB_COMPAT (libc, GLIBC_2_3_4, GLIBC_2_32)
strong_alias (__pthread_getaffinity_np, __pthread_getaffinity_alias)
compat_symbol (libc, __pthread_getaffinity_alias, pthread_getaffinity_np,
	       GLIBC_2_3_4);
#endif

#if SHLIB_COMPAT (libc, GLIBC_2_3_3, GLIBC_2_3_4)
int
__pthread_getaffinity_old (pthread_t th, cpu_set_t *cpuset)
{
  /* The old interface by default assumed a 1024 processor bitmap.  */
  return __pthread_getaffinity_np (th, 128, cpuset);
}
compat_symbol (libc, __pthread_getaffinity_old, pthread_getaffinity_np,
	       GLIBC_2_3_3);
#endif
