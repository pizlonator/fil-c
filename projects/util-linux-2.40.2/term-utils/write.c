/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jef Poskanzer and Craig Leres of the Lawrence Berkeley Laboratory.
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
 * Modified for Linux, Mon Mar  8 18:16:24 1993, faith@cs.unc.edu
 * Wed Jun 22 21:41:56 1994, faith@cs.unc.edu:
 *      Added fix from Mike Grupenhoff (kashmir@umiacs.umd.edu)
 * Mon Jul  1 17:01:39 MET DST 1996, janl@math.uio.no:
 *      - Added fix from David.Chapell@mail.trincoll.edu enabling daemons
 *	  to use write.
 *      - ANSIed it since I was working on it anyway.
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */

#include <errno.h>
#include <getopt.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#if defined(USE_SYSTEMD) && HAVE_DECL_SD_SESSION_GET_USERNAME == 1
# include <systemd/sd-login.h>
# include <systemd/sd-daemon.h>
#endif

#include "c.h"
#include "carefulputc.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "ttyutils.h"
#include "xalloc.h"

static volatile sig_atomic_t signal_received = 0;

struct write_control {
	uid_t src_uid;
	const char *src_login;
	const char *src_tty_path;
	const char *src_tty_name;
	const char *dst_login;
	char *dst_tty_path;
	const char *dst_tty_name;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] <user> [<ttyname>]\n"),
	      program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Send a message to another user.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("write(1)"));
	exit(EXIT_SUCCESS);
}

/*
 * check_tty - check that a terminal exists, and get the message bit
 *     and the access time
 */
static int check_tty(const char *tty, int *tty_writeable, time_t *tty_atime, int showerror)
{
	struct stat s;

	if (stat(tty, &s) < 0) {
		if (showerror)
			warn("%s", tty);
		return 1;
	}
	if (getuid() == 0)	/* root can always write */
		*tty_writeable = 1;
	else {
		if (getegid() != s.st_gid) {
			warnx(_("effective gid does not match group of %s"), tty);
			return 1;
		}
		*tty_writeable = s.st_mode & S_IWGRP;
	}
	if (tty_atime)
		*tty_atime = s.st_atime;
	return 0;
}

/*
 * check_utmp - checks that the given user is actually logged in on
 *     the given tty
 */
static int check_utmp(const struct write_control *ctl)
{
	struct utmpx *u;
	int res = 1;
#if defined(USE_SYSTEMD) && HAVE_DECL_SD_SESSION_GET_USERNAME == 1
	if (sd_booted() > 0) {
		char **sessions_list;
		int sessions = sd_get_sessions(&sessions_list);
		if (sessions < 0)
			errx(EXIT_FAILURE, _("error getting sessions: %s"),
				strerror(-sessions));

		for (int i = 0; i < sessions; i++) {

			char *name, *tty;
			int r;

			if ((r = sd_session_get_username(sessions_list[i], &name)) < 0)
				errx(EXIT_FAILURE, _("get user name failed: %s"), strerror (-r));
			if (sd_session_get_tty(sessions_list[i], &tty) < 0) {
				free(name);
				continue;
			}

			if  (strcmp(ctl->dst_login, name) == 0 &&
					strcmp(ctl->dst_tty_name, tty) == 0) {
				free(name);
				free(tty);
				res = 0;
				break;
			}
			free(name);
			free(tty);
		}
		for (int i = 0; i < sessions; i++)
			free(sessions_list[i]);
		free(sessions_list);
	} else {
#endif
		utmpxname(_PATH_UTMP);
		setutxent();

		while ((u = getutxent())) {
			if (strncmp(ctl->dst_login, u->ut_user, sizeof(u->ut_user)) == 0 &&
		    		strncmp(ctl->dst_tty_name, u->ut_line, sizeof(u->ut_line)) == 0) {
				res = 0;
				break;
			}
		}

		endutxent();
#if defined(USE_SYSTEMD) && HAVE_DECL_SD_SESSION_GET_USERNAME == 1
	}
#endif
	return res;
}

