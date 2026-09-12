/* Test that readdir does not skip entries with d_ino == 0 (bug 12165).
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

#include <stdlib.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/readdir.h>
#include <support/xdirent.h>

/* Add the directory entry at OFFSET to the stream D.  */
static uint64_t
add_directory_entry (struct support_fuse_dirstream *d, uint64_t offset)
{
  bool added = false;
  ++offset;
  switch (offset - 1)
    {
    case 0:
      added = support_fuse_dirstream_add (d, 1, offset, DT_DIR, ".");
      break;
    case 1:
      added = support_fuse_dirstream_add (d, 1, offset, DT_DIR, "..");
      break;
    case 2:
      added = support_fuse_dirstream_add (d, 2, offset, DT_REG, "before");
      break;
    case 3:
      added = support_fuse_dirstream_add (d, 0, offset, DT_REG, "zero");
      break;
    case 4:
      added = support_fuse_dirstream_add (d, 3, offset, DT_REG, "after");
      break;
    }
  if (added)
    return offset;
  else
    return 0;
}

/* Set to true if getdents64 should produce only one entry.  */
static bool one_entry_per_getdents64;

static void
fuse_thread (struct support_fuse *f, void *closure)
{
  struct fuse_in_header *inh;
  while ((inh = support_fuse_next (f)) != NULL)
    {
      if (support_fuse_handle_mountpoint (f)
          || (inh->nodeid == 1 && support_fuse_handle_directory (f)))
        continue;
      switch (inh->opcode)
        {
        case FUSE_READDIR:
          if (inh->nodeid == 1)
            {
              uint64_t offset = support_fuse_cast (READ, inh)->offset;
              struct support_fuse_dirstream *d
                = support_fuse_prepare_readdir (f);
              while (true)
                {
                  offset = add_directory_entry (d, offset);
                  if (offset == 0 || one_entry_per_getdents64)
                    break;
                }
              support_fuse_reply_prepared (f);
            }
          else
            support_fuse_reply_error (f, EIO);
          break;
        default:
          FAIL ("unexpected event %s", support_fuse_opcode (inh->opcode));
          support_fuse_reply_error (f, EIO);
        }
    }
}

static int
do_test (void)
{
  support_fuse_init ();

  for (enum support_readdir_op op = 0; op <= support_readdir_op_last (); ++op)
    {
      struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);
      DIR *dir = xopendir (support_fuse_mountpoint (f));
      struct support_dirent e = { 0, };

      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE_STRING (e.d_name, ".");
      TEST_COMPARE (e.d_ino, 1);

      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE_STRING (e.d_name, "..");
      TEST_COMPARE (e.d_ino, 1);

      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE_STRING (e.d_name, "before");
      TEST_COMPARE (e.d_ino, 2);

      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE_STRING (e.d_name, "zero");
      TEST_COMPARE (e.d_ino, 0);

      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE_STRING (e.d_name, "after");
      TEST_COMPARE (e.d_ino, 3);

      TEST_VERIFY (!support_readdir (dir, op, &e));

      free (e.d_name);
      xclosedir (dir);
      support_fuse_unmount (f);
    }

  return 0;
}

#include <support/test-driver.c>
