/* Capture output from a subprocess.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#include <support/subprocess.h>
#include <support/capture_subprocess.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <scratch_buffer.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <support/xsocket.h>
#include <support/xspawn.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>

static void
transfer (const char *what, struct pollfd *pfd, struct xmemstream *stream)
{
  if (pfd->revents != 0)
    {
      char buf[1024];
      ssize_t ret = TEMP_FAILURE_RETRY (read (pfd->fd, buf, sizeof (buf)));
      if (ret < 0)
        {
          support_record_failure ();
          printf ("error: reading from subprocess %s: %m\n", what);
          pfd->events = 0;
          pfd->revents = 0;
        }
      else if (ret == 0)
        {
          /* EOF reached.  Stop listening.  */
          pfd->events = 0;
          pfd->revents = 0;
        }
      else
        /* Store the data just read.   */
        TEST_VERIFY (fwrite (buf, ret, 1, stream->out) == 1);
    }
}

static void
support_capture_poll (struct support_capture_subprocess *result,
		      struct support_subprocess *proc)
{
  struct pollfd fds[2] =
    {
      { .fd = proc->stdout_pipe[0], .events = POLLIN },
      { .fd = proc->stderr_pipe[0], .events = POLLIN },
    };

  do
    {
      xpoll (fds, 2, -1);
      transfer ("stdout", &fds[0], &result->out);
      transfer ("stderr", &fds[1], &result->err);
    }
  while (fds[0].events != 0 || fds[1].events != 0);

  xfclose_memstream (&result->out);
  xfclose_memstream (&result->err);

  result->status = support_process_wait (proc);
}

struct support_capture_subprocess
support_capture_subprocess (void (*callback) (void *), void *closure)
{
  struct support_capture_subprocess result;
  xopen_memstream (&result.out);
  xopen_memstream (&result.err);

  struct support_subprocess proc = support_subprocess (callback, closure);

  support_capture_poll (&result, &proc);
  return result;
}

struct support_capture_subprocess
support_capture_subprogram (const char *file, char *const argv[],
			    char *const envp[])
{
  struct support_capture_subprocess result;
  xopen_memstream (&result.out);
  xopen_memstream (&result.err);

  struct support_subprocess proc = support_subprogram (file, argv, envp);

  support_capture_poll (&result, &proc);
  return result;
}

/* Copies the executable into a restricted directory, so that we can
   safely make it SGID with the TARGET group ID.  Then runs the
   executable.  */
static void
copy_and_spawn_sgid (const char *child_id, gid_t gid)
{
  char *dirname = support_create_temp_directory ("tst-glibc-sgid-");
  char *execname = xasprintf ("%s/bin", dirname);
  add_temp_file (execname);

  if (access ("/proc/self/exe", R_OK) != 0)
    FAIL_UNSUPPORTED ("unsupported: Cannot read binary from procfs\n");

  support_copy_file ("/proc/self/exe", execname);

  if (chown (execname, getuid (), gid) != 0)
    FAIL_UNSUPPORTED ("cannot change group of \"%s\" to %jd: %m",
		      execname, (intmax_t) gid);

  if (chmod (execname, 02750) != 0)
    FAIL_UNSUPPORTED ("cannot make \"%s\" SGID: %m ", execname);

  /* Now we can drop the privilege of that group.  */
  const int count = 64;
  gid_t groups[count];
  int ngroups = getgroups(count, groups);

  if (ngroups < 0)
    FAIL_UNSUPPORTED ("Could not get group list again for user %jd\n",
		      (intmax_t) getuid ());

  int n = 0;
  for (int i = 0; i < ngroups; i++)
    {
      if (groups[i] != gid)
	{
	  if (n != i)
	    groups[n] = groups[i];
	  n++;
	}
    }
  setgroups (n, groups);

  /* We have the binary, now spawn the subprocess.  Avoid using
     support_subprogram because we only want the program exit status, not the
     contents.  */

  char * const args[] = {execname, (char *) child_id, NULL};
  int status = support_subprogram_wait (args[0], args);

  free (execname);
  free (dirname);

  if (WIFEXITED (status))
    {
      if (WEXITSTATUS (status) == 0)
	return;
      else
	exit (WEXITSTATUS (status));
    }
  else
    FAIL_EXIT1 ("subprogram failed with status %d", status);
}

/* Returns true if a group with NAME has been found, and writes its
   GID to *TARGET.  */
static bool
find_sgid_group (gid_t *target, const char *name)
{
  /* Do not use getgrname_r because it does not work in statically
     linked binaries if the system libc is different.  */
  FILE *fp = fopen ("/etc/group", "rce");
  if (fp == NULL)
    return false;
  __fsetlocking (fp, FSETLOCKING_BYCALLER);

  bool ok = false;
  struct scratch_buffer buf;
  scratch_buffer_init (&buf);
  while (true)
    {
      struct group grp;
      struct group *result = NULL;
      int status = fgetgrent_r (fp, &grp, buf.data, buf.length, &result);
      if (status == 0 && result != NULL)
	{
	  if (strcmp (result->gr_name, name) == 0)
	    {
	      *target = result->gr_gid;
	      ok = true;
	      break;
	    }
	}
      else if (errno != ERANGE)
	break;
      else if (!scratch_buffer_grow (&buf))
	break;
    }
  scratch_buffer_free (&buf);
  fclose (fp);
  return ok;
}

void
support_capture_subprogram_self_sgid (const char *child_id)
{
  const int count = 64;
  gid_t groups[count];

  /* Get a GID which is not our current GID, but is present in the
     supplementary group list.  */
  int ret = getgroups (count, groups);
  if (ret < 0)
    FAIL_UNSUPPORTED("Could not get group list for user %jd\n",
		     (intmax_t) getuid ());

  gid_t current = getgid ();
  gid_t target = current;
  for (int i = 0; i < ret; ++i)
    {
      if (groups[i] != current)
	{
	  target = groups[i];
	  break;
	}
    }

  if (target == current)
    {
      /* If running as root, try to find a harmless group for SGID.  */
      if (getuid () != 0
	  || (!find_sgid_group (&target, "nogroup")
	      && !find_sgid_group (&target, "bin")
	      && !find_sgid_group (&target, "daemon")))
	FAIL_UNSUPPORTED("Could not find a suitable GID for user %jd\n",
			 (intmax_t) getuid ());
    }

  copy_and_spawn_sgid (child_id, target);
}

void
support_capture_subprocess_free (struct support_capture_subprocess *p)
{
  free (p->out.buffer);
  free (p->err.buffer);
}
