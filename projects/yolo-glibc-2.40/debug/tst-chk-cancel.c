/* Test for required cancellation points in fortified functions (BZ #29274)
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
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xthread.h>
#include <support/xunistd.h>
#include <sys/socket.h>

/* Cleanup handling test.  */
static int cl_called;

static void
cl (void *arg)
{
  ++cl_called;
}

static int fds[2];
static pthread_barrier_t barrier;

static void *
tf_read (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __read_chk because the buffer size
     is known, but the read length is non-constant.  */
  char c;
  if (read (fds[0], &c, (uintptr_t) n) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static void *
tf_pread (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __pread_chk because the buffer size
     is known, but the read length is non-constant.  */
  char c;
  if (pread (fds[0], &c, (uintptr_t) n, 0) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static void *
tf_pread64 (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __pread64_chk because the buffer size
     is known, but the read length is non-constant.  */
  char c;
  if (pread64 (fds[0], &c, (uintptr_t) n, 0) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static void *
tf_poll (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __poll_chk because the pollfd size
     is known, but the number of entries is non-constant.  */
  struct pollfd pfd = { fds[0], POLLIN, 0 };
  if (poll (&pfd, (uintptr_t) n, -1) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static void *
tf_ppoll (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __ppoll_chk because the pollfd size
     is known, but the number of entries is non-constant.  */
  struct pollfd pfd = { fds[0], POLLIN, 0 };
  if (ppoll (&pfd, (uintptr_t) n, 0, 0) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static void *
tf_recv (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __ppoll_chk because the pollfd size
     is known, but the number of entries is non-constant.  */
  char c;
  if (recv (fds[0], &c, (uintptr_t) n, 0) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static void *
tf_recvfrom (void *n)
{
  pthread_cleanup_push (cl, NULL);

  xpthread_barrier_wait (&barrier);

  /* This call should be forwarded to __ppoll_chk because the pollfd size
     is known, but the number of entries is non-constant.  */
  char c;
  if (recvfrom (fds[0], &c, (uintptr_t) n, 0, NULL, NULL) != 1)
    return (void *) -1L;

  pthread_cleanup_pop (0);
  return 0;
}

static struct cancel_tests
{
  const char *name;
  void *(*tf) (void *);
  bool only_early;
#define ADD_TEST(name, early) { #name, tf_##name, early }
} tests[] =
{
  ADD_TEST (poll,     false),
  ADD_TEST (ppoll,    false),
  ADD_TEST (pread,    true),
  ADD_TEST (pread64,  true),
  ADD_TEST (read,     false),
  ADD_TEST (recv,     false),
  ADD_TEST (recvfrom, false),
};

/* Set the send buffer of socket S to 1 byte so any send operation
   done with WRITE_BUFFER_SIZE bytes will force syscall blocking.  */
static void
set_socket_buffer (int s)
{
  int val = 1;
  socklen_t len = sizeof (val);

  TEST_VERIFY_EXIT (setsockopt (s, SOL_SOCKET, SO_SNDBUF, &val,
                    sizeof (val)) == 0);
  TEST_VERIFY_EXIT (getsockopt (s, SOL_SOCKET, SO_SNDBUF, &val, &len) == 0);
  printf ("%s: got size %d\n", __func__, val);
}

static int
do_test (void)
{
  xpthread_barrier_init (&barrier, 0, 2);

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    FAIL_EXIT1 ("socketpair: %m");
  set_socket_buffer (fds[1]);

  /* This is the !only_early test.  It is a late cancel test that has a sleep
     in the main thread in an attempt to allow the child thread to reach and
     block on the syscall.  The cancellation should happen with high
     probability when the child thread blocked on the syscall, and that is
     the intent of the test (syscall cancellation registration complete).  */
  for (int i = 0; i < array_length (tests); i++)
    {
      if (tests[i].only_early)
	continue;

      xpthread_barrier_init (&barrier, NULL, 2);
      /* Reset the counter for the cleanup handler.  */
      cl_called = 0;

      pthread_t thr = xpthread_create (0, tests[i].tf, (void *) 1L);
      /* After this wait the threads cancellation handler is installed.  */
      xpthread_barrier_wait (&barrier);

      struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
      TEMP_FAILURE_RETRY (clock_nanosleep (CLOCK_REALTIME, 0, &ts, &ts));

      xpthread_cancel (thr);

      void *status = xpthread_join (thr);
      TEST_VERIFY (status == PTHREAD_CANCELED);
      TEST_COMPARE (cl_called, 1);

      printf ("in-time cancel test of '%s' successful\n", tests[i].name);
    }

  /* This is a early cancel test that happens before the syscall is issued.
     In this case there is no signal involved, pthread_cancel will just mark
     the target thread canceled, since asynchronous mode is not set, and the
     cancellable entrypoint will check if the thread is set as cancelled and
     exit early.

     Keep in mind that neither pthread_barrier_wait nor pthread_cleanup_push
     act as cancellation entrypoints.  */
  for (int i = 0; i < array_length (tests); i++)
    {
      xpthread_barrier_init (&barrier, NULL, 2);
      /* Reset the counter for the cleanup handler.  */
      cl_called = 0;

      /* After this wait the cancellation handler is in place.  */
      pthread_t thr = xpthread_create (0, tests[i].tf, NULL);

      xpthread_cancel (thr);
      xpthread_barrier_wait (&barrier);

      void *status = xpthread_join (thr);
      TEST_VERIFY (status == PTHREAD_CANCELED);
      TEST_COMPARE (cl_called, 1);

      printf ("early cancel test of '%s' successful\n", tests[i].name);
    }

  xpthread_barrier_destroy (&barrier);

  return 0;
}

#include <support/test-driver.c>
