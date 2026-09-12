/* FUSE-based test for fchmod.
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

#include <support/fuse.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>

/* Set from do_test to indicate the expected incoming mode change request.  */
static _Atomic int expected_mode;

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
            char *name = support_fuse_cast (LOOKUP, inh);
            TEST_COMPARE_STRING (name, "file");
            struct fuse_entry_out *out
              = support_fuse_prepare_entry (f, 2);
            out->attr.mode = S_IFREG | 0600;
            support_fuse_reply_prepared (f);
          }
          break;
        case FUSE_OPEN:
          {
            TEST_COMPARE (inh->nodeid, 2);
            struct fuse_open_in *p = support_fuse_cast (OPEN, inh);
            TEST_COMPARE (p->flags & O_ACCMODE, O_RDWR);
            struct fuse_open_out out = { 0, };
            support_fuse_reply (f, &out, sizeof (out));
          }
          break;
        case FUSE_SETATTR:
          {
            TEST_COMPARE (inh->nodeid, 2);
            struct fuse_setattr_in *p = support_fuse_cast (SETATTR, inh);
            TEST_COMPARE (p->valid , FATTR_MODE);
            TEST_COMPARE (p->mode, S_IFREG | expected_mode);
            struct fuse_attr_out *out = support_fuse_prepare_attr (f);
            out->attr.mode = S_IFREG | p->mode;
            support_fuse_reply_prepared (f);
          }
          break;
        case FUSE_FLUSH:
          support_fuse_reply_empty (f);
          break;
        default:
          support_fuse_reply_error (f, EIO);
        }
    }
}

/* Test all mode values with the specified extra bits.  */
static void
test_with_bits (int fd, unsigned int extra_bits)
{
  for (int do_mode = 0; do_mode <= 07777; ++do_mode)
    {
      expected_mode = do_mode;
      TEST_COMPARE (fchmod (fd, extra_bits | do_mode), 0);
    }
}

static int
do_test (void)
{
  support_fuse_init ();

  struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);
  char *path = xasprintf ("%s/file", support_fuse_mountpoint (f));
  int fd = xopen (path, O_RDWR, 0600);
  free (path);

  test_with_bits (fd, 0);
  /* POSIX requires that the extra bits are ignored.  */
  test_with_bits (fd, S_IFREG);
  test_with_bits (fd, S_IFDIR);
  test_with_bits (fd, ~07777);

  xclose (fd);
  support_fuse_unmount (f);

  return 0;
}

#include <support/test-driver.c>
