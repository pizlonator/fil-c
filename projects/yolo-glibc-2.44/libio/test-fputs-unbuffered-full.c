/* Regression test for 20632.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <unistd.h>

#ifndef WIDE
# define TEST_NAME "fputs-unbuffered-full"
# define CHAR char
# define FPUTS fputs
# define TEXT "0123456789ABCDEF"
#else
# include <wchar.h>
# define TEST_NAME "fputws-unbuffered-full"
# define CHAR wchar_t
# define FPUTS fputws
# define TEXT L"0123456789ABCDEF"
#endif /* WIDE */


static int
do_test (void)
{
  /* Open an unbuffered stream to /dev/full.  */
  FILE *fp = fopen ("/dev/full", "w");
  TEST_VERIFY_EXIT (fp != NULL);
  int ret = setvbuf (fp, NULL, _IONBF, 0);
  TEST_VERIFY_EXIT (ret == 0);

  /* Output a long string. */
  const int sz = 4096;
  CHAR *buff = calloc (sz+1, sizeof *buff);
  for (int i=0; i < sz; i++)
    buff[i] = (CHAR) 'x';
  buff[sz] = (CHAR) '\0';
  errno = 0;
  ret = FPUTS (buff, fp);
  TEST_VERIFY (ret == EOF);
  TEST_VERIFY (errno == ENOSPC);
  free (buff);

  /* Output shorter strings.   */
  for (int i=0; i < 1024; i++)
    {
      errno = 0;
      ret = FPUTS (TEXT, fp);
      TEST_VERIFY (ret == EOF);
      TEST_VERIFY (errno == ENOSPC);

      /* Call malloc, triggering a crash if its
         function pointers have been overwritten.  */
      void *volatile ptr = malloc (1);
      free (ptr);
    }
  return 0;
}

#include <support/test-driver.c>
