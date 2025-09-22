/*
 * Copyright (c) 1980 Regents of the University of California.
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

/*
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * 	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 * 	modified to work correctly in multi-byte locales
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include "nls.h"
#include "strutils.h"
#include "c.h"
#include "widechar.h"
#include "closestream.h"

/*
COLRM removes unwanted columns from a file
	Jeff Schriebman  UC Berkeley 11-74
*/

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fprintf(out, _("\nUsage:\n"
		       " %s [startcol [endcol]]\n"),
		       program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Filter out the specified columns.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, _("%s reads from standard input and writes to standard output\n\n"),
		       program_invocation_short_name);
	fprintf(out, USAGE_MAN_TAIL("colrm(1)"));
	exit(EXIT_SUCCESS);
}

static int process_input(unsigned long first, unsigned long last)
{
	unsigned long ct = 0;
	wint_t c;
	unsigned long i;
	int w;
	int padding;

	for (;;) {
		c = getwc(stdin);
		if (c == WEOF)
			return 0;
		if (c == '\t')
			w = ((ct + 8) & ~7) - ct;
		else if (c == '\b')
			w = (ct ? ct - 1 : 0) - ct;
		else {
			w = wcwidth(c);
			if (w < 0)
				w = 0;
		}
		ct += w;
		if (c == '\n') {
			putwc(c, stdout);
			ct = 0;
			continue;

		}
		if (!first || ct < first) {
			putwc(c, stdout);
			continue;
		}
		break;
	}

	for (i = ct - w + 1; i < first; i++)
		putwc(' ', stdout);

	/* Loop getting rid of characters */
	while (!last || ct < last) {
		c = getwc(stdin);
		if (c == WEOF)
			return 0;
		if (c == '\n') {
			putwc(c, stdout);
			return 1;
		}
		if (c == '\t')
			ct = (ct + 8) & ~7;
		else if (c == '\b')
			ct = ct ? ct - 1 : 0;
		else {
			w = wcwidth(c);
			if (w < 0)
				w = 0;
			ct += w;
		}
	}

	padding = 0;

	/* Output last of the line */
	for (;;) {
		c = getwc(stdin);
		if (c == WEOF)
			break;
		if (c == '\n') {
			putwc(c, stdout);
			return 1;
		}
		if (padding == 0 && last < ct) {
			for (i = last; i < ct; i++)
				putwc(' ', stdout);
			padding = 1;
		}
		putwc(c, stdout);
	}
	return 0;
}

int main(int argc, char **argv)
{
	unsigned long first = 0, last = 0;
	int opt;

	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help",    no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((opt =
		getopt_long(argc, argv, "Vh", longopts,
			    NULL)) != -1)
		switch (opt) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	if (argc > 1)
		first = strtoul_or_err(*++argv, _("first argument"));
	if (argc > 2)
		last = strtoul_or_err(*++argv, _("second argument"));

	while (process_input(first, last))
		;

	fflush(stdout);
	return EXIT_SUCCESS;
}
