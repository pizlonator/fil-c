/* Test that nstrftime works as required.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering.  */

#include <config.h>

/* Specification.  */
#include "strftime.h"

#include <locale.h>
#include <stdlib.h>

#include "intprops.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "macros.h"

#define FUNC nstrftime
#include "test-nstrftime.h"

int
main (void)
{
  /* Try to set the locale by implicitly looking at the LC_ALL environment
     variable.
     configure should already have checked that the locale is supported.  */
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  bool is_C_locale = STREQ (getenv ("LC_ALL"), "C");

  int fail = 0;
  fail |= posixtm_test ();
  fail |= tzalloc_test ();
  fail |= quarter_test ();
  fail |= errno_test ();
  if (is_C_locale)
    fail |= locales_test (english);
  else
    {
#if MUSL_LIBC
      if (fail == 0)
        {
          if (test_exit_status != EXIT_SUCCESS)
            return test_exit_status;
          fputs ("Skipping test: musl libc does not come with localizations\n",
                 stderr);
          return 77;
        }
#else
      fail |= locales_test (french);
#endif
    }
  return (fail ? fail : test_exit_status);
}
