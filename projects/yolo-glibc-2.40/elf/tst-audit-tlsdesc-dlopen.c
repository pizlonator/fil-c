/* DT_AUDIT with modules with TLSDESC.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <support/xthread.h>
#include <support/xdlfcn.h>

static void *
thr_func (void *mod)
{
  int* (*get_global1)(void) = xdlsym (mod, "get_global1");
  int* (*get_global2)(void) = xdlsym (mod, "get_global2");
  void (*set_global2)(int) = xdlsym (mod, "set_global2");
  int* (*get_local1)(void) = xdlsym (mod, "get_local1");
  int* (*get_local2)(void) = xdlsym (mod, "get_local2");

  int *global1 = get_global1 ();
  TEST_COMPARE (*global1, 0);
  ++*global1;

  int *global2 = get_global2 ();
  TEST_COMPARE (*global2, 0);
  ++*global2;
  TEST_COMPARE (*global2, 1);

  set_global2 (10);
  TEST_COMPARE (*global2, 10);

  int *local1 = get_local1 ();
  TEST_COMPARE (*local1, 0);
  ++*local1;

  int *local2 = get_local2 ();
  TEST_COMPARE (*local2, 0);
  ++*local2;

  return 0;
}

static int
do_test (void)
{
  void *mod = xdlopen ("tst-audit-tlsdesc-mod1.so", RTLD_LAZY);

  pthread_t thr = xpthread_create (NULL, thr_func, mod);
  void *r = xpthread_join (thr);
  TEST_VERIFY (r == NULL);

  return 0;
}

#include <support/test-driver.c>
