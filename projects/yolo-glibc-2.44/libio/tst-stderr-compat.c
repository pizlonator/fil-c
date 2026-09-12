/* Test that fclose works on stderr from glibc 2.0.
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

#include <shlib-compat.h>

#if TEST_COMPAT (libc, GLIBC_2_0, GLIBC_2_1)
# define _LIBC
# define _IO_USE_OLD_IO_FILE
# include <stdio.h>
# include <support/check.h>

extern FILE _IO_stderr_;
compat_symbol_reference (libc, _IO_stderr_, _IO_stderr_, GLIBC_2_0);
compat_symbol_reference (libc, fclose, fclose, GLIBC_2_0);

__attribute__ ((weak, noclone, noinline))
void
do_fclose (FILE *fp)
{
  TEST_VERIFY_EXIT (fclose (fp) == 0);
}

static int
do_test (void)
{
  do_fclose (&_IO_stderr_);
  return 0;
}
#else
static int
do_test (void)
{
  return 0;
}
#endif

#include <support/test-driver.c>
