/*
 * Copyright (C) 1980 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* more.c - General purpose tty output filter and file perusal program
 *
 * by Eric Shienbrood, UC Berkeley
 *
 * modified by Geoff Peck
 *	UCB to add underlining, single spacing
 * modified by John Foderaro
 *	UCB to add -c and MORE environment variable
 * modified by Erik Troan <ewt@redhat.com>
 *	to be more posix and so compile on linux/axp.
 * modified by Kars de Jong <jongk@cs.utwente.nl>
 *	to use terminfo instead of termcap.
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *	added Native Language Support
 * 1999-03-19 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *	more nls translatable strings
 * 1999-05-09 aeb
 *	applied a RedHat patch (setjmp->sigsetjmp); without it a second
 *	^Z would fail.
 * 1999-05-09 aeb
 *	undone Kars' work, so that more works without libcurses (and
 *	hence can be in /bin with libcurses being in
 *	/usr/lib which may not be mounted).  However, when termcap is not
 *	present curses can still be used.
 * 2010-10-21 Davidlohr Bueso <dave@gnu.org>
 *	modified mem allocation handling for util-linux
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/param.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ttydefaults.h>
#include <sys/wait.h>
#include <regex.h>
#include <assert.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <paths.h>
#include <getopt.h>

#if defined(HAVE_NCURSESW_TERM_H)
# include <ncursesw/term.h>
#elif defined(HAVE_NCURSES_TERM_H)
# include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
# include <term.h>
#endif

#ifdef HAVE_MAGIC
# include <magic.h>
#endif

#include "strutils.h"
#include "nls.h"
#include "xalloc.h"
#include "widechar.h"
#include "closestream.h"
#include "env.h"

#ifdef TEST_PROGRAM
# define NON_INTERACTIVE_MORE 1
#endif

#define BACKSPACE	"\b"
#define CARAT		"^"

#define ARROW_UP	"\x1b\x5b\x41"
#define ARROW_DOWN	"\x1b\x5b\x42"
#define PAGE_UP		"\x1b\x5b\x35\x7e"
#define PAGE_DOWN	"\x1b\x5b\x36\x7e"

#define MIN_LINE_SZ	256	/* minimal line_buf buffer size */
#define ESC		'\033'
#define SCROLL_LEN	11
#define LINES_PER_PAGE	24
#define NUM_COLUMNS	80
#define TERMINAL_BUF	4096
#define INIT_BUF	80
#define COMMAND_BUF	200
#define REGERR_BUF	NUM_COLUMNS

#define TERM_AUTO_RIGHT_MARGIN    "am"
#define TERM_BACKSPACE            "cub1"
#define TERM_CEOL                 "xhp"
#define TERM_CLEAR                "clear"
#define TERM_CLEAR_TO_LINE_END    "el"
#define TERM_CLEAR_TO_SCREEN_END  "ed"
#define TERM_COLS                 "cols"
#define TERM_CURSOR_ADDRESS       "cup"
#define TERM_EAT_NEW_LINE         "xenl"
#define TERM_EXIT_STANDARD_MODE   "rmso"
#define TERM_HARD_COPY            "hc"
#define TERM_HOME                 "home"
#define TERM_LINE_DOWN            "cud1"
#define TERM_LINES                "lines"
#define TERM_OVER_STRIKE          "os"
#define TERM_STANDARD_MODE        "smso"
#define TERM_STD_MODE_GLITCH      "xmc"

/* Used in read_command() */
typedef enum {
	more_kc_unknown_command,
	more_kc_colon,
	more_kc_repeat_previous,
	more_kc_backwards,
	more_kc_jump_lines_per_screen,
	more_kc_set_lines_per_screen,
	more_kc_set_scroll_len,
	more_kc_quit,
	more_kc_skip_forward_screen,
	more_kc_skip_forward_line,
	more_kc_next_line,
	more_kc_clear_screen,
	more_kc_previous_search_match,
	more_kc_display_line,
	more_kc_display_file_and_line,
	more_kc_repeat_search,
	more_kc_search,
	more_kc_run_shell,
	more_kc_help,
	more_kc_next_file,
	more_kc_previous_file,
	more_kc_run_editor
} more_key_commands;
struct number_command {
	unsigned int number;
	more_key_commands key;
};

struct more_control {
	struct termios output_tty;	/* output terminal */
	struct termios original_tty;	/* original terminal settings */
	FILE *current_file;		/* currently open input file */
	off_t file_position;		/* file position */
	off_t file_size;		/* file size */
	int argv_position;		/* argv[] position */
	int lines_per_screen;		/* screen size in lines */
	int d_scroll_len;		/* number of lines scrolled by 'd' */
	int prompt_len;			/* message prompt length */
	int current_line;		/* line we are currently at */
	int next_jump;			/* number of lines to skip ahead */
	char **file_names;		/* The list of file names */
	int num_files;			/* Number of files left to process */
	char *shell;			/* name of the shell to use */
	int sigfd;			/* signalfd() file descriptor */
	sigset_t sigset;		/* signal operations */
	char *line_buf;			/* line buffer */
	size_t line_sz;			/* size of line_buf buffer */
	int lines_per_page;		/* lines per page */
	char *clear;			/* clear screen */
	char *erase_line;		/* erase line */
	char *enter_std;		/* enter standout mode */
	char *exit_std;			/* exit standout mode */
	char *backspace_ch;		/* backspace character */
	char *go_home;			/* go to home */
	char *move_line_down;		/* move line down */
	char *clear_rest;		/* clear rest of screen */
	int num_columns;		/* number of columns */
	char *next_search;		/* file beginning search string */
	char *previous_search;		/* previous search() buf[] item */
	struct {
		off_t row_num;		/* row file position */
		long line_num;		/* line number */
	} context,
	  screen_start;
	unsigned int leading_number;	/* number in front of key command */
	struct number_command previous_command;	/* previous key command */
	char *shell_line;		/* line to execute in subshell */
#ifdef HAVE_MAGIC
	magic_t magic;			/* libmagic database entries */
#endif
	unsigned int
		ignore_stdin:1,		/* POLLHUP; peer closed pipe */
		bad_stdout:1,		/* true if overwriting does not turn off standout */
		catch_suspend:1,	/* we should catch the SIGTSTP signal */
		clear_line_ends:1,	/* do not scroll, paint each screen from the top */
		clear_first:1,		/* is first character in file \f */
		dumb_tty:1,		/* is terminal type known */
		eat_newline:1,		/* is newline ignored after 80 cols */
		erase_input_ok:1,	/* is erase input supported */
		erase_previous_ok:1,	/* is erase previous supported */
		exit_on_eof:1,		/* exit on EOF */
		first_file:1,		/* is the input file the first in list */
		fold_long_lines:1,	/* fold long lines */
		hard_tabs:1,		/* print spaces instead of '\t' */
		hard_tty:1,		/* is this hard copy terminal (a printer or such) */
		leading_colon:1,	/* key command has leading ':' character */
		is_eof:1,               /* EOF detected */
		is_paused:1,		/* is output paused */
		no_quit_dialog:1,	/* suppress quit dialog */
		no_scroll:1,		/* do not scroll, clear the screen and then display text */
		no_tty_in:1,		/* is input in interactive mode */
		no_tty_out:1,		/* is output in interactive mode */
		no_tty_err:1,           /* is stderr terminal */
		print_banner:1,		/* print file name banner */
		reading_num:1,		/* are we reading leading_number */
		report_errors:1,	/* is an error reported */
		search_at_start:1,	/* search pattern defined at start up */
		search_called:1,	/* previous more command was a search */
		squeeze_spaces:1,	/* suppress white space */
		stdout_glitch:1,	/* terminal has standout mode glitch */
		stop_after_formfeed:1,	/* stop after form feeds */
		suppress_bell:1,	/* suppress bell */
		wrap_margin:1;		/* set if automargins */
};

static void __attribute__((__noreturn__)) usage(void)
{
	printf("%s", USAGE_HEADER);
	printf(_(" %s [options] <file>...\n"), program_invocation_short_name);

	printf("%s", USAGE_SEPARATOR);
	printf("%s\n", _("Display the contents of a file in a terminal."));

	printf("%s", USAGE_OPTIONS);
	printf("%s\n", _(" -d, --silent          display help instead of ringing bell"));
	printf("%s\n", _(" -f, --logical         count logical rather than screen lines"));
	printf("%s\n", _(" -l, --no-pause        suppress pause after form feed"));
	printf("%s\n", _(" -c, --print-over      do not scroll, display text and clean line ends"));
	printf("%s\n", _(" -p, --clean-print     do not scroll, clean screen and display text"));
	printf("%s\n", _(" -e, --exit-on-eof     exit on end-of-file"));
	printf("%s\n", _(" -s, --squeeze         squeeze multiple blank lines into one"));
	printf("%s\n", _(" -u, --plain           suppress underlining and bold"));
	printf("%s\n", _(" -n, --lines <number>  the number of lines per screenful"));
	printf("%s\n", _(" -<number>             same as --lines"));
	printf("%s\n", _(" +<number>             display file beginning from line number"));
	printf("%s\n", _(" +/<pattern>           display file beginning from pattern match"));
	printf("%s", USAGE_SEPARATOR);
	printf(USAGE_HELP_OPTIONS(23));
	printf(USAGE_MAN_TAIL("more(1)"));
	exit(EXIT_SUCCESS);
}

