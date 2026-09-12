/* Test for locale loading error handling (Bug 14247)
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

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <support/check.h>
#include <support/support.h>

static int
do_test (void)
{
  locale_t loc = NULL;

  /* We must load an en_US-based locale for the final test to fail.
     The locale loading code will find the en_US locale already loaded
     but the codesets won't match.  */
  xsetlocale (LC_ALL, "en_US.UTF-8");

  /* Call newlocale with an invalid locale.  We expect the test system
     does not have "invalidlocale" locale.  */
  errno = 0;
  loc = newlocale (LC_COLLATE_MASK | LC_CTYPE_MASK, "invalidlocale.utf8", 0);
  TEST_VERIFY (loc == NULL && errno != 0);

  /* Call newlocale again with the same name. This triggers bug 14247 where
     the second call fails to set errno correctly.  */
  errno = 0;
  loc = newlocale (LC_COLLATE_MASK | LC_CTYPE_MASK, "invalidlocale.utf8", 0);
  TEST_VERIFY (loc == NULL && errno != 0);

  /* Next we attempt to load a locale that exists but whose codeset
     does not match the codeset of the locale on the system.
     This is more difficult to test but we rely on the fact that locale
     processing will normalize the locale name and attempt to open
     "en_US" with no codeset as a fallback and this will allow us to
     compare a loaded "en_US" locale with a UTF-8 codeset to the
     ficiticious "en_US.utf99" and get a codeset match failure.  */
  errno = 0;
  loc = newlocale (LC_COLLATE_MASK | LC_CTYPE_MASK, "en_US.utf99", 0);
  TEST_VERIFY (loc == NULL && errno != 0);

  /* Call newlocale again with the same name. This triggers bug 14247 where
     the second call fails to set errno correctly.  */
  errno = 0;
  loc = newlocale (LC_COLLATE_MASK | LC_CTYPE_MASK, "en_US.utf99", 0);
  TEST_VERIFY (loc == NULL && errno != 0);

  return 0;
}

#include <support/test-driver.c>
