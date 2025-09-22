/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mkfs		A simple generic frontend for the mkfs program
 *		under Linux.  See the manual page for details.
 *
 * Copyright (C) David Engel, <david@ods.com>
 * Copyright (C) Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 * Copyright (C) Ron Sommeling, <sommel@sci.kun.nl>
 *
 * Mon Jul  1 18:52:58 1996: janl@math.uio.no (Nicolai Langfeldt):
 *	Incorporated fix by Jonathan Kamens <jik@annex-1-slip-jik.cam.ov.com>
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */

/*
 * This command is deprecated.  The utility is in maintenance mode,
 * meaning we keep them in source tree for backward compatibility
 * only.  Do not waste time making this command better, unless the
 * fix is about security or other very critical issue.
 *
 * See Documentation/deprecated.txt for more information.
 */

#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "xalloc.h"

#ifndef DEFAULT_FSTYPE
#define DEFAULT_FSTYPE	"ext2"
#endif

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [-t <type>] [fs-options] <device> [<size>]\n"),
		     program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Make a Linux filesystem.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, _(" -t, --type=<type>  filesystem type; when unspecified, ext2 is used\n"));
	fprintf(out, _("     fs-options     parameters for the real filesystem builder\n"));
	fprintf(out, _("     <device>       path to the device to be used\n"));
	fprintf(out, _("     <size>         number of blocks to be used on the device\n"));
	fprintf(out, _(" -V, --verbose      explain what is being done;\n"
		       "                      specifying -V more than once will cause a dry-run\n"));
	fprintf(out, USAGE_HELP_OPTIONS(20));

	fprintf(out, USAGE_MAN_TAIL("mkfs(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *progname;		/* name of executable to be called */
	char *fstype = NULL;
	int i, more = 0, verbose = 0;

	enum { VERSION_OPTION = CHAR_MAX + 1 };

	static const struct option longopts[] = {
		{"type", required_argument, NULL, 't'},
		{"version", no_argument, NULL, VERSION_OPTION},
		{"verbose", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	if (argc == 2 && !strcmp(argv[1], "-V"))
		print_version(EXIT_SUCCESS);

	/* Check commandline options. */
	opterr = 0;
	while ((more == 0)
	       && ((i = getopt_long(argc, argv, "Vt:h", longopts, NULL))
		   != -1))
		switch (i) {
		case 'V':
			verbose++;
			break;
		case 't':
			fstype = optarg;
			break;
		case 'h':
			usage();
		case VERSION_OPTION:
			print_version(EXIT_SUCCESS);
		default:
			optind--;
			more = 1;
			break;	/* start of specific arguments */
		}
	if (optind == argc) {
		warnx(_("no device specified"));
		errtryhelp(EXIT_FAILURE);
	}

	/* If -t wasn't specified, use the default */
	if (fstype == NULL)
		fstype = DEFAULT_FSTYPE;

	xasprintf(&progname, "mkfs.%s", fstype);
	argv[--optind] = progname;

	if (verbose) {
		printf(UTIL_LINUX_VERSION);
		i = optind;
		while (argv[i])
			printf("%s ", argv[i++]);
		printf("\n");
		if (verbose > 1)
			return EXIT_SUCCESS;
	}

	/* Execute the program */
	execvp(progname, argv + optind);
	err(EXIT_FAILURE, _("failed to execute %s"), progname);
}
