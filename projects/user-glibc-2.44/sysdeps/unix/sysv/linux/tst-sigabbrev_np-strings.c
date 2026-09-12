/* Test that sig{abbrev,descr}_np return unique strings for each signal.

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

#include <string.h>
#include <stdlib.h>

#include <support/support.h>
#include <support/check.h>

#include "tst-verify-unique-strings.c"

/* Both functions should return NULL for (unknown) signal numbers outside
   [1, 31].  */

static int
do_test (void)
{
  char *str_sigabbrev[31];
  char *str_sigdescr[31];

  for (int i = -128; i <= 128; i++)
    if (i >= 1 && i <= 31)
      {
         str_sigabbrev[i-1] = xstrdup (sigabbrev_np (i));
         str_sigdescr[i-1] = xstrdup (sigdescr_np (i));
      }
    else
      {
        TEST_VERIFY_EXIT (sigabbrev_np (i) == NULL);
        TEST_VERIFY_EXIT (sigdescr_np (i) == NULL);
      }

  VERIFY_UNIQUE_STRINGS (str_sigabbrev, 31);
  VERIFY_UNIQUE_STRINGS (str_sigdescr, 31);

  for (int i = 0; i < 31; i++)
    {
      free (str_sigabbrev[i]);
      free (str_sigdescr[i]);
    }

  return 0;
}

#include <support/test-driver.c>
