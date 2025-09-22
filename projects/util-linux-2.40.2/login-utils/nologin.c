/*
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "pathnames.h"
#include "fileutils.h"

/*
 * Always return EXIT_FAILURE (1), don't try to be smart!
 */

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Politely refuse a login.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -c, --command <command>  does nothing (for compatibility with su -c)\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fprintf(out, USAGE_MAN_TAIL("nologin(8)"));
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int c, fd = -1;
	struct stat st;
	enum {
		OPT_INIT_FILE = CHAR_MAX + 1,
		OPT_NOPROFILE,
		OPT_NORC,
		OPT_POSIX,
		OPT_RCFILE
	};
	static const struct option longopts[] = {
		{ "command",     required_argument, NULL, 'c'           },
		{ "init-file",   required_argument, NULL, OPT_INIT_FILE },
		{ "interactive", no_argument,       NULL, 'i'           },
		{ "login",       no_argument,       NULL, 'l'           },
		{ "noprofile",   no_argument,       NULL, OPT_NOPROFILE },
		{ "norc",        no_argument,       NULL, OPT_NORC      },
		{ "posix",       no_argument,       NULL, OPT_POSIX     },
		{ "rcfile",      required_argument, NULL, OPT_RCFILE    },
		{ "restricted",  no_argument,       NULL, 'r'           },
		{ "help",        no_argument,       NULL, 'h'           },
		{ "version",     no_argument,       NULL, 'V'           },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "c:ilrhV", longopts, NULL)) != -1) {
		switch (c) {
		case 'c':
		case OPT_INIT_FILE:
		case 'i':
		case 'l':
		case OPT_NOPROFILE:
		case OPT_NORC:
		case OPT_POSIX:
		case OPT_RCFILE:
		case 'r':
			/* Ignore well known shell command-line options */
			break;
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_FAILURE);	/* yes FAILURE! */
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	fd = open(_PATH_NOLOGIN_TXT, O_RDONLY);
	if (fd < 0)
		goto dflt;

	c = fstat(fd, &st);
	if (c < 0 || !S_ISREG(st.st_mode))
		goto dflt;
	else {
		ul_copy_file(fd, STDOUT_FILENO);
		close(fd);
		return EXIT_FAILURE;
	}

dflt:
	if (fd >= 0)
		close(fd);
	fprintf(stdout, _("This account is currently not available.\n"));
	return EXIT_FAILURE;
}
