/* pthread_setname_np.  Mach version.
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
#include <pthread.h>
#include <pthreadP.h>
#include <string.h>

#include <pt-internal.h>
#include <shlib-compat.h>
#include <ldsodefs.h>

int
__pthread_setname_np (pthread_t thread, const char *name)
{
#ifdef HAVE_MACH_THREAD_SET_NAME
/* GNU Mach doesn't export this so we have to define it ourselves.  */
#define MACH_THREAD_NAME_MAX 32
  struct __pthread *pthread;
  error_t err;

  /* Lookup the thread structure for THREAD.  */
  pthread = __pthread_getid (thread);
  if (pthread == NULL)
    return ESRCH;

  if (strlen (name) >= MACH_THREAD_NAME_MAX)
    return ERANGE;

  err = __thread_set_name (pthread->kernel_thread, name);
  if (err != KERN_SUCCESS)
    return __hurd_fail (err);
  return 0;
#else
  return ENOTSUP;
#endif
}

libc_hidden_def (__pthread_setname_np)
versioned_symbol (libc, __pthread_setname_np, pthread_setname_np, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_40, GLIBC_2_43)
compat_symbol (libpthread, __pthread_setname_np, pthread_setname_np, GLIBC_2_40);
#endif
