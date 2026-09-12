/* Test that the pointer guard is propagated to ld.so via static dlopen.
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

/* A statically linked program dlopens a shared object; the object's setjmp
   uses the just-mapped libc.so's pointer guard while the longjmp below uses
   the program's guard.  Unless __rtld_static_init propagates the guard to
   the loaded loader the two differ, and the setjmp/longjmp round-trip jumps
   to a corrupt address and crashes.  */

#include <setjmp.h>
#include <support/check.h>
#include <support/xdlfcn.h>

static void
call_longjmp (jmp_buf jb)
{
  longjmp (jb, 1);
}

static int
do_test (void)
{
  void *h = xdlopen ("tst-ptrguard-static-dlopen-mod.so", RTLD_NOW);
  void (*foo) (void) = xdlsym (h, "foo");
  void (**do_longjmp) (jmp_buf) = xdlsym (h, "do_longjmp");
  *do_longjmp = call_longjmp;

  /* foo () sets the jump buffer and calls back into call_longjmp; a
     mismatched guard makes the return jump fault.  */
  foo ();

  xdlclose (h);
  return 0;
}

#include <support/test-driver.c>
