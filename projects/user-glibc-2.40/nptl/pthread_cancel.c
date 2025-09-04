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
#include <signal.h>
#include <stdlib.h>
#include "pthreadP.h"
#include <atomic.h>
#include <sysdep.h>
#include <unistd.h>
#include <unwind-link.h>
#include <stdio.h>
#include <gnu/lib-names.h>
#include <sys/single_threaded.h>

/* For asynchronous cancellation we use a signal.  */
static void
sigcancel_handler (int sig, siginfo_t *si, void *ctx)
{
  /* Safety check.  It would be possible to call this function for
     other signals and send a signal from another process.  This is not
     correct and might even be a security problem.  Try to catch as
     many incorrect invocations as possible.  */
  if (sig != SIGCANCEL
      || si->si_pid != __getpid()
      || si->si_code != SI_TKILL)
    return;

  struct pthread *self = THREAD_SELF;

  int oldval = atomic_load_relaxed (&self->cancelhandling);
  while (1)
    {
      /* We are canceled now.  When canceled by another thread this flag
	 is already set but if the signal is directly send (internally or
	 from another process) is has to be done here.  */
      int newval = oldval | CANCELING_BITMASK | CANCELED_BITMASK;

      if (oldval == newval || (oldval & EXITING_BITMASK) != 0)
	/* Already canceled or exiting.  */
	break;

      if (atomic_compare_exchange_weak_acquire (&self->cancelhandling,
						&oldval, newval))
	{
	  self->result = PTHREAD_CANCELED;

	  /* Make sure asynchronous cancellation is still enabled.  */
	  if ((oldval & CANCELTYPE_BITMASK) != 0)
	    /* Run the registered destructors and terminate the thread.  */
	    __do_cancel ();
	}
    }
}

int
__pthread_cancel (pthread_t th)
{
  volatile struct pthread *pd = (volatile struct pthread *) th;

  if (pd->tid == 0)
    /* The thread has already exited on the kernel side.  Its outcome
       (regular exit, other cancelation) has already been
       determined.  */
    return 0;

  static int init_sigcancel = 0;
  if (atomic_load_relaxed (&init_sigcancel) == 0)
    {
      struct sigaction sa;
      sa.sa_sigaction = sigcancel_handler;
      /* The signal handle should be non-interruptible to avoid the risk of
	 spurious EINTR caused by SIGCANCEL sent to process or if
	 pthread_cancel() is called while cancellation is disabled in the
	 target thread.  */
      sa.sa_flags = SA_SIGINFO | SA_RESTART;
      __sigemptyset (&sa.sa_mask);
      __libc_sigaction (SIGCANCEL, &sa, NULL);
      atomic_store_relaxed (&init_sigcancel, 1);
    }

#ifdef SHARED
  /* Trigger an error if libgcc_s cannot be loaded.  */
  {
    struct unwind_link *unwind_link = __libc_unwind_link_get ();
    if (unwind_link == NULL)
      __libc_fatal (LIBGCC_S_SO
		    " must be installed for pthread_cancel to work\n");
  }
#endif

  /* Some syscalls are never restarted after being interrupted by a signal
     handler, regardless of the use of SA_RESTART (they always fail with
     EINTR).  So pthread_cancel cannot send SIGCANCEL unless the cancellation
     is enabled and set as asynchronous (in this case the cancellation will
     be acted in the cancellation handler instead by the syscall wrapper).
     Otherwise the target thread is set as 'cancelling' (CANCELING_BITMASK)
     by atomically setting 'cancelhandling' and the cancelation will be acted
     upon on next cancellation entrypoing in the target thread.

     It also requires to atomically check if cancellation is enabled and
     asynchronous, so both cancellation state and type are tracked on
     'cancelhandling'.  */

  int result = 0;
  int oldval = atomic_load_relaxed (&pd->cancelhandling);
  int newval;
  do
    {
    again:
      newval = oldval | CANCELING_BITMASK | CANCELED_BITMASK;
      if (oldval == newval)
	break;

      /* If the cancellation is handled asynchronously just send a
	 signal.  We avoid this if possible since it's more
	 expensive.  */
      if (cancel_enabled_and_canceled_and_async (newval))
	{
	  /* Mark the cancellation as "in progress".  */
	  int newval2 = oldval | CANCELING_BITMASK;
	  if (!atomic_compare_exchange_weak_acquire (&pd->cancelhandling,
						     &oldval, newval2))
	    goto again;

	  if (pd == THREAD_SELF)
	    /* This is not merely an optimization: An application may
	       call pthread_cancel (pthread_self ()) without calling
	       pthread_create, so the signal handler may not have been
	       set up for a self-cancel.  */
	    {
	      pd->result = PTHREAD_CANCELED;
	      if ((newval & CANCELTYPE_BITMASK) != 0)
		__do_cancel ();
	    }
	  else
	    /* The cancellation handler will take care of marking the
	       thread as canceled.  */
	    result = __pthread_kill_internal (th, SIGCANCEL);

	  break;
	}

	/* A single-threaded process should be able to kill itself, since
	   there is nothing in the POSIX specification that says that it
	   cannot.  So we set multiple_threads to true so that cancellation
	   points get executed.  */
	THREAD_SETMEM (THREAD_SELF, header.multiple_threads, 1);
#ifndef TLS_MULTIPLE_THREADS_IN_TCB
	__libc_single_threaded_internal = 0;
#endif
    }
  while (!atomic_compare_exchange_weak_acquire (&pd->cancelhandling, &oldval,
						newval));

  return result;
}
versioned_symbol (libc, __pthread_cancel, pthread_cancel, GLIBC_2_34);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_0, GLIBC_2_34)
compat_symbol (libpthread, __pthread_cancel, pthread_cancel, GLIBC_2_0);
#endif

/* Ensure that the unwinder is always linked in (the __pthread_unwind
   reference from __do_cancel is weak).  Use ___pthread_unwind_next
   (three underscores) to produce a strong reference to the same
   file.  */
PTHREAD_STATIC_FN_REQUIRE (___pthread_unwind_next)
