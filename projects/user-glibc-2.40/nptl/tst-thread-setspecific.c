/* Test to verify that passing a pointer to an uninitialized object
   to pthread_setspecific doesn't trigger bogus uninitialized warnings.
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

#include <pthread.h>
#include <stdlib.h>
#include <libc-diag.h>

/* Turn uninitialized warnings into errors to detect the problem.
   See BZ #27714.  */

DIAG_PUSH_NEEDS_COMMENT;
DIAG_IGNORE_NEEDS_COMMENT (6, "-Wmaybe-uninitialized");
DIAG_IGNORE_NEEDS_COMMENT (6, "-Wuninitialized");

static int
do_test (void)
{
  void *p = malloc (1);   /* Deliberately uninitialized.  */
  pthread_setspecific (pthread_self (), p);

  void *q = pthread_getspecific (pthread_self ());

  return p == q;
}

DIAG_POP_NEEDS_COMMENT;

#include <support/test-driver.c>