/*
 * search_utmp - search utmp for the "best" terminal to write to
 *
 * Ignores terminals with messages disabled, and of the rest, returns
 * the one with the most recent access time.  Returns as value the number
 * of the user's terminals with messages enabled, or -1 if the user is
 * not logged in at all.
 *
 * Special case for writing to yourself - ignore the terminal you're
 * writing from, unless that's the only terminal with messages enabled.
 */
static void search_utmp(struct write_control *ctl)
{
	struct utmpx *u;
	time_t best_atime = 0, tty_atime;
	int num_ttys = 0, valid_ttys = 0, tty_writeable = 0, user_is_me = 0;

#if defined(USE_SYSTEMD) && HAVE_DECL_SD_SESSION_GET_USERNAME == 1
	if (sd_booted() > 0) {
		char path[256];
		char **sessions_list;
		int sessions = sd_get_sessions(&sessions_list);
		if (sessions < 0)
			errx(EXIT_FAILURE, _("error getting sessions: %s"),
			     strerror(-sessions));

		for (int i = 0; i < sessions; i++) {
			char *name, *tty;
			int r;

			if ((r = sd_session_get_username(sessions_list[i], &name)) < 0)
				errx(EXIT_FAILURE, _("get user name failed: %s"), strerror (-r));

			if  (strcmp(ctl->dst_login, name) != 0) {
				free(name);
				continue;
			}

			if (sd_session_get_tty(sessions_list[i], &tty) < 0) {
				free(name);
				continue;
			}

			num_ttys++;
			snprintf(path, sizeof(path), "/dev/%s", tty);
			if (check_tty(path, &tty_writeable, &tty_atime, 0)) {
				/* bad term? skip */
				free(name);
				free(tty);
				continue;
			}
			if (ctl->src_uid && !tty_writeable) {
				/* skip ttys with msgs off */
				free(name);
				free(tty);
				continue;
			}
			if (strcmp(tty, ctl->src_tty_name) == 0) {
				user_is_me = 1;
				free(name);
				free(tty);
				/* don't write to yourself */
				continue;
			}
			valid_ttys++;
			if (best_atime < tty_atime) {
				best_atime = tty_atime;
				free(ctl->dst_tty_path);
				ctl->dst_tty_path = xstrdup(path);
				ctl->dst_tty_name = ctl->dst_tty_path + 5;
			}
			free(name);
			free(tty);
		}
		for (int i = 0; i < sessions; i++)
			free(sessions_list[i]);
		free(sessions_list);
	} else
#endif
	{
		char path[sizeof(u->ut_line) + 6];

		utmpxname(_PATH_UTMP);
		setutxent();

		while ((u = getutxent())) {
			if (strncmp(ctl->dst_login, u->ut_user, sizeof(u->ut_user)) != 0)
				continue;
			num_ttys++;
			snprintf(path, sizeof(path), "/dev/%s", u->ut_line);
			if (check_tty(path, &tty_writeable, &tty_atime, 0))
				/* bad term? skip */
				continue;
			if (ctl->src_uid && !tty_writeable)
				/* skip ttys with msgs off */
				continue;
			if (memcmp(u->ut_line, ctl->src_tty_name, strlen(ctl->src_tty_name) + 1) == 0) {
				user_is_me = 1;
				/* don't write to yourself */
				continue;
			}
			if (u->ut_type != USER_PROCESS)
				/* it's not a valid entry */
				continue;
			valid_ttys++;
			if (best_atime < tty_atime) {
				best_atime = tty_atime;
				free(ctl->dst_tty_path);
				ctl->dst_tty_path = xstrdup(path);
				ctl->dst_tty_name = ctl->dst_tty_path + 5;
			}
		}

		endutxent();
	}

	if (num_ttys == 0)
		errx(EXIT_FAILURE, _("%s is not logged in"), ctl->dst_login);
	if (valid_ttys == 0) {
		if (user_is_me) {
			/* ok, so write to yourself! */
			if (!ctl->src_tty_path)
				errx(EXIT_FAILURE, _("can't find your tty's name"));
			ctl->dst_tty_path = xstrdup(ctl->src_tty_path);
			ctl->dst_tty_name = ctl->dst_tty_path + 5;
			return;
		}
		errx(EXIT_FAILURE, _("%s has messages disabled"), ctl->dst_login);
	}
	if (1 < valid_ttys)
		warnx(_("%s is logged in more than once; writing to %s"),
		      ctl->dst_login, ctl->dst_tty_name);
}

