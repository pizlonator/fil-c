/* Test pthread_timedjoin_np and pthread_clockjoin_np with an invalid timeout.
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
#include <stdio.h>
#include <time.h>
#include <support/check.h>
#include <support/xthread.h>
#include <support/xtime.h>


#define CLOCK_USE_TIMEDJOIN (-1)

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *
tf (void *arg)
{
  xpthread_mutex_lock (&lock);
  xpthread_mutex_unlock (&lock);
  return (void *) 42l;
}

static int
do_test_clock (clockid_t clockid)
{
  const clockid_t clockid_for_get =
    (clockid == CLOCK_USE_TIMEDJOIN) ? CLOCK_REALTIME : clockid;

  xpthread_mutex_lock (&lock);
  pthread_t th = xpthread_create (NULL, tf, NULL);

  void *status;
  int ret;
  struct timespec timeout = xclock_now (clockid_for_get);
  timeout.tv_sec += 2;
  timeout.tv_nsec = -1;
  if (clockid == CLOCK_USE_TIMEDJOIN)
    ret = pthread_timedjoin_np (th, &status, &timeout);
  else
    ret = pthread_clockjoin_np (th, &status, clockid, &timeout);
  TEST_COMPARE (ret, EINVAL);
  timeout.tv_nsec = 1000000000;
  if (clockid == CLOCK_USE_TIMEDJOIN)
    ret = pthread_timedjoin_np (th, &status, &timeout);
  else
    ret = pthread_clockjoin_np (th, &status, clockid, &timeout);
  TEST_COMPARE (ret, EINVAL);
  xpthread_mutex_unlock (&lock);
  timeout.tv_nsec = 0;
  ret = pthread_join (th, &status);
  TEST_COMPARE (ret, 0);
  if (status != (void *) 42l)
    FAIL_EXIT1 ("return value %p, expected %p\n", status, (void *) 42l);

  return 0;
}

static int
do_test (void)
{
  puts ("testing pthread_timedjoin_np");
  do_test_clock (CLOCK_USE_TIMEDJOIN);
  puts ("testing CLOCK_REALTIME");
  do_test_clock (CLOCK_REALTIME);
  puts ("testing CLOCK_MONOTONIC");
  do_test_clock (CLOCK_MONOTONIC);
  return 0;
}

#include <support/test-driver.c>
