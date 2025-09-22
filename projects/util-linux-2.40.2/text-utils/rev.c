/*-
 * Copyright (c) 1987, 1992 The Regents of the University of California.
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
 *
 * Modified for Linux by Charles Hannum (mycroft@gnu.ai.mit.edu)
 *                   and Brian Koehmstedt (bpk@gnu.ai.mit.edu)
 *
 * Wed Sep 14 22:26:00 1994: Patch from bjdouma <bjdouma@xs4all.nl> to handle
 *                           last line that has no newline correctly.
 * 3-Jun-1998: Patched by Nicolai Langfeldt to work better on Linux:
 *	Handle any-length-lines.  Code copied from util-linux' setpwnam.c
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 *	modified to work correctly in multi-byte locales
 * July 2010 - Davidlohr Bueso <dave@gnu.org>
 *      Fixed memory leaks (including Linux signal handling)
 *      Added some memory allocation error handling
 *      Lowered the default buffer size to 256, instead of 512 bytes
 *      Changed tab indentation to 8 chars for better reading the code
 */

#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "nls.h"
#include "xalloc.h"
#include "widechar.h"
#include "c.h"
#include "closestream.h"

static void sig_handler(int signo __attribute__ ((__unused__)))
{
	_exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fprintf(out, _("Usage: %s [options] [file ...]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Reverse lines characterwise.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("rev(1)"));

	exit(EXIT_SUCCESS);
}

static void reverse_str(wchar_t *str, size_t n)
{
	size_t i;

	for (i = 0; i < n / 2; ++i) {
		wchar_t tmp = str[i];
		str[i] = str[n - 1 - i];
		str[n - 1 - i] = tmp;
	}
}

static size_t read_line(wchar_t sep, wchar_t *str, size_t n, FILE *stream)
{
	size_t r = 0;
	while (r < n) {
		wint_t c = fgetwc(stream);
		if (c == WEOF)
			break;
		str[r++] = c;
		if ((wchar_t) c == sep)
			break;
	}
	return r;
}

static void write_line(wchar_t *str, size_t n, FILE *stream)
{
	for (size_t i = 0; i < n; i++)
		fputwc(str[i], stream);
}

int main(int argc, char *argv[])
{
	char const *filename = "stdin";
	wchar_t *buf;
	wchar_t sep = L'\n';
	size_t len, bufsiz = BUFSIZ;
	FILE *fp = stdin;
	int ch, rval = EXIT_SUCCESS;
	uintmax_t line;

	static const struct option longopts[] = {
		{ "zero",       no_argument,       NULL, '0' },
		{ "version",    no_argument,       NULL, 'V' },
		{ "help",       no_argument,       NULL, 'h' },
		{ NULL,         0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	while ((ch = getopt_long(argc, argv, "Vh0", longopts, NULL)) != -1)
		switch(ch) {
		case '0':
			sep = L'\0';
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	buf = xreallocarray(NULL, bufsiz, sizeof(wchar_t));

	do {
		if (*argv) {
			if ((fp = fopen(*argv, "r")) == NULL) {
				warn(_("cannot open %s"), *argv );
				rval = EXIT_FAILURE;
				++argv;
				continue;
			}
			filename = *argv++;
		}

		line = 0;
		while (!feof(fp)) {
			len = read_line(sep, buf, bufsiz, fp);

			/* This is my hack from setpwnam.c -janl */
			while (len == bufsiz && !feof(fp)) {
				/* Extend input buffer if it failed getting the whole line */
				/* So now we double the buffer size */
				bufsiz *= 2;

				buf = xreallocarray(buf, bufsiz, sizeof(wchar_t));

				/* And fill the rest of the buffer */
				len += read_line(sep, &buf[len], bufsiz/2, fp);
			}
			if (ferror(fp)) {
				warn("%s: %ju", filename, line);
				rval = EXIT_FAILURE;
				break;
			}
			if (len == 0)
				continue;

			reverse_str(buf, buf[len - 1] == sep ? len - 1 : len);
			write_line(buf, len, stdout);
			line++;
		}
		if (fp != stdin)
			fclose(fp);
	} while(*argv);

	free(buf);
	return rval;
}
