/* Test that dprintf returns the expected length.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <sys/socket.h>
#include <unistd.h>

static int
do_test (void)
{
  /* Use a datagram socket to check that everything arrives in one packet.
     The dprintf function should perform a single write call.  */
  int fds[2];
  TEST_VERIFY_EXIT (socketpair (AF_LOCAL, SOCK_DGRAM, 0, fds) == 0);

  TEST_COMPARE (dprintf (fds[0], "(%d)%s[%d]", 123, "---", 4567), 14);

  char buf[32];
  ssize_t ret = read (fds[1], buf, sizeof (buf));
  TEST_VERIFY_EXIT (ret > 0);
  TEST_COMPARE_BLOB (buf, ret, "(123)---[4567]", strlen ("(123)---[4567]"));

  close (fds[1]);
  close (fds[0]);
  return 0;
}

#include <support/test-driver.c>
