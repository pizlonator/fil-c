/* Test to verify that overlong hostname is rejected by clnt_create
   and doesn't cause a buffer overflow (bug  22542).

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
   <http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <rpc/clnt.h>
#include <string.h>
#include <support/check.h>
#include <sys/socket.h>
#include <sys/un.h>

static int
do_test (void)
{
  /* Create an arbitrary hostname that's longer than fits in sun_path.  */
  char name [sizeof ((struct sockaddr_un*)0)->sun_path * 2];
  memset (name, 'x', sizeof name - 1);
  name [sizeof name - 1] = '\0';

  errno = 0;
  CLIENT *clnt = clnt_create (name, 0, 0, "unix");

  TEST_VERIFY (clnt == NULL);
  TEST_COMPARE (errno, EINVAL);
  return 0;
}

#include <support/test-driver.c>
