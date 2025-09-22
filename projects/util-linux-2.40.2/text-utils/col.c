/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Rendell of the Memorial University of Newfoundland.
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
 * Wed Jun 22 22:15:41 1994, faith@cs.unc.edu: Added internationalization
 *                           patches from Andries.Brouwer@cwi.nl
 * Wed Sep 14 22:31:17 1994: patches from Carl Christofferson
 *                           (cchris@connected.com)
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * 	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 * 	modified to work correctly in multi-byte locales
 *
 */

/*
 * This command is deprecated.  The utility is in maintenance mode,
 * meaning we keep them in source tree for backward compatibility
 * only.  Do not waste time making this command better, unless the
 * fix is about security or other very critical issue.
 *
 * See Documentation/deprecated.txt for more information.
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "optutils.h"
#include "strutils.h"
#include "widechar.h"
#include "xalloc.h"

#define	SPACE	' '		/* space */
#define	BS	'\b'		/* backspace */
#define	NL	'\n'		/* newline */
#define	CR	'\r'		/* carriage return */
#define	TAB	'\t'		/* tab */
#define	VT	'\v'		/* vertical tab (aka reverse line feed) */

#define	ESC	'\033'		/* escape */
#define	RLF	'\a'		/* ESC-007 reverse line feed */
#define	RHLF	BS		/* ESC-010 reverse half-line feed */
#define	FHLF	TAB		/* ESC-011 forward half-line feed */

#define	SO	'\016'		/* activate the G1 character set */
#define	SI	'\017'		/* activate the G0 character set */

/* build up at least this many lines before flushing them out */
#define	BUFFER_MARGIN		32

/* number of lines to allocate */
#define	NALLOC			64

#if HAS_FEATURE_ADDRESS_SANITIZER || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
# define COL_DEALLOCATE_ON_EXIT
#endif

/* SI & SO charset mode */
enum {
	CS_NORMAL,
	CS_ALTERNATE
};

struct col_char {
	size_t		c_column;	/* column character is in */
	wchar_t		c_char;		/* character in question */
	int		c_width;	/* character width */

	uint8_t		c_set:1;	/* character set (currently only 2) */
};

struct col_line {
	struct col_char	*l_line;	/* characters on the line */
	struct col_line	*l_prev;	/* previous line */
	struct col_line	*l_next;	/* next line */
	size_t		l_lsize;	/* allocated sizeof l_line */
	size_t		l_line_len;	/* strlen(l_line) */
	size_t		l_max_col;	/* max column in the line */

	uint8_t		l_needs_sort:1;	/* set if chars went in out of order */
};

#ifdef COL_DEALLOCATE_ON_EXIT
/*
 * Free memory before exit when compiling LeakSanitizer.
 */
struct col_alloc {
	struct col_line	 *l;
	struct col_alloc *next;
};
#endif

struct col_ctl {
	struct col_line *lines;
	struct col_line *l;		/* current line */
	size_t max_bufd_lines;		/* max # lines to keep in memory */
	struct col_line *line_freelist;
	size_t nblank_lines;		/* # blanks after last flushed line */
#ifdef COL_DEALLOCATE_ON_EXIT
	struct col_alloc *alloc_root;	/* first of line allocations */
	struct col_alloc *alloc_head;	/* latest line allocation */
#endif
	unsigned int
		last_set:1,		/* char_set of last char printed */
		compress_spaces:1,	/* if doing space -> tab conversion */
		fine:1,			/* if `fine' resolution (half lines) */
		no_backspaces:1,	/* if not to output any backspaces */
		pass_unknown_seqs:1;	/* whether to pass unknown control sequences */
};

struct col_lines {
	struct col_char *c;
	wint_t ch;
	size_t adjust;
	size_t cur_col;
	ssize_t cur_line;
	size_t extra_lines;
	size_t max_line;
	size_t nflushd_lines;
	size_t this_line;

	unsigned int
		cur_set:1,
		warned:1;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fprintf(out, _(
		"\nUsage:\n"
		" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Filter out reverse line feeds from standard input.\n"), out);

