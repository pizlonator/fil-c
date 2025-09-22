/*
 * fadvise - utility to use the posix_fadvise(2)
 *
 * Copyright (C) 2022 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"

static const struct advice {
	const char *name;
	int num;
} advices [] = {
	{ "normal",     POSIX_FADV_NORMAL,     },
	{ "sequential", POSIX_FADV_SEQUENTIAL, },
	{ "random",     POSIX_FADV_RANDOM,     },
	{ "noreuse",    POSIX_FADV_NOREUSE,    },
	{ "willneeded", POSIX_FADV_WILLNEED,   },
	{ "dontneed",   POSIX_FADV_DONTNEED,   },
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] file\n"), program_invocation_short_name);
	fprintf(out, _(" %s [options] --fd|-d file-descriptor\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --advice <advice> applying advice to the file (default: \"dontneed\")\n"), out);
	fputs(_(" -l, --length <num>    length for range operations, in bytes\n"), out);
	fputs(_(" -o, --offset <num>    offset for range operations, in bytes\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(23));

	fputs(_("\nAvailable values for advice:\n"), out);
	for (i = 0; i < ARRAY_SIZE(advices); i++) {
		fprintf(out, "  %s\n",
			advices[i].name);
	}

	fprintf(out, USAGE_MAN_TAIL("fadvise(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char ** argv)
{
	int c;
	int rc;
	bool do_close = false;

	int fd = -1;
	off_t offset = 0;
	off_t len = 0;
	int advice = POSIX_FADV_DONTNEED;

	static const struct option longopts[] = {
		{ "advice",     required_argument, NULL, 'a' },
		{ "fd",         required_argument, NULL, 'd' },
		{ "length",     required_argument, NULL, 'l' },
		{ "offset",     required_argument, NULL, 'o' },
		{ "version",    no_argument,       NULL, 'V' },
		{ "help",	no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long (argc, argv, "a:d:hl:o:V", longopts, NULL)) != -1) {
		switch (c) {
		case 'a':
			advice = -1;
			for (size_t i = 0; i < ARRAY_SIZE(advices); i++) {
				if (strcmp(optarg, advices[i].name) == 0) {
					advice = advices[i].num;
					break;
				}
			}
			if (advice == -1)
				errx(EXIT_FAILURE, "invalid advice argument: '%s'", optarg);
			break;
		case 'd':
			fd = strtos32_or_err(optarg,
					     _("invalid fd argument"));
			break;
		case 'l':
			len = strtosize_or_err(optarg,
					       _("invalid length argument"));
			break;
		case 'o':
			offset = strtosize_or_err(optarg,
						  _("invalid offset argument"));
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind == argc && fd == -1) {
		warnx(_("no file specified"));
		errtryhelp(EXIT_FAILURE);
	}

	if (argc - optind > 0 && fd != -1) {
		warnx(_("specify either file descriptor or file name"));
		errtryhelp(EXIT_FAILURE);
	}

	if (argc - optind > 1) {
		warnx(_("specify one file descriptor or file name"));
		errtryhelp(EXIT_FAILURE);
	}

	if (fd == -1) {
		fd = open(argv[optind], O_RDONLY);
		if (fd < 0)
			err(EXIT_FAILURE, _("cannot open %s"), argv[optind]);
		do_close = true;
	}

	rc = posix_fadvise(fd,
			   offset, len,
			   advice);
	if (rc != 0)
		warnx(_("failed to advise: %s"), strerror(rc));

	if (do_close)
		close(fd);

	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
