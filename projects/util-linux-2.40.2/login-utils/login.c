/*
 * login(1)
 *
 * This program is derived from 4.3 BSD software and is subject to the
 * copyright notice below.
 *
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 * Rewritten to PAM-only version.
 *
 * Michael Glad (glad@daimi.dk)
 * Computer Science Department, Aarhus University, Denmark
 * 1990-07-04
 *
 * Copyright (c) 1980, 1987, 1988 The Regents of the University of California.
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
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <memory.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <utmpx.h>
#include <path.h>

#ifdef HAVE_LASTLOG_H
# include <lastlog.h>
#endif

#include <stdlib.h>
#include <sys/syslog.h>

#ifdef HAVE_LINUX_MAJOR_H
# include <linux/major.h>
#endif

#include <netdb.h>
#include <security/pam_appl.h>

#ifdef HAVE_SECURITY_PAM_MISC_H
# include <security/pam_misc.h>
#elif defined(HAVE_SECURITY_OPENPAM_H)
# include <security/openpam.h>
#endif

#ifdef HAVE_LIBAUDIT
# include <libaudit.h>
#endif

#include "c.h"
#include "pathnames.h"
#include "strutils.h"
#include "nls.h"
#include "env.h"
#include "xalloc.h"
#include "all-io.h"
#include "fileutils.h"
#include "timeutils.h"
#include "ttyutils.h"
#include "pwdutils.h"

#include "logindefs.h"

#define LOGIN_MAX_TRIES        3
#define LOGIN_EXIT_TIMEOUT     5
#define LOGIN_TIMEOUT          60

static char **argv0;
static size_t argv_lth;

#define VCS_PATH_MAX	64

#if defined(HAVE_SCANDIRAT) && defined(HAVE_OPENAT)
# include <dirent.h>
# define MOTDDIR_SUPPORT
# define MOTDDIR_EXT	".motd"
# define MOTDDIR_EXTSIZ	(sizeof(MOTDDIR_EXT) - 1)
#endif

/*
 * Login control struct
 */
struct login_context {
	const char	*tty_path;	/* ttyname() return value */
	const char	*tty_name;	/* tty_path without /dev prefix */
	const char	*tty_number;	/* end of the tty_path */
	mode_t		tty_mode;	/* chmod() mode */

	const char	*username;	/* points to PAM, pwd or cmd_username */
	char            *cmd_username;	/* username specified on command line */

	struct passwd	*pwd;		/* user info */
	char		*pwdbuf;	/* pwd strings */

	pam_handle_t	*pamh;		/* PAM handler */
	struct pam_conv	conv;		/* PAM conversation */

#ifdef LOGIN_CHOWN_VCS
	char		vcsn[VCS_PATH_MAX];	/* virtual console name */
	char		vcsan[VCS_PATH_MAX];
#endif

	char		*thishost;		/* this machine */
	char		*thisdomain;		/* this machine's domain */
	char		*hostname;		/* remote machine */
	char		hostaddress[16];	/* remote address */

	pid_t		pid;

	unsigned int	quiet:1,        /* hush file exists */
			remote:1,	/* login -h */
			nohost:1,	/* login -H */
			noauth:1,	/* login -f */
			keep_env:1;	/* login -p */
};

static pid_t child_pid = 0;
static volatile sig_atomic_t got_sig = 0;
static char *timeout_msg;

#ifdef LOGIN_CHOWN_VCS
/* true if the filedescriptor fd is a console tty, very Linux specific */
static int is_consoletty(int fd)
{
	struct stat stb;

	if ((fstat(fd, &stb) >= 0)
	    && (major(stb.st_rdev) == TTY_MAJOR)
	    && (minor(stb.st_rdev) < 64)) {
		return 1;
	}
	return 0;
}
#endif

/*
 * Robert Ambrose writes:
 * A couple of my users have a problem with login processes hanging around
 * soaking up pts's.  What they seem to hung up on is trying to write out the
 * message 'Login timed out after %d seconds' when the connection has already
 * been dropped.
 * What I did was add a second timeout while trying to write the message, so
 * the process just exits if the second timeout expires.
 */
static void __attribute__((__noreturn__))
    timedout2(int sig __attribute__((__unused__)))
{
	struct termios ti;

	/* reset echo */
	if (tcgetattr(0, &ti) >= 0) {
		ti.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &ti);
	}
	_exit(EXIT_SUCCESS);	/* %% */
}

static void timedout(int sig __attribute__((__unused__)))
{
	signal(SIGALRM, timedout2);
	alarm(10);
	if (timeout_msg)
		ignore_result( write(STDERR_FILENO, timeout_msg, strlen(timeout_msg)) );
	signal(SIGALRM, SIG_IGN);
	alarm(0);
	timedout2(0);
}

/*
 * This handler can be used to inform a shell about signals to login. If you have
 * (root) permissions, you can kill all login children by one signal to the
 * login process.
 *
 * Also, a parent who is session leader is able (before setsid() in the child)
 * to inform the child when the controlling tty goes away (e.g. modem hangup).
 */
static void sig_handler(int signal)
{
	if (child_pid > 0) {
		kill(-child_pid, signal);
		if (signal == SIGTERM)
			kill(-child_pid, SIGHUP);	/* because the shell often ignores SIGTERM */
	} else
		got_sig = 1;
}

/*
 * Let us delay all exit() calls when the user is not authenticated
 * or the session not fully initialized (loginpam_session()).
 */
static void __attribute__((__noreturn__)) sleepexit(int eval)
{
	sleep((unsigned int)getlogindefs_num("FAIL_DELAY", LOGIN_EXIT_TIMEOUT));
	exit(eval);
}

static void process_title_init(int argc, char **argv)
{
	int i;
	char **envp = environ;

	/*
	 * Move the environment so we can reuse the memory.
	 * (Code borrowed from sendmail.)
	 * WARNING: ugly assumptions on memory layout here;
	 *          if this ever causes problems, #undef DO_PS_FIDDLING
	 */
	for (i = 0; envp[i] != NULL; i++)
		continue;

	environ = xmalloc(sizeof(char *) * (i + 1));

	for (i = 0; envp[i] != NULL; i++)
		environ[i] = xstrdup(envp[i]);
	environ[i] = NULL;

	if (i > 0)
		argv_lth = envp[i - 1] + strlen(envp[i - 1]) - argv[0];
	else
		argv_lth = argv[argc - 1] + strlen(argv[argc - 1]) - argv[0];
	if (argv_lth > 1)
		argv0 = argv;
}