	fprintf(out, _(
		"\nOptions:\n"
		" -b, --no-backspaces    do not output backspaces\n"
		" -f, --fine             permit forward half line feeds\n"
		" -p, --pass             pass unknown control sequences\n"
		" -h, --tabs             convert spaces to tabs\n"
		" -x, --spaces           convert tabs to spaces\n"
		" -l, --lines NUM        buffer at least NUM lines\n"
		));
	fprintf(out, " -H, --help             %s\n", USAGE_OPTSTR_HELP);
	fprintf(out, " -V, --version          %s\n", USAGE_OPTSTR_VERSION);

	fprintf(out, USAGE_MAN_TAIL("col(1)"));
	exit(EXIT_SUCCESS);
}

static inline void col_putchar(wchar_t ch)
{
	if (putwchar(ch) == WEOF)
		err(EXIT_FAILURE, _("write failed"));
}

/*
 * Print a number of newline/half newlines.  If fine flag is set, nblank_lines
 * is the number of half line feeds, otherwise it is the number of whole line
 * feeds.
 */
static void flush_blanks(struct col_ctl *ctl)
{
	int half = 0;
	ssize_t i, nb = ctl->nblank_lines;

	if (nb & 1) {
		if (ctl->fine)
			half = 1;
		else
			nb++;
	}
	nb /= 2;
	for (i = nb; --i >= 0;)
		col_putchar(NL);

	if (half) {
		col_putchar(ESC);
		col_putchar('9');
		if (!nb)
			col_putchar(CR);
	}
	ctl->nblank_lines = 0;
}

/*
 * Write a line to stdout taking care of space to tab conversion (-h flag)
 * and character set shifts.
 */
static void flush_line(struct col_ctl *ctl, struct col_line *l)
{
	struct col_char *c, *endc;
	size_t nchars = l->l_line_len, last_col = 0, this_col;

	if (l->l_needs_sort) {
		static struct col_char *sorted = NULL;
		static size_t count_size = 0, *count = NULL, sorted_size = 0;
		size_t i, tot;

		/*
		 * Do an O(n) sort on l->l_line by column being careful to
		 * preserve the order of characters in the same column.
		 */
		if (sorted_size < l->l_lsize) {
			sorted_size = l->l_lsize;
			sorted = xreallocarray(sorted, sorted_size, sizeof(struct col_char));
		}
		if (count_size <= l->l_max_col) {
			count_size = l->l_max_col + 1;
			count = xreallocarray(count, count_size, sizeof(size_t));
		}
		memset(count, 0, sizeof(size_t) * l->l_max_col + 1);
		for (i = nchars, c = l->l_line; c && 0 < i; i--, c++)
			count[c->c_column]++;

		/*
		 * calculate running total (shifted down by 1) to use as
		 * indices into new line.
		 */
		for (tot = 0, i = 0; i <= l->l_max_col; i++) {
			size_t save = count[i];
			count[i] = tot;
			tot += save;
		}

		for (i = nchars, c = l->l_line; 0 < i; i--, c++)
			sorted[count[c->c_column]++] = *c;
		c = sorted;
	} else
		c = l->l_line;

	while (0 < nchars) {
		this_col = c->c_column;
		endc = c;

		/* find last character */
		do {
			++endc;
		} while (0 < --nchars && this_col == endc->c_column);

		if (ctl->no_backspaces) {
			/* print only the last character */
			c = endc - 1;
			if (0 < nchars && endc->c_column < this_col + c->c_width)
				continue;
		}

		if (last_col < this_col) {
			/* tabs and spaces handling */
			ssize_t nspace = this_col - last_col;

			if (ctl->compress_spaces && 1 < nspace) {
				ssize_t ntabs;

				ntabs = this_col / 8 - last_col / 8;
				if (0 < ntabs) {
					nspace = this_col & 7;
					while (0 <= --ntabs)
						col_putchar(TAB);
				}
			}
			while (0 <= --nspace)
				col_putchar(SPACE);
			last_col = this_col;
		}

		for (;;) {
			/* SO / SI character set changing */
			if (c->c_set != ctl->last_set) {
				switch (c->c_set) {
				case CS_NORMAL:
					col_putchar(SI);
					break;
				case CS_ALTERNATE:
					col_putchar(SO);
					break;
				default:
					abort();
				}
				ctl->last_set = c->c_set;
			}

			/* output a character */
			col_putchar(c->c_char);

			/* rubout control chars from output */
			if (c + 1 < endc) {
				int i;

				for (i = 0; i < c->c_width; i++)
					col_putchar(BS);
			}

			if (endc <= ++c)
				break;
		}
		last_col += (c - 1)->c_width;
	}
}

