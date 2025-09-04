/* Test PLT rewrite with 32-bit displacement overflow.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

extern int (*func_p) (void);
extern void foo (void);

int
func (void)
{
  return 0xbadbeef;
}

void
bar (void)
{
  TEST_VERIFY (func_p == &func);
  TEST_VERIFY (func_p () == 0xbadbeef);
}

static int
do_test (void)
{
  func_p = &func;
  foo ();
  bar ();
  return 0;
}

#include <support/test-driver.c>
