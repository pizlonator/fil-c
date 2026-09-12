/* Test setvbuf on open_memstream, BZ #34019.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <libio/tst-memstream.h>

static int
do_test (void)
{
  /* Regression test for setvbuf doallocate on open_memstream and
     open_wmemstream.  This test covers the _IO_setvbuf path for:

     setvbuf (stream, NULL, _IOFBF, 0)

     This path may call _IO_DOALLOCATE and return without invoking
     the stream setbuf hook.  The generic doallocate hook must not
     replace the growable result buffer.  */

  CHAR_T *buf = NULL;
  size_t len = 0;
  FILE *fp = OPEN_MEMSTREAM (&buf, &len);

  TEST_VERIFY_EXIT (fp != NULL);

  TEST_COMPARE (setvbuf (fp, NULL, _IOFBF, 0), 0);

  TEST_COMPARE (FPUTC (W('A'), fp), W('A'));
  TEST_COMPARE (fflush (fp), 0);
  TEST_COMPARE (len, 1);
  TEST_VERIFY_EXIT (buf != NULL);
  TEST_VERIFY (buf[0] == W('A'));
  TEST_VERIFY (buf[1] == W('\0'));

  TEST_COMPARE (FPUTC (W('B'), fp), W('B'));
  TEST_COMPARE (fclose (fp), 0);
  TEST_COMPARE (len, 2);
  TEST_VERIFY_EXIT (buf != NULL);
  TEST_VERIFY (buf[0] == W('A'));
  TEST_VERIFY (buf[1] == W('B'));
  TEST_VERIFY (buf[2] == W('\0'));

  free (buf);

  return 0;
}

#include <support/test-driver.c>
