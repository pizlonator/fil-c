/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2000 Werner Almesberger
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"

#define pivot_root(new_root,put_old) syscall(SYS_pivot_root,new_root,put_old)

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] new_root put_old\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Change the root filesystem.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("pivot_root(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int ch;
	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((ch = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (ch) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	if (argc != 3) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
	if (pivot_root(argv[1], argv[2]) < 0)
		err(EXIT_FAILURE, _("failed to change root from `%s' to `%s'"),
		    argv[1], argv[2]);

	return EXIT_SUCCESS;
}
