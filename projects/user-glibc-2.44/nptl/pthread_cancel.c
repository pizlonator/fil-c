/* Copyright (C) 2002-2026 Free Software Foundation, Inc.
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
#include <cancellation-pc-check.h>
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

  /* Check if asynchronous cancellation mode is set and cancellation is not
     already in progress, or if interrupted instruction pointer falls within
     the cancellable syscall bridge.
     For interruptable syscalls with external side-effects (i.e. partial
     reads), the kernel will set the IP to after __syscall_cancel_arch_end,
     thus disabling the cancellation and allowing the process to handle such
     conditions.  */
  struct pthread *self = THREAD_SELF;
  int oldval = atomic_load_relaxed (&self->cancelhandling);
  if (cancel_enabled_and_canceled_and_async (oldval)
      || cancellation_pc_check (ctx))
    __syscall_do_cancel ();
}

int
__pthread_cancel (pthread_t th)
{
  volatile struct pthread *pd = (volatile struct pthread *) th;

  unsigned int state = atomic_load_relaxed (&pd->joinstate);
  if (state == THREAD_STATE_EXITED)
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
      __libc_fatal (UNWIND_SONAME
		    " must be installed for pthread_cancel to work\n");
  }
#endif

  /* Some syscalls are never restarted after being interrupted by a signal
     handler, regardless of the use of SA_RESTART (they always fail with
     EINTR).  So pthread_cancel cannot send SIGCANCEL unless the cancellation
     is enabled.
     In this case the target thread is set as 'cancelled' (CANCELED_BITMASK)
     by atomically setting 'cancelhandling' and the cancellation will be acted
     upon on next cancellation entrypoint in the target thread.

     It also requires to atomically check if cancellation is enabled, so the
     state are also tracked on 'cancelhandling'.  */

  int result = 0;
  int oldval = atomic_load_relaxed (&pd->cancelhandling);
  int newval;
  do
    {
    again:
      newval = oldval | CANCELED_BITMASK;
      if (oldval == newval)
	break;

      /* Only send the SIGCANCEL signal if cancellation is enabled, since some
	 syscalls are never restarted even with SA_RESTART.  The signal
	 will act iff async cancellation is enabled.  */
      if (cancel_enabled (newval))
	{
	  if (!atomic_compare_exchange_weak_acquire (&pd->cancelhandling,
						     &oldval, newval))
	    goto again;

	  if (pd == THREAD_SELF)
	    /* This is not merely an optimization: An application may
	       call pthread_cancel (pthread_self ()) without calling
	       pthread_create, so the signal handler may not have been
	       set up for a self-cancel.  */
	    {
	      if (cancel_async_enabled (newval))
		__do_cancel (PTHREAD_CANCELED);
	    }
	  else
	    /* The cancellation handler will take care of marking the
	       thread as canceled.  */
	    result = __pthread_kill_internal (th, SIGCANCEL);

	  break;
	}
    }
  while (!atomic_compare_exchange_weak_acquire (&pd->cancelhandling, &oldval,
						newval));

  /* A single-threaded process should be able to kill itself, since there is
     nothing in the POSIX specification that says that it cannot.  So we set
     multiple_threads to true so that cancellation points get executed.  */
  THREAD_SETMEM (THREAD_SELF, header.multiple_threads, 1);
#ifndef TLS_MULTIPLE_THREADS_IN_TCB
  __libc_single_threaded_internal = 0;
#endif

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
