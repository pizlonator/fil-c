/* Bug 22786: test for buffer overflow in realpath.
   Copyright (C) 2018-2026 Free Software Foundation, Inc.
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

/* This file must be run from within a directory called "stdlib".  */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <support/blob_repeat.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <libc-diag.h>

static int
do_test (void)
{
  char *dir = support_create_temp_directory ("bz22786.");
  char *lnk = xasprintf ("%s/symlink", dir);
  const size_t path_len = (size_t) INT_MAX + strlen (lnk) + 1;

  struct support_blob_repeat repeat
    = support_blob_repeat_allocate ("a", 1, path_len);
  char *path = repeat.start;

  TEST_VERIFY_EXIT (symlink (".", lnk) == 0);

  /* Construct very long path = "/tmp/bz22786.XXXX/symlink/aaaa....."  */
  char *p = mempcpy (path, lnk, strlen (lnk));
  *(p++) = '/';
  p[path_len - (p - path) - 1] = '\0';

  /* This call crashes before the fix for bz22786 on 32-bit platforms.
     It may trigger an OOM event. */
  support_accept_oom (true);
  p = realpath (path, NULL);
  support_accept_oom (false);
  TEST_VERIFY (p == NULL);
  /* For 64-bit platforms readlink return ENAMETOOLONG, while for 32-bit
     realpath will try to allocate a buffer larger than PTRDIFF_MAX.  */
  TEST_VERIFY (errno == ENOMEM || errno == ENAMETOOLONG);

  /* Cleanup.  */
  unlink (lnk);
  support_blob_repeat_free (&repeat);
  free (lnk);
  free (dir);

  return 0;
}

#include <support/test-driver.c>
