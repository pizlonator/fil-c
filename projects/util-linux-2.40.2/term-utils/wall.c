/*
 * Copyright (c) 1988, 1990, 1993
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
 *
 * Modified Sun Mar 12 10:34:34 1995, faith@cs.unc.edu, for Linux
 */

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <errno.h>
#include <paths.h>
#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>
#include <getopt.h>
#include <sys/types.h>
#include <grp.h>

#if defined(USE_SYSTEMD) && HAVE_DECL_SD_SESSION_GET_USERNAME == 1
# include <systemd/sd-login.h>
# include <systemd/sd-daemon.h>
#endif

#include "nls.h"
#include "xalloc.h"
#include "strutils.h"
#include "ttymsg.h"
#include "pathnames.h"
#include "carefulputc.h"
#include "c.h"
#include "cctype.h"
#include "fileutils.h"
#include "closestream.h"
#include "timeutils.h"
#include "pwdutils.h"

#define	TERM_WIDTH	79
#define	WRITE_TIME_OUT	300		/* in seconds */

/* Function prototypes */
static char *makemsg(char *fname, char **mvec, int mvecsz,
		    size_t *mbufsize, int print_banner);

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] [<file> | <message>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Write a message to all users.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -g, --group <group>     only send message to group\n"), out);
	fputs(_(" -n, --nobanner          do not print banner, works only for root\n"), out);
	fputs(_(" -t, --timeout <timeout> write timeout in seconds\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(25));
	fprintf(out, USAGE_MAN_TAIL("wall(1)"));

	exit(EXIT_SUCCESS);
}

struct group_workspace {
	gid_t	requested_group;
	int	ngroups;

/* getgrouplist() on OSX takes int* not gid_t* */
#ifdef __APPLE__
	int	*groups;
#else
	gid_t	*groups;
#endif
};

static gid_t get_group_gid(const char *group)
{
	struct group *gr;
	gid_t gid;

	if ((gr = getgrnam(group)))
		return gr->gr_gid;

	gid = strtou32_or_err(group, _("invalid group argument"));
	if (!getgrgid(gid))
		errx(EXIT_FAILURE, _("%s: unknown gid"), group);

	return gid;
}

static struct group_workspace *init_group_workspace(const char *group)
{
	struct group_workspace *buf;
	long n;

	n = sysconf(_SC_NGROUPS_MAX);
	if (n < 0 || n > INT_MAX - 1)
		return NULL;

	buf = xmalloc(sizeof(struct group_workspace));
	buf->ngroups = n + 1;	/* room for the primary gid */

	buf->requested_group = get_group_gid(group);
	buf->groups = xcalloc(buf->ngroups, sizeof(*buf->groups));

	return buf;
}

static void free_group_workspace(struct group_workspace *buf)
{
	if (!buf)
		return;

	free(buf->groups);
	free(buf);
}

static int is_gr_member(const char *login, const struct group_workspace *buf)
{
	struct passwd *pw;
	int ngroups = buf->ngroups;
	int rc;

	pw = getpwnam(login);
	if (!pw)
		return 0;

	if (buf->requested_group == pw->pw_gid)
		return 1;

	rc = getgrouplist(login, pw->pw_gid, buf->groups, &ngroups);
	if (rc < 0) {
		/* buffer too small, not sure how this can happen, since
		   we used sysconf to get the size... */
		errx(EXIT_FAILURE,
			_("getgrouplist found more groups than sysconf allows"));
	}

	for (; ngroups >= 0; --ngroups) {
		if (buf->requested_group == (gid_t) buf->groups[ngroups])
			return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ch;
	struct iovec iov;
	struct utmpx *utmpptr;
	char *p;
	char line[sizeof(utmpptr->ut_line) + 1];
	int print_banner = TRUE;
	struct group_workspace *group_buf = NULL;
	char *mbuf, *fname = NULL;
	size_t mbufsize;
	unsigned timeout = WRITE_TIME_OUT;
	char **mvec = NULL;
	int mvecsz = 0;

	static const struct option longopts[] = {
		{ "nobanner",	no_argument,		NULL, 'n' },
		{ "timeout",	required_argument,	NULL, 't' },
		{ "group",	required_argument,	NULL, 'g' },
		{ "version",	no_argument,		NULL, 'V' },
		{ "help",	no_argument,		NULL, 'h' },
		{ NULL,	0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((ch = getopt_long(argc, argv, "nt:g:Vh", longopts, NULL)) != -1) {
		switch (ch) {
		case 'n':
			if (geteuid() == 0)
				print_banner = FALSE;
			else
				warnx(_("--nobanner is available only for root"));
			break;
		case 't':
			timeout = strtou32_or_err(optarg, _("invalid timeout argument"));
			if (timeout < 1)
				errx(EXIT_FAILURE, _("invalid timeout argument: %s"), optarg);
			break;
		case 'g':
			group_buf = init_group_workspace(optarg);
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

	if (argc == 1 && access(argv[0], F_OK) == 0)
		fname = argv[0];
	else if (argc >= 1) {
		mvec = argv;
		mvecsz = argc;
	}

	mbuf = makemsg(fname, mvec, mvecsz, &mbufsize, print_banner);

	iov.iov_base = mbuf;
	iov.iov_len = mbufsize;

#if defined(USE_SYSTEMD) && HAVE_DECL_SD_SESSION_GET_USERNAME == 1
	if (sd_booted() > 0) {
		char **sessions_list;
		int sessions;

		sessions = sd_get_sessions(&sessions_list);
		if (sessions < 0)
			errx(EXIT_FAILURE, _("error getting sessions: %s"),
				strerror(-sessions));

		for (int i = 0; i < sessions; i++) {
			char *name, *tty;
			int r;

			if ((r = sd_session_get_username(sessions_list[i], &name)) < 0)
				errx(EXIT_FAILURE, _("get user name failed: %s"), strerror (-r));

			if (!(group_buf && !is_gr_member(name, group_buf))) {
				if (sd_session_get_tty(sessions_list[i], &tty) >= 0) {
					if ((p = ttymsg(&iov, 1, tty, timeout)) != NULL)
						warnx("%s", p);

					free(tty);
				}
			}
			free(name);
			free(sessions_list[i]);
		}
		free(sessions_list);
	} else
#endif
	{
		while ((utmpptr = getutxent())) {
			if (!utmpptr->ut_user[0])
				continue;
#ifdef USER_PROCESS
			if (utmpptr->ut_type != USER_PROCESS)
				continue;
#endif
			/* Joey Hess reports that use-sessreg in /etc/X11/wdm/ produces
			 * ut_line entries like :0, and a write to /dev/:0 fails.
			 *
			 * It also seems that some login manager may produce empty ut_line.
			 */
			if (!*utmpptr->ut_line || *utmpptr->ut_line == ':')
				continue;

			if (group_buf && !is_gr_member(utmpptr->ut_user, group_buf))
				continue;

			mem2strcpy(line, utmpptr->ut_line, sizeof(utmpptr->ut_line), sizeof(line));
			if ((p = ttymsg(&iov, 1, line, timeout)) != NULL)
				warnx("%s", p);
		}
		endutxent();
	}

	free(mbuf);
	free_group_workspace(group_buf);
	exit(EXIT_SUCCESS);
}

static char *makemsg(char *fname, char **mvec, int mvecsz,
		     size_t *mbufsize, int print_banner)
{
	char *lbuf, *retbuf;
	FILE * fs = open_memstream(&retbuf, mbufsize);
	size_t lbuflen = 512;
	lbuf = xmalloc(lbuflen);

	if (print_banner == TRUE) {
		char *hostname = xgethostname();
		char *whombuf, *whom, *where, date[CTIME_BUFSIZ];
		time_t now;

		whombuf = whom = xgetlogin();
		if (!whom) {
			whom = "<someone>";
			warn(_("cannot get passwd uid"));
		}
		where = ttyname(STDOUT_FILENO);
		if (!where) {
			where = "somewhere";
		} else if (strncmp(where, "/dev/", 5) == 0)
			where += 5;

		time(&now);
		ctime_r(&now, date);
		date[strlen(date) - 1] = '\0';

		/*
		 * all this stuff is to blank out a square for the message;
		 * we wrap message lines at column 79, not 80, because some
		 * terminals wrap after 79, some do not, and we can't tell.
		 * Which means that we may leave a non-blank character
		 * in column 80, but that can't be helped.
		 */
		/* snprintf is not always available, but the sprintf's here
		   will not overflow as long as %d takes at most 100 chars */
		fprintf(fs, "\r%*s\r\n", TERM_WIDTH, " ");

		snprintf(lbuf, lbuflen,
				_("Broadcast message from %s@%s (%s) (%s):"),
				whom, hostname, where, date);
		fprintf(fs, "%-*.*s\007\007\r\n", TERM_WIDTH, TERM_WIDTH, lbuf);
		free(hostname);
		free(whombuf);
	}
	fprintf(fs, "%*s\r\n", TERM_WIDTH, " ");

	 if (mvec) {
		/*
		 * Read message from argv[]
		 */
		int i;

		for (i = 0; i < mvecsz; i++) {
			fputs_careful(mvec[i], fs, '^', true, TERM_WIDTH);
			if (i < mvecsz - 1)
				fputc(' ', fs);
		}
		fputs("\r\n", fs);
	} else {
		/*
		 * read message from <file>
		 */
		if (fname) {
			/*
			 * When we are not root, but suid or sgid, refuse to read files
			 * (e.g. device files) that the user may not have access to.
			 * After all, our invoker can easily do "wall < file"
			 * instead of "wall file".
			 */
			uid_t uid = getuid();
			if (uid && (uid != geteuid() || getgid() != getegid()))
				errx(EXIT_FAILURE, _("will not read %s - use stdin."),
				     fname);

			if (!freopen(fname, "r", stdin))
				err(EXIT_FAILURE, _("cannot open %s"), fname);

		}

		/*
		 * Read message from stdin.
		 */
		while (getline(&lbuf, &lbuflen, stdin) >= 0)
			fputs_careful(lbuf, fs, '^', true, TERM_WIDTH);
	}
	fprintf(fs, "%*s\r\n", TERM_WIDTH, " ");

	free(lbuf);

	fclose(fs);
	return retbuf;
}
