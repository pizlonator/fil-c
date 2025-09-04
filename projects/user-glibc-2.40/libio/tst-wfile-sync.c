/* Test that _IO_wfile_sync does not crash (bug 20568).
   Copyright (C) 2019-2024 Free Software Foundation, Inc.
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

#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xunistd.h>
#include <support/temp_file.h>

static const char test_data[] = "This is a test of _IO_wfile_sync.";

static int
do_test (void)
{
  static char *infile;
  int infd;
  FILE *infp;

  infd = create_temp_file ("tst-wfile-sync-in-", &infile);
  xwrite (infd, test_data, strlen (test_data));
  xclose (infd);

  infd = xopen (infile, O_RDONLY, 0);
  infp = fdopen (infd, "r");

  TEST_VERIFY_EXIT (setlocale (LC_ALL, "de_DE.UTF-8") != NULL);
  /* Fill the stdio buffer and advance the read pointer.  */
  TEST_VERIFY_EXIT (fgetwc (infp) != WEOF);
  /* This calls _IO_wfile_sync, it should not crash.  */
  TEST_VERIFY_EXIT (setvbuf (infp, NULL, _IONBF, 0) == 0);
  /* Verify that the external file offset has been synchronized.  */
  TEST_COMPARE (xlseek (infd, 0, SEEK_CUR), 1);

  fclose (infp);
  free (infile);

  return 0;
}

#include <support/test-driver.c>
