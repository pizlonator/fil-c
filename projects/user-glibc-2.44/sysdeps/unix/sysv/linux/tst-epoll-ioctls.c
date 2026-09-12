/* Basic tests for Linux epoll ioctls.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <intprops.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/process_state.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/xsignal.h>
#include <support/xunistd.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

static void
test_epoll_ioctl (void)
{
  int efd = epoll_create1 (0);
  TEST_VERIFY_EXIT (efd != -1);

  struct epoll_params params;

  TEST_COMPARE (ioctl (efd, EPIOCGPARAMS, &params), 0);

  /* parameters are all 0 by default */
  TEST_COMPARE (params.busy_poll_usecs, 0);
  TEST_COMPARE (params.busy_poll_budget, 0);
  TEST_COMPARE (params.prefer_busy_poll, 0);
  TEST_COMPARE (params.__pad, 0);

  /* set custom parameters */
  params.busy_poll_usecs = 40;
  params.busy_poll_budget = 8;
  params.prefer_busy_poll = 1;
  params.__pad = 0;

  TEST_COMPARE (ioctl (efd, EPIOCSPARAMS, &params), 0);

  memset (&params, 0, sizeof (params));

  TEST_COMPARE (ioctl (efd, EPIOCGPARAMS, &params), 0);

  /* check custom values were retrieved after being set */
  TEST_COMPARE (params.busy_poll_usecs, 40);
  TEST_COMPARE (params.busy_poll_budget, 8);
  TEST_COMPARE (params.prefer_busy_poll, 1);
  TEST_COMPARE (params.__pad, 0);

  xclose (efd);
}

static bool
ioctl_supported (void)
{
  int efd = epoll_create1 (0);
  TEST_VERIFY_EXIT (efd != -1);

  struct epoll_params params;
  int r = ioctl (efd, EPIOCGPARAMS, &params);
  xclose (efd);

  return (r == 0);
}

static int
do_test (void)
{
  if (ioctl_supported ())
    test_epoll_ioctl ();
  else
    return EXIT_UNSUPPORTED;

  return 0;
}

#include <support/test-driver.c>
