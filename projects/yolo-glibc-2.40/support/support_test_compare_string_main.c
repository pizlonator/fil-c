/* Check two strings for equality.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xmemstream.h>

static void
report_length (const char *what, const CHAR *str, size_t length)
{
  if (str == NULL)
    printf ("  %s string: NULL\n", what);
  else
    printf ("  %s string: %zu %s\n", what, length,
	    WIDE ? "wide characters" : "bytes");
}

static void
report_string (const char *what, const UCHAR *blob,
               size_t length, const char *expr)
{
  if (length > 0)
    {
      printf ("  %s (evaluated from %s):\n", what, expr);
      char *quoted = SUPPORT_QUOTE_BLOB (blob, length);
      printf ("      %s\"%s\"\n", LPREFIX, quoted);
      free (quoted);

      fputs ("     ", stdout);
      for (size_t i = 0; i < length; ++i)
        printf (" %02X", (unsigned int) blob[i]);
      putc ('\n', stdout);
    }
}

static size_t
string_length_or_zero (const CHAR *str)
{
  if (str == NULL)
    return 0;
  else
    return STRLEN (str);
}

void
SUPPORT_TEST_COMPARE_STRING (const CHAR *left, const CHAR *right,
                             const char *file, int line,
                             const char *left_expr, const char *right_expr)
{
  /* Two null pointers are accepted.  */
  if (left == NULL && right == NULL)
    return;

  size_t left_length = string_length_or_zero (left);
  size_t right_length = string_length_or_zero (right);

  if (left_length != right_length || left == NULL || right == NULL
      || MEMCMP (left, right, left_length) != 0)
    {
      support_record_failure ();
      printf ("%s:%d: error: string comparison failed\n", file, line);
      if (left_length == right_length && right != NULL && left != NULL)
        printf ("  string length: %zu %s\n", left_length,
		WIDE ? "wide characters" : "bytes");
      else
        {
          report_length ("left", left, left_length);
          report_length ("right", right, right_length);
        }
      report_string ("left", (const UCHAR *) left,
                     left_length, left_expr);
      report_string ("right", (const UCHAR *) right,
                     right_length, right_expr);
    }
}
