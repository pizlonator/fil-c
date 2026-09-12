/* Set a thread's signal state.  Hurd on Mach version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include <hurd/signal.h>
#include <hurd/msg.h>

#include <pt-internal.h>

error_t
__pthread_sigstate (struct __pthread *thread, int how,
		    const sigset_t *set, sigset_t *oset, int clear_pending)
{
  return __sigthreadmask (_hurd_thread_sigstate (thread->kernel_thread),
			  how, set, oset, clear_pending);
}
libc_hidden_def (__pthread_sigstate)
