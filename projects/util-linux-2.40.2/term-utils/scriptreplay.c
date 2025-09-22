/*
 * Copyright (C) 2008-2019, Karel Zak <kzak@redhat.com>
 * Copyright (C) 2008, James Youngman <jay@gnu.org>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Based on scriptreplay.pl by Joey Hess <joey@kitenet.net>
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <termios.h>

#include "c.h"
#include "xalloc.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "optutils.h"
#include "script-playutils.h"

static void __attribute__((__noreturn__))
usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options]\n"),
	      program_invocation_short_name);
	fprintf(out,
	      _(" %s [-t] timingfile [typescript] [divisor]\n"),
	      program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Play back terminal typescripts, using timing information.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -t, --timing <file>     script timing log file\n"), out);
	fputs(_(" -T, --log-timing <file> alias to -t\n"), out);
	fputs(_(" -I, --log-in <file>     script stdin log file\n"), out);
	fputs(_(" -O, --log-out <file>    script stdout log file (default)\n"), out);
	fputs(_(" -B, --log-io <file>     script stdin and stdout log file\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -s, --typescript <file> deprecated alias to -O\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("     --summary           display overview about recorded session and exit\n"), out);
	fputs(_(" -d, --divisor <num>     speed up or slow down execution with time divisor\n"), out);
	fputs(_(" -m, --maxdelay <num>    wait at most this many seconds between updates\n"), out);
	fputs(_(" -x, --stream <name>     stream type (out, in, signal or info)\n"), out);
	fputs(_(" -c, --cr-mode <type>    CR char mode (auto, never, always)\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(25));

	fprintf(out, USAGE_MAN_TAIL("scriptreplay(1)"));
	exit(EXIT_SUCCESS);
}

static double
getnum(const char *s)
{
	const double d = strtod_or_err(s, _("failed to parse number"));

	if (isnan(d)) {
		errno = EINVAL;
		err(EXIT_FAILURE, "%s: %s", _("failed to parse number"), s);
	}
	return d;
}

static void
delay_for(struct timeval *delay)
{
#ifdef HAVE_NANOSLEEP
	struct timespec ts, remainder;
	ts.tv_sec = (time_t) delay->tv_sec;
	ts.tv_nsec = delay->tv_usec * 1000;

	DBG(TIMING, ul_debug("going to sleep for %"PRId64".%06"PRId64,
			(int64_t) delay->tv_sec, (int64_t) delay->tv_usec));

	while (-1 == nanosleep(&ts, &remainder)) {
		if (EINTR == errno)
			ts = remainder;
		else
			break;
	}
#else
	select(0, NULL, NULL, NULL, delay);
#endif
}

static void
appendchr(char *buf, size_t bufsz, int c)
{
	size_t sz;

	if (strchr(buf, c))
		return;		/* already in */

	sz = strlen(buf);
	if (sz + 1 < bufsz)
		buf[sz] = c;
}

static int
setterm(struct termios *backup)
{
	struct termios tattr;

	if (tcgetattr(STDOUT_FILENO, backup) != 0) {
		if (errno != ENOTTY) /* For debugger. */
			err(EXIT_FAILURE, _("unexpected tcgetattr failure"));
		return 0;
	}
	tattr = *backup;
	cfmakeraw(&tattr);
	tattr.c_lflag |= ISIG;
	tattr.c_iflag |= IXON;
	tcsetattr(STDOUT_FILENO, TCSANOW, &tattr);
	return 1;
}

