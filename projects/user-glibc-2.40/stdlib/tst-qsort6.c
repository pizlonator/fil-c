/* Test qsort with invalid comparison functions.
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

#include <array_length.h>
#include <stdlib.h>
#include <support/check.h>

/* Invalid comparison function that always returns -1.  */
static int
invalid_compare_1 (const void *a1, const void *b1)
{
  const int *a = a1;
  const int *b = b1;
  /* Check that the marker value matches, which means that we are
     likely within the array.  */
  TEST_COMPARE (*a, 842523635);
  TEST_COMPARE (*b, 842523635);
  TEST_VERIFY_EXIT (*a == 842523635);
  TEST_VERIFY_EXIT (*b == 842523635);
  return -1;
}

/* Invalid comparison function that always returns 1.  */
static int
invalid_compare_2 (const void *a1, const void *b1)
{
  const int *a = a1;
  const int *b = b1;
  TEST_COMPARE (*a, 842523635);
  TEST_COMPARE (*b, 842523635);
  TEST_VERIFY_EXIT (*a == 842523635);
  TEST_VERIFY_EXIT (*b == 842523635);
  return 1;
}

static int
do_test (void)
{
  int array[] = {842523635, 842523635, 842523635, 842523635, 842523635};
  qsort (array, array_length (array), sizeof (array[0]), invalid_compare_1);
  qsort (array, array_length (array), sizeof (array[0]), invalid_compare_2);
  return 0;
}

#include <support/test-driver.c>