static void process_title_update(const char *username)
{
	size_t i;
	const char prefix[] = "login -- ";
	char buf[sizeof(prefix) + LOGIN_NAME_MAX];

	if (!argv0)
		return;

	if (sizeof(buf) < (sizeof(prefix) + strlen(username) + 1))
		return;

	snprintf(buf, sizeof(buf), "%s%s", prefix, username);

	i = strlen(buf);
	if (i > argv_lth - 2) {
		i = argv_lth - 2;
		buf[i] = '\0';
	}
	memset(argv0[0], '\0', argv_lth);	/* clear the memory area */
	strcpy(argv0[0], buf);

	argv0[1] = NULL;
}

static const char *get_thishost(struct login_context *cxt, const char **domain)
{
	if (!cxt->thishost) {
		cxt->thishost = xgethostname();
		if (!cxt->thishost) {
			if (domain)
				*domain = NULL;
			return NULL;
		}
		cxt->thisdomain = strchr(cxt->thishost, '.');
		if (cxt->thisdomain)
			*cxt->thisdomain++ = '\0';
	}

	if (domain)
		*domain = cxt->thisdomain;
	return cxt->thishost;
}

#ifdef MOTDDIR_SUPPORT
static int motddir_filter(const struct dirent *d)
{
	size_t namesz;

#ifdef _DIRENT_HAVE_D_TYPE
	if (d->d_type != DT_UNKNOWN && d->d_type != DT_REG &&
	    d->d_type != DT_LNK)
		return 0;
#endif
	if (*d->d_name == '.')
		return 0;

	namesz = strlen(d->d_name);
	if (!namesz || namesz < MOTDDIR_EXTSIZ + 1 ||
	    strcmp(d->d_name + (namesz - MOTDDIR_EXTSIZ), MOTDDIR_EXT) != 0)
		return 0;

	return 1; /* accept */
}

