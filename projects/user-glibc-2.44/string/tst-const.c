/* Test <string.h> const-generic macros.
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

#include <string.h>
#include <libc-diag.h>

void *vp;
const void *cvp;
int *ip;
const int *cip;
char *cp;
const char *ccp;
int c;
size_t sz;

#define CHECK_TYPE(EXPR, TYPE)					\
  _Static_assert (_Generic (EXPR, TYPE: 1), "type check")

static int
do_test (void)
{
  /* This is a compilation test.  */
  CHECK_TYPE (memchr (vp, c, sz), void *);
  CHECK_TYPE (memchr (cvp, c, sz), const void *);
  CHECK_TYPE (memchr (ip, c, sz), void *);
  CHECK_TYPE (memchr (cip, c, sz), const void *);
  CHECK_TYPE (memchr (cp, c, sz), void *);
  CHECK_TYPE (memchr (ccp, c, sz), const void *);
  DIAG_PUSH_NEEDS_COMMENT;
  /* This deliberately tests the type of the result with a null
     pointer constant argument.  */
  DIAG_IGNORE_NEEDS_COMMENT (14, "-Wnonnull");
  CHECK_TYPE (memchr (0, c, sz), void *);
  CHECK_TYPE (memchr ((void *) 0, c, sz), void *);
  DIAG_POP_NEEDS_COMMENT;
  CHECK_TYPE ((memchr) (cvp, c, sz), void *);
  CHECK_TYPE (strchr (vp, c), char *);
  CHECK_TYPE (strchr (cvp, c), const char *);
  CHECK_TYPE (strchr (cp, c), char *);
  CHECK_TYPE (strchr (ccp, c), const char *);
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (14, "-Wnonnull");
  CHECK_TYPE (strchr (0, c), char *);
  CHECK_TYPE (strchr ((void *) 0, c), char *);
  DIAG_POP_NEEDS_COMMENT;
  CHECK_TYPE ((strchr) (ccp, c), char *);
  CHECK_TYPE (strpbrk (vp, vp), char *);
  CHECK_TYPE (strpbrk (vp, cvp), char *);
  CHECK_TYPE (strpbrk (cvp, vp), const char *);
  CHECK_TYPE (strpbrk (cvp, cvp), const char *);
  CHECK_TYPE (strpbrk (cp, cp), char *);
  CHECK_TYPE (strpbrk (cp, ccp), char *);
  CHECK_TYPE (strpbrk (ccp, cp), const char *);
  CHECK_TYPE (strpbrk (ccp, ccp), const char *);
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (14, "-Wnonnull");
  CHECK_TYPE (strpbrk (0, cp), char *);
  CHECK_TYPE (strpbrk (0, ccp), char *);
  CHECK_TYPE (strpbrk ((void *) 0, cp), char *);
  CHECK_TYPE (strpbrk ((void *) 0, ccp), char *);
  DIAG_POP_NEEDS_COMMENT;
  CHECK_TYPE ((strpbrk) (ccp, ccp), char *);
  CHECK_TYPE (strrchr (vp, c), char *);
  CHECK_TYPE (strrchr (cvp, c), const char *);
  CHECK_TYPE (strrchr (cp, c), char *);
  CHECK_TYPE (strrchr (ccp, c), const char *);
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (14, "-Wnonnull");
  CHECK_TYPE (strrchr (0, c), char *);
  CHECK_TYPE (strrchr ((void *) 0, c), char *);
  DIAG_POP_NEEDS_COMMENT;
  CHECK_TYPE ((strrchr) (ccp, c), char *);
  CHECK_TYPE (strstr (vp, vp), char *);
  CHECK_TYPE (strstr (vp, cvp), char *);
  CHECK_TYPE (strstr (cvp, vp), const char *);
  CHECK_TYPE (strstr (cvp, cvp), const char *);
  CHECK_TYPE (strstr (cp, cp), char *);
  CHECK_TYPE (strstr (cp, ccp), char *);
  CHECK_TYPE (strstr (ccp, cp), const char *);
  CHECK_TYPE (strstr (ccp, ccp), const char *);
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (14, "-Wnonnull");
  CHECK_TYPE (strstr (0, cp), char *);
  CHECK_TYPE (strstr (0, ccp), char *);
  CHECK_TYPE (strstr ((void *) 0, cp), char *);
  CHECK_TYPE (strstr ((void *) 0, ccp), char *);
  DIAG_POP_NEEDS_COMMENT;
  CHECK_TYPE ((strstr) (ccp, ccp), char *);
  return 0;
}

#include <support/test-driver.c>
