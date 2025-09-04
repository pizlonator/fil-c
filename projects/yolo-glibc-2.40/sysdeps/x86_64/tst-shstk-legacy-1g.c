/* Check that legacy shadow stack code in init_array will trigger
   segfault.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include <sys/platform/x86.h>
#include <support/test-driver.h>
#include <support/xsignal.h>

static int
do_test (void)
{
  if (!CPU_FEATURE_ACTIVE (SHSTK))
    return EXIT_UNSUPPORTED;

  return EXIT_FAILURE;
}

#define EXPECTED_SIGNAL (CPU_FEATURE_ACTIVE (SHSTK) ? SIGSEGV : 0)
#include <support/test-driver.c>
