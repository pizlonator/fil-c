/* Test the heapsort implementation behind qsort.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include "qsort.c"

#include <stdio.h>
#include <support/check.h>
#include <support/support.h>

static int
cmp (const void *a1, const void *b1, void *closure)
{
  const signed char *a = a1;
  const signed char *b = b1;
  return *a - *b;
}

static void
check_one_sort (signed char *array, int length)
{
  signed char *copy = xmalloc (length);
  memcpy (copy, array, length);
  heapsort_r (copy, length - 1, 1, cmp, NULL);

  /* Verify that the result is sorted.  */
  for (int i = 1; i < length; ++i)
    if (copy[i] < copy[i - 1])
      {
        support_record_failure ();
        printf ("error: sorting failure for length %d at offset %d\n",
                length, i - 1);
        printf ("input:");
        for (int i = 0; i < length; ++i)
          printf (" %d", array[i]);
        printf ("\noutput:");
        for (int i = 0; i < length; ++i)
          printf (" %d", copy[i]);
        putchar ('\n');
        break;
      }

  /* Verify that no elements went away or were added.  */
  {
    int expected_counts[256];
    for (int i = 0; i < length; ++i)
      ++expected_counts[array[i] & 0xff];
    int actual_counts[256];
    for (int i = 0; i < length; ++i)
      ++actual_counts[copy[i] & 0xff];
    for (int i = 0; i < 256; ++i)
      TEST_COMPARE (expected_counts[i], expected_counts[i]);
  }

  free (copy);
}

/* Enumerate all possible combinations of LENGTH elements.  */
static void
check_combinations (int length, signed char *start, int offset)
{
  if (offset == length)
    check_one_sort (start, length);
  else
    for (int i = 0; i < length; ++i)
      {
        start[offset] = i;
        check_combinations(length, start, offset + 1);
      }
}

static int
do_test (void)
{
  /* A random permutation of 20 values.  */
  check_one_sort ((signed char[20]) {5, 12, 16, 10, 14, 11, 9, 13, 8, 15,
                                     0, 17, 3, 7, 1, 18, 2, 19, 4, 6}, 20);


  /* A permutation that appeared during adversarial testing for the
     quicksort pass.  */
  check_one_sort ((signed char[16]) {15, 3, 4, 2, 1, 0, 8, 7, 6, 5, 14,
                                     13, 12, 11, 10, 9}, 16);

  for (int i = 2; i <= 8; ++i)
    {
      signed char *buf = xmalloc (i);
      check_combinations (i, buf, 0);
      free (buf);
    }

  return 0;
}

#include <support/test-driver.c>
