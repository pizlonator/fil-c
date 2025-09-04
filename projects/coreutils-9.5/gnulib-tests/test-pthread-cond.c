/* Test of condition variables in multithreaded situations.
   Copyright (C) 2008-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#if USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS

/* Which tests to perform.
   Uncomment some of these, to verify that all tests crash if no locking
   is enabled.  */
#define DO_TEST_COND 1
#define DO_TEST_TIMEDCOND 1

/* Whether to help the scheduler through explicit sched_yield().
   Uncomment this to see if the operating system has a fair scheduler.  */
#define EXPLICIT_YIELD 1

/* Whether to print debugging messages.  */
#define ENABLE_DEBUGGING 0

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#if EXPLICIT_YIELD
# include <sched.h>
#endif

#if HAVE_DECL_ALARM
# include <signal.h>
# include <unistd.h>
#endif

#include "macros.h"

#if ENABLE_DEBUGGING
# define dbgprintf printf
#else
# define dbgprintf if (0) printf
#endif

#if EXPLICIT_YIELD
# define yield() sched_yield ()
#else
# define yield()
#endif


/*
 * Condition check
 */
static int cond_value = 0;
static pthread_cond_t condtest;
static pthread_mutex_t lockcond;

static void *
pthread_cond_wait_routine (void *arg)
{
  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  while (!cond_value)
    {
      ASSERT (pthread_cond_wait (&condtest, &lockcond) == 0);
    }
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  cond_value = 2;

  return NULL;
}

static void
test_pthread_cond_wait ()
{
  struct timespec remain;
  pthread_t thread;
  int ret;

  remain.tv_sec = 2;
  remain.tv_nsec = 0;

  cond_value = 0;

  ASSERT (pthread_create (&thread, NULL, pthread_cond_wait_routine, NULL) == 0);
  do
    {
      yield ();
      ret = nanosleep (&remain, &remain);
      ASSERT (ret >= -1);
    }
  while (ret == -1 && (remain.tv_sec != 0 || remain.tv_nsec != 0));

  /* signal condition */
  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  cond_value = 1;
  ASSERT (pthread_cond_signal (&condtest) == 0);
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  ASSERT (pthread_join (thread, NULL) == 0);

  if (cond_value != 2)
    abort ();
}


/*
 * Timed Condition check
 */
static int cond_timeout;

static void
get_ts (struct timespec *ts)
{
  struct timeval now;

  gettimeofday (&now, NULL);

  ts->tv_sec = now.tv_sec + 1;
  ts->tv_nsec = now.tv_usec * 1000;
}

static void *
pthread_cond_timedwait_routine (void *arg)
{
  int ret;
  struct timespec ts;

  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  while (!cond_value)
    {
      get_ts (&ts);
      ret = pthread_cond_timedwait (&condtest, &lockcond, &ts);
      if (ret == ETIMEDOUT)
        cond_timeout = 1;
    }
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  return NULL;
}

static void
test_pthread_cond_timedwait (void)
{
  struct timespec remain;
  pthread_t thread;
  int ret;

  remain.tv_sec = 2;
  remain.tv_nsec = 0;

  cond_value = cond_timeout = 0;

  ASSERT (pthread_create (&thread, NULL, pthread_cond_timedwait_routine, NULL)
          == 0);
  do
    {
      yield ();
      ret = nanosleep (&remain, &remain);
      ASSERT (ret >= -1);
    }
  while (ret == -1 && (remain.tv_sec != 0 || remain.tv_nsec != 0));

  /* signal condition */
  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  cond_value = 1;
  ASSERT (pthread_cond_signal (&condtest) == 0);
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  ASSERT (pthread_join (thread, NULL) == 0);

  if (!cond_timeout)
    abort ();
}

int
main ()
{
#if HAVE_DECL_ALARM
  /* Declare failure if test takes too long, by using default abort
     caused by SIGALRM.  */
  int alarm_value = 600;
  signal (SIGALRM, SIG_DFL);
  alarm (alarm_value);
#endif

  ASSERT (pthread_cond_init (&condtest, NULL) == 0);

  {
    pthread_mutexattr_t attr;

    ASSERT (pthread_mutexattr_init (&attr) == 0);
    ASSERT (pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_NORMAL) == 0);
    ASSERT (pthread_mutex_init (&lockcond, &attr) == 0);
    ASSERT (pthread_mutexattr_destroy (&attr) == 0);
  }

#if DO_TEST_COND
  printf ("Starting test_pthread_cond_wait ..."); fflush (stdout);
  test_pthread_cond_wait ();
  printf (" OK\n"); fflush (stdout);
#endif
#if DO_TEST_TIMEDCOND
  printf ("Starting test_pthread_cond_timedwait ..."); fflush (stdout);
  test_pthread_cond_timedwait ();
  printf (" OK\n"); fflush (stdout);
#endif

  return 0;
}

#else

/* No multithreading available.  */

#include <stdio.h>

int
main ()
{
  fputs ("Skipping test: multithreading not enabled\n", stderr);
  return 77;
}

#endif
