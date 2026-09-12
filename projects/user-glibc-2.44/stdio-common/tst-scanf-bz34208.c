/* Test scanf pushback for incomplete nan/inf inputs (BZ #34208).
   Copyright (C) 2026 The GNU Toolchain Authors.
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
#include <support/check.h>
#include <wchar.h>

#ifdef TEST_WCHAR
# define CHAR_T wchar_t
# define WINT_T wint_t
# define FSCANF fwscanf
# define FPUTC fputwc
# define FGETC fgetwc
# define EOF_VALUE WEOF
# define L_(Str) L ## Str
#else
# define CHAR_T char
# define WINT_T int
# define FSCANF fscanf
# define FPUTC fputc
# define FGETC fgetc
# define EOF_VALUE EOF
# define L_(Str) Str
#endif

#define SCAN_FORMAT L_("%e")

static const float sentinel = -123.0f;

struct test
{
  const CHAR_T *input;
  long int expected_offset;
  const CHAR_T *expected_rest;
};

static const struct test tests[] =
  {
    /* Original reproducer.  The "[" is read while looking for the
       second "n" in "nan", so it must be pushed back.  */
    { L_("+NA[..z"), 3, L_("[..z") },

    /* Mismatch while matching "nan(...)", "inf" and "infinity" must be
       pushed back.  */
    { L_("nan(@X"), 4, L_("@X") },
    { L_("iX"), 1, L_("X") },
    { L_("infiX"), 4, L_("X") },
  };

static void
do_one_test (const struct test *test)
{
  FILE *fp = tmpfile ();
  TEST_VERIFY_EXIT (fp != NULL);

  for (const CHAR_T *p = test->input; *p != '\0'; p++)
    TEST_COMPARE (FPUTC (*p, fp), (WINT_T) *p);

  TEST_COMPARE (fseek (fp, 0, SEEK_SET), 0);

  float value = sentinel;
  TEST_COMPARE (FSCANF (fp, SCAN_FORMAT, &value), 0);
  TEST_COMPARE (ftell (fp), test->expected_offset);
  TEST_VERIFY (value == sentinel);

  for (const CHAR_T *p = test->expected_rest; *p != '\0'; p++)
    TEST_COMPARE (FGETC (fp), (WINT_T) *p);

  TEST_COMPARE (FGETC (fp), EOF_VALUE);
  TEST_VERIFY (feof (fp));
  TEST_VERIFY (! ferror (fp));

  TEST_COMPARE (fclose (fp), 0);
}

static int
do_test (void)
{
  for (size_t i = 0; i < array_length (tests); i++)
    {
      printf ("info: case %zu\n", i);
      do_one_test (&tests[i]);
    }

  return 0;
}

#include <support/test-driver.c>
