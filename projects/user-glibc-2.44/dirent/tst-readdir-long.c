/* Test readdir (+variants) behavior with file names of varying length.
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/support.h>
#include <support/xdirent.h>
#include <support/readdir.h>
#include <libc-diag.h>

/* If positive, at this length an EMSGSIZE error is injected.  */
static _Atomic int inject_error_at_length;

/* Return a file name, LENGTH bytes long.  */
static char *
name_of_length (size_t length)
{
  char *result = xmalloc (length + 1);
  unsigned int prefix = snprintf (result, length + 1, "%zu-", length);
  for (size_t i = prefix; i < length; ++i)
    result[i] = 'A' + ((length + i) % 26);
  result[length] = '\0';
  return result;
}

/* Add the directory entry at OFFSET to the stream D.  */
static uint64_t
add_directory_entry (struct support_fuse_dirstream *d, uint64_t offset)
{
  unsigned int length = offset + 1;
  if (length > 1000)
    /* Longer than what is possible to produce with 256
       UTF-8-encoded Unicode code points.  */
    return 0;

  char *to_free = NULL;
  const char *name;
  uint64_t ino = 1000 + length; /* Arbitrary value, distinct from 1.  */
  uint32_t type = DT_REG;
  if (offset <= 1)
    {
      type = DT_DIR;
      DIAG_PUSH_NEEDS_COMMENT_CLANG;
      DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wstring-plus-int");
      name = ".." + !offset;    /* "." or "..".  */
      DIAG_POP_NEEDS_COMMENT_CLANG;
      ino = 1;
    }
  else if (length == 1000)
    name = "short";
  else
    {
      to_free = name_of_length (length);
      name = to_free;
    }

  ++offset;
  bool added = support_fuse_dirstream_add (d, ino, offset, type, name);
  free (to_free);
  if (added)
    return offset;
  else
    return 0;
}

/* Set to true if getdents64 should produce only one entry.  */
static _Atomic bool one_entry_per_getdents64;

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
              if (inject_error_at_length == offset + 1)
                support_fuse_reply_error (f, EMSGSIZE);
              else
                {
                  struct support_fuse_dirstream *d
                    = support_fuse_prepare_readdir (f);
                  while (true)
                    {
                      offset = add_directory_entry (d, offset);
                      if (offset == 0 || one_entry_per_getdents64
                          /* Error will be reported at next READDIR.  */
                          || offset + 1 == inject_error_at_length)
                        break;
                    }
                  support_fuse_reply_prepared (f);
                }
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

/* Run the tests for the specified readdir variant OP.  */
static void
run_readdir_tests (struct support_fuse *f, enum support_readdir_op op)
{
  printf ("info: testing %s (inject_error=%d unbuffered=%d)\n",
          support_readdir_function (op), inject_error_at_length,
          (int) one_entry_per_getdents64);

  bool testing_r = support_readdir_r_variant (op);

  DIR *dir = xopendir (support_fuse_mountpoint (f));
  struct support_dirent e = { 0, };
  TEST_VERIFY (support_readdir (dir, op, &e));
  TEST_COMPARE (e.d_ino, 1);
  TEST_COMPARE_STRING (e.d_name, ".");

  TEST_VERIFY (support_readdir (dir, op, &e));
  TEST_COMPARE (e.d_ino, 1);
  TEST_COMPARE_STRING (e.d_name, "..");

  for (unsigned int i = 3; i < 1000; ++i)
    {
      if (i == inject_error_at_length)
        /* Error expected below.  */
        break;

      if (i >= sizeof ((struct dirent) { 0, }.d_name) && testing_r)
        /* This is a readir_r test.  The longer names are not
           available because they do not fit into struct dirent.  */
        break;

      char *expected_name = name_of_length (i);
      TEST_COMPARE (strlen (expected_name), i);
      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE (e.d_ino, 1000 + i);
      TEST_COMPARE_STRING (e.d_name, expected_name);
      free (expected_name);
    }

  if (inject_error_at_length == 0)
    {
      /* Check that the ENAMETOOLONG error does not prevent reading a
         later short name.  */
      TEST_VERIFY (support_readdir (dir, op, &e));
      TEST_COMPARE (e.d_ino, 2000);
      TEST_COMPARE_STRING (e.d_name, "short");

      if (testing_r)
        /* An earlier name was too long.  */
        support_readdir_expect_error (dir, op, ENAMETOOLONG);
      else
        /* Entire directory read without error.  */
        TEST_VERIFY (!support_readdir (dir, op, &e));
    }
  else
    support_readdir_expect_error (dir, op, EMSGSIZE);

  free (e.d_name);
  xclosedir (dir);
}

/* Run all readdir variants for both fully-buffered an unbuffered
   (one-at-a-time) directory streams.  */
static void
run_fully_buffered_and_singleton_buffers (struct support_fuse *f)
{
  for (int do_one_entry = 0; do_one_entry < 2; ++do_one_entry)
    {
      one_entry_per_getdents64 = do_one_entry;
      for (enum support_readdir_op op = 0; op <= support_readdir_op_last();
           ++op)
        run_readdir_tests (f, op);
    }
}

static int
do_test (void)
{
  /* Smoke test for name_of_length.  */
  {
    char *name = name_of_length (5);
    TEST_COMPARE_STRING (name, "5-HIJ");
    free (name);

    name = name_of_length (6);
    TEST_COMPARE_STRING (name, "6-IJKL");
    free (name);
  }

  support_fuse_init ();
  struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);

  run_fully_buffered_and_singleton_buffers (f);

  inject_error_at_length = 100;
  run_fully_buffered_and_singleton_buffers (f);

  inject_error_at_length = 300;
  run_fully_buffered_and_singleton_buffers (f);

  support_fuse_unmount (f);
  return 0;
}

#include <support/test-driver.c>
