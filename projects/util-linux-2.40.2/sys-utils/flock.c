/*   SPDX-License-Identifier: MIT
 *
 *   Copyright 2003-2005 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "closestream.h"
#include "monotonic.h"
#include "timer.h"

static void __attribute__((__noreturn__)) usage(void)
{
	fputs(USAGE_HEADER, stdout);
	fprintf(stdout,
		_(" %1$s [options] <file>|<directory> <command> [<argument>...]\n"
		  " %1$s [options] <file>|<directory> -c <command>\n"
		  " %1$s [options] <file descriptor number>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Manage file locks from shell scripts.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(  " -s, --shared             get a shared lock\n"), stdout);
	fputs(_(  " -x, --exclusive          get an exclusive lock (default)\n"), stdout);
	fputs(_(  " -u, --unlock             remove a lock\n"), stdout);
	fputs(_(  " -n, --nonblock           fail rather than wait\n"), stdout);
	fputs(_(  " -w, --timeout <secs>     wait for a limited amount of time\n"), stdout);
	fputs(_(  " -E, --conflict-exit-code <number>  exit code after conflict or timeout\n"), stdout);
	fputs(_(  " -o, --close              close file descriptor before running command\n"), stdout);
	fputs(_(  " -c, --command <command>  run a single command string through the shell\n"), stdout);
	fputs(_(  " -F, --no-fork            execute command without forking\n"), stdout);
	fputs(_(  "     --verbose            increase verbosity\n"), stdout);
	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(26));
	fprintf(stdout, USAGE_MAN_TAIL("flock(1)"));
	exit(EXIT_SUCCESS);
}

static volatile sig_atomic_t timeout_expired = 0;

static void timeout_handler(int sig __attribute__((__unused__)),
			    siginfo_t *info,
			    void *context __attribute__((__unused__)))
{
#ifdef HAVE_TIMER_CREATE
	if (info->si_code == SI_TIMER)
#endif
		timeout_expired = 1;
}

static int open_file(const char *filename, int *flags)
{

	int fd;
	int fl = *flags == 0 ? O_RDONLY : *flags;

	errno = 0;
	fl |= O_NOCTTY | O_CREAT;
	fd = open(filename, fl, 0666);

	/* Linux doesn't like O_CREAT on a directory, even though it
	 * should be a no-op; POSIX doesn't allow O_RDWR or O_WRONLY
	 */
	if (fd < 0 && errno == EISDIR) {
		fl = O_RDONLY | O_NOCTTY;
		fd = open(filename, fl);
	}
	if (fd < 0) {
		warn(_("cannot open lock file %s"), filename);
		if (errno == ENOMEM || errno == EMFILE || errno == ENFILE)
			exit(EX_OSERR);
		if (errno == EROFS || errno == ENOSPC)
			exit(EX_CANTCREAT);
		exit(EX_NOINPUT);
	}
	*flags = fl;
	return fd;
}

static void __attribute__((__noreturn__)) run_program(char **cmd_argv)
{
	execvp(cmd_argv[0], cmd_argv);

	warn(_("failed to execute %s"), cmd_argv[0]);
	_exit((errno == ENOMEM) ? EX_OSERR : EX_UNAVAILABLE);
}

