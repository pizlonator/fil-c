/* Static interposition of getenv (bug 32541).
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

#include <stdlib.h>
#include <support/check.h>

/* Some programs try to interpose getenv for their own use (not
   glibc's internal use).  Make sure that this is possible without
   introducing linker failures due to duplicate symbols.  */

char *
getenv (const char *ignored)
{
  return NULL;
}

static int
do_test (void)
{
  TEST_COMPARE_STRING (getenv ("PATH"), NULL);
  return 0;
}

#include <support/test-driver.c>
