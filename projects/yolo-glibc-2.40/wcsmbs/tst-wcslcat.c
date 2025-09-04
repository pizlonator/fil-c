/* Test the wcslcat function.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <wchar.h>

static int
do_test (void)
{
  struct {
    wchar_t buf1[16];
    wchar_t buf2[16];
  } s;

  /* Nothing is written to the destination if its size is 0.  */
  wmemset (s.buf1, '@', array_length (s.buf1));
  wmemset (s.buf2, '@', array_length (s.buf2));
  TEST_COMPARE (wcslcat (s.buf1, L"", 0), 0);
  TEST_COMPARE_BLOB (&s, sizeof (s), L"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", 128);
  TEST_COMPARE (wcslcat (s.buf1, L"Hello!", 0), 6);
  TEST_COMPARE_BLOB (&s, sizeof (s), L"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", 128);

  /* No bytes are are modified in the target buffer if the source
     string is short enough.  */
  wmemset (s.buf1, '@', array_length (s.buf1));
  wmemset (s.buf2, '@', array_length (s.buf2));
  wcscpy (s.buf1, L"He");
  TEST_COMPARE (wcslcat (s.buf1, L"llo!", array_length (s.buf1)), 6);
  TEST_COMPARE_BLOB (&s, sizeof (s), L"Hello!\0@@@@@@@@@@@@@@@@@@@@@@@@@", 128);

  /* A source string which fits exactly into the destination buffer is
     not truncated.  */
  wmemset (s.buf1, '@', array_length (s.buf1));
  wmemset (s.buf2, '@', array_length (s.buf2));
  wcscpy (s.buf1, L"H");
  TEST_COMPARE (wcslcat (s.buf1, L"ello, world!!!", array_length (s.buf1)),
                15);
  TEST_COMPARE_BLOB (&s, sizeof (s),
		     L"Hello, world!!!\0@@@@@@@@@@@@@@@@@@@@@@@@@", 128);

  /* A source string one character longer than the destination buffer
     is truncated by one character.  The total length is returned.  */
  wmemset (s.buf1, '@', array_length (s.buf1));
  wmemset (s.buf2, '@', array_length (s.buf2));
  wcscpy (s.buf1, L"Hello");
  TEST_COMPARE (wcslcat (s.buf1, L", world!!!!", array_length (s.buf1)), 16);
  TEST_COMPARE_BLOB (&s, sizeof (s),
		     L"Hello, world!!!\0@@@@@@@@@@@@@@@@@@@@@@@@@", 128);

  /* An even longer source string is truncated as well, and the total
     length is returned.  */
  wmemset (s.buf1, '@', array_length (s.buf1));
  wmemset (s.buf2, '@', array_length (s.buf2));
  wcscpy (s.buf1, L"Hello,");
  TEST_COMPARE (wcslcat (s.buf1, L" world!!!!!!!!", array_length (s.buf1)),
                20);
  TEST_COMPARE_BLOB (&s, sizeof (s),
		     L"Hello, world!!!\0@@@@@@@@@@@@@@@@@@@@@@@@@", 128);

  /* A destination string which is not NUL-terminated does not result
     in any changes to the buffer.  */
  wmemset (s.buf1, '$', array_length (s.buf1));
  wmemset (s.buf2, '@', array_length (s.buf2));
  TEST_COMPARE (wcslcat (s.buf1, L"", array_length (s.buf1)), 16);
  TEST_COMPARE_BLOB (&s, sizeof (s), L"$$$$$$$$$$$$$$$$@@@@@@@@@@@@@@@@", 128);
  TEST_COMPARE (wcslcat (s.buf1, L"Hello!", array_length (s.buf1)), 22);
  TEST_COMPARE_BLOB (&s, sizeof (s), L"$$$$$$$$$$$$$$$$@@@@@@@@@@@@@@@@", 128);
  TEST_COMPARE (wcslcat (s.buf1, L"Hello, world!!!!!!!!",
                         array_length (s.buf1)), 36);
  TEST_COMPARE_BLOB (&s, sizeof (s), L"$$$$$$$$$$$$$$$$@@@@@@@@@@@@@@@@", 128);

  return 0;
}

#include <support/test-driver.c>
