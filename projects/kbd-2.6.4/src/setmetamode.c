/*
 * setmetamode.c - aeb, 940130
 *
 * Call: setmetamode { metabit | escprefix }
 * and report the setting before and after.
 * Without arguments setmetamode will only report.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sysexits.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

#include "libcommon.h"

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...] [argument]\n"), get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _(
				"Arguments:\n"
				"  metabit     the keysym marked with the high bit set.\n"
				"  escprefix   specifies if pressing the meta (alt) key\n"
				"              generates an ESC (\\033) prefix followed by\n"
				"              the keysym.\n"
			 ));

	if (options) {
		int max = 0;

		fprintf(stderr, "\n");
		fprintf(stderr, _("Options:"));
		fprintf(stderr, "\n");

		for (h = options; h && h->opts; h++) {
			int len = (int) strlen(h->opts);
			if (max < len)
				max = len;
		}
		max += 2;

		for (h = options; h && h->opts; h++)
			fprintf(stderr, "  %-*s %s\n", max, h->opts, h->desc);
	}

	fprintf(stderr, "\n");
	fprintf(stderr, _("Report bugs to authors.\n"));
	fprintf(stderr, "\n");

	exit(rc);
}

static void
report(unsigned int meta)
{
	const char *s;

	switch (meta) {
		case K_METABIT:
			s = _("Meta key sets high order bit");
			break;
		case K_ESCPREFIX:
			s = _("Meta key gives Esc prefix");
			break;
		default:
			s = _("Strange mode for Meta key?");
	}
	printf("%s\n", s);
}

struct meta {
	const char *name;
	unsigned int val;
} metas[] = {
	{ "metabit", K_METABIT },
	{ "meta", K_METABIT },
	{ "bit", K_METABIT },
	{ "escprefix", K_ESCPREFIX },
	{ "esc", K_ESCPREFIX },
	{ "prefix", K_ESCPREFIX }
};

#define SIZE(a) (sizeof(a) / sizeof(a[0]))

int main(int argc, char **argv)
{
	unsigned int ometa, nmeta;
	struct meta *mp;
	int c;
	int fd = 0;
	char *console = NULL;

	set_progname(argv[0]);
	setuplocale();

	const char *short_opts = "C:hV";
	const struct option long_opts[] = {
		{ "console", required_argument, NULL, 'C' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'V' },
		{ NULL,      0,                 NULL,  0  }
	};
	const struct kbd_help opthelp[] = {
		{ "-C, --console=DEV", _("the console device to be used.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'C':
				if (optarg == NULL || optarg[0] == '\0')
					usage(EX_USAGE, opthelp);
				console = optarg;
				break;
			case 'V':
				print_version_and_exit();
				break;
			case 'h':
				usage(EXIT_SUCCESS, opthelp);
				break;
			case '?':
				usage(EX_USAGE, opthelp);
				break;
		}
	}

	if (console && (fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (ioctl(fd, KDGKBMETA, &ometa))
		kbd_error(EXIT_FAILURE, errno, _("Unable to read meta key handling mode"));

	if (optind == argc) {
		report(ometa);
		exit(EXIT_SUCCESS);
	}

	nmeta = 0; /* make gcc happy */
	for (mp = metas; (unsigned)(mp - metas) < SIZE(metas); mp++) {
		if (!strcmp(argv[1], mp->name)) {
			nmeta = mp->val;
			goto end;
		}
	}
	fprintf(stderr, _("Unrecognized argument: %s"), argv[1]);
	fprintf(stderr, "\n\n");
	usage(EXIT_FAILURE, opthelp);

end:
	printf(_("old state:    "));
	report(ometa);
	if (ioctl(fd, KDSKBMETA, nmeta)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl KDSKBMETA");
	}
	printf(_("new state:    "));
	report(nmeta);

	return EXIT_SUCCESS;
}
