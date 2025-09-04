/* LD_AUDIT test for la_symbind and bind-now.
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

#include <support/check.h>
#include <support/support.h>

int tst_audit24amod1_func1 (void);
int tst_audit24amod1_func2 (void);
int tst_audit24amod2_func1 (void);

int
do_test (void)
{
  TEST_COMPARE (tst_audit24amod1_func1 (), 1);
  TEST_COMPARE (tst_audit24amod1_func2 (), 2);
  TEST_COMPARE (tst_audit24amod2_func1 (), 10);

  return 0;
}

#include <support/test-driver.c>
