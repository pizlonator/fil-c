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
#include "tst-audit27mod.h"

int
do_test (void)
{
  {
    float r = tst_audit27_func_float (FUNC_FLOAT_ARG0, FUNC_FLOAT_ARG1,
				      FUNC_FLOAT_ARG2, FUNC_FLOAT_ARG3,
				      FUNC_FLOAT_ARG4, FUNC_FLOAT_ARG5,
				      FUNC_FLOAT_ARG6, FUNC_FLOAT_ARG7);
    if (r != FUNC_FLOAT_RET)
      FAIL_EXIT1 ("tst_audit27_func_float() returned %a, expected %a",
		  r, FUNC_FLOAT_RET);
  }

  {
    double r = tst_audit27_func_double (FUNC_DOUBLE_ARG0, FUNC_DOUBLE_ARG1,
					FUNC_DOUBLE_ARG2, FUNC_DOUBLE_ARG3,
					FUNC_DOUBLE_ARG4, FUNC_DOUBLE_ARG5,
					FUNC_DOUBLE_ARG6, FUNC_DOUBLE_ARG7);
    if (r != FUNC_DOUBLE_RET)
      FAIL_EXIT1 ("tst_audit27_func_double() returned %la, expected %la",
		  r, FUNC_DOUBLE_RET);
  }

  {
    long double r = tst_audit27_func_ldouble (FUNC_LDOUBLE_ARG0,
					      FUNC_LDOUBLE_ARG1,
					      FUNC_LDOUBLE_ARG2,
					      FUNC_LDOUBLE_ARG3,
					      FUNC_LDOUBLE_ARG4,
					      FUNC_LDOUBLE_ARG5,
					      FUNC_LDOUBLE_ARG6,
					      FUNC_LDOUBLE_ARG7);
    if (r != FUNC_LDOUBLE_RET)
      FAIL_EXIT1 ("tst_audit27_func_ldouble() returned %La, expected %La",
		  r, FUNC_LDOUBLE_RET);
  }

  return 0;
}

#include <support/test-driver.c>
