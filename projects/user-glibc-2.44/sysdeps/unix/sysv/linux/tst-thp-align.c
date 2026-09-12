/* Test the THP compatible alignment of PT_LOAD segments.

   Copyright (C) 2026 Free Software Foundation, Inc.

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

#include <support/xdlfcn.h>
#include "tst-thp-align-check.h"

#define THP_SIZE_MOD_NAME "tst-thp-size-mod.so"

static int
do_test (void)
{
  void *dl;

  dl = xdlopen (THP_SIZE_MOD_NAME, RTLD_NOW);
  check_align (THP_SIZE_MOD_NAME);
  xdlclose (dl);

  return 0;
}

#include <support/test-driver.c>
