/* Test for bug 33980: combining characters in IBM1390/IBM1399.
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

#include <alloc_buffer.h>
#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <string.h>

#include <support/check.h>
#include <support/next_to_fault.h>
#include <support/support.h>

/* Run iconv in a loop with a small output buffer of OUTBUFSIZE bytes
   starting at OUTBUF.  OUTBUF should be right before an unmapped page
   so that writing past the end will fault.  Skip SHIFT bytes at the
   start of the input and output, to exercise different buffer
   alignment.  TRUNCATE indicates skipped bytes at the end of
   input (0 and 1 a valid).  */
static void
test_one (const char *encoding, unsigned int shift, unsigned int truncate,
          char *outbuf, size_t outbufsize)
{
  /* In IBM1390 and IBM1399, the DBCS code 0xECB5 expands to two
     Unicode code points when translated.  */
  static char input[] =
    {
      /* 8 letters X.  */
      0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
      /* SO, 0xECB5, SI: shift to DBCS, special character, shift back.  */
      0x0e, 0xec, 0xb5, 0x0f
    };

  /* Expected output after UTF-8 conversion.  */
  static char expected[] =
    {
      'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X',
      /* U+304B (HIRAGANA LETTER KA).  */
      0xe3, 0x81, 0x8b,
      /* U+309A (COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK).  */
      0xe3, 0x82, 0x9a
    };

  iconv_t cd = iconv_open ("UTF-8", encoding);
  TEST_VERIFY_EXIT (cd != (iconv_t) -1);

  char result_storage[64];
  struct alloc_buffer result_buf
    = alloc_buffer_create (result_storage, sizeof (result_storage));

  char *inptr = &input[shift];
  size_t inleft = sizeof (input) - shift - truncate;

  while (inleft > 0)
    {
      char *outptr = outbuf;
      size_t outleft = outbufsize;
      size_t inleft_before = inleft;

      size_t ret = iconv (cd, &inptr, &inleft, &outptr, &outleft);
      size_t produced = outptr - outbuf;
      alloc_buffer_copy_bytes (&result_buf, outbuf, produced);

      if (ret == (size_t) -1 && errno == E2BIG)
        {
          if (produced == 0 && inleft == inleft_before)
            {
              /* Output buffer too small to make progress.  This is
                 expected for very small output buffer sizes.  */
              TEST_VERIFY_EXIT (outbufsize < 3);
              break;
            }
          continue;
        }
      if (ret == (size_t) -1)
        FAIL_EXIT1 ("%s (outbufsize %zu): iconv: %m", encoding, outbufsize);
      break;
    }

  /* Flush any pending state (e.g. a buffered combined character).
     With outbufsize < 3, we could not store the first character, so
     the second character did not become pending, and there is nothing
     to flush.  */
  {
    char *outptr = outbuf;
    size_t outleft = outbufsize;

    size_t ret = iconv (cd, NULL, NULL, &outptr, &outleft);
    TEST_VERIFY_EXIT (ret == 0);
    size_t produced = outptr - outbuf;
    alloc_buffer_copy_bytes (&result_buf, outbuf, produced);

    /* Second flush does not provide more data.  */
    outptr = outbuf;
    outleft = outbufsize;
    ret = iconv (cd, NULL, NULL, &outptr, &outleft);
    TEST_VERIFY_EXIT (ret == 0);
    TEST_VERIFY (outptr == outbuf);
  }

  TEST_VERIFY_EXIT (!alloc_buffer_has_failed (&result_buf));
  size_t result_used
    = sizeof (result_storage) - alloc_buffer_size (&result_buf);

  if (outbufsize >= 3)
    {
      TEST_COMPARE (inleft, 0);
      TEST_COMPARE (result_used, sizeof (expected) - shift);
      TEST_COMPARE_BLOB (result_storage, result_used,
                         &expected[shift], sizeof (expected) - shift);
    }
  else
    /* If the buffer is too small, only the leading X could be converted.  */
    TEST_COMPARE (result_used, 8 - shift);

  TEST_VERIFY_EXIT (iconv_close (cd) == 0);
}

static int
do_test (void)
{
  struct support_next_to_fault ntf
    = support_next_to_fault_allocate (8);

  for (int shift = 0; shift <= 8; ++shift)
    for (int truncate = 0; truncate < 2; ++truncate)
      for (size_t outbufsize = 1; outbufsize <= 8; outbufsize++)
        {
          char *outbuf = ntf.buffer + ntf.length - outbufsize;
          test_one ("IBM1390", shift, truncate, outbuf, outbufsize);
          test_one ("IBM1399", shift, truncate, outbuf, outbufsize);
        }

  support_next_to_fault_free (&ntf);
  return 0;
}

#include <support/test-driver.c>
