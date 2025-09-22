#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "libcommon.h"
#include "kfont.h"

#ifndef USE_LIBC
/* There is such function in libc5 but it doesn't work for me [libc 5.4.13] */
#include "wctomb.c"
#define wctomb our_wctomb
#endif

static int
ud_compar(const void *u1, const void *u2)
{
	unsigned short fp1 = ((struct unipair *)u1)->fontpos;
	unsigned short fp2 = ((struct unipair *)u2)->fontpos;
	return (int)fp1 - (int)fp2;
}

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
	int sortflag = 0;
	char mb[]    = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int mb_length;
	int fd, c, i;
	char *console = NULL;
	struct unimapdesc ud;

	set_progname(argv[0]);
	setuplocale();

	const char *const short_opts = "hVsC:";
	const struct option long_opts[] = {
		{ "sort",    no_argument,       NULL, 's' },
		{ "console", required_argument, NULL, 'C' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-s, --sort",        _("sort and merge elements.") },
		{ "-C, --console=DEV", _("the console device to be used.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 's':
				sortflag = 1;
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

	int ret;
	struct kfont_context *kfont;

	if ((ret = kfont_init(get_progname(), &kfont)) < 0)
		return -ret;

	if (kfont_get_unicodemap(kfont, fd, &ud))
		return EXIT_FAILURE;

	if (sortflag) {
		printf("# sorted kernel unimap - count=%d\n", ud.entry_ct);
		/* sort and merge entries */
		qsort(ud.entries, ud.entry_ct, sizeof(ud.entries[0]),
		      ud_compar);
		for (i = 0; i < ud.entry_ct; i++) {
			int fp = ud.entries[i].fontpos;
			printf("0x%03x\tU+%04x", fp, ud.entries[i].unicode);
			while (i + 1 < ud.entry_ct &&
			       ud.entries[i + 1].fontpos == fp)
				printf(" U+%04x", ud.entries[++i].unicode);
			printf("\n");
		}
	} else {
		printf("# kernel unimap - count=%d\n", ud.entry_ct);
		for (i = 0; i < ud.entry_ct; i++) {
			mb_length = wctomb(mb, ud.entries[i].unicode);
			mb[(mb_length > 6) ? 0 : mb_length] = 0;
			if (mb_length == 1 && !isprint(mb[0])) {
				mb[2] = 0;
				mb[1] = mb[0] + 0100;
				mb[0] = '^';
			}
			printf("0x%03x\tU+%04x\t# %s \n",
			       ud.entries[i].fontpos,
			       ud.entries[i].unicode, mb);
		}
	}

	return EXIT_SUCCESS;
}
