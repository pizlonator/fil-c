/* Test for rewinddir, using FUSE.
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

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/readdir.h>
#include <support/support.h>
#include <support/xdirent.h>
#include <libc-diag.h>

/* Return the file name at the indicated directory offset.  */
static char *
name_at_offset (unsigned int offset)
{
  DIAG_PUSH_NEEDS_COMMENT_CLANG;
  DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wstring-plus-int");
  if (offset <= 1)
    return xstrdup (".." + !offset); /* "." or "..".  */
  else
    /* Pad the name with a lot of zeros, so that the dirent buffer gets
       filled more quickly.  */
    return xasprintf ("file%0240u", offset);
  DIAG_POP_NEEDS_COMMENT_CLANG;
}

/* This many directory entries, including "." and "..".  */
enum { directory_entries = 200 };

/* Add the directory entry at OFFSET to the stream D.  */
static uint64_t
add_directory_entry (struct support_fuse_dirstream *d, uint64_t offset)
{
  if (offset >= directory_entries)
    return 0;

  char *name = name_at_offset (offset);
  uint64_t ino = 1000 + offset; /* Arbitrary value, distinct from 1.  */
  uint32_t type = DT_REG;
  if (offset <= 1)
    {
      type = DT_DIR;
      ino = 1;
    }

  ++offset;
  bool added = support_fuse_dirstream_add (d, ino, offset, type, name);
  free (name);
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

/* Lists the entire directory from start to end.  */
static void
verify_directory (DIR *dir, enum support_readdir_op op)
{
  struct support_dirent e = { 0, };

  TEST_VERIFY (support_readdir (dir, op, &e));
  TEST_COMPARE_STRING (e.d_name, ".");
  TEST_VERIFY (support_readdir (dir, op, &e));
  TEST_COMPARE_STRING (e.d_name, "..");
  for (int i = 2; i < directory_entries; ++i)
    {
      char *expected = name_at_offset (i);
      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE_STRING (e.d_name, expected);
      free (expected);
    }
  TEST_VERIFY (!support_readdir (dir, op, &e));
  free (e.d_name);
}

/* Run tests with rewinding after ENTRIES readdir calls.  */
static void
rewind_after (unsigned int rewind_at)
{
  for (enum support_readdir_op op = 0; op <= support_readdir_op_last (); ++op)
    {
      printf ("info: testing %s (rewind_at=%u)\n",
              support_readdir_function (op), rewind_at);

      struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);
      DIR *dir = xopendir (support_fuse_mountpoint (f));
      struct support_dirent e = { 0, };

      switch (rewind_at)
        {
        case 0:
          break;
        case 1:
          TEST_VERIFY (support_readdir (dir, op, &e));
          TEST_COMPARE_STRING (e.d_name, ".");
          break;
        default:
          TEST_VERIFY (support_readdir (dir, op, &e));
          TEST_COMPARE_STRING (e.d_name, ".");
          TEST_VERIFY (support_readdir (dir, op, &e));
          TEST_COMPARE_STRING (e.d_name, "..");
          for (int i = 2; i < directory_entries; ++i)
            {
              if (i == rewind_at)
                break;
              char *expected = name_at_offset (i);
              TEST_VERIFY (support_readdir (dir, op, &e));
              TEST_COMPARE_STRING (e.d_name, expected);
              free (expected);
            }
          break;
        }

      errno = 0;
      rewinddir (dir);
      TEST_COMPARE (errno, 0);
      verify_directory (dir, op);

      free (e.d_name);
      xclosedir (dir);
      support_fuse_unmount (f);
    }
}

static int
do_test (void)
{
  support_fuse_init ();

  /* One pass without rewinding to verify that the generated directory
     content matches expectations.  */
  {
    struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);
    DIR *dir = xopendir (support_fuse_mountpoint (f));
    verify_directory (dir, SUPPORT_READDIR64);
    xclosedir (dir);
    support_fuse_unmount (f);
  }

  for (int do_unbuffered = 0; do_unbuffered < 2; ++do_unbuffered)
    {
      one_entry_per_getdents64 = do_unbuffered;

      for (int i = 0; i < 20; ++i)
        rewind_after (i);
      rewind_after (50);
      rewind_after (100);
      rewind_after (150);
      rewind_after (180);
      rewind_after (199);
    }

  return 0;
}

#include <support/test-driver.c>
