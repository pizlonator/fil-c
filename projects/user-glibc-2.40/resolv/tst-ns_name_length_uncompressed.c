/* Test __ns_name_length_uncompressed.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <arpa/nameser.h>
#include <array_length.h>
#include <errno.h>
#include <stdio.h>
#include <support/check.h>
#include <support/next_to_fault.h>

/* Reference implementation based on other building blocks.  */
static int
reference_length (const unsigned char *p, const unsigned char *eom)
{
  unsigned char buf[NS_MAXCDNAME];
  int n = __ns_name_unpack (p, eom, p, buf, sizeof (buf));
  if (n < 0)
    return n;
  const unsigned char *q = buf;
  if (__ns_name_skip (&q, array_end (buf)) < 0)
    return -1;
  if (q - buf != n)
    /* Compressed name.  */
    return -1;
  return n;
}

static int
do_test (void)
{
  {
    unsigned char buf[] = { 3, 'w', 'w', 'w', 0, 0, 0 };
    TEST_COMPARE (reference_length (buf, array_end (buf)), sizeof (buf) - 2);
    TEST_COMPARE (__ns_name_length_uncompressed (buf, array_end (buf)),
                  sizeof (buf) - 2);
    TEST_COMPARE (reference_length (array_end (buf) - 1, array_end (buf)), 1);
    TEST_COMPARE (__ns_name_length_uncompressed (array_end (buf) - 1,
                                                 array_end (buf)), 1);
    buf[4]  = 0xc0;             /* Forward compression reference.  */
    buf[5]  = 0x06;
    TEST_COMPARE (reference_length (buf, array_end (buf)), -1);
    TEST_COMPARE (__ns_name_length_uncompressed (buf, array_end (buf)), -1);
  }

  struct support_next_to_fault ntf = support_next_to_fault_allocate (300);

  /* Buffer region with all possible bytes at start and end.  */
  for (int length = 1; length <= 300; ++length)
    {
      unsigned char *end = (unsigned char *) ntf.buffer + ntf.length;
      unsigned char *start = end - length;
      memset (start, 'X', length);
      for (int first = 0; first <= 255; ++first)
        {
          *start = first;
          for (int last = 0; last <= 255; ++last)
            {
              start[length - 1] = last;
              TEST_COMPARE (reference_length (start, end),
                            __ns_name_length_uncompressed (start, end));
            }
        }
    }

  /* Poor man's fuzz testing: patch two bytes.   */
  {
    unsigned char ref[] =
      {
        7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'n', 'e', 't', 0, 0, 0
      };
    TEST_COMPARE (reference_length (ref, array_end (ref)), 13);
    TEST_COMPARE (__ns_name_length_uncompressed (ref, array_end (ref)), 13);

    int good = 0;
    int bad = 0;
    for (int length = 1; length <= sizeof (ref); ++length)
      {
        unsigned char *end = (unsigned char *) ntf.buffer + ntf.length;
        unsigned char *start = end - length;
        memcpy (start, ref, length);

        for (int patch1_pos = 0; patch1_pos < length; ++patch1_pos)
          {
            for (int patch1_value = 0; patch1_value <= 255; ++patch1_value)
              {
                start[patch1_pos] = patch1_value;
                for (int patch2_pos = 0; patch2_pos < length; ++patch2_pos)
                  {
                    for (int patch2_value = 0; patch2_value <= 255;
                         ++patch2_value)
                      {
                        start[patch2_pos] = patch2_value;
                        int expected = reference_length (start, end);
                        errno = EINVAL;
                        int actual
                          =  __ns_name_length_uncompressed (start, end);
                        if (actual > 0)
                          ++good;
                        else
                          {
                            TEST_COMPARE (errno, EMSGSIZE);
                            ++bad;
                          }
                        TEST_COMPARE (expected, actual);
                      }
                    start[patch2_pos] = ref[patch2_pos];
                  }
              }
            start[patch1_pos] = ref[patch1_pos];
          }
      }
    printf ("info: patched inputs with success: %d\n", good);
    printf ("info: patched inputs with failure: %d\n", bad);
  }

  support_next_to_fault_free (&ntf);
  return 0;
}

#include <support/test-driver.c>
