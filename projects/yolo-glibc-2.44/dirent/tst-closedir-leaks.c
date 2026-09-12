/* Test for resource leaks in closedir.
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

#include <fcntl.h>
#include <limits.h>
#include <mcheck.h>
#include <stdbool.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/descriptors.h>
#include <support/readdir.h>
#include <support/xdirent.h>
#include <support/xunistd.h>

static void
one_test (enum support_readdir_op op, unsigned int read_limit,
          bool use_fdopendir)
{
  struct support_descriptors *fds = support_descriptors_list ();
  struct support_dirent e = { 0, };

  DIR *stream;
  if (use_fdopendir)
    {
      int fd = xopen (".", O_RDONLY | O_DIRECTORY, 0);
      stream = xfdopendir (fd);
      /* The descriptor fd will be closed by closedir below.  */
    }
  else
    stream = xopendir (".");
  for (unsigned int i = 0; i < read_limit; ++i)
    if (!support_readdir (stream, op, &e))
      break;
  TEST_COMPARE (closedir (stream), 0);

  free (e.d_name);
  support_descriptors_check (fds);
  support_descriptors_free (fds);
}

static int
do_test (void)
{
  mtrace ();

  for (int use_fdopendir = 0; use_fdopendir < 2; ++use_fdopendir)
    {
      /* No reads, operation does not matter.  */
      one_test (SUPPORT_READDIR, 0, use_fdopendir);

      for (enum support_readdir_op op = 0; op <= support_readdir_op_last();
           ++op)
        {
          one_test (op, 1, use_fdopendir);
          one_test (op, UINT_MAX, use_fdopendir); /* Unlimited reads.  */
        }
    }

  return 0;
}

#include <support/test-driver.c>
