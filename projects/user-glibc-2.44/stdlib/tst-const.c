/* Test bsearch const-generic macro.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <libc-diag.h>

void *vp;
const void *cvp;
int *ip;
const int *cip;
size_t sz;
int (*compar) (const void *, const void *);

#define CHECK_TYPE(EXPR, TYPE)					\
  _Static_assert (_Generic (EXPR, TYPE: 1), "type check")

static int
do_test (void)
{
  /* This is a compilation test.  */
  CHECK_TYPE (bsearch (cvp, cvp, sz, sz, compar), const void *);
  CHECK_TYPE (bsearch (cvp, vp, sz, sz, compar), void *);
  CHECK_TYPE (bsearch (vp, cvp, sz, sz, compar), const void *);
  CHECK_TYPE (bsearch (vp, vp, sz, sz, compar), void *);
  CHECK_TYPE (bsearch (cvp, cip, sz, sz, compar), const void *);
  CHECK_TYPE (bsearch (cvp, ip, sz, sz, compar), void *);
  CHECK_TYPE (bsearch (vp, cip, sz, sz, compar), const void *);
  CHECK_TYPE (bsearch (vp, ip, sz, sz, compar), void *);
  DIAG_PUSH_NEEDS_COMMENT;
  /* This deliberately tests the type of the result with a null
     pointer constant argument.  */
  DIAG_IGNORE_NEEDS_COMMENT (14, "-Wnonnull");
  CHECK_TYPE (bsearch (cvp, 0, sz, sz, compar), void *);
  CHECK_TYPE (bsearch (cvp, (void *) 0, sz, sz, compar), void *);
  DIAG_POP_NEEDS_COMMENT;
  CHECK_TYPE ((bsearch) (cvp, cvp, sz, sz, compar), void *);
  return 0;
}

#include <support/test-driver.c>
