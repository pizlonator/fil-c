/* Tests of utime.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include <utime.h>

#include <stdio.h>
#include <stdlib.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-utime.t"

#include "test-utimens-common.h"

/* If PRINT, warn before skipping tests with status 77.  */
static int
test_utime (bool print)
{
  struct stat st1;
  struct stat st2;

  ASSERT (close (creat (BASE "file", 0600)) == 0);
  ASSERT (stat (BASE "file", &st1) == 0);
  nap ();
  ASSERT (utime (BASE "file", NULL) == 0);
  ASSERT (stat (BASE "file", &st2) == 0);
  ASSERT (0 <= utimecmp (BASE "file", &st2, &st1, UTIMECMP_TRUNCATE_SOURCE));
  if (check_ctime)
    ASSERT (ctime_compare (&st1, &st2) < 0);
  {
    /* On some NFS systems, the 'now' timestamp of creat or a NULL
       utimbuf is determined by the server, but the 'now' timestamp
       determined by time() is determined by the client; since the two
       machines are not necessarily on the same clock, this is another
       case where time can appear to go backwards.  The rest of this
       test cares about client time, so manually use time() to set
       both times.  */
    struct utimbuf ts;
    ts.actime = ts.modtime = time (NULL);
    ASSERT (utime (BASE "file", &ts) == 0);
    ASSERT (stat (BASE "file", &st1) == 0);
    nap ();
  }

  /* Invalid arguments.  */
  errno = 0;
  ASSERT (utime ("no_such", NULL) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (utime ("no_such/", NULL) == -1);
  ASSERT (errno == ENOENT || errno == ENOTDIR);
  errno = 0;
  ASSERT (utime ("", NULL) == -1);
  ASSERT (errno == ENOENT);
  {
    struct utimbuf ts;
    ts.actime = ts.modtime = Y2K;
    errno = 0;
    ASSERT (utime (BASE "file/", &ts) == -1);
    ASSERT (errno == ENOTDIR || errno == EINVAL);
  }
  ASSERT (stat (BASE "file", &st2) == 0);
  ASSERT (st1.st_atime == st2.st_atime);
  ASSERT (get_stat_atime_ns (&st1) == get_stat_atime_ns (&st2));
  ASSERT (utimecmp (BASE "file", &st1, &st2, 0) == 0);

  /* Set both times.  */
  {
    struct utimbuf ts;
    ts.actime = ts.modtime = Y2K;
    ASSERT (utime (BASE "file", &ts) == 0);
    ASSERT (stat (BASE "file", &st2) == 0);
    ASSERT (st2.st_atime == Y2K);
    ASSERT (0 <= get_stat_atime_ns (&st2));
    ASSERT (get_stat_atime_ns (&st2) < BILLION / 2);
    ASSERT (st2.st_mtime == Y2K);
    ASSERT (0 <= get_stat_mtime_ns (&st2));
    ASSERT (get_stat_mtime_ns (&st2) < BILLION);
    if (check_ctime)
      ASSERT (ctime_compare (&st1, &st2) < 0);
  }

  /* Make sure this dereferences symlinks.  */
  if (symlink (BASE "file", BASE "link"))
    {
      ASSERT (unlink (BASE "file") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  ASSERT (lstat (BASE "link", &st1) == 0);
  ASSERT (st1.st_mtime != Y2K);
  errno = 0;
  ASSERT (utime (BASE "link/", NULL) == -1);
  ASSERT (errno == ENOTDIR);
  {
    struct utimbuf ts;
    ts.actime = ts.modtime = Y2K;
    ASSERT (utime (BASE "link", &ts) == 0);
    ASSERT (lstat (BASE "link", &st2) == 0);
    /* Can't compare atimes, since lstat() changes symlink atime on cygwin.  */
    ASSERT (st1.st_mtime == st2.st_mtime);
    ASSERT (stat (BASE "link", &st2) == 0);
    ASSERT (st2.st_mtime == Y2K);
    ASSERT (get_stat_mtime_ns (&st2) == 0);
  }

  /* Cleanup.  */
  ASSERT (unlink (BASE "link") == 0);
  ASSERT (unlink (BASE "file") == 0);
  return 0;
}

int
main (void)
{
  int result1; /* Skip because of no symlink support.  */

  /* Clean up any trash from prior testsuite runs.  */
  ignore_value (system ("rm -rf " BASE "*"));

  result1 = test_utime (true);
  return result1;
}
