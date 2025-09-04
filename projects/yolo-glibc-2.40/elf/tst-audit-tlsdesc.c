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

extern __thread int global1;
extern __thread int global2;
void *get_local1 (void);
void set_global2 (int v);
void *get_local2 (void);

static void *
thr_func (void *clousure)
{
  TEST_COMPARE (global1, 0);
  ++global1;
  TEST_COMPARE (global2, 0);
  ++global2;
  TEST_COMPARE (global2, 1);

  set_global2 (10);
  TEST_COMPARE (global2, 10);

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
  pthread_t thr = xpthread_create (NULL, thr_func, NULL);
  void *r = xpthread_join (thr);
  TEST_VERIFY (r == NULL);
  return 0;
}

#include <support/test-driver.c>
