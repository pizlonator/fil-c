/* Test that GNU strerror_r returns unique strings for each errnum.

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

#include <string.h>
#include <support/support.h>
#include <support/check.h>

/* Wrap strerror_r into a checked variant that can be plugged into the
   equivalent strerror test.  */
static char *
test_and_return_strerror_r (int errnum)
{
  char buf[1024];

  char *ret = strerror_r (errnum, buf, sizeof (buf));

  /* User supplied buffer used for and only for "Unknown error" strings.  */
  if (strstr (ret, "Unknown error ") == ret)
    TEST_VERIFY_EXIT (ret == buf);
  else
    TEST_VERIFY_EXIT (ret != buf);

  return xstrdup (ret);
}

#define TEST_STRERROR_VARIANT test_and_return_strerror_r
#include "tst-strerror-strings.c"
