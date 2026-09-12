/* Test __GCONV_ENCOUNTERED_ILLEGAL_INPUT, as used by iconv -c (bug 32046).
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


#include <array_length.h>
#include <errno.h>
#include <gconv_int.h>
#include <iconv.h>
#include <stdbool.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <stdio.h>
#include <libc-diag.h>

/* FROM is the input character set, TO the output character set.  If
   IGNORE is true, the iconv descriptor is set up in the same way as
   iconv -c would.  INPUT is the input string, EXPECTED_OUTPUT the
   output.  OUTPUT_LIMIT is a byte count, specifying how many input
   bytes are passed to the iconv function on each invocation.  */
static void
one_direction (const char *from, const char *to, bool ignore,
               const char *input, const char *expected_output,
               size_t output_limit)
{
  if (test_verbose)
    {
      char *quoted_input = support_quote_string (input);
      char *quoted_output = support_quote_string (expected_output);
      printf ("info: testing from=\"%s\" to=\"%s\" ignore=%d input=\"%s\""
              " expected_output=\"%s\" output_limit=%zu\n",
              from, to, (int) ignore, quoted_input,
              quoted_output, output_limit);
      free (quoted_output);
      free (quoted_input);
    }

  __gconv_t cd;
  if (ignore)
    {
      struct gconv_spec conv_spec;
      TEST_VERIFY_EXIT (__gconv_create_spec (&conv_spec, from, to)
                        == &conv_spec);
      conv_spec.ignore = true;
      cd = (iconv_t) -1;
      TEST_COMPARE (__gconv_open (&conv_spec, &cd, 0), __GCONV_OK);
      __gconv_destroy_spec (&conv_spec);
    }
  else
    cd = iconv_open (to, from);
  TEST_VERIFY_EXIT (cd != (iconv_t) -1);

  char *input_ptr = (char *) input;
  size_t input_len = strlen (input);
  char output_buf[20];
  char *output_ptr = output_buf;
  size_t output_len;
  do
    {
      output_len = array_end (output_buf) - output_ptr;
      if (output_len > output_limit)
        /* Limit the buffer size as requested by the caller.  */
        output_len = output_limit;
      TEST_VERIFY_EXIT (output_len > 0);
      if (input_len == 0)
        /* Trigger final flush.  */
        input_ptr = NULL;
      char *old_input_ptr = input_ptr;
      size_t ret = iconv (cd, &input_ptr, &input_len,
                          &output_ptr, &output_len);
      if (ret == (size_t) -1)
        {
          if (errno != EILSEQ)
            TEST_COMPARE (errno, E2BIG);
        }

      if (input_ptr == old_input_ptr)
        /* Avoid endless loop if stuck on an invalid input character.  */
        break;
    }
  while (input_ptr != NULL);

  /* Test the sticky illegal input bit.  */
  TEST_VERIFY (__gconv_has_illegal_input (cd));

  TEST_COMPARE_BLOB (expected_output, strlen (expected_output),
                     output_buf, output_ptr - output_buf);

  TEST_COMPARE (iconv_close (cd), 0);
}

static int
do_test (void)
{
  static const char charsets[][14] =
    {
      "ASCII",
      "ASCII//IGNORE",
      "UTF-8",
      "UTF-8//IGNORE",
    };

  for (size_t from_idx = 0; from_idx < array_length (charsets); ++from_idx)
    for (size_t to_idx = 0; to_idx < array_length (charsets); ++to_idx)
      for (int do_ignore = 0; do_ignore < 2; ++do_ignore)
        for (int limit = 1; limit < 5; ++limit)
          for (int skip = 0; skip < 3; ++skip)
            {
              const char *expected_output;
	      DIAG_PUSH_NEEDS_COMMENT_CLANG;
	      DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wstring-plus-int");
              if (do_ignore || strstr (charsets[to_idx], "//IGNORE") != NULL)
                expected_output = "ABXY" + skip;
              else
                expected_output = "AB" + skip;
              one_direction (charsets[from_idx], charsets[to_idx], do_ignore,
                             "AB\xffXY" + skip, expected_output, limit);
	      DIAG_POP_NEEDS_COMMENT_CLANG;
            }

  return 0;
}

#include <support/test-driver.c>
