/* Check the usability of <dlfcn.h> functions in audit modules.
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
   <https://www.gnu.org/licenses/>.  */

#include <gnu/lib-names.h>

#include <support/check.h>
#include <support/xdlfcn.h>

static int
do_test (void)
{
  /* Check that the audit module has been loaded.  */
  void *handle = xdlopen ("mapped to libc", RTLD_LOCAL | RTLD_NOW);
  TEST_VERIFY (handle
	       == xdlopen (LIBC_SO, RTLD_LOCAL | RTLD_NOW | RTLD_NOLOAD));

  return 0;
}

#include <support/test-driver.c>
