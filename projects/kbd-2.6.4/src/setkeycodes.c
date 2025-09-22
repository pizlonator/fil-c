/*
 * call: setkeycode scancode keycode ...
 *  (where scancode is either xx or e0xx, given in hexadecimal,
 *   and keycode is given in decimal)
 *
 * aeb, 941108, 2004-01-11
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "libcommon.h"

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...] scancode keycode ...\n"), get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _("(where scancode is either xx or e0xx, given in hexadecimal,\n"
	                  "and keycode is given in decimal)\n"));

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

static int
str_to_uint(const char *str, int base, unsigned int *res)
{
	char *ep;
	long int v;

	errno = 0;
	v = strtol(str, &ep, base);

	if (*ep) {
		kbd_warning(0, _("error reading scancode"));
		return -1;
	}

	if (errno == ERANGE) {
		kbd_warning(0, _("Argument out of range: %s"), str);
		return -1;
	}

	if (v < 0) {
		kbd_warning(0, _("Argument must be positive: %s"), str);
		return -1;
	}

	if (v > UINT_MAX) {
		kbd_warning(0, "Argument is too big: %s", str);
		return -1;
	}

	*res = (unsigned int) v;
	return 0;
}

int main(int argc, char **argv)
{
	int fd, c;
	struct kbkeycode a;
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

	if (optind == argc) {
		kbd_warning(0, _("Not enough arguments."));
		usage(EX_USAGE, opthelp);
	}

	if ((fd = getfd(console)) < 0)
		kbd_error(EX_OSERR, 0, _("Couldn't get a file descriptor referring to the console."));

	while (argc > 2) {
		if (str_to_uint(argv[1], 16, &a.scancode) < 0)
			return EX_DATAERR;

		if (str_to_uint(argv[2], 0, &a.keycode) < 0)
			return EX_DATAERR;

		if (a.scancode >= 0xe000) {
			a.scancode -= 0xe000;
			a.scancode += 128; /* some kernels needed +256 */
		}
#if 0
		/* Test is OK up to 2.5.31--later kernels have more keycodes */
		if (a.scancode > 255 || a.keycode > 127)
			usage(_("code outside bounds"));

		/* Both fields are unsigned int, so can be large;
		   for current kernels the correct test might be
		     (a.scancode > 255 || a.keycode > 239)
		   but we can leave testing to the kernel. */
#endif
		if (ioctl(fd, KDSETKEYCODE, &a)) {
			kbd_error(EXIT_FAILURE, errno,
			          _("failed to set scancode %x to keycode %d: ioctl KDSETKEYCODE"),
			          a.scancode, a.keycode);
		}
		argc -= 2;
		argv += 2;
	}
	return EX_OK;
}
