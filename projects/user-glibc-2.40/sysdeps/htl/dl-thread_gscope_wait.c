/* Out-of-line notification function for the GSCOPE locking mechanism.
   Copyright (C) 2007-2024 Free Software Foundation, Inc.
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

#include <ldsodefs.h>
#include <pthread.h>
#include <htl/pt-internal.h>

static inline int *
thread_gscope_flag (struct __pthread *t)
{
#if TLS_TCB_AT_TP
  return &t->tcb->gscope_flag;
#elif TLS_DTV_AT_TP
  return &((tcbprehead_t *) t->tcb - 1)->gscope_flag;
#else
# error "Either TLS_TCB_AT_TP or TLS_DTV_AT_TP must be defined"
#endif
}

void
__thread_gscope_wait (void)
{
  size_t i;
  struct __pthread *t;
  int *gscope_flagp;

  __libc_rwlock_rdlock (GL (dl_pthread_threads_lock));

  /* Iterate over the list of threads.  */
  for (i = 0; i < GL (dl_pthread_num_threads); ++i)
    {
      t = GL (dl_pthread_threads[i]);
      if (t == NULL || *thread_gscope_flag (t) == THREAD_GSCOPE_FLAG_UNUSED)
        continue;

      gscope_flagp = thread_gscope_flag (t);

      /* We have to wait until this thread is done with the global
         scope.  First tell the thread that we are waiting and
         possibly have to be woken.  */
      if (atomic_compare_and_exchange_bool_acq (gscope_flagp,
                                                THREAD_GSCOPE_FLAG_WAIT,
                                                THREAD_GSCOPE_FLAG_USED))
        continue;

      do
        lll_wait (gscope_flagp, THREAD_GSCOPE_FLAG_WAIT, LLL_PRIVATE);
      while (*gscope_flagp == THREAD_GSCOPE_FLAG_WAIT);
    }

  __libc_rwlock_unlock (GL (dl_pthread_threads_lock));
}