static int motddir(const char *dirname)
{
	int dd, nfiles, i, done = 0;
	struct dirent **namelist = NULL;

	dd = open(dirname, O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (dd < 0)
		return 0;

	nfiles = scandirat(dd, ".", &namelist, motddir_filter, versionsort);
	if (nfiles <= 0)
		goto done;

	for (i = 0; i < nfiles; i++) {
		struct dirent *d = namelist[i];
		int fd;

		fd = openat(dd, d->d_name, O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			ul_copy_file(fd, fileno(stdout));
			close(fd);
			done++;
		}
	}

	for (i = 0; i < nfiles; i++)
		free(namelist[i]);
	free(namelist);
done:
	close(dd);
	return done;
}
#endif /* MOTDDIR_SUPPORT */

/*
 * Output the /etc/motd file.
 *
 * It determines the name of a login announcement file/dir and outputs it to the
 * user's terminal at login time.  The MOTD_FILE configuration option is a
 * colon-delimited list of filenames or directories.  An empty option disables
 * message-of-the-day printing completely.
 */
static void motd(void)
{
	const char *mb;
	char *file, *list;
	int firstonly, done = 0;

	firstonly = getlogindefs_bool("MOTD_FIRSTONLY", 0);

	mb = getlogindefs_str("MOTD_FILE", _PATH_MOTDFILE);
	if (!mb || !*mb)
		return;

	list = xstrdup(mb);

	for (file = strtok(list, ":"); file; file = strtok(NULL, ":")) {
		struct stat st;

		if (stat(file, &st) < 0)
			continue;
#ifdef MOTDDIR_SUPPORT
		if (S_ISDIR(st.st_mode))
			done += motddir(file);
#endif
		if (S_ISREG(st.st_mode) && st.st_size > 0) {
			int fd = open(file, O_RDONLY, 0);
			if (fd >= 0) {
				ul_copy_file(fd, fileno(stdout));
				close(fd);
			}
			done++;
		}
		if (firstonly && done)
			break;
	}
	free(list);
}

/*
 * Display message of the day and you have mail notifications
 */
static void display_login_messages(void)
{
	motd();

#ifdef LOGIN_STAT_MAIL
	/*
	 * This turns out to be a bad idea: when the mail spool
	 * is NFS mounted, and the NFS connection hangs, the
	 * login hangs, even root cannot login.
	 * Checking for mail should be done from the shell.
	 */
	{
		struct stat st;
		char *mail;

		mail = getenv("MAIL");
		if (mail && stat(mail, &st) == 0 && st.st_size != 0) {
			if (st.st_mtime > st.st_atime)
				printf(_("You have new mail.\n"));
			else
				printf(_("You have mail.\n"));
		}
	}
#endif
}

/*
 * Nice and simple code provided by Linus Torvalds 16-Feb-93.
 * Non-blocking stuff by Maciej W. Rozycki, macro@ds2.pg.gda.pl, 1999.
 *
 * He writes: "Login performs open() on a tty in a blocking mode.
 * In some cases it may make login wait in open() for carrier infinitely,
 * for example if the line is a simplistic case of a three-wire serial
 * connection. I believe login should open the line in non-blocking mode,
 * leaving the decision to make a connection to getty (where it actually
 * belongs)."
 */
static void open_tty(const char *tty)
{
	int i, fd, flags;

	fd = open(tty, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		syslog(LOG_ERR, _("FATAL: can't reopen tty: %m"));
		sleepexit(EXIT_FAILURE);
	}

	if (!isatty(fd)) {
		close(fd);
		syslog(LOG_ERR, _("FATAL: %s is not a terminal"), tty);
		sleepexit(EXIT_FAILURE);
	}

	flags = fcntl(fd, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);

	for (i = 0; i < fd; i++)
		close(i);
	for (i = 0; i < 3; i++)
		if (fd != i)
			dup2(fd, i);
	if (fd >= 3)
		close(fd);
}

static inline void chown_err(const char *what, uid_t uid, gid_t gid)
{
	syslog(LOG_ERR, _("chown (%s, %u, %u) failed: %m"), what, uid, gid);
}

static inline void chmod_err(const char *what, mode_t mode)
{
	syslog(LOG_ERR, _("chmod (%s, %u) failed: %m"), what, mode);
}

static void chown_tty(struct login_context *cxt)
{
	const char *grname;
	uid_t uid = cxt->pwd->pw_uid;
	gid_t gid = cxt->pwd->pw_gid;

	grname = getlogindefs_str("TTYGROUP", TTYGRPNAME);
	if (grname && *grname) {
		struct group *gr = getgrnam(grname);
		if (gr)	/* group by name */
			gid = gr->gr_gid;
		else	/* group by ID */
			gid = (gid_t) getlogindefs_num("TTYGROUP", gid);
	}
	if (fchown(0, uid, gid))				/* tty */
		chown_err(cxt->tty_name, uid, gid);
	if (fchmod(0, cxt->tty_mode))
		chmod_err(cxt->tty_name, cxt->tty_mode);

#ifdef LOGIN_CHOWN_VCS
	if (is_consoletty(0)) {
		if (chown(cxt->vcsn, uid, gid))			/* vcs */
			chown_err(cxt->vcsn, uid, gid);
		if (chmod(cxt->vcsn, cxt->tty_mode))
			chmod_err(cxt->vcsn, cxt->tty_mode);

		if (chown(cxt->vcsan, uid, gid))		/* vcsa */
			chown_err(cxt->vcsan, uid, gid);
		if (chmod(cxt->vcsan, cxt->tty_mode))
			chmod_err(cxt->vcsan, cxt->tty_mode);
	}
#endif
}

/*
 * Reads the current terminal path and initializes cxt->tty_* variables.
 */
static void init_tty(struct login_context *cxt)
{
	struct stat st;
	struct termios tt, ttt = { 0 };
	struct winsize ws = { 0 };
	int fd;

	cxt->tty_mode = (mode_t) getlogindefs_num("TTYPERM", TTY_MODE);

	get_terminal_name(&cxt->tty_path, &cxt->tty_name, &cxt->tty_number);
	fd = get_terminal_stdfd();

	/*
	 * In case login is suid it was possible to use a hardlink as stdin
	 * and exploit races for a local root exploit. (Wojciech Purczynski).
	 *
	 * More precisely, the problem is  ttyn := ttyname(0); ...; chown(ttyn);
	 * here ttyname() might return "/tmp/x", a hardlink to a pseudotty.
	 * All of this is a problem only when login is suid, which it isn't.
	 */
	if (!cxt->tty_path || !*cxt->tty_path ||
	    lstat(cxt->tty_path, &st) != 0 || !S_ISCHR(st.st_mode) ||
	    (st.st_nlink > 1 && strncmp(cxt->tty_path, "/dev/", 5) != 0) ||
	    access(cxt->tty_path, R_OK | W_OK) != 0 || fd == -EINVAL) {

		syslog(LOG_ERR, _("FATAL: bad tty"));
		sleepexit(EXIT_FAILURE);
	}

#ifdef LOGIN_CHOWN_VCS
	if (cxt->tty_number) {
		/* find names of Virtual Console devices, for later mode change */
		snprintf(cxt->vcsn, sizeof(cxt->vcsn), "/dev/vcs%s", cxt->tty_number);
		snprintf(cxt->vcsan, sizeof(cxt->vcsan), "/dev/vcsa%s", cxt->tty_number);
	}
#endif

	/* The TTY size might be reset to 0x0 by the kernel when we close the stdin/stdout/stderr file
	 * descriptors so let's save the size now so we can reapply it later */
	if (ioctl(fd, TIOCGWINSZ, &ws) < 0) {
		syslog(LOG_WARNING, _("TIOCGWINSZ ioctl failed: %m"));
		ws.ws_row = 0;
		ws.ws_col = 0;
	}

	if (tcgetattr(fd, &tt) >= 0) {
		ttt = tt;
		ttt.c_cflag &= ~HUPCL;
	} else {
		ttt.c_cflag = HUPCL;
	}

	if ((fchown(fd, 0, 0) || fchmod(fd, cxt->tty_mode)) && errno != EROFS) {

		syslog(LOG_ERR, _("FATAL: %s: change permissions failed: %m"),
				cxt->tty_path);
		sleepexit(EXIT_FAILURE);
	}

	/* Kill processes left on this tty */
	if ((ttt.c_cflag & HUPCL) == 0)
		tcsetattr(fd, TCSANOW, &ttt);

	/*
	 * Let's close file descriptors before vhangup
	 * https://lkml.org/lkml/2012/6/5/145
	 */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	signal(SIGHUP, SIG_IGN);	/* so vhangup() won't kill us */
	vhangup();
	signal(SIGHUP, SIG_DFL);

	/* open stdin,stdout,stderr to the tty */
	open_tty(cxt->tty_path);

	/* restore tty modes */
	if ((ttt.c_cflag & HUPCL) == 0)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);

	/* Restore tty size */
	if ((ws.ws_row > 0 || ws.ws_col > 0)
	    && ioctl(STDIN_FILENO, TIOCSWINSZ, &ws) < 0)
		syslog(LOG_WARNING, _("TIOCSWINSZ ioctl failed: %m"));
}

/*
 * Logs failed login attempts in _PATH_BTMP, if it exists.
 * Must be called only with username the name of an actual user.
 * The most common login failure is to give password instead of username.
 */
static void log_btmp(struct login_context *cxt)
{
	struct utmpx ut;
	struct timeval tv;

	memset(&ut, 0, sizeof(ut));

	str2memcpy(ut.ut_user,
		cxt->username ? cxt->username : "(unknown)",
		sizeof(ut.ut_user));

	if (cxt->tty_number)
		str2memcpy(ut.ut_id, cxt->tty_number, sizeof(ut.ut_id));
	if (cxt->tty_name)
		str2memcpy(ut.ut_line, cxt->tty_name, sizeof(ut.ut_line));

	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;

	ut.ut_type = LOGIN_PROCESS;	/* XXX doesn't matter */
	ut.ut_pid = cxt->pid;

	if (cxt->hostname) {
		str2memcpy(ut.ut_host, cxt->hostname, sizeof(ut.ut_host));
		if (*cxt->hostaddress)
			memcpy(&ut.ut_addr_v6, cxt->hostaddress,
			       sizeof(ut.ut_addr_v6));
	}

	updwtmpx(_PATH_BTMP, &ut);
}

