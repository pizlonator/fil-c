/* Smoke test for faccessat with different UID/GID combinations.  Needs root
   access.
   Copyright The GNU Toolchain Authors.
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>

#include <support/check.h>
#include <support/namespace.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xdirent.h>
#include <support/xunistd.h>

#define SOMEFILE "some-file"

static int dir_fd;
uid_t users[3];
gid_t groups[3];

struct test_case
{
  int mode;
  uid_t uid;
  uid_t euid;
  gid_t gid;
  gid_t egid;
  int flags;
  bool succeeds;
};

static void
run_one_test_child (void *in)
{
  struct test_case *t = (struct test_case *) in;

  printf ("TEST: MODE=%s, UID=%d, EUID=%d, GID=%d, EGID=%d, FLAGS=%s: ",
	  t->mode == R_OK ? "R_OK" : "W_OK", t->uid, t->euid, t->gid, t->egid,
	  t->flags ? "AT_EACCESS" : "0");

  if (setregid (t->gid, t->egid) != 0)
    FAIL_EXIT1 ("Could not change group: %m\n");
  if (setreuid (t->uid, t->euid) != 0)
    FAIL_EXIT1 ("Could not change user: %m\n");

  if (faccessat (dir_fd, SOMEFILE, t->mode, t->flags) != 0 && t->succeeds)
    FAIL_EXIT1 ("faccessat failed: %m\n");

  if (!t->succeeds && errno != EACCES)
    FAIL_EXIT1 ("Unexpected faccessat failure: %m\n");

  printf ("OK%s\n", !t->succeeds ? " (FAILED with EACCES)" : "");
}

static void
run_one_test (int mode, int u, int eu, int g, int eg, int flags, bool succeeds)
{
  struct test_case t =
    {mode, users[u], users[eu], groups[g], groups[eg], flags, succeeds};
  support_isolate_in_subprocess (run_one_test_child, &t);
}

static int
do_test (void)
{

  /* We need to start as root.  */
  if (getuid () != 0)
    FAIL_UNSUPPORTED ("Test needs to be run as root (UID 0)\n");

  /* Collect 3 distinct users and groups to test with.  */
  struct passwd *ent = NULL;
  int count = 0;
  while ((ent = getpwent ()) != NULL && count < 3)
    {
      if (ent->pw_uid == 0 || ent->pw_gid == 0)
	continue;

      int i = count;
      bool skip = false;
      while (i > 0)
	if (groups[--i] == ent->pw_gid)
	  skip = true;

      if (skip)
	continue;

      users[count] = ent->pw_uid;
      groups[count++] = ent->pw_gid;
    }

  if (count < 3)
    FAIL_UNSUPPORTED ("Not enough users in the system to do this test\n");

  printf ("Testing with UID/GID:\n");
  while (--count >= 0)
    printf ("    UID: %d, GID: %d\n", users[count], groups[count]);
  printf ("\n");

  char *tempdir = support_create_temp_directory ("tst-faccessat-setuid.");
  dir_fd = xopen (tempdir, O_RDONLY | O_DIRECTORY, 0);

  xfchmod (dir_fd, 0777);

  /* Now, create a file in it, which will be our test case.  */

  int fd = openat (dir_fd, SOMEFILE, O_CREAT|O_RDWR|O_EXCL, 0640);
  if (fd == -1)
    {
      if (errno == ENOSYS)
	FAIL_UNSUPPORTED ("*at functions not supported");

      FAIL_EXIT1 ("file creation failed");
    }
  xwrite (fd, "hello", 5);

  if (fchown (fd, users[0], groups[1]) == -1)
    FAIL_EXIT1 ("fchown failed: %m\n");
  xclose (fd);

  char *somefile = xasprintf ("%s/" SOMEFILE, tempdir);
  add_temp_file (somefile);

  /* Finally, run through the combinations.  */
  for (int u = 0; u < 3; u++)
    for (int eu = 0; eu < 3; eu++)
      for (int g = 0; g < 3; g++)
	for (int eg = 0; eg < 3; eg++)
	  {
	    run_one_test (R_OK, u, eu, g, eg, 0, u == 0 || g == 1);
	    run_one_test (W_OK, u, eu, g, eg, 0, u == 0);
	    run_one_test (R_OK, u, eu, g, eg, AT_EACCESS, eu == 0 || eg == 1);
	    run_one_test (W_OK, u, eu, g, eg, AT_EACCESS, eu == 0);
	  }

  xclose (dir_fd);
  free (tempdir);
  free (somefile);

  return 0;
}
#include <support/test-driver.c>
