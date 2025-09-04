/* Test the __sockaddr_un_set function.
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

/* Re-compile the function because the version in libc is not
   exported.  */
#include "sockaddr_un_set.c"

#include <support/check.h>

static int
do_test (void)
{
  struct sockaddr_un sun;

  memset (&sun, 0xcc, sizeof (sun));
  __sockaddr_un_set (&sun, "");
  TEST_COMPARE (sun.sun_family, AF_UNIX);
  TEST_COMPARE (__sockaddr_un_set (&sun, ""), 0);

  memset (&sun, 0xcc, sizeof (sun));
  TEST_COMPARE (__sockaddr_un_set (&sun, "/example"), 0);
  TEST_COMPARE_STRING (sun.sun_path, "/example");

  {
    char pathname[108];         /* Length of sun_path (ABI constant).  */
    memset (pathname, 'x', sizeof (pathname));
    pathname[sizeof (pathname) - 1] = '\0';
    memset (&sun, 0xcc, sizeof (sun));
    TEST_COMPARE (__sockaddr_un_set (&sun, pathname), 0);
    TEST_COMPARE (sun.sun_family, AF_UNIX);
    TEST_COMPARE_STRING (sun.sun_path, pathname);
  }

  {
    char pathname[109];
    memset (pathname, 'x', sizeof (pathname));
    pathname[sizeof (pathname) - 1] = '\0';
    memset (&sun, 0xcc, sizeof (sun));
    errno = 0;
    TEST_COMPARE (__sockaddr_un_set (&sun, pathname), -1);
    TEST_COMPARE (errno, EINVAL);
  }

  return 0;
}

#include <support/test-driver.c>
