#include "config.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <sys/wait.h>
#include <sys/file.h>

#include "libcommon.h"

#ifdef COMPAT_HEADERS
#include "compat/linux-limits.h"
#endif

// There must be a universal way to find these!
#define TRUE (1)
#define FALSE (0)

#ifdef ESIX_5_3_2_D
#define VTBASE "/dev/vt%02d"
#endif

// Where your VTs are hidden
#ifdef __linux__
#define VTNAME "/dev/tty%d"
#define VTNAME2 "/dev/vc/%d"
#endif

#ifndef VTNAME
#error vt device name must be defined
#endif

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...] -- command\n"), get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _("This utility helps you to start a program on a new virtual terminal (VT).\n"));

	if (options) {
		int max = 0;

		fprintf(stderr, "\n");
		fprintf(stderr, _("Options:"));
		fprintf(stderr, "\n");

		for (h = options; h && h->opts; h++) {
			int len = (int) strlen(h->opts);
			if (max < len)
				max = len;
		}
		max += 2;

		for (h = options; h && h->opts; h++)
			fprintf(stderr, "  %-*s %s\n", max, h->opts, h->desc);
	}

	fprintf(stderr, "\n");
	fprintf(stderr, _("Report bugs to authors.\n"));
	fprintf(stderr, "\n");

	exit(rc);
}

/*
 * Support for Spawn_Console: openvt running from init
 * added by Joshua Spoerri, Thu Jul 18 21:13:16 EDT 1996
 *
 *  -u     Figure  out  the  owner  of the current VT, and run
 *         login as that user.  Suitable to be called by init.
 *         Shouldn't be used with -c or -l.
 *         Sample inittab line:
 *                kb::kbrequest:/usr/bin/openvt -us
 *
 * It is the job of authenticate_user() to find out who
 * produced this keyboard signal.  It is called only as root.
 *
 * Note that there is a race condition: curvt may not be the vt
 * from which the keyboard signal was produced.
 * (Possibly the signal was not produced at the keyboard at all,
 * but by a "kill -SIG 1".  However, only root can do this.)
 *
 * Conclusion: do not use this in high security environments.
 * Or fix the code below to be more suspicious.
 *
 * Maybe it is better to just start a login at the new vt,
 * instead of pre-authenticating the user with "login -f".
 */

static char *
authenticate_user(int curvt)
{
	DIR *dp;
	struct dirent *dentp;
	struct stat buf;
	dev_t console_dev;
	ino_t console_ino;
	uid_t console_uid;
	char filename[NAME_MAX + 12];
	struct passwd *pwnam;

	if (!(dp = opendir("/proc")))
		kbd_error(EXIT_FAILURE, errno, "opendir(/proc)");

	/* get the current tty */
	/* try /dev/ttyN, then /dev/vc/N */
	sprintf(filename, VTNAME, curvt);
	if (stat(filename, &buf)) {
		int errsv = errno;
		sprintf(filename, VTNAME2, curvt);
		if (stat(filename, &buf)) {
			/* give error message for first attempt */
			sprintf(filename, VTNAME, curvt);
			kbd_error(EXIT_FAILURE, errsv, "%s", filename);
		}
	}
	console_dev = buf.st_dev;
	console_ino = buf.st_ino;
	console_uid = buf.st_uid;

	/* get the owner of current tty */
	if (!(pwnam = getpwuid(console_uid)))
		kbd_error(EXIT_FAILURE, errno, "getpwuid");

	/* check to make sure that user has a process on that tty */
	/* this will fail for example when X is running on the tty */
	while ((dentp = readdir(dp))) {
		sprintf(filename, "/proc/%s/fd/0", dentp->d_name);

		if (stat(filename, &buf))
			continue;

		if (buf.st_dev == console_dev && buf.st_ino == console_ino && buf.st_uid == console_uid)
			goto got_a_process;
	}

	kbd_error(EXIT_FAILURE, 0, _("Couldn't find owner of current tty!"));

got_a_process:
	closedir(dp);

	return pwnam->pw_name;
}

static int
open_vt(char *vtname, int force)
{
	int fd;

	if ((fd = open(vtname, O_RDWR)) == -1)
		return -1;

	if (ioctl(fd, TIOCSCTTY, force) == -1) {
		close(fd);
		return -1;
	}

	return fd;
}

static void
sighandler(int sig __attribute__((unused)),
           siginfo_t *si __attribute__((unused)),
           void *uc __attribute__((unused)))
{
	return;
}

