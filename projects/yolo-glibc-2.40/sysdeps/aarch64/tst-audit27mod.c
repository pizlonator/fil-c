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
#include <stdlib.h>
#include <support/check.h>
#include "tst-audit27mod.h"

float
tst_audit27_func_float (float a0, float a1, float a2, float a3, float a4,
			float a5, float a6, float a7)
{
  if (a0 != FUNC_FLOAT_ARG0)
    FAIL_EXIT1 ("a0: %a != %a", a0, FUNC_FLOAT_ARG0);
  if (a1 != FUNC_FLOAT_ARG1)
    FAIL_EXIT1 ("a1: %a != %a", a1, FUNC_FLOAT_ARG1);
  if (a2 != FUNC_FLOAT_ARG2)
    FAIL_EXIT1 ("a2: %a != %a", a2, FUNC_FLOAT_ARG2);
  if (a3 != FUNC_FLOAT_ARG3)
    FAIL_EXIT1 ("a3: %a != %a", a3, FUNC_FLOAT_ARG3);
  if (a4 != FUNC_FLOAT_ARG4)
    FAIL_EXIT1 ("a4: %a != %a", a4, FUNC_FLOAT_ARG4);
  if (a5 != FUNC_FLOAT_ARG5)
    FAIL_EXIT1 ("a5: %a != %a", a5, FUNC_FLOAT_ARG5);
  if (a6 != FUNC_FLOAT_ARG6)
    FAIL_EXIT1 ("a6: %a != %a", a6, FUNC_FLOAT_ARG6);
  if (a7 != FUNC_FLOAT_ARG7)
    FAIL_EXIT1 ("a7: %a != %a", a7, FUNC_FLOAT_ARG7);

  return FUNC_FLOAT_RET;
}

double
tst_audit27_func_double (double a0, double a1, double a2, double a3, double a4,
			 double a5, double a6, double a7)
{
  if (a0 != FUNC_DOUBLE_ARG0)
    FAIL_EXIT1 ("a0: %la != %la", a0, FUNC_DOUBLE_ARG0);
  if (a1 != FUNC_DOUBLE_ARG1)
    FAIL_EXIT1 ("a1: %la != %la", a1, FUNC_DOUBLE_ARG1);
  if (a2 != FUNC_DOUBLE_ARG2)
    FAIL_EXIT1 ("a2: %la != %la", a2, FUNC_DOUBLE_ARG2);
  if (a3 != FUNC_DOUBLE_ARG3)
    FAIL_EXIT1 ("a3: %la != %la", a3, FUNC_DOUBLE_ARG3);
  if (a4 != FUNC_DOUBLE_ARG4)
    FAIL_EXIT1 ("a4: %la != %la", a4, FUNC_DOUBLE_ARG4);
  if (a5 != FUNC_DOUBLE_ARG5)
    FAIL_EXIT1 ("a5: %la != %la", a5, FUNC_DOUBLE_ARG5);
  if (a6 != FUNC_DOUBLE_ARG6)
    FAIL_EXIT1 ("a6: %la != %la", a6, FUNC_DOUBLE_ARG6);
  if (a7 != FUNC_DOUBLE_ARG7)
    FAIL_EXIT1 ("a7: %la != %la", a7, FUNC_DOUBLE_ARG7);

  return FUNC_DOUBLE_RET;
}

long double
tst_audit27_func_ldouble (long double a0, long double a1, long double a2,
			  long double a3, long double a4, long double a5,
			  long double a6, long double a7)
{
  if (a0 != FUNC_LDOUBLE_ARG0)
    FAIL_EXIT1 ("a0: %La != %La", a0, FUNC_LDOUBLE_ARG0);
  if (a1 != FUNC_LDOUBLE_ARG1)
    FAIL_EXIT1 ("a1: %La != %La", a1, FUNC_LDOUBLE_ARG1);
  if (a2 != FUNC_LDOUBLE_ARG2)
    FAIL_EXIT1 ("a2: %La != %La", a2, FUNC_LDOUBLE_ARG2);
  if (a3 != FUNC_LDOUBLE_ARG3)
    FAIL_EXIT1 ("a3: %La != %La", a3, FUNC_LDOUBLE_ARG3);
  if (a4 != FUNC_LDOUBLE_ARG4)
    FAIL_EXIT1 ("a4: %La != %La", a4, FUNC_LDOUBLE_ARG4);
  if (a5 != FUNC_LDOUBLE_ARG5)
    FAIL_EXIT1 ("a5: %La != %La", a5, FUNC_LDOUBLE_ARG5);
  if (a6 != FUNC_LDOUBLE_ARG6)
    FAIL_EXIT1 ("a6: %La != %La", a6, FUNC_LDOUBLE_ARG6);
  if (a7 != FUNC_LDOUBLE_ARG7)
    FAIL_EXIT1 ("a7: %La != %La", a7, FUNC_LDOUBLE_ARG7);

  return FUNC_LDOUBLE_RET;
}
