/*
 * disalloc.c - aeb - 940501 - Disallocate virtual terminal(s)
 * Renamed deallocvt.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <getopt.h>
#include <sysexits.h>

#include "libcommon.h"

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;
	fprintf(stderr, _("Usage: %s [option...] [N ...]\n"), get_progname());
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

int main(int argc, char *argv[])
{
	int fd, num, i;

	if (argc < 1) /* unlikely */
		return EXIT_FAILURE;

	set_progname(argv[0]);
	setuplocale();

	const char *const short_opts = "hV";
	const struct option long_opts[] = {
		{ "help",    no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	const struct kbd_help opthelp[] = {
		{ "-h, --help",    _("print this usage message.") },
		{ "-V, --version", _("print version number.")     },
		{ NULL, NULL }
	};

	while ((i = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (i) {
			case 'V':
				print_version_and_exit();
				break;
			case 'h':
				usage(EXIT_SUCCESS, opthelp);
			case '?':
				usage(EX_USAGE, opthelp);
		}
	}

	for (i = optind; i < argc; i++) {
		if (!isdigit(argv[i][0])) {
			kbd_warning(0, _("Unrecognized argument: %s"), argv[i]);
			return EX_USAGE;
		}
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (argc == 1) {
		/* deallocate all unused consoles */
		if (ioctl(fd, VT_DISALLOCATE, 0)) {
			kbd_error(EXIT_FAILURE, errno, "ioctl VT_DISALLOCATE");
		}
	} else
		for (i = 1; i < argc; i++) {
			num = atoi(argv[i]);
			if (num == 0) {
				kbd_error(EXIT_FAILURE, 0, _("0: illegal VT number"));
			} else if (num == 1) {
				kbd_error(EXIT_FAILURE, 0, _("VT 1 is the console and cannot be deallocated"));
			} else if (ioctl(fd, VT_DISALLOCATE, num)) {
				kbd_error(EXIT_FAILURE, errno, _("could not deallocate console %d: "
				                                 "ioctl VT_DISALLOCATE"),
				          num);
			}
		}
	exit(EXIT_SUCCESS);
}
