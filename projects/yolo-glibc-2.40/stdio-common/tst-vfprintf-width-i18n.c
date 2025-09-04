/* Test for width of non-ASCII digit sequences.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

/* Behavior is currently inconsistent between %d and %f (bug 28943,
   bug 28944).  This test intends to capture the status quo.  */

#include <monetary.h>
#include <stdio.h>
#include <support/support.h>
#include <support/check.h>

static int
do_test (void)
{
  char buf[40];

  xsetlocale (LC_ALL, "hi_IN.UTF-8");

  /* Ungrouped, not translated.  */
  TEST_COMPARE (sprintf (buf, "%7d", 12345), 7);
  TEST_COMPARE_STRING (buf, "  12345");
  TEST_COMPARE (sprintf (buf, "%10.2f", 12345.67), 10);
  TEST_COMPARE_STRING (buf, "  12345.67");
  TEST_COMPARE (strfmon (buf, sizeof (buf), "%^13i", 12345.67), 13);
  TEST_COMPARE_STRING (buf, "  INR12345.67");

  /* Grouped.  */
  TEST_COMPARE (sprintf (buf, "%'8d", 12345), 8);
  TEST_COMPARE_STRING (buf, "  12,345");
  TEST_COMPARE (sprintf (buf, "%'11.2f", 12345.67), 11);
  TEST_COMPARE_STRING (buf, "  12,345.67");
  TEST_COMPARE (strfmon (buf, sizeof (buf), "%13i", 12345.67), 13);
  TEST_COMPARE_STRING (buf, " INR12,345.67");

  /* Translated.  */
  TEST_COMPARE (sprintf (buf, "%I16d", 12345), 16);
  TEST_COMPARE_STRING (buf, " १२३४५");
  TEST_COMPARE (sprintf (buf, "%I12.2f", 12345.67), 26);
  TEST_COMPARE_STRING (buf, "    १२३४५.६७");

  /* Translated and grouped.  */
  TEST_COMPARE (sprintf (buf, "%'I17d", 12345), 17);
  TEST_COMPARE_STRING (buf, " १२,३४५");
  TEST_COMPARE (sprintf (buf, "%'I12.2f", 12345.67), 26);
  TEST_COMPARE_STRING (buf, "   १२,३४५.६७");

  xsetlocale (LC_ALL, "ps_AF.UTF-8");

  /* Ungrouped, not translated.  */
  TEST_COMPARE (sprintf (buf, "%7d", 12345), 7);
  TEST_COMPARE_STRING (buf, "  12345");
  TEST_COMPARE (sprintf (buf, "%10.2f", 12345.67), 11);
  TEST_COMPARE_STRING (buf, "  12345٫67");
  TEST_COMPARE (strfmon (buf, sizeof (buf), "%^13i", 12345.67), 13);
  TEST_COMPARE_STRING (buf, "    12346 AFN");

  /* Grouped.  */
  TEST_COMPARE (sprintf (buf, "%'8d", 12345), 8);
  TEST_COMPARE_STRING (buf, " 12٬345");
  TEST_COMPARE (sprintf (buf, "%'11.2f", 12345.67), 13);
  TEST_COMPARE_STRING (buf, "  12٬345٫67"); /* Counts characters.  */
  TEST_COMPARE (strfmon (buf, sizeof (buf), "%13i", 12345.67), 13);
  TEST_COMPARE_STRING (buf, "  12٬346 AFN"); /* Counts bytes.   */

  /* Translated.  */
  TEST_COMPARE (sprintf (buf, "%I11d", 12345), 11);
  TEST_COMPARE_STRING (buf, " ١٢٣۴٥");
  TEST_COMPARE (sprintf (buf, "%I12.2f", 12345.67), 20);
  TEST_COMPARE_STRING (buf, "    ١٢٣۴٥٫٦٧");

  /* Translated and grouped.  */
  TEST_COMPARE (sprintf (buf, "%'I13d", 12345), 13);
  TEST_COMPARE_STRING (buf, " ١٢٬٣۴٥");
  TEST_COMPARE (sprintf (buf, "%'I12.2f", 12345.67), 21);
  TEST_COMPARE_STRING (buf, "   ١٢٬٣۴٥٫٦٧");

  return 0;
}

#include <support/test-driver.c>
