/* Test lazy binding during dlclose (bug 30425).
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

/* This test re-creates a situation that can arise naturally for C++
   applications due to the use of vague linkage and differences in the
   set of compiler-emitted functions.  A function in
   tst-dlclose-lazy-mod1.so (exported_function) interposes a function
   in tst-dlclose-lazy-mod2.so.  This function is called from the
   destructor in tst-dlclose-lazy-mod2.so, after the destructor for
   tst-dlclose-lazy-mod1.so has already completed.  Prior to the fix
   for bug 30425, this would lead to a lazy binding failure in
   tst-dlclose-lazy-mod1.so because dlclose had already marked the DSO
   as unavailable for binding (by setting l_removed).  */

#include <dlfcn.h>
#include <support/xdlfcn.h>
#include <support/check.h>

int
main (void)
{
  /* Load tst-dlclose-lazy-mod1.so, indirectly loading
     tst-dlclose-lazy-mod2.so.  */
  void *handle = xdlopen ("tst-dlclose-lazy-mod1.so", RTLD_GLOBAL | RTLD_LAZY);

  /* Invoke the destructor of tst-dlclose-lazy-mod2.so, which calls
     into tst-dlclose-lazy-mod1.so after its destructor has been
     called.  */
  xdlclose (handle);

  return 0;
}
