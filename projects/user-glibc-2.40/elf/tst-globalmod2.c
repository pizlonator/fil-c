/* Test BZ #28455.
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

/* In glibc configured with --enable-hardcoded-path-in-tests, a test
   program built with -Wl,--enable-new-dtags, which adds DT_RUNPATH,
   instead of DT_RPATH, can call a function in a shared library, which
   dlopens another shared library.  */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

extern int test (void);

int
test (void)
{
  (void) dlopen ("reldepmod4.so", RTLD_LAZY | RTLD_GLOBAL);
  if (dlsym (RTLD_DEFAULT, "call_me") != NULL)
    {
      puts ("found \"call_me\"");
      return EXIT_SUCCESS;
    }
  puts ("didn't find \"call_me\"");
  return EXIT_FAILURE;
}
