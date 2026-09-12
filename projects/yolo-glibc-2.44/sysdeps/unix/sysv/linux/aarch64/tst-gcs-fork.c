/* AArch64 test for GCS for creating child process using fork.
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

#include <support/xunistd.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

static int
do_test (void)
{
  /* Check if GCS could possible by enabled.  */
  if (!(getauxval (AT_HWCAP) & HWCAP_GCS))
    FAIL_UNSUPPORTED ("kernel or CPU does not support GCS");

  /* GCS should be enabled for this test at the start.  */
  TEST_VERIFY (__check_gcs_status ());

  pid_t pid = xfork ();
  const char *name;
  if (pid == 0)
    name = "child";
  else
    name = "parent";

  /* Both parent and child should initially have GCS enabled.  */
  TEST_VERIFY (__check_gcs_status ());
  uint64_t data;
  if (prctl (PR_GET_SHADOW_STACK_STATUS, &data, 0, 0, 0))
    FAIL_EXIT1 ("prctl: %m");
  printf ("in %s: gcs status: %016lx\n", name, data);

  if (pid)
    {
      int status;
      xwaitpid (pid, &status, 0);
      printf ("in %s: child exited with code %u\n", name, WEXITSTATUS(status));
    }
  else
    {
      /* Try disabling GCS for the child
	 (should succeed because of the tunable).  */
      if (prctl (PR_SET_SHADOW_STACK_STATUS, 0, 0, 0, 0))
	FAIL_EXIT1 ("prctl: %m");
      /* GCS should be disabled.  */
      TEST_VERIFY (!__check_gcs_status ());
      if (prctl (PR_GET_SHADOW_STACK_STATUS, &data, 0, 0, 0))
	FAIL_EXIT1 ("prctl: %m");
      printf ("in %s: gcs status: %016lx\n", name, data);
    }

  return 0;
}

#include <support/test-driver.c>