#ifdef HAVE_LIBAUDIT
static void log_audit(struct login_context *cxt, int status)
{
	int audit_fd;
	struct passwd *pwd = cxt->pwd;

	audit_fd = audit_open();
	if (audit_fd == -1)
		return;
	if (!pwd && cxt->username)
		pwd = getpwnam(cxt->username);

	ignore_result( audit_log_acct_message(audit_fd,
					      AUDIT_USER_LOGIN,
					      NULL,
					      "login",
					      cxt->username ? cxt->username : "(unknown)",
					      pwd ? pwd->pw_uid : (unsigned int)-1,
					      cxt->hostname,
					      NULL,
					      cxt->tty_name,
					      status) );

	close(audit_fd);
}
#else				/* !HAVE_LIBAUDIT */
# define log_audit(cxt, status)
#endif				/* HAVE_LIBAUDIT */

static void log_lastlog(struct login_context *cxt)
{
	struct sigaction sa, oldsa_xfsz;
	struct lastlog ll;
	off_t offset;
	time_t t;
	int fd;

	if (!cxt->pwd)
		return;

	if (cxt->pwd->pw_uid > (uid_t) getlogindefs_num("LASTLOG_UID_MAX", ULONG_MAX))
		return;

	/* lastlog is huge on systems with large UIDs, ignore SIGXFSZ */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGXFSZ, &sa, &oldsa_xfsz);

	fd = open(_PATH_LASTLOG, O_RDWR, 0);
	if (fd < 0)
		goto done;
	offset = cxt->pwd->pw_uid * sizeof(ll);

	/*
	 * Print last log message.
	 */
	if (!cxt->quiet) {
		if ((pread(fd, (void *)&ll, sizeof(ll), offset) == sizeof(ll)) &&
		    ll.ll_time != 0) {
			char time_string[CTIME_BUFSIZ];
			char buf[sizeof(ll.ll_host) + 1];

			time_t ll_time = (time_t)ll.ll_time;

			ctime_r(&ll_time, time_string);
			printf(_("Last login: %.*s "), 24 - 5, time_string);

			if (*ll.ll_host != '\0') {
				mem2strcpy(buf, ll.ll_host, sizeof(ll.ll_host), sizeof(buf));
				printf(_("from %s\n"), buf);
			} else {
				mem2strcpy(buf, ll.ll_line, sizeof(ll.ll_line), sizeof(buf));
				printf(_("on %s\n"), buf);
			}
		}
	}

	memset((char *)&ll, 0, sizeof(ll));

	time(&t);
	ll.ll_time = t;		/* ll_time is always 32bit */

	if (cxt->tty_name)
		str2memcpy(ll.ll_line, cxt->tty_name, sizeof(ll.ll_line));
	if (cxt->hostname)
		str2memcpy(ll.ll_host, cxt->hostname, sizeof(ll.ll_host));

	if (pwrite(fd, (void *)&ll, sizeof(ll), offset) != sizeof(ll))
		warn(_("write lastlog failed"));
done:
	if (fd >= 0)
		close(fd);

	sigaction(SIGXFSZ, &oldsa_xfsz, NULL);		/* restore original setting */
}

/*
 * Update wtmp and utmp logs.
 */
static void log_utmp(struct login_context *cxt)
{
	struct utmpx ut = { 0 };
	struct utmpx *utp = NULL;
	struct timeval tv = { 0 };

	utmpxname(_PATH_UTMP);
	setutxent();

	/* Find pid in utmp.
	 *
	 * login sometimes overwrites the runlevel entry in /var/run/utmp,
	 * confusing sysvinit. I added a test for the entry type, and the
	 * problem was gone. (In a runlevel entry, st_pid is not really a pid
	 * but some number calculated from the previous and current runlevel.)
	 * -- Michael Riepe <michael@stud.uni-hannover.de>
	 */
	while ((utp = getutxent()))
		if (utp->ut_pid == cxt->pid
		    && utp->ut_type >= INIT_PROCESS
		    && utp->ut_type <= DEAD_PROCESS)
			break;

	/* If we can't find a pre-existing entry by pid, try by line.
	 * BSD network daemons may rely on this. */
	if (utp == NULL && cxt->tty_name) {
		setutxent();
		ut.ut_type = LOGIN_PROCESS;
		str2memcpy(ut.ut_line, cxt->tty_name, sizeof(ut.ut_line));
		utp = getutxline(&ut);
	}

	/* If we can't find a pre-existing entry by pid and line, try it by id.
	 * Very stupid telnetd daemons don't set up utmp at all. (kzak) */
	if (utp == NULL && cxt->tty_number) {
		setutxent();
		ut.ut_type = DEAD_PROCESS;
		str2memcpy(ut.ut_id, cxt->tty_number, sizeof(ut.ut_id));
		utp = getutxid(&ut);
	}

	if (utp)
		memcpy(&ut, utp, sizeof(ut));
	else
		/* some gettys/telnetds don't initialize utmp... */
		memset(&ut, 0, sizeof(ut));

	if (cxt->tty_number && ut.ut_id[0] == 0)
		str2memcpy(ut.ut_id, cxt->tty_number, sizeof(ut.ut_id));
	if (cxt->username)
		str2memcpy(ut.ut_user, cxt->username, sizeof(ut.ut_user));
	if (cxt->tty_name)
		str2memcpy(ut.ut_line, cxt->tty_name, sizeof(ut.ut_line));

	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
	ut.ut_type = USER_PROCESS;
	ut.ut_pid = cxt->pid;
	if (cxt->hostname) {
		str2memcpy(ut.ut_host, cxt->hostname, sizeof(ut.ut_host));
		if (*cxt->hostaddress)
			memcpy(&ut.ut_addr_v6, cxt->hostaddress,
			       sizeof(ut.ut_addr_v6));
	}

	pututxline(&ut);
	endutxent();

	updwtmpx(_PATH_WTMP, &ut);
}

