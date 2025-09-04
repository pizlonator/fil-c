/* Check _dl_debug_vdprintf.
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

#include <ldsodefs.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xunistd.h>

#define BUFSZ 64

#define TEST(fmt, ...)                                                        \
  do                                                                          \
    {                                                                         \
      char str1[BUFSZ], str2[BUFSZ];                                          \
      int len1 = snprintf (str1, BUFSZ, fmt, __VA_ARGS__);                    \
      TEST_VERIFY_EXIT (len1 >= 0);                                           \
      TEST_VERIFY_EXIT (len1 < BUFSZ);                                        \
      _dl_dprintf (fds[1], fmt, __VA_ARGS__);                                 \
      ssize_t len2 = read (fds[0], str2, BUFSZ);                              \
      TEST_VERIFY_EXIT (len2 >= 0);                                           \
      TEST_VERIFY_EXIT (len2 < BUFSZ);                                        \
      str2[len2] = '\0';                                                      \
      TEST_COMPARE_STRING (str1, str2);                                       \
    }                                                                         \
  while (0)

static int
do_test (void)
{
  int fds[2];
  xpipe (fds);
  TEST ("%d", 0);
  TEST ("%d", 1);
  TEST ("%d", INT_MAX);
  TEST ("%d", -1);
  TEST ("%d", INT_MIN + 1);
  TEST ("%d", INT_MIN);
  TEST ("%u", 0U);
  TEST ("%u", 1U);
  TEST ("%u", UINT_MAX);
  TEST ("%x", 0);
  TEST ("%x", 1);
  TEST ("%x", UINT_MAX);
  TEST ("%ld", 0L);
  TEST ("%ld", 1L);
  TEST ("%ld", LONG_MAX);
  TEST ("%ld", -1L);
  TEST ("%ld", LONG_MIN + 1);
  TEST ("%ld", LONG_MIN);
  TEST ("%lu", 0UL);
  TEST ("%lu", 1UL);
  TEST ("%lu", ULONG_MAX);
  TEST ("%lx", 0UL);
  TEST ("%lx", 1UL);
  TEST ("%lx", ULONG_MAX);
  xclose (fds[0]);
  xclose (fds[1]);
  return 0;
}

#include <support/test-driver.c>
