/* Test program for initstate(), initstate_r() for BZ #30584.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <stdlib.h>
#include <time.h>

static int
do_test (void)
{
  struct random_data rand_state = { .state = NULL };
  _Alignas (double) char buf[128 + sizeof (int32_t)];

  /* Test initstate_r with an unaligned `state` array.  */
  initstate_r (time (NULL), buf + 1, sizeof buf, &rand_state);

  /* Ditto initstate.  */
  initstate (time (NULL), buf + 1, sizeof buf);

  return 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
