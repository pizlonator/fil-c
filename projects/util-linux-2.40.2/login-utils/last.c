/*
 * last(1) from sysvinit project, merged into util-linux in Aug 2013.
 *
 * Copyright (C) 1991-2004 Miquel van Smoorenburg.
 * Copyright (C) 2013      Ondrej Oprala <ooprala@redhat.com>
 *                         Karel Zak <kzak@redhat.com>
 *
 * Re-implementation of the 'last' command, this time for Linux. Yes I know
 * there is BSD last, but I just felt like writing this. No thanks :-).  Also,
 * this version gives lots more info (especially with -x)
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <utmpx.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <libgen.h>

#include "c.h"
#include "nls.h"
#include "optutils.h"
#include "pathnames.h"
#include "xalloc.h"
#include "closestream.h"
#include "carefulputc.h"
#include "strutils.h"
#include "timeutils.h"
#include "monotonic.h"
#include "fileutils.h"

#ifdef FUZZ_TARGET
#include "fuzz.h"
#endif

#ifndef SHUTDOWN_TIME
# define SHUTDOWN_TIME 254
#endif

#ifndef LAST_LOGIN_LEN
# define LAST_LOGIN_LEN 8
#endif

#ifndef LAST_DOMAIN_LEN
# define LAST_DOMAIN_LEN 16
#endif

#ifndef LAST_TIMESTAMP_LEN
# define LAST_TIMESTAMP_LEN 32
#endif

#define UCHUNKSIZE	16384	/* How much we read at once. */

struct last_control {
	unsigned int lastb :1,	  /* Is this command 'lastb' */
		     extended :1, /* Lots of info */
		     showhost :1, /* Show hostname */
		     altlist :1,  /* Hostname at the end */
		     usedns :1,	  /* Use DNS to lookup the hostname */
		     useip :1;    /* Print IP address in number format */

	unsigned int name_len;	/* Number of login name characters to print */
	unsigned int domain_len; /* Number of domain name characters to print */
	unsigned int maxrecs;	/* Maximum number of records to list */

	char **show;		/* Match search list */

	struct timeval boot_time; /* system boot time */
	time_t since;		/* at what time to start displaying the file */
	time_t until;		/* at what time to stop displaying the file */
	time_t present;		/* who where present at time_t */
	unsigned int time_fmt;	/* time format */
	char separator;        /* output separator */
};

/* Double linked list of struct utmp's */
struct utmplist {
	struct utmpx ut;
	struct utmplist *next;
	struct utmplist *prev;
};

/* Types of listing */
enum {
	R_CRASH = 1,	/* No logout record, system boot in between */
	R_DOWN,		/* System brought down in decent way */
	R_NORMAL,	/* Normal */
	R_NOW,		/* Still logged in */
	R_REBOOT,	/* Reboot record. */
	R_REBOOT_CRASH,	/* Reboot record without matching shutdown */
	R_PHANTOM,	/* No logout record but session is stale. */
	R_TIMECHANGE	/* NEW_TIME or OLD_TIME */
};

enum {
	LAST_TIMEFTM_NONE = 0,
	LAST_TIMEFTM_SHORT,
	LAST_TIMEFTM_CTIME,
	LAST_TIMEFTM_ISO8601,

	LAST_TIMEFTM_HHMM,	/* non-public */
};

struct last_timefmt {
	const char *name;
	int in_len;	/* log-in */
	int in_fmt;
	int out_len;	/* log-out */
	int out_fmt;
};

static struct last_timefmt timefmts[] = {
	[LAST_TIMEFTM_NONE] = { .name = "notime" },
	[LAST_TIMEFTM_SHORT] = {
		.name    = "short",
		.in_len  = 16,
		.out_len = 7,
		.in_fmt  = LAST_TIMEFTM_CTIME,
		.out_fmt = LAST_TIMEFTM_HHMM
	},
	[LAST_TIMEFTM_CTIME] = {
		.name    = "full",
		.in_len  = 24,
		.out_len = 26,
		.in_fmt  = LAST_TIMEFTM_CTIME,
		.out_fmt = LAST_TIMEFTM_CTIME
	},
	[LAST_TIMEFTM_ISO8601] = {
		.name    = "iso",
		.in_len  = 25,
		.out_len = 27,
		.in_fmt  = LAST_TIMEFTM_ISO8601,
		.out_fmt = LAST_TIMEFTM_ISO8601
	}
};

