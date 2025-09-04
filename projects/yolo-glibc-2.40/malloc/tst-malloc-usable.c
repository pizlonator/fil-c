/* Ensure that malloc_usable_size returns the request size with
   MALLOC_CHECK_ exported to a positive value.

   Copyright (C) 2012-2024 Free Software Foundation, Inc.
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

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <support/support.h>
#include <support/check.h>

static int
do_test (void)
{
  size_t usable_size;
  void *p = malloc (7);

  TEST_VERIFY_EXIT (p != NULL);
  usable_size = malloc_usable_size (p);
  TEST_COMPARE (usable_size, 7);
  memset (p, 0, usable_size);
  free (p);

  TEST_COMPARE (malloc_usable_size (NULL), 0);

  return 0;
}

#include "support/test-driver.c"
