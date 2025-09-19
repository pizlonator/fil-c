/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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
#include "pthreadP.h"
#include <atomic.h>
#include <shlib-compat.h>

int
___pthread_detach (pthread_t th)
{
  struct pthread *pd = (struct pthread *) th;

  /* Make sure the descriptor is valid.  */
  if (INVALID_NOT_TERMINATED_TD_P (pd))
    /* Not a valid thread handle.  */
    return ESRCH;

  for (;;)
    {
      struct pthread *joinid = pd->joinid;
      /* Here are the possibilities:
         
         - joinid == NULL.
         
           The thread was joinable. We'll just make it detached instead. Nothing else to do.
         
         - joinid == 1.
           
           The thread was joinable and is now done executing, but has not deleted pd. So, we have to
           join it.
         
         - joinid == pd or joinid == pd + 1.
         
           The thread was already detached. Glibc semantics say that we return EINVAL.
           
         - Any other case.
         
           The thread is already being joined. Glibc semantics say that we just return 0. */

      if (joinid == (struct pthread *) 1)
        return __pthread_join(th, NULL);

      if (joinid == pd || joinid == (struct pthread *) ((char*)pd + 1))
        return EINVAL;

      if (joinid != NULL)
        return 0;

      if (!atomic_compare_and_exchange_bool_acq (&pd->joinid, pd, NULL))
        return 0;
    }
}
versioned_symbol (libc, ___pthread_detach, pthread_detach, GLIBC_2_34);
libc_hidden_ver (___pthread_detach, __pthread_detach)
#ifndef SHARED
strong_alias (___pthread_detach, __pthread_detach)
#endif

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_0, GLIBC_2_34)
compat_symbol (libc, ___pthread_detach, pthread_detach, GLIBC_2_0);
#endif
