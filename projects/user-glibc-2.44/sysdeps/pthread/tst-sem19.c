/* Test sem_trywait with threads.
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

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <support/check.h>
#include <support/xthread.h>

/* The test uses two threads, the main thread and a newly created
   thread to test the operation of sem_trywait in a threaded scenario.
   The intent is to test sem_trywait when it would return EAGAIN, and
   then again after the critical section in the new thread has posted
   to the semaphore and the main thread succeeds in calling
   sem_trywait.  It is possible this test fails with a timeout if the
   second thread takes longer than the test timeout to acquire the
   lock, and post.  */

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem;

static void *
tf (void *arg)
{
  xpthread_mutex_lock (&lock);
  sem_post (&sem);
  xpthread_mutex_unlock (&lock);
  return NULL;
}

static int
do_test (void)
{
  int ret;

  ret = sem_init (&sem, 0, 0);
  TEST_VERIFY_EXIT (ret == 0);
  xpthread_mutex_lock (&lock);
  pthread_t th = xpthread_create (NULL, tf, NULL);
  errno = 0;
  /* The other thread is waiting on the lock before it calls sem_post,
     so sem_trywait should fail.  */
  ret = sem_trywait (&sem);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EAGAIN);
  xpthread_mutex_unlock (&lock);
  /* The other thread now takes the lock, calls sem_post and releases
     the lock.  */
  for (;;)
    {
      errno = 0;
      ret = sem_trywait (&sem);
      if (ret == 0)
	break;
      TEST_COMPARE (errno, EAGAIN);
    }
  xpthread_join (th);

  return 0;
}

#include <support/test-driver.c>