int
main(int argc, char *argv[])
{
	static const struct timeval mindelay = { .tv_sec = 0, .tv_usec = 100 };
	struct timeval maxdelay;

	int isterm;
	struct termios saved;

	struct replay_setup *setup = NULL;
	struct replay_step *step = NULL;
	char streams[6] = {0};		/* IOSI - in, out, signal,info */
	const char *log_out = NULL,
	           *log_in = NULL,
		   *log_io = NULL,
		   *log_tm = NULL;
	double divi = 1;
	int diviopt = FALSE, idx;
	int ch, rc, crmode = REPLAY_CRMODE_AUTO, summary = 0;
	enum {
		OPT_SUMMARY = CHAR_MAX + 1
	};

	static const struct option longopts[] = {
		{ "cr-mode",    required_argument,	0, 'c' },
		{ "timing",	required_argument,	0, 't' },
		{ "log-timing", required_argument,      0, 'T' },
		{ "log-in",     required_argument,      0, 'I' },
		{ "log-out",    required_argument,      0, 'O' },
		{ "log-io",     required_argument,      0, 'B' },
		{ "typescript",	required_argument,	0, 's' },
		{ "divisor",	required_argument,	0, 'd' },
		{ "maxdelay",	required_argument,	0, 'm' },
		{ "stream",     required_argument,	0, 'x' },
		{ "summary",    no_argument,            0, OPT_SUMMARY },
		{ "version",	no_argument,		0, 'V' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL,		0, 0, 0 }
	};
	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'O', 's' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	/* Because we use space as a separator, we can't afford to use any
	 * locale which tolerates a space in a number.  In any case, script.c
	 * sets the LC_NUMERIC locale to C, anyway.
	 */
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	replay_init_debug();
	timerclear(&maxdelay);

	while ((ch = getopt_long(argc, argv, "B:c:I:O:T:t:s:d:m:x:Vh", longopts, NULL)) != -1) {

		err_exclusive_options(ch, longopts, excl, excl_st);

		switch(ch) {
		case 'c':
			if (strcmp("auto", optarg) == 0)
				crmode = REPLAY_CRMODE_AUTO;
			else if (strcmp("never", optarg) == 0)
				crmode = REPLAY_CRMODE_NEVER;
			else if (strcmp("always", optarg) == 0)
				crmode = REPLAY_CRMODE_ALWAYS;
			else
				errx(EXIT_FAILURE, _("unsupported mode name: '%s'"), optarg);
			break;
		case 't':
		case 'T':
			log_tm = optarg;
			break;
		case 'O':
		case 's':
			log_out = optarg;
			break;
		case 'I':
			log_in = optarg;
			break;
		case 'B':
			log_io = optarg;
			break;
		case 'd':
			diviopt = TRUE;
			divi = getnum(optarg);
			break;
		case 'm':
			strtotimeval_or_err(optarg, &maxdelay, _("failed to parse maximal delay argument"));
			break;
		case 'x':
			if (strcmp("in", optarg) == 0)
				appendchr(streams, sizeof(streams), 'I');
			else if (strcmp("out", optarg) == 0)
				appendchr(streams, sizeof(streams), 'O');
			else if (strcmp("signal", optarg) == 0)
				appendchr(streams, sizeof(streams), 'S');
			else if (strcmp("info", optarg) == 0)
				appendchr(streams, sizeof(streams), 'H');
			else
				errx(EXIT_FAILURE, _("unsupported stream name: '%s'"), optarg);
			break;
		case OPT_SUMMARY:
			summary = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	idx = 0;

	if (summary)
		streams[0] = 'H', streams[1] = '\0';

	if (!log_tm && idx < argc)
		log_tm = argv[idx++];
	if (!log_out && !summary && !log_in && !log_io)
		log_out = idx < argc ? argv[idx++] : "typescript";

	if (!diviopt)
		divi = idx < argc ? getnum(argv[idx]) : 1;

	if (!log_tm)
		errx(EXIT_FAILURE, _("timing file not specified"));
	if (!(log_out || log_in || log_io) && !summary)
		errx(EXIT_FAILURE, _("data log file not specified"));

	setup = replay_new_setup();

	if (replay_set_timing_file(setup, log_tm) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_tm);

	if (log_out && replay_associate_log(setup, "O", log_out) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_out);

	if (log_in && replay_associate_log(setup, "I", log_in) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_in);

	if (log_io && replay_associate_log(setup, "IO", log_io) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_io);

	if (!*streams) {
		/* output is preferred default */
		if (log_out || log_io)
			appendchr(streams, sizeof(streams), 'O');
		else if (log_in)
			appendchr(streams, sizeof(streams), 'I');
	}

	replay_set_default_type(setup,
			*streams && streams[1] == '\0' ? *streams : 'O');
	replay_set_crmode(setup, crmode);

	if (divi != 1)
		replay_set_delay_div(setup, divi);
	if (timerisset(&maxdelay))
		replay_set_delay_max(setup, &maxdelay);
	replay_set_delay_min(setup, &mindelay);

	isterm = setterm(&saved);

	do {
		rc = replay_get_next_step(setup, streams, &step);
		if (rc)
			break;

		if (!summary) {
			struct timeval *delay = replay_step_get_delay(step);

			if (delay && timerisset(delay))
				delay_for(delay);
		}
		rc = replay_emit_step_data(setup, step, STDOUT_FILENO);
	} while (rc == 0);

	if (isterm)
		tcsetattr(STDOUT_FILENO, TCSADRAIN, &saved);

	if (step && rc < 0)
		err(EXIT_FAILURE, _("%s: log file error"), replay_step_get_filename(step));
	else if (rc < 0)
		err(EXIT_FAILURE, _("%s: line %d: timing file error"),
				replay_get_timing_file(setup),
				replay_get_timing_line(setup));
	printf("\n");
	replay_free_setup(setup);

	exit(EXIT_SUCCESS);
}
