/* Check if exit/quick_exit can be called concurrently by multiple threads.
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

#include <array_length.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/xthread.h>
#include <stdio.h>
#include <support/xunistd.h>
#include <string.h>

/* A value larger than the minimum required by C/POSIX (32), to trigger a
   new block memory allocation.  */
#define MAX_atexit 64

static pthread_barrier_t barrier;

static void *
tf (void *closure)
{
  xpthread_barrier_wait (&barrier);
  EXIT (0);

  return NULL;
}

static const char expected[] = "00000000000000000000000000000000000"
			       "00000000000000000000003021121130211";
static char crumbs[sizeof (expected)];
static int next_slot = 0;

static void
exit_with_flush (int code)
{
  fflush (stdout);
  /* glibc allows recursive EXIT, the ATEXIT handlers execution will be
     resumed from the where the previous EXIT was interrupted.  */
  EXIT (code);
}

/* Take some time, so another thread potentially issue EXIT.  */
#define SETUP_NANOSLEEP \
  if (nanosleep (&(struct timespec) { .tv_sec = 0, .tv_nsec = 1000L },	\
		 NULL) != 0)						\
    FAIL_EXIT1 ("nanosleep: %m")

static void
fn0 (void)
{
  crumbs[next_slot++] = '0';
  SETUP_NANOSLEEP;
}

static void
fn1 (void)
{
  crumbs[next_slot++] = '1';
  SETUP_NANOSLEEP;
}

static void
fn2 (void)
{
  crumbs[next_slot++] = '2';
  ATEXIT (fn1);
  SETUP_NANOSLEEP;
}

static void
fn3 (void)
{
  crumbs[next_slot++] = '3';
  ATEXIT (fn2);
  ATEXIT (fn0);
  SETUP_NANOSLEEP;
}

static void
fn_final (void)
{
  TEST_COMPARE_STRING (crumbs, expected);
  exit_with_flush (0);
}

_Noreturn static void
child (void)
{
  enum { nthreads = 8 };

  xpthread_barrier_init (&barrier, NULL, nthreads + 1);

  pthread_t thr[nthreads];
  for (int i = 0; i < nthreads; i++)
    thr[i] = xpthread_create (NULL, tf, NULL);

  xpthread_barrier_wait (&barrier);

  for (int i = 0; i < nthreads; i++)
    {
      pthread_join (thr[i], NULL);
      /* It should not be reached, it means that thread did not exit for
	 some reason.  */
      support_record_failure ();
    }

  EXIT (2);
}

static int
do_test (void)
{
  /* Register a large number of handler that will trigger a heap allocation
     for the handle state.  On EXIT, each block will be freed after the
     handle is processed.  */
  int slots_remaining = MAX_atexit;

  /* Register this first so it can verify expected order of the rest.  */
  ATEXIT (fn_final); --slots_remaining;

  TEST_VERIFY_EXIT (ATEXIT (fn1) == 0); --slots_remaining;
  TEST_VERIFY_EXIT (ATEXIT (fn3) == 0); --slots_remaining;
  TEST_VERIFY_EXIT (ATEXIT (fn1) == 0); --slots_remaining;
  TEST_VERIFY_EXIT (ATEXIT (fn2) == 0); --slots_remaining;
  TEST_VERIFY_EXIT (ATEXIT (fn1) == 0); --slots_remaining;
  TEST_VERIFY_EXIT (ATEXIT (fn3) == 0); --slots_remaining;

  while (slots_remaining > 0)
    {
      TEST_VERIFY_EXIT (ATEXIT (fn0) == 0); --slots_remaining;
    }

  pid_t pid = xfork ();
  if (pid != 0)
    {
      int status;
      xwaitpid (pid, &status, 0);
      TEST_VERIFY (WIFEXITED (status));
    }
  else
    child ();

  return 0;
}

#include <support/test-driver.c>