static int
change_vt(int fd, int vtno)
{
	int rc = -1;
	timer_t timerid = NULL;
	struct sigaction sa;
	struct sigevent sev;
	struct itimerspec its;

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sighandler;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		kbd_warning(errno, _("Unable to set signal handler"));
		return rc;
	}

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGALRM;
	sev.sigev_value.sival_ptr = &timerid;

	if (timer_create(CLOCK_REALTIME, &sev, &timerid) < 0) {
		kbd_warning(errno, _("Unable to create timer"));
		goto fail;
	}

	its.it_value.tv_sec     = 1;
	its.it_value.tv_nsec    = 0;
	its.it_interval.tv_sec  = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	if (timer_settime(timerid, 0, &its, NULL) < 0) {
		kbd_warning(errno, _("Unable to set timer"));
		goto fail;
	}

	do {
		errno = 0;

		if (ioctl(fd, VT_ACTIVATE, vtno) < 0 && errno != EINTR) {
			kbd_warning(errno, _("Couldn't activate vt %d"), vtno);
			goto fail;
		}

		if (ioctl(fd, VT_WAITACTIVE, vtno) < 0 && errno != EINTR) {
			kbd_warning(errno, "ioctl(VT_WAITACTIVE)");
			goto fail;
		}
	} while (errno == EINTR);

	rc = 0;
fail:
	if (timerid)
		timer_delete(timerid);

	signal(SIGALRM, SIG_DFL);

	return rc;
}

