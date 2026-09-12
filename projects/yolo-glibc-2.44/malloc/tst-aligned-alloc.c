/* Test for C17 alignment requirements.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc-diag.h>
#include <support/check.h>

#include "tst-malloc-aux.h"

static int
do_test (void)
{
  void *p1;
  void *p2;
  void *p3;
  void *p4;
  void *p5;

  errno = 0;

  /* The implementation supports alignments that are non-negative powers of 2.
     We test 5 distinct conditions here:
     - A non-negative power of 2 alignment e.g. 64.
     - A degenerate zero power of 2 alignment e.g. 1.
     - A non-power-of-2 alignment e.g. 65.
     - A zero alignment.
     - A corner case SIZE_MAX / 2 + 1 alignment.
  */

  p1 = aligned_alloc (64, 64);

  if (p1 == NULL)
    FAIL_EXIT1 ("aligned_alloc(64, 64) failed");

  p2 = aligned_alloc (1, 64);

  if (p2 == NULL)
    FAIL_EXIT1 ("aligned_alloc(1, 64) failed");

  p3 = aligned_alloc (65, 64);

  if (p3 != NULL)
    FAIL_EXIT1 ("aligned_alloc(65, 64) did not fail");

  p4 = aligned_alloc (0, 64);

  if (p4 != NULL)
    FAIL_EXIT1 ("aligned_alloc(0, 64) did not fail");

  /* This is an alignment like 0x80000000...UL */
  p5 = aligned_alloc (SIZE_MAX / 2 + 1, 64);

  if (p5 != NULL)
    FAIL_EXIT1 ("aligned_alloc(SIZE_MAX/2+1, 64) did not fail");

  free (p1);
  free (p2);
  return 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
