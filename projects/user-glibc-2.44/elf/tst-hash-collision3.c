/* Test ELF symbol version hash collisions.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <shlib-compat.h>
#include <support/check.h>

extern int ref_foo1 (void);
extern int ref_foo2 (void);
extern int ref_foo3 (void);
extern int ref_foo4 (void);
extern int ref_foo5 (void);
extern int ref_bar1 (void);
extern int ref_bar2 (void);
extern int ref_bar3 (void);
extern int ref_bar4 (void);
extern int ref_bar5 (void);

symbol_version_reference (ref_foo1, foo, Hxxxyoa);
symbol_version_reference (ref_foo2, foo, HxxxypQ);
symbol_version_reference (ref_foo3, foo, HxxxyqA);
symbol_version_reference (ref_foo4, foo, HxxxzaA);
symbol_version_reference (ref_foo5, foo, Hxxxz_a);
symbol_version_reference (ref_bar1, bar, Hxxxyob);
symbol_version_reference (ref_bar2, bar, HxxxypR);
symbol_version_reference (ref_bar3, bar, HxxxyqB);
symbol_version_reference (ref_bar4, bar, HxxxzaB);
symbol_version_reference (ref_bar5, bar, Hxxxz_b);


int
do_test (void)
{
  TEST_COMPARE (ref_foo1 (), 1);
  TEST_COMPARE (ref_foo2 (), 2);
  TEST_COMPARE (ref_foo3 (), 3);
  TEST_COMPARE (ref_foo4 (), 4);
  TEST_COMPARE (ref_foo5 (), 5);
  TEST_COMPARE (ref_bar1 (), 6);
  TEST_COMPARE (ref_bar2 (), 7);
  TEST_COMPARE (ref_bar3 (), 8);
  TEST_COMPARE (ref_bar4 (), 9);
  TEST_COMPARE (ref_bar5 (), 10);
  return 0;
}

#include <support/test-driver.c>
