/* Verify the BIG5HKSCS outputs that generate 2 wchar_t's (Bug 25734).
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
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
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <support/check.h>
#include <support/support.h>

/* A few BIG5-HKSCS characters map in two unicode code points.
   They are:
   /x88/x62 => <U00CA><U0304>
   /x88/x64 => <U00CA><U030C>
   /x88/xa3 => <U00EA><U0304>
   /x88/xa5 => <U00EA><U030C>
   Each of these is special cased in iconvdata/big5hkscs.c.
   This test ensures that we correctly reset the shift state after
   outputting any of these characters.  We do this by converting
   each them followed by converting an ASCII character.  If we fail
   to reset the shift state (bug 25734) then we'll see the last
   character in the queue output again.  */

/* Each test has  name, input bytes, and expected wide character
   output.  */
struct testdata {
  const char *name;
  const char input[3];
  wchar_t expected[3];
};

/* In BIG5-HKSCS (2008) there are 4 characters that generate multiple
   wide characters.  */
struct testdata tests[4] = {
  /* <H-8862>X => <U+00CA><U+0304>X */
  { "<H-8862>", "\x88\x62\x58", { 0x00CA, 0x0304, 0x0058 } },
  /* <H-8864>X => <U+00CA><U+030C>X */
  { "<H-8864>", "\x88\x64\x58", { 0x00CA, 0x030C, 0x0058 } },
  /* <H-88A3>X => <U+00EA><U+0304>X */
  { "<H-88A3>", "\x88\xa3\x58", { 0x00EA, 0x0304, 0x0058 } },
  /* <H-88A5>X => <U+00EA><U+030C>X */
  { "<H-88A5>", "\x88\xa5\x58", { 0x00EA, 0x030C, 0x0058 } }
};

/* Each test is of the form:
   - Translate first code sequence (two bytes)
   - Translate second (zero bytes)
   - Translate the third (one byte).  */
static int
check_conversion (struct testdata test)
{
  int err = 0;
  wchar_t wc;
  mbstate_t st;
  size_t ret;
  const char *mbs = test.input;
  int consumed = 0;
  /* Input is always 3 bytes long.  */
  int inlen = 3;

  memset (&st, 0, sizeof (st));
  /* First conversion: Consumes first 2 bytes.  */
  ret = mbrtowc (&wc, mbs, inlen - consumed, &st);
  if (ret != 2)
    {
      printf ("error: First conversion consumed only %zd bytes.\n", ret);
      err++;
    }
  /* Advance the two consumed bytes.  */
  mbs += ret;
  consumed += ret;
  if (wc != test.expected[0])
    {
      printf ("error: Result of first conversion was wrong.\n");
      err++;
    }
  /* Second conversion: Consumes 0 bytes.  */
  ret = mbrtowc (&wc, mbs, inlen - consumed, &st);
  if (ret != 0)
    {
      printf ("error: Second conversion consumed only %zd bytes.\n", ret);
      err++;
    }
  /* Advance the zero consumed bytes.  */
  mbs += ret;
  consumed += ret;
  if (wc != test.expected[1])
    {
      printf ("error: Result of second conversion was wrong.\n");
      err++;
    }
  /* After the second conversion the state of the converter should be
     in the initial state.  It is in the initial state because the two
     input BIG5-HKSCS bytes have been consumed and the 2 wchar_t's have
     been output.  */
  if (mbsinit (&st) == 0)
    {
      printf ("error: Converter not in initial state.\n");
      err++;
    }
  /* Third conversion: Consumes 1 byte (it's an ASCII character).  */
  ret = mbrtowc (&wc, mbs, inlen - consumed, &st);
  if (ret != 1)
    {
      printf ("error: Third conversion consumed only %zd bytes.\n", ret);
      err++;
    }
  /* Advance the one byte.  */
  mbs += ret;
  consumed += ret;
  if (wc != test.expected[2])
    {
      printf ("error: Result of third conversion was wrong.\n");
      err++;
    }

  /* Now perform the same test as above consuming one byte at a time.  */
  mbs = test.input;
  memset (&st, 0, sizeof (st));

  /* Consume the first byte; expect an incomplete multibyte character.  */
  ret = mbrtowc (&wc, mbs, 1, &st);
  if (ret != -2)
    {
      printf ("error: First byte conversion returned %zd.\n", ret);
      err++;
    }
  /* Advance past the first consumed byte.  */
  mbs += 1;
  /* Consume the second byte; expect the first wchar_t.  */
  ret = mbrtowc (&wc, mbs, 1, &st);
  if (ret != 1)
    {
      printf ("error: Second byte conversion returned %zd.\n", ret);
      err++;
    }
  /* Advance past the second consumed byte.  */
  mbs += 1;
  if (wc != test.expected[0])
    {
      printf ("error: Result of first wchar_t conversion was wrong.\n");
      err++;
    }
  /* Consume no bytes; expect the second wchar_t.  */
  ret = mbrtowc (&wc, mbs, 1, &st);
  if (ret != 0)
    {
      printf ("error: First attempt of third byte conversion returned %zd.\n", ret);
      err++;
    }
  /* Do not advance past the third byte.  */
  mbs += 0;
  if (wc != test.expected[1])
    {
      printf ("error: Result of second wchar_t conversion was wrong.\n");
      err++;
    }
  /* After the second wchar_t conversion, the converter should be in
     the initial state since the two input BIG5-HKSCS bytes have been
     consumed and the two wchar_t's have been output.  */
  if (mbsinit (&st) == 0)
    {
      printf ("error: Converter not in initial state.\n");
      err++;
    }
  /* Consume the third byte; expect the third wchar_t.  */
  ret = mbrtowc (&wc, mbs, 1, &st);
  if (ret != 1)
    {
      printf ("error: Third byte conversion returned %zd.\n", ret);
      err++;
    }
  /* Advance past the third consumed byte.  */
  mbs += 1;
  if (wc != test.expected[2])
    {
      printf ("error: Result of third wchar_t conversion was wrong.\n");
      err++;
    }

  /* Return 0 if we saw no errors.  */
  return err;
}

static int
do_test (void)
{
  int err = 0;
  int ret;
  /* Testing BIG5-HKSCS.  */
  xsetlocale (LC_ALL, "zh_HK.BIG5-HKSCS");

  /* Run all the special conversions.  */
  for (int i = 0; i < (sizeof (tests) / sizeof (struct testdata)); i++)
    {
      printf ("Running test for %s\n", tests[i].name);
      ret = check_conversion (tests[i]);
      if (ret > 0)
	printf ("Test %s failed.\n", tests[i].name);
      err += ret;
    }

  /* Fail if any conversion had an error.  */
  if (err > 0)
    FAIL_EXIT1 ("One or more conversions failed.");

  return 0;
}

#include <support/test-driver.c>
