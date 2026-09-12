/* Common definition for pthread_{timed,try}join{_np}.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#include "pthreadP.h"
#include <atomic.h>
#include <stap-probe.h>
#include <time.h>
#include <futex-internal.h>
#include <stdfil.h>

static void
cleanup (void *arg)
{
  /* If we already changed the waiter ID, reset it.  The call cannot
     fail for any reason but the thread not having done that yet so
     there is no reason for a loop.  */
  struct pthread *self = THREAD_SELF;
  atomic_compare_and_exchange_bool_acq ((void **) arg, NULL, self);
}

int
__pthread_clockjoin_ex (pthread_t threadid, void **thread_return,
                        clockid_t clockid,
                        const struct __timespec64 *abstime,
                        bool cancel)
{
  if (cancel)
    __pthread_testcancel ();

  struct pthread *pd = (struct pthread *) threadid;

  /* Make sure the clock and time specified are valid.  */
  if (abstime
      && __glibc_unlikely (!futex_abstimed_supported_clockid (clockid)
			   || ! valid_nanoseconds (abstime->tv_nsec)))
    return EINVAL;

  LIBC_PROBE (pthread_join, 1, threadid);

  int result = 0;

  /* In Fil-C, threads are created by the Fil-C runtime (zthread_create2)
     instead of the clone syscall, so the kernel never sets PD->joinstate
     to THREAD_STATE_EXITED (there is no CLONE_CHILD_CLEARTID notification
     for Fil-C threads).  The exiting thread marks its own termination
     instead: it sets PD->dead and wakes the futex on it, and it flips
     PD->joinid from NULL to 1 (for a joinable thread) or from PD to
     PD + 1 (for a detached thread), so we wait on PD->dead and use
     PD->joinid to keep track of joiners.  */

  struct pthread *self = THREAD_SELF;
  if (pd == self
      && !cancel_enabled_and_canceled (self->cancelhandling))
    return EDEADLK;

  /* POSIX states calling pthread_join on a non joinable thread is
     undefined.  However, if PD is still in the cache we can warn
     the caller.  */
  if (atomic_load_acquire (&pd->joinstate) == THREAD_STATE_DETACHED)
    return EINVAL;

  /* Wait for the thread to finish.  If it is already locked something
     is wrong.  There can only be one waiter.  */
  for (;;)
    {
      struct pthread *joinid = pd->joinid;
      if (joinid != NULL && joinid != (struct pthread *) 1)
	return EINVAL;
      if (!atomic_compare_and_exchange_bool_acq (&pd->joinid, self, joinid))
	break;
    }

  /* BLOCK waits either indefinitely or based on an absolute time.  POSIX also
     states a cancellation point shall occur for pthread_join, and we use the
     same rationale for posix_timedjoin_np.  Both clockwait_tid and the futex
     call use the cancellable variant.  */

  /* During the wait we change to asynchronous cancellation.  If we
     are cancelled the thread we are waiting for must be marked as
     un-wait-ed for again.  */
  pthread_cleanup_push (cleanup, &pd->joinid);

  /* We need acquire MO here so that we synchronize with the exiting
     thread's store to PD->dead.  */
  int dead;
  while ((dead = atomic_load_acquire (&pd->dead)) == 0)
    {
      /* The exiting thread itself sets PD->dead and wakes the futex on it
	 (the kernel does not do it for Fil-C threads), so this uses a
	 private futex.  */
      int ret = cancel
	? __futex_abstimed_wait_cancelable64 ((unsigned int *) &pd->dead, dead,
					      clockid, abstime, LLL_PRIVATE)
	: __futex_abstimed_wait64 ((unsigned int *) &pd->dead, dead, clockid,
				   abstime, LLL_PRIVATE);
      if (ret == ETIMEDOUT || ret == EOVERFLOW)
	{
	  result = ret;
	  break;
	}
    }

  pthread_cleanup_pop (0);

  void *pd_result = pd->result;
  if (__glibc_likely (result == 0))
    {
      /* Store the return value if the caller is interested.  */
      if (thread_return != NULL)
	*thread_return = pd_result;

      /* Free the TCB.  */
      __nptl_free_tcb (pd);
    }
  else
    /* We did not reap the thread, so give up our claim on it.  */
    pd->joinid = NULL;

  LIBC_PROBE (pthread_join_ret, 3, threadid, result, pd_result);

  return result;
}
