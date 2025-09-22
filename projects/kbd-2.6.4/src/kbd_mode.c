/*
 * kbd_mode: report and set keyboard mode - aeb, 940406
 * 
 * If you make \215A\201 an alias for "kbd_mode -a", and you are
 * in raw mode, then hitting F7 = (two keys) will return you to sanity.
 */
#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "libcommon.h"

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;
	fprintf(stderr, _("Usage: %s [option...]\n"), get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _("This utility reports or sets the keyboard mode.\n"));

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
fprint_mode(FILE *stream, int  mode)
{
	switch (mode) {
		case K_RAW:
			fprintf(stream, _("The keyboard is in raw (scancode) mode"));
			break;
		case K_MEDIUMRAW:
			fprintf(stream, _("The keyboard is in mediumraw (keycode) mode"));
			break;
		case K_XLATE:
			fprintf(stream, _("The keyboard is in the default (ASCII) mode"));
			break;
		case K_UNICODE:
			fprintf(stream, _("The keyboard is in Unicode (UTF-8) mode"));
			break;
		default:
			fprintf(stream, _("The keyboard is in some unknown mode"));
	}
	fprintf(stream, "\n");
}

int main(int argc, char *argv[])
{
	int fd, mode, orig_mode, c, n = 0, force = 0;
	char *console = NULL;

	set_progname(argv[0]);
	setuplocale();

	const char *short_opts = "auskfC:hV";
	const struct option long_opts[] = {
		{ "ascii",    no_argument,       NULL, 'a' },
		{ "keycode",  no_argument,       NULL, 'k' },
		{ "scancode", no_argument,       NULL, 's' },
		{ "unicode",  no_argument,       NULL, 'u' },
		{ "force",    no_argument,       NULL, 'f' },
		{ "console",  required_argument, NULL, 'C' },
		{ "help",     no_argument,       NULL, 'h' },
		{ "version",  no_argument,       NULL, 'V' },
		{ NULL,       0,                 NULL,  0  }
	};
	const struct kbd_help opthelp[] = {
		{ "-a, --ascii",       _("set ASCII mode.") },
		{ "-k, --keycode",     _("set keycode mode.") },
		{ "-s, --scancode",    _("set scancode mode.") },
		{ "-u, --unicode",     _("set UTF-8 mode.") },
		{ "-f, --force",       _("switch the mode even if it makes the keyboard unusable.") },
		{ "-C, --console=DEV", _("the console device to be used.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'a':
				if (n > 0)
					usage(EX_USAGE, opthelp);
				mode = K_XLATE;
				n++;
				break;
			case 'u':
				if (n > 0)
					usage(EX_USAGE, opthelp);
				mode = K_UNICODE;
				n++;
				break;
			case 's':
				if (n > 0)
					usage(EX_USAGE, opthelp);
				mode = K_RAW;
				n++;
				break;
			case 'k':
				if (n > 0)
					usage(EX_USAGE, opthelp);
				mode = K_MEDIUMRAW;
				n++;
				break;
			case 'f':
				force = 1;
				break;
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

	if ((fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (n == 0) {
		/* report mode */
		if (ioctl(fd, KDGKBMODE, &mode))
			kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard mode"));

		fprint_mode(stdout, mode);
		return EXIT_SUCCESS;
	}

	if (force == 0) {
		/* only perform safe mode switches */
		if (ioctl(fd, KDGKBMODE, &orig_mode))
			kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard mode"));

		if (mode == orig_mode) {
			/* skip mode change */
			return EXIT_SUCCESS;
		}

		if ((mode == K_XLATE && orig_mode != K_UNICODE) || (mode == K_UNICODE && orig_mode != K_XLATE)) {
			fprint_mode(stderr, orig_mode);
			fprintf(stderr, _("Changing to the requested mode may make "
				"your keyboard unusable, please use -f to force the change.\n"));
			return EXIT_FAILURE;
		}
	}
	if (ioctl(fd, KDSKBMODE, mode)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl KDSKBMODE");
	}

	return EXIT_SUCCESS;
}
