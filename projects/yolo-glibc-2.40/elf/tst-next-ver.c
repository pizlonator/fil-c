/* Test RTLD_DEFAULT/RTLD_NEXT when the definition has multiple versions.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <stdio.h>

#include "testobj.h"

static int
do_test (void)
{
  /* Resolve to foo@@v3 in nextmod3.so, instead of
     foo@v1 or foo@v2.  */
  int (*fp) (int) = dlsym (RTLD_DEFAULT, "foo");
  int res = fp (0);
  printf ("preload (0) = %d, %s\n", res, res == 3 ? "ok" : "wrong");
  if (res != 3)
    return 1;

  /* Resolve to foo@@v3 in nextmod3.so, instead of
     foo@v1 or foo@v2.  */
  fp = dlsym (RTLD_NEXT, "foo");
  res = fp (0);
  printf ("preload (0) = %d, %s\n", res, res == 3 ? "ok" : "wrong");
  if (res != 3)
    return 1;

  return 0;
}

#include <support/test-driver.c>
