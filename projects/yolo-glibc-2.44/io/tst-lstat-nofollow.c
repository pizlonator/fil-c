/* Test that lstat does not follow symbolic links.
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

#include <string.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/support.h>
#include <sys/stat.h>
#include <unistd.h>

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
        case FUSE_LOOKUP:
          {
            TEST_COMPARE (inh->nodeid, 1);
            TEST_COMPARE_STRING (support_fuse_cast (LOOKUP, inh), "symlink");
            struct fuse_entry_out *out = support_fuse_prepare_entry (f, 2);
            out->attr.mode = S_IFLNK | 0777;
            out->attr.size = strlen ("target");
            support_fuse_reply_prepared (f);
          }
          break;
        case FUSE_GETATTR:
          {
            TEST_COMPARE (inh->nodeid, 2);
            struct fuse_attr_out *out = support_fuse_prepare_attr (f);
            out->attr.mode = S_IFLNK | 0777;
            out->attr.size = strlen ("target");
            support_fuse_reply_prepared (f);
          }
          break;
        case FUSE_READLINK:
          /* The lstat operation must not attempt to look at the
             symbolic link target.  */
          FAIL ("attempt to obtain target of symblic link for node %llu",
                (unsigned long long int) inh->nodeid);
          break;
        default:
          FAIL ("unexpected event %s", support_fuse_opcode (inh->opcode));
        }
    }
}

static int
do_test (void)
{
  support_fuse_init ();
  struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);
  char *symlink_path = xasprintf ("%s/symlink", support_fuse_mountpoint (f));

  {
    struct stat st = { 0, };
    TEST_COMPARE (lstat (symlink_path, &st), 0);
    TEST_COMPARE (st.st_uid, getuid ());
    TEST_COMPARE (st.st_gid, getgid ());
    TEST_COMPARE (st.st_size, 6);
    TEST_COMPARE (st.st_mode, S_IFLNK | 0777);
  }

  {
    struct stat64 st = { 0, };
    TEST_COMPARE (lstat64 (symlink_path, &st), 0);
    TEST_COMPARE (st.st_uid, getuid ());
    TEST_COMPARE (st.st_gid, getgid ());
    TEST_COMPARE (st.st_size, 6);
    TEST_COMPARE (st.st_mode, S_IFLNK | 0777);
  }

  free (symlink_path);
  support_fuse_unmount (f);
  return 0;
}

#include <support/test-driver.c>
