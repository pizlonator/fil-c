/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * line - read one line
 *
 * Gunnar Ritter, Freiburg i. Br., Germany, December 2000.
 *
 * This command is deprecated.  The utility is in maintenance mode,
 * meaning we keep them in source tree for backward compatibility
 * only.  Do not waste time making this command better, unless the
 * fix is about security or other very critical issue.
 *
 * See Documentation/deprecated.txt for more information.
 */

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "widechar.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Read one line.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out,USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("line(1)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	wint_t c;
	int opt;
	int status = EXIT_SUCCESS;

	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((opt = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (opt) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	setvbuf(stdin, NULL, _IONBF, 0);
	for (;;) {
		c = getwchar();
		if (c == WEOF) {
			status = EXIT_FAILURE;
			break;
		}
		if (c == '\n')
			break;
		putwchar(c);
	}
	putwchar(L'\n');

	return status;
}
