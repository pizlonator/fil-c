/* Test heap buffer overflow in syslog with long __progname (CVE-2023-6246)
   Copyright (C) 2023 Free Software Foundation, Inc.
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

#include <syslog.h>
#include <string.h>

extern char * __progname;

static int
do_test (void)
{
  char long_progname[2048];

  memset (long_progname, 'X', sizeof (long_progname) - 1);
  long_progname[sizeof (long_progname) - 1] = '\0';

  __progname = long_progname;

  syslog (LOG_INFO, "Hello, World!");

  return 0;
}

#include <support/test-driver.c>
