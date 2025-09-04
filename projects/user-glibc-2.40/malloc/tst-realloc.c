/* Copyright (C) 2013-2024 Free Software Foundation, Inc.
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
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libc-diag.h>
#include <support/check.h>

#include "tst-malloc-aux.h"

static int
do_test (void)
{
  void *p;
  unsigned char *c;
  int save, i, ok;

  errno = 0;

  /* realloc (NULL, ...) behaves similarly to malloc (C89).  */
  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (7, 0)
  /* GCC 7 warns about too-large allocations; here we want to test
     that they fail.  */
  DIAG_IGNORE_NEEDS_COMMENT (7, "-Walloc-size-larger-than=");
#endif
  p = realloc (NULL, -1);
  DIAG_POP_NEEDS_COMMENT;
  save = errno;

  if (p != NULL)
    FAIL_EXIT1 ("realloc (NULL, -1) succeeded.");

  /* errno should be set to ENOMEM on failure (POSIX).  */
  if (p == NULL && save != ENOMEM)
    FAIL_EXIT1 ("errno is not set correctly");

  errno = 0;

  /* realloc (NULL, ...) behaves similarly to malloc (C89).  */
  p = realloc (NULL, 10);
  save = errno;

  if (p == NULL)
    FAIL_EXIT1 ("realloc (NULL, 10) failed.");

  free (p);

  p = calloc (20, 1);
  if (p == NULL)
    FAIL_EXIT1 ("calloc (20, 1) failed.");

  /* Check increasing size preserves contents (C89).  */
  p = realloc (p, 200);
  if (p == NULL)
    FAIL_EXIT1 ("realloc (p, 200) failed.");

  c = p;
  ok = 1;

  for (i = 0; i < 20; i++)
    {
      if (c[i] != 0)
        ok = 0;
    }

  if (ok == 0)
    FAIL_EXIT1 ("first 20 bytes were not cleared");

  free (p);

  p = realloc (NULL, 100);
  if (p == NULL)
    FAIL_EXIT1 ("realloc (NULL, 100) failed.");

  memset (p, 0xff, 100);

  /* Check decreasing size preserves contents (C89).  */
  p = realloc (p, 16);
  if (p == NULL)
    FAIL_EXIT1 ("realloc (p, 16) failed.");

  c = p;
  ok = 1;

  for (i = 0; i < 16; i++)
    {
      if (c[i] != 0xff)
        ok = 0;
    }

  if (ok == 0)
    FAIL_EXIT1 ("first 16 bytes were not correct");

  /* Check failed realloc leaves original untouched (C89).  */
  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (7, 0)
  /* GCC 7 warns about too-large allocations; here we want to test
     that they fail.  */
  DIAG_IGNORE_NEEDS_COMMENT (7, "-Walloc-size-larger-than=");
#endif
  c = realloc (p, -1);
  DIAG_POP_NEEDS_COMMENT;
  if (c != NULL)
    FAIL_EXIT1 ("realloc (p, -1) succeeded.");

  c = p;
  ok = 1;

  for (i = 0; i < 16; i++)
    {
      if (c[i] != 0xff)
        ok = 0;
    }

  if (ok == 0)
    FAIL_EXIT1 ("first 16 bytes were not correct after failed realloc");

  /* realloc (p, 0) frees p (C89) and returns NULL (glibc).  */
  p = realloc (p, 0);
  if (p != NULL)
    FAIL_EXIT1 ("realloc (p, 0) returned non-NULL.");

  /* realloc (NULL, 0) acts like malloc (0) (glibc).  */
  p = realloc (NULL, 0);
  if (p == NULL)
    FAIL_EXIT1 ("realloc (NULL, 0) returned NULL.");

  free (p);

  /* Smoke test to make sure that allocations do not move if they have enough
     space to expand in the chunk.  */
  for (size_t sz = 3; sz < 256 * 1024; sz += 2048)
    {
      p = realloc (NULL, sz);
      if (p == NULL)
	FAIL_EXIT1 ("realloc (NULL, %zu) returned NULL.", sz);
      size_t newsz = malloc_usable_size (p);
      printf ("size: %zu, usable size: %zu, extra: %zu\n",
	      sz, newsz, newsz - sz);
      uintptr_t oldp = (uintptr_t) p;
      void *new_p = realloc (p, newsz);
      if ((uintptr_t) new_p != oldp)
	FAIL_EXIT1 ("Expanding (%zu bytes) to usable size (%zu) moved block",
		    sz, newsz);
      free (new_p);

      /* We encountered a large enough extra size at least once.  */
      if (newsz - sz > 1024)
	break;
    }

  return 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
