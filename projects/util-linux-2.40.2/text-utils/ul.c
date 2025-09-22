/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * modified by Kars de Jong <jongk@cs.utwente.nl>
 *	to use terminfo instead of termcap.
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 *	modified to work correctly in multi-byte locales
 */

#include <stdio.h>
#include <unistd.h>		/* for getopt(), isatty() */
#include <string.h>		/* for memset(), strcpy() */
#include <stdlib.h>		/* for getenv() */
#include <limits.h>		/* for INT_MAX */
#include <signal.h>		/* for signal() */
#include <errno.h>
#include <getopt.h>

#if defined(HAVE_NCURSESW_TERM_H)
# include <ncursesw/term.h>
#elif defined(HAVE_NCURSES_TERM_H)
# include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
# include <term.h>
#endif

#include "nls.h"
#include "xalloc.h"
#include "widechar.h"
#include "c.h"
#include "closestream.h"

#define	ESC	'\033'
#define	SO	'\016'
#define	SI	'\017'
#define	HFWD	'9'
#define	HREV	'8'
#define	FREV	'7'

enum {
	NORMAL_CHARSET	    = 0,	/* Must be zero, see initbuf() */
	ALTERNATIVE_CHARSET = 1 << 0,	/* Reverse */
	SUPERSCRIPT	    = 1 << 1,	/* Dim */
	SUBSCRIPT	    = 1 << 2,	/* Dim | Ul */
	UNDERLINE	    = 1 << 3,	/* Ul */
	BOLD		    = 1 << 4,	/* Bold */
};

struct term_caps {
	char *curs_up;
	char *curs_right;
	char *curs_left;
	char *enter_standout;
	char *exit_standout;
	char *enter_underline;
	char *exit_underline;
	char *enter_dim;
	char *enter_bold;
	char *enter_reverse;
	char *under_char;
	char *exit_attributes;
};

struct ul_char {
	wchar_t	c_char;
	int	c_width;
	char	c_mode;
};

