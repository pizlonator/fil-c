/* Test that strerror variants return unique strings for each errnum.

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

#include <array_length.h>
#include <string.h>
#include <stdlib.h>

#include <support/support.h>
#include <support/check.h>

#include "tst-verify-unique-strings.c"

/* As defined by stdio-common/errlist-data-gen.c */
#include <errno.h>
#include <stdio-common/err_map.h>
#define N_(msgid)      msgid
const char *const errlist_internal[] __attribute_maybe_unused__ =
  {
#define _S(n, str)         [ERR_MAP(n)] = str,
#include <errlist.h>
#undef _S
      };
const int errlist_internal_len = array_length (errlist_internal);

static int
do_test (void)
{
  char *string[2 * errlist_internal_len + 1];

  /* Convenient indexing for error strings from -errlist_internal_len to
     errlist_internal_len.  */
  char **err_str = string + errlist_internal_len;

  unsetenv ("LANGUAGE");

  xsetlocale (LC_ALL, "C");

  for (int i = -errlist_internal_len; i <= errlist_internal_len; i++)
    {

#ifdef TEST_STRERROR_VARIANT
      /* Used for testing strerror_r and strerror_l.  */
      err_str[i] = TEST_STRERROR_VARIANT (i);
#else
      err_str[i] = xstrdup (strerror (i));
#endif

      int is_unknown_error
        = (strstr (err_str[i], "Unknown error ") == err_str[i]);
      TEST_VERIFY_EXIT ((i >= 0 && i < errlist_internal_len)
                        || is_unknown_error);
    }

  /* We check for and fail on duplicate strings.  */
  VERIFY_UNIQUE_STRINGS (string, 2 * errlist_internal_len + 1);

  for (int i = -errlist_internal_len; i <= errlist_internal_len; i++)
    free (err_str[i]);

  return 0;
}

#include <support/test-driver.c>