/* Global variables */
static unsigned int recsdone;	/* Number of records listed */
static time_t lastdate;		/* Last date we've seen */
static time_t currentdate;	/* date when we started processing the file */

#ifndef FUZZ_TARGET
/* --time-format=option parser */
static int which_time_format(const char *s)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(timefmts); i++) {
		if (strcmp(timefmts[i].name, s) == 0)
			return i;
	}
	errx(EXIT_FAILURE, _("unknown time format: %s"), s);
}
#endif

/*
 *	Read one utmp entry, return in new format.
 *	Automatically reposition file pointer.
 */
static int uread(FILE *fp, struct utmpx *u,  int *quit, const char *filename)
{
	static int utsize;
	static char buf[UCHUNKSIZE];
	char tmp[1024];
	static off_t fpos;
	static int bpos;
	off_t o;

	if (quit == NULL && u != NULL) {
		/*
		 *	Normal read.
		 */
		return fread(u, sizeof(struct utmpx), 1, fp);
	}

	if (u == NULL) {
		/*
		 *	Initialize and position.
		 */
		utsize = sizeof(struct utmpx);
		fseeko(fp, 0, SEEK_END);
		fpos = ftello(fp);
		if (fpos == 0)
			return 0;
		o = ((fpos - 1) / UCHUNKSIZE) * UCHUNKSIZE;
		if (fseeko(fp, o, SEEK_SET) < 0) {
			warn(_("seek on %s failed"), filename);
			return 0;
		}
		bpos = (int)(fpos - o);
		if (fread(buf, bpos, 1, fp) != 1) {
			warn(_("cannot read %s"), filename);
			return 0;
		}
		fpos = o;
		return 1;
	}

	/*
	 *	Read one struct. From the buffer if possible.
	 */
	bpos -= utsize;
	if (bpos >= 0) {
		memcpy(u, buf + bpos, sizeof(struct utmpx));
		return 1;
	}

	/*
	 *	Oops we went "below" the buffer. We should be able to
	 *	seek back UCHUNKSIZE bytes.
	 */
	fpos -= UCHUNKSIZE;
	if (fpos < 0)
		return 0;

	/*
	 *	Copy whatever is left in the buffer.
	 */
	memcpy(tmp + (-bpos), buf, utsize + bpos);
	if (fseeko(fp, fpos, SEEK_SET) < 0) {
		warn(_("seek on %s failed"), filename);
		return 0;
	}

	/*
	 *	Read another UCHUNKSIZE bytes.
	 */
	if (fread(buf, UCHUNKSIZE, 1, fp) != 1) {
		warn(_("cannot read %s"), filename);
		return 0;
	}

	/*
	 *	The end of the UCHUNKSIZE byte buffer should be the first
	 *	few bytes of the current struct utmp.
	 */
	memcpy(tmp, buf + UCHUNKSIZE + bpos, -bpos);
	bpos += UCHUNKSIZE;

	memcpy(u, tmp, sizeof(struct utmpx));

	return 1;
}

#ifndef FUZZ_TARGET
/*
 *	SIGINT handler
 */
static void int_handler(int sig __attribute__((unused)))
{
	ul_sig_err(EXIT_FAILURE, "Interrupted");
}

/*
 *	SIGQUIT handler
 */
static void quit_handler(int sig __attribute__((unused)))
{
	ul_sig_warn("Interrupted");
	signal(SIGQUIT, quit_handler);
}
#endif

/*
 *	Lookup a host with DNS.
 */
static int dns_lookup(char *result, int size, int useip, int32_t *a)
{
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
	struct sockaddr		*sa;
	int			salen, flags;
	int			mapped = 0;

	flags = useip ? NI_NUMERICHOST : 0;

	/*
	 *	IPv4 or IPv6 ?
	 *	1. If last 3 4bytes are 0, must be IPv4
	 *	2. If IPv6 in IPv4, handle as IPv4
	 *	3. Anything else is IPv6
	 *
	 *	Ugly.
	 */
	if (a[0] == 0 && a[1] == 0 && a[2] == (int32_t)htonl (0xffff))
		mapped = 1;

	if (mapped || (a[1] == 0 && a[2] == 0 && a[3] == 0)) {
		/* IPv4 */
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		sin.sin_addr.s_addr = mapped ? a[3] : a[0];
		sa = (struct sockaddr *)&sin;
		salen = sizeof(sin);
	} else {
		/* IPv6 */
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = 0;
		memcpy(sin6.sin6_addr.s6_addr, a, 16);
		sa = (struct sockaddr *)&sin6;
		salen = sizeof(sin6);
	}

	return getnameinfo(sa, salen, result, size, NULL, 0, flags);
}

