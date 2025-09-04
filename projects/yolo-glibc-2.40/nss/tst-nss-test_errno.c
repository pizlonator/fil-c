/* getpwent failure when dlopen clobbers errno (bug 28953).
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

#include <nss.h>
#include <support/check.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <pwd.h>
#include <string.h>

static int
do_test (void)
{
  __nss_configure_lookup ("passwd", "files test_errno");

  errno = 0;
  setpwent ();
  TEST_COMPARE (errno, 0);

  bool root_seen = false;
  while (true)
    {
      errno = 0;
      struct passwd *e = getpwent ();
      if (e == NULL)
        break;
      if (strcmp (e->pw_name, "root"))
        root_seen = true;
    }

  TEST_COMPARE (errno, 0);
  TEST_VERIFY (root_seen);

  errno = 0;
  endpwent ();
  TEST_COMPARE (errno, 0);

  TEST_COMPARE_STRING (getenv ("_nss_test_errno_setpwent"), "yes");
  TEST_COMPARE_STRING (getenv ("_nss_test_errno_getpwent_r"), "yes");
  TEST_COMPARE_STRING (getenv ("_nss_test_errno_endpwent"), "yes");

  return 0;
}

#include <support/test-driver.c>
