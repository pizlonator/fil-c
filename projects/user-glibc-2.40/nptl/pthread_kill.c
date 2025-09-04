/* Send a signal to a specific pthread.  Stub version.
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

#include <libc-lock.h>
#include <unistd.h>
#include <pthreadP.h>
#include <shlib-compat.h>

/* Sends SIGNO to THREADID.  If the thread is about to exit or has
   already exited on the kernel side, return NO_TID.  Otherwise return
   0 or an error code. */
static int
__pthread_kill_implementation (pthread_t threadid, int signo, int no_tid)
{
  struct pthread *pd = (struct pthread *) threadid;
  if (pd == THREAD_SELF)
    {
      /* Use the actual TID from the kernel, so that it refers to the
         current thread even if called after vfork.  There is no
         signal blocking in this case, so that the signal is delivered
         immediately, before __pthread_kill_internal returns: a signal
         sent to the thread itself needs to be delivered
         synchronously.  (It is unclear if Linux guarantees the
         delivery of all pending signals after unblocking in the code
         below.  POSIX only guarantees delivery of a single signal,
         which may not be the right one.)  */
      pid_t tid = INTERNAL_SYSCALL_CALL (gettid);
      int ret = INTERNAL_SYSCALL_CALL (tgkill, __getpid (), tid, signo);
      return INTERNAL_SYSCALL_ERROR_P (ret) ? INTERNAL_SYSCALL_ERRNO (ret) : 0;
    }

  /* Block all signals, as required by pd->exit_lock.  */
  internal_sigset_t old_mask;
  internal_signal_block_all (&old_mask);
  __libc_lock_lock (pd->exit_lock);

  int ret;
  if (pd->exiting)
    /* The thread is about to exit (or has exited).  Sending the
       signal is either not observable (the target thread has already
       blocked signals at this point), or it will fail, or it might be
       delivered to a new, unrelated thread that has reused the TID.
       So do not actually send the signal.  */
    ret = no_tid;
  else
    {
      ret = INTERNAL_SYSCALL_CALL (tgkill, __getpid (), pd->tid, signo);
      ret = INTERNAL_SYSCALL_ERROR_P (ret) ? INTERNAL_SYSCALL_ERRNO (ret) : 0;
    }

  __libc_lock_unlock (pd->exit_lock);
  internal_signal_restore_set (&old_mask);

  return ret;
}

int
__pthread_kill_internal (pthread_t threadid, int signo)
{
  /* Do not report an error in the no-tid case because the threadid
     argument is still valid (the thread ID lifetime has not ended),
     and ESRCH (for example) would be misleading.  */
  return __pthread_kill_implementation (threadid, signo, 0);
}

int
__pthread_kill (pthread_t threadid, int signo)
{
  /* Disallow sending the signal we use for cancellation, timers,
     for the setxid implementation.  */
  if (is_internal_signal (signo))
    return EINVAL;

  return __pthread_kill_internal (threadid, signo);
}

/* Some architectures (for instance arm) might pull raise through libgcc, so
   avoid the symbol version if it ends up being used on ld.so.  */
#if !IS_IN(rtld)
libc_hidden_def (__pthread_kill)
versioned_symbol (libc, __pthread_kill, pthread_kill, GLIBC_2_34);

# if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_0, GLIBC_2_34)
/* Variant which returns ESRCH in the no-TID case, for backwards
   compatibility.  */
int
attribute_compat_text_section
__pthread_kill_esrch (pthread_t threadid, int signo)
{
  if (is_internal_signal (signo))
    return EINVAL;

  return __pthread_kill_implementation (threadid, signo, ESRCH);
}
compat_symbol (libc, __pthread_kill_esrch, pthread_kill, GLIBC_2_0);
# endif
#endif
