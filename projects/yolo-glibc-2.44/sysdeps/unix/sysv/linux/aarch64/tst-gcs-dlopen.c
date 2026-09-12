/* AArch64 tests for GCS for dlopen use case.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <string.h>

static int
do_test (void)
{
  /* Check if GCS could possible by enabled.  */
  if (!(getauxval (AT_HWCAP) & HWCAP_GCS))
    FAIL_UNSUPPORTED ("kernel or CPU does not support GCS");

  /* The tst-gcs-mod2.so test library does not have GCS marking.  */
  void *h = dlopen ("tst-gcs-mod2.so", RTLD_NOW);
  const char *err = dlerror ();

#if TEST_GCS_EXPECT_DLOPEN
  TEST_VERIFY (h != NULL);
#else
  TEST_VERIFY (h == NULL);
  /* Only accept expected GCS-related errors.  */
  TEST_VERIFY (strstr (err, "not GCS compatible") != NULL);
#endif

#if TEST_GCS_EXPECT_ENABLED
  TEST_VERIFY (__check_gcs_status ());
#else
  TEST_VERIFY (!__check_gcs_status ());
#endif

  if (h == NULL)
    printf ("dlopen error: %s\n", err);
  else
    {
      puts ("library loaded normally");
      dlclose (h);
    }

  return 0;
}

#include <support/test-driver.c>