static void log_syslog(struct login_context *cxt)
{
	struct passwd *pwd = cxt->pwd;

	if (!cxt->tty_name)
		return;

	if (!strncmp(cxt->tty_name, "ttyS", 4))
		syslog(LOG_INFO, _("DIALUP AT %s BY %s"),
		       cxt->tty_name, pwd->pw_name);

	if (!pwd->pw_uid) {
		if (cxt->hostname)
			syslog(LOG_NOTICE, _("ROOT LOGIN ON %s FROM %s"),
			       cxt->tty_name, cxt->hostname);
		else
			syslog(LOG_NOTICE, _("ROOT LOGIN ON %s"), cxt->tty_name);
	} else {
		if (cxt->hostname)
			syslog(LOG_INFO, _("LOGIN ON %s BY %s FROM %s"),
			       cxt->tty_name, pwd->pw_name, cxt->hostname);
		else
			syslog(LOG_INFO, _("LOGIN ON %s BY %s"), cxt->tty_name,
			       pwd->pw_name);
	}
}

/* encapsulate stupid "void **" pam_get_item() API */
static int loginpam_get_username(pam_handle_t *pamh, const char **name)
{
	const void *item = (const void *)*name;
	int rc;

	rc = pam_get_item(pamh, PAM_USER, &item);
	*name = (const char *)item;
	return rc;
}

static void loginpam_err(pam_handle_t *pamh, int retcode)
{
	const char *msg = pam_strerror(pamh, retcode);

	if (msg) {
		fprintf(stderr, "\n%s\n", msg);
		syslog(LOG_ERR, "%s", msg);
	}
	pam_end(pamh, retcode);
	sleepexit(EXIT_FAILURE);
}

/*
 * Composes "<host> login: " string; or returns "login: " if -H is given or
 * LOGIN_PLAIN_PROMPT=yes configured.
 */
static const char *loginpam_get_prompt(struct login_context *cxt)
{
	const char *host;
	char *prompt = NULL, *dflt_prompt = _("login: ");

	if (cxt->nohost)
		return dflt_prompt;	/* -H on command line */

	if (getlogindefs_bool("LOGIN_PLAIN_PROMPT", 0) == 1)
		return dflt_prompt;

	if (!(host = get_thishost(cxt, NULL)))
		return dflt_prompt;

	xasprintf(&prompt, "%s %s", host, dflt_prompt);

	return prompt;
}

static inline int is_pam_failure(int rc)
{
	return rc != PAM_SUCCESS;
}

static pam_handle_t *init_loginpam(struct login_context *cxt)
{
	pam_handle_t *pamh = NULL;
	int rc;

	/*
	 * username is initialized to NULL and if specified on the command line
	 * it is set.  Therefore, we are safe not setting it to anything.
	 */
	rc = pam_start(cxt->remote ? "remote" : "login",
		       cxt->username, &cxt->conv, &pamh);
	if (rc != PAM_SUCCESS) {
		warnx(_("PAM failure, aborting: %s"), pam_strerror(pamh, rc));
		syslog(LOG_ERR, _("Couldn't initialize PAM: %s"),
		       pam_strerror(pamh, rc));
		sleepexit(EXIT_FAILURE);
	}

	/* hostname & tty are either set to NULL or their correct values,
	 * depending on how much we know. */
	rc = pam_set_item(pamh, PAM_RHOST, cxt->hostname);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	if (cxt->tty_path) {
		rc = pam_set_item(pamh, PAM_TTY, cxt->tty_path);
		if (is_pam_failure(rc))
			loginpam_err(pamh, rc);
	}

	/*
	 * Andrew.Taylor@cal.montage.ca: Provide a user prompt to PAM so that
	 * the "login: " prompt gets localized. Unfortunately, PAM doesn't have
	 * an interface to specify the "Password: " string (yet).
	 */
	rc = pam_set_item(pamh, PAM_USER_PROMPT, loginpam_get_prompt(cxt));
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	/* We don't need the original username. We have to follow PAM. */
	cxt->username = NULL;
	cxt->pamh = pamh;

	return pamh;
}

static void loginpam_auth(struct login_context *cxt)
{
	int rc, show_unknown, keep_username;
	unsigned int retries, failcount = 0;
	const char *hostname = cxt->hostname ? cxt->hostname :
			       cxt->tty_name ? cxt->tty_name : "<unknown>";
	pam_handle_t *pamh = cxt->pamh;

	/* if we didn't get a user on the command line, set it to NULL */
	loginpam_get_username(pamh, &cxt->username);

	show_unknown = getlogindefs_bool("LOG_UNKFAIL_ENAB", 0);
	retries = getlogindefs_num("LOGIN_RETRIES", LOGIN_MAX_TRIES);
	keep_username = getlogindefs_bool("LOGIN_KEEP_USERNAME", 0);

	/*
	 * There may be better ways to deal with some of these conditions, but
	 * at least this way I don't think we'll be giving away information...
	 *
	 * Perhaps someday we can trust that all PAM modules will pay attention
	 * to failure count and get rid of LOGIN_MAX_TRIES?
	 */
	rc = pam_authenticate(pamh, 0);

	while ((++failcount < retries) &&
	       ((rc == PAM_AUTH_ERR) ||
		(rc == PAM_USER_UNKNOWN) ||
		(rc == PAM_CRED_INSUFFICIENT) ||
		(rc == PAM_AUTHINFO_UNAVAIL))) {

		if (rc == PAM_USER_UNKNOWN && !show_unknown)
			/*
			 * Logging unknown usernames may be a security issue if
			 * a user enters their password instead of their login name.
			 */
			cxt->username = NULL;
		else
			loginpam_get_username(pamh, &cxt->username);

		syslog(LOG_NOTICE,
		       _("FAILED LOGIN %u FROM %s FOR %s, %s"),
		       failcount, hostname,
		       cxt->username ? cxt->username : "(unknown)",
		       pam_strerror(pamh, rc));

		log_btmp(cxt);
		log_audit(cxt, 0);

		if (!keep_username || rc == PAM_USER_UNKNOWN) {
			pam_set_item(pamh, PAM_USER, NULL);
			fprintf(stderr, _("Login incorrect\n\n"));
		} else
			fprintf(stderr, _("Password incorrect\n\n"));

		rc = pam_authenticate(pamh, 0);
	}

	if (is_pam_failure(rc)) {

		if (rc == PAM_USER_UNKNOWN && !show_unknown)
			cxt->username = NULL;
		else
			loginpam_get_username(pamh, &cxt->username);

		if (rc == PAM_MAXTRIES)
			syslog(LOG_NOTICE,
			       _("TOO MANY LOGIN TRIES (%u) FROM %s FOR %s, %s"),
			       failcount, hostname,
			       cxt->username ? cxt->username : "(unknown)",
			       pam_strerror(pamh, rc));
		else
			syslog(LOG_NOTICE,
			       _("FAILED LOGIN SESSION FROM %s FOR %s, %s"),
			       hostname,
			       cxt->username ? cxt->username : "(unknown)",
			       pam_strerror(pamh, rc));

		log_btmp(cxt);
		log_audit(cxt, 0);

		fprintf(stderr, _("\nLogin incorrect\n"));
		pam_end(pamh, rc);
		sleepexit(EXIT_SUCCESS);
	}
}