static void argscan(struct more_control *ctl, int as_argc, char **as_argv)
{
	int c, opt;
	static const struct option longopts[] = {
		{ "silent",      no_argument,       NULL, 'd' },
		{ "logical",     no_argument,       NULL, 'f' },
		{ "no-pause",    no_argument,       NULL, 'l' },
		{ "print-over",  no_argument,       NULL, 'c' },
		{ "clean-print", no_argument,       NULL, 'p' },
		{ "exit-on-eof", no_argument,       NULL, 'e' },
		{ "squeeze",     no_argument,       NULL, 's' },
		{ "plain",       no_argument,       NULL, 'u' },
		{ "lines",       required_argument, NULL, 'n' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "help",        no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	/* Take care of number option and +args. */
	for (opt = 0; opt < as_argc; opt++) {
		int move = 0;

		if (as_argv[opt][0] == '-' && isdigit_string(as_argv[opt] + 1)) {
			ctl->lines_per_screen =
			    strtos16_or_err(as_argv[opt], _("failed to parse number"));
			ctl->lines_per_screen = abs(ctl->lines_per_screen);
			move = 1;
		} else if (as_argv[opt][0] == '+') {
			if (isdigit_string(as_argv[opt] + 1)) {
				ctl->next_jump = strtos32_or_err(as_argv[opt],
				    _("failed to parse number")) - 1;
				move = 1;
			} else if (as_argv[opt][1] == '/') {
				free(ctl->next_search);
				ctl->next_search = xstrdup(as_argv[opt] + 2);
				ctl->search_at_start = 1;
				move = 1;
			}
		}
		if (move) {
			as_argc = remove_entry(as_argv, opt, as_argc);
			opt--;
		}
	}

	while ((c = getopt_long(as_argc, as_argv, "dflcpsun:eVh", longopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			ctl->suppress_bell = 1;
			break;
		case 'l':
			ctl->stop_after_formfeed = 0;
			break;
		case 'f':
			ctl->fold_long_lines = 0;
			break;
		case 'p':
			ctl->no_scroll = 1;
			break;
		case 'c':
			ctl->clear_line_ends = 1;
			break;
		case 's':
			ctl->squeeze_spaces = 1;
			break;
		case 'u':
			break;
		case 'n':
			ctl->lines_per_screen = strtou16_or_err(optarg, _("argument error"));
			break;
		case 'e':
			ctl->exit_on_eof = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
			break;
		}
	}
	ctl->num_files = as_argc - optind;
	ctl->file_names = as_argv + optind;
}

static void env_argscan(struct more_control *ctl, const char *s)
{
	char **env_argv;
	int env_argc = 1;
	int size = 8;
	const char delim[] = { ' ', '\n', '\t', '\0' };
	char *str = xstrdup(s);
	char *key = NULL, *tok;

	env_argv = xreallocarray(NULL, size, sizeof(char *));
	env_argv[0] = _("MORE environment variable");	/* program name */
	for (tok = strtok_r(str, delim, &key); tok; tok = strtok_r(NULL, delim, &key)) {
		if (size == env_argc) {
			size *= 2;
			env_argv = xreallocarray(env_argv, size, sizeof(char *));
		}
		env_argv[env_argc++] = tok;
	}

	argscan(ctl, env_argc, env_argv);
	/* Reset optind, command line parsing needs this.  */
	optind = 0;
	free(str);
	free(env_argv);
}

static void more_fseek(struct more_control *ctl, off_t pos)
{
	ctl->file_position = pos;
	fseeko(ctl->current_file, pos, SEEK_SET);
}

static int more_getc(struct more_control *ctl)
{
	int ret = getc(ctl->current_file);
	ctl->file_position = ftello(ctl->current_file);
	return ret;
}

static int more_ungetc(struct more_control *ctl, int c)
{
	int ret = ungetc(c, ctl->current_file);
	ctl->file_position = ftello(ctl->current_file);
	return ret;
}

static void print_separator(const int c, int n)
{
	while (n--)
		putchar(c);
	putchar('\n');
}

/* check_magic -- check for file magic numbers. */
static int check_magic(struct more_control *ctl, char *fs)
{
#ifdef HAVE_MAGIC
	const int fd = fileno(ctl->current_file);
	const char *mime_encoding = magic_descriptor(ctl->magic, fd);
	const char *magic_error_msg = magic_error(ctl->magic);

	if (magic_error_msg) {
		printf("%s: %s: %s\n", program_invocation_short_name,
			_("magic failed"), magic_error_msg);
		return 0;
	}
	if (!mime_encoding || !(strcmp("binary", mime_encoding))) {
		printf(_("\n******** %s: Not a text file ********\n\n"), fs);
		return 1;
	}
#else
	signed char twobytes[2];

	/* don't try to look ahead if the input is unseekable */
	if (fseek(ctl->current_file, 0L, SEEK_SET))
		return 0;

	if (fread(twobytes, 2, 1, ctl->current_file) == 1) {
		switch (twobytes[0] + (twobytes[1] << 8)) {
		case 0407:	/* a.out obj */
		case 0410:	/* a.out exec */
		case 0413:	/* a.out demand exec */
		case 0405:
		case 0411:
		case 0177545:
		case 0x457f:	/* simple ELF detection */
			printf(_("\n******** %s: Not a text file ********\n\n"),
			       fs);
			return 1;
		}
	}
	fseek(ctl->current_file, 0L, SEEK_SET);	/* rewind() not necessary */
#endif
	return 0;
}

/* Check whether the file named by fs is an ASCII file which the user may
 * access.  If it is, return the opened file.  Otherwise return NULL. */
static void checkf(struct more_control *ctl, char *fs)
{
	struct stat st;
	int c;

	ctl->current_line = 0;
	ctl->file_position = 0;
	ctl->file_size = 0;
	fflush(NULL);

	ctl->current_file = fopen(fs, "r");
	if (ctl->current_file == NULL) {
		if (ctl->clear_line_ends)
			putp(ctl->erase_line);
		warn(_("cannot open %s"), fs);
		return;
	}
	if (fstat(fileno(ctl->current_file), &st) != 0) {
		warn(_("stat of %s failed"), fs);
		return;
	}
	if ((st.st_mode & S_IFMT) == S_IFDIR) {
		printf(_("\n*** %s: directory ***\n\n"), fs);
		ctl->current_file = NULL;
		return;
	}
	ctl->file_size = st.st_size;
	if (0 < ctl->file_size && check_magic(ctl, fs)) {
		fclose(ctl->current_file);
		ctl->current_file = NULL;
		return;
	}
	fcntl(fileno(ctl->current_file), F_SETFD, FD_CLOEXEC);
	c = more_getc(ctl);
	ctl->clear_first = (c == '\f');
	more_ungetc(ctl, c);
}

static void prepare_line_buffer(struct more_control *ctl)
{
	size_t sz = ctl->num_columns * 4;

	if (ctl->line_sz >= sz)
		return;

	if (sz < MIN_LINE_SZ)
		sz = MIN_LINE_SZ;

	/* alloc sz and extra space for \n\0 */
	ctl->line_buf = xrealloc(ctl->line_buf, sz + 2);
	ctl->line_sz = sz;
}

/* Get a logical line */
static int get_line(struct more_control *ctl, int *length)
{
	int c;
	char *p;
	int column;
	static int column_wrap;

#ifdef HAVE_WIDECHAR
	size_t i;
	wchar_t wc;
	int wc_width;
	mbstate_t state, state_bak;	/* Current status of the stream. */
	char mbc[MB_LEN_MAX];		/* Buffer for one multibyte char. */
	size_t mblength;		/* Byte length of multibyte char. */
	size_t mbc_pos = 0;		/* Position of the MBC. */
	int use_mbc_buffer_flag = 0;	/* If 1, mbc has data. */
	int break_flag = 0;		/* If 1, exit while(). */
	off_t file_position_bak = ctl->file_position;

	memset(&state, '\0', sizeof(mbstate_t));
#endif

	p = ctl->line_buf;
	column = 0;
	c = more_getc(ctl);
	if (column_wrap && c == '\n') {
		ctl->current_line++;
		c = more_getc(ctl);
	}
	while (p < &ctl->line_buf[ctl->line_sz - 1]) {
#ifdef HAVE_WIDECHAR
		if (ctl->fold_long_lines && use_mbc_buffer_flag && MB_CUR_MAX > 1) {
			use_mbc_buffer_flag = 0;
			state_bak = state;
			mbc[mbc_pos++] = c;
 process_mbc:
			mblength = mbrtowc(&wc, mbc, mbc_pos, &state);

			switch (mblength) {
			case (size_t)-2:	/* Incomplete multibyte character. */
				use_mbc_buffer_flag = 1;
				state = state_bak;
				break;

			case (size_t)-1:	/* Invalid as a multibyte character. */
				*p++ = mbc[0];
				state = state_bak;
				column++;
				file_position_bak++;
				if (column >= ctl->num_columns) {
					more_fseek(ctl, file_position_bak);
				} else {
					memmove(mbc, mbc + 1, --mbc_pos);
					if (mbc_pos > 0) {
						mbc[mbc_pos] = '\0';
						goto process_mbc;
					}
				}
				break;

			default:
				wc_width = wcwidth(wc);
				if (column + wc_width > ctl->num_columns) {
					more_fseek(ctl, file_position_bak);
					break_flag = 1;
				} else {
					for (i = 0; p < &ctl->line_buf[ctl->line_sz - 1] &&
						    i < mbc_pos; i++)
						*p++ = mbc[i];
					if (wc_width > 0)
						column += wc_width;
				}
			}

			if (break_flag || column >= ctl->num_columns)
				break;

			c = more_getc(ctl);
			continue;
		}
#endif	/* HAVE_WIDECHAR */
		if (c == EOF) {
			if (p > ctl->line_buf) {
				*p = '\0';
				*length = p - ctl->line_buf;
				return column;
			}
			*length = p - ctl->line_buf;
			return EOF;
		}
		if (c == '\n') {
			ctl->current_line++;
			break;
		}

		*p++ = c;
		if (c == '\t') {
			if (!ctl->hard_tabs || (column < ctl->prompt_len && !ctl->hard_tty)) {
				if (ctl->hard_tabs && ctl->erase_line && !ctl->dumb_tty) {
					column = 1 + (column | 7);
					putp(ctl->erase_line);
					ctl->prompt_len = 0;
				} else {
					for (--p; p < &ctl->line_buf[ctl->line_sz - 1];) {
						*p++ = ' ';
						if ((++column & 7) == 0)
							break;
					}
					if (column >= ctl->prompt_len)
						ctl->prompt_len = 0;
				}
			} else
				column = 1 + (column | 7);
		} else if (c == '\b' && column > 0) {
			column--;
		} else if (c == '\r') {
			int next = more_getc(ctl);
			if (next == '\n') {
				p--;
				ctl->current_line++;
				break;
			}
			more_ungetc(ctl, c);
			column = 0;
		} else if (c == '\f' && ctl->stop_after_formfeed) {
			p[-1] = '^';
			*p++ = 'L';
			column += 2;
			ctl->is_paused = 1;
		} else {
#ifdef HAVE_WIDECHAR
			if (ctl->fold_long_lines && MB_CUR_MAX > 1) {
				memset(mbc, '\0', MB_LEN_MAX);
				mbc_pos = 0;
				mbc[mbc_pos++] = c;
				state_bak = state;

				mblength = mbrtowc(&wc, mbc, mbc_pos, &state);
				/* The value of mblength is always less than 2 here. */
				switch (mblength) {
				case (size_t)-2:
					p--;
					file_position_bak = ctl->file_position - 1;
					state = state_bak;
					use_mbc_buffer_flag = 1;
					break;

				case (size_t)-1:
					state = state_bak;
					column++;
					break;

				default:
					wc_width = wcwidth(wc);
					if (wc_width > 0)
						column += wc_width;
				}
			} else
#endif	/* HAVE_WIDECHAR */
			{
				if (isprint(c))
					column++;
			}
		}

		if (column >= ctl->num_columns && ctl->fold_long_lines)
			break;
#ifdef HAVE_WIDECHAR
		if (use_mbc_buffer_flag == 0 && p >= &ctl->line_buf[ctl->line_sz - 1 - 4])
			/* don't read another char if there is no space for
			 * whole multibyte sequence */
			break;
#endif
		c = more_getc(ctl);
	}
	if (column >= ctl->num_columns && ctl->num_columns > 0) {
		if (!ctl->wrap_margin) {
			*p++ = '\n';
		}
	}
	column_wrap = column == ctl->num_columns && ctl->fold_long_lines;
	if (column_wrap && ctl->eat_newline && ctl->wrap_margin) {
		*p++ = '\n';	/* simulate normal wrap */
	}
	*length = p - ctl->line_buf;
	*p = 0;
	return column;
}

/* Erase the rest of the prompt, assuming we are starting at column col. */
static void erase_to_col(struct more_control *ctl, int col)
{

	if (ctl->prompt_len == 0)
		return;
	if (col == 0 && ctl->clear_line_ends)
		puts(ctl->erase_line);
	else if (ctl->hard_tty)
		putchar('\n');
	else {
		if (col == 0)
			putchar('\r');
		if (!ctl->dumb_tty && ctl->erase_line)
			putp(ctl->erase_line);
		else {
			printf("%*s", ctl->prompt_len - col, "");
			if (col == 0)
				putchar('\r');
		}
	}
	ctl->prompt_len = col;
}

static void output_prompt(struct more_control *ctl, char *filename)
{
	if (ctl->clear_line_ends)
		putp(ctl->erase_line);
	else if (ctl->prompt_len > 0)
		erase_to_col(ctl, 0);
	if (!ctl->hard_tty) {
		ctl->prompt_len = 0;
		if (ctl->enter_std) {
			putp(ctl->enter_std);
			ctl->prompt_len += (2 * ctl->stdout_glitch);
		}
		if (ctl->clear_line_ends)
			putp(ctl->erase_line);
		ctl->prompt_len += printf(_("--More--"));
		if (filename != NULL) {
			ctl->prompt_len += printf(_("(Next file: %s)"), filename);
		} else if (!ctl->no_tty_in && 0 < ctl->file_size) {
		    int position = ((ctl->file_position * 100) / ctl->file_size);
		    if (position == 100) {
			erase_to_col(ctl, 0);
			ctl->prompt_len += printf(_("(END)"));
		    } else {
			ctl->prompt_len += printf("(%d%%)", position);
		    }
		} else if (ctl->is_eof) {
			erase_to_col(ctl, 0);
			ctl->prompt_len += printf(_("(END)"));
		}

		if (ctl->suppress_bell) {
			ctl->prompt_len +=
			    printf(_("[Press space to continue, 'q' to quit.]"));
		}
		if (ctl->exit_std)
			putp(ctl->exit_std);
		if (ctl->clear_line_ends)
			putp(ctl->clear_rest);
	} else
		fprintf(stderr, "\a");
	fflush(NULL);
}

static void reset_tty(struct more_control *ctl)
{
	if (ctl->no_tty_out)
		return;
	fflush(NULL);
	ctl->output_tty.c_lflag |= ICANON | ECHO;
	ctl->output_tty.c_cc[VMIN] = ctl->original_tty.c_cc[VMIN];
	ctl->output_tty.c_cc[VTIME] = ctl->original_tty.c_cc[VTIME];
	tcsetattr(STDERR_FILENO, TCSANOW, &ctl->original_tty);
}

/* Clean up terminal state and exit. Also come here if interrupt signal received */
static void __attribute__((__noreturn__)) more_exit(struct more_control *ctl)
{
#ifdef HAVE_MAGIC
	magic_close(ctl->magic);
#endif
	reset_tty(ctl);
	if (ctl->clear_line_ends) {
		putchar('\r');
		putp(ctl->erase_line);
	} else if (!ctl->clear_line_ends && (ctl->prompt_len > 0))
		erase_to_col(ctl, 0);
	fflush(NULL);
	free(ctl->previous_search);
	free(ctl->shell_line);
	free(ctl->line_buf);
	free(ctl->go_home);
	if (ctl->current_file)
		fclose(ctl->current_file);
	del_curterm(cur_term);
	_exit(EXIT_SUCCESS);
}

static cc_t read_user_input(struct more_control *ctl)
{
	cc_t c;

	errno = 0;
	/*
	 * Key commands can be read() from either stderr or stdin.  If they
	 * are read from stdin such as 'cat file.txt | more' then the pipe
	 * input is understood as series key commands - and that is not
	 * wanted.  Keep the read() reading from stderr.
	 */
	if (read(STDERR_FILENO, &c, 1) <= 0) {
		if (errno != EINTR)
			more_exit(ctl);
		else
			c = ctl->output_tty.c_cc[VKILL];
	}
	return c;
}

/* Read a number and command from the terminal.  Set cmd to the non-digit
 * which terminates the number. */
static struct number_command read_command(struct more_control *ctl)
{
	cc_t input[8] = { 0 };
	ssize_t i, ilen;
	struct number_command cmd = { .key = more_kc_unknown_command };

	/* See stderr note in read_user_input() */
	if ((ilen = read(STDERR_FILENO, &input, sizeof(input))) <= 0)
		return cmd;
	if (2 < ilen) {
		if (!memcmp(input, ARROW_UP, sizeof(ARROW_UP))) {
			cmd.key = more_kc_backwards;
			return cmd;
		} else if (!memcmp(input, ARROW_DOWN, sizeof(ARROW_DOWN))) {
			cmd.key = more_kc_jump_lines_per_screen;
			return cmd;
		} else if (!memcmp(input, PAGE_UP, sizeof(PAGE_UP))) {
			cmd.key = more_kc_backwards;
			return cmd;
		} else if (!memcmp(input, PAGE_DOWN, sizeof(PAGE_DOWN))) {
			cmd.key = more_kc_jump_lines_per_screen;
			return cmd;
		}
	}
	for (i = 0; i < ilen; i++) {
		if (isdigit(input[i])) {
			if (0 < ctl->reading_num) {
				ctl->leading_number *= 10;
				ctl->leading_number += input[i] - '0';
			} else
				ctl->leading_number = input[i] - '0';
			ctl->reading_num = 1;
			continue;
		}
		cmd.number = ctl->leading_number;
		ctl->reading_num = 0;
		ctl->leading_number = 0;
		if (ctl->leading_colon) {
			ctl->leading_colon = 0;
			switch (input[i]) {
			case 'f':
				cmd.key = more_kc_display_file_and_line;
				return cmd;
			case 'n':
				cmd.key = more_kc_next_file;
				return cmd;
			case 'p':
				cmd.key = more_kc_previous_file;
				return cmd;
			default:
				cmd.key = more_kc_unknown_command;
				return cmd;
			}
		}
		/* command is a single char */
		switch (input[i]) {
		case '.':
			cmd.key = more_kc_repeat_previous;
			break;
		case ':':
			ctl->leading_colon = 1;
			break;
		case 'b':
		case CTRL('B'):
			cmd.key = more_kc_backwards;
			break;
		case ' ':
			cmd.key = more_kc_jump_lines_per_screen;
			break;
		case 'z':
			cmd.key = more_kc_set_lines_per_screen;
			break;
		case 'd':
		case CTRL('D'):
			cmd.key = more_kc_set_scroll_len;
			break;
		case 'q':
		case 'Q':
			cmd.key = more_kc_quit;
			return cmd;
			break;
		case 'f':
		case CTRL('F'):
			cmd.key = more_kc_skip_forward_screen;
			break;
		case 's':
			cmd.key = more_kc_skip_forward_line;
			break;
		case '\n':
			cmd.key = more_kc_next_line;
			break;
		case '\f':
			cmd.key = more_kc_clear_screen;
			break;
		case '\'':
			cmd.key = more_kc_previous_search_match;
			break;
		case '=':
			cmd.key = more_kc_display_line;
			break;
		case 'n':
			cmd.key = more_kc_repeat_search;
			break;
		case '/':
			cmd.key = more_kc_search;
			break;
		case '!':
			cmd.key = more_kc_run_shell;
			break;
		case '?':
		case 'h':
			cmd.key = more_kc_help;
			break;
		case 'v':
			cmd.key = more_kc_run_editor;
			break;
		}
	}
	return cmd;
}

/* Change displayed file from command line list to next nskip, where nskip
 * is relative position in argv and can be negative, that is a previous
 * file.  */
static void change_file(struct more_control *ctl, int nskip)
{
	if (nskip == 0)
		return;
	if (nskip > 0) {
		if (ctl->argv_position + nskip > ctl->num_files - 1)
			nskip = ctl->num_files - ctl->argv_position - 1;
	}
	ctl->argv_position += nskip;
	if (ctl->argv_position < 0)
		ctl->argv_position = 0;
	puts(_("\n...Skipping "));
	if (ctl->clear_line_ends)
		putp(ctl->erase_line);
	if (nskip > 0)
		fputs(_("...Skipping to file "), stdout);
	else
		fputs(_("...Skipping back to file "), stdout);
	puts(ctl->file_names[ctl->argv_position]);
	if (ctl->clear_line_ends)
		putp(ctl->erase_line);
	putchar('\n');
	ctl->argv_position--;
}

static void show(struct more_control *ctl, char c)
{
	if ((c < ' ' && c != '\n' && c != ESC) || c == CERASE) {
		c += (c == CERASE) ? -0100 : 0100;
		fputs(CARAT, stderr);
		ctl->prompt_len++;
	}
	fputc(c, stderr);
	ctl->prompt_len++;
}

static void more_error(struct more_control *ctl, char *mess)
{
	if (ctl->clear_line_ends)
		putp(ctl->erase_line);
	else
		erase_to_col(ctl, 0);
	ctl->prompt_len += strlen(mess);
	if (ctl->enter_std)
		putp(ctl->enter_std);
	fputs(mess, stdout);
	if (ctl->exit_std)
		putp(ctl->exit_std);
	fflush(NULL);
	ctl->report_errors++;
}

static void erase_one_column(struct more_control *ctl)
{
	if (ctl->erase_previous_ok)
		fprintf(stderr, "%s ", ctl->backspace_ch);
	fputs(ctl->backspace_ch, stderr);
}

static void ttyin(struct more_control *ctl, char buf[], int nmax, char pchar)
{
	char *sp;
	cc_t c;
	int slash = 0;
	int maxlen;

	sp = buf;
	maxlen = 0;
	while (sp - buf < nmax) {
		if (ctl->prompt_len > maxlen)
			maxlen = ctl->prompt_len;
		c = read_user_input(ctl);
		if (c == '\\') {
			slash++;
		} else if (c == ctl->output_tty.c_cc[VERASE] && !slash) {
			if (sp > buf) {
#ifdef HAVE_WIDECHAR
				if (MB_CUR_MAX > 1) {
					wchar_t wc;
					size_t pos = 0, mblength;
					mbstate_t state, state_bak;

					memset(&state, '\0', sizeof(mbstate_t));

					while (1) {
						state_bak = state;
						mblength =
						    mbrtowc(&wc, buf + pos,
							    sp - buf, &state);

						switch (mblength) {
						case (size_t)-2:
						case (size_t)-1:
							state = state_bak;
							/* fallthrough */
						case 0:
							mblength = 1;
						}
						if (buf + pos + mblength >= sp)
							break;

						pos += mblength;
					}

					if (mblength == 1) {
						erase_one_column(ctl);
					} else {
						int wc_width;
						wc_width = wcwidth(wc);
						wc_width =
						    (wc_width <
						     1) ? 1 : wc_width;
						while (wc_width--) {
							erase_one_column(ctl);
						}
					}

					while (mblength--) {
						--ctl->prompt_len;
						--sp;
					}
				} else
#endif	/* HAVE_WIDECHAR */
				{
					--ctl->prompt_len;
					erase_one_column(ctl);
					--sp;
				}

				if ((*sp < ' ' && *sp != '\n') || *sp == CERASE) {
					--ctl->prompt_len;
					erase_one_column(ctl);
				}
				continue;
			}
			if (!ctl->erase_line)
				ctl->prompt_len = maxlen;
		} else if (c == ctl->output_tty.c_cc[VKILL] && !slash) {
			if (ctl->hard_tty) {
				show(ctl, c);
				putchar('\n');
				putchar(pchar);
			} else {
				putchar('\r');
				putchar(pchar);
				if (ctl->erase_line)
					erase_to_col(ctl, 1);
				else if (ctl->erase_input_ok)
					while (ctl->prompt_len-- > 1)
						fprintf(stderr, "%s %s", ctl->backspace_ch, ctl->backspace_ch);
				ctl->prompt_len = 1;
			}
			sp = buf;
			fflush(NULL);
			continue;
		}
		if (slash && (c == ctl->output_tty.c_cc[VKILL] ||
			      c == ctl->output_tty.c_cc[VERASE])) {
			erase_one_column(ctl);
			--sp;
		}
		if (c != '\\')
			slash = 0;
		*sp++ = c;
		if ((c < ' ' && c != '\n' && c != ESC) || c == CERASE) {
			c += (c == CERASE) ? -0100 : 0100;
			fputs(CARAT, stderr);
			ctl->prompt_len++;
		}
		if (c != '\n' && c != ESC) {
			fputc(c, stderr);
			ctl->prompt_len++;
		} else
			break;
	}
	*--sp = '\0';
	if (!ctl->erase_line)
		ctl->prompt_len = maxlen;
	if (sp - buf >= nmax - 1)
		more_error(ctl, _("Line too long"));
}

/* Expand shell command line. */
static void expand(struct more_control *ctl, char *inbuf)
{
	char *inpstr;
	char *outstr;
	char c;
	char *temp;
	int tempsz, xtra = 0, offset;

	if (!ctl->no_tty_in)
		xtra += strlen(ctl->file_names[ctl->argv_position]) + 1;
	if (ctl->shell_line)
		xtra += strlen(ctl->shell_line) + 1;

	tempsz = COMMAND_BUF + xtra;
	temp = xmalloc(tempsz);
	inpstr = inbuf;
	outstr = temp;

	while ((c = *inpstr++) != '\0') {
		offset = outstr - temp;
		if (tempsz - offset - 1 < xtra) {
			tempsz += COMMAND_BUF + xtra;
			temp = xrealloc(temp, tempsz);
			outstr = temp + offset;
		}
		switch (c) {
		case '%':
			if (!ctl->no_tty_in) {
				strcpy(outstr, ctl->file_names[ctl->argv_position]);
				outstr += strlen(ctl->file_names[ctl->argv_position]);
			} else
				*outstr++ = c;
			break;
		case '!':
			if (ctl->shell_line) {
				strcpy(outstr, ctl->shell_line);
				outstr += strlen(ctl->shell_line);
			} else
				more_error(ctl, _
					   ("No previous command to substitute for"));
			break;
		case '\\':
			if (*inpstr == '%' || *inpstr == '!') {
				*outstr++ = *inpstr++;
				break;
			}
			/* fallthrough */
		default:
			*outstr++ = c;
		}
	}
	*outstr++ = '\0';
	free(ctl->shell_line);
	ctl->shell_line = temp;
}

static void set_tty(struct more_control *ctl)
{
	ctl->output_tty.c_lflag &= ~(ICANON | ECHO);
	ctl->output_tty.c_cc[VMIN] = 1;	/* read at least 1 char */
	ctl->output_tty.c_cc[VTIME] = 0;	/* no timeout */
	tcsetattr(STDERR_FILENO, TCSANOW, &ctl->output_tty);
}

/* Come here if a quit signal is received */
static void sigquit_handler(struct more_control *ctl)
{
	if (!ctl->dumb_tty && ctl->no_quit_dialog) {
		ctl->prompt_len += fprintf(stderr, _("[Use q or Q to quit]"));
		ctl->no_quit_dialog = 0;
	} else
		more_exit(ctl);
}

/* Come here when we get a suspend signal from the terminal */
static void sigtstp_handler(struct more_control *ctl)
{
	reset_tty(ctl);
	fflush(NULL);
	kill(getpid(), SIGSTOP);
}

/* Come here when we get a continue signal from the terminal */
static void sigcont_handler(struct more_control *ctl)
{
	set_tty(ctl);
}

/* Come here if a signal for a window size change is received */
static void sigwinch_handler(struct more_control *ctl)
{
	struct winsize win;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1) {
		if (win.ws_row != 0) {
			ctl->lines_per_page = win.ws_row;
			ctl->d_scroll_len = ctl->lines_per_page / 2 - 1;
			if (ctl->d_scroll_len < 1)
				ctl->d_scroll_len = 1;
			ctl->lines_per_screen = ctl->lines_per_page - 1;
		}
		if (win.ws_col != 0)
			ctl->num_columns = win.ws_col;
	}
	prepare_line_buffer(ctl);
}

static void __attribute__((__format__ (__printf__, 3, 4)))
	execute(struct more_control *ctl, char *filename, const char *cmd, ...)
{
	pid_t id;
	va_list argp;
	char *arg;
	char **args;
	int argcount;

	fflush(NULL);
	id = fork();
	if (id == 0) {
		int errsv;
		if (!isatty(STDIN_FILENO)) {
			close(STDIN_FILENO);
			ignore_result( open("/dev/tty", 0) );
		}
		reset_tty(ctl);

		va_start(argp, cmd);
		arg = va_arg(argp, char *);
		argcount = 0;
		while (arg) {
			argcount++;
			arg = va_arg(argp, char *);
		}
		va_end(argp);

		args = xcalloc(argcount + 1, sizeof(char *));

		va_start(argp, cmd);
		arg = va_arg(argp, char *);
		argcount = 0;
		while (arg) {
			args[argcount] = arg;
			argcount++;
			arg = va_arg(argp, char *);
		}
		va_end(argp);

		if ((geteuid() != getuid() || getegid() != getgid())
		    && drop_permissions() != 0)
			err(EXIT_FAILURE, _("drop permissions failed"));

		execvp(cmd, args);
		errsv = errno;
		fputs(_("exec failed\n"), stderr);
		exit(errsv == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED);
	}
	if (id > 0) {
		errno = 0;
		while (wait(NULL) > 0) {
			if (errno == EINTR)
				continue;
		}
	} else
		fputs(_("can't fork\n"), stderr);
	set_tty(ctl);
	print_separator('-', 24);
	output_prompt(ctl, filename);
}

static void run_shell(struct more_control *ctl, char *filename)
{
	char cmdbuf[COMMAND_BUF];

	erase_to_col(ctl, 0);
	putchar('!');
	fflush(NULL);
	if (ctl->previous_command.key == more_kc_run_shell && ctl->shell_line)
		fputs(ctl->shell_line, stderr);
	else {
		ttyin(ctl, cmdbuf, sizeof(cmdbuf) - 2, '!');
		if (strpbrk(cmdbuf, "%!\\"))
			expand(ctl, cmdbuf);
		else {
			free(ctl->shell_line);
			ctl->shell_line = xstrdup(cmdbuf);
		}
	}
	fputc('\n', stderr);
	fflush(NULL);
	ctl->prompt_len = 0;
	execute(ctl, filename, ctl->shell, ctl->shell, "-c", ctl->shell_line, 0);
}

/* Skip n lines in the file f */
static void skip_lines(struct more_control *ctl)
{
	int c;

	while (ctl->next_jump > 0) {
		while ((c = more_getc(ctl)) != '\n')
			if (c == EOF)
				return;
		ctl->next_jump--;
		ctl->current_line++;
	}
}

/*  Clear the screen */
static void more_clear_screen(struct more_control *ctl)
{
	if (ctl->clear && !ctl->hard_tty) {
		putp(ctl->clear);
		/* Put out carriage return so that system doesn't get
		 * confused by escape sequences when expanding tabs */
		putchar('\r');
		ctl->prompt_len = 0;
	}
}

static void read_line(struct more_control *ctl)
{
	int c;
	char *p;

	p = ctl->line_buf;
	while ((c = more_getc(ctl)) != '\n' && c != EOF
	       && (ptrdiff_t)p != (ptrdiff_t)(ctl->line_buf + ctl->line_sz - 1))
		*p++ = c;
	if (c == '\n')
		ctl->current_line++;
	*p = '\0';
}

/* returns: 0 timeout or nothing; <0 error, >0 success */
static int more_poll(struct more_control *ctl, int timeout)
{
	enum {
		POLLFD_SIGNAL = 0,
		POLLFD_STDIN,
		POLLFD_STDERR,
	};
	struct pollfd pfd[] = {
		[POLLFD_SIGNAL] = { .fd = ctl->sigfd,    .events = POLLIN | POLLERR | POLLHUP },
		[POLLFD_STDIN]  = { .fd = STDIN_FILENO,  .events = POLLIN | POLLERR | POLLHUP },
		[POLLFD_STDERR] = { .fd = STDERR_FILENO, .events = POLLIN | POLLERR | POLLHUP }
	};
	int has_data = 0;

	while (!has_data) {
		int rc;

		if (ctl->ignore_stdin)
			pfd[POLLFD_STDIN].fd = -1;	/* probably closed, ignore */

		rc = poll(pfd, ARRAY_SIZE(pfd), timeout);

		/* error */
		if (rc < 0) {
			if (errno == EAGAIN)
				continue;

			more_error(ctl, _("poll failed"));
			return rc;
		}

		/* timeout */
		if (rc == 0)
			return 0;

		/* event on signal FD */
		if (pfd[POLLFD_SIGNAL].revents) {
			struct signalfd_siginfo info;
			ssize_t sz;

			sz = read(pfd[POLLFD_SIGNAL].fd, &info, sizeof(info));
			assert(sz == sizeof(info));
			switch (info.ssi_signo) {
			case SIGINT:
				more_exit(ctl);
				break;
			case SIGQUIT:
				sigquit_handler(ctl);
				break;
			case SIGTSTP:
				sigtstp_handler(ctl);
				break;
			case SIGCONT:
				sigcont_handler(ctl);
				break;
			case SIGWINCH:
				sigwinch_handler(ctl);
				break;
			default:
				abort();
			}
		}

		/* event on stdin */
		if (pfd[POLLFD_STDIN].revents) {
			/* Check for POLLERR and POLLHUP in stdin revents */
			if ((pfd[POLLFD_STDIN].revents & POLLERR) &&
			    (pfd[POLLFD_STDIN].revents & POLLHUP))
				more_exit(ctl);

			/* poll() return POLLHUP event after pipe close() and POLLNVAL
			 * means that fd is already closed. */
			if ((pfd[POLLFD_STDIN].revents & POLLHUP) ||
			    (pfd[POLLFD_STDIN].revents & POLLNVAL))
				ctl->ignore_stdin = 1;
			else
				has_data++;
		}

		/* event on stderr (we reads user commands from stderr!) */
		if (pfd[POLLFD_STDERR].revents)
			has_data++;
	}

	return has_data;
}

/* Search for nth occurrence of regular expression contained in buf in
 * the file */
static void search(struct more_control *ctl, char buf[], int n)
{
	off_t startline = ctl->file_position;
	off_t line1 = startline;
	off_t line2 = startline;
	off_t line3;
	int lncount;
	int saveln, rc;
	regex_t re;

	if (buf != ctl->previous_search) {
		free(ctl->previous_search);
		ctl->previous_search = buf;
	}

	ctl->search_called = 1;
	ctl->context.line_num = saveln = ctl->current_line;
	ctl->context.row_num = startline;
	lncount = 0;
	if (!buf)
		goto notfound;
	if ((rc = regcomp(&re, buf, REG_NOSUB)) != 0) {
		char s[REGERR_BUF];
		regerror(rc, &re, s, sizeof s);
		more_error(ctl, s);
		return;
	}
	while (!feof(ctl->current_file)) {
		line3 = line2;
		line2 = line1;
		line1 = ctl->file_position;
		read_line(ctl);
		lncount++;
		if (regexec(&re, ctl->line_buf, 0, NULL, 0) == 0 && --n == 0) {
			if ((1 < lncount && ctl->no_tty_in) || 3 < lncount) {
				putchar('\n');
				if (ctl->clear_line_ends)
					putp(ctl->erase_line);
				fputs(_("...skipping\n"), stdout);
			}
			if (!ctl->no_tty_in) {
				ctl->current_line -= (lncount < 3 ? lncount : 3);
				more_fseek(ctl, line3);
				if (ctl->no_scroll) {
					if (ctl->clear_line_ends) {
						putp(ctl->go_home);
						putp(ctl->erase_line);
					} else
						more_clear_screen(ctl);
				}
			} else {
				erase_to_col(ctl, 0);
				if (ctl->no_scroll) {
					if (ctl->clear_line_ends) {
						putp(ctl->go_home);
						putp(ctl->erase_line);
					} else
						more_clear_screen(ctl);
				}
				puts(ctl->line_buf);
			}
			break;
		}
		more_poll(ctl, 0);
	}
	/* Move ctrl+c signal handling back to more_key_command(). */
	signal(SIGINT, SIG_DFL);
	sigaddset(&ctl->sigset, SIGINT);
	sigprocmask(SIG_BLOCK, &ctl->sigset, NULL);
	regfree(&re);
	if (feof(ctl->current_file)) {
		if (!ctl->no_tty_in) {
			ctl->current_line = saveln;
			more_fseek(ctl, startline);
		} else {
			fputs(_("\nPattern not found\n"), stdout);
			more_exit(ctl);
		}
notfound:
		more_error(ctl, _("Pattern not found"));
	}
}

static char *find_editor(void)
{
	static char *editor;

	editor = getenv("VISUAL");
	if (editor == NULL || *editor == '\0')
		editor = getenv("EDITOR");
	if (editor == NULL || *editor == '\0')
		editor = _PATH_VI;
	return editor;
}

static void runtime_usage(void)
{
	fputs(_("Most commands optionally preceded by integer argument k.  "
		"Defaults in brackets.\n"
		"Star (*) indicates argument becomes new default.\n"), stdout);
	print_separator('-', 79);
	fprintf(stdout,
		_
		("<space>                 Display next k lines of text [current screen size]\n"
		 "z                       Display next k lines of text [current screen size]*\n"
		 "<return>                Display next k lines of text [1]*\n"
		 "d or ctrl-D             Scroll k lines [current scroll size, initially 11]*\n"
		 "q or Q or <interrupt>   Exit from more\n"
		 "s                       Skip forward k lines of text [1]\n"
		 "f                       Skip forward k screenfuls of text [1]\n"
		 "b or ctrl-B             Skip backwards k screenfuls of text [1]\n"
		 "'                       Go to place where previous search started\n"
		 "=                       Display current line number\n"
		 "/<regular expression>   Search for kth occurrence of regular expression [1]\n"
		 "n                       Search for kth occurrence of last r.e [1]\n"
		 "!<cmd> or :!<cmd>       Execute <cmd> in a subshell\n"
		 "v                       Start up '%s' at current line\n"
		 "ctrl-L                  Redraw screen\n"
		 ":n                      Go to kth next file [1]\n"
		 ":p                      Go to kth previous file [1]\n"
		 ":f                      Display current file name and line number\n"
		 ".                       Repeat previous command\n"),
		find_editor());
	print_separator('-', 79);
}

static void execute_editor(struct more_control *ctl, char *cmdbuf, size_t buflen, char *filename)
{
	char *editor, *p;
	int split = 0;
	int n;

	if ((ctl->current_line - ctl->lines_per_screen) < 1)
		n = 1;
	else
		n = ctl->current_line - (ctl->lines_per_screen + 1) / 2;
	editor = find_editor();
	p = strrchr(editor, '/');
	if (p)
		p++;
	else
		p = editor;
	/*
	 * Earlier: call vi +n file. This also works for emacs.
	 * POSIX: call vi -c n file (when editor is vi or ex).
	 */
	if (!strcmp(p, "vi") || !strcmp(p, "ex")) {
		snprintf(cmdbuf, buflen, "-c %d", n);
		split = 1;
	} else
		snprintf(cmdbuf, buflen, "+%d", n);

	erase_to_col(ctl, 0);
	printf("%s %s %s", editor, cmdbuf, ctl->file_names[ctl->argv_position]);
	if (split) {
		cmdbuf[2] = 0;
		execute(ctl, filename, editor, editor,
			cmdbuf, cmdbuf + 3,
			ctl->file_names[ctl->argv_position], (char *)0);
	} else
		execute(ctl, filename, editor, editor,
			cmdbuf, ctl->file_names[ctl->argv_position], (char *)0);
}

static int skip_backwards(struct more_control *ctl, int nlines)
{
	if (nlines == 0)
		nlines++;
	erase_to_col(ctl, 0);
	printf(P_("...back %d page", "...back %d pages", nlines), nlines);
	putchar('\n');
	ctl->next_jump = ctl->current_line - (ctl->lines_per_screen * (nlines + 1)) - 1;
	if (ctl->next_jump < 0)
		ctl->next_jump = 0;
	more_fseek(ctl, 0);
	ctl->current_line = 0;
	skip_lines(ctl);
	return ctl->lines_per_screen;
}

static int skip_forwards(struct more_control *ctl, int nlines, cc_t comchar)
{
	int c;

	if (nlines == 0)
		nlines++;
	if (comchar == 'f')
		nlines *= ctl->lines_per_screen;
	putchar('\r');
	erase_to_col(ctl, 0);
	putchar('\n');
	if (ctl->clear_line_ends)
		putp(ctl->erase_line);
	printf(P_("...skipping %d line",
		  "...skipping %d lines", nlines), nlines);

	if (ctl->clear_line_ends)
		putp(ctl->erase_line);
	putchar('\n');

	while (nlines > 0) {
		while ((c = more_getc(ctl)) != '\n')
			if (c == EOF)
				return 0;
		ctl->current_line++;
		nlines--;
	}
	return 1;
}

/* Read a command and do it.  A command consists of an optional integer
 * argument followed by the command character.  Return the number of
 * lines to display in the next screenful.  If there is nothing more to
 * display in the current file, zero is returned. */
static int more_key_command(struct more_control *ctl, char *filename)
{
	int retval = 0;
	int done = 0, search_again = 0;
	char cmdbuf[INIT_BUF];
	struct number_command cmd;

	if (!ctl->report_errors)
		output_prompt(ctl, filename);
	else
		ctl->report_errors = 0;
	ctl->search_called = 0;
	for (;;) {
		if (more_poll(ctl, -1) <= 0)
			continue;
		cmd = read_command(ctl);
		if (cmd.key == more_kc_unknown_command)
			continue;
		if (cmd.key == more_kc_repeat_previous)
			cmd = ctl->previous_command;
		switch (cmd.key) {
		case more_kc_backwards:
			if (ctl->no_tty_in) {
				fprintf(stderr, "\a");
				return -1;
			}
			retval = skip_backwards(ctl, cmd.number);
			done = 1;
			break;
		case more_kc_jump_lines_per_screen:
		case more_kc_set_lines_per_screen:
			if (cmd.number == 0)
				cmd.number = ctl->lines_per_screen;
			else if (cmd.key == more_kc_set_lines_per_screen)
				ctl->lines_per_screen = cmd.number;
			retval = cmd.number;
			done = 1;
			break;
		case more_kc_set_scroll_len:
			if (cmd.number != 0)
				ctl->d_scroll_len = cmd.number;
			retval = ctl->d_scroll_len;
			done = 1;
			break;
		case more_kc_quit:
			more_exit(ctl);
		case more_kc_skip_forward_screen:
			if (skip_forwards(ctl, cmd.number, 'f'))
				retval = ctl->lines_per_screen;
			done = 1;
			break;
		case more_kc_skip_forward_line:
			if (skip_forwards(ctl, cmd.number, 's'))
				retval = ctl->lines_per_screen;
			done = 1;
			break;
		case more_kc_next_line:
			if (cmd.number != 0)
				ctl->lines_per_screen = cmd.number;
			else
				cmd.number = 1;
			retval = cmd.number;
			done = 1;
			break;
		case more_kc_clear_screen:
			if (!ctl->no_tty_in) {
				more_clear_screen(ctl);
				more_fseek(ctl, ctl->screen_start.row_num);
				ctl->current_line = ctl->screen_start.line_num;
				retval = ctl->lines_per_screen;
				done = 1;
				break;
			} else {
				fprintf(stderr, "\a");
				break;
			}
		case more_kc_previous_search_match:
			if (!ctl->no_tty_in) {
				erase_to_col(ctl, 0);
				fputs(_("\n***Back***\n\n"), stdout);
				more_fseek(ctl, ctl->context.row_num);
				ctl->current_line = ctl->context.line_num;
				retval = ctl->lines_per_screen;
				done = 1;
				break;
			} else {
				fprintf(stderr, "\a");
				break;
			}
		case more_kc_display_line:
			erase_to_col(ctl, 0);
			ctl->prompt_len = printf("%d", ctl->current_line);
			fflush(NULL);
			break;
		case more_kc_display_file_and_line:
			erase_to_col(ctl, 0);
			if (!ctl->no_tty_in)
				ctl->prompt_len =
				    printf(_("\"%s\" line %d"),
				           ctl->file_names[ctl->argv_position], ctl->current_line);
			else
				ctl->prompt_len = printf(_("[Not a file] line %d"),
							 ctl->current_line);
			fflush(NULL);
			break;
		case more_kc_repeat_search:
			if (!ctl->previous_search) {
				more_error(ctl, _("No previous regular expression"));
				break;
			}
			search_again = 1;
			/* fallthrough */
		case more_kc_search:
			if (cmd.number == 0)
				cmd.number++;
			erase_to_col(ctl, 0);
			putchar('/');
			ctl->prompt_len = 1;
			fflush(NULL);
			if (search_again) {
				fputc('\r', stderr);
				search(ctl, ctl->previous_search, cmd.number);
				search_again = 0;
			} else {
				ttyin(ctl, cmdbuf, sizeof(cmdbuf) - 2, '/');
				fputc('\r', stderr);
				ctl->next_search = xstrdup(cmdbuf);
				search(ctl, ctl->next_search, cmd.number);
			}
			retval = ctl->lines_per_screen - 1;
			done = 1;
			break;
		case more_kc_run_shell:
			run_shell(ctl, filename);
			break;
		case more_kc_help:
			if (ctl->no_scroll)
				more_clear_screen(ctl);
			erase_to_col(ctl, 0);
			runtime_usage();
			output_prompt(ctl, filename);
			break;
		case more_kc_next_file:
			putchar('\r');
			erase_to_col(ctl, 0);
			if (cmd.number == 0)
				cmd.number = 1;
			if (ctl->argv_position + cmd.number >= (unsigned int)ctl->num_files)
				more_exit(ctl);
			change_file(ctl, cmd.number);
			done = 1;
			break;
		case more_kc_previous_file:
			if (ctl->no_tty_in) {
				fprintf(stderr, "\a");
				break;
			}
			putchar('\r');
			erase_to_col(ctl, 0);
			if (cmd.number == 0)
				cmd.number = 1;
			change_file(ctl, -cmd.number);
			done = 1;
			break;
		case more_kc_run_editor:	/* This case should go right before default */
			if (!ctl->no_tty_in) {
				execute_editor(ctl, cmdbuf, sizeof(cmdbuf), filename);
				break;
			}
			/* fallthrough */
		default:
			if (ctl->suppress_bell) {
				erase_to_col(ctl, 0);
				if (ctl->enter_std)
					putp(ctl->enter_std);
				ctl->prompt_len =
				    printf(_("[Press 'h' for instructions.]"))
					    + 2 * ctl->stdout_glitch;
				if (ctl->exit_std)
					putp(ctl->exit_std);
			} else
				fprintf(stderr, "\a");
			fflush(NULL);
			break;
		}
		ctl->previous_command = cmd;
		if (done) {
			cmd.key = more_kc_unknown_command;
			break;
		}
	}
	putchar('\r');
	ctl->no_quit_dialog = 1;
	return retval;
}

/* Print out the contents of the file f, one screenful at a time. */
static void screen(struct more_control *ctl, int num_lines)
{
	int c;
	int nchars;
	int length;			/* length of current line */
	static int prev_len = 1;	/* length of previous line */

	for (;;) {
		while (num_lines > 0 && !ctl->is_paused) {
			nchars = get_line(ctl, &length);
			ctl->is_eof = nchars == EOF;
			if (ctl->is_eof && ctl->exit_on_eof) {
				if (ctl->clear_line_ends)
					putp(ctl->clear_rest);
				return;
			}
			if (ctl->squeeze_spaces && length == 0 && prev_len == 0 && !ctl->is_eof)
				continue;
			prev_len = length;
			if (ctl->bad_stdout
			    || ((ctl->enter_std && *ctl->enter_std == ' ') && (ctl->prompt_len > 0)))
				erase_to_col(ctl, 0);
			/* must clear before drawing line since tabs on
			 * some terminals do not erase what they tab
			 * over. */
			if (ctl->clear_line_ends)
				putp(ctl->erase_line);
			fwrite(ctl->line_buf, length, 1, stdout);
			if (nchars < ctl->prompt_len)
				erase_to_col(ctl, nchars);
			ctl->prompt_len = 0;
			if (nchars < ctl->num_columns || !ctl->fold_long_lines)
				putchar('\n');
			num_lines--;
		}
		fflush(NULL);

		c = more_getc(ctl);
		ctl->is_eof = c == EOF;

		if (ctl->is_eof && ctl->exit_on_eof) {
			if (ctl->clear_line_ends)
				putp(ctl->clear_rest);
			return;
		}

		if (ctl->is_paused && ctl->clear_line_ends)
			putp(ctl->clear_rest);
		more_ungetc(ctl, c);
		ctl->is_paused = 0;
		do {
			if ((num_lines = more_key_command(ctl, NULL)) == 0)
				return;
		} while (ctl->search_called && !ctl->previous_search);
		if (ctl->hard_tty && ctl->prompt_len > 0)
			erase_to_col(ctl, 0);
		if (ctl->no_scroll && num_lines >= ctl->lines_per_screen) {
			if (ctl->clear_line_ends)
				putp(ctl->go_home);
			else
				more_clear_screen(ctl);
		}
		ctl->screen_start.line_num = ctl->current_line;
		ctl->screen_start.row_num = ctl->file_position;
	}
}

static void copy_file(FILE *f)
{
	char buf[BUFSIZ];
	size_t sz;

	while ((sz = fread(&buf, sizeof(char), sizeof(buf), f)) > 0)
		fwrite(&buf, sizeof(char), sz, stdout);
}


static void display_file(struct more_control *ctl, int left)
{
	if (!ctl->current_file)
		return;
	ctl->context.line_num = ctl->context.row_num = 0;
	ctl->current_line = 0;
	if (ctl->first_file) {
		ctl->first_file = 0;
		if (ctl->next_jump)
			skip_lines(ctl);
		if (ctl->search_at_start) {
			search(ctl, ctl->next_search, 1);
			if (ctl->no_scroll)
				left--;
		}
	} else if (ctl->argv_position < ctl->num_files && !ctl->no_tty_out)
		left =
		    more_key_command(ctl, ctl->file_names[ctl->argv_position]);
	if (left != 0) {
		if ((ctl->no_scroll || ctl->clear_first)
		    && 0 < ctl->file_size) {
			if (ctl->clear_line_ends)
				putp(ctl->go_home);
			else
				more_clear_screen(ctl);
		}
		if (ctl->print_banner) {
			if (ctl->bad_stdout)
				erase_to_col(ctl, 0);
			if (ctl->clear_line_ends)
				putp(ctl->erase_line);
			if (ctl->prompt_len > 14)
				erase_to_col(ctl, 14);
			if (ctl->clear_line_ends)
				putp(ctl->erase_line);
			print_separator(':', 14);
			if (ctl->clear_line_ends)
				putp(ctl->erase_line);
			puts(ctl->file_names[ctl->argv_position]);
			if (ctl->clear_line_ends)
				putp(ctl->erase_line);
			print_separator(':', 14);
			if (left > ctl->lines_per_page - 4)
				left = ctl->lines_per_page - 4;
		}
		if (ctl->no_tty_out)
			copy_file(ctl->current_file);
		else
			screen(ctl, left);
	}
	fflush(NULL);
	fclose(ctl->current_file);
	ctl->current_file = NULL;
	ctl->screen_start.line_num = ctl->screen_start.row_num = 0;
	ctl->context.line_num = ctl->context.row_num = 0L;
}

static void initterm(struct more_control *ctl)
{
	int ret;
	char *term;
	struct winsize win;
	char *cursor_addr;

#ifndef NON_INTERACTIVE_MORE
	ctl->no_tty_out = tcgetattr(STDOUT_FILENO, &ctl->output_tty);
#endif
	ctl->no_tty_in = tcgetattr(STDIN_FILENO, &ctl->output_tty);
	ctl->no_tty_err = tcgetattr(STDERR_FILENO, &ctl->output_tty);
	ctl->original_tty = ctl->output_tty;

	ctl->hard_tabs = (ctl->output_tty.c_oflag & TABDLY) != TAB3;
	if (ctl->no_tty_out)
		return;

	ctl->output_tty.c_lflag &= ~(ICANON | ECHO);
	ctl->output_tty.c_cc[VMIN] = 1;
	ctl->output_tty.c_cc[VTIME] = 0;
	ctl->erase_previous_ok = (ctl->output_tty.c_cc[VERASE] != 255);
	ctl->erase_input_ok = (ctl->output_tty.c_cc[VKILL] != 255);
	if ((term = getenv("TERM")) == NULL) {
		ctl->dumb_tty = 1;
	}
	setupterm(term, 1, &ret);
	if (ret <= 0) {
		ctl->dumb_tty = 1;
		return;
	}
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) < 0) {
		ctl->lines_per_page = tigetnum(TERM_LINES);
		ctl->num_columns = tigetnum(TERM_COLS);
	} else {
		if ((ctl->lines_per_page = win.ws_row) == 0)
			ctl->lines_per_page = tigetnum(TERM_LINES);
		if ((ctl->num_columns = win.ws_col) == 0)
			ctl->num_columns = tigetnum(TERM_COLS);
	}
	if ((ctl->lines_per_page <= 0) || tigetflag(TERM_HARD_COPY)) {
		ctl->hard_tty = 1;
		ctl->lines_per_page = LINES_PER_PAGE;
	}

	if (tigetflag(TERM_EAT_NEW_LINE))
		/* Eat newline at last column + 1; dec, concept */
		ctl->eat_newline++;
	if (ctl->num_columns <= 0)
		ctl->num_columns = NUM_COLUMNS;

	ctl->wrap_margin = tigetflag(TERM_AUTO_RIGHT_MARGIN);
	ctl->bad_stdout = tigetflag(TERM_CEOL);
	ctl->erase_line = tigetstr(TERM_CLEAR_TO_LINE_END);
	ctl->clear = tigetstr(TERM_CLEAR);
	if ((ctl->enter_std = tigetstr(TERM_STANDARD_MODE)) != NULL) {
		ctl->exit_std = tigetstr(TERM_EXIT_STANDARD_MODE);
		if (0 < tigetnum(TERM_STD_MODE_GLITCH))
			ctl->stdout_glitch = 1;
	}

	cursor_addr = tigetstr(TERM_HOME);
	if (cursor_addr == NULL || *cursor_addr == '\0') {
		cursor_addr = tigetstr(TERM_CURSOR_ADDRESS);
		if (cursor_addr)
			cursor_addr = tparm(cursor_addr, 0, 0);
	}
	if (cursor_addr)
		ctl->go_home = xstrdup(cursor_addr);

	if ((ctl->move_line_down = tigetstr(TERM_LINE_DOWN)) == NULL)
		ctl->move_line_down = BACKSPACE;
	ctl->clear_rest = tigetstr(TERM_CLEAR_TO_SCREEN_END);
	if ((ctl->backspace_ch = tigetstr(TERM_BACKSPACE)) == NULL)
		ctl->backspace_ch = BACKSPACE;

	if ((ctl->shell = getenv("SHELL")) == NULL)
		ctl->shell = _PATH_BSHELL;
}

