/* Check if a thread that disables cancellation and which call functions
   that might be interrupted by a signal do not see the internal SIGCANCEL.

   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xthread.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

/* On Linux some interfaces are never restarted after being interrupted by
   a signal handler, regardless of the use of SA_RESTART.  It means that
   if asynchronous cancellation is not enabled, the pthread_cancel can not
   set the internal SIGCANCEL otherwise the interface might see a spurious
   EINTR failure.  */

static pthread_barrier_t b;

/* Cleanup handling test.  */
static int cl_called;
static void
cl (void *arg)
{
  ++cl_called;
}

static void *
tf_sigtimedwait (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
  xpthread_barrier_wait (&b);

  int r;
  pthread_cleanup_push (cl, NULL);

  sigset_t mask;
  sigemptyset (&mask);
  r = sigtimedwait (&mask, NULL, &(struct timespec) { 0, 250000000 });
  if (r != -1)
    return (void*) -1;
  if (errno != EAGAIN)
    return (void*) -2;

  pthread_cleanup_pop (0);
  return NULL;
}

static void *
tf_poll (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
  xpthread_barrier_wait (&b);

  int r;
  pthread_cleanup_push (cl, NULL);

  r = poll (NULL, 0, 250);
  if (r != 0)
    return (void*) -1;

  pthread_cleanup_pop (0);
  return NULL;
}

static void *
tf_ppoll (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);

  xpthread_barrier_wait (&b);

  int r;
  pthread_cleanup_push (cl, NULL);

  r = ppoll (NULL, 0, &(struct timespec) { 0, 250000000 }, NULL);
  if (r != 0)
    return (void*) -1;

  pthread_cleanup_pop (0);
  return NULL;
}

static void *
tf_select (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
  xpthread_barrier_wait (&b);

  int r;
  pthread_cleanup_push (cl, NULL);

  r = select (0, NULL, NULL, NULL, &(struct timeval) { 0, 250000 });
  if (r != 0)
    return (void*) -1;

  pthread_cleanup_pop (0);
  return NULL;
}

static void *
tf_pselect (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
  xpthread_barrier_wait (&b);

  int r;
  pthread_cleanup_push (cl, NULL);

  r = pselect (0, NULL, NULL, NULL, &(struct timespec) { 0, 250000000 }, NULL);
  if (r != 0)
    return (void*) -1;

  pthread_cleanup_pop (0);
  return NULL;
}

static void *
tf_clock_nanosleep (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
  xpthread_barrier_wait (&b);

  int r;
  pthread_cleanup_push (cl, NULL);

  r = clock_nanosleep (CLOCK_REALTIME, 0, &(struct timespec) { 0, 250000000 },
		       NULL);
  if (r != 0)
    return (void*) -1;

  pthread_cleanup_pop (0);
  return NULL;
}

struct cancel_test_t
{
  const char *name;
  void * (*cf) (void *);
} tests[] =
{
  { "sigtimedwait",    tf_sigtimedwait,    },
  { "poll",            tf_poll,            },
  { "ppoll",           tf_ppoll,           },
  { "select",          tf_select,          },
  { "pselect",         tf_pselect  ,       },
  { "clock_nanosleep", tf_clock_nanosleep, },
};

static int
do_test (void)
{
  for (int i = 0; i < array_length (tests); i++)
    {
      xpthread_barrier_init (&b, NULL, 2);

      cl_called = 0;

      pthread_t th = xpthread_create (NULL, tests[i].cf, NULL);

      xpthread_barrier_wait (&b);

      struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
      while (nanosleep (&ts, &ts) != 0)
	continue;

      xpthread_cancel (th);

      void *status = xpthread_join (th);
      if (status != NULL)
	printf ("test '%s' failed: %" PRIdPTR "\n", tests[i].name,
		(intptr_t) status);
      TEST_VERIFY (status == NULL);

      xpthread_barrier_destroy (&b);

      TEST_COMPARE (cl_called, 0);

      printf ("in-time cancel test of '%s' successful\n", tests[i].name);
    }

  return 0;
}

#include <support/test-driver.c>
