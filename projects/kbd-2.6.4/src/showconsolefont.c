/* showfont.c - aeb, 940207 - updated 2001-02-06 */
/* renamed to showconsolefont.c to avoid clash with the X showfont */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/ioctl.h>

#include "libcommon.h"
#include "kfont.h"

/*
 * Showing the font is nontrivial mostly because testing whether
 * we are in utf8 mode cannot be done in an easy and clean way.
 * So, we set up things here in such a way that it does not matter
 * whether we are in utf8 mode.
 */

unsigned short obuf[E_TABSZ], nbuf[E_TABSZ];
struct unimapdesc ounimap, nunimap;
int fd           = 0;
int have_obuf    = 0;
int have_ounimap = 0;

static void __attribute__((noreturn))
leave(struct kfont_context *ctx, int n)
{
	if (have_obuf && kfont_put_uniscrnmap(ctx, fd, obuf)) {
		kbd_warning(0, _("failed to restore original translation table"));
		n = EXIT_FAILURE;
	}
	if (have_ounimap && kfont_put_unicodemap(ctx, fd, NULL, &ounimap)) {
		kbd_warning(0, _("failed to restore original unimap"));
		n = EXIT_FAILURE;
	}
	exit(n);
}

static void
settrivialscreenmap(struct kfont_context *ctx)
{
	unsigned short i;

	if (kfont_get_uniscrnmap(ctx, fd, obuf))
		exit(1);
	have_obuf = 1;

	for (i = 0; i < E_TABSZ; i++)
		nbuf[i] = i;

	if (kfont_put_uniscrnmap(ctx, fd, nbuf)) {
		kbd_error(EXIT_FAILURE, 0, _("cannot change translation table"));
	}
}

static void
getoldunicodemap(struct kfont_context *ctx)
{
	struct unimapdesc descr;

	if (kfont_get_unicodemap(ctx, fd, &descr))
		leave(ctx, EXIT_FAILURE);
	ounimap = descr;
	have_ounimap = 1;
}

#define BASE 041 /* ' '+1 */

static void
setnewunicodemap(struct kfont_context *ctx, unsigned int *list, int cnt)
{
	unsigned short i;

	if (!nunimap.entry_ct) {
		nunimap.entry_ct = 512;

		nunimap.entries  = malloc(nunimap.entry_ct * sizeof(struct unipair));
		if (!nunimap.entries)
			kbd_error(EX_OSERR, errno, "malloc");
	}
	for (i = 0; i < 512; i++) {
		nunimap.entries[i].fontpos = i;
		nunimap.entries[i].unicode = 0;
	}
	for (i = 0; i < cnt; i++)
		nunimap.entries[list[i]].unicode = (unsigned short) (BASE + i);

	if (kfont_put_unicodemap(ctx, fd, NULL, &nunimap))
		leave(ctx, EXIT_FAILURE);
}

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...]\n"), get_progname());
	fprintf(stderr, _("(probably after loading a font with `setfont font')\n"));

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
	int c, ret;
	unsigned int cols, rows, nr, n, i, j, k;
	int mode;
	const char *space, *sep;
	char *console = NULL;
	unsigned int list[64];
	int lth, info = 0;

	set_progname(argv[0]);
	setuplocale();

	const char *const short_opts = "C:ivVh";
	const struct option long_opts[] = {
		{ "console", required_argument, NULL, 'C' },
		{ "info",    no_argument, NULL, 'i' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "help",    no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-C, --console=DEV", _("the console device to be used.") },
		{ "-i, --info",        _("don't print out the font table, just show: ROWSxCOLSxCOUNT and exit.") },
		{ "-v, --verbose",     _("be more verbose.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	struct kfont_context *kfont;

	if ((ret = kfont_init(get_progname(), &kfont)) < 0)
		return -ret;

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'C':
				console = optarg;
				break;
			case 'i':
				info = 1;
				break;
			case 'v':
				kfont_inc_verbosity(kfont);
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

	if (optind != argc)
		usage(EX_USAGE, opthelp);

	if ((fd = getfd(console)) < 0)
		kbd_error(EX_OSERR, 0, _("Couldn't get a file descriptor referring to the console."));

	if (ioctl(fd, KDGKBMODE, &mode)) {
		kbd_warning(errno, _("Unable to read keyboard mode"));
		leave(kfont, EX_OSERR);
	}
	if (mode == K_UNICODE)
		space = "\xef\x80\xa0"; /* U+F020 (direct-to-font space) */
	else
		space = " ";

	if (info) {
		nr = rows = cols = 0;

		ret = kfont_get_font(kfont, fd, NULL, &nr, &rows, &cols, NULL);
		if (ret != 0)
			leave(kfont, EXIT_FAILURE);

		if (kfont_get_verbosity(kfont)) {
			printf(_("Character count: %u\n"), nr);
			printf(_("Font width     : %u\n"), rows);
			printf(_("Font height    : %u\n"), cols);
		} else
			printf("%dx%dx%d\n", rows, cols, nr);
		leave(kfont, EXIT_SUCCESS);
	}

	settrivialscreenmap(kfont);
	getoldunicodemap(kfont);

	n = kfont_get_fontsize(kfont, fd);
	if (kfont_get_verbosity(kfont))
		printf(_("Showing %d-char font\n\n"), n);
	cols = ((n > 256) ? 32 : 16);
	nr   = 64 / cols;
	rows = (n + cols - 1) / cols;
	sep  = ((cols == 16) ? "%1$s%1$s" : "%1$s");

	for (i = 0; i < rows; i++) {
		if (i % nr == 0) {
			lth = 0;
			for (k = i; k < i + nr; k++)
				for (j              = 0; j < cols; j++)
					list[lth++] = k + j * rows;
			setnewunicodemap(kfont, list, lth);
		}
		printf("%1$s%1$s%1$s%1$s", space);
		for (j = 0; j < cols && i + j * rows < n; j++) {
			printf("%c", BASE + (i % nr) * cols + j);
			printf(sep, space);
			if (j % 8 == 7)
				printf(sep, space);
		}
		putchar('\n');
		if (i % 8 == 7)
			putchar('\n');
		fflush(stdout);
	}

	leave(kfont, EXIT_SUCCESS);
	return EXIT_SUCCESS;
}
