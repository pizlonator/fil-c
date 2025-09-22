/*
 * SPDX-FileCopyrightText: 2011       , Jonathan Nieder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "exitcodes.h"
#include "prototypes.h"

#include "shadowlog_internal.h"

int
run_command(const char *cmd, const char *argv[],
            /*@null@*/const char *envp[], int *restrict status)
{
	pid_t pid, wpid;

	if (NULL == envp) {
		envp = (const char **)environ;
	}

	(void) fflush (stdout);
	(void) fflush (shadow_logfd);

	pid = fork ();
	if (0 == pid) {
		(void) execve (cmd, (char * const *) argv,
		               (char * const *) envp);
		if (ENOENT == errno) {
			exit (E_CMD_NOTFOUND);
		}
		fprintf (shadow_logfd, "%s: cannot execute %s: %s\n",
		         shadow_progname, cmd, strerror (errno));
		exit (E_CMD_NOEXEC);
	} else if ((pid_t)-1 == pid) {
		fprintf (shadow_logfd, "%s: cannot execute %s: %s\n",
		         shadow_progname, cmd, strerror (errno));
		return -1;
	}

	do {
		wpid = waitpid (pid, status, 0);
		if ((pid_t)-1 == wpid && errno == ECHILD)
			break;
	} while (   ((pid_t)-1 == wpid && errno == EINTR)
	         || ((pid_t)-1 != wpid && wpid != pid));

	if ((pid_t)-1 == wpid) {
		fprintf (shadow_logfd, "%s: waitpid (status: %d): %s\n",
		         shadow_progname, *status, strerror (errno));
		return -1;
	}

	return 0;
}

