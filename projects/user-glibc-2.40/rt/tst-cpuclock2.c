/* Test program for process and thread CPU clocks.
   Copyright (C) 2005-2024 Free Software Foundation, Inc.
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

#include <unistd.h>
#include <stdint.h>

#if (_POSIX_THREADS - 0) <= 0

static int
do_test ()
{
  return 0;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <support/xunistd.h>

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
      int nullfd = open ("/dev/null", O_WRONLY);
      for (int i = 0; i < 100; ++i)
	for (size_t j = 0; j < sizeof buf; ++j)
	  buf[j] = 0xbb;
      xwrite (nullfd, (char *) buf, sizeof buf);
      close (nullfd);
    }

  return NULL;
}

static void
test_nanosleep (clockid_t clock, const char *which,
		int *bad)
{
  const struct timespec sleeptime = { .tv_nsec = 100000000 };
  int e = clock_nanosleep (clock, 0, &sleeptime, NULL);
  if (e == EINVAL || e == ENOTSUP || e == ENOSYS)
    {
      printf ("clock_nanosleep not supported for %s CPU clock: %s\n",
	      which, strerror (e));
      return;
    }
  if (e != 0)
    {
      printf ("clock_nanosleep on %s CPU clock: %s\n", which, strerror (e));
      *bad = 1;
      return;
    }

  struct timespec after;
  if (clock_gettime (clock, &after) < 0)
    {
      printf ("clock_gettime on %s CPU clock %lx => %s\n",
	      which, (unsigned long int) clock, strerror (errno));
      *bad = 1;
      return;
    }

  struct timespec sleeptimeabs = sleeptime;
  sleeptimeabs.tv_sec += after.tv_sec;
  sleeptimeabs.tv_nsec += after.tv_nsec;
  while (sleeptimeabs.tv_nsec >= 1000000000)
    {
      ++sleeptimeabs.tv_sec;
      sleeptimeabs.tv_nsec -= 1000000000;
    }
  e = clock_nanosleep (clock, TIMER_ABSTIME, &sleeptimeabs, NULL);
  if (e != 0)
    {
      printf ("absolute clock_nanosleep on %s CPU clock: %s\n",
	      which, strerror (e));
      *bad = 1;
      return;
    }

  struct timespec afterabs;
  if (clock_gettime (clock, &afterabs) < 0)
    {
      printf ("clock_gettime on %s CPU clock %lx => %s\n",
	      which, (unsigned long int) clock, strerror (errno));
      *bad = 1;
      return;
    }

  return;
}



static int
do_test (void)
{
  int result = 0;
  clockid_t process_clock, th_clock, my_thread_clock;
  int e;
  pthread_t th;

  e = clock_getcpuclockid (0, &process_clock);
  if (e != 0)
    {
      printf ("clock_getcpuclockid on self => %s\n", strerror (e));
      return 1;
    }

  e = pthread_getcpuclockid (pthread_self (), &my_thread_clock);
  if (e != 0)
    {
      printf ("pthread_getcpuclockid on self => %s\n", strerror (e));
      return 1;
    }

  /* This is a kludge.  This test fails if the semantics of thread and
     process clocks are wrong.  The old code using hp-timing without kernel
     support has bogus semantics if there are context switches.  We don't
     fail to report failure when the proper functionality is not available
     in the kernel.  It so happens that Linux kernels without correct CPU
     clock support also lack CPU timer support, so we use use that to guess
     that we are using the bogus code and not test it.  */
  timer_t t;
  if (timer_create (my_thread_clock, NULL, &t) != 0)
    {
      printf ("timer_create: %m\n");
      puts ("No support for CPU clocks with good semantics, skipping test");
      return 0;
    }
  timer_delete (t);


  pthread_barrier_init (&barrier, NULL, 2);

  e = pthread_create (&th, NULL, chew_cpu, NULL);
  if (e != 0)
    {
      printf ("pthread_create: %s\n", strerror (e));
      return 1;
    }

  e = pthread_getcpuclockid (th, &th_clock);
  if (e == ENOENT || e == ENOSYS || e == ENOTSUP)
    {
      puts ("pthread_getcpuclockid does not support other threads");
      return 1;
    }

  pthread_barrier_wait (&barrier);

  struct timespec res;
  if (clock_getres (th_clock, &res) < 0)
    {
      printf ("clock_getres on live thread clock %lx => %s\n",
	      (unsigned long int) th_clock, strerror (errno));
      result = 1;
      return 1;
    }
  printf ("live thread clock %lx resolution %ju.%.9ju\n",
	  (unsigned long int) th_clock,
	  (uintmax_t) res.tv_sec, (uintmax_t) res.tv_nsec);

  struct timespec process_before, process_after;
  if (clock_gettime (process_clock, &process_before) < 0)
    {
      printf ("clock_gettime on process clock %lx => %s\n",
	      (unsigned long int) process_clock, strerror (errno));
      return 1;
    }

  struct timespec before, after;
  if (clock_gettime (th_clock, &before) < 0)
    {
      printf ("clock_gettime on live thread clock %lx => %s\n",
	      (unsigned long int) th_clock, strerror (errno));
      return 1;
    }
  printf ("live thread before sleep => %ju.%.9ju\n",
	  (uintmax_t) before.tv_sec, (uintmax_t) before.tv_nsec);

  struct timespec me_before, me_after;
  if (clock_gettime (my_thread_clock, &me_before) < 0)
    {
      printf ("clock_gettime on self thread clock %lx => %s\n",
	      (unsigned long int) my_thread_clock, strerror (errno));
      return 1;
    }
  printf ("self thread before sleep => %ju.%.9ju\n",
	  (uintmax_t) me_before.tv_sec, (uintmax_t) me_before.tv_nsec);

  struct timespec sleeptime = { .tv_nsec = 500000000 };
  if (nanosleep (&sleeptime, NULL) != 0)
    {
      perror ("nanosleep");
      return 1;
    }

  if (clock_gettime (th_clock, &after) < 0)
    {
      printf ("clock_gettime on live thread clock %lx => %s\n",
	      (unsigned long int) th_clock, strerror (errno));
      return 1;
    }
  printf ("live thread after sleep => %ju.%.9ju\n",
	  (uintmax_t) after.tv_sec, (uintmax_t) after.tv_nsec);

  if (clock_gettime (process_clock, &process_after) < 0)
    {
      printf ("clock_gettime on process clock %lx => %s\n",
	      (unsigned long int) process_clock, strerror (errno));
      return 1;
    }

  if (clock_gettime (my_thread_clock, &me_after) < 0)
    {
      printf ("clock_gettime on self thread clock %lx => %s\n",
	      (unsigned long int) my_thread_clock, strerror (errno));
      return 1;
    }
  printf ("self thread after sleep => %ju.%.9ju\n",
	  (uintmax_t) me_after.tv_sec, (uintmax_t) me_after.tv_nsec);

  test_nanosleep (th_clock, "live thread",
		  &result);
  test_nanosleep (process_clock, "process",
		  &result);
  test_nanosleep (CLOCK_PROCESS_CPUTIME_ID,
		  "PROCESS_CPUTIME_ID", &result);

  pthread_cancel (th);

  e = clock_nanosleep (CLOCK_THREAD_CPUTIME_ID, 0, &sleeptime, NULL);
  if (e != EINVAL)
    {
      printf ("clock_nanosleep CLOCK_THREAD_CPUTIME_ID: %s\n",
	      strerror (e));
      result = 1;
    }

  return result;
}
#endif

#include <support/test-driver.c>
