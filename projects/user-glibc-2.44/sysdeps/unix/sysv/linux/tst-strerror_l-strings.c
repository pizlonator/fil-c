/* Test that strerror_l returns unique strings for each errnum.

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <string.h>
#include <support/support.h>
#include <support/check.h>

/* newlocale returns (locale_t) 0 upon error, so it makes for a good initial
   value that is different from any valid locale_t.  */
static locale_t loc = (locale_t) 0;

/* Wrap strerror_l to be plugged into the equivalent strerror test.  */
static char *
wrap_strerror_l (int errnum)
{
  if (loc == (locale_t) 0)
    loc = xnewlocale (LC_ALL_MASK, "C", (locale_t) 0);

  return xstrdup (strerror_l (errnum, loc));
}

#define TEST_STRERROR_VARIANT wrap_strerror_l
#include "tst-strerror-strings.c"
