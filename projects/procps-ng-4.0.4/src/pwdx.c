/*
 * pwdx.c - print process working directory
 *
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2004-2006 Albert Cahalan
 * Copyright © 2004      Nicholas Miell
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "xalloc.h"
#include "fileutils.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] pid...\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("pwdx(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int check_pid_argument(char *input)
{
	int skip = 0;
	long pid;
	char *end = NULL;

	if (!strncmp("/proc/", input, 6))
		skip = 6;
	errno = 0;
	pid = strtol(input + skip, &end, 10);

	if (errno || input + skip == end || (end && *end))
		return 1;
	if (pid < 1)
		return 1;
	return 0;
}

int main(int argc, char *argv[])
{
	int ch;
	int retval = 0, i;
	ssize_t alloclen = 128;
	char *pathbuf;

	static const struct option longopts[] = {
		{"version", no_argument, 0, 'V'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, 0, 0}
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((ch = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (ch) {
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage(stderr);

	pathbuf = xmalloc(alloclen);

	for (i = 0; i < argc; i++) {
		char *s;
		ssize_t len, buflen;
		/* Constant 10 is the length of strings "/proc/" + "/cwd" */
		char *buf;
		buflen = 10 + strlen(argv[i]) + 1;
		buf = xmalloc(buflen);

		if (check_pid_argument(argv[i]))
			xerrx(EXIT_FAILURE, _("invalid process id: %s"),
			     argv[i]);
		/*
		 * At this point, all arguments are in the form
		 * /proc/NNNN or NNNN, so a simple check based on
		 * the first char is possible
		 */
		if (argv[i][0] != '/')
			snprintf(buf, buflen, "/proc/%s/cwd", argv[i]);
		else
			snprintf(buf, buflen, "%s/cwd", argv[i]);

		/*
		 * buf contains /proc/NNNN/cwd symlink name
		 * on entry, the target of that symlink on return
		 */
		while ((len = readlink(buf, pathbuf, alloclen)) == alloclen) {
			alloclen *= 2;
			pathbuf = xrealloc(pathbuf, alloclen);
		}
		free(buf);

		if (len < 0) {
			s = strerror(errno == ENOENT ? ESRCH : errno);
			retval = EXIT_FAILURE;
			fprintf(stderr, "%s: %s\n", argv[i], s);
			continue;
		} else {
			pathbuf[len] = 0;
			s = pathbuf;
		}

		printf("%s: %s\n", argv[i], s);
	}
	free(pathbuf);
	return retval;
}
