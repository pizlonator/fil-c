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
#include <stdfil.h>

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
  zerror ("abort(3) called.");

  /* If even this fails try to use the provided instruction to crash
     or otherwise make sure we never return.  */
  while (1)
    /* Try for ever and ever.  */
    ABORT_INSTRUCTION;
}
libc_hidden_def (abort)
