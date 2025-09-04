/* Test code for locating libraries in root directories.
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
   <http://www.gnu.org/licenses/>.  */

#include <support/test-driver.h>
#include <support/check.h>
#include <dlfcn.h>
#include <assert.h>

static int
do_test (void)
{
  void *handle = dlopen ("libtest.so", RTLD_LAZY);
  TEST_VERIFY_EXIT (handle != NULL);
  typedef const char *(test_func_t) (void);
  test_func_t *func = dlsym (handle, "test_func");
  assert (func != NULL);
  TEST_COMPARE_STRING (func (), "Success");
  dlclose (handle);
  return 0;
}

#include <support/test-driver.c>
