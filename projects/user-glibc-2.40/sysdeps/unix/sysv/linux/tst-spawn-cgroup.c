/* Tests for posix_spawn cgroup extension.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/xunistd.h>
#include <support/temp_file.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <unistd.h>

#define CGROUPFS "/sys/fs/cgroup/"
#ifndef CGROUP2_SUPER_MAGIC
# define CGROUP2_SUPER_MAGIC 0x63677270
#endif

#define F_TYPE_EQUAL(a, b) (a == (typeof (a)) b)

#define CGROUP_TEST "test-spawn-cgroup"

/* Nonzero if the program gets called via `exec'.  */
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },
static int restart;

/* Hold the four initial argument used to respawn the process, plus the extra
   '--direct', '--restart', the check type ('SIG_IGN' or 'SIG_DFL'), and a
   final NULL.  */
static char *spargs[8];

static inline char *
startswith (const char *s, const char *prefix)
{
  size_t l = strlen (prefix);
  if (strncmp (s, prefix, l) == 0)
    return (char *) s + l;
  return NULL;
}

static char *
get_cgroup (void)
{
  FILE *f = fopen ("/proc/self/cgroup", "re");
  if (f == NULL)
    FAIL_UNSUPPORTED ("no cgroup defined for the process: %m");

  char *cgroup = NULL;

  char *line = NULL;
  size_t linesiz = 0;
  while (xgetline (&line, &linesiz, f) > 0)
    {
      char *entry = startswith (line, "0:");
      if (entry == NULL)
	continue;

      entry = strchr (entry, ':');
      if (entry == NULL)
	continue;

      cgroup = entry + 1;
      size_t l = strlen (cgroup);
      if (cgroup[l - 1] == '\n')
	cgroup[l - 1] = '\0';

      cgroup = xstrdup (entry + 1);
      break;
    }

  xfclose (f);
  free (line);

  return cgroup;
}


/* Called on process re-execution.  */
static void
handle_restart (int argc, char *argv[])
{
  assert (argc == 1);
  char *newcgroup = argv[0];

  char *current_cgroup = get_cgroup ();
  TEST_VERIFY_EXIT (current_cgroup != NULL);
  TEST_COMPARE_STRING (newcgroup, current_cgroup);
}

static int
do_test_cgroup_failure (pid_t *pid, int cgroup)
{
  posix_spawnattr_t attr;
  TEST_COMPARE (posix_spawnattr_init (&attr), 0);
  TEST_COMPARE (posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETCGROUP), 0);
  TEST_COMPARE (posix_spawnattr_setcgroup_np (&attr, cgroup), 0);

  int cgetgroup;
  TEST_COMPARE (posix_spawnattr_getcgroup_np (&attr, &cgetgroup), 0);
  TEST_COMPARE (cgroup, cgetgroup);

  return posix_spawn (pid, spargs[0], NULL, &attr, spargs, environ);
}

static int
create_new_cgroup (char **newcgroup)
{
  struct statfs fs;
  if (statfs (CGROUPFS, &fs) < 0)
    {
      if (errno == ENOENT)
	FAIL_UNSUPPORTED ("no cgroupv2 mount found");
      FAIL_EXIT1 ("statfs (%s): %m\n", CGROUPFS);
    }

  if (!F_TYPE_EQUAL (fs.f_type, CGROUP2_SUPER_MAGIC))
    FAIL_UNSUPPORTED ("%s is not a cgroupv2 (expected %#jx, got %#jx)",
		      CGROUPFS, (intmax_t) CGROUP2_SUPER_MAGIC,
		      (intmax_t) fs.f_type);

  char *cgroup = get_cgroup ();
  TEST_VERIFY_EXIT (cgroup != NULL);
  *newcgroup = xasprintf ("%s/%s", cgroup, CGROUP_TEST);
  char *cgpath = xasprintf ("%s%s/%s", CGROUPFS, cgroup, CGROUP_TEST);
  free (cgroup);

  if (mkdir (cgpath, 0755) == -1 && errno != EEXIST)
    {
      if (errno == EACCES || errno == EPERM || errno == EROFS)
	FAIL_UNSUPPORTED ("can not create a new cgroupv2 group");
      FAIL_EXIT1 ("mkdir (%s): %m", cgpath);
    }
  add_temp_file (cgpath);

  return xopen (cgpath, O_DIRECTORY | O_RDONLY | O_CLOEXEC, 0666);
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:

     - one or four parameters if called initially:
       + argv[1]: path for ld.so        optional
       + argv[2]: "--library-path"      optional
       + argv[3]: the library path      optional
       + argv[4]: the application name

     - six parameters left if called through re-execution:
       + argv[4/1]: the application name
       + argv[5/2]: the created cgroup

     * When built with --enable-hardcoded-path-in-tests or issued without
       using the loader directly.  */

  if (restart)
    {
      handle_restart (argc - 1, &argv[1]);
      return 0;
    }

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

  char *newcgroup;
  int cgroup = create_new_cgroup (&newcgroup);

  int i;
  for (i = 0; i < argc - 1; i++)
    spargs[i] = argv[i + 1];
  spargs[i++] = (char *) "--direct";
  spargs[i++] = (char *) "--restart";
  spargs[i++] = (char *) newcgroup;
  spargs[i] = NULL;

  /* Check if invalid cgroups returns an error.  */
  {
    int r = do_test_cgroup_failure (NULL, -1);
    if (r == EOPNOTSUPP)
      FAIL_UNSUPPORTED ("posix_spawn POSIX_SPAWN_SETCGROUP is not supported");
    TEST_COMPARE (r, EINVAL);
  }

  {
    pid_t pid;
    TEST_COMPARE (do_test_cgroup_failure (&pid, cgroup), 0);

    siginfo_t sinfo;
    TEST_COMPARE (waitid (P_PID, pid, &sinfo, WEXITED), 0);
    TEST_COMPARE (sinfo.si_signo, SIGCHLD);
    TEST_COMPARE (sinfo.si_code, CLD_EXITED);
    TEST_COMPARE (sinfo.si_status, 0);
  }

  xclose (cgroup);
  free (newcgroup);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
