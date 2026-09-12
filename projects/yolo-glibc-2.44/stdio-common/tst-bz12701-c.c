/* Verify scanf field width handling with the 'c' conversion (BZ #12701).
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

#include <stdio.h>
#include <string.h>

#include <libc-diag.h>
#include <support/check.h>
#include <support/next_to_fault.h>
#include <support/xstdio.h>

/* Verify various aspects of field width handling, including the data
   obtained, the number of bytes consumed, and the stream position.  */

static int
do_test (void)
{
  static const char s[43] = "The quick brown fox jumps over the lazy dog";
  struct support_next_to_fault ntfo, ntfi;
  ntfo = support_next_to_fault_allocate (sizeof (s));
  ntfi = support_next_to_fault_allocate (sizeof (s));
  char *e = ntfo.buffer + sizeof (s);
  char *b = ntfi.buffer;

  char *c;
  FILE *f;
  int n;
  int i;

  memcpy (ntfi.buffer, s, sizeof (s));

  i = 0;
  f = xfmemopen (b, sizeof (s), "r");

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  /* Avoid: "warning: zero width in gnu_scanf format [-Werror=format=]".  */
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wformat");
  TEST_VERIFY_EXIT (fscanf (f, "%0c%n", c, &n) == 1);
  DIAG_POP_NEEDS_COMMENT;
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%1c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 2;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 2);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 4;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%4c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 4);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 8;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%8c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 8);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 16;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%16c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 16);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - (sizeof (s) - i);
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%32c%n", c, &n) == EOF);
  TEST_VERIFY_EXIT (n == 16);
  TEST_VERIFY_EXIT (memcmp (c, s + i, sizeof (s) - i) == 0);

  TEST_VERIFY_EXIT (ftell (f) == sizeof (s));
  TEST_VERIFY_EXIT (feof (f) != 0);

  xfclose (f);

  i = 0;
  f = xfmemopen (b, 3, "r");

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - 2;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 2);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - (3 - i);
  TEST_VERIFY_EXIT (feof (f) == 0);
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2c%n", c, &n) == EOF);
  TEST_VERIFY_EXIT (n == 2);

  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (feof (f) != 0);

  xfclose (f);

  i = 0;
  f = xfmemopen (b, 3, "r");

  c = e - 2;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2c%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 2);
  TEST_VERIFY_EXIT (memcmp (c, s + i, n) == 0);
  i += n;

  c = e - (3 - i);
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2c%n", c, &n) == EOF);
  TEST_VERIFY_EXIT (n == 2);
  TEST_VERIFY_EXIT (memcmp (c, s + i, 3 - i) == 0);

  TEST_VERIFY_EXIT (ftell (f) == 3);
  TEST_VERIFY_EXIT (feof (f) != 0);

  xfclose (f);

  support_next_to_fault_free (&ntfi);
  support_next_to_fault_free (&ntfo);

  return 0;
}

#include <support/test-driver.c>
