/* Verify that dlopen (NULL, RTLD_LAZY) does not complete initialization.
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

/* This test mimics what the glvndSetupPthreads function in libglvnd
   does. */

#include <stdlib.h>
#include <support/check.h>

/* Defined and initialized in the shared objects.  */
extern int mod1_status;
extern int mod2_status;

static int
do_test (void)
{
  TEST_COMPARE (mod1_status, 1);
  TEST_COMPARE (mod2_status, 1);
  TEST_COMPARE_STRING (getenv ("mod2_status"), "constructed");
  return 0;
}

#include <support/test-driver.c>
