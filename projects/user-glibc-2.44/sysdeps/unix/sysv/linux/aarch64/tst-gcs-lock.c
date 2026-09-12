/* AArch64 test for GCS locking.
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

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <errno.h>

static int
do_test (void)
{
  /* Check if GCS could possible by enabled.  */
  if (!(getauxval (AT_HWCAP) & HWCAP_GCS))
    FAIL_UNSUPPORTED ("kernel or CPU does not support GCS");

  TEST_VERIFY (__check_gcs_status ());

  /* Try disabling GCS.  */
  int res = prctl (PR_SET_SHADOW_STACK_STATUS, 0, 0, 0, 0);
  if (res)
    {
      TEST_COMPARE (errno, EBUSY);
#ifdef GCS_SHOULD_UNLOCK
      FAIL_EXIT1 ("GCS was not unlocked (was supposed to): %m");
#else
      TEST_VERIFY (__check_gcs_status ());
#endif
    }
  else
    {
#ifdef GCS_SHOULD_UNLOCK
      TEST_VERIFY (!__check_gcs_status ());
      puts ("GCS unlocked successfully");
#else
      FAIL_EXIT1 ("GCS was unlocked (was not supposed to)");
#endif
    }

  return 0;
}

#include <support/test-driver.c>