/*
 * signal_handler - cause write loop to exit
 */
static void signal_handler(int signo)
{
	signal_received = signo;
}

/*
 * do_write - actually make the connection
 */
static void do_write(const struct write_control *ctl)
{
	char *login, *pwuid;
	struct passwd *pwd;
	time_t now;
	struct tm *tm;
	char *host, *line = NULL;
	size_t linelen = 0;
	struct sigaction sigact;

	/* Determine our login name(s) before the we reopen() stdout */
	if ((pwd = getpwuid(ctl->src_uid)) != NULL)
		pwuid = pwd->pw_name;
	else
		pwuid = "???";
	if ((login = getlogin()) == NULL)
		login = pwuid;

	if ((freopen(ctl->dst_tty_path, "w", stdout)) == NULL)
		err(EXIT_FAILURE, "%s", ctl->dst_tty_path);

	sigact.sa_handler = signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);

	host = xgethostname();
	if (!host)
		host = xstrdup("???");

	now = time((time_t *)NULL);
	tm = localtime(&now);
	/* print greeting */
	printf("\r\n\a\a\a");
	if (strcmp(login, pwuid) != 0)
		printf(_("Message from %s@%s (as %s) on %s at %02d:%02d ..."),
		       login, host, pwuid, ctl->src_tty_name,
		       tm->tm_hour, tm->tm_min);
	else
		printf(_("Message from %s@%s on %s at %02d:%02d ..."),
		       login, host, ctl->src_tty_name,
		       tm->tm_hour, tm->tm_min);
	free(host);
	printf("\r\n");

	while (getline(&line, &linelen, stdin) >= 0) {
		if (signal_received)
			break;

		if (fputs_careful(line, stdout, '^', true, 0) == EOF)
			err(EXIT_FAILURE, _("carefulputc failed"));
	}
	free(line);
	printf("EOF\r\n");
}

int main(int argc, char **argv)
{
	int tty_writeable = 0, c;
	struct write_control ctl = { 0 };

	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (c) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	if (get_terminal_name(&ctl.src_tty_path, &ctl.src_tty_name, NULL) == 0) {
		/* check that sender has write enabled */
		if (check_tty(ctl.src_tty_path, &tty_writeable, NULL, 1))
			exit(EXIT_FAILURE);
		if (!tty_writeable)
			errx(EXIT_FAILURE,
			     _("you have write permission turned off"));
		tty_writeable = 0;
	} else
		ctl.src_tty_name = "<no tty>";

	ctl.src_uid = getuid();

	/* check args */
	switch (argc) {
	case 2:
		ctl.dst_login = argv[1];
		search_utmp(&ctl);
		do_write(&ctl);
		break;
	case 3:
		ctl.dst_login = argv[1];
		if (!strncmp(argv[2], "/dev/", 5))
			ctl.dst_tty_path = xstrdup(argv[2]);
		else
			xasprintf(&ctl.dst_tty_path, "/dev/%s", argv[2]);
		ctl.dst_tty_name = ctl.dst_tty_path + 5;
		if (check_utmp(&ctl))
			errx(EXIT_FAILURE,
			     _("%s is not logged in on %s"),
			     ctl.dst_login, ctl.dst_tty_name);
		if (check_tty(ctl.dst_tty_path, &tty_writeable, NULL, 1))
			exit(EXIT_FAILURE);
		if (ctl.src_uid && !tty_writeable)
			errx(EXIT_FAILURE,
			     _("%s has messages disabled on %s"),
			     ctl.dst_login, ctl.dst_tty_name);
		do_write(&ctl);
		break;
	default:
		errtryhelp(EXIT_FAILURE);
	}
	free(ctl.dst_tty_path);
	return EXIT_SUCCESS;
}
