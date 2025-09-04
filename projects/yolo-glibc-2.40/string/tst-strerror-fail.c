/* Check that strerror, strerror_l do not return NULL on  failure (bug 30555).
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


#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/namespace.h>
#include <support/xdlfcn.h>

/* Interposed malloc that can be used to inject allocation failures.  */

static volatile bool fail_malloc;

void *
malloc (size_t size)
{
  if (fail_malloc)
    return NULL;

  static void *(*original_malloc) (size_t);
  if (original_malloc == NULL)
    original_malloc = xdlsym (RTLD_NEXT, "malloc");
  return original_malloc (size);
}

/* Callbacks for the actual tests.  Use fork to run both tests with a
   clean state.  */

static void
test_strerror (void *closure)
{
  fail_malloc = true;
  const char *s = strerror (999);
  fail_malloc = false;
  TEST_COMPARE_STRING (s, "Unknown error");
}

static void
test_strerror_l (void *closure)
{
  locale_t loc = newlocale (LC_ALL, "C", (locale_t) 0);
  TEST_VERIFY (loc != (locale_t) 0);
  fail_malloc = true;
  const char *s = strerror_l (999, loc);
  fail_malloc = false;
  TEST_COMPARE_STRING (s, "Unknown error");
  freelocale (loc);
}

static int
do_test (void)
{
  support_isolate_in_subprocess (test_strerror, NULL);
  support_isolate_in_subprocess (test_strerror_l, NULL);

  return 0;
}

#include <support/test-driver.c>
