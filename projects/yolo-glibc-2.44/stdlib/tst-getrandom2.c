/* Tests for the getrandom functions.
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

#include <gnu/lib-names.h>
#include <support/check.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>
#include <sys/random.h>

static __typeof (getrandom) *getrandom_ptr;

static void *
threadfunc (void *ignored)
{
  char buffer;
  TEST_COMPARE (getrandom_ptr (&buffer, 1, 0), 1);
  return NULL;
}

static int
do_test (void)
{
  /* Check if issuing getrandom in the secondary libc.so works when
     the vDSO might be potentially used.  */
  void *handle = xdlmopen (LM_ID_NEWLM, LIBC_SO, RTLD_NOW);
  getrandom_ptr = xdlsym (handle, "getrandom");
  for (int i = 0; i < 1000; ++i)
    xpthread_join (xpthread_create (NULL, threadfunc, NULL));
  return 0;
}

#include <support/test-driver.c>
