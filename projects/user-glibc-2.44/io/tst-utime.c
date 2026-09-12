/* Test for utime.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <utime.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static int
test_utime_helper (const char *file, int fd, const struct utimbuf *ut)
{
  int result = utime (file, ut);
  TEST_VERIFY_EXIT (result == 0);

  struct statx st;
  xstatx (fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &st);

  /* Check if seconds for actime match */
  TEST_COMPARE (st.stx_atime.tv_sec, ut->actime);

  /* Check if seconds for modtime match */
  TEST_COMPARE (st.stx_mtime.tv_sec, ut->modtime);

  return 0;
}

#define TEST_CALL(fname, fd, lname, v1, v2) \
  test_utime_helper (fname, fd, &(struct utimbuf) { (v1), (v2) })

#include "tst-utimensat-skeleton.c"
