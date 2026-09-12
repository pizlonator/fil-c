/* Verify side-effects of cancellable syscalls (BZ #12683).
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

/* This testcase checks if there is resource leakage if the syscall has
   returned from kernelspace, but before userspace saves the return
   value.  The 'leaker' thread should be able to close the file descriptor
   if the resource is already allocated, meaning that if the cancellation
   signal arrives *after* the open syscal return from kernel, the
   side-effect should be visible to application.  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <support/xunistd.h>
#include <support/xthread.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/support.h>
#include <support/descriptors.h>

static void *
writeopener (void *arg)
{
  int fd;
  for (;;)
    {
      fd = open (arg, O_WRONLY);
      pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, 0);
      xclose (fd);
      pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
    }
  return NULL;
}

static void *
leaker (void *arg)
{
  int fd = open (arg, O_RDONLY);
  TEST_VERIFY_EXIT (fd > 0);
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, 0);
  xclose (fd);
  return NULL;
}

static int
do_test (void)
{
  enum {
    iter_count = 1000
  };

  char *dir = support_create_temp_directory ("tst-cancel28");
  char *name = xasprintf ("%s/fifo", dir);
  TEST_COMPARE (mkfifo (name, 0600), 0);
  add_temp_file (name);

  struct support_descriptors *descrs = support_descriptors_list ();

  srand (1);

  pthread_t tw = xpthread_create (NULL, writeopener, name);
  for (int i = 0; i < iter_count; i++)
    {
      pthread_t td = xpthread_create (NULL, leaker, name);
      struct timespec ts =
	{ .tv_nsec = rand () % 100000, .tv_sec = 0 };
      nanosleep (&ts, NULL);
      /* Ignore pthread_cancel result because it might be the
	 case when pthread_cancel is called when thread is already
	 exited.  */
      pthread_cancel (td);
      xpthread_join (td);
    }

  pthread_cancel (tw);
  xpthread_join (tw);

  support_descriptors_check (descrs);

  support_descriptors_free (descrs);

  free (name);

  return 0;
}

#include <support/test-driver.c>