int main(int argc, char *argv[])
{
	struct ul_timer timer;
	struct itimerval timeout;
	int have_timeout = 0;
	int type = LOCK_EX;
	int block = 0;
	int open_flags = 0;
	int fd = -1;
	int opt, ix;
	int do_close = 0;
	int no_fork = 0;
	int status;
	int verbose = 0;
	struct timeval time_start = { 0 }, time_done = { 0 };
	/*
	 * The default exit code for lock conflict or timeout
	 * is specified in man flock.1
	 */
	int conflict_exit_code = 1;
	char **cmd_argv = NULL, *sh_c_argv[4];
	const char *filename = NULL;
	enum {
		OPT_VERBOSE = CHAR_MAX + 1
	};
	static const struct option long_options[] = {
		{"shared", no_argument, NULL, 's'},
		{"exclusive", no_argument, NULL, 'x'},
		{"unlock", no_argument, NULL, 'u'},
		{"nonblocking", no_argument, NULL, 'n'},
		{"nb", no_argument, NULL, 'n'},
		{"timeout", required_argument, NULL, 'w'},
		{"wait", required_argument, NULL, 'w'},
		{"conflict-exit-code", required_argument, NULL, 'E'},
		{"close", no_argument, NULL, 'o'},
		{"no-fork", no_argument, NULL, 'F'},
		{"verbose", no_argument, NULL, OPT_VERBOSE},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(EX_USAGE);

	if (argc < 2) {
		warnx(_("not enough arguments"));
		errtryhelp(EX_USAGE);
	}

	memset(&timeout, 0, sizeof timeout);

	optopt = 0;
	while ((opt =
		getopt_long(argc, argv, "+sexnoFuw:E:hV?", long_options,
			    &ix)) != EOF) {
		switch (opt) {
		case 's':
			type = LOCK_SH;
			break;
		case 'e':
		case 'x':
			type = LOCK_EX;
			break;
		case 'u':
			type = LOCK_UN;
			break;
		case 'o':
			do_close = 1;
			break;
		case 'F':
			no_fork = 1;
			break;
		case 'n':
			block = LOCK_NB;
			break;
		case 'w':
			have_timeout = 1;
			strtotimeval_or_err(optarg, &timeout.it_value,
				_("invalid timeout value"));
			break;
		case 'E':
			conflict_exit_code = strtos32_or_err(optarg,
				_("invalid exit code"));
			if (conflict_exit_code < 0 || conflict_exit_code > 255)
				errx(EX_USAGE, _("exit code out of range (expected 0 to 255)"));
			break;
		case OPT_VERBOSE:
			verbose = 1;
			break;

		case 'V':
			print_version(EX_OK);
		case 'h':
			usage();
		default:
			errtryhelp(EX_USAGE);
		}
	}

	if (no_fork && do_close)
		errx(EX_USAGE,
			_("the --no-fork and --close options are incompatible"));

	if (argc > optind + 1) {
		/* Run command */
		if (!strcmp(argv[optind + 1], "-c") ||
		    !strcmp(argv[optind + 1], "--command")) {
			if (argc != optind + 3)
				errx(EX_USAGE,
				     _("%s requires exactly one command argument"),
				     argv[optind + 1]);
			cmd_argv = sh_c_argv;
			cmd_argv[0] = getenv("SHELL");
			if (!cmd_argv[0] || !*cmd_argv[0])
				cmd_argv[0] = _PATH_BSHELL;
			cmd_argv[1] = "-c";
			cmd_argv[2] = argv[optind + 2];
			cmd_argv[3] = NULL;
		} else {
			cmd_argv = &argv[optind + 1];
		}

		filename = argv[optind];
		fd = open_file(filename, &open_flags);

	} else if (optind < argc) {
		/* Use provided file descriptor */
		fd = strtos32_or_err(argv[optind], _("bad file descriptor"));
	} else {
		/* Bad options */
		errx(EX_USAGE, _("requires file descriptor, file or directory"));
	}

	if (have_timeout) {
		if (timeout.it_value.tv_sec == 0 &&
		    timeout.it_value.tv_usec == 0) {
			/* -w 0 is equivalent to -n; this has to be
			 * special-cased because setting an itimer to zero
			 * means disabled!
			 */
			have_timeout = 0;
			block = LOCK_NB;
		} else
			if (setup_timer(&timer, &timeout, &timeout_handler))
				err(EX_OSERR, _("cannot set up timer"));
	}

	if (verbose)
		gettime_monotonic(&time_start);
	while (flock(fd, type | block)) {
		switch (errno) {
		case EWOULDBLOCK:
			/* -n option set and failed to lock. */
			if (verbose)
				warnx(_("failed to get lock"));
			exit(conflict_exit_code);
		case EINTR:
			/* Signal received */
			if (timeout_expired) {
				/* -w option set and failed to lock. */
				if (verbose)
					warnx(_("timeout while waiting to get lock"));
				exit(conflict_exit_code);
			}
			/* otherwise try again */
			continue;
		case EIO:
		case EBADF:		/* since Linux 3.4 (commit 55725513) */
			/* Probably NFSv4 where flock() is emulated by fcntl().
			 * Let's try to reopen in read-write mode.
			 */
			if (!(open_flags & O_RDWR) &&
			    type != LOCK_SH &&
			    filename &&
			    access(filename, R_OK | W_OK) == 0) {

				close(fd);
				open_flags = O_RDWR;
				fd = open_file(filename, &open_flags);

				if (open_flags & O_RDWR)
					break;
			}
			/* fallthrough */
		default:
			/* Other errors */
			if (filename)
				warn("%s", filename);
			else
				warn("%d", fd);
			exit((errno == ENOLCK
			      || errno == ENOMEM) ? EX_OSERR : EX_DATAERR);
		}
	}

	if (have_timeout)
		cancel_timer(&timer);
	if (verbose) {
		struct timeval delta = { 0 };

		gettime_monotonic(&time_done);
		timersub(&time_done, &time_start, &delta);
		printf(_("%s: getting lock took %"PRId64".%06"PRId64" seconds\n"),
		       program_invocation_short_name,
		       (int64_t) delta.tv_sec,
		       (int64_t) delta.tv_usec);
	}
	status = EX_OK;

	if (cmd_argv) {
		pid_t w, f;
		/* Clear any inherited settings */
		signal(SIGCHLD, SIG_DFL);
		if (verbose)
			printf(_("%s: executing %s\n"), program_invocation_short_name, cmd_argv[0]);

		if (!no_fork) {
			f = fork();
			if (f < 0)
				err(EX_OSERR, _("fork failed"));

			/* child */
			else if (f == 0) {
				if (do_close)
					close(fd);
				run_program(cmd_argv);

			/* parent */
			} else {
				do {
					w = waitpid(f, &status, 0);
					if (w == -1 && errno != EINTR)
						break;
				} while (w != f);

				if (w == -1) {
					status = EXIT_FAILURE;
					warn(_("waitpid failed"));
				} else if (WIFEXITED(status))
					status = WEXITSTATUS(status);
				else if (WIFSIGNALED(status))
					status = WTERMSIG(status) + 128;
				else
					/* WTF? */
					status = EX_OSERR;
			}

		} else
			/* no-fork execution */
			run_program(cmd_argv);
	}

	return status;
}
