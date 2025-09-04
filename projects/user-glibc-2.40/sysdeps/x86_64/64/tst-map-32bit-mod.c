/* Check that LD_PREFER_MAP_32BIT_EXEC works in shared library.
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
   <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdint.h>
#include <support/check.h>

static void
dso_do_test (void)
{
}

void
dso_check_map_32bit (void)
{
  printf ("dso_do_test: %p\n", dso_do_test);
  TEST_VERIFY ((uintptr_t) dso_do_test < 0xffffffffUL);
}
