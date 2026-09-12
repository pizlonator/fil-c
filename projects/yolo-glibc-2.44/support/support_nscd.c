/* Support functions for nscd testing.
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

#include <support/nscd_test.h>

#include <libgen.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xspawn.h>
#include <support/xunistd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <nscd/nscd.h>
#include <nscd/nscd-client.h>

static pid_t nscd_pid;

void
support_nscd_copy_configuration (void)
{
  char *from = xasprintf ("%s/nscd/nscd.conf", support_srcdir_root);
  support_copy_file (from, _PATH_NSCDCONF);
  free (from);
}

void
support_nscd_start (void)
{
  /* The --shutdown directive used in support_nscd_stop has a UID check.  */
  if (getuid () != 0)
    FAIL_UNSUPPORTED ("nscd needs to run as root"
                      " (use test-container su directive)");

  /* Create required directories.  Without /var/db/nscd, the shared
     cache will not work.  */
  {
    char socket[] = _PATH_NSCDSOCKET;
    xmkdirp (dirname (socket), 0755);
  }
  {
    char var_db_passwd[] = _PATH_NSCD_PASSWD_DB;
    xmkdirp (dirname (var_db_passwd), 0755);
  }

  /* Create the directory for the persistent database files.  Without
     this, nscd cannot create the shared memory mappings.  */
  xmkdirp ("/var/db/nscd", 0755);

  char *nscd_path = xasprintf ("%s/nscd/nscd", support_objdir_root);
  char *args[] = { nscd_path, (char *) "--foreground",
                   (char *) "--debug", NULL };
  posix_spawn_file_actions_t file_actions;
  posix_spawn_file_actions_init (&file_actions);
  xposix_spawn_file_actions_adddup2 (&file_actions,
                                     STDOUT_FILENO, STDERR_FILENO);
  nscd_pid = xposix_spawn (nscd_path, &file_actions, NULL, args, NULL);
  posix_spawn_file_actions_destroy (&file_actions);
  free (nscd_path);

  /* Wait for the nscd socket to be come available.  Without that, the
     following tests will likely fallback to non-nscd NSS processing
     instead.  */
  while (true)
    {
      if (access (_PATH_NSCDSOCKET, F_OK) == 0)
        break;
      int status;
      int ret = waitpid (nscd_pid, &status, WNOHANG);
      if (ret < 0)
        FAIL_EXIT1 ("waitpid on nscd failed: %m");
      else if (ret > 0)
        FAIL_EXIT1 ("nscd exited with status %d", status);
      usleep (10 * 1000);
    }
}

void
support_nscd_stop (void)
{
  char *cmd = xasprintf ("%s/nscd/nscd --shutdown", support_objdir_root);
  TEST_COMPARE (system (cmd), 0);
  free (cmd);
  int status;
  xwaitpid (nscd_pid, &status, 0);
  TEST_COMPARE (status, 0);
}

void
support_nscd_invalidate (const char *database)
{
  char *cmd = xasprintf ("%s/nscd/nscd --invalidate %s",
                         support_objdir_root, database);
  TEST_COMPARE (system (cmd), 0);
  free (cmd);
}
