/* Verify that static pthread executable works.
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

#include <pthread.h>
#include <support/check.h>

pthread_key_t k;

static int
do_test (void)
{
  int rc;

  rc = pthread_key_create (&k, NULL);
  TEST_VERIFY (rc == 0);
  rc = pthread_setspecific (k, NULL);
  TEST_VERIFY (rc == 0);

  return 0;
}

#include <support/test-driver.c>
