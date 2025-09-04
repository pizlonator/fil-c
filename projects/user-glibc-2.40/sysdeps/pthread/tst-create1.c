/* Verify that pthread_create does not deadlock when ctors take locks.
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

#include <stdio.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>

/*
Check if ctor and pthread_create deadlocks in

thread 1: dlopen -> ctor -> lock(user_lock)
thread 2: lock(user_lock) -> pthread_create

or in

thread 1: dlclose -> dtor -> lock(user_lock)
thread 2: lock(user_lock) -> pthread_create
*/

static pthread_barrier_t bar_ctor;
static pthread_barrier_t bar_ctor_finish;
static pthread_barrier_t bar_dtor;
static pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;

void
ctor (void)
{
  xpthread_barrier_wait (&bar_ctor);
  dprintf (1, "thread 1: in ctor: started.\n");
  xpthread_mutex_lock (&user_lock);
  dprintf (1, "thread 1: in ctor: locked user_lock.\n");
  xpthread_mutex_unlock (&user_lock);
  dprintf (1, "thread 1: in ctor: unlocked user_lock.\n");
  dprintf (1, "thread 1: in ctor: done.\n");
  xpthread_barrier_wait (&bar_ctor_finish);
}

void
dtor (void)
{
  xpthread_barrier_wait (&bar_dtor);
  dprintf (1, "thread 1: in dtor: started.\n");
  xpthread_mutex_lock (&user_lock);
  dprintf (1, "thread 1: in dtor: locked user_lock.\n");
  xpthread_mutex_unlock (&user_lock);
  dprintf (1, "thread 1: in dtor: unlocked user_lock.\n");
  dprintf (1, "thread 1: in dtor: done.\n");
}

static void *
thread3 (void *a)
{
  dprintf (1, "thread 3: started.\n");
  dprintf (1, "thread 3: done.\n");
  return 0;
}

static void *
thread2 (void *a)
{
  pthread_t t3;
  dprintf (1, "thread 2: started.\n");

  xpthread_mutex_lock (&user_lock);
  dprintf (1, "thread 2: locked user_lock.\n");
  xpthread_barrier_wait (&bar_ctor);
  t3 = xpthread_create (0, thread3, 0);
  xpthread_mutex_unlock (&user_lock);
  dprintf (1, "thread 2: unlocked user_lock.\n");
  xpthread_join (t3);
  xpthread_barrier_wait (&bar_ctor_finish);

  xpthread_mutex_lock (&user_lock);
  dprintf (1, "thread 2: locked user_lock.\n");
  xpthread_barrier_wait (&bar_dtor);
  t3 = xpthread_create (0, thread3, 0);
  xpthread_mutex_unlock (&user_lock);
  dprintf (1, "thread 2: unlocked user_lock.\n");
  xpthread_join (t3);

  dprintf (1, "thread 2: done.\n");
  return 0;
}

static void
thread1 (void)
{
  dprintf (1, "thread 1: started.\n");
  xpthread_barrier_init (&bar_ctor, NULL, 2);
  xpthread_barrier_init (&bar_ctor_finish, NULL, 2);
  xpthread_barrier_init (&bar_dtor, NULL, 2);
  pthread_t t2 = xpthread_create (0, thread2, 0);
  void *p = xdlopen ("tst-create1mod.so", RTLD_NOW | RTLD_GLOBAL);
  dprintf (1, "thread 1: dlopen done.\n");
  xdlclose (p);
  dprintf (1, "thread 1: dlclose done.\n");
  xpthread_join (t2);
  dprintf (1, "thread 1: done.\n");
}

static int
do_test (void)
{
  thread1 ();
  return 0;
}

#include <support/test-driver.c>
