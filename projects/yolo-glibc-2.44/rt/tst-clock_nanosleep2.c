/* Test for clock_nanosleep parameter checks and sleep duration.
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

/* This test has two primary goals - first, to validate that invalid
   inputs to clock_nanosleep are caught, and second, to validate that
   clock_nanosleep sleeps for at least the amount of time requested.
   It is assumed that the system may sleep for an arbitrary additional
   amount of time beyond the requested time.  */

#include <unistd.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <support/xunistd.h>
#include <support/check.h>
#include <support/xthread.h>
#include <support/timespec.h>

/* This is 1 ms per test, we have 10 tests, so this file runs in on
   the order of 0.01 seconds.  */
#define TEST_NSEC 1000000

/* Nanoseconds per second.  */
#define NSECMAX 1000000000L

static pthread_barrier_t barrier;

/* This function is intended to rack up both user and system time.  */
static void *
chew_cpu (void *arg)
{
  pthread_barrier_wait (&barrier);

  while (1)
    {
      static volatile char buf[4096];
      for (int i = 0; i < 100; ++i)
	for (size_t j = 0; j < sizeof buf; ++j)
	  buf[j] = 0xaa;
      int nullfd = xopen ("/dev/null", O_WRONLY, 0);
      for (int i = 0; i < 100; ++i)
	for (size_t j = 0; j < sizeof buf; ++j)
	  buf[j] = 0xbb;
      xwrite (nullfd, (char *) buf, sizeof buf);
      close (nullfd);
    }

  return NULL;
}

static void
ptime_1 (const char *n, struct timespec t)
{
  /* This is only for debugging failed test cases.  */
  printf ("%12s: %lld.%09lld\n", n, (long long int) t.tv_sec,
	  (long long int) t.tv_nsec);
}
#define ptime(t) ptime_1 (#t, t)

static void
test_interval_1 (const char *n_clock, clockid_t t_clock)
{
  struct timespec me_before, me_after, quantum, me_sleep, me_slept;
  long long int slept, min_slept;

  /* Arbitrary to ensure our time period is sufficiently bigger than
     the time step.  */
  TEST_VERIFY (clock_getres (t_clock, &quantum) == 0);
  printf("Clock quantum: %lld ns, test time: %lld ns\n",
	 (long long int) quantum.tv_nsec, (long long int) TEST_NSEC);
  TEST_VERIFY (quantum.tv_nsec <= TEST_NSEC / 10);

  min_slept = TEST_NSEC;

  me_sleep = make_timespec (0,  min_slept);

  printf ("test clock %s for %lld.%09lld sec relative\n",
	  n_clock, (long long int) me_sleep.tv_sec,
	  (long long int) me_sleep.tv_nsec);

  TEST_COMPARE (clock_gettime (t_clock, &me_before), 0);
  TEST_COMPARE (clock_nanosleep (t_clock, 0, &me_sleep, NULL), 0);
  TEST_COMPARE (clock_gettime (t_clock, &me_after), 0);

  me_slept = timespec_sub (me_after, me_before);
  slept = support_timespec_ns (me_slept);

  ptime (me_before);
  ptime (me_after);
  ptime (me_sleep);
  ptime (me_slept);
  printf ("test slept %lld nsec >= asked for %lld ?\n", slept, min_slept);

  /* This is the important part - verify that the time slept is at
     least as much as the time requested.  */
  TEST_VERIFY (slept >= min_slept);
}

static void
test_abs_1 (const char *n_clock, clockid_t t_clock)
{
  struct timespec me_before, me_after, quantum, me_sleep;

  /* Arbitrary to ensure our time period is sufficiently bigger than
     the time step.  */
  TEST_VERIFY (clock_getres (t_clock, &quantum) == 0);
  printf("Clock quantum: %lld ns, test time: %lld ns\n",
	 (long long int) quantum.tv_nsec, (long long int) TEST_NSEC);
  TEST_VERIFY (quantum.tv_nsec <= TEST_NSEC / 10);

  me_sleep = make_timespec (0,  TEST_NSEC);

  printf ("test clock %s for %lld.%09lld sec absolute\n",
	  n_clock, (long long int) me_sleep.tv_sec,
	  (long long int) me_sleep.tv_nsec);

  TEST_COMPARE (clock_gettime (t_clock, &me_before), 0);
  me_sleep = timespec_add (me_sleep, me_before);
  TEST_COMPARE (clock_nanosleep (t_clock, TIMER_ABSTIME, &me_sleep, NULL), 0);
  TEST_COMPARE (clock_gettime (t_clock, &me_after), 0);

  ptime (me_before);
  ptime (me_sleep);
  ptime (me_after);

  printf("test slept until %lld.%09lld after requested %lld.%09lld ?\n",
	 (long long int) me_after.tv_sec, (long long int) me_after.tv_nsec,
	 (long long int) me_sleep.tv_sec, (long long int) me_sleep.tv_nsec);

  /* This is the important part - verify that the time slept is at
     least as much as the time requested.  */
  TEST_TIMESPEC_EQUAL_OR_AFTER (me_after, me_sleep);
}

