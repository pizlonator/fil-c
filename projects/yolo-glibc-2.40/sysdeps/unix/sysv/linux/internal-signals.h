/* Special use of signals internally.  Linux version.
   Copyright (C) 2014-2024 Free Software Foundation, Inc.
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

#ifndef __INTERNAL_SIGNALS_H
# define __INTERNAL_SIGNALS_H

#include <internal-sigset.h>
#include <limits.h>
#include <signal.h>
#include <sigsetops.h>
#include <stdbool.h>
#include <stddef.h>
#include <sysdep.h>

/* The signal used for asynchronous cancelation.  */
#define SIGCANCEL       __SIGRTMIN


/* Signal needed for the kernel-supported POSIX timer implementation.
   We can reuse the cancellation signal since we can distinguish
   cancellation from timer expirations.  */
#define SIGTIMER        SIGCANCEL


/* Signal used to implement the setuid et.al. functions.  */
#define SIGSETXID       (__SIGRTMIN + 1)


/* How many signal numbers need to be reserved for libpthread's private uses
   (SIGCANCEL and SIGSETXID).  */
#define RESERVED_SIGRT  2


/* Return is sig is used internally.  */
static inline bool
is_internal_signal (int sig)
{
  return (sig == SIGCANCEL) || (sig == SIGSETXID);
}

/* Remove internal glibc signal from the mask.  */
static inline void
clear_internal_signals (sigset_t *set)
{
  __sigdelset (set, SIGCANCEL);
  __sigdelset (set, SIGSETXID);
}

static const internal_sigset_t sigall_set = {
   .__val = {[0 ...  __NSIG_WORDS-1 ] =  -1 }
};

/* Obtain and change blocked signals, including internal glibc ones.  */
static inline int
internal_sigprocmask (int how, const internal_sigset_t *set,
		      internal_sigset_t *oldset)
{
  return INTERNAL_SYSCALL_CALL (rt_sigprocmask, how, set, oldset,
				__NSIG_BYTES);
}

/* Block all signals, including internal glibc ones.  */
static inline void
internal_signal_block_all (internal_sigset_t *oset)
{
  INTERNAL_SYSCALL_CALL (rt_sigprocmask, SIG_BLOCK, &sigall_set, oset,
			 __NSIG_BYTES);
}

/* Restore current process signal mask.  */
static inline void
internal_signal_restore_set (const internal_sigset_t *set)
{
  INTERNAL_SYSCALL_CALL (rt_sigprocmask, SIG_SETMASK, set, NULL,
			 __NSIG_BYTES);
}


/* It is used on timer_create code directly on sigwaitinfo call, so it can not
   use the internal_sigset_t definitions.  */
static const sigset_t sigtimer_set = {
  .__val = { [0]                      = __sigmask (SIGTIMER),
             [1 ... _SIGSET_NWORDS-1] = 0
  }
};

/* Unblock only SIGTIMER.  */
static inline void
signal_unblock_sigtimer (void)
{
  INTERNAL_SYSCALL_CALL (rt_sigprocmask, SIG_UNBLOCK, &sigtimer_set, NULL,
			 __NSIG_BYTES);
}

#endif
