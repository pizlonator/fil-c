/* Check that struct utmp, struct utmpx, struct lastlog use unsigned epoch.
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

/* Undefined.  Used to check that the conditions below are optimized away.  */
void link_failure_utmp (void);
void link_failure_utmpx (void);
void link_failure_lastlog (void);

static int
do_test (void)
{
  if ((struct utmp) { .ut_tv = { 0x80000000U, }, }.ut_tv.tv_sec <= 0)
    link_failure_utmp ();
  if ((struct utmpx) { .ut_tv = { 0x80000000U, }, }.ut_tv.tv_sec <= 0)
    link_failure_utmpx ();
  if ((struct lastlog) { .ll_time = 0x80000000U, }.ll_time <= 0)
    link_failure_lastlog ();
  return 0;
}

#include <support/test-driver.c>
