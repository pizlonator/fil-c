/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /* 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <getopt.h>
#include "hexdump.h"

#include "list.h"
#include "nls.h"
#include "c.h"
#include "colors.h"
#include "strutils.h"
#include "xalloc.h"
#include "closestream.h"

void hex_free(struct hexdump *);

int
parse_args(int argc, char **argv, struct hexdump *hex)
{
	int ch;
	int colormode = UL_COLORMODE_UNDEF;
	char *hex_offt = "\"%07.7_Ax\n\"";


	static const struct option longopts[] = {
		{"one-byte-octal", no_argument, NULL, 'b'},
		{"one-byte-hex", no_argument, NULL, 'X'},
		{"one-byte-char", no_argument, NULL, 'c'},
		{"canonical", no_argument, NULL, 'C'},
		{"two-bytes-decimal", no_argument, NULL, 'd'},
		{"two-bytes-octal", no_argument, NULL, 'o'},
		{"two-bytes-hex", no_argument, NULL, 'x'},
		{"format", required_argument, NULL, 'e'},
		{"format-file", required_argument, NULL, 'f'},
		{"color", optional_argument, NULL, 'L'},
		{"length", required_argument, NULL, 'n'},
		{"skip", required_argument, NULL, 's'},
		{"no-squeezing", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, no_argument, NULL, 0}
	};

	while ((ch = getopt_long(argc, argv, "bXcCde:f:L::n:os:vxhV", longopts, NULL)) != -1) {
		switch (ch) {
		case 'b':
			add_fmt(hex_offt, hex);
			add_fmt("\"%07.7_ax \" 16/1 \"%03o \" \"\\n\"", hex);
			break;
		case 'X':
			add_fmt("\"%07.7_Ax\n\"", hex);
			add_fmt("\"%07.7_ax \" 16/1 \" %02x \" \"\\n\"", hex);
			break;
		case 'c':
			add_fmt(hex_offt, hex);
			add_fmt("\"%07.7_ax \" 16/1 \"%3_c \" \"\\n\"", hex);
			break;
		case 'C':
			add_fmt("\"%08.8_Ax\n\"", hex);
			add_fmt("\"%08.8_ax  \" 8/1 \"%02x \" \"  \" 8/1 \"%02x \" ", hex);
			add_fmt("\"  |\" 16/1 \"%_p\" \"|\\n\"", hex);
			break;
		case 'd':
			add_fmt(hex_offt, hex);
			add_fmt("\"%07.7_ax \" 8/2 \"  %05u \" \"\\n\"", hex);
			break;
		case 'e':
			add_fmt(optarg, hex);
			break;
		case 'f':
			addfile(optarg, hex);
			break;
		case 'L':
			colormode = UL_COLORMODE_AUTO;
			if (optarg)
				colormode = colormode_or_err(optarg,
						_("unsupported color mode"));
                        break;
		case 'n':
			hex->length = strtosize_or_err(optarg, _("failed to parse length"));
			break;
		case 'o':
			add_fmt(hex_offt, hex);
			add_fmt("\"%07.7_ax \" 8/2 \" %06o \" \"\\n\"", hex);
			break;
		case 's':
			hex->skip = strtosize_or_err(optarg, _("failed to parse offset"));
			break;
		case 'v':
			vflag = ALL;
			break;
		case 'x':
			add_fmt(hex_offt, hex);
			add_fmt("\"%07.7_ax \" 8/2 \"   %04x \" \"\\n\"", hex);
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (list_empty(&hex->fshead)) {
		if (!strcmp(program_invocation_short_name, "hd")) {
			/* Canonical format */
			add_fmt("\"%08.8_Ax\n\"", hex);
			add_fmt("\"%08.8_ax  \" 8/1 \"%02x \" \"  \" 8/1 \"%02x \" ", hex);
			add_fmt("\"  |\" 16/1 \"%_p\" \"|\\n\"", hex);
		} else {
			add_fmt(hex_offt, hex);
			add_fmt("\"%07.7_ax \" 8/2 \"%04x \" \"\\n\"", hex);
		}
	}
	colors_init (colormode, "hexdump");
	return optind;
}

void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <file>...\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display file contents in hexadecimal, decimal, octal, or ascii.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -b, --one-byte-octal      one-byte octal display\n"), out);
	fputs(_(" -X, --one-byte-hex        one-byte hexadecimal display\n"), out);
	fputs(_(" -c, --one-byte-char       one-byte character display\n"), out);
	fputs(_(" -C, --canonical           canonical hex+ASCII display\n"), out);
	fputs(_(" -d, --two-bytes-decimal   two-byte decimal display\n"), out);
	fputs(_(" -o, --two-bytes-octal     two-byte octal display\n"), out);
	fputs(_(" -x, --two-bytes-hex       two-byte hexadecimal display\n"), out);
	fputs(_(" -L, --color[=<mode>]      interpret color formatting specifiers\n"), out);
	fprintf(out,
	        "                             %s\n", USAGE_COLORS_DEFAULT);
	fputs(_(" -e, --format <format>     format string to be used for displaying data\n"), out);
	fputs(_(" -f, --format-file <file>  file that contains format strings\n"), out);
	fputs(_(" -n, --length <length>     interpret only length bytes of input\n"), out);
	fputs(_(" -s, --skip <offset>       skip offset bytes from the beginning\n"), out);
	fputs(_(" -v, --no-squeezing        output identical lines\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(27));

	fputs(USAGE_ARGUMENTS, out);
	fprintf(out, USAGE_ARG_SIZE(_("<length> and <offset>")));

	fprintf(out, USAGE_MAN_TAIL("hexdump(1)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct list_head *p;
	struct hexdump_fs *tfs;
	int ret;

	struct hexdump *hex = xcalloc(1, sizeof (struct hexdump));
	hex->length = -1;
	INIT_LIST_HEAD(&hex->fshead);

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	argv += parse_args(argc, argv, hex);

	/* figure out the data block size */
	hex->blocksize = 0;
	list_for_each(p, &hex->fshead) {
		tfs = list_entry(p, struct hexdump_fs, fslist);
		if ((tfs->bcnt = block_size(tfs)) > hex->blocksize)
			hex->blocksize = tfs->bcnt;
	}

	/* rewrite the rules, do syntax checking */
	list_for_each(p, &hex->fshead)
		rewrite_rules(list_entry(p, struct hexdump_fs, fslist), hex);

	next(argv, hex);
	display(hex);

	ret = hex->exitval;
	hex_free(hex);

	return ret;
}

void hex_free(struct hexdump *hex)
{
	struct list_head *p, *pn, *q, *qn, *r, *rn, *s, *sn;
	struct hexdump_fs *fs;
	struct hexdump_fu *fu;
	struct hexdump_pr *pr;
	struct hexdump_clr *clr;

	list_for_each_safe(p, pn, &hex->fshead) {
		fs = list_entry(p, struct hexdump_fs, fslist);
		list_for_each_safe(q, qn, &fs->fulist) {
			fu = list_entry(q, struct hexdump_fu, fulist);
			list_for_each_safe(r, rn, &fu->prlist) {
				pr = list_entry(r, struct hexdump_pr, prlist);
				if (pr->colorlist) {
					list_for_each_safe(s, sn, pr->colorlist) {
						clr = list_entry (s, struct hexdump_clr, colorlist);
						free(clr->str);
						free(clr);
					}
				}
				free(pr->fmt);
				free(pr);
			}
			free(fu->fmt);
			free(fu);
		}
		free(fs);
	}
	free (hex);
}
