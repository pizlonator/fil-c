/* Simple tests for some gnulib imported fts features.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xunistd.h>

static char *tempdir;

static void
do_prepare (int argc, char **argv)
{
  tempdir = support_create_temp_directory ("tst-fts-newflags");

  /* Create directory tree:
     tempdir/dir
     tempdir/dir/file
     tempdir/dir/symlink -> tempdir   (cycle)  */
  char *path;

  path = xasprintf ("%s/dir", tempdir);
  xmkdir (path, 0777);
  add_temp_file (path);
  free (path);

  path = xasprintf ("%s/dir/file", tempdir);
  int fd = xopen (path, O_CREAT | O_RDWR, 0666);
  xclose (fd);
  add_temp_file (path);
  free (path);

  path = xasprintf ("%s/dir/symlink", tempdir);
  xsymlink (tempdir, path);
  add_temp_file (path);
  free (path);
}
#define PREPARE do_prepare

/* FTS_TIGHT_CYCLE_CHECK:  we use FTS_LOGICAL to follow the symlink we
   created, causing a loop.  The tight cycle checker should catch this
   immediately and emit FTS_DC.  */
static void
test_tight_cycle (void)
{
  char *paths[] = { tempdir, NULL };
  FTS *fts = fts_open (paths, FTS_LOGICAL | FTS_TIGHT_CYCLE_CHECK, NULL);
  TEST_VERIFY_EXIT (fts != NULL);

  FTSENT *ent;
  bool found_cycle = false;
  while ((ent = fts_read (fts)) != NULL)
    {
      if (ent->fts_info == FTS_DC)
        {
          found_cycle = true;
          break;
        }
    }
  TEST_VERIFY (found_cycle);
  fts_close (fts);
}

/* FTS_CWDFD: ensures that fts uses virtual file descriptors and does not
   actually change the process's global working directory.  */
static void
test_cwdfd (void)
{
  char *paths[] = { tempdir, NULL };
  FTS *fts = fts_open (paths, FTS_PHYSICAL | FTS_CWDFD, NULL);
  TEST_VERIFY_EXIT (fts != NULL);

  char *cwd_before = getcwd (NULL, 0);
  TEST_VERIFY_EXIT (cwd_before != NULL);

  FTSENT *ent;
  while ((ent = fts_read (fts)) != NULL)
    {
    }

  char *cwd_after = getcwd (NULL, 0);
  TEST_VERIFY_EXIT (cwd_after != NULL);

  /* If FTS_CWDFD works, the global CWD remains untouched.  */
  TEST_COMPARE_STRING (cwd_before, cwd_after);

  free (cwd_before);
  free (cwd_after);
  fts_close (fts);
}

/* FTS_DEFER_STAT: verifies that deferring the stat calls does not break
   standard physical tree traversals.  */
static void
test_defer_stat (void)
{
  char *paths[] = { tempdir, NULL };
  FTS *fts = fts_open (paths, FTS_PHYSICAL | FTS_DEFER_STAT, NULL);
  TEST_VERIFY_EXIT (fts != NULL);

  FTSENT *ent;
  int count = 0;
  while ((ent = fts_read (fts)) != NULL)
    {
      count++;
    }

  /* Expects:

     1: $tmpdir
     2: $tmpdir/dir
     3: $tmpdir/dir/file
     4: $tmpdir/dir/symlink
     5: $tmpdir/dir/
     6: $tmpdir/  */
  TEST_COMPARE (count, 6);
  fts_close (fts);
}

/* FTS_MOUNT: validates that the traversal refuses to cross into a different
   file system when the flag is set. Assumes /proc is on a different mount.  */
static void
test_mount (void)
{
  /* Create a symlink to /proc, which resides on a different mount point. */
  char *link_path = xasprintf ("%s/mnt_link", tempdir);
  xsymlink ("/proc", link_path);

  /* Traverse with FTS_LOGICAL so fts resolves the symlink and sees the
     /proc device ID, but add FTS_MOUNT to enforce the boundary. */
  char *paths[] = { tempdir, NULL };
  FTS *fts = fts_open (paths, FTS_LOGICAL | FTS_MOUNT, NULL);
  TEST_VERIFY_EXIT (fts != NULL);

  FTSENT *ent;
  bool found_mnt_link = false;
  while ((ent = fts_read (fts)) != NULL)
    {
      if (strcmp (ent->fts_name, "mnt_link") == 0)
        found_mnt_link = true;
    }

  /* Because the device ID of /proc differs from the root traversal
     device ID, FTS_MOUNT forces fts to skip the entry entirely. */
  TEST_VERIFY (!found_mnt_link);

  fts_close (fts);
  unlink (link_path);
  free (link_path);
}

/* FTS_VERBATIM: Ensures paths are passed through without having trailing
   slashes stripped.  */
static void
test_verbatim (void)
{
  char *path_with_slashes = xasprintf ("%s///", tempdir);
  char *paths[] = { path_with_slashes, NULL };

  FTS *fts = fts_open (paths, FTS_PHYSICAL | FTS_VERBATIM, NULL);
  TEST_VERIFY_EXIT (fts != NULL);

  FTSENT *ent = fts_read (fts);
  TEST_VERIFY_EXIT (ent != NULL);

  /* The returned root path should retain the exact slashes passed to it. */
  TEST_COMPARE_STRING (ent->fts_path, path_with_slashes);

  fts_close (fts);
  free (path_with_slashes);
}

/* Main test driver */
static int
do_test (void)
{
  support_need_proc ("FTS_MOUNT test requires /proc");

  {
    /* Check if fts_open does not fail if neither FTS_LOGICAL nor FTS_PHYSICAL
       are specified.  */
    char *paths[] = { tempdir, NULL };
    FTS *fts = fts_open (paths, 0, NULL);
    TEST_VERIFY_EXIT (fts != NULL);
    fts_close (fts);
  }

  {
    FTS *fts;
    char *paths[] = { tempdir, NULL };

    /* There are internal only flags.  */
    fts = fts_open (paths, FTS_NAMEONLY, NULL);
    TEST_VERIFY_EXIT (fts == NULL);
    TEST_COMPARE (errno, EINVAL);

    fts = fts_open (paths, FTS_STOP, NULL);
    TEST_VERIFY_EXIT (fts == NULL);
    TEST_COMPARE (errno, EINVAL);
  }

  test_tight_cycle ();
  test_cwdfd ();
  test_defer_stat ();
  test_mount ();
  test_verbatim ();

  return 0;
}

/* Includes the glibc test framework driver */
#include <support/test-driver.c>
