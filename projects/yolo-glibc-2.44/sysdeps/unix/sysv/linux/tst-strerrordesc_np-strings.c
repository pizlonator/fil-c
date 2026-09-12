/* Test that strerrordesc_np returns unique strings for each errnum.

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

  int i, s;
  for (i = -errlist_internal_len, s = 0; i <= errlist_internal_len; i++)
    {
      const char *ret = strerrordesc_np (i);

      /* Range of known errors.  Some errnums could still be unused.  */
      if (i >= 0 && i < errlist_internal_len)
        {
          if (ret != NULL)
            {
              TEST_VERIFY_EXIT (strcasestr (ret, "Unknown error") == NULL);
              string[s++] = xstrdup (ret);
            }
        }
      else
        TEST_VERIFY_EXIT (ret == NULL);
    }

  /* We check for and fail on duplicate strings.  */
  VERIFY_UNIQUE_STRINGS (string, s);

  for (int i = 0; i < s; i++)
    free (string[i]);

  return 0;
}

#include <support/test-driver.c>
