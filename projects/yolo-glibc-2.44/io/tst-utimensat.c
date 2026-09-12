/* Test for utimensat.
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

#include <fcntl.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <sys/stat.h>
#include <sys/time.h>

static int
test_utimesat_helper (const char *testfile, int fd, const char *testlink,
                      const struct timespec *ts)
{
  {
    TEST_VERIFY_EXIT (utimensat (fd, testfile, ts, 0) == 0);

    struct statx st;
    xstatx (fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &st);

    /* Check if seconds for atime match */
    TEST_COMPARE (st.stx_atime.tv_sec, ts[0].tv_sec);

    /* Check if seconds for mtime match */
    TEST_COMPARE (st.stx_mtime.tv_sec, ts[1].tv_sec);
  }

  /* Alter the timestamp using a NULL path.  */
  {
    struct timespec ts1[2] = {ts[0], ts[1]};
    ts1[0].tv_sec ^= 1;
    ts1[1].tv_sec ^= 1;

    TEST_VERIFY_EXIT (utimensat (fd, NULL, ts1, 0) == 0);

    struct statx st;
    xstatx (fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &st);

    /* Check if seconds for atime match */
    TEST_COMPARE (st.stx_atime.tv_sec, ts[0].tv_sec ^ 1);

    /* Check if seconds for mtime match */
    TEST_COMPARE (st.stx_mtime.tv_sec, ts[1].tv_sec ^ 1);
  }

  /* And switch it back using a NULL path.  */
  {
    TEST_VERIFY_EXIT (utimensat (fd, NULL, ts, 0) == 0);

    struct statx st;
    xstatx (fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &st);

    /* Check if seconds for atime match */
    TEST_COMPARE (st.stx_atime.tv_sec, ts[0].tv_sec);

    /* Check if seconds for mtime match */
    TEST_COMPARE (st.stx_mtime.tv_sec, ts[1].tv_sec);
  }

  {
    struct statx stfile_orig;
    xstatx (AT_FDCWD, testfile, AT_SYMLINK_NOFOLLOW, STATX_BASIC_STATS,
	    &stfile_orig);

    TEST_VERIFY_EXIT (utimensat (0 /* ignored  */, testlink, ts,
				 AT_SYMLINK_NOFOLLOW)
		       == 0);
    struct statx stlink;
    xstatx (AT_FDCWD, testlink, AT_SYMLINK_NOFOLLOW, STATX_BASIC_STATS,
	    &stlink);

    TEST_COMPARE (stlink.stx_atime.tv_sec, ts[0].tv_sec);
    TEST_COMPARE (stlink.stx_mtime.tv_sec, ts[1].tv_sec);

    /* Check if the timestamp from original file is not changed.  */
    struct statx stfile;
    xstatx (AT_FDCWD, testfile, AT_SYMLINK_NOFOLLOW, STATX_BASIC_STATS,
	    &stfile);

    TEST_COMPARE (stfile_orig.stx_atime.tv_sec, stfile.stx_atime.tv_sec);
    TEST_COMPARE (stfile_orig.stx_mtime.tv_sec, stfile.stx_mtime.tv_sec);
  }

  return 0;
}

#define TEST_CALL(fname, fd, lname, v1, v2) \
  test_utimesat_helper (fname, fd, lname, (struct timespec[]) { { v1, 0 }, \
                                                                { v2, 0 } })

#include "tst-utimensat-skeleton.c"
