/* Attempt to trigger fault with very short TZ variable (bug 31931).
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


#include <support/next_to_fault.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static char tz[] = "TZ=/";

static int
do_test (void)
{
  struct support_next_to_fault ntf
    = support_next_to_fault_allocate (sizeof (tz));
  memcpy (ntf.buffer, tz, sizeof (tz));
  putenv (ntf.buffer);

  tzset ();

  /* Avoid dangling pointer in environ.  */
  putenv (tz);
  support_next_to_fault_free (&ntf);

  return 0;
}

#include <support/test-driver.c>
