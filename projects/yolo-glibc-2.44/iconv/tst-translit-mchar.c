/* Test multi-character transliterations.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <locale.h>
#include <iconv.h>
#include <support/support.h>
#include <support/check.h>

static int
do_test (void)
{
  iconv_t cd;
  /* An input sequence that shares a common prefix with a transliteration
     rule.  */
  char input[] = "ÄÅ";
  char *inptr = input;
  char outbuf[10];
  char *outptr = outbuf;
  size_t inlen = sizeof (input), outlen = sizeof (outbuf);
  size_t n;

  xsetlocale (LC_CTYPE, "tst-translit");

  cd = iconv_open ("ASCII//TRANSLIT", "UTF-8");
  TEST_VERIFY (cd != (iconv_t) -1);

  /* This call used to loop infinitely.  */
  n = iconv (cd, &inptr, &inlen, &outptr, &outlen);
  TEST_VERIFY (iconv_close (cd) == 0);
  return n == 0;
}

#include <support/test-driver.c>
