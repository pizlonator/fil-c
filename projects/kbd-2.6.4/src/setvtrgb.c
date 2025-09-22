#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <errno.h>
#include <sysexits.h>

#include "libcommon.h"

static unsigned char cmap[3 * 16];

/* Standard VGA terminal colors, matching those hardcoded in the Linux kernel's
 * drivers/char/vt.c
 */
unsigned char vga_colors[] = {
	0x00, 0x00, 0x00,
	0xaa, 0x00, 0x00,
	0x00, 0xaa, 0x00,
	0xaa, 0x55, 0x00,
	0x00, 0x00, 0xaa,
	0xaa, 0x00, 0xaa,
	0x00, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa,
	0x55, 0x55, 0x55,
	0xff, 0x55, 0x55,
	0x55, 0xff, 0x55,
	0xff, 0xff, 0x55,
	0x55, 0x55, 0xff,
	0xff, 0x55, 0xff,
	0x55, 0xff, 0xff,
	0xff, 0xff, 0xff,
};

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...] [vga|FILE|-]\n"), get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _(
				"If you use the FILE parameter, it can be either in decimal\n"
				"or hexadecimal format, and will be detected on runtime.\n"
				"\n"
				"Decimal FILE format should be exactly 3 lines of comma-separated\n"
				"decimal values for RED, GREEN, and BLUE.\n"
				"To seed a valid FILE:\n"
				"   cat /sys/module/vt/parameters/default_{red,grn,blu} > FILE\n"
				"\n"
				"Hexadecimal FILE format should be exactly 16 lines of hex triplets\n"
				"for RED, GREEN and BLUE, prefixed with a number sign (#).\n"
				"For example:\n"
				"   #000000\n"
				"   #AA0000\n"
				"   #00AA00\n"
				"And so on, for all the 16 colors.\n"
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
parse_dec_file(FILE *fd, const char *filename)
{
	int c;
	unsigned int rows, cols, val;

	for (rows = 0; rows < 3; rows++) {
		for (cols = 0; cols < 16; cols++) {
			if ((c = fscanf(fd, "%u", &val)) != 1) {
				if (c == EOF)
					kbd_error(EXIT_FAILURE, errno, "fscanf");

				kbd_error(EXIT_FAILURE, 0, _("Error: %s: Invalid value in field %u in line %u."),
				          filename, rows + 1, cols + 1);
			}

			cmap[rows + cols * 3] = (unsigned char)val;

			if (cols < 15 && fgetc(fd) != ',')
				kbd_error(EXIT_FAILURE, 0, _("Error: %s: Insufficient number of fields in line %u."),
				          filename, rows + 1);
		}

		if ((c = fgetc(fd)) == EOF)
			kbd_error(EXIT_FAILURE, 0, _("Error: %s: Line %u has ended unexpectedly."),
			          filename, rows + 1);

		if (c != '\n')
			kbd_error(EXIT_FAILURE, 0, _("Error: %s: Line %u is too long."),
			          filename, rows + 1);
	}
}

static void
parse_hex_file(FILE *fd, const char *filename)
{
	int c,l;
	unsigned int r, g, b;

	for (l = 0; l < 16; l++) {
		if ((c = fscanf(fd, "#%2x%2x%2x\n", &r, &g, &b)) != 3) {
			if (c == EOF)
				kbd_error(EXIT_FAILURE, 0, _("Error: %s: Insufficient number of colors/lines: %u"),
				          filename, l);

			kbd_error(EXIT_FAILURE, 0, _("Error: %s: Invalid value in line %u."),
			          filename, l + 1);
		}

		cmap[l * 3]     = (unsigned char)r;
		cmap[l * 3 + 1] = (unsigned char)g;
		cmap[l * 3 + 2] = (unsigned char)b;
	}
}

static void
parse_file(FILE *fd, const char *filename)
{
	int c = fgetc(fd);

	if (c == EOF)
		kbd_error(EXIT_FAILURE, 0, _("Error: %s: File ended unexpectedly."),
		          filename);
	ungetc(c, fd);

	if (c == '#')
		parse_hex_file(fd, filename);
	else
		parse_dec_file(fd, filename);
}

int main(int argc, char **argv)
{
	int c, fd;
	const char *file;
	unsigned char *colormap = cmap;
	FILE *f;
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

	if (optind == argc)
		usage(EX_USAGE, opthelp);

	file = argv[optind];

	if (!strcmp(file, "vga")) {
		colormap = vga_colors;

	} else if (!strcmp(file, "-")) {
		parse_file(stdin, "stdin");

	} else {
		if ((f = fopen(file, "r")) == NULL)
			kbd_error(EXIT_FAILURE, errno, "fopen");

		parse_file(f, file);
		fclose(f);
	}

	if ((fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	/* Apply the color map to the tty via ioctl */
	if (ioctl(fd, PIO_CMAP, colormap) == -1)
		kbd_error(EXIT_FAILURE, errno, "ioctl");

	close(fd);

	return EXIT_SUCCESS;
}