static void loginpam_acct(struct login_context *cxt)
{
	int rc;
	pam_handle_t *pamh = cxt->pamh;

	rc = pam_acct_mgmt(pamh, 0);

	if (rc == PAM_NEW_AUTHTOK_REQD)
		rc = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);

	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	/*
	 * First get the username that we are actually using, though.
	 */
	rc = loginpam_get_username(pamh, &cxt->username);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	if (!cxt->username || !*cxt->username) {
		warnx(_("\nSession setup problem, abort."));
		syslog(LOG_ERR, _("NULL user name. Abort."));
		pam_end(pamh, PAM_SYSTEM_ERR);
		sleepexit(EXIT_FAILURE);
	}
}

/*
 * Note that the position of the pam_setcred() call is discussable:
 *
 *  - the PAM docs recommend pam_setcred() before pam_open_session()
 *  - but the original RFC http://www.opengroup.org/rfc/mirror-rfc/rfc86.0.txt
 *    uses pam_setcred() after pam_open_session()
 *
 * The old login versions (before year 2011) followed the RFC. This is probably
 * not optimal, because there could be a dependence between some session modules
 * and the user's credentials.
 *
 * The best is probably to follow openssh and call pam_setcred() before and
 * after pam_open_session().                -- kzak@redhat.com (18-Nov-2011)
 *
 */
static void loginpam_session(struct login_context *cxt)
{
	int rc;
	pam_handle_t *pamh = cxt->pamh;

	rc = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	rc = pam_open_session(pamh, cxt->quiet ? PAM_SILENT : 0);
	if (is_pam_failure(rc)) {
		pam_setcred(cxt->pamh, PAM_DELETE_CRED);
		loginpam_err(pamh, rc);
	}

	rc = pam_setcred(pamh, PAM_REINITIALIZE_CRED);
	if (is_pam_failure(rc)) {
		pam_close_session(pamh, 0);
		loginpam_err(pamh, rc);
	}
}

/*
 * Detach the controlling terminal, fork, restore syslog stuff, and create
 * a new session.
 */
static void fork_session(struct login_context *cxt)
{
	struct sigaction sa, oldsa_hup, oldsa_term;

	signal(SIGALRM, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_IGN);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sa, NULL);

	sigaction(SIGHUP, &sa, &oldsa_hup);	/* ignore when TIOCNOTTY */

	/*
	 * Detach the controlling tty.
	 * We don't need the tty in a parent who only waits for a child.
	 * The child calls setsid() that detaches from the tty as well.
	 */
	ioctl(0, TIOCNOTTY, NULL);

	/*
	 * We have to beware of SIGTERM, because leaving a PAM session
	 * without pam_close_session() is a pretty bad thing.
	 */
	sa.sa_handler = sig_handler;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, &oldsa_term);

	closelog();

	/*
	 * We must fork before setuid(), because we need to call
	 * pam_close_session() as root.
	 */
	child_pid = fork();
	if (child_pid < 0) {
		warn(_("fork failed"));

		pam_setcred(cxt->pamh, PAM_DELETE_CRED);
		pam_end(cxt->pamh, pam_close_session(cxt->pamh, 0));
		sleepexit(EXIT_FAILURE);
	}

	if (child_pid) {
		/*
		 * parent - wait for child to finish, then clean up session
		 */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		free_getlogindefs_data();

		sa.sa_handler = SIG_IGN;
		sigaction(SIGQUIT, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);

		/* wait as long as any child is there */
		while (wait(NULL) == -1 && errno == EINTR) ;
		openlog("login", LOG_ODELAY, LOG_AUTHPRIV);

		pam_setcred(cxt->pamh, PAM_DELETE_CRED);
		pam_end(cxt->pamh, pam_close_session(cxt->pamh, 0));
		exit(EXIT_SUCCESS);
	}

	/*
	 * child
	 */
	sigaction(SIGHUP, &oldsa_hup, NULL);		/* restore old state */
	sigaction(SIGTERM, &oldsa_term, NULL);
	if (got_sig)
		exit(EXIT_FAILURE);

	/*
	 * Problem: if the user's shell is a shell like ash that doesn't do
	 * setsid() or setpgrp(), then a ctrl-\, sending SIGQUIT to every
	 * process in the pgrp, will kill us.
	 */

	/* start new session */
	setsid();

	/* make sure we have a controlling tty */
	open_tty(cxt->tty_path);
	openlog("login", LOG_ODELAY, LOG_AUTHPRIV);	/* reopen */

	/*
	 * TIOCSCTTY: steal tty from other process group.
	 */
	if (ioctl(0, TIOCSCTTY, 1))
		syslog(LOG_ERR, _("TIOCSCTTY failed: %m"));
	signal(SIGINT, SIG_DFL);
}

/*
 * Initialize $TERM, $HOME, ...
 */
