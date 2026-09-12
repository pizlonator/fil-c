/* Verify ISO-2022-CN-EXT does not write out of the bounds.
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

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <iconv.h>
#include <sys/mman.h>

#include <support/xunistd.h>
#include <support/check.h>
#include <support/support.h>

/* The test sets up a two memory page buffer with the second page marked
   PROT_NONE to trigger a fault if the conversion writes beyond the exact
   expected amount.  Then we carry out various conversions and precisely
   place the start of the output buffer in order to trigger a SIGSEGV if the
   process writes anywhere between 1 and page sized bytes more (only one
   PROT_NONE page is setup as a canary) than expected.  These tests exercise
   all three of the cases in ISO-2022-CN-EXT where the converter must switch
   character sets and may run out of buffer space while doing the
   operation.  */

static int
do_test (void)
{
  iconv_t cd = iconv_open ("ISO-2022-CN-EXT", "UTF-8");
  TEST_VERIFY_EXIT (cd != (iconv_t) -1);

  char *ntf;
  size_t ntfsize;
  char *outbufbase;
  {
    int pgz = getpagesize ();
    TEST_VERIFY_EXIT (pgz > 0);
    ntfsize = 2 * pgz;

    ntf = xmmap (NULL, ntfsize, PROT_READ | PROT_WRITE, MAP_PRIVATE
		 | MAP_ANONYMOUS, -1);
    xmprotect (ntf + pgz, pgz, PROT_NONE);

    outbufbase = ntf + pgz;
  }

  /* Check if SOdesignation escape sequence does not trigger an OOB write.  */
  {
    char inbuf[] = "\xe4\xba\xa4\xe6\x8d\xa2";

    for (int i = 0; i < 9; i++)
      {
	char *inp = inbuf;
	size_t inleft = sizeof (inbuf) - 1;

	char *outp = outbufbase - i;
	size_t outleft = i;

	TEST_VERIFY_EXIT (iconv (cd, &inp, &inleft, &outp, &outleft)
			  == (size_t) -1);
	TEST_COMPARE (errno, E2BIG);

	TEST_VERIFY_EXIT (iconv (cd, NULL, NULL, NULL, NULL) == 0);
      }
  }

  /* Same as before for SS2designation.  */
  {
    char inbuf[] = "㴽 \xe3\xb4\xbd";

    for (int i = 0; i < 14; i++)
      {
	char *inp = inbuf;
	size_t inleft = sizeof (inbuf) - 1;

	char *outp = outbufbase - i;
	size_t outleft = i;

	TEST_VERIFY_EXIT (iconv (cd, &inp, &inleft, &outp, &outleft)
			  == (size_t) -1);
	TEST_COMPARE (errno, E2BIG);

	TEST_VERIFY_EXIT (iconv (cd, NULL, NULL, NULL, NULL) == 0);
      }
  }

  /* Same as before for SS3designation.  */
  {
    char inbuf[] = "劄 \xe5\x8a\x84";

    for (int i = 0; i < 14; i++)
      {
	char *inp = inbuf;
	size_t inleft = sizeof (inbuf) - 1;

	char *outp = outbufbase - i;
	size_t outleft = i;

	TEST_VERIFY_EXIT (iconv (cd, &inp, &inleft, &outp, &outleft)
			  == (size_t) -1);
	TEST_COMPARE (errno, E2BIG);

	TEST_VERIFY_EXIT (iconv (cd, NULL, NULL, NULL, NULL) == 0);
      }
  }

  TEST_VERIFY_EXIT (iconv_close (cd) != -1);

  xmunmap (ntf, ntfsize);

  return 0;
}

#include <support/test-driver.c>