static int time_formatter(int fmt, char *dst, size_t dlen, time_t *when)
{
	int ret = 0;

	switch (fmt) {
	case LAST_TIMEFTM_NONE:
		*dst = 0;
		break;
	case LAST_TIMEFTM_HHMM:
	{
		struct tm tm;

		localtime_r(when, &tm);
		if (!snprintf(dst, dlen, "%02d:%02d", tm.tm_hour, tm.tm_min))
			ret = -1;
		break;
	}
	case LAST_TIMEFTM_CTIME:
	{
		char buf[CTIME_BUFSIZ];

		if (!ctime_r(when, buf)) {
			ret = -1;
			break;
		}
		snprintf(dst, dlen, "%s", buf);
		ret = rtrim_whitespace((unsigned char *) dst);
		break;
	}
	case LAST_TIMEFTM_ISO8601:
		ret = strtime_iso(when, ISO_TIMESTAMP_T, dst, dlen);
		break;
	default:
		abort();
	}
	return ret;
}

/*
 *	Remove trailing spaces from a string.
 */
static void trim_trailing_spaces(char *s)
{
	char *p;

	for (p = s; *p; ++p)
		continue;
	while (p > s && isspace(*--p))
		continue;
	if (p > s)
		++p;
	*p++ = '\n';
	*p = '\0';
}

/*
 *	Show one line of information on screen
 */