static void init_environ(struct login_context *cxt)
{
	struct passwd *pwd = cxt->pwd;
	char *termenv, **env;
	char tmp[PATH_MAX];
	int len, i;

	termenv = getenv("TERM");
	if (termenv)
		termenv = xstrdup(termenv);

	/* destroy environment unless user has requested preservation (-p) */
	if (!cxt->keep_env)
		environ = xcalloc(1, sizeof(char *));

	xsetenv("HOME", pwd->pw_dir, 0);	/* legal to override */
	xsetenv("USER", pwd->pw_name, 1);
	xsetenv("SHELL", pwd->pw_shell, 1);
	xsetenv("TERM", termenv ? termenv : "dumb", 1);
	free(termenv);

	if (pwd->pw_uid) {
		if (logindefs_setenv("PATH", "ENV_PATH", _PATH_DEFPATH) != 0)
			err(EXIT_FAILURE, _("failed to set the %s environment variable"), "PATH");

	} else if (logindefs_setenv("PATH", "ENV_ROOTPATH", NULL) != 0 &&
		   logindefs_setenv("PATH", "ENV_SUPATH", _PATH_DEFPATH_ROOT) != 0) {
			err(EXIT_FAILURE, _("failed to set the %s environment variable"), "PATH");
	}

	/* mailx will give a funny error msg if you forget this one */
	len = snprintf(tmp, sizeof(tmp), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
	if (len > 0 && (size_t)len < sizeof(tmp))
		xsetenv("MAIL", tmp, 0);

	/* LOGNAME is not documented in login(1) but HP-UX 6.5 does it. We'll
	 * not allow modifying it.
	 */
	xsetenv("LOGNAME", pwd->pw_name, 1);

	env = pam_getenvlist(cxt->pamh);
	for (i = 0; env && env[i]; i++)
		putenv(env[i]);
}

/*
 * This is called for the -h option, initializes cxt->{hostname,hostaddress}.
 */
static void init_remote_info(struct login_context *cxt, char *remotehost)
{
	const char *domain;
	char *p;
	struct addrinfo hints, *info = NULL;

	cxt->remote = 1;

	get_thishost(cxt, &domain);

	if (domain && (p = strchr(remotehost, '.')) &&
	    strcasecmp(p + 1, domain) == 0)
		*p = '\0';

	cxt->hostname = xstrdup(remotehost);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	cxt->hostaddress[0] = 0;

	if (getaddrinfo(cxt->hostname, NULL, &hints, &info) == 0 && info) {
		if (info->ai_family == AF_INET) {
			struct sockaddr_in *sa =
				    (struct sockaddr_in *)info->ai_addr;

			memcpy(cxt->hostaddress, &(sa->sin_addr), sizeof(sa->sin_addr));

		} else if (info->ai_family == AF_INET6) {
			struct sockaddr_in6 *sa =
				     (struct sockaddr_in6 *)info->ai_addr;
#ifdef IN6_IS_ADDR_V4MAPPED
			if (IN6_IS_ADDR_V4MAPPED(&sa->sin6_addr)) {
				const uint8_t *bytes = sa->sin6_addr.s6_addr;
				struct in_addr addr = { *(const in_addr_t *)(bytes + 12) };

				memcpy(cxt->hostaddress, &addr, sizeof(struct in_addr));
			} else
#endif
				memcpy(cxt->hostaddress, &(sa->sin6_addr), sizeof(sa->sin6_addr));
		}
		freeaddrinfo(info);
	}
}

static void __attribute__((__noreturn__)) usage(void)
{
	fputs(USAGE_HEADER, stdout);
	printf(_(" %s [-p] [-h <host>] [-H] [[-f] <username>]\n"), program_invocation_short_name);
	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Begin a session on the system.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	puts(_(" -p             do not destroy the environment"));
	puts(_(" -f             skip a login authentication"));
	puts(_(" -h <host>      hostname to be used for utmp logging"));
	puts(_(" -H             suppress hostname in the login prompt"));
	printf("     --help     %s\n", USAGE_OPTSTR_HELP);
	printf(" -V, --version  %s\n", USAGE_OPTSTR_VERSION);
	printf(USAGE_MAN_TAIL("login(1)"));
	exit(EXIT_SUCCESS);
}

static void load_credentials(struct login_context *cxt) {
	char str[32] = { 0 };
	char *env;
	struct path_cxt *pc;

	env = safe_getenv("CREDENTIALS_DIRECTORY");
        if (!env)
                return;

	pc = ul_new_path("%s", env);
	if (!pc) {
		syslog(LOG_WARNING, _("failed to initialize path context"));
		return;
	}

	if (ul_path_read_buffer(pc, str, sizeof(str), "login.noauth") > 0
	    && *str && strcmp(str, "yes") == 0)
		cxt->noauth = 1;

	ul_unref_path(pc);
}

static void initialize(int argc, char **argv, struct login_context *cxt)
{
	int c;
	unsigned int timeout;
	struct sigaction act;

	/* the only two longopts to satisfy UL standards */
	enum { HELP_OPTION = CHAR_MAX + 1 };
	const struct option longopts[] = {
		{"help", no_argument, NULL, HELP_OPTION},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	/*
	 * This bounds the time given to login.  Not a define, so it can
	 * be patched on machines where it's too small.
	 */
	timeout = (unsigned int)getlogindefs_num("LOGIN_TIMEOUT", LOGIN_TIMEOUT);

	/* TRANSLATORS: The standard value for %u is 60. */
	xasprintf(&timeout_msg, _("%s: timed out after %u seconds"),
				  program_invocation_short_name, timeout);

	signal(SIGALRM, timedout);
	sigaction(SIGALRM, NULL, &act);
	act.sa_flags &= ~SA_RESTART;
	sigaction(SIGALRM, &act, NULL);
	alarm(timeout);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	setpriority(PRIO_PROCESS, 0, 0);
	process_title_init(argc, argv);

	load_credentials(cxt);

	while ((c = getopt_long(argc, argv, "fHh:pV", longopts, NULL)) != -1)
		switch (c) {
		case 'f':
			cxt->noauth = 1;
			break;

		case 'H':
			cxt->nohost = 1;
			break;

		case 'h':
			if (getuid()) {
				fprintf(stderr,
					_("login: -h is for superuser only\n"));
				exit(EXIT_FAILURE);
			}
			init_remote_info(cxt, optarg);
			break;

		case 'p':
			cxt->keep_env = 1;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case HELP_OPTION:
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		char *p = *argv;

		/* username from command line */
		cxt->cmd_username = xstrdup(p);
		/* used temporary, it'll be replaced by username from PAM or/and cxt->pwd */
		cxt->username = cxt->cmd_username;

		/* Wipe the name - some people mistype their password here. */
		/* (Of course we are too late, but perhaps this helps a little...) */
#ifdef HAVE_EXPLICIT_BZERO
		explicit_bzero(p, strlen(p));
#else
		while (*p)
			*p++ = ' ';
#endif
	}
#ifdef HAVE_CLOSE_RANGE
	if (close_range(STDERR_FILENO + 1, ~0U, 0) < 0)
#endif
		ul_close_all_fds(STDERR_FILENO + 1, ~0U);
}

int main(int argc, char **argv)
{
	char *child_argv[10];
	int child_argc = 0;
	struct passwd *pwd;
	struct login_context cxt = {
		.tty_mode = TTY_MODE,		  /* tty chmod() */
		.pid = getpid(),		  /* PID */
#ifdef HAVE_SECURITY_PAM_MISC_H
		.conv = { misc_conv, NULL }	  /* Linux-PAM conversation function */
#elif defined(HAVE_SECURITY_OPENPAM_H)
		.conv = { openpam_ttyconv, NULL } /* OpenPAM conversation function */
#endif
	};
	bool script;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	initialize(argc, argv, &cxt);

	setpgrp();	 /* set pgid to pid this means that setsid() will fail */
	init_tty(&cxt);

	openlog("login", LOG_ODELAY, LOG_AUTHPRIV);

	init_loginpam(&cxt);

	/* login -f, then the user has already been authenticated */
	cxt.noauth = cxt.noauth && getuid() == 0 ? 1 : 0;

	if (!cxt.noauth)
		loginpam_auth(&cxt);

	/*
	 * Authentication may be skipped (for example, during krlogin, rlogin,
	 * etc...), but it doesn't mean that we can skip other account checks.
	 * The account could be disabled or the password has expired (although
	 * the kerberos ticket is valid).      -- kzak@redhat.com (22-Feb-2006)
	 */
	loginpam_acct(&cxt);

	cxt.pwd = xgetpwnam(cxt.username, &cxt.pwdbuf);
	if (!cxt.pwd) {
		warnx(_("\nSession setup problem, abort."));
		syslog(LOG_ERR, _("Invalid user name \"%s\". Abort."),
		       cxt.username);
		pam_end(cxt.pamh, PAM_SYSTEM_ERR);
		sleepexit(EXIT_FAILURE);
	}

	pwd = cxt.pwd;
	cxt.username = pwd->pw_name;

	/*
	 * Initialize the supplementary group list. This should be done before
	 * pam_setcred, because PAM modules might add groups during that call.
	 *
	 * For root we don't call initgroups, instead we call setgroups with
	 * group 0. This avoids the need to step through the whole group file,
	 * which can cause problems if NIS, NIS+, LDAP or something similar
	 * is used and the machine has network problems.
	 */
	{
		int retcode;

		retcode = pwd->pw_uid ? initgroups(cxt.username, pwd->pw_gid) :	/* user */
					setgroups(0, NULL);			/* root */
		if (retcode < 0) {
			syslog(LOG_ERR, _("groups initialization failed: %m"));
			warnx(_("\nSession setup problem, abort."));
			pam_end(cxt.pamh, PAM_SYSTEM_ERR);
			sleepexit(EXIT_FAILURE);
		}
	}

	cxt.quiet = get_hushlogin_status(pwd, 1) == 1 ? 1 : 0;

	/*
	 * Open PAM session (after successful authentication and account check).
	 */
	loginpam_session(&cxt);

	/* committed to login -- turn off timeout */
	alarm((unsigned int)0);
	free(timeout_msg);
	timeout_msg = NULL;

	endpwent();

	log_utmp(&cxt);
	log_audit(&cxt, 1);
	log_lastlog(&cxt);

	chown_tty(&cxt);

	if (setgid(pwd->pw_gid) < 0 && pwd->pw_gid) {
		syslog(LOG_ALERT, _("setgid() failed"));
		exit(EXIT_FAILURE);
	}

	if (pwd->pw_shell == NULL || *pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;

	init_environ(&cxt);		/* init $HOME, $TERM ... */

	process_title_update(cxt.username);

	log_syslog(&cxt);

	if (!cxt.quiet)
		display_login_messages();

	/*
	 * Detach the controlling terminal, fork, and create a new session
	 * and reinitialize syslog stuff.
	 */
	fork_session(&cxt);

	/* discard permissions last so we can't get killed and drop core */
	if (setuid(pwd->pw_uid) < 0 && pwd->pw_uid) {
		syslog(LOG_ALERT, _("setuid() failed"));
		exit(EXIT_FAILURE);
	}

	/* wait until here to change directory! */
	if (chdir(pwd->pw_dir) < 0) {
		warn(_("%s: change directory failed"), pwd->pw_dir);

		if (!getlogindefs_bool("DEFAULT_HOME", 1))
			exit(0);
		if (chdir("/"))
			exit(EXIT_FAILURE);
		pwd->pw_dir = "/";
		printf(_("Logging in with home = \"/\".\n"));
	}

	/* if the shell field has a space: treat it like a shell script */
	script = strchr(pwd->pw_shell, ' ') != NULL;

	if (script) {
		char *buff;

		xasprintf(&buff, "exec %s", pwd->pw_shell);
		child_argv[child_argc++] = "/bin/sh";
		child_argv[child_argc++] = "-sh";
		child_argv[child_argc++] = "-c";
		child_argv[child_argc++] = buff;
	} else {
		char *buff, *p;

		xasprintf(&buff, "-%.*s", PATH_MAX,
			  ((p = strrchr(pwd->pw_shell, '/')) ?
			  p + 1 : pwd->pw_shell));
		child_argv[child_argc++] = pwd->pw_shell;
		child_argv[child_argc++] = buff;
	}

	child_argv[child_argc++] = NULL;

	/* http://www.linux-pam.org/Linux-PAM-html/adg-interface-by-app-expected.html#adg-pam_end */
	(void) pam_end(cxt.pamh, PAM_SUCCESS|PAM_DATA_SILENT);

	execvp(child_argv[0], child_argv + 1);

	if (script)
		warn(_("couldn't exec shell script"));
	else
		warn(_("no shell"));

	exit(EXIT_SUCCESS);
}
