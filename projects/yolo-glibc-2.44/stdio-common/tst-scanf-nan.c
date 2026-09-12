/* Test scanf formats for nan, nan(), nan(n-char-sequence) types.
   Copyright The GNU Toolchain Authors.
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

#include <stdint.h>
#include <stdio.h>

#include <support/check.h>

#define CHECK_SCANF_RET(OK, STR, FMT, ...)                                    \
  do                                                                          \
    {                                                                         \
      int ret = sscanf (STR, FMT, __VA_ARGS__);                               \
      TEST_VERIFY (ret == (OK));                                              \
    }                                                                         \
  while (0)

/* Valid nan types:
   1. nan
   2. nan()
   3. nan([a-zA-Z0-9_]+)
   Any other nan format is invalid and should produce a conversion error.
   The return value denotes the number of valid conversions.  On conversion
   error the rest of the input is discarded.  */
static int
do_test (void)
{
  int a;
  float b;
  double c;
  long double d;

  /* All valid inputs.  */
  CHECK_SCANF_RET (1, "nan", "%lf", &c);
  CHECK_SCANF_RET (1, "nan()", "%lf", &c);
  CHECK_SCANF_RET (1, "nan(12345)", "%lf", &c);
  CHECK_SCANF_RET (2, "nan12", "%lf%d", &c, &a);
  CHECK_SCANF_RET (2, "nan nan()", "%f%Lf", &b, &d);
  CHECK_SCANF_RET (2, "nan nan(12345foo)", "%lf%Lf", &c, &d);
  CHECK_SCANF_RET (3, "nan nan() 12.234", "%lf%Lf%f", &c, &d, &b);
  CHECK_SCANF_RET (4, "nannan()nan(foo)1234", "%lf%f%Lf%d", &c, &b, &d, &a);

  /* Partially valid inputs.  */
  CHECK_SCANF_RET (1, "nan( )", "%3lf", &c);
  CHECK_SCANF_RET (1, "nan nan(", "%lf%f", &c, &b);

  /* Invalid inputs.  */

  /* Dangling parentheses.  */
  CHECK_SCANF_RET (0, "nan(", "%lf", &c);
  CHECK_SCANF_RET (0, "nan(123", "%lf", &c);
  CHECK_SCANF_RET (0, "nan(12345", "%lf%d", &c, &a);

  /* Field width is not sufficient for valid conversion.  */
  CHECK_SCANF_RET (0, "nan()", "%4Lf", &d);
  CHECK_SCANF_RET (0, "nan(1", "%5lf", &c);

  /* Space is not a valid character.  */
  CHECK_SCANF_RET (0, "nan( )", "%lf", &c);
  CHECK_SCANF_RET (0, "nan( )12.34", "%Lf%f", &d, &b);
  CHECK_SCANF_RET (0, "nan(12 foo)", "%f", &b);

  /* Period '.' is not a valid character.  */
  CHECK_SCANF_RET (0, "nan(12.34) nan(FooBar)", "%lf%Lf", &c, &d);

  return 0;
}

#include <support/test-driver.c>
