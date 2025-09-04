/* Copyright (C) 2017-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <sched.h>
#include <sys/prctl.h>

#include <support/namespace.h>

#include "tst-ttyname-common.c"

/* These chroot setup functions put the TTY at at "/console" (where it
   won't be found by ttyname), and create "/dev/console" as an
   ordinary file.  This way, it's easier to write test-cases that
   expect ttyname to fail; test-cases that expect it to succeed need
   to explicitly remount it at "/dev/console".  */

static int
do_in_chroot_1 (int (*cb)(const char *, int))
{
  printf ("info:  entering chroot 1\n");

  /* Open the PTS that we'll be testing on.  */
  int master;
  char *slavename;
  master = posix_openpt (O_RDWR|O_NOCTTY|O_NONBLOCK);
  if (master < 0)
    {
      if (errno == ENOENT)
	FAIL_UNSUPPORTED ("posix_openpt: %m");
      else
	FAIL_EXIT1 ("posix_openpt: %m");
    }
  VERIFY ((slavename = ptsname (master)));
  VERIFY (unlockpt (master) == 0);
  if (strncmp (slavename, "/dev/pts/", 9) != 0)
    FAIL_UNSUPPORTED ("slave pseudo-terminal is not under /dev/pts/: %s",
                      slavename);
  adjust_file_limit (slavename);
  int slave = xopen (slavename, O_RDWR, 0);
  if (!doit (slave, "basic smoketest",
             (struct result_r){.name=slavename, .ret=0, .err=0}))
    return 1;

  pid_t pid = xfork ();
  if (pid == 0)
    {
      xclose (master);

      if (!support_enter_mount_namespace ())
	FAIL_UNSUPPORTED ("could not enter new mount namespace");

      VERIFY (mount ("tmpfs", chrootdir, "tmpfs", 0, "mode=755") == 0);
      VERIFY (chdir (chrootdir) == 0);

      xmkdir ("proc", 0755);
      xmkdir ("dev", 0755);
      xmkdir ("dev/pts", 0755);

      VERIFY (mount ("/proc", "proc", NULL, MS_BIND|MS_REC, NULL) == 0);
      VERIFY (mount ("devpts", "dev/pts", "devpts",
                     MS_NOSUID|MS_NOEXEC,
                     "newinstance,ptmxmode=0666,mode=620") == 0);
      VERIFY (symlink ("pts/ptmx", "dev/ptmx") == 0);

      touch ("console", 0);
      touch ("dev/console", 0);
      VERIFY (mount (slavename, "console", NULL, MS_BIND, NULL) == 0);

      xchroot (".");

      char *linkname = xasprintf ("/proc/self/fd/%d", slave);
      char *target = proc_fd_readlink (linkname);
      VERIFY (strcmp (target, slavename) == 0);
      free (linkname);

      _exit (cb (slavename, slave));
    }
  int status;
  xwaitpid (pid, &status, 0);
  VERIFY (WIFEXITED (status));
  xclose (master);
  xclose (slave);
  return WEXITSTATUS (status);
}

static int
do_test (void)
{
  support_become_root ();

  do_in_chroot_1 (run_chroot_tests);

  return 0;
}

#include <support/test-driver.c>
