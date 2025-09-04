/* Test to verify that long path is rejected by svcunix_create (bug 28768).
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <rpc/svc.h>
#include <shlib-compat.h>
#include <string.h>
#include <support/check.h>

/* svcunix_create does not have a default version in linkobj/libc.so.  */
compat_symbol_reference (libc, svcunix_create, svcunix_create, GLIBC_2_1);

static int
do_test (void)
{
  char pathname[109];
  memset (pathname, 'x', sizeof (pathname));
  pathname[sizeof (pathname) - 1] = '\0';

  errno = 0;
  TEST_VERIFY (svcunix_create (RPC_ANYSOCK, 4096, 4096, pathname) == NULL);
  TEST_COMPARE (errno, EINVAL);

  return 0;
}

#include <support/test-driver.c>