static int list(const struct last_control *ctl, struct utmpx *p, time_t logout_time, int what)
{
	time_t		secs, utmp_time;
	char		logintime[LAST_TIMESTAMP_LEN];
	char		logouttime[LAST_TIMESTAMP_LEN];
	char		length[LAST_TIMESTAMP_LEN];
	char		final[512];
	char		utline[sizeof(p->ut_line) + 1];
	char		domain[256];
	int		mins, hours, days;
	int		r, len;
	struct last_timefmt *fmt;

	/*
	 *	uucp and ftp have special-type entries
	 */
	mem2strcpy(utline, p->ut_line, sizeof(p->ut_line), sizeof(utline));
	if (strncmp(utline, "ftp", 3) == 0 && isdigit(utline[3]))
		utline[3] = 0;
	if (strncmp(utline, "uucp", 4) == 0 && isdigit(utline[4]))
		utline[4] = 0;

	/*
	 *	Is this something we want to show?
	 */
	if (ctl->show) {
		char **walk;
		for (walk = ctl->show; *walk; walk++) {
			if (strncmp(p->ut_user, *walk, sizeof(p->ut_user)) == 0 ||
			    strcmp(utline, *walk) == 0 ||
			    (strncmp(utline, "tty", 3) == 0 &&
			     strcmp(utline + 3, *walk) == 0)) break;
		}
		if (*walk == NULL) return 0;
	}

	/*
	 *	Calculate times
	 */
	fmt = &timefmts[ctl->time_fmt];

	utmp_time = p->ut_tv.tv_sec;

	if (ctl->present) {
		if (ctl->present < utmp_time)
			return 0;
		if (0 < logout_time && logout_time < ctl->present)
			return 0;
	}

	/* log-in time */
	if (time_formatter(fmt->in_fmt, logintime,
			   sizeof(logintime), &utmp_time) < 0)
		errx(EXIT_FAILURE, _("preallocation size exceeded"));

	/* log-out time */
	secs  = logout_time - utmp_time; /* Under strange circumstances, secs < 0 can happen */
	mins  = (secs / 60) % 60;
	hours = (secs / 3600) % 24;
	days  = secs / 86400;

	strcpy(logouttime, "- ");
	if (time_formatter(fmt->out_fmt, logouttime + 2,
			   sizeof(logouttime) - 2, &logout_time) < 0)
		errx(EXIT_FAILURE, _("preallocation size exceeded"));

	if (logout_time == currentdate) {
		if (ctl->time_fmt > LAST_TIMEFTM_SHORT) {
			snprintf(logouttime, sizeof(logouttime), "  still running");
			length[0] = 0;
		} else {
			snprintf(logouttime, sizeof(logouttime), "  still");
			snprintf(length, sizeof(length), "running");
		}
	} else if (days) {
		snprintf(length, sizeof(length), "(%d+%02d:%02d)", days, abs(hours), abs(mins)); /* hours and mins always shown as positive (w/o minus sign!) even if secs < 0 */
	} else if (hours) {
		snprintf(length, sizeof(length), " (%02d:%02d)", hours, abs(mins));  /* mins always shown as positive (w/o minus sign!) even if secs < 0 */
	} else if (secs >= 0) {
		snprintf(length, sizeof(length), " (%02d:%02d)", hours, mins);
	} else {
		snprintf(length, sizeof(length), " (-00:%02d)", abs(mins));  /* mins always shown as positive (w/o minus sign!) even if secs < 0 */
	}

	switch(what) {
		case R_CRASH:
		case R_REBOOT_CRASH:
			snprintf(logouttime, sizeof(logouttime), "- crash");
			break;
		case R_DOWN:
			snprintf(logouttime, sizeof(logouttime), "- down ");
			break;
		case R_NOW:
			if (ctl->time_fmt > LAST_TIMEFTM_SHORT) {
				snprintf(logouttime, sizeof(logouttime), "  still logged in");
				length[0] = 0;
			} else {
				snprintf(logouttime, sizeof(logouttime), "  still");
				snprintf(length, sizeof(length), "logged in");
			}
			break;
		case R_PHANTOM:
			if (ctl->time_fmt > LAST_TIMEFTM_SHORT) {
				snprintf(logouttime, sizeof(logouttime), "  gone - no logout");
				length[0] = 0;
			} else if (ctl->time_fmt == LAST_TIMEFTM_SHORT) {
				snprintf(logouttime, sizeof(logouttime), "   gone");
				snprintf(length, sizeof(length), "- no logout");
			} else {
				logouttime[0] = 0;
				snprintf(length, sizeof(length), "no logout");
			}
			break;
		case R_TIMECHANGE:
			logouttime[0] = 0;
			length[0] = 0;
			break;
		case R_NORMAL:
		case R_REBOOT:
			break;
		default:
			abort();
	}

	/*
	 *	Look up host with DNS if needed.
	 */
	r = -1;
	if (ctl->usedns || ctl->useip)
		r = dns_lookup(domain, sizeof(domain), ctl->useip, (int32_t*)p->ut_addr_v6);
	if (r < 0)
		mem2strcpy(domain, p->ut_host, sizeof(p->ut_host), sizeof(domain));

	if (ctl->showhost) {
		if (!ctl->altlist) {
			len = snprintf(final, sizeof(final),
				"%-8.*s%c%-12.12s%c%-16.*s%c%-*.*s%c%-*.*s%c%s\n",
				ctl->name_len, p->ut_user, ctl->separator, utline, ctl->separator,
				ctl->domain_len, domain, ctl->separator,
				fmt->in_len, fmt->in_len, logintime, ctl->separator, fmt->out_len, fmt->out_len,
				logouttime, ctl->separator, length);
		} else {
			len = snprintf(final, sizeof(final),
				"%-8.*s%c%-12.12s%c%-*.*s%c%-*.*s%c%-12.12s%c%s\n",
				ctl->name_len, p->ut_user, ctl->separator, utline, ctl->separator,
				fmt->in_len, fmt->in_len, logintime, ctl->separator, fmt->out_len, fmt->out_len,
				logouttime, ctl->separator, length, ctl->separator, domain);
		}
	} else
		len = snprintf(final, sizeof(final),
			"%-8.*s%c%-12.12s%c%-*.*s%c%-*.*s%c%s\n",
			ctl->name_len, p->ut_user, ctl->separator, utline, ctl->separator,
			fmt->in_len, fmt->in_len, logintime, ctl->separator, fmt->out_len, fmt->out_len,
			logouttime, ctl->separator, length);

#if defined(__GLIBC__)
#  if (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0)
	final[sizeof(final)-1] = '\0';
#  endif
#endif

	trim_trailing_spaces(final);
	/*
	 *	Print out "final" string safely.
	 */
	fputs_careful(final, stdout, '*', false, 0);

	if (len < 0 || (size_t)len >= sizeof(final))
		putchar('\n');

	recsdone++;
	if (ctl->maxrecs && ctl->maxrecs <= recsdone)
		return 1;

	return 0;
}

