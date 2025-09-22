#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <getopt.h>
#include <sysexits.h>

#include "libcommon.h"

static const char *action = NULL;
static const char *value  = NULL;

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr,
			_("Usage: %1$s [option...] getmode [text|graphics]\n"
			  "   or: %1$s [option...] gkbmode [raw|xlate|mediumraw|unicode]\n"
			  "   or: %1$s [option...] gkbmeta [metabit|escprefix]\n"
			  "   or: %1$s [option...] gkbled  [scrolllock|numlock|capslock]\n"),
			get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _(
				"The utility allows to read and check various parameters\n"
				"of the keyboard and virtual console.\n"));

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
answer(const char *ans)
{
	if (value)
		return strcasecmp(value, ans) ? EXIT_FAILURE : EXIT_SUCCESS;

	printf("%s\n", ans);
	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int fd, mode, c;
	int rc = EXIT_FAILURE;
	char flags;
	const char *console = NULL;

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
		kbd_error(EXIT_FAILURE, 0, _("Not enough arguments."));
	}

	action = argv[optind++];

	if (optind < argc)
		value = argv[optind++];

	if ((fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (!strcasecmp("GETMODE", action)) {
		if (ioctl(fd, KDGETMODE, &mode) == -1)
			kbd_error(EXIT_FAILURE, errno, _("Unable to read console mode"));

		switch (mode) {
			case KD_TEXT:
				rc = answer("text");
				break;
			case KD_GRAPHICS:
				rc = answer("graphics");
				break;
		}

	} else if (!strcasecmp("GKBMODE", action)) {
		if (ioctl(fd, KDGKBMODE, &mode) == -1)
			kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard mode"));

		switch (mode) {
			case K_RAW:
				rc = answer("raw");
				break;
			case K_XLATE:
				rc = answer("xlate");
				break;
			case K_MEDIUMRAW:
				rc = answer("mediumraw");
				break;
			case K_UNICODE:
				rc = answer("unicode");
				break;
		}

	} else if (!strcasecmp("GKBMETA", action)) {
		if (ioctl(fd, KDGKBMETA, &mode) == -1)
			kbd_error(EXIT_FAILURE, errno, _("Unable to read meta key handling mode"));

		switch (mode) {
			case K_METABIT:
				rc = answer("metabit");
				break;
			case K_ESCPREFIX:
				rc = answer("escprefix");
				break;
		}

	} else if (!strcasecmp("GKBLED", action)) {
		if (ioctl(fd, KDGKBLED, &flags) == -1)
			kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard flags"));

		mode = (flags & 0x7);

		if (value) {
			if (((mode & LED_SCR) && !strcasecmp(value, "scrolllock")) ||
			    ((mode & LED_NUM) && !strcasecmp(value, "numlock")) ||
			    ((mode & LED_CAP) && !strcasecmp(value, "capslock")))
				rc = EXIT_SUCCESS;
		} else {
			printf("scrolllock:%s ", (mode & LED_SCR) ? "on" : "off");
			printf("numlock:%s ", (mode & LED_NUM) ? "on" : "off");
			printf("capslock:%s\n", (mode & LED_CAP) ? "on" : "off");
			rc = EXIT_SUCCESS;
		}

	} else {
		kbd_warning(0, _("Unrecognized argument: %s"), action);
	}

	close(fd);
	return rc;
}