static struct col_line *alloc_line(struct col_ctl *ctl)
{
	struct col_line *l;
	size_t i;

	if (!ctl->line_freelist) {
		l = xmalloc(sizeof(struct col_line) * NALLOC);
#ifdef COL_DEALLOCATE_ON_EXIT
		if (ctl->alloc_root == NULL) {
			ctl->alloc_root = xcalloc(1, sizeof(struct col_alloc));
			ctl->alloc_root->l = l;
			ctl->alloc_head = ctl->alloc_root;
		} else {
			ctl->alloc_head->next = xcalloc(1, sizeof(struct col_alloc));
			ctl->alloc_head = ctl->alloc_head->next;
			ctl->alloc_head->l = l;
		}
#endif
		ctl->line_freelist = l;
		for (i = 1; i < NALLOC; i++, l++)
			l->l_next = l + 1;
		l->l_next = NULL;
	}
	l = ctl->line_freelist;
	ctl->line_freelist = l->l_next;

	memset(l, 0, sizeof(struct col_line));
	return l;
}

static void free_line(struct col_ctl *ctl, struct col_line *l)
{
	l->l_next = ctl->line_freelist;
	ctl->line_freelist = l;
}

static void flush_lines(struct col_ctl *ctl, ssize_t nflush)
{
	struct col_line *l;

	while (0 <= --nflush) {
		l = ctl->lines;
		ctl->lines = l->l_next;
		if (l->l_line) {
			flush_blanks(ctl);
			flush_line(ctl, l);
		}
		ctl->nblank_lines++;
		free(l->l_line);
		free_line(ctl, l);
	}
	if (ctl->lines)
		ctl->lines->l_prev = NULL;
}

static int handle_not_graphic(struct col_ctl *ctl, struct col_lines *lns)
{
	switch (lns->ch) {
	case BS:
		if (lns->cur_col == 0)
			return 1;	/* can't go back further */
		if (lns->c)
			lns->cur_col -= lns->c->c_width;
		else
			lns->cur_col -= 1;
		return 1;
	case CR:
		lns->cur_col = 0;
		return 1;
	case ESC:
		switch (getwchar()) {	/* just ignore EOF */
		case RLF:
			lns->cur_line -= 2;
			break;
		case RHLF:
			lns->cur_line -= 1;
			break;
		case FHLF:
			lns->cur_line += 1;
			if (0 < lns->cur_line && lns->max_line < (size_t)lns->cur_line)
				lns->max_line = lns->cur_line;
			break;
		default:
			break;
		}
		return 1;
	case NL:
		lns->cur_line += 2;
		if (0 < lns->cur_line && lns->max_line < (size_t)lns->cur_line)
			lns->max_line = lns->cur_line;
		lns->cur_col = 0;
		return 1;
	case SPACE:
		lns->cur_col += 1;
		return 1;
	case SI:
		lns->cur_set = CS_NORMAL;
		return 1;
	case SO:
		lns->cur_set = CS_ALTERNATE;
		return 1;
	case TAB:		/* adjust column */
		lns->cur_col |= 7;
		lns->cur_col += 1;
		return 1;
	case VT:
		lns->cur_line -= 2;
		return 1;
	default:
		break;
	}
	if (iswspace(lns->ch)) {
		if (0 < wcwidth(lns->ch))
			lns->cur_col += wcwidth(lns->ch);
		return 1;
	}

	if (!ctl->pass_unknown_seqs)
		return 1;
	return 0;
}

