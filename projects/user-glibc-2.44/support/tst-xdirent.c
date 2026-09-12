/* Compile test for error-checking wrappers for <dirent.h>
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

#include <support/xdirent.h>

#include <libc-diag.h>
#include <support/check.h>
#include <unistd.h>

static int
do_test (void)
{
  {
    DIR *d = xopendir (".");
    struct dirent *e = xreaddir (d);
    /* Assume that the "." special entry always comes first.  */
    TEST_COMPARE_STRING (e->d_name, ".");
    while (xreaddir (d) != NULL)
      ;
    xclosedir (d);
  }

  {
    DIR *d = xopendir (".");
    struct dirent64 *e = xreaddir64 (d);
    TEST_COMPARE_STRING (e->d_name, ".");
    while (xreaddir64 (d) != NULL)
      ;
    xclosedir (d);
  }

  /* The functions readdir_r, readdir64_r were deprecated in glibc 2.24.  */
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wdeprecated-declarations");

  {
    DIR *d = xopendir (".");
    union
      {
	struct dirent d;
	char b[offsetof (struct dirent, d_name) + NAME_MAX + 1];
      } buf;
    TEST_VERIFY (xreaddir_r (d, &buf.d));
    TEST_COMPARE_STRING (buf.d.d_name, ".");
    while (xreaddir_r (d, &buf.d))
      ;
    xclosedir (d);
  }

  {
    DIR *d = xopendir (".");
    struct dirent64 buf = { 0, };
    TEST_VERIFY (xreaddir64_r (d, &buf));
    TEST_COMPARE_STRING (buf.d_name, ".");
    while (xreaddir64_r (d, &buf))
      ;
    xclosedir (d);
  }

  DIAG_POP_NEEDS_COMMENT;

  return 0;
}

#include <support/test-driver.c>
