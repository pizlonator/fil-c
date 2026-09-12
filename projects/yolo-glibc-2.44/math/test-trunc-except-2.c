/* Test trunc functions do not disable exception traps.
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

#include <fenv.h>
#include <math.h>
#include <stdio.h>

#ifndef FE_INEXACT
# define FE_INEXACT 0
#endif

#define TEST_FUNC(NAME, FLOAT, SUFFIX)					\
static int								\
NAME (void)								\
{									\
  int result = 0;							\
  volatile FLOAT a, b __attribute__ ((unused));				\
  a = 1.5;								\
  /* trunc must work when traps on "inexact" are enabled.  */	\
  b = trunc ## SUFFIX (a);						\
  /* And it must have left those traps enabled.  */			\
  if (fegetexcept () == FE_INEXACT)					\
    puts ("PASS: " #FLOAT);						\
  else									\
    {									\
      puts ("FAIL: " #FLOAT);						\
      result = 1;							\
    }									\
  return result;							\
}

TEST_FUNC (float_test, float, f)
TEST_FUNC (double_test, double, )
TEST_FUNC (ldouble_test, long double, l)

static int
do_test (void)
{
  if (feenableexcept (FE_INEXACT) == -1)
    {
      puts ("enabling FE_INEXACT traps failed, cannot test");
      return 77;
    }
  int result = float_test ();
  feenableexcept (FE_INEXACT);
  result |= double_test ();
  feenableexcept (FE_INEXACT);
  result |= ldouble_test ();
  return result;
}

#include <support/test-driver.c>
