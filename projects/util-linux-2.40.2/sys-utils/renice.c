/*
 * SPDX-License-Identifier: BSD-4-Clause-UC
 *
 * Copyright (c) 1983, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /* 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "nls.h"
#include "c.h"
#include "closestream.h"

static const char *idtype[] = {
	[PRIO_PROCESS]	= N_("process ID"),
	[PRIO_PGRP]	= N_("process group ID"),
	[PRIO_USER]	= N_("user ID"),
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %1$s [-n|--priority|--relative] <priority> [-p|--pid] <pid>...\n"
		" %1$s [-n|--priority|--relative] <priority>  -g|--pgrp <pgid>...\n"
		" %1$s [-n|--priority|--relative] <priority>  -u|--user <user>...\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Alter the priority of running processes.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -n <num>               specify the nice value\n"), out);
	fputs(_("                          If POSIXLY_CORRECT flag is set in environment\n"), out);
	fputs(_("                          then the priority is 'relative' to current\n"), out);
	fputs(_("                          process priority. Otherwise it is 'absolute'.\n"), out);
	fputs(_(" --priority <num>       specify the 'absolute' nice value\n"), out);
	fputs(_(" --relative <num>       specify the 'relative' nice value\n"), out);
	fputs(_(" -p, --pid              interpret arguments as process ID (default)\n"), out);
	fputs(_(" -g, --pgrp             interpret arguments as process group ID\n"), out);
	fputs(_(" -u, --user             interpret arguments as username or user ID\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(24));
	fprintf(out, USAGE_MAN_TAIL("renice(1)"));
	exit(EXIT_SUCCESS);
}

static int getprio(const int which, const int who, int *prio)
{
	errno = 0;
	*prio = getpriority(which, who);
	if (*prio == -1 && errno) {
		warn(_("failed to get priority for %d (%s)"), who, idtype[which]);
		return -errno;
	}
	return 0;
}

static int donice(const int which, const int who, const int prio, const int relative)
{
	int oldprio, newprio;

	if (getprio(which, who, &oldprio) != 0)
		return 1;

	newprio = prio; // if not relative, set absolute priority

	if (relative)
		newprio = oldprio + prio;

	if (setpriority(which, who, newprio) < 0) {
		warn(_("failed to set priority for %d (%s)"), who, idtype[which]);
		return 1;
	}
	if (getprio(which, who, &newprio) != 0)
		return 1;
	printf(_("%d (%s) old priority %d, new priority %d\n"),
	       who, idtype[which], oldprio, newprio);
	return 0;
}

/*
 * Change the priority (the nice value) of processes
 * or groups of processes which are already running.
 */
int main(int argc, char **argv)
{
	int which = PRIO_PROCESS;
	int who = 0, prio, errs = 0;
	int relative = 0;
	char *endptr = NULL;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	argc--;
	argv++;

	if (argc == 1) {
		if (strcmp(*argv, "-h") == 0 ||
		    strcmp(*argv, "--help") == 0)
			usage();

		if (strcmp(*argv, "-v") == 0 ||
		    strcmp(*argv, "-V") == 0 ||
		    strcmp(*argv, "--version") == 0)
			print_version(EXIT_SUCCESS);
	}

	if (*argv)
	{
		if (strcmp(*argv, "-n") == 0)
		{
			// Fully conform to posix only if POSIXLY_CORRECT is
			// set in the environment. If not, use the absolute
			// value as it's been used (incorrectly) since 2009
			relative = getenv("POSIXLY_CORRECT") == NULL ? 0 : 1;
			argc--;
			argv++;
		}
		else if (strcmp(*argv, "--relative") == 0)
		{
			relative = 1;
			argc--;
			argv++;
		}
		else if (strcmp(*argv, "--priority") == 0) {
			relative = 0;
			argc--;
			argv++;
		}
	}

	if (argc < 2 || !*argv) {
		warnx(_("not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	prio = strtol(*argv, &endptr, 10);
	if (*endptr) {
		warnx(_("invalid priority '%s'"), *argv);
		errtryhelp(EXIT_FAILURE);
	}
	argc--;
	argv++;

	for (; argc > 0; argc--, argv++) {
		if (strcmp(*argv, "-g") == 0 || strcmp(*argv, "--pgrp") == 0) {
			which = PRIO_PGRP;
			continue;
		}
		if (strcmp(*argv, "-u") == 0 || strcmp(*argv, "--user") == 0) {
			which = PRIO_USER;
			continue;
		}
		if (strcmp(*argv, "-p") == 0 || strcmp(*argv, "--pid") == 0) {
			which = PRIO_PROCESS;
			continue;
		}
		if (which == PRIO_USER) {
			struct passwd *pwd = getpwnam(*argv);

			if (pwd != NULL)
				who = pwd->pw_uid;
			else
				who = strtol(*argv, &endptr, 10);
			if (who < 0 || *endptr) {
				warnx(_("unknown user %s"), *argv);
				errs = 1;
				continue;
			}
		} else {
			who = strtol(*argv, &endptr, 10);
			if (who < 0 || *endptr) {
				/* TRANSLATORS: The first %s is one of the above
				 * three ID names. Read: "bad value for %s: %s" */
				warnx(_("bad %s value: %s"), idtype[which], *argv);
				errs = 1;
				continue;
			}
		}
		errs |= donice(which, who, prio, relative);
	}
	return errs != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
