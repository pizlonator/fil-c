/* Check expected sizes of struct utmp, struct utmpx, struct lastlog.
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

#include <utmp.h>
#include <utmpx.h>
#include <utmp-size.h>

static int
do_test (void)
{
  _Static_assert (sizeof (struct utmp) == UTMP_SIZE, "struct utmp size");
  _Static_assert (sizeof (struct utmpx) == UTMP_SIZE, "struct utmpx size");
  _Static_assert (sizeof (struct lastlog) == LASTLOG_SIZE,
                  "struct lastlog size");
  return 0;
}

#include <support/test-driver.c>
