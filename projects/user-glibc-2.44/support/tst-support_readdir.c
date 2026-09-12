/* Test the support_readdir function.
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

#include <support/readdir.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/xdirent.h>
#include <support/xunistd.h>

static int
do_test (void)
{
  DIR *reference_stream = xopendir (".");
  struct dirent64 *reference = xreaddir64 (reference_stream);

  for (enum support_readdir_op op = 0; op <= support_readdir_op_last (); ++op)
    {
      DIR *stream = xopendir (".");
      struct support_dirent e;
      memset (&e, 0xcc, sizeof (e));
      e.d_name = NULL;
      TEST_VERIFY (support_readdir (stream, op, &e));
      TEST_COMPARE (e.d_ino, reference->d_ino);
#ifdef _DIRENT_HAVE_D_OFF
      TEST_VERIFY (support_readdir_offset_width (op) != 0);
      TEST_COMPARE (e.d_off, reference->d_off);
#else
      TEST_COMPARE (support_readdir_offset_width (op), 0);
      TEST_COMPARE (e.d_off, 0);
#endif
      TEST_COMPARE (e.d_type, reference->d_type);
      TEST_COMPARE_STRING (e.d_name, reference->d_name);
      free (e.d_name);
      xclosedir (stream);
    }

  xclosedir (reference_stream);

  /* Error injection test.  */
  int devnull = xopen ("/dev/null", O_RDONLY, 0);
  for (enum support_readdir_op op = 0; op <= support_readdir_op_last (); ++op)
    {
      DIR *stream = xopendir (".");
      /* A descriptor incompatible with readdir.  */
      xdup2 (devnull, dirfd (stream));
      errno = -1;
      support_readdir_expect_error (stream, op, ENOTDIR);
      xclosedir (stream);
    }
  xclose (devnull);

  return 0;
}

#include <support/test-driver.c>