static void
test_invalids_1 (const char *the_clock_name, int the_clock,
		 const char *flags_name, int flags)
{
  struct timespec me_before;

  /* Note: do not use make_timespec() in case that function tries to
     normalize the fields.  */

  printf ("%s: %s: test tv 0, 0\n", the_clock_name, flags_name);
  me_before.tv_sec = 0;
  me_before.tv_nsec = 0;
  TEST_COMPARE (clock_nanosleep (the_clock, 0, &me_before, NULL), 0);

  printf ("%s: %s: test tv -1, 0\n", the_clock_name, flags_name);
  me_before.tv_sec = -1;
  me_before.tv_nsec = 0;
  TEST_COMPARE (clock_nanosleep (the_clock, 0, &me_before, NULL), EINVAL);

  printf ("%s: %s: test tv 0, -1\n", the_clock_name, flags_name);
  me_before.tv_sec = 0;
  me_before.tv_nsec = -1;
  TEST_COMPARE (clock_nanosleep (the_clock, 0, &me_before, NULL), EINVAL);

  printf ("%s: %s: test tv -1, -1\n", the_clock_name, flags_name);
  me_before.tv_sec = -1;
  me_before.tv_nsec = -1;
  TEST_COMPARE (clock_nanosleep (the_clock, 0, &me_before, NULL), EINVAL);

  printf ("%s: %s: test tv 0, MAX\n", the_clock_name, flags_name);
  me_before.tv_sec = 0;
  me_before.tv_nsec = NSECMAX;
  TEST_COMPARE (clock_nanosleep (the_clock, 0, &me_before, NULL), EINVAL);
}

static int
do_test (void)
{
  pthread_t th;

  pthread_barrier_init (&barrier, NULL, 2);

  /* Test for proper error detection.  */

#define test_invalids(c, f) test_invalids_1 (#c, c, #f, f)
  test_invalids (CLOCK_REALTIME, 0);
#ifdef CLOCK_TAI
  test_invalids (CLOCK_TAI, 0);
#endif
  test_invalids (CLOCK_MONOTONIC, 0);
#ifdef CLOCK_BOOTTIME
  test_invalids (CLOCK_BOOTTIME, 0);
#endif
  test_invalids (CLOCK_PROCESS_CPUTIME_ID, 0);
  test_invalids (CLOCK_REALTIME, TIMER_ABSTIME);
#ifdef CLOCK_TAI
  test_invalids (CLOCK_TAI, TIMER_ABSTIME);
#endif
  test_invalids (CLOCK_MONOTONIC, TIMER_ABSTIME);
#ifdef CLOCK_BOOTTIME
  test_invalids (CLOCK_BOOTTIME, TIMER_ABSTIME);
#endif
  test_invalids (CLOCK_PROCESS_CPUTIME_ID, TIMER_ABSTIME);

  /* Test for various clocks "working".  */

#define test_interval(c) test_interval_1 (#c, c)
  test_interval (CLOCK_REALTIME);
#ifdef CLOCK_TAI
  test_interval (CLOCK_TAI);
#endif
  test_interval (CLOCK_MONOTONIC);
#ifdef CLOCK_BOOTTIME
  test_interval (CLOCK_BOOTTIME);
#endif

  th = xpthread_create (NULL, chew_cpu, NULL);
  xpthread_barrier_wait (&barrier);
  test_interval (CLOCK_PROCESS_CPUTIME_ID);
  xpthread_cancel (th);

#define test_abs(c) test_abs_1 (#c, c)
  test_abs (CLOCK_REALTIME);
#ifdef CLOCK_TAI
  test_abs (CLOCK_TAI);
#endif
  test_abs (CLOCK_MONOTONIC);
#ifdef CLOCK_BOOTTIME
  test_abs (CLOCK_BOOTTIME);
#endif

  th = xpthread_create (NULL, chew_cpu, NULL);
  xpthread_barrier_wait (&barrier);
  test_abs (CLOCK_PROCESS_CPUTIME_ID);
  xpthread_cancel (th);

  return 0;
}

#include <support/test-driver.c>
