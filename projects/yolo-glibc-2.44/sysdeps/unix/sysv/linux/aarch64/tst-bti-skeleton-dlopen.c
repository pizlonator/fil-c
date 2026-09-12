/* Simple test for BTI support with dlopen: this base source file is
   included in several tests.
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

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/signal.h>

#include <support/check.h>
#include <support/test-driver.h>

typedef int (*fun_t) (void);

static int
do_test (void)
{
  unsigned long hwcap2 = getauxval (AT_HWCAP2);
  if ((hwcap2 & HWCAP2_BTI) == 0)
    FAIL_UNSUPPORTED ("BTI is not supported by this system");

  void *h = dlopen (TEST_BTI_DLOPEN_MODULE, RTLD_NOW);
  const char *err = dlerror ();

#if TEST_BTI_EXPECT_DLOPEN
  TEST_VERIFY (h != NULL);
#else
  TEST_VERIFY (h == NULL);
  /* Only accept expected BTI-related errors.  */
  TEST_VERIFY (strstr (err, "failed to turn on BTI protection") != NULL);
#endif

  if (h == NULL)
    printf ("dlopen error: %s\n", err);
  else
    {
      puts ("library "  TEST_BTI_DLOPEN_MODULE " loaded normally");
      dlclose (h);
    }

  return 0;
}

#include <support/test-driver.c>
