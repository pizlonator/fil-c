/*
 * SPDX-FileCopyrightText: 2011       , Julian Pidancet
 * SPDX-FileCopyrightText: 2011       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <stdio.h>
#include <assert.h>
#include "defines.h"
#include "prototypes.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"

static void change_root (const char* newroot);

/*
 * process_root_flag - chroot if given the --root option
 *
 * This shall be called before accessing the passwd, group, shadow,
 * gshadow, useradd's default, login.defs files (non exhaustive list)
 * or authenticating the caller.
 *
 * The audit, syslog, or locale files shall be open before
 */
extern void process_root_flag (const char* short_opt, int argc, char **argv)
{
	/*
	 * Parse the command line options.
	 */
	int i;
	const char *newroot = NULL, *val;

	for (i = 0; i < argc; i++) {
		val = NULL;
		if (   (strcmp (argv[i], "--root") == 0)
		    || ((strncmp (argv[i], "--root=", 7) == 0)
			&& (val = argv[i] + 7))
		    || (strcmp (argv[i], short_opt) == 0)) {
			if (NULL != newroot) {
				fprintf (log_get_logfd(),
				         _("%s: multiple --root options\n"),
				         log_get_progname());
				exit (E_BAD_ARG);
			}

			if (val) {
				newroot = val;
			} else if (i + 1 == argc) {
				fprintf (log_get_logfd(),
				         _("%s: option '%s' requires an argument\n"),
				         log_get_progname(), argv[i]);
				exit (E_BAD_ARG);
			} else {
				newroot = argv[++ i];
			}
		}
	}

	if (NULL != newroot) {
		change_root (newroot);
	}
}

static void change_root (const char* newroot)
{
	/* Drop privileges */
	if (   (setregid (getgid (), getgid ()) != 0)
	    || (setreuid (getuid (), getuid ()) != 0)) {
		fprintf (log_get_logfd(), _("%s: failed to drop privileges (%s)\n"),
		         log_get_progname(), strerror (errno));
		exit (EXIT_FAILURE);
	}

	if ('/' != newroot[0]) {
		fprintf (log_get_logfd(),
		         _("%s: invalid chroot path '%s', only absolute paths are supported.\n"),
		         log_get_progname(), newroot);
		exit (E_BAD_ARG);
	}

	if (access (newroot, F_OK) != 0) {
		fprintf(log_get_logfd(),
		        _("%s: cannot access chroot directory %s: %s\n"),
		        log_get_progname(), newroot, strerror (errno));
		exit (E_BAD_ARG);
	}

	if (chroot (newroot) != 0) {
		fprintf(log_get_logfd(),
			        _("%s: unable to chroot to directory %s: %s\n"),
				log_get_progname(), newroot, strerror (errno));
		exit (E_BAD_ARG);
	}

	if (chdir ("/") != 0) {
		fprintf(log_get_logfd(),
			_("%s: cannot chdir in chroot directory %s: %s\n"),
		        log_get_progname(), newroot, strerror (errno));
		exit (E_BAD_ARG);
	}
}

