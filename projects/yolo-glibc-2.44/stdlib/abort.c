/* Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

#include <signal.h>
#include <internal-signals.h>
#include <libc-lock.h>
#include <pthreadP.h>
#include <string.h>
#include <unistd.h>

/* Try to get a machine dependent instruction which will make the
   program crash.  This is used in case everything else fails.  */
#include <abort-instr.h>
#ifndef ABORT_INSTRUCTION
/* No such instruction is available.  */
# define ABORT_INSTRUCTION
#endif

/* Exported variable to locate abort message in core files etc.  */
struct abort_msg_s *__abort_msg;
libc_hidden_def (__abort_msg)

/* The lock is used to prevent multiple thread to change the SIGABRT
   to SIG_IGN while abort tries to change to SIG_DFL, and to avoid
   a new process to see a wrong disposition if there is a SIGABRT
   handler installed.  */
__libc_rwlock_define_initialized (static, lock);

void
__abort_fork_reset_child (void)
{
  /* Reinitialize lock without calling pthread_rwlock_init, to
     avoid a valgrind DRD false positive.  */
  __libc_rwlock_define_initialized (, reset_lock);
  memcpy (&lock, &reset_lock, sizeof (lock));
}

void
__abort_lock_rdlock (internal_sigset_t *set)
{
  internal_signal_block_all (set);
  __libc_rwlock_rdlock (lock);
}

void
__abort_lock_wrlock (internal_sigset_t *set)
{
  internal_signal_block_all (set);
  __libc_rwlock_wrlock (lock);
}

void
__abort_lock_unlock (const internal_sigset_t *set)
{
  __libc_rwlock_unlock (lock);
  internal_signal_restore_set (set);
}

/* Cause an abnormal program termination with core-dump.  */
_Noreturn void
abort (void)
{
  raise (SIGABRT);

  /* There is a SIGABRT handle installed and it returned, or SIGABRT was
     blocked or ignored.  In this case use a AS-safe lock to prevent sigaction
     to change the signal disposition again, set the handle to default
     disposition, and re-raise the signal.  Even if POSIX state this step is
     optional, this a QoI by forcing the process termination through the
     signal handler.  */
  __abort_lock_wrlock (NULL);

  struct sigaction act = {.sa_handler = SIG_DFL, .sa_flags = 0 };
  __sigfillset (&act.sa_mask);
  __libc_sigaction (SIGABRT, &act, NULL);
  __pthread_raise_internal (SIGABRT);
  internal_signal_unblock_signal (SIGABRT);

  /* This code should be unreachable, try the arch-specific code and the
     syscall fallback.  */
  ABORT_INSTRUCTION;

  _exit (127);
}
libc_hidden_def (abort)
