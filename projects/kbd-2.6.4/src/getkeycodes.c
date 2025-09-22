/*
 * call: getkeycodes
 *
 * aeb, 941108
 */
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

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
	int fd, c;
	unsigned int sc, sc0;
	struct kbkeycode a;

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

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
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

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	/* Old kernels don't support changing scancodes below SC_LIM. */
	a.scancode = 0;
	a.keycode  = 0;
	if (ioctl(fd, KDGETKEYCODE, &a)) {
		sc0 = 89;
	} else
		for (sc0 = 1; sc0 <= 88; sc0++) {
			a.scancode = sc0;
			a.keycode  = 0;
			if (ioctl(fd, KDGETKEYCODE, &a) || a.keycode != sc0)
				break;
		}

	printf(_("Plain scancodes xx (hex) versus keycodes (dec)\n"));

	if (sc0 == 89) {
		printf(_("0 is an error; "
		         "for 1-88 (0x01-0x58) scancode equals keycode\n"));
	} else if (sc0 > 1) {
		printf(_("for 1-%d (0x01-0x%02x) scancode equals keycode\n"),
		       sc0 - 1, sc0 - 1);
	}

	for (sc = (sc0 & ~7U); sc < 256; sc++) {
		if (sc == 128) {
			printf("\n\n");
			printf(_("Escaped scancodes e0 xx (hex)\n"));
		}
		if (sc % 8 == 0) {
			if (sc < 128)
				printf("\n 0x%02x: ", sc);
			else
				printf("\ne0 %02x: ", sc - 128);
		}

		if (sc < sc0) {
			printf(" %3d", sc);
			continue;
		}

		a.scancode = sc;
		a.keycode  = 0;
		if (ioctl(fd, KDGETKEYCODE, &a) == 0) {
			printf(" %3d", a.keycode);
			continue;
		}
		if (errno == EINVAL) {
			printf("   -");
			continue;
		}
		kbd_error(EXIT_FAILURE, errno, _("failed to get keycode for scancode 0x%x: "
		                                 "ioctl KDGETKEYCODE"),
		          sc);
		exit(1);
	}
	printf("\n");
	return EXIT_SUCCESS;
}