static void update_cur_line(struct col_ctl *ctl, struct col_lines *lns)
{
	ssize_t nmove;

	lns->adjust = 0;
	nmove = lns->cur_line - lns->this_line;
	if (!ctl->fine) {
		/* round up to next line */
		if (lns->cur_line & 1) {
			lns->adjust = 1;
			nmove++;
		}
	}
	if (nmove < 0) {
		for (; nmove < 0 && ctl->l->l_prev; nmove++)
			ctl->l = ctl->l->l_prev;

		if (nmove) {
			if (lns->nflushd_lines == 0) {
				/*
				 * Allow backup past first line if nothing
				 * has been flushed yet.
				 */
				for (; nmove < 0; nmove++) {
					struct col_line *lnew = alloc_line(ctl);
					ctl->l->l_prev = lnew;
					lnew->l_next = ctl->l;
					ctl->l = ctl->lines = lnew;
					lns->extra_lines += 1;
				}
			} else {
				if (!lns->warned) {
					warnx(_("warning: can't back up %s."),
						  lns->cur_line < 0 ?
						    _("past first line") :
					            _("-- line already flushed"));
					lns->warned = 1;
				}
				lns->cur_line -= nmove;
			}
		}
	} else {
		/* may need to allocate here */
		for (; 0 < nmove && ctl->l->l_next; nmove--)
			ctl->l = ctl->l->l_next;

		for (; 0 < nmove; nmove--) {
			struct col_line *lnew = alloc_line(ctl);
			lnew->l_prev = ctl->l;
			ctl->l->l_next = lnew;
			ctl->l = lnew;
		}
	}

	lns->this_line = lns->cur_line + lns->adjust;
	nmove = lns->this_line - lns->nflushd_lines;

	if (0 < nmove && ctl->max_bufd_lines + BUFFER_MARGIN <= (size_t)nmove) {
		lns->nflushd_lines += nmove - ctl->max_bufd_lines;
		flush_lines(ctl, nmove - ctl->max_bufd_lines);
	}
}

