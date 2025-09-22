/*
 * dumpkeys.c
 *
 * derived from version 0.81 - aeb@cwi.nl
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include "ksyms.h"
#include "modifiers.h"

#include "libcommon.h"

static int fd;

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
	fprintf(stderr, _("Available charsets: "));
	lk_list_charsets(stderr);
	fprintf(stderr, "\n");

	fprintf(stderr, _("Available shapes:\n"
	                  "  2  - default output;\n"
	                  "  4  - one line for each keycode;\n"
	                  "  8  - one line for each (modifier,keycode) pair;\n"
	                  "  16 - one line for each keycode until 1st hole.\n"
	                 ));
	fprintf(stderr, "\n");
	fprintf(stderr, _("Report bugs to authors.\n"));
	fprintf(stderr, "\n");

	exit(rc);
}

int main(int argc, char *argv[])
{
	int c, rc;
	int kbd_mode;

	char long_info       = 0;
	char short_info      = 0;
	char numeric         = 0;
	lk_table_shape table = LK_SHAPE_DEFAULT;
	char funcs_only      = 0;
	char keys_only       = 0;
	char diac_only       = 0;
	char *console        = NULL;

	struct lk_ctx *ctx;

	set_progname(argv[0]);
	setuplocale();

	const char *short_opts = "hilvsnf1tkdS:c:C:V";
	const struct option long_opts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "short-info", no_argument, NULL, 'i' },
		{ "long-info", no_argument, NULL, 'l' },
		{ "numeric", no_argument, NULL, 'n' },
		{ "full-table", no_argument, NULL, 'f' },
		{ "separate-lines", no_argument, NULL, '1' },
		{ "shape", required_argument, NULL, 'S' },
		{ "funcs-only", no_argument, NULL, 't' },
		{ "keys-only", no_argument, NULL, 'k' },
		{ "compose-only", no_argument, NULL, 'd' },
		{ "charset", required_argument, NULL, 'c' },
		{ "console", required_argument, NULL, 'C' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-i, --short-info",       _("display information about keyboard driver.") },
		{ "-l, -s, --long-info",    _("display above and symbols known to loadkeys.") },
		{ "-n, --numeric",          _("display keytable in hexadecimal notation.") },
		{ "-f, --full-table",       _("don't use short-hand notations, one row per keycode.") },
		{ "-1, --separate-lines",   _("one line per (modifier,keycode) pair.") },
		{ "-S, --shape={2|4|8|16}", "" },
		{ "-t, --funcs-only",       _("display only the function key strings.") },
		{ "-k, --keys-only",        _("display only key bindings.") },
		{ "-d, --compose-only",     _("display only compose key combinations.") },
		{ "-c, --charset=CHARSET",  _("interpret character action codes to be from the specified character set.") },
		{ "-C, --console=DEV",      _("the console device to be used.") },
		{ "-v, --verbose",          _("be more verbose.") },
		{ "-V, --version",          _("print version number.")     },
		{ "-h, --help",             _("print this usage message.") },
		{ NULL, NULL }
	};

	ctx = lk_init();
	if (!ctx) {
		exit(EXIT_FAILURE);
	}

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'i':
				short_info = 1;
				break;
			case 's':
			case 'l':
				long_info = 1;
				break;
			case 'n':
				numeric = 1;
				break;
			case 'f':
				table = LK_SHAPE_FULL_TABLE;
				break;
			case '1':
				table = LK_SHAPE_SEPARATE_LINES;
				break;
			case 'S':
				table = atoi(optarg);
				break;
			case 't':
				funcs_only = 1;
				break;
			case 'k':
				keys_only = 1;
				break;
			case 'd':
				diac_only = 1;
				break;
			case 'v':
				lk_set_log_priority(ctx, LOG_INFO);
				break;
			case 'c':
				if ((lk_set_charset(ctx, optarg)) != 0) {
					fprintf(stderr, _("unknown charset %s - ignoring charset request\n"),
					        optarg);
					usage(EX_USAGE, opthelp);
				}
				printf("charset \"%s\"\n", optarg);
				break;
			case 'C':
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

	if (optind < argc)
		usage(EX_USAGE, opthelp);

	if ((fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	/* check whether the keyboard is in Unicode mode */
	if (ioctl(fd, KDGKBMODE, &kbd_mode))
		kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard mode"));

	if (kbd_mode == K_UNICODE) {
		lk_set_parser_flags(ctx, LK_FLAG_PREFER_UNICODE);
	}

	if ((rc = lk_kernel_keymap(ctx, fd)) < 0)
		goto fail;

	if (short_info || long_info) {
		lk_dump_summary(ctx, stdout, fd);

		if (long_info) {
			printf(_("Symbols recognized by %s:\n(numeric value, symbol)\n\n"),
			       get_progname());
			lk_dump_symbols(ctx, stdout);
		}
		exit(EXIT_SUCCESS);
	}

#ifdef KDGKBDIACR
	if (!diac_only) {
#endif
		if (!funcs_only) {
			lk_dump_keymap(ctx, stdout, table, numeric);
		}
#ifdef KDGKBDIACR
	}

	if (!funcs_only && !keys_only)
		lk_dump_diacs(ctx, stdout);
#endif

fail:
	lk_free(ctx);
	close(fd);

	if (rc < 0)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
