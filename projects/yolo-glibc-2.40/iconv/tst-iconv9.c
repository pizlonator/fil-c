/* Verify that using C.UTF-8 works.

   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <iconv.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <support/support.h>
#include <support/check.h>

/* This test does two things:
   (1) Verify that we have likely included translit_combining in C.UTF-8.
   (2) Verify default_missing is '?' as expected.  */

/* ISO-8859-1 encoding of "für".  */
char iso88591_in[] = { 0x66, 0xfc, 0x72, 0x0 };
/* ASCII transliteration is "fur" with C.UTF-8 translit_combining.  */
char ascii_exp[] = { 0x66, 0x75, 0x72, 0x0 };

/* First 3-byte UTF-8 code point.  */
char utf8_in[] = { 0xe0, 0xa0, 0x80, 0x0 };
/* There is no ASCII transliteration for SAMARITAN LETTER ALAF
   so we get default_missing used which is '?'.  */
char default_missing_exp[] = { 0x3f, 0x0 };

static int
do_test (void)
{
  char ascii_out[5];
  iconv_t cd;
  char *inbuf;
  char *outbuf;
  size_t inbytes;
  size_t outbytes;
  size_t n;

  /* The C.UTF-8 locale should include translit_combining, which provides
     the transliteration for "LATIN SMALL LETTER U WITH DIAERESIS" which
     is not provided by locale/C-translit.h.in.  */
  xsetlocale (LC_ALL, "C.UTF-8");

  /* From ISO-8859-1 to ASCII.  */
  cd = iconv_open ("ASCII//TRANSLIT,IGNORE", "ISO-8859-1");
  TEST_VERIFY (cd != (iconv_t) -1);
  inbuf = iso88591_in;
  inbytes = 3;
  outbuf = ascii_out;
  outbytes = 3;
  n = iconv (cd, &inbuf, &inbytes, &outbuf, &outbytes);
  TEST_VERIFY (n != -1);
  *outbuf = '\0';
  TEST_COMPARE_BLOB (ascii_out, 3, ascii_exp, 3);
  TEST_VERIFY (iconv_close (cd) == 0);

  /* From UTF-8 to ASCII.  */
  cd = iconv_open ("ASCII//TRANSLIT,IGNORE", "UTF-8");
  TEST_VERIFY (cd != (iconv_t) -1);
  inbuf = utf8_in;
  inbytes = 3;
  outbuf = ascii_out;
  outbytes = 3;
  n = iconv (cd, &inbuf, &inbytes, &outbuf, &outbytes);
  TEST_VERIFY (n != -1);
  *outbuf = '\0';
  TEST_COMPARE_BLOB (ascii_out, 1, default_missing_exp, 1);
  TEST_VERIFY (iconv_close (cd) == 0);

  return 0;
}

#include <support/test-driver.c>
