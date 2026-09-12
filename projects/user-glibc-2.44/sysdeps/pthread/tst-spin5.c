/* Threaded test the pthread_spin_trylock function.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
#include <errno.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <support/xthread.h>

pthread_spinlock_t lock;

void *
thread (void *arg)
{
  int ret;
  int thr_id = *(int *) arg;

  ret = pthread_spin_trylock (&lock);
  if (thr_id == 1)
    /* thread with already acquired lock.  */
    {
      if (ret != EBUSY)
        {
          FAIL_EXIT1 ("pthread_spin_trylock should fail with EBUSY");
        }
    }
  else if (thr_id == 2)
    /* thread with released spin lock.  */
    {
      if (ret != 0)
        {
          FAIL_EXIT1 ("pthread_spin_trylock should be able to acquire lock");
        }
    }
  return NULL;
}

static int
do_test (void)
{
  pthread_t thr1, thr2;
  int ret;
  int thr1_id = 1, thr2_id = 2;

  pthread_spin_init (&lock, PTHREAD_PROCESS_PRIVATE);
  /* lock spin in main thread.  */
  ret = pthread_spin_trylock (&lock);
  if (ret != 0)
    {
      FAIL_EXIT1 ("Main thread should be able to acquire spin lock");
    }

  /* create first thread to try locking already acquired spin lock.  */
  thr1 = xpthread_create (NULL, thread, &thr1_id);
  xpthread_join (thr1);

  /* release spin lock and create thread to acquire released spin lock.  */
  pthread_spin_unlock (&lock);
  thr2 = xpthread_create (NULL, thread, &thr2_id);
  xpthread_join (thr2);

  pthread_spin_destroy (&lock);
  return 0;
}

#include <support/test-driver.c>
