/* Copyright (C) 2005-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <libc-diag.h>

#include "tst-malloc-aux.h"

static int errors = 0;

static void
merror (const char *msg)
{
  ++errors;
  printf ("Error: %s\n", msg);
}

static int
do_test (void)
{
  void *p, *q;

  errno = 0;

  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (7, 0)
  /* GCC 7 warns about too-large allocations; here we want to test
     that they fail.  */
  DIAG_IGNORE_NEEDS_COMMENT (7, "-Walloc-size-larger-than=");
#endif
  p = malloc (-1);
  DIAG_POP_NEEDS_COMMENT;

  if (p != NULL)
    merror ("malloc (-1) succeeded.");
  else if (errno != ENOMEM)
    merror ("errno is not set correctly.");

  p = malloc (10);
  if (p == NULL)
    merror ("malloc (10) failed.");

  p = realloc (p, 0);
  if (p != NULL)
    merror ("realloc (p, 0) failed.");

  p = malloc (0);
  if (p == NULL)
    merror ("malloc (0) failed.");

  p = realloc (p, 0);
  if (p != NULL)
    merror ("realloc (p, 0) failed.");

  q = malloc (256);
  if (q == NULL)
    merror ("malloc (256) failed.");

  p = malloc (512);
  if (p == NULL)
    merror ("malloc (512) failed.");

  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (7, 0)
  /* GCC 7 warns about too-large allocations; here we want to test
     that they fail.  */
  DIAG_IGNORE_NEEDS_COMMENT (7, "-Walloc-size-larger-than=");
#endif
  if (realloc (p, -256) != NULL)
    merror ("realloc (p, -256) succeeded.");
  else if (errno != ENOMEM)
    merror ("errno is not set correctly.");
  DIAG_POP_NEEDS_COMMENT;

#if __GNUC_PREREQ (12, 0)
  /* Ignore a valid warning about using a pointer made indeterminate
     by a prior call to realloc().  */
  DIAG_IGNORE_NEEDS_COMMENT (12, "-Wuse-after-free");
#endif
  free (p);
#if __GNUC_PREREQ (12, 0)
  DIAG_POP_NEEDS_COMMENT;
#endif

  p = malloc (512);
  if (p == NULL)
    merror ("malloc (512) failed.");

  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (7, 0)
  /* GCC 7 warns about too-large allocations; here we want to test
     that they fail.  */
  DIAG_IGNORE_NEEDS_COMMENT (7, "-Walloc-size-larger-than=");
#endif
  if (realloc (p, -1) != NULL)
    merror ("realloc (p, -1) succeeded.");
  else if (errno != ENOMEM)
    merror ("errno is not set correctly.");
  DIAG_POP_NEEDS_COMMENT;

#if __GNUC_PREREQ (12, 0)
  /* Ignore a valid warning about using a pointer made indeterminate
     by a prior call to realloc().  */
  DIAG_IGNORE_NEEDS_COMMENT (12, "-Wuse-after-free");
#endif
  free (p);
#if __GNUC_PREREQ (12, 0)
  DIAG_POP_NEEDS_COMMENT;
#endif
  free (q);

  return errors != 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