int main(int argc, char *argv[])
{
	int opt, i;
	struct vt_stat vtstat;
	int pid          = 0;
	int vtno         = -1;
	int fd           = -1;
	int consfd       = -1;
	int force        = 0;
	char optc        = FALSE;
	char show        = FALSE;
	char login       = FALSE;
	char verbose     = FALSE;
	char direct_exec = FALSE;
	char do_wait     = FALSE;
	char as_user     = FALSE;
	char vtname[PATH_MAX+1];
	char *cmd = NULL, *def_cmd = NULL, *username = NULL;

	set_progname(argv[0]);
	setuplocale();

	struct option long_options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "version", no_argument, 0, 'V' },
		{ "verbose", no_argument, 0, 'v' },
		{ "exec", no_argument, 0, 'e' },
		{ "force", no_argument, 0, 'f' },
		{ "login", no_argument, 0, 'l' },
		{ "user", no_argument, 0, 'u' },
		{ "switch", no_argument, 0, 's' },
		{ "wait", no_argument, 0, 'w' },
		{ "console", required_argument, 0, 'c' },
		{ 0, 0, 0, 0 }
	};

	const struct kbd_help opthelp[] = {
		{ "-c, --console=DEV", _("the console device to be used.") },
		{ "-e, --exec",        _("execute the command, without forking.") },
		{ "-f, --force",       _("force opening a VT without checking.") },
		{ "-l, --login",       _("make the command a login shell.") },
		{ "-u, --user",        _("figure out the owner of the current VT.") },
		{ "-s, --switch",      _("switch to the new VT.") },
		{ "-w, --wait",        _("wait for command to complete") },
		{ "-v, --verbose",     _("be more verbose.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	while ((opt = getopt_long(argc, argv, "c:lsfuewhvV", long_options, NULL)) != -1) {
		switch (opt) {
			case 'c':
				optc = 1; /* vtno was specified by the user */
				vtno = (int)atol(optarg);

				if (vtno <= 0 || vtno > 63)
					kbd_error(5, 0, _("%s: Illegal vt number"), optarg);

				/* close security holes - until we can do this safely */
				if (setuid(getuid()) < 0)
					kbd_error(5, errno, "%s: setuid", optarg);
				break;
			case 'l':
				login = TRUE;
				break;
			case 's':
				show = TRUE;
				break;
			case 'v':
				verbose = TRUE;
				break;
			case 'f':
				force = 1;
				break;
			case 'e':
				direct_exec = TRUE;
				break;
			case 'w':
				do_wait = TRUE;
				break;
			case 'u':
				/* we'll let 'em get away with the meaningless -ul combo */
				if (getuid())
					kbd_error(EXIT_FAILURE, 0, _("Only root can use the -u flag."));

				as_user = TRUE;
				break;
			case 'V':
				print_version_and_exit();
				break;
			default:
			case 'h':
				usage(EXIT_SUCCESS, opthelp);
				break;
		}
	}

	for (i = 0; i < 3; i++) {
		struct stat st;

		if (fstat(i, &st) == -1 && open("/dev/null", O_RDWR) == -1)
			kbd_error(EXIT_FAILURE, errno, "open(/dev/null)");
	}

	if ((consfd = getfd(NULL)) < 0)
		kbd_error(2, 0, _("Couldn't get a file descriptor referring to the console."));

	if (ioctl(consfd, VT_GETSTATE, &vtstat) < 0)
		kbd_error(4, errno, "ioctl(VT_GETSTATE)");

	if (vtno == -1) {
		if (ioctl(consfd, VT_OPENQRY, &vtno) < 0 || vtno == -1)
			kbd_error(3, errno, _("Cannot find a free vt"));

	} else if (!force) {
		if (vtno >= 16)
			kbd_error(7, 0, _("Cannot check whether vt %d is free; use `%s -f' to force."),
			          vtno, get_progname());

		if (vtstat.v_state & (1 << vtno))
			kbd_error(7, 0, _("vt %d is in use; command aborted; use `%s -f' to force."),
			          vtno, get_progname());
	}

	if (as_user)
		username = authenticate_user(vtstat.v_active);
	else {
		if (!(argc > optind)) {
			def_cmd = getenv("SHELL");
			if (def_cmd == NULL)
				kbd_error(7, 0, _("Unable to find command."));
			cmd = malloc(strlen(def_cmd) + 2);
		} else {
			cmd = malloc(strlen(argv[optind]) + 2);
		}

		if (!cmd)
			kbd_error(EX_OSERR, errno, "malloc");

		if (login)
			strcpy(cmd, "-");
		else
			cmd[0] = '\0';

		if (def_cmd)
			strcat(cmd, def_cmd);
		else
			strcat(cmd, argv[optind]);

		if (login)
			argv[optind] = cmd++;
	}

	if (direct_exec || ((pid = fork()) == 0)) {
		/* leave current vt */
		if (!direct_exec) {
#ifdef ESIX_5_3_2_D
#ifdef HAVE_SETPGRP
			if (setpgrp() < 0)
#else
			if (1)
#endif /* HAVE_SETPGRP */
#else
			if (setsid() < 0)
#endif /* ESIX_5_3_2_D */
				kbd_error(5, errno, _("Unable to set new session"));
		}

		snprintf(vtname, PATH_MAX, VTNAME, vtno);

		/* Can we open the vt we want? */
		if ((fd = open_vt(vtname, force)) == -1) {
			int errsv = errno;
			if (!optc) {
				/* We found vtno ourselves - it is free according
				   to the kernel, but we cannot open it. Maybe X
				   used it and did a chown.  Try a few vt's more
				   before giving up. Note: the 16 is a kernel limitation. */
				for (i = vtno + 1; i < 16; i++) {
					if ((vtstat.v_state & (1 << i)) == 0) {
						snprintf(vtname, PATH_MAX, VTNAME, i);
						if ((fd = open_vt(vtname, force)) >= 0) {
							vtno = i;
							goto got_vtno;
						}
					}
				}
				snprintf(vtname, PATH_MAX, VTNAME, vtno);
			}
			errno = errsv;
			kbd_error(5, 0, _("Unable to open file: %s: %m"), vtname);
		}
	got_vtno:
		if (verbose)
			kbd_warning(0, _("Using VT %s"), vtname);

		/* Maybe we are suid root, and the -c option was given.
		   Check that the real user can access this VT.
		   We assume getty has made any in use VT non accessable */
		if (access(vtname, R_OK | W_OK) < 0)
			kbd_error(5, errno, _("Cannot open %s read/write"), vtname);

		if (!as_user && !geteuid()) {
			uid_t uid = getuid();
			if (chown(vtname, uid, getgid()) == -1)
				kbd_error(5, errno, "chown");
			if (setuid(uid) < 0)
				kbd_error(5, errno, "setuid");
		}

		if (show && change_vt(fd, vtno) < 0)
			exit(1);

		close(0);
		close(1);
		close(2);
		close(consfd);

		if ((dup2(fd, 0) == -1) || (dup2(fd, 1) == -1) || (dup2(fd, 2) == -1))
			kbd_error(1, errno, "dup");

		/* slight problem: after "openvt -su" has finished, the
		   utmp entry is not removed */
		if (as_user)
			execlp("login", "login", "-f", username, NULL);
		else if (def_cmd)
			execlp(cmd, def_cmd, NULL);
		else
			execvp(cmd, &argv[optind]);

		kbd_error(127, errno, "exec");
	}

	if (pid < 0)
		kbd_error(6, errno, "fork");

	if (do_wait) {
		int retval = 0; /* actual value returned form process */

		wait(NULL);
		waitpid(pid, &retval, 0);

		if (show) { /* Switch back... */
			if (change_vt(consfd, vtstat.v_active) < 0)
				exit(8);

			if (ioctl(consfd, VT_DISALLOCATE, vtno))
				kbd_error(8, 0, _("Couldn't deallocate console %d"), vtno);
		}

		/* if all our stuff went right, we want to return the exit code of the command we ran
		   super vital for scripting loops etc */
		return (retval);
	}

	return EXIT_SUCCESS;
}
