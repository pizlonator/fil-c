/* Check if static-TLS variables are correctly intialized in IFUNC resolvers.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <support/check.h>

#define MARKER   0x32125A5Au

extern unsigned int (*fptr) (void);
extern unsigned int get_counter (void);

static int
do_test (void)
{
  TEST_VERIFY (fptr != NULL);
  TEST_COMPARE (get_counter (), MARKER);
  return 0;
}

#include <support/test-driver.c>
