/* Test clock_gettime function.
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

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <support/check.h>
#include <support/test-driver.h>
#include <support/xsignal.h>

/* Compare two struct timespec values, returning a value -1, 0 or 1.  */

int
compare_timespec (const struct timespec *tv1, const struct timespec *tv2)
{
  if (tv1->tv_sec < tv2->tv_sec)
    return -1;
  if (tv1->tv_sec > tv2->tv_sec)
    return 1;
  if (tv1->tv_nsec < tv2->tv_nsec)
    return -1;
  if (tv1->tv_nsec > tv2->tv_nsec)
    return 1;
  return 0;
}

struct test_clockid
{
  clockid_t clockid;
  const char *name;
  bool is_cputime;
  bool fail_ok;
};

#define CLOCK(clockid) { clockid, # clockid, false, false }
#define CLOCK_CPU(clockid) { clockid, # clockid, true, false }
#define CLOCK_FAIL_OK(clockid) { clockid, # clockid, false, true }

static const struct test_clockid clocks[] =
  {
    CLOCK (CLOCK_REALTIME),
#ifdef CLOCK_MONOTONIC
    CLOCK (CLOCK_MONOTONIC),
#endif
#ifdef CLOCK_PROCESS_CPUTIME_ID
    CLOCK_CPU (CLOCK_PROCESS_CPUTIME_ID),
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
    CLOCK_CPU (CLOCK_THREAD_CPUTIME_ID),
#endif
#ifdef CLOCK_MONOTONIC_RAW
    CLOCK (CLOCK_MONOTONIC_RAW),
#endif
#ifdef CLOCK_REALTIME_COARSE
    CLOCK (CLOCK_REALTIME_COARSE),
#endif
#ifdef CLOCK_MONOTONIC_COARSE
    CLOCK (CLOCK_MONOTONIC_COARSE),
#endif
#ifdef CLOCK_BOOTTIME
    CLOCK (CLOCK_BOOTTIME),
#endif
#ifdef CLOCK_REALTIME_ALARM
    CLOCK_FAIL_OK (CLOCK_REALTIME_ALARM),
#endif
#ifdef CLOCK_BOOTTIME_ALARM
    CLOCK_FAIL_OK (CLOCK_BOOTTIME_ALARM),
#endif
#ifdef CLOCK_TAI
    CLOCK (CLOCK_TAI),
#endif
  };


volatile int sigalrm_received;

void
handle_sigalrm (int sig)
{
  sigalrm_received = 1;
}

int
do_test (void)
{
  /* Verify that the calls to clock_gettime succeed, that the time does
     not decrease, and that time returns a truncated (not rounded)
     version of the time.  */
  for (size_t i = 0; i < sizeof clocks / sizeof clocks[0]; i++)
    {
      printf ("testing %s\n", clocks[i].name);
      struct timespec ts1, ts2, ts3;
      int ret;
      time_t t1;
      t1 = time (NULL);
      TEST_VERIFY_EXIT (t1 != (time_t) -1);
      ret = clock_gettime (clocks[i].clockid, &ts1);
      if (clocks[i].fail_ok && ret == -1)
	{
	  printf ("failed (OK for this clock): %m\n");
	  continue;
	}
      TEST_VERIFY_EXIT (ret == 0);
      if (clocks[i].clockid == CLOCK_REALTIME)
	TEST_VERIFY (t1 <= ts1.tv_sec);
      TEST_VERIFY (ts1.tv_nsec >= 0);
      TEST_VERIFY (ts1.tv_nsec < 1000000000);
      ret = clock_gettime (clocks[i].clockid, &ts2);
      TEST_VERIFY_EXIT (ret == 0);
      TEST_VERIFY (compare_timespec (&ts1, &ts2) <= 0);
      TEST_VERIFY (ts2.tv_nsec >= 0);
      TEST_VERIFY (ts2.tv_nsec < 1000000000);
      /* Also verify that after sleeping, the time returned has
	 increased.  Repeat several times to verify that each time,
	 the time from the time function is truncated not rounded.
	 For CPU time clocks, the time spent spinning on the CPU, and
	 so whether we end in the later half of a second, is not
	 predictable; thus, only test once for those clocks.  */
      const struct timespec duration = { .tv_nsec = 100000000 };
      for (int j = 0; j < 5; j++)
	{
	  if (clocks[i].is_cputime)
	    {
	      timer_t timer;
	      ret = timer_create (CLOCK_PROCESS_CPUTIME_ID, NULL, &timer);
	      TEST_VERIFY_EXIT (ret == 0);
	      sigalrm_received = 0;
	      xsignal (SIGALRM, handle_sigalrm);
	      struct itimerspec t =
		{ .it_value =
		  {
		    .tv_sec = 0,
		    .tv_nsec = 200000000
		  }
		};
	      ret = timer_settime (timer, 0, &t, NULL);
	      TEST_VERIFY_EXIT (ret == 0);
	      while (sigalrm_received == 0)
		;
	      xsignal (SIGALRM, SIG_DFL);
	      ret = timer_delete (timer);
	      TEST_VERIFY_EXIT (ret == 0);
	    }
	  else
	    {
	      ret = nanosleep (&duration, NULL);
	      TEST_VERIFY_EXIT (ret == 0);
	    }
	  t1 = time (NULL);
	  TEST_VERIFY_EXIT (t1 != (time_t) -1);
	  ret = clock_gettime (clocks[i].clockid, &ts3);
	  TEST_VERIFY_EXIT (ret == 0);
	  TEST_VERIFY (compare_timespec (&ts2, &ts3) < 0);
	  if (clocks[i].clockid == CLOCK_REALTIME)
	    TEST_VERIFY (t1 <= ts3.tv_sec);
	  TEST_VERIFY (ts3.tv_nsec >= 0);
	  TEST_VERIFY (ts3.tv_nsec < 1000000000);
	  ts2 = ts3;
	  if (clocks[i].is_cputime)
	    break;
	}
    }
  return 0;
}

#define TIMEOUT 60

#include <support/test-driver.c>
