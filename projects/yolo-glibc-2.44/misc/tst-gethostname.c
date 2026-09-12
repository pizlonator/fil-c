/* Basic test for gethostname.
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

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>

#include <support/check.h>
#include <support/namespace.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#define MAX_LEN_INCREASE 10


static int
do_test (void)
{
  char default_hostname[HOST_NAME_MAX + 1] = "\0";
  char hostname[HOST_NAME_MAX + 1] = "\0";
  char generated_hostname[HOST_NAME_MAX + MAX_LEN_INCREASE + 1] = "\0";

  /* sethostname needs support_become_root for namespace privileges. */
  support_become_root ();

  /* Enter a UTS namespace so sethostname does not affect the host.  */
#ifdef CLONE_NEWUTS
  if (unshare (CLONE_NEWUTS) != 0)
    FAIL_UNSUPPORTED ("unshare (CLONE_NEWUTS): %m");
#else
  FAIL_UNSUPPORTED ("CLONE_NEWUTS not supported");
#endif

  /* Check default hostname is not an empty string.  */
  TEST_VERIFY_EXIT (gethostname (default_hostname, sizeof (default_hostname))
		    == 0);
  TEST_VERIFY (strlen (default_hostname) > 0);

  /* Check that sethostname is permitted in the UTS namespace.  */
  if (sethostname ("a", 1) != 0)
    FAIL_UNSUPPORTED ("sethostname: %m");
  TEST_VERIFY_EXIT (sethostname (default_hostname,
				 strlen (default_hostname)) == 0);

  /* input: ''
     expected output: '' */
  TEST_VERIFY_EXIT (sethostname ("", 0) == 0);
  TEST_VERIFY (gethostname (hostname, sizeof (hostname)) == 0);
  TEST_VERIFY (strlen (hostname) == 0);

  /* input: 'a'
     expected output: 'a' */
  TEST_VERIFY_EXIT (sethostname ("a", strlen ("a")) == 0);
  TEST_VERIFY (gethostname (hostname, sizeof (hostname)) == 0);
  TEST_COMPARE (strlen (hostname), 1);
  TEST_VERIFY (strcmp (hostname, "a") == 0);

  /* input: abc.def.ghi
     expected output: abc.def.ghi */
  TEST_VERIFY_EXIT (sethostname ("abc.def.ghi", strlen ("abc.def.ghi")) == 0);
  TEST_VERIFY (gethostname (hostname, sizeof (hostname)) == 0);
  TEST_COMPARE (strlen (hostname), strlen ("abc.def.ghi"));
  TEST_VERIFY (strcmp (hostname, "abc.def.ghi") == 0);

  /* input: exactly HOST_NAME_MAX
     expected output: exactly HOST_NAME_MAX, gethostname returns -1,
     errno set.  */
  for (int i = 0; i < HOST_NAME_MAX; i++)
    {
      /* Generate hostname of length == HOST_NAME_MAX.  */
      generated_hostname[i] = 'a' + i % 26;
    }
  TEST_VERIFY_EXIT (sethostname (generated_hostname,
                                 strlen (generated_hostname)) == 0);
  errno = 0;
  TEST_VERIFY (gethostname (hostname, HOST_NAME_MAX) == -1);
  TEST_COMPARE (errno, ENAMETOOLONG);
  TEST_COMPARE (strlen (hostname), HOST_NAME_MAX);
  TEST_VERIFY (strcmp (hostname, generated_hostname) == 0);

  /* input: longer than HOST_NAME_MAX
     expected output: sethostname fails, return -1, errno set */
  for (int i = 0; i < HOST_NAME_MAX + MAX_LEN_INCREASE; i++)
    {
      /* Generate hostname LONGER than HOST_NAME_MAX.  */
      generated_hostname[i] = 'a' + i % 26;
    }
  errno = 0;
  TEST_VERIFY (sethostname (generated_hostname,
			    strlen (generated_hostname)) == -1);
  TEST_COMPARE (errno, EINVAL);

  /* Restore default hostname.  */
  TEST_VERIFY_EXIT (sethostname (default_hostname,
				 strlen (default_hostname)) == 0);
  return 0;
}

#include <support/test-driver.c>
