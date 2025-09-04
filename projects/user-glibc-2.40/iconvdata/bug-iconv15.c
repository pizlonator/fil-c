/* Bug 28524: Conversion from ISO-2022-JP-3 with iconv
   may emit spurious NUL character on state reset.
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

#include <stddef.h>
#include <iconv.h>
#include <support/check.h>

static int
do_test (void)
{
  char in[] = "\x1b(I";
  char *inbuf = in;
  size_t inleft = sizeof (in) - 1;
  char out[1];
  char *outbuf = out;
  size_t outleft = sizeof (out);
  iconv_t cd;

  cd = iconv_open ("UTF8", "ISO-2022-JP-3");
  TEST_VERIFY_EXIT (cd != (iconv_t) -1);

  /* First call to iconv should alter internal state.
     Now, JISX0201_Kana_set is selected and
     state value != ASCII_set.  */
  TEST_VERIFY (iconv (cd, &inbuf, &inleft, &outbuf, &outleft) != (size_t) -1);

  /* No bytes should have been added to
     the output buffer at this point.  */
  TEST_VERIFY (outbuf == out);
  TEST_VERIFY (outleft == sizeof (out));

  /* Second call shall emit spurious NUL character in unpatched glibc.  */
  TEST_VERIFY (iconv (cd, NULL, NULL, &outbuf, &outleft) != (size_t) -1);

  /* No characters are expected to be produced.  */
  TEST_VERIFY (outbuf == out);
  TEST_VERIFY (outleft == sizeof (out));

  TEST_VERIFY_EXIT (iconv_close (cd) != -1);

  return 0;
}

#include <support/test-driver.c>
