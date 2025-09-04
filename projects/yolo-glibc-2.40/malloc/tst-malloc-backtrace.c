/* Verify that backtrace does not deadlock on itself on memory corruption.
   Copyright (C) 2015-2024 Free Software Foundation, Inc.
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

#include <signal.h>
#include <stdlib.h>

#include <support/support.h>
#include <libc-diag.h>

#define SIZE 4096

/* Wrap free with a function to prevent gcc from optimizing it out.  */
static void
__attribute__((noinline))
call_free (void *ptr)
{
  free (ptr);
#if __GNUC_PREREQ (12, 0)
  /* Ignore a valid warning about using a pointer made indeterminate
     by a prior call to free().  */
  DIAG_IGNORE_NEEDS_COMMENT (12, "-Wuse-after-free");
#endif
  *(size_t *)(ptr - sizeof (size_t)) = 1;
#if __GNUC_PREREQ (12, 0)
  DIAG_POP_NEEDS_COMMENT;
#endif
}

int
do_test (void)
{
  void *ptr1 = malloc (SIZE);
  void *ptr2 = malloc (SIZE);

  /* Avoid unwanted output to TTY after an expected memory corruption.  */
  ignore_stderr();

  call_free (ptr1);
  ptr1 = malloc (SIZE);

  /* Not reached.  The return statement is to put ptr2 into use so that gcc
     doesn't optimize out that malloc call.  */
  return (ptr1 == ptr2);
}

#define EXPECTED_SIGNAL SIGABRT
#include <support/test-driver.c>
