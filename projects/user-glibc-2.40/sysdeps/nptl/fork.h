/* System specific fork hooks.  Linux version.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef _FORK_H
#define _FORK_H

#include <assert.h>
#include <kernel-posix-timers.h>
#include <ldsodefs.h>
#include <list.h>
#include <mqueue.h>
#include <pthreadP.h>
#include <sysdep.h>

static inline void
fork_system_setup (void)
{
  /* See __pthread_once.  */
  __fork_generation += __PTHREAD_ONCE_FORK_GEN_INCR;
}

static void
fork_system_setup_after_fork (void)
{
  /* There is one thread running.  */
  __nptl_nthreads = 1;

  /* Initialize thread library locks.  */
  GL (dl_stack_cache_lock) = LLL_LOCK_INITIALIZER;
  __default_pthread_attr_lock = LLL_LOCK_INITIALIZER;

  call_function_static_weak (__mq_notify_fork_subprocess);
  call_function_static_weak (__timer_fork_subprocess);
}

/* In case of a fork() call the memory allocation in the child will be
   the same but only one thread is running.  All stacks except that of
   the one running thread are not used anymore.  We have to recycle
   them.  */
static void
reclaim_stacks (void)
{
}


#endif