struct ul_ctl {
	size_t column;
	size_t max_column;
	int half_position;
	int up_line;
	int mode;
	int current_mode;
	size_t buflen;
	struct ul_char *buf;
	unsigned int
		indicated_opt:1,
		must_use_uc:1,
		must_overstrike:1;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [<file> ...]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Do underlining.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -t, -T, --terminal TERMINAL  override the TERM environment variable\n"), out);
	fputs(_(" -i, --indicated              underlining is indicated via a separate line\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(30));

	fprintf(out, USAGE_MAN_TAIL("ul(1)"));

	exit(EXIT_SUCCESS);
}

static void need_column(struct ul_ctl *ctl, size_t new_max)
{
	ctl->max_column = new_max;

	while (new_max >= ctl->buflen) {
		ctl->buflen *= 2;
		ctl->buf = xreallocarray(ctl->buf, ctl->buflen, sizeof(struct ul_char));
	}
}

static void set_column(struct ul_ctl *ctl, size_t column)
{
	ctl->column = column;

	if (ctl->max_column < ctl->column)
		need_column(ctl, ctl->column);
}

static void init_buffer(struct ul_ctl *ctl)
{
	if (ctl->buf == NULL) {
		/* First time. */
		ctl->buflen = BUFSIZ;
		ctl->buf = xcalloc(ctl->buflen, sizeof(struct ul_char));
	} else
		/* assumes NORMAL_CHARSET == 0 */
		memset(ctl->buf, 0, sizeof(struct ul_char) * ctl->max_column);

	set_column(ctl, 0);
	ctl->max_column = 0;
	ctl->mode &= ALTERNATIVE_CHARSET;
}

static void init_term_caps(struct ul_ctl *ctl, struct term_caps *const tcs)
{
	tcs->curs_up		= tigetstr("cuu1");
	tcs->curs_right		= tigetstr("cuf1");
	tcs->curs_left		= tigetstr("cub1");
	if (tcs->curs_left == NULL)
		tcs->curs_left = "\b";

	tcs->enter_standout	= tigetstr("smso");
	tcs->exit_standout	= tigetstr("rmso");
	tcs->enter_underline	= tigetstr("smul");
	tcs->exit_underline	= tigetstr("rmul");
	tcs->enter_dim		= tigetstr("dim");
	tcs->enter_bold		= tigetstr("bold");
	tcs->enter_reverse	= tigetstr("rev");
	tcs->exit_attributes	= tigetstr("sgr0");

	if (!tcs->enter_bold && tcs->enter_reverse)
		tcs->enter_bold	= tcs->enter_reverse;

	if (!tcs->enter_bold && tcs->enter_standout)
		tcs->enter_bold	= tcs->enter_standout;

	if (!tcs->enter_underline && tcs->enter_standout) {
		tcs->enter_underline = tcs->enter_standout;
		tcs->exit_underline = tcs->exit_standout;
	}

	if (!tcs->enter_dim && tcs->enter_standout)
		tcs->enter_dim = tcs->enter_standout;

	if (!tcs->enter_reverse && tcs->enter_standout)
		tcs->enter_reverse = tcs->enter_standout;

	if (!tcs->exit_attributes && tcs->exit_standout)
		tcs->exit_attributes = tcs->exit_standout;

	/*
	 * Note that we use REVERSE for the alternate character set,
	 * not the as/ae capabilities.  This is because we are modeling
	 * the model 37 teletype (since that's what nroff outputs) and
	 * the typical as/ae is more of a graphics set, not the greek
	 * letters the 37 has.
	 */
	tcs->under_char = tigetstr("uc");
	ctl->must_use_uc = (tcs->under_char && !tcs->enter_underline);

	if ((tigetflag("os") && tcs->enter_bold == NULL) ||
	    (tigetflag("ul") && tcs->enter_underline == NULL
			     && tcs->under_char == NULL))
		ctl->must_overstrike = 1;
}

static void sig_handler(int signo __attribute__((__unused__)))
{
	_exit(EXIT_SUCCESS);
}

static int ul_putwchar(int c)
{
	if (putwchar(c) == WEOF)
		return EOF;
	return c;
}

static void print_line(char *line)
{
	if (line == NULL)
		return;
	tputs(line, STDOUT_FILENO, ul_putwchar);
}

static void ul_setmode(struct ul_ctl *ctl, struct term_caps const *const tcs,
		       int new_mode)
{
	if (!ctl->indicated_opt) {
		if (ctl->current_mode != NORMAL_CHARSET && new_mode != NORMAL_CHARSET)
			ul_setmode(ctl, tcs, NORMAL_CHARSET);

		switch (new_mode) {
		case NORMAL_CHARSET:
			switch (ctl->current_mode) {
			case NORMAL_CHARSET:
				break;
			case UNDERLINE:
				print_line(tcs->exit_underline);
				break;
			default:
				/* This includes standout */
				print_line(tcs->exit_attributes);
				break;
			}
			break;
		case ALTERNATIVE_CHARSET:
			print_line(tcs->enter_reverse);
			break;
		case SUPERSCRIPT:
			/*
			 * This only works on a few terminals.
			 * It should be fixed.
			 */
			print_line(tcs->enter_underline);
			print_line(tcs->enter_dim);
			break;
		case SUBSCRIPT:
			print_line(tcs->enter_dim);
			break;
		case UNDERLINE:
			print_line(tcs->enter_underline);
			break;
		case BOLD:
			print_line(tcs->enter_bold);
			break;
		default:
			/*
			 * We should have some provision here for multiple modes
			 * on at once.  This will have to come later.
			 */
			print_line(tcs->enter_standout);
			break;
		}
	}
	ctl->current_mode = new_mode;
}

static void indicate_attribute(struct ul_ctl *ctl)
{
	size_t i;
	wchar_t *buf = xcalloc(ctl->max_column + 1, sizeof(wchar_t));
	wchar_t *p = buf;

	for (i = 0; i < ctl->max_column; i++) {
		switch (ctl->buf[i].c_mode) {
		case NORMAL_CHARSET:	*p++ = ' '; break;
		case ALTERNATIVE_CHARSET:	*p++ = 'g'; break;
		case SUPERSCRIPT:	*p++ = '^'; break;
		case SUBSCRIPT:	*p++ = 'v'; break;
		case UNDERLINE:	*p++ = '_'; break;
		case BOLD:	*p++ = '!'; break;
		default:	*p++ = 'X'; break;
		}
	}

	for (*p = ' '; *p == ' '; p--)
		*p = 0;

	fputws(buf, stdout);
	putwchar('\n');
	free(buf);
}

static void output_char(struct ul_ctl *ctl, struct term_caps const *const tcs,
			wint_t c, int width)
{
	int i;

	putwchar(c);
	if (ctl->must_use_uc && (ctl->current_mode & UNDERLINE)) {
		for (i = 0; i < width; i++)
			print_line(tcs->curs_left);
		for (i = 0; i < width; i++)
			print_line(tcs->under_char);
	}
}

/*
 * For terminals that can overstrike, overstrike underlines and bolds.
 * We don't do anything with halfline ups and downs, or Greek.
 */
static void overstrike(struct ul_ctl *ctl)
{
	size_t i;
	wchar_t *buf = xcalloc(ctl->max_column + 1, sizeof(wchar_t));
	wchar_t *p = buf;
	int had_bold = 0;

	/* Set up overstrike buffer */
	for (i = 0; i < ctl->max_column; i++) {
		switch (ctl->buf[i].c_mode) {
		case NORMAL_CHARSET:
		default:
			*p++ = ' ';
			break;
		case UNDERLINE:
			*p++ = '_';
			break;
		case BOLD:
			*p++ = ctl->buf[i].c_char;
			if (1 < ctl->buf[i].c_width)
				i += ctl->buf[i].c_width - 1;
			had_bold = 1;
			break;
		}
	}

	putwchar('\r');
	for (*p = ' '; *p == ' '; p--)
		*p = 0;
	fputws(buf, stdout);

	if (had_bold) {
		putwchar('\r');
		for (p = buf; *p; p++)
			putwchar(*p == '_' ? ' ' : *p);
		putwchar('\r');
		for (p = buf; *p; p++)
			putwchar(*p == '_' ? ' ' : *p);
	}
	free(buf);
}

static void flush_line(struct ul_ctl *ctl, struct term_caps const *const tcs)
{
	int last_mode;
	size_t i;
	int had_mode = 0;

	last_mode = NORMAL_CHARSET;
	for (i = 0; i < ctl->max_column; i++) {
		if (ctl->buf[i].c_mode != last_mode) {
			had_mode = 1;
			ul_setmode(ctl, tcs, ctl->buf[i].c_mode);
			last_mode = ctl->buf[i].c_mode;
		}
		if (ctl->buf[i].c_char == '\0') {
			if (ctl->up_line)
				print_line(tcs->curs_right);
			else
				output_char(ctl, tcs, ' ', 1);
		} else
			output_char(ctl, tcs, ctl->buf[i].c_char, ctl->buf[i].c_width);
		if (1 < ctl->buf[i].c_width)
			i += ctl->buf[i].c_width - 1;
	}
	if (last_mode != NORMAL_CHARSET)
		ul_setmode(ctl, tcs, NORMAL_CHARSET);
	if (ctl->must_overstrike && had_mode)
		overstrike(ctl);
	putwchar('\n');
	if (ctl->indicated_opt && had_mode)
		indicate_attribute(ctl);
	fflush(stdout);
	if (ctl->up_line)
		ctl->up_line--;
	init_buffer(ctl);
}

static void forward(struct ul_ctl *ctl, struct term_caps const *const tcs)
{
	int old_column, old_maximum;

	old_column = ctl->column;
	old_maximum = ctl->max_column;
	flush_line(ctl, tcs);
	set_column(ctl, old_column);
	ctl->max_column = old_maximum;
}

static void reverse(struct ul_ctl *ctl, struct term_caps const *const tcs)
{
	ctl->up_line++;
	forward(ctl, tcs);
	print_line(tcs->curs_up);
	print_line(tcs->curs_up);
	ctl->up_line++;
}

static int handle_escape(struct ul_ctl *ctl, struct term_caps const *const tcs, FILE *f)
{
	wint_t c;

	switch (c = getwc(f)) {
	case HREV:
		if (0 < ctl->half_position) {
			ctl->mode &= ~SUBSCRIPT;
			ctl->half_position--;
		} else if (ctl->half_position == 0) {
			ctl->mode |= SUPERSCRIPT;
			ctl->half_position--;
		} else {
			ctl->half_position = 0;
			reverse(ctl, tcs);
		}
		return 0;
	case HFWD:
		if (ctl->half_position < 0) {
			ctl->mode &= ~SUPERSCRIPT;
			ctl->half_position++;
		} else if (ctl->half_position == 0) {
			ctl->mode |= SUBSCRIPT;
			ctl->half_position++;
		} else {
			ctl->half_position = 0;
			forward(ctl, tcs);
		}
		return 0;
	case FREV:
		reverse(ctl, tcs);
		return 0;
	default:
		/* unknown escape */
		ungetwc(c, f);
		return 1;
	}
}

static void filter(struct ul_ctl *ctl, struct term_caps const *const tcs, FILE *f)
{
	wint_t c;
	int i, width;

	while ((c = getwc(f)) != WEOF) {
		switch (c) {
		case '\b':
			set_column(ctl, ctl->column && 0 < ctl->column ? ctl->column - 1 : 0);
			continue;
		case '\t':
			set_column(ctl, (ctl->column + 8) & ~07);
			continue;
		case '\r':
			set_column(ctl, 0);
			continue;
		case SO:
			ctl->mode |= ALTERNATIVE_CHARSET;
			continue;
		case SI:
			ctl->mode &= ~ALTERNATIVE_CHARSET;
			continue;
		case ESC:
			if (handle_escape(ctl, tcs, f)) {
				c = getwc(f);
				errx(EXIT_FAILURE,
				     _("unknown escape sequence in input: %o, %o"), ESC, c);
			}
			continue;
		case '_':
			if (ctl->buf[ctl->column].c_char || ctl->buf[ctl->column].c_width < 0) {
				while (ctl->buf[ctl->column].c_width < 0 && 0 < ctl->column)
					ctl->column--;
				width = ctl->buf[ctl->column].c_width;
				for (i = 0; i < width; i++)
					ctl->buf[ctl->column++].c_mode |= UNDERLINE | ctl->mode;
				set_column(ctl, 0 < ctl->column ? ctl->column : 0);
				continue;
			}
			ctl->buf[ctl->column].c_char = '_';
			ctl->buf[ctl->column].c_width = 1;
			/* fallthrough */
		case ' ':
			set_column(ctl, ctl->column + 1);
			continue;
		case '\n':
			flush_line(ctl, tcs);
			continue;
		case '\f':
			flush_line(ctl, tcs);
			putwchar('\f');
			continue;
		default:
			if (!iswprint(c))
				/* non printable */
				continue;
			width = wcwidth(c);
			need_column(ctl, ctl->column + width);
			if (ctl->buf[ctl->column].c_char == '\0') {
				ctl->buf[ctl->column].c_char = c;
				for (i = 0; i < width; i++)
					ctl->buf[ctl->column + i].c_mode = ctl->mode;
				ctl->buf[ctl->column].c_width = width;
				for (i = 1; i < width; i++)
					ctl->buf[ctl->column + i].c_width = -1;
			} else if (ctl->buf[ctl->column].c_char == '_') {
				ctl->buf[ctl->column].c_char = c;
				for (i = 0; i < width; i++)
					ctl->buf[ctl->column + i].c_mode |= UNDERLINE | ctl->mode;
				ctl->buf[ctl->column].c_width = width;
				for (i = 1; i < width; i++)
					ctl->buf[ctl->column + i].c_width = -1;
			} else if ((wint_t) ctl->buf[ctl->column].c_char == c) {
				for (i = 0; i < width; i++)
					ctl->buf[ctl->column + i].c_mode |= BOLD | ctl->mode;
			} else {
				width = ctl->buf[ctl->column].c_width;
				for (i = 0; i < width; i++)
					ctl->buf[ctl->column + i].c_mode = ctl->mode;
			}
			set_column(ctl, ctl->column + width);
			continue;
		}
	}
	if (ctl->max_column)
		flush_line(ctl, tcs);
}

int main(int argc, char **argv)
{
	int c, ret, opt_terminal = 0;
	char *termtype;
	struct term_caps tcs = { 0 };
	struct ul_ctl ctl = { .current_mode = NORMAL_CHARSET };
	FILE *f;

	static const struct option longopts[] = {
		{ "terminal",	required_argument,	NULL, 't' },
		{ "indicated",	no_argument,		NULL, 'i' },
		{ "version",	no_argument,		NULL, 'V' },
		{ "help",	no_argument,		NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	termtype = getenv("TERM");

	while ((c = getopt_long(argc, argv, "it:T:Vh", longopts, NULL)) != -1) {
		switch (c) {

		case 't':
		case 'T':
			/* for nroff compatibility */
			termtype = optarg;
			opt_terminal = 1;
			break;
		case 'i':
			ctl.indicated_opt = 1;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	setupterm(termtype, STDOUT_FILENO, &ret);
	switch (ret) {
	case 1:
		break;
	default:
		warnx(_("trouble reading terminfo"));
		/* fallthrough */
	case 0:
		if (opt_terminal)
			warnx(_("terminal `%s' is not known, defaulting to `dumb'"),
				termtype);
		setupterm("dumb", STDOUT_FILENO, (int *)0);
		break;
	}

	init_term_caps(&ctl, &tcs);
	init_buffer(&ctl);

	if (optind == argc)
		filter(&ctl, &tcs, stdin);
	else {
		for (; optind < argc; optind++) {
			f = fopen(argv[optind], "r");
			if (!f)
				err(EXIT_FAILURE, _("cannot open %s"), argv[optind]);
			filter(&ctl, &tcs, f);
			fclose(f);
		}
	}

	free(ctl.buf);
	del_curterm(cur_term);
	return EXIT_SUCCESS;
}
