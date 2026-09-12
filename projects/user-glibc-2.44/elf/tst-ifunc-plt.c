/* Test that IRELATIVE resolvers may call PLT functions during startup.
   Copyright (C) 2026 Free Software Foundation, Inc.
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


/* tst-ifunc-plt-multi-lib.so defines two static IFUNCs (compute_a and
   compute_b), each producing an R_*_IRELATIVE in .rel{a}.dyn, with both
   resolvers calling get_value() via PLT.  The test verifies that both
   IRELATIVEs are deferred until after .rel{a}.plt is processed.  */

#include <support/check.h>

extern int compute_a (void);
extern int compute_b (void);

static int
do_test (void)
{
  TEST_COMPARE (compute_a (), 1);
  TEST_COMPARE (compute_b (), 2);
  return 0;
}

#include <support/test-driver.c>
