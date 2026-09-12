/* Regression test for ungetwc operating on byte stream (BZ #33998)
   Copyright The GNU Toolchain Authors.
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

#include "support/temp_file.h"
#include "support/xstdio.h"
#include "support/xunistd.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <wchar.h>
#include <support/check.h>

static int
do_test (void)
{
  char *filename;
  int fd = create_temp_file ("tst-bz33998-", &filename);
  TEST_VERIFY (fd != -1);
  xwrite (fd, "A", sizeof ("A")); // write "A\0" by design
  xclose (fd);

  FILE *fp = xfopen (filename, "r+");
  TEST_COMPARE (getwc (fp), L'A');
  /* If the bug is fixed, then ungetwc should not touch byte stream.
     If the bug is not fixed, ungetwc firstly match last read char, L'A',
     failed, then the pbackfail branch, matching last read char in byte
     stream, that is, '\0' (initialized when setup wide stream). */
  char *old_read_ptr = fp->_IO_read_ptr;
  TEST_COMPARE (ungetwc (L'\0', fp), L'\0');
  TEST_VERIFY (fp->_IO_read_ptr == old_read_ptr);

  xfclose (fp);
  free (filename);

  return 0;
}

#include <support/test-driver.c>