int main(int argc, char **argv)
{
	char *s;
	int left;
	struct more_control ctl = {
		.first_file = 1,
		.fold_long_lines = 1,
		.no_quit_dialog = 1,
		.stop_after_formfeed = 1,
		.wrap_margin = 1,
		.lines_per_page = LINES_PER_PAGE,
		.num_columns = NUM_COLUMNS,
		.d_scroll_len = SCROLL_LEN,
		0
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();
	setlocale(LC_ALL, "");

	/* Auto set no scroll on when binary is called page */
	if (!(strcmp(program_invocation_short_name, "page")))
		ctl.no_scroll++;

	ctl.exit_on_eof = getenv("POSIXLY_CORRECT") ? 0 : 1;

	if ((s = getenv("MORE")) != NULL)
		env_argscan(&ctl, s);

	argscan(&ctl, argc, argv);

	/* clear any inherited settings */
	signal(SIGCHLD, SIG_DFL);

	initterm(&ctl);

	if (ctl.no_tty_err)
		/* exit when we cannot read user's input */
		ctl.exit_on_eof = 1;

#ifdef HAVE_MAGIC
	ctl.magic = magic_open(MAGIC_MIME_ENCODING | MAGIC_SYMLINK);
	magic_load(ctl.magic, NULL);
#endif
	prepare_line_buffer(&ctl);

	ctl.d_scroll_len = ctl.lines_per_page / 2 - 1;
	if (ctl.d_scroll_len <= 0)
		ctl.d_scroll_len = 1;

	/* allow clear_line_ends only if go_home and erase_line and clear_rest strings are
	 * defined, and in that case, make sure we are in no_scroll mode */
	if (ctl.clear_line_ends) {
		if ((ctl.go_home == NULL) || (*ctl.go_home == '\0') ||
		    (ctl.erase_line == NULL) || (*ctl.erase_line == '\0') ||
		    (ctl.clear_rest == NULL) || (*ctl.clear_rest == '\0'))
			ctl.clear_line_ends = 0;
		else
			ctl.no_scroll = 1;
	}
	if (ctl.lines_per_screen == 0)
		ctl.lines_per_screen = ctl.lines_per_page - 1;
	left = ctl.lines_per_screen;
	if (ctl.num_files > 1)
		ctl.print_banner = 1;
	if (!ctl.no_tty_in && ctl.num_files == 0) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	} else
		ctl.current_file = stdin;
	if (!ctl.no_tty_out) {
		if (signal(SIGTSTP, SIG_IGN) == SIG_DFL) {
			ctl.catch_suspend++;
		}
		tcsetattr(STDERR_FILENO, TCSANOW, &ctl.output_tty);
	}
	sigemptyset(&ctl.sigset);
	sigaddset(&ctl.sigset, SIGINT);
	sigaddset(&ctl.sigset, SIGQUIT);
	sigaddset(&ctl.sigset, SIGTSTP);
	sigaddset(&ctl.sigset, SIGCONT);
	sigaddset(&ctl.sigset, SIGWINCH);
	sigprocmask(SIG_BLOCK, &ctl.sigset, NULL);
	ctl.sigfd = signalfd(-1, &ctl.sigset, SFD_CLOEXEC);
	if (ctl.no_tty_in) {
		if (ctl.no_tty_out)
			copy_file(stdin);
		else {
			ctl.current_file = stdin;
			display_file(&ctl, left);
		}
		ctl.no_tty_in = 0;
		ctl.print_banner = 1;
		ctl.first_file = 0;
	}

	while (ctl.argv_position < ctl.num_files) {
		checkf(&ctl, ctl.file_names[ctl.argv_position]);
		display_file(&ctl, left);
		ctl.first_file = 0;
		ctl.argv_position++;
	}
	ctl.clear_line_ends = 0;
	ctl.prompt_len = 0;
	more_exit(&ctl);
}