static void parse_options(struct col_ctl *ctl, int argc, char **argv)
{
	static const struct option longopts[] = {
		{ "no-backspaces", no_argument,		NULL, 'b' },
		{ "fine",	   no_argument,		NULL, 'f' },
		{ "pass",	   no_argument,		NULL, 'p' },
		{ "tabs",	   no_argument,		NULL, 'h' },
		{ "spaces",	   no_argument,		NULL, 'x' },
		{ "lines",	   required_argument,	NULL, 'l' },
		{ "version",	   no_argument,		NULL, 'V' },
		{ "help",	   no_argument,		NULL, 'H' },
		{ NULL, 0, NULL, 0 }
	};
	static const ul_excl_t excl[] = {
		{ 'h', 'x' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	int opt;

	while ((opt = getopt_long(argc, argv, "bfhl:pxVH", longopts, NULL)) != -1) {
		err_exclusive_options(opt, longopts, excl, excl_st);

		switch (opt) {
		case 'b':		/* do not output backspaces */
			ctl->no_backspaces = 1;
			break;
		case 'f':		/* allow half forward line feeds */
			ctl->fine = 1;
			break;
		case 'h':		/* compress spaces into tabs */
			ctl->compress_spaces = 1;
			break;
		case 'l':
			/*
			 * Buffered line count, which is a value in half
			 * lines e.g. twice the amount specified.
			 */
			ctl->max_bufd_lines = strtou32_or_err(optarg, _("bad -l argument")) * 2;
			break;
		case 'p':
			ctl->pass_unknown_seqs = 1;
			break;
		case 'x':		/* do not compress spaces into tabs */
			ctl->compress_spaces = 0;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'H':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind != argc) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
}

#ifdef COL_DEALLOCATE_ON_EXIT
static void free_line_allocations(struct col_alloc *root)
{
	struct col_alloc *next;

	while (root) {
		next = root->next;
		free(root->l);
		free(root);
		root = next;
	}
}
#endif

static void process_char(struct col_ctl *ctl, struct col_lines *lns)
{
                /* Deal printable characters */
                if (!iswgraph(lns->ch) && handle_not_graphic(ctl, lns))
                        return;

                /* Must stuff ch in a line - are we at the right one? */
                if ((size_t)lns->cur_line != lns->this_line - lns->adjust)
                        update_cur_line(ctl, lns);

                /* Does line buffer need to grow? */
                if (ctl->l->l_lsize <= ctl->l->l_line_len + 1) {
                        size_t need;

                        need = ctl->l->l_lsize ? ctl->l->l_lsize * 2 : NALLOC;
                        ctl->l->l_line = xrealloc(ctl->l->l_line, need * sizeof(struct col_char));
                        ctl->l->l_lsize = need;
                }

                /* Store character */
                lns->c = &ctl->l->l_line[ctl->l->l_line_len++];
                lns->c->c_char = lns->ch;
                lns->c->c_set = lns->cur_set;

                if (0 < lns->cur_col)
                        lns->c->c_column = lns->cur_col;
                else
                        lns->c->c_column = 0;
                lns->c->c_width = wcwidth(lns->ch);

                /*
                 * If things are put in out of order, they will need sorting
                 * when it is flushed.
                 */
                if (lns->cur_col < ctl->l->l_max_col)
                        ctl->l->l_needs_sort = 1;
                else
                        ctl->l->l_max_col = lns->cur_col;
                if (0 < lns->c->c_width)
                        lns->cur_col += lns->c->c_width;

}

int main(int argc, char **argv)
{
	struct col_ctl ctl = {
		.compress_spaces = 1,
		.last_set = CS_NORMAL,
		.max_bufd_lines = BUFFER_MARGIN * 2,
	};
	struct col_lines lns = {
		.cur_set = CS_NORMAL,
	};
	int ret = EXIT_SUCCESS;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	ctl.lines = ctl.l = alloc_line(&ctl);

	parse_options(&ctl, argc, argv);

	while (feof(stdin) == 0) {
		errno = 0;
		/* Get character */
		lns.ch = getwchar();

		if (lns.ch == WEOF) {
			if (errno == EILSEQ) {
				/* Illegal multibyte sequence */
				int c;
				char buf[5];
				size_t len, i;

				c = getchar();
				if (c == EOF)
					break;
				sprintf(buf, "\\x%02x", (unsigned char) c);
				len = strlen(buf);
				for (i = 0; i < len; i++) {
					lns.ch = buf[i];
					process_char(&ctl, &lns);
				}
			} else
				/* end of file */
				break;
		} else
			/* the common case */
			process_char(&ctl, &lns);
	}

	/* goto the last line that had a character on it */
	for (; ctl.l->l_next; ctl.l = ctl.l->l_next)
		lns.this_line++;
	if (lns.max_line == 0 && lns.cur_col == 0) {
#ifdef COL_DEALLOCATE_ON_EXIT
		free_line_allocations(ctl.alloc_root);
#endif
		return EXIT_SUCCESS;	/* no lines, so just exit */
	}
	flush_lines(&ctl, lns.this_line - lns.nflushd_lines + lns.extra_lines + 1);

	/* make sure we leave things in a sane state */
	if (ctl.last_set != CS_NORMAL)
		col_putchar(SI);

	/* flush out the last few blank lines */
	ctl.nblank_lines = lns.max_line - lns.this_line;
	if (lns.max_line & 1)
		ctl.nblank_lines++;
	else if (!ctl.nblank_lines)
		/* missing a \n on the last line? */
		ctl.nblank_lines = 2;
	flush_blanks(&ctl);
#ifdef COL_DEALLOCATE_ON_EXIT
	free_line_allocations(ctl.alloc_root);
#endif
	return ret;
}
