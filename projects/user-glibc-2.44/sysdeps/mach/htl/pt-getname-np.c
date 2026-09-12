/* pthread_getname_np.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <hurd.h>
#include <mach_debug/mach_debug_types.h>
#include <pthread.h>
#include <pthreadP.h>
#include <string.h>

#include <pt-internal.h>
#include <string.h>
#include <shlib-compat.h>
#include <ldsodefs.h>

int
__pthread_getname_np (pthread_t thread, char *buf, size_t len)
{
#ifdef HAVE_MACH_THREAD_GET_NAME
/* GNU Mach doesn't export this so we have to define it ourselves.  */
#define MACH_THREAD_NAME_MAX 32
  struct __pthread *pthread;
  error_t err;
  kernel_debug_name_t tmp;

  /* Note that we don't check for len to be MACH_THREAD_NAME_MAX
   * since we want to be more compatible with the Linux API which
   * requires that the buffer is at least 16 bytes long.
   *
   * We check for at least 1 byte since we truncate the result below.  */
  if (len < 1)
    return ERANGE;
  if (len > MACH_THREAD_NAME_MAX)
    len = MACH_THREAD_NAME_MAX;

  /* Lookup the thread structure for THREAD.  */
  pthread = __pthread_getid (thread);
  if (pthread == NULL)
    return ESRCH;

  /* __thread_get_name expects a buffer of size sizeof (kernel_debug_name_t)
   * and anything smaller will overflow.  */
  err = __thread_get_name (pthread->kernel_thread, tmp);
  if (err != KERN_SUCCESS)
    return __hurd_fail (err);
  /* Truncate the source name to fit in the destination buffer.  */
  tmp[len - 1] = '\0';
  memcpy (buf, tmp, len);

  return 0;
#else
  return ENOTSUP;
#endif
}

libc_hidden_def (__pthread_getname_np)
versioned_symbol (libc, __pthread_getname_np, pthread_getname_np, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_40, GLIBC_2_43)
compat_symbol (libc, __pthread_getname_np, pthread_getname_np, GLIBC_2_40);
#endif
