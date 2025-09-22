/* loadkeys.c
 *
 * This file is part of kbd project.
 * Copyright (C) 1993  Risto Kankkunen.
 * Copyright (C) 1993  Eugene G. Crosser.
 * Copyright (C) 1994-2007  Andries E. Brouwer.
 * Copyright (C) 2007-2012  Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * This file is covered by the GNU General Public License,
 * which should be included with kbd as the file COPYING.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>
#include <sys/ioctl.h>

#include "libcommon.h"

#include "paths.h"
#include "keymap.h"

static const char *const dirpath1[] = {
	DATADIR "/" KEYMAPDIR "/**",
	KERNDIR "/",
	NULL
};
static const char *const suffixes[] = {
	"",
	".kmap",
	".map",
	NULL
};

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...] [mapfile...]\n"), get_progname());

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
	fprintf(stderr, _("Default keymap: %s\n"), DEFMAP);
	fprintf(stderr, "\n");
	fprintf(stderr, _("Report bugs to authors.\n"));
	fprintf(stderr, "\n");

	exit(rc);
}

int main(int argc, char *argv[])
{
	int options = 0;

	const char *const *dirpath;
	const char *dirpath2[] = { NULL, NULL };

	struct lk_ctx *ctx;
	lk_flags flags = 0;

	int c, i, rc = -1;
	int fd = -1;
	int kbd_mode;
	int kd_mode;
	char *console = NULL;
	char *ev;
	struct kbdfile_ctx *fctx;
	struct kbdfile *fp = NULL;

	set_progname(argv[0]);
	setuplocale();

	const char *const short_opts = "abcC:dhmpsuqvV";
	const struct option long_opts[] = {
		{ "console", required_argument, NULL, 'C' },
		{ "ascii", no_argument, NULL, 'a' },
		{ "bkeymap", no_argument, NULL, 'b' },
		{ "clearcompose", no_argument, NULL, 'c' },
		{ "default", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "mktable", no_argument, NULL, 'm' },
		{ "parse", no_argument, NULL, 'p' },
		{ "clearstrings", no_argument, NULL, 's' },
		{ "unicode", no_argument, NULL, 'u' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-C, --console=DEV",  _("the console device to be used.") },
		{ "-a, --ascii",        _("force conversion to ASCII.") },
		{ "-b, --bkeymap",      _("output a binary keymap to stdout.") },
		{ "-c, --clearcompose", _("clear kernel compose table.") },
		{ "-d, --default",      _("load default.") },
		{ "-m, --mktable",      _("output a 'defkeymap.c' to stdout.") },
		{ "-p, --parse",        _("search and parse keymap without action.") },
		{ "-s, --clearstrings", _("clear kernel string table.") },
		{ "-u, --unicode",      _("force conversion to Unicode.") },
		{ "-q, --quiet",        _("suppress all normal output.") },
		{ "-v, --verbose",      _("be more verbose.") },
		{ "-V, --version",      _("print version number.")     },
		{ "-h, --help",         _("print this usage message.") },
		{ NULL, NULL }
	};

	enum options {
		OPT_A = (1 << 1),
		OPT_B = (1 << 2),
		OPT_D = (1 << 3),
		OPT_M = (1 << 4),
		OPT_U = (1 << 5),
		OPT_P = (1 << 6)
	};

	ctx = lk_init();
	if (!ctx) {
		exit(EXIT_FAILURE);
	}

	if (!(fctx = kbdfile_context_new()))
		kbd_error(EXIT_FAILURE, errno, _("Unable to create kbdfile context"));

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'a':
				options |= OPT_A;
				break;
			case 'b':
				options |= OPT_B;
				break;
			case 'c':
				flags |= LK_FLAG_CLEAR_COMPOSE;
				break;
			case 'C':
				console = optarg;
				break;
			case 'd':
				options |= OPT_D;
				break;
			case 'm':
				options |= OPT_M;
				break;
			case 'p':
				options |= OPT_P;
				break;
			case 's':
				flags |= LK_FLAG_CLEAR_STRINGS;
				break;
			case 'u':
				options |= OPT_U;
				flags |= LK_FLAG_UNICODE_MODE;
				flags |= LK_FLAG_PREFER_UNICODE;
				break;
			case 'q':
				lk_set_log_priority(ctx, LOG_ERR);
				break;
			case 'v':
				lk_set_log_priority(ctx, LOG_INFO);
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

	if ((options & OPT_U) && (options & OPT_A))
		kbd_error(EXIT_FAILURE, 0, _("Options %s and %s are mutually exclusive."),
				"--unicode", "--ascii");

	if (!(options & OPT_M) && !(options & OPT_B)) {
		/* get console */
		if ((fd = getfd(console)) < 0)
			kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

		/* check whether the keyboard is in Unicode mode */
		if (ioctl(fd, KDGKBMODE, &kbd_mode))
			kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard mode"));
		if (ioctl(fd, KDGETMODE, &kd_mode))
			kbd_error(EXIT_FAILURE, errno, _("Unable to read console mode"));

		if (kbd_mode == K_UNICODE) {
			if (options & OPT_A) {
				kbd_warning(0,
				        _("Warning: loading non-Unicode keymap on Unicode console\n"
				          "    (perhaps you want to do `kbd_mode -a'?)"));
			} else {
				flags |= LK_FLAG_PREFER_UNICODE;
			}

			/* reset -u option if keyboard is in K_UNICODE anyway */
			flags ^= LK_FLAG_UNICODE_MODE;

		} else if (options & OPT_U && kd_mode != KD_GRAPHICS) {
			kbd_warning(0,
			        _("Warning: loading Unicode keymap on non-Unicode console\n"
			          "    (perhaps you want to do `kbd_mode -u'?)"));
		}
	}

	lk_set_parser_flags(ctx, flags);

	dirpath = dirpath1;
	if ((ev = getenv("LOADKEYS_KEYMAP_PATH")) != NULL) {
		dirpath2[0] = ev;
		dirpath     = dirpath2;
	}

	if (options & OPT_D) {
		if (!(fp = kbdfile_new(fctx)))
			kbd_error(EXIT_FAILURE, 0, _("Unable to create kbdfile instance: %m"));

		/* first read default map - search starts in . */
		if (kbdfile_find(DEFMAP, dirpath, suffixes, fp))
			kbd_error(EXIT_FAILURE, 0, _("Unable to find file: %s"), DEFMAP);

		rc = lk_parse_keymap(ctx, fp);
		kbdfile_free(fp);

		if (rc == -1)
			goto fail;

	} else if (optind == argc) {
		if (!(fp = kbdfile_new(fctx)))
			kbd_error(EXIT_FAILURE, 0, _("Unable to create kbdfile instance: %m"));

		kbdfile_set_file(fp, stdin);
		kbdfile_set_pathname(fp, "<stdin>");

		rc = lk_parse_keymap(ctx, fp);
		kbdfile_free(fp);

		if (rc == -1)
			goto fail;
	}

	for (i = optind; argv[i]; i++) {
		if (!(fp = kbdfile_new(fctx))) {
			kbd_error(EXIT_FAILURE, 0, _("Unable to create kbdfile instance: %m"));
		}

		if (!strcmp(argv[i], "-")) {
			kbdfile_set_file(fp, stdin);
			kbdfile_set_pathname(fp, "<stdin>");

		} else if (kbdfile_find(argv[i], dirpath, suffixes, fp)) {
			kbd_warning(0, _("Unable to open file: %s: %m"), argv[i]);
			goto fail;
		}

		rc = lk_parse_keymap(ctx, fp);
		kbdfile_free(fp);

		if (rc == -1)
			goto fail;
	}

	if (!(options & OPT_P)) {
		if (options & OPT_B) {
			rc = lk_dump_bkeymap(ctx, stdout);
		} else if (options & OPT_M) {
			rc = lk_dump_ctable(ctx, stdout);
		} else {
			rc = lk_load_keymap(ctx, fd, kbd_mode);
		}
	}

fail:
	lk_free(ctx);
	kbdfile_context_free(fctx);

	if (fd >= 0)
		close(fd);

	if (rc < 0)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
