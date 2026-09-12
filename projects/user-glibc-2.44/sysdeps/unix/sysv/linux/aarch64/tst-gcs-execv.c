/* AArch64 test for GCS for creating child process.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include "tst-gcs-helper.h"

#include <sys/prctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static int
target (void)
{
  /* In child.  */
  printf ("in child: %u\n", getpid ());
  TEST_VERIFY (__check_gcs_status ());

  /* Try disabling GCS (should fail with EBUSY).  */
  int res = prctl (PR_SET_SHADOW_STACK_STATUS, 0, 0, 0, 0);
  TEST_COMPARE (res, -1);
  TEST_COMPARE (errno, EBUSY);
  return 0;
}

int main(int argc, char *argv[])
{
  /* Check if GCS could possible by enabled.  */
  if (!(getauxval (AT_HWCAP) & HWCAP_GCS))
    FAIL_UNSUPPORTED ("kernel or CPU does not support GCS");

  /* GCS should be enabled for this test at the start.  */
  TEST_VERIFY (__check_gcs_status ());

  /* If last argument is 'target', we just run target code.  */
  if (strcmp (argv[argc - 1], "target") == 0)
    return target ();

  /* In parent, we should at least have 3 arguments.  */
  if (argc < 3)
    FAIL_EXIT1 ("wrong number of arguments: %d", argc);

  char *child_args[] = { NULL, NULL, NULL, NULL, NULL, NULL };

  /* Check command line arguments to construct child command.  */
  if (strcmp (argv[0], argv[2]) == 0)
    {
      /* Command looks like
	 /path/to/test -- /path/to/test  */
      /* /path/to/test  */
      child_args[0] = argv[0];
      /* Extra argument for the child process.  */
      child_args[1] = (char *)"target";
    }
  else
    {
      /* Command looks like
	 /path/to/test -- /path/to/ld.so ...  */
      TEST_VERIFY_EXIT (argc > 5);
      TEST_COMPARE_STRING (argv[3], "--library-path");
      /* /path/to/ld-linux-aarch64.so.1  */
      child_args[0] = argv[2];
      /* --library-path  */
      child_args[1] = argv[3];
      /* Library path...  */
      child_args[2] = argv[4];
      /* /path/to/test  */
      child_args[3] = argv[5];
      /* Extra argument for the child process.  */
      child_args[4] = (char *)"target";
    }

  printf ("in parent: %u\n", getpid ());
  /* Spawn child process.  */
  execv (child_args[0], child_args);
  FAIL_EXIT1 ("execv: %m");
}
