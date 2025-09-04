/* Basic test for the TEST_COMPARE_STRING_WIDE macro.
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

#include <string.h>
#include <support/check.h>
#include <support/capture_subprocess.h>

static void
subprocess (void *closure)
{
  /* These tests should fail.  They were chosen to cover differences
     in length (with the same contents), single-bit mismatches, and
     mismatching null pointers.  */
  TEST_COMPARE_STRING_WIDE (L"", NULL);             /* Line 29.  */
  TEST_COMPARE_STRING_WIDE (L"X", L"");              /* Line 30.  */
  TEST_COMPARE_STRING_WIDE (NULL, L"X");            /* Line 31.  */
  TEST_COMPARE_STRING_WIDE (L"abcd", L"abcD");       /* Line 32.  */
  TEST_COMPARE_STRING_WIDE (L"abcd", NULL);         /* Line 33.  */
  TEST_COMPARE_STRING_WIDE (NULL, L"abcd");         /* Line 34.  */
}

/* Same contents, different addresses.  */
wchar_t buffer_abc_1[] = L"abc";
wchar_t buffer_abc_2[] = L"abc";

static int
do_test (void)
{
  /* This should succeed.  Even if the pointers and array contents are
     different, zero-length inputs are not different.  */
  TEST_COMPARE_STRING_WIDE (NULL, NULL);
  TEST_COMPARE_STRING_WIDE (L"", L"");
  TEST_COMPARE_STRING_WIDE (buffer_abc_1, buffer_abc_2);
  TEST_COMPARE_STRING_WIDE (buffer_abc_1, L"abc");

  struct support_capture_subprocess proc = support_capture_subprocess
    (&subprocess, NULL);

  /* Discard the reported error.  */
  support_record_failure_reset ();

  puts ("info: *** subprocess output starts ***");
  fputs (proc.out.buffer, stdout);
  puts ("info: *** subprocess output ends ***");

  TEST_VERIFY
    (strcmp (proc.out.buffer,
"tst-test_compare_string_wide.c:29: error: string comparison failed\n"
"  left string: 0 wide characters\n"
"  right string: NULL\n"
"tst-test_compare_string_wide.c:30: error: string comparison failed\n"
"  left string: 1 wide characters\n"
"  right string: 0 wide characters\n"
"  left (evaluated from L\"X\"):\n"
"      L\"X\"\n"
"      58\n"
"tst-test_compare_string_wide.c:31: error: string comparison failed\n"
"  left string: NULL\n"
"  right string: 1 wide characters\n"
"  right (evaluated from L\"X\"):\n"
"      L\"X\"\n"
"      58\n"
"tst-test_compare_string_wide.c:32: error: string comparison failed\n"
"  string length: 4 wide characters\n"
"  left (evaluated from L\"abcd\"):\n"
"      L\"abcd\"\n"
"      61 62 63 64\n"
"  right (evaluated from L\"abcD\"):\n"
"      L\"abcD\"\n"
"      61 62 63 44\n"
"tst-test_compare_string_wide.c:33: error: string comparison failed\n"
"  left string: 4 wide characters\n"
"  right string: NULL\n"
"  left (evaluated from L\"abcd\"):\n"
"      L\"abcd\"\n"
"      61 62 63 64\n"
"tst-test_compare_string_wide.c:34: error: string comparison failed\n"
"  left string: NULL\n"
"  right string: 4 wide characters\n"
"  right (evaluated from L\"abcd\"):\n"
"      L\"abcd\"\n"
"      61 62 63 64\n"
             ) == 0);

  /* Check that there is no output on standard error.  */
  support_capture_subprocess_check (&proc, "TEST_COMPARE_STRING_WIDE",
                                    0, sc_allow_stdout);

  return 0;
}

#include <support/test-driver.c>
