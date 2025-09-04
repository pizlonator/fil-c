/* Check LD_AUDIT for aarch64 ABI specifics.
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

#include <array_length.h>
#include <string.h>
#include <support/check.h>
#include "tst-audit26mod.h"

int
do_test (void)
{
  /* Returning a large struct uses 'x8' as indirect result location.  */
  struct large_struct r = tst_audit26_func (ARG1, ARG2, ARG3);

  struct large_struct e = set_large_struct (ARG1, ARG2, ARG3);

  TEST_COMPARE_BLOB (r.a, sizeof (r.a), e.a, sizeof (e.a));

  return 0;
}

#include <support/test-driver.c>
