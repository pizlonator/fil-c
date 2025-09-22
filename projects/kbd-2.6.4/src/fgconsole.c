/*
 * fgconsole.c - aeb - 960123 - Print foreground console
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <sysexits.h>
#include <linux/vt.h>
#include <linux/serial.h>

#include "libcommon.h"

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...]\n"), get_progname());

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

int main(int argc, char **argv)
{
	struct vt_stat vtstat;
	int fd, vtno = -1, c, show_vt = 0;
	struct serial_struct sr;
	char *console = NULL;

	set_progname(argv[0]);
	setuplocale();

	const struct option long_opts[] = {
		{ "console", required_argument, NULL, 'C' },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ "next-available", no_argument, NULL, 'n' },
		{ NULL, 0, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-C, --console=DEV",    _("the console device to be used.") },
		{ "-n, --next-available", _("print number of next unallocated VT.") },
		{ "-V, --version",        _("print version number.")     },
		{ "-h, --help",           _("print this usage message.") },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, "C:Vhn", long_opts, NULL)) != EOF) {
		switch (c) {
			case 'C':
				console = optarg;
				break;
			case 'n':
				show_vt = 1;
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

	if ((fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (show_vt) {
		if ((ioctl(fd, VT_OPENQRY, &vtno) < 0) || vtno == -1) {
			kbd_error(2, errno, _("Couldn't read VTNO: "));
		}
		printf("%d\n", vtno);
		return EXIT_SUCCESS;
	}

	if (ioctl(fd, TIOCGSERIAL, &sr) == 0) {
		printf("serial\n");
		return EXIT_SUCCESS;
	}

	if (ioctl(fd, VT_GETSTATE, &vtstat)) {
		kbd_error(EXIT_FAILURE, errno, "fgconsole: VT_GETSTATE");
	}
	printf("%d\n", vtstat.v_active);
	return EXIT_SUCCESS;
}
