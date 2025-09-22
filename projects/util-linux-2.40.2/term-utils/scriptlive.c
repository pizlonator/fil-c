/*
 * Copyright (C) 2019, Karel Zak <kzak@redhat.com>
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
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <paths.h>

#include "c.h"
#include "xalloc.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "optutils.h"
#include "pty-session.h"
#include "script-playutils.h"
#include "monotonic.h"


#define SCRIPT_MIN_DELAY 0.0001		/* from original scriptreplay.pl */

struct scriptlive {
	struct ul_pty *pty;
	struct replay_setup *setup;
	struct replay_step *step;
};

static void __attribute__((__noreturn__))
usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options]\n"),
	      program_invocation_short_name);
	fprintf(out,
	      _(" %s [-t] timingfile [-I|-B] typescript\n"),
	      program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Execute terminal typescript.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -t, --timing <file>     script timing log file\n"), out);
	fputs(_(" -T, --log-timing <file> alias to -t\n"), out);
	fputs(_(" -I, --log-in <file>     script stdin log file\n"), out);
	fputs(_(" -B, --log-io <file>     script stdin and stdout log file\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -c, --command <command> run command rather than interactive shell\n"), out);
	fputs(_(" -d, --divisor <num>     speed up or slow down execution with time divisor\n"), out);
	fputs(_(" -m, --maxdelay <num>    wait at most this many seconds between updates\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(25));

	fprintf(out, USAGE_MAN_TAIL("scriptlive(1)"));
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

static void callback_child_sigstop(
			void *data __attribute__((__unused__)),
			pid_t child)
{
	kill(getpid(), SIGSTOP);
	kill(child, SIGCONT);
}

static int process_next_step(struct scriptlive *ss)
{
	int rc = 0, fd = ul_pty_get_childfd(ss->pty);

	/* read next step(s) */
	do {
		struct timeval *delay;

		rc = replay_get_next_step(ss->setup, "I", &ss->step);
		if (rc == 1) {
			ul_pty_write_eof_to_child(ss->pty);
			rc = 0;
			break;
		}
		if (rc)
			break;

		delay = replay_step_get_delay(ss->step);
		if (timerisset(delay)) {
			/* wait until now+delay in mainloop */
			struct timeval now = { 0 }, target = { 0 };

			gettime_monotonic(&now);
			timeradd(&now, delay, &target);

			ul_pty_set_mainloop_time(ss->pty, &target);
			break;
		}

		/* no delay -- immediately write */
		rc = replay_emit_step_data(ss->setup, ss->step, fd);
		fdatasync(fd);
	} while (rc == 0);

	return rc;
}

static int mainloop_cb(void *data)
{
	struct scriptlive *ss = (struct scriptlive *) data;
	int rc = 0;

	/* emit previous waiting step */
	if (ss->step && !replay_step_is_empty(ss->step)) {
		int fd = ul_pty_get_childfd(ss->pty);;

		rc = replay_emit_step_data(ss->setup, ss->step, fd);
		fdatasync(fd);
		if (rc)
			return rc;
	}

	return process_next_step(ss);
}

int
main(int argc, char *argv[])
{
	static const struct timeval mindelay = { .tv_sec = 0, .tv_usec = 100 };
	struct timeval maxdelay;

	const char *log_in = NULL, *log_io = NULL, *log_tm = NULL,
		   *shell = NULL, *command = NULL;
	double divi = 1;
	int diviopt = FALSE, idx;
	int ch, caught_signal = 0;
	struct ul_pty_callbacks *cb;
	struct scriptlive ss = { .pty = NULL };
	pid_t child;

	static const struct option longopts[] = {
		{ "command",    required_argument,      0, 'c' },
		{ "timing",	required_argument,	0, 't' },
		{ "log-timing",	required_argument,	0, 'T' },
		{ "log-in",     required_argument,      0, 'I'},
		{ "log-io",     required_argument,      0, 'B'},
		{ "divisor",	required_argument,	0, 'd' },
		{ "maxdelay",	required_argument,	0, 'm' },
		{ "version",	no_argument,		0, 'V' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL,		0, 0, 0 }
	};
	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'B', 'I' },
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

	while ((ch = getopt_long(argc, argv, "c:B:I:T:t:d:m:Vh", longopts, NULL)) != -1) {

		err_exclusive_options(ch, longopts, excl, excl_st);

		switch(ch) {
		case 'c':
			command = optarg;
			break;
		case 't':
		case 'T':
			log_tm = optarg;
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

	if (!log_tm && idx < argc)
		log_tm = argv[idx++];
	if (!log_in && !log_io && idx < argc)
		log_in = argv[idx++];

	if (!diviopt)
		divi = idx < argc ? getnum(argv[idx]) : 1;

	if (!log_tm)
		errx(EXIT_FAILURE, _("timing file not specified"));
	if (!(log_in || log_io))
		errx(EXIT_FAILURE, _("stdin typescript file not specified"));

	ss.setup = replay_new_setup();

	if (replay_set_timing_file(ss.setup, log_tm) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_tm);

	if (log_in && replay_associate_log(ss.setup, "I", log_in) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_in);

	if (log_io && replay_associate_log(ss.setup, "IO", log_io) != 0)
		err(EXIT_FAILURE, _("cannot open %s"), log_io);

	replay_set_default_type(ss.setup, 'I');
	replay_set_crmode(ss.setup, REPLAY_CRMODE_NEVER);

	if (divi != 1)
		replay_set_delay_div(ss.setup, divi);
	if (timerisset(&maxdelay))
		replay_set_delay_max(ss.setup, &maxdelay);
	replay_set_delay_min(ss.setup, &mindelay);

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	fprintf(stdout, _(">>> scriptlive: Starting your typescript execution by %s.\n"),
			command ? command : shell);

	ul_pty_init_debug(0);

	ss.pty = ul_new_pty(isatty(STDIN_FILENO));
	if (!ss.pty)
		err(EXIT_FAILURE, _("failed to allocate PTY handler"));

	ul_pty_set_callback_data(ss.pty, (void *) &ss);
	cb = ul_pty_get_callbacks(ss.pty);
	cb->child_sigstop = callback_child_sigstop;
	cb->mainloop = mainloop_cb;

	if (!isatty(STDIN_FILENO))
		/* We keep ECHO flag for compatibility with script(1) */
		ul_pty_slave_echo(ss.pty, 1);

	if (ul_pty_setup(ss.pty))
		err(EXIT_FAILURE, _("failed to create pseudo-terminal"));
	if (ul_pty_signals_setup(ss.pty))
		err(EXIT_FAILURE, _("failed to initialize signals handler"));

	fflush(stdout);			/* ??? */

	switch ((int) (child = fork())) {
	case -1: /* error */
		ul_pty_cleanup(ss.pty);
		err(EXIT_FAILURE, _("cannot create child process"));
		break;

	case 0: /* child */
	{
		const char *shname;

		ul_pty_init_slave(ss.pty);

		signal(SIGTERM, SIG_DFL); /* because /etc/csh.login */

		shname = strrchr(shell, '/');
		shname = shname ? shname + 1 : shell;

		if (access(shell, X_OK) == 0) {
			if (command)
				execl(shell, shname, "-c", command, (char *)NULL);
			else
				execl(shell, shname, "-i", (char *)NULL);
		} else {
			if (command)
				execlp(shname, shname, "-c", command, (char *)NULL);
			else
				execlp(shname, shname, "-i", (char *)NULL);
		}
		err(EXIT_FAILURE, "failed to execute %s", shell);
		break;
	}
	default:
		break;
	}

	/* parent */
	ul_pty_set_child(ss.pty, child);

	/* read the first step and set initial delay for pty main loop; the
	 * next steps will be processed by mainloop_cb() */
	process_next_step(&ss);

	/* this is the main loop */
	ul_pty_proxy_master(ss.pty);

	/* all done; cleanup and kill */
	caught_signal = ul_pty_get_delivered_signal(ss.pty);

	if (!caught_signal && ul_pty_get_child(ss.pty) != (pid_t)-1)
		ul_pty_wait_for_child(ss.pty);	/* final wait */

	if (caught_signal && ul_pty_get_child(ss.pty) != (pid_t)-1) {
		fprintf(stderr, _("\nSession terminated, killing shell..."));
		kill(child, SIGTERM);
		sleep(2);
		kill(child, SIGKILL);
		fprintf(stderr, " ...killed.\n");
	}

	ul_pty_cleanup(ss.pty);
	ul_free_pty(ss.pty);
	replay_free_setup(ss.setup);

	fprintf(stdout, _("\n>>> scriptlive: done.\n"));
	return EXIT_SUCCESS;
}
