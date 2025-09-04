/* Tests for fortified sprintf with unknown buffer bounds (bug 30039).
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

#include <printf.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>

/* This test is not built with _FORTIFY_SOURCE.  Instead it calls the
   appropriate implementation directly.  The fortify mode is specified
   in this variable.  */
static int fortify_mode;

/* This does not handle long-double redirects etc., but we test only
   format strings that stay within the confines of the base
   implementation.  */
int __vsprintf_chk (char *s, int flag, size_t slen, const char *format,
                    va_list ap);

/* Invoke vsprintf or __vsprintf_chk according to fortify_mode.  */
static int
my_vsprintf (char *buf, const char *format, va_list ap)
{
  int result;
  if (fortify_mode == 0)
    result = vsprintf (buf, format, ap);
  else
    /* Call the fortified version with an unspecified length.  */
    result = __vsprintf_chk (buf, fortify_mode - 1, -1, format, ap);
  return result;
}

/* Run one test, with the specified expected output.  */
static void __attribute ((format (printf, 2, 3)))
do_check (const char *expected, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);

  char buf_expected[24];
  memset (buf_expected, '@', sizeof (buf_expected));
  TEST_VERIFY (strlen (expected) < sizeof (buf_expected));
  strcpy (buf_expected, expected);

  char buf[sizeof (buf_expected)];
  memset (buf, '@', sizeof (buf));

  int ret = my_vsprintf (buf, format, ap);
  TEST_COMPARE_BLOB (buf_expected, sizeof (buf_expected), buf, sizeof (buf));
  TEST_COMPARE (ret, strlen (expected));

  va_end (ap);
}

/* Run the tests in all fortify modes.  */
static void
do_tests (void)
{
  for (fortify_mode = 0; fortify_mode <= 3; ++fortify_mode)
    {
      do_check ("0", "%d", 0);
      do_check ("-2147483648", "%d", -2147483647 - 1);
      do_check ("-9223372036854775808", "%lld", -9223372036854775807LL - 1);
      do_check ("", "%s", "");
      do_check ("                      ", "%22s", "");
      do_check ("XXXXXXXXXXXXXXXXXXXXXX", "%s", "XXXXXXXXXXXXXXXXXXXXXX");
      do_check ("1.125000", "%f", 1.125);
      do_check ("1.125", "%g", 1.125);
      do_check ("1.125", "%.8g", 1.125);
    }
}

/* printf callback that falls back to the glibc-supplied
   implementation.  */
static int
dummy_printf_function (FILE *__stream,
                       const struct printf_info *__info,
                       const void *const *__args)
{
  return -2;                    /* Request fallback.  */
}

/* Likewise for the type information.  */
static int
dummy_arginfo_function (const struct printf_info *info,
                        size_t n, int *argtypes, int *size)
{
  return -1;                    /* Request fallback.  */
}

static int
do_test (void)
{
  do_tests ();

  /* Activate __printf_function_invoke mode.  */
  register_printf_specifier ('d', dummy_printf_function,
                             dummy_arginfo_function);
  register_printf_specifier ('g', dummy_printf_function,
                             dummy_arginfo_function);
  register_printf_specifier ('s', dummy_printf_function,
                             dummy_arginfo_function);

  /* Rerun the tests with callback functions.  */
  do_tests ();

  return 0;
}

#include <support/test-driver.c>
