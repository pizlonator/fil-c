/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2012-2023 Karel Zak <kzak@redhat.com>
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "c.h"
#include "nls.h"
#include "partx.h"
#include "strutils.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s <disk device> <partition number>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Tell the kernel to forget about a specified partition.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("delpart(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int c, fd;

	static const struct option longopts[] = {
		{"help",    no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0},
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (c) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	if (argc != 3) {
		warnx(_("not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}


	if ((fd = open(argv[1], O_RDONLY)) < 0)
		err(EXIT_FAILURE, _("cannot open %s"), argv[1]);

	if (partx_del_partition(fd,
			strtou32_or_err(argv[2], _("invalid partition number argument"))))
		err(EXIT_FAILURE, _("failed to remove partition"));

	return EXIT_SUCCESS;
}
