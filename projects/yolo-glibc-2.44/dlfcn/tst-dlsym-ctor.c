/* Test that a tail-called dlsym from a constructor works.
   Copyright The GNU Toolchain Authors.
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
#include <support/check.h>
#include <support/xdlfcn.h>

static int
do_test (void)
{
  /* Loading the module runs its constructor, which performs a dlsym
     whose result is discarded.  Under optimization the compiler lowers
     that to a tail call, so dlsym sees a caller address inside the
     dynamic linker itself.  Before the fix that resolved to the ld.so
     link map, which has no l_scope, and the lookup crashed.  */
  void *h = xdlopen ("tst-dlsym-ctormod.so", RTLD_NOW);
  xdlclose (h);
  return 0;
}

#include <support/test-driver.c>