#ifndef FUZZ_TARGET
static void __attribute__((__noreturn__)) usage(const struct last_control *ctl)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %s [options] [<username>...] [<tty>...]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show a listing of last logged in users.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -<number>            how many lines to show\n"), out);
	fputs(_(" -a, --hostlast       display hostnames in the last column\n"), out);
	fputs(_(" -d, --dns            translate the IP number back into a hostname\n"), out);
	fprintf(out,
	      _(" -f, --file <file>    use a specific file instead of %s\n"), ctl->lastb ? _PATH_BTMP : _PATH_WTMP);
	fputs(_(" -F, --fulltimes      print full login and logout times and dates\n"), out);
	fputs(_(" -i, --ip             display IP numbers in numbers-and-dots notation\n"), out);
	fputs(_(" -n, --limit <number> how many lines to show\n"), out);
	fputs(_(" -R, --nohostname     don't display the hostname field\n"), out);
	fputs(_(" -s, --since <time>   display the lines since the specified time\n"), out);
	fputs(_(" -t, --until <time>   display the lines until the specified time\n"), out);
	fputs(_(" -T, --tab-separated	use tabs as delimiters\n"), out);
	fputs(_(" -p, --present <time> display who were present at the specified time\n"), out);
	fputs(_(" -w, --fullnames      display full user and domain names\n"), out);
	fputs(_(" -x, --system         display system shutdown entries and run level changes\n"), out);
	fputs(_("     --time-format <format>  show timestamps in the specified <format>:\n"
		"                               notime|short|full|iso\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(22));
	fprintf(out, USAGE_MAN_TAIL("last(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}
#endif

static int is_phantom(const struct last_control *ctl, struct utmpx *ut)
{
	struct passwd *pw;
	char path[sizeof(ut->ut_line) + 16];
	char user[sizeof(ut->ut_user) + 1];
	int ret = 0;

	if (ut->ut_tv.tv_sec < ctl->boot_time.tv_sec)
		return 1;

	mem2strcpy(user, ut->ut_user, sizeof(ut->ut_user), sizeof(user));
	pw = getpwnam(user);
	if (!pw)
		return 1;
	snprintf(path, sizeof(path), "/proc/%u/loginuid", ut->ut_pid);
	if (access(path, R_OK) == 0) {
		unsigned int loginuid;
		FILE *f = NULL;

		if (!(f = fopen(path, "r")))
			return 1;
		if (fscanf(f, "%u", &loginuid) != 1)
			ret = 1;
		fclose(f);
		if (!ret && pw->pw_uid != loginuid)
			return 1;
	} else {
		struct stat st;
		char utline[sizeof(ut->ut_line) + 1];

		mem2strcpy(utline, ut->ut_line, sizeof(ut->ut_line), sizeof(utline));

		snprintf(path, sizeof(path), "/dev/%s", utline);
		if (stat(path, &st))
			return 1;
		if (pw->pw_uid != st.st_uid)
			return 1;
	}
	return ret;
}

static void process_wtmp_file(const struct last_control *ctl,
			      const char *filename)
{
	FILE *fp;		/* File pointer of wtmp file */

	struct utmpx ut;	/* Current utmp entry */
	struct utmplist *ulist = NULL;	/* All entries */
	struct utmplist *p;	/* Pointer into utmplist */
	struct utmplist *next;	/* Pointer into utmplist */

	time_t lastboot = 0;	/* Last boottime */
	time_t lastrch = 0;	/* Last run level change */
	time_t lastdown;	/* Last downtime */
	time_t begintime;	/* When wtmp begins */
	int whydown = 0;	/* Why we went down: crash or shutdown */

	int c, x;		/* Scratch */
	struct stat st;		/* To stat the [uw]tmp file */
	int quit = 0;		/* Flag */
	int down = 0;		/* Down flag */

#ifndef FUZZ_TARGET
	time(&lastdown);
#else
	lastdown = 1596001948;
#endif
	/*
	 * Fill in 'lastdate'
	 */
	lastdate = currentdate = lastrch = lastdown;

#ifndef FUZZ_TARGET
	/*
	 * Install signal handlers
	 */
	signal(SIGINT, int_handler);
	signal(SIGQUIT, quit_handler);
#endif

	/*
	 * Open the utmp file
	 */
	if ((fp = fopen(filename, "r")) == NULL)
		err(EXIT_FAILURE, _("cannot open %s"), filename);

	/*
	 * Optimize the buffer size.
	 */
	setvbuf(fp, NULL, _IOFBF, UCHUNKSIZE);

	/*
	 * Read first structure to capture the time field
	 */
	if (uread(fp, &ut, NULL, filename) == 1)
		begintime = ut.ut_tv.tv_sec;
	else {
		if (fstat(fileno(fp), &st) != 0)
			err(EXIT_FAILURE, _("stat of %s failed"), filename);
		begintime = st.st_ctime;
		quit = 1;
	}

	/*
	 * Go to end of file minus one structure
	 * and/or initialize utmp reading code.
	 */
	uread(fp, NULL, NULL, filename);

	/*
	 * Read struct after struct backwards from the file.
	 */
	while (!quit) {

		if (uread(fp, &ut, &quit, filename) != 1)
			break;

		if (ctl->since && ut.ut_tv.tv_sec < ctl->since)
			continue;

		if (ctl->until && ctl->until < ut.ut_tv.tv_sec)
			continue;

		lastdate = ut.ut_tv.tv_sec;

		if (ctl->lastb) {
			quit = list(ctl, &ut, ut.ut_tv.tv_sec, R_NORMAL);
			continue;
		}

		/*
		 * Set ut_type to the correct type.
		 */
		if (strncmp(ut.ut_line, "~", 1) == 0) {
			if (strncmp(ut.ut_user, "shutdown", 8) == 0)
				ut.ut_type = SHUTDOWN_TIME;
			else if (strncmp(ut.ut_user, "reboot", 6) == 0)
				ut.ut_type = BOOT_TIME;
			else if (strncmp(ut.ut_user, "runlevel", 8) == 0)
				ut.ut_type = RUN_LVL;
		}
#if 1 /*def COMPAT*/
		/*
		 * For stupid old applications that don't fill in
		 * ut_type correctly.
		 */
		else {
			if (ut.ut_type != DEAD_PROCESS &&
			    ut.ut_user[0] && ut.ut_line[0] &&
			    strncmp(ut.ut_user, "LOGIN", 5) != 0)
				ut.ut_type = USER_PROCESS;
			/*
			 * Even worse, applications that write ghost
			 * entries: ut_type set to USER_PROCESS but
			 * empty ut_user...
			 */
			if (ut.ut_user[0] == 0)
				ut.ut_type = DEAD_PROCESS;

			/*
			 * Clock changes.
			 */
			if (strncmp(ut.ut_user, "date", 4) == 0) {
				if (ut.ut_line[0] == '|')
					ut.ut_type = OLD_TIME;
				if (ut.ut_line[0] == '{')
					ut.ut_type = NEW_TIME;
			}
		}
#endif
		switch (ut.ut_type) {
		case SHUTDOWN_TIME:
			if (ctl->extended) {
				strcpy(ut.ut_line, "system down");
				quit = list(ctl, &ut, lastboot, R_NORMAL);
			}
			lastdown = lastrch = ut.ut_tv.tv_sec;
			down = 1;
			break;
		case OLD_TIME:
		case NEW_TIME:
			if (ctl->extended) {
				strcpy(ut.ut_line,
				ut.ut_type == NEW_TIME ? "new time" :
					"old time");
				quit = list(ctl, &ut, lastdown, R_TIMECHANGE);
			}
			break;
		case BOOT_TIME:
			strcpy(ut.ut_line, "system boot");
			if (lastdown > lastboot && lastdown != currentdate)
				quit = list(ctl, &ut, lastboot, R_REBOOT_CRASH);
			else
				quit = list(ctl, &ut, lastdown, R_REBOOT);
			lastboot = ut.ut_tv.tv_sec;
			down = 1;
			break;
		case RUN_LVL:
			x = ut.ut_pid & 255;
			if (ctl->extended) {
				snprintf(ut.ut_line, sizeof(ut.ut_line), "(to lvl %c)", x);
				quit = list(ctl, &ut, lastrch, R_NORMAL);
			}
			if (x == '0' || x == '6') {
				lastdown = ut.ut_tv.tv_sec;
				down = 1;
				ut.ut_type = SHUTDOWN_TIME;
			}
			lastrch = ut.ut_tv.tv_sec;
			break;

		case USER_PROCESS:
			/*
			 * This was a login - show the first matching
			 * logout record and delete all records with
			 * the same ut_line.
			 */
			c = 0;
			for (p = ulist; p; p = next) {
				next = p->next;
				if (strncmp(p->ut.ut_line, ut.ut_line,
				    sizeof(ut.ut_line)) == 0) {
					/* Show it */
					if (c == 0) {
						quit = list(ctl, &ut, p->ut.ut_tv.tv_sec, R_NORMAL);
						c = 1;
					}
					if (p->next)
						p->next->prev = p->prev;
					if (p->prev)
						p->prev->next = p->next;
					else
						ulist = p->next;
					free(p);
				}
			}
			/*
			 * Not found? Then crashed, down, still
			 * logged in, or missing logout record.
			 */
			if (c == 0) {
				if (!lastboot) {
					c = R_NOW;
					/* Is process still alive? */
					if (is_phantom(ctl, &ut))
						c = R_PHANTOM;
				} else
					c = whydown;
				quit = list(ctl, &ut, lastboot, c);
			}
			/* fallthrough */

		case DEAD_PROCESS:
			/*
			 * Just store the data if it is
			 * interesting enough.
			 */
			if (ut.ut_line[0] == 0)
				break;
			p = xmalloc(sizeof(struct utmplist));
			memcpy(&p->ut, &ut, sizeof(struct utmpx));
			p->next  = ulist;
			p->prev  = NULL;
			if (ulist)
				ulist->prev = p;
			ulist = p;
			break;

		case EMPTY:
		case INIT_PROCESS:
		case LOGIN_PROCESS:
#ifdef ACCOUNTING
		case ACCOUNTING:
#endif
			/* ignored ut_types */
			break;

		default:
			warnx("unrecognized ut_type: %d", ut.ut_type);
		}

		/*
		 * If we saw a shutdown/reboot record we can remove
		 * the entire current ulist.
		 */
		if (down) {
			lastboot = ut.ut_tv.tv_sec;
			whydown = (ut.ut_type == SHUTDOWN_TIME) ? R_DOWN : R_CRASH;
			for (p = ulist; p; p = next) {
				next = p->next;
				free(p);
			}
			ulist = NULL;
			down = 0;
		}
	}

	if (ctl->time_fmt != LAST_TIMEFTM_NONE) {
		struct last_timefmt *fmt;
		char timestr[LAST_TIMESTAMP_LEN];
		char *tmp = xstrdup(filename);

		fmt = &timefmts[ctl->time_fmt];
		if (time_formatter(fmt->in_fmt, timestr,
				   sizeof(timestr), &begintime) < 0)
			errx(EXIT_FAILURE, _("preallocation size exceeded"));
		printf(_("\n%s begins %s\n"), basename(tmp), timestr);
		free(tmp);
	}

	fclose(fp);

	for (p = ulist; p; p = next) {
		next = p->next;
		free(p);
	}
}

#ifdef FUZZ_TARGET
# include "all-io.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	struct last_control ctl = {
		.showhost = TRUE,
		.name_len = LAST_LOGIN_LEN,
		.time_fmt = LAST_TIMEFTM_SHORT,
		.domain_len = LAST_DOMAIN_LEN,
		.boot_time = {
			.tv_sec = 1595978419,
			.tv_usec = 816074
		}
	};
	char name[] = "/tmp/test-last-fuzz.XXXXXX";
	int fd;

	fd = mkstemp_cloexec(name);
	if (fd < 0)
		err(EXIT_FAILURE, "mkstemp() failed");
	if (write_all(fd, data, size) != 0)
		err(EXIT_FAILURE, "write() failed");

	process_wtmp_file(&ctl, name);

	close(fd);
	unlink(name);

	return 0;
}
#else
int main(int argc, char **argv)
{
	struct last_control ctl = {
		.showhost = TRUE,
		.name_len = LAST_LOGIN_LEN,
		.time_fmt = LAST_TIMEFTM_SHORT,
		.domain_len = LAST_DOMAIN_LEN
	};
	char **files = NULL;
	size_t i, nfiles = 0;
	int c;
	usec_t p;

	enum {
		OPT_TIME_FORMAT = CHAR_MAX + 1
	};
	static const struct option long_opts[] = {
	      { "limit",	required_argument, NULL, 'n' },
	      { "help",	no_argument,       NULL, 'h' },
	      { "file",       required_argument, NULL, 'f' },
	      { "nohostname", no_argument,       NULL, 'R' },
	      { "version",    no_argument,       NULL, 'V' },
	      { "hostlast",   no_argument,       NULL, 'a' },
	      { "since",      required_argument, NULL, 's' },
	      { "until",      required_argument, NULL, 't' },
	      { "present",    required_argument, NULL, 'p' },
	      { "system",     no_argument,       NULL, 'x' },
	      { "dns",        no_argument,       NULL, 'd' },
	      { "ip",         no_argument,       NULL, 'i' },
	      { "fulltimes",  no_argument,       NULL, 'F' },
	      { "fullnames",  no_argument,       NULL, 'w' },
	      { "tab-separated",  no_argument,   NULL, 'T' },
	      { "time-format", required_argument, NULL, OPT_TIME_FORMAT },
	      { NULL, 0, NULL, 0 }
	};
	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'F', OPT_TIME_FORMAT },	/* fulltime, time-format */
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();
	/*
	 * Which file do we want to read?
	 */
	ctl.lastb = strcmp(program_invocation_short_name, "lastb") == 0 ? 1 : 0;
	ctl.separator = ' ';
	while ((c = getopt_long(argc, argv,
			 "hVf:n:RxadFit:p:s:T0123456789w", long_opts, NULL)) != -1) {

		err_exclusive_options(c, long_opts, excl, excl_st);

		switch(c) {
		case 'h':
			usage(&ctl);
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'R':
			ctl.showhost = 0;
			break;
		case 'x':
			ctl.extended = 1;
			break;
		case 'n':
			ctl.maxrecs = strtos32_or_err(optarg, _("failed to parse number"));
			break;
		case 'f':
			if (!files)
				files = xmalloc(sizeof(char *) * argc);
			files[nfiles++] = xstrdup(optarg);
			break;
		case 'd':
			ctl.usedns = 1;
			break;
		case 'i':
			ctl.useip = 1;
			break;
		case 'a':
			ctl.altlist = 1;
			break;
		case 'F':
			ctl.time_fmt = LAST_TIMEFTM_CTIME;
			break;
		case 'p':
			if (parse_timestamp(optarg, &p) < 0)
				errx(EXIT_FAILURE, _("invalid time value \"%s\""), optarg);
			ctl.present = (time_t) (p / 1000000);
			break;
		case 's':
			if (parse_timestamp(optarg, &p) < 0)
				errx(EXIT_FAILURE, _("invalid time value \"%s\""), optarg);
			ctl.since = (time_t) (p / 1000000);
			break;
		case 't':
			if (parse_timestamp(optarg, &p) < 0)
				errx(EXIT_FAILURE, _("invalid time value \"%s\""), optarg);
			ctl.until = (time_t) (p / 1000000);
			break;
		case 'w':
			if (ctl.name_len < sizeof_member(struct utmpx, ut_user))
				ctl.name_len = sizeof_member(struct utmpx, ut_user);
			if (ctl.domain_len < sizeof_member(struct utmpx, ut_host))
				ctl.domain_len = sizeof_member(struct utmpx, ut_host);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			ctl.maxrecs = 10 * ctl.maxrecs + c - '0';
			break;
		case OPT_TIME_FORMAT:
			ctl.time_fmt = which_time_format(optarg);
			break;
		case 'T':
			ctl.separator = '\t';
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind < argc)
		ctl.show = argv + optind;

	if (!files) {
		files = xmalloc(sizeof(char *));
		files[nfiles++] = xstrdup(ctl.lastb ? _PATH_BTMP : _PATH_WTMP);
	}

	for (i = 0; i < nfiles; i++) {
		get_boot_time(&ctl.boot_time);
		process_wtmp_file(&ctl, files[i]);
		free(files[i]);
	}
	free(files);
	return EXIT_SUCCESS;
}
#endif
