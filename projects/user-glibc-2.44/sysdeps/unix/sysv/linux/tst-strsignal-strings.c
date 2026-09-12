/* Test that strsignal returns unique strings for each signal.

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
#include <signal.h>

#include <support/support.h>
#include <support/check.h>

#include "tst-verify-unique-strings.c"

#define NSTRINGS 257

static int
do_test (void)
{
  char *string[NSTRINGS];
  /* Convenient indexing for signal strings from -128 to 128.  */
  char **sig_str = string + 128;

  unsetenv ("LANGUAGE");

  xsetlocale (LC_ALL, "C");

  for (int i = -128; i <= 128; i++)
    {
      sig_str[i] = xstrdup (strsignal (i));

      if (i > 0 && i <= 31)
        {
          /* Signals between 1 and 31 are known.  */
          TEST_VERIFY_EXIT (strstr (sig_str[i], "Unknown signal ")
                            == NULL);
          TEST_VERIFY_EXIT (strstr (sig_str[i], "Real-time signal ")
                            == NULL);
        }
      else if ((i <= 0)
          || (i > 31 && i < SIGRTMIN)
          || (i > SIGRTMAX))
        TEST_VERIFY_EXIT (strstr (sig_str[i], "Unknown signal ")
                          == sig_str[i]);

      else if (i >= SIGRTMIN && i <= SIGRTMAX)
        TEST_VERIFY_EXIT (strstr (sig_str[i], "Real-time signal ")
                          == sig_str[i]);
    }

  VERIFY_UNIQUE_STRINGS (string, NSTRINGS);

  for (int i = -128; i <= 128; i++)
    free (sig_str[i]);

  return 0;
}

#include <support/test-driver.c>
