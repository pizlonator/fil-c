/* Implementation of the getrandom system call.
   Copyright (C) 2016-2026 Free Software Foundation, Inc.
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

#include <sys/random.h>
#include <errno.h>
#include <unistd.h>
#include <sysdep-cancel.h>
#include <pizlonated_syscalls.h>
#include <getrandom-internal.h>

/* Fil-C: the getrandom vDSO (and its per-thread state allocator) is not
   used because the vDSO code is not pizlonated; every getrandom request
   goes through the pizlonated syscall.  The vDSO support entry points
   below are kept (they are called directly by elf/libc_early_init.c,
   nptl/pthread_create.c, and the fork code), but they are no-ops.  */

/* Write up to LENGTH bytes of randomness starting at BUFFER.
   Return the number of bytes written, or -1 on error.  */
ssize_t
__getrandom (void *buffer, size_t length, unsigned int flags)
{
  return zsys_getrandom (buffer, length, flags);
}

/* Same as __getrandom, but not cancellable.  */
ssize_t
__getrandom_nocancel (void *buffer, size_t length, unsigned int flags)
{
  return zsys_getrandom (buffer, length, flags);
}

void
__getrandom_early_init (_Bool initial)
{
}

/* Re-add the state from CURP on the free list.  This function is
   called after fork returns in the child, so no locking is required.  */
void
__getrandom_reset_state (struct pthread *curp)
{
}

/* Called when a thread terminates, and adds its random buffer back into the
   allocator pool for use in a future thread.  This is called by
   pthread_create during thread termination, and after signal has been
   blocked. */
void
__getrandom_vdso_release (struct pthread *curp)
{
}

/* Reset the internal lock state in case another thread has locked while
   this thread calls fork.  The stale thread states will be handled by
   reclaim_stacks which calls __getrandom_reset_state on each thread.  */
void
__getrandom_fork_subprocess (void)
{
}

libc_hidden_def (__getrandom)
weak_alias (__getrandom, getrandom)
