/* Test that XSI strerror_r returns unique strings for each errnum.

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

extern int
xsi_strerror_r (int errnum, char *buf, size_t buflen);

/* Wrap strerror_r into a checked variant that can be plugged into the
   equivalent strerror test.  */
static char *
test_and_return_xsi_strerror_r (int errnum)
{
  char buf[1024];

  int ret = xsi_strerror_r (errnum, buf, sizeof (buf));

  /* Unknown errnums lead to a positive error returned from strerror_r.  */
  if (strstr (buf, "Unknown error ") == buf)
    TEST_VERIFY (ret > 0);
  else
    TEST_VERIFY (ret == 0);

  return xstrdup (buf);
}

#define TEST_STRERROR_VARIANT test_and_return_xsi_strerror_r
#include "tst-strerror-strings.c"
