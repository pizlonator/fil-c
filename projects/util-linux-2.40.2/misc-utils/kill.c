/*
 * Copyright (c) 1988, 1993, 1994
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
 */
/*
 *  oct 5 1994 -- almost entirely re-written to allow for process names.
 *  modifications (c) salvatore valente <svalente@mit.edu>
 *  may be used / modified / distributed under the same terms as the original.
 *
 *  1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *  - added Native Language Support
 *
 *  1999-11-13 aeb Accept signal numbers 128+s.
 *
 * Copyright (C) 2014 Sami Kerola <kerolasa@iki.fi>
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 */

#include <ctype.h>		/* for isdigit() */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "pidfd-utils.h"
#include "procfs.h"
#include "pathnames.h"
#include "signames.h"
#include "strutils.h"
#include "ttyutils.h"
#include "xalloc.h"
#include "fileutils.h"

/* partial success, otherwise we return regular EXIT_{SUCCESS,FAILURE} */
#define KILL_EXIT_SOMEOK	64

enum {
	KILL_FIELD_WIDTH = 11,
	KILL_OUTPUT_WIDTH = 72
};

#ifdef UL_HAVE_PIDFD
# include <poll.h>
# include "list.h"
struct timeouts {
	int period;
	int sig;
	struct list_head follow_ups;
};
#endif

struct kill_control {
	char *arg;
	pid_t pid;
	int numsig;
#ifdef HAVE_SIGQUEUE
	union sigval sigdata;
#endif
#ifdef UL_HAVE_PIDFD
	struct list_head follow_ups;
#endif
	unsigned int
		check_all:1,
		do_kill:1,
		do_pid:1,
		require_handler:1,
		use_sigval:1,
#ifdef UL_HAVE_PIDFD
		timeout:1,
#endif
		verbose:1;
};

static void print_signal_name(int signum)
{
	const char *name = signum_to_signame(signum);

	if (name) {
		printf("%s\n", name);
		return;
	}
#ifdef SIGRTMIN
	if (SIGRTMIN <= signum && signum <= SIGRTMAX) {
		printf("RT%d\n", signum - SIGRTMIN);
		return;
	}
#endif
	printf("%d\n", signum);
}

static void pretty_print_signal(FILE *fp, size_t term_width, size_t *lpos,
				int signum, const char *name)
{
	if (term_width < (*lpos + KILL_FIELD_WIDTH)) {
		fputc('\n', fp);
		*lpos = 0;
	}
	*lpos += KILL_FIELD_WIDTH;
	fprintf(fp, "%2d %-8s", signum, name);
}

static void print_all_signals(FILE *fp, int pretty)
{
	size_t n, lth, lpos = 0, width;
	const char *signame = NULL;
	int signum = 0;

	if (!pretty) {
		for (n = 0; get_signame_by_idx(n, &signame, NULL) == 0; n++) {
			lth = 1 + strlen(signame);
			if (KILL_OUTPUT_WIDTH < lpos + lth) {
				fputc('\n', fp);
				lpos = 0;
			} else if (lpos)
				fputc(' ', fp);
			lpos += lth;
			fputs(signame, fp);
		}
#ifdef SIGRTMIN
		fputs(" RT<N> RTMIN+<N> RTMAX-<N>", fp);
#endif
		fputc('\n', fp);
		return;
	}

	/* pretty print */
	width = get_terminal_width(KILL_OUTPUT_WIDTH + 1) - 1;
	for (n = 0; get_signame_by_idx(n, &signame, &signum) == 0; n++)
		pretty_print_signal(fp, width, &lpos, signum, signame);
#ifdef SIGRTMIN
	pretty_print_signal(fp, width, &lpos, SIGRTMIN, "RTMIN");
	pretty_print_signal(fp, width, &lpos, SIGRTMAX, "RTMAX");
#endif
	fputc('\n', fp);
}

static void err_nosig(char *name)
{
	warnx(_("unknown signal %s; valid signals:"), name);
	print_all_signals(stderr, 1);
	exit(EXIT_FAILURE);
}

static int arg_to_signum(char *arg, int maskbit)
{
	int numsig;
	char *ep;

	if (isdigit(*arg)) {
		errno = 0;
		numsig = strtol(arg, &ep, 10);
		if (NSIG <= numsig && maskbit && (numsig & 128) != 0)
			numsig -= 128;
		if (errno || *ep != 0 || numsig < 0 || NSIG <= numsig)
			return -1;
		return numsig;
	}
	return signame_to_signum(arg);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <pid>|<name>...\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Forcibly terminate a process.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all              do not restrict the name-to-pid conversion to processes\n"
		"                          with the same uid as the present process\n"), out);
	fputs(_(" -s, --signal <signal>  send this <signal> instead of SIGTERM\n"), out);
#ifdef HAVE_SIGQUEUE
	fputs(_(" -q, --queue <value>    use sigqueue(2), not kill(2), and pass <value> as data\n"), out);
#endif
#ifdef UL_HAVE_PIDFD
	fputs(_("     --timeout <milliseconds> <follow-up signal>\n"
		"                        wait up to timeout and send follow-up signal\n"), out);
#endif
	fputs(_(" -p, --pid              print pids without signaling them\n"), out);
	fputs(_(" -l, --list[=<signal>]  list signal names, or convert a signal number to a name\n"), out);
	fputs(_(" -L, --table            list signal names and numbers\n"), out);
	fputs(_(" -r, --require-handler  do not send signal if signal handler is not present\n"), out);
	fputs(_("     --verbose          print pids that will be signaled\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(24));
	fprintf(out, USAGE_MAN_TAIL("kill(1)"));

	exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) print_kill_version(void)
{
	static const char *features[] = {
#ifdef HAVE_SIGQUEUE
		"sigqueue",
#endif
#ifdef UL_HAVE_PIDFD
		"pidfd",
#endif
	};

	printf(_("%s from %s"), program_invocation_short_name, PACKAGE_STRING);

	if (ARRAY_SIZE(features)) {
		size_t i;
		fputs(_(" (with: "), stdout);
		for (i = 0; i < ARRAY_SIZE(features); i++) {
			fputs(features[i], stdout);
			if (i + 1 < ARRAY_SIZE(features))
				fputs(", ", stdout);
		}
		fputs(")\n", stdout);
	}
	exit(EXIT_SUCCESS);
}

static char **parse_arguments(int argc, char **argv, struct kill_control *ctl)
{
	char *arg;

	/* Loop through the arguments.  Actually, -a is the only option
	 * can be used with other options.  The 'kill' is basically a
	 * one-option-at-most program. */
	for (argc--, argv++; 0 < argc; argc--, argv++) {
		arg = *argv;
		if (*arg != '-')
			break;
		if (!strcmp(arg, "--")) {
			argc--, argv++;
			break;
		}
		if (!strcmp(arg, "-v") || !strcmp(arg, "-V") ||
		    !strcmp(arg, "--version"))
			print_kill_version();
		if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
			usage();
		if (!strcmp(arg, "--verbose")) {
			ctl->verbose = 1;
			continue;
		}
		if (!strcmp(arg, "-a") || !strcmp(arg, "--all")) {
			ctl->check_all = 1;
			continue;
		}
		if (!strcmp(arg, "-l") || !strcmp(arg, "--list")) {
			if (argc < 2) {
				print_all_signals(stdout, 0);
				exit(EXIT_SUCCESS);
			}
			if (2 < argc)
				errx(EXIT_FAILURE, _("too many arguments"));
			/* argc == 2, accept "kill -l $?" */
			arg = argv[1];
			if ((ctl->numsig = arg_to_signum(arg, 1)) < 0)
				errx(EXIT_FAILURE, _("unknown signal: %s"),
				     arg);
			print_signal_name(ctl->numsig);
			exit(EXIT_SUCCESS);
		}
		/* for compatibility with procps kill(1) */
		if (!strncmp(arg, "--list=", 7) || !strncmp(arg, "-l=", 3)) {
			char *p = strchr(arg, '=') + 1;
			if ((ctl->numsig = arg_to_signum(p, 1)) < 0)
				errx(EXIT_FAILURE, _("unknown signal: %s"), p);
			print_signal_name(ctl->numsig);
			exit(EXIT_SUCCESS);
		}
		if (!strcmp(arg, "-L") || !strcmp(arg, "--table")) {
			print_all_signals(stdout, 1);
			exit(EXIT_SUCCESS);
		}
		if (!strcmp(arg, "-r") || !strcmp(arg, "--require-handler")) {
			ctl->require_handler = 1;
			continue;
		}
		if (!strcmp(arg, "-p") || !strcmp(arg, "--pid")) {
			ctl->do_pid = 1;
			if (ctl->do_kill)
				errx(EXIT_FAILURE, _("%s and %s are mutually exclusive"), "--pid", "--signal");
#ifdef HAVE_SIGQUEUE
			if (ctl->use_sigval)
				errx(EXIT_FAILURE, _("%s and %s are mutually exclusive"), "--pid", "--queue");
#endif
			continue;
		}
		if (!strcmp(arg, "-s") || !strcmp(arg, "--signal")) {
			if (argc < 2)
				errx(EXIT_FAILURE, _("not enough arguments"));
			ctl->do_kill = 1;
			if (ctl->do_pid)
				errx(EXIT_FAILURE, _("%s and %s are mutually exclusive"), "--pid", "--signal");
			argc--, argv++;
			arg = *argv;
			if ((ctl->numsig = arg_to_signum(arg, 0)) < 0)
				err_nosig(arg);
			continue;
		}
#ifdef HAVE_SIGQUEUE
		if (!strcmp(arg, "-q") || !strcmp(arg, "--queue")) {
			if (argc < 2)
				errx(EXIT_FAILURE, _("option '%s' requires an argument"), arg);
			if (ctl->do_pid)
				errx(EXIT_FAILURE, _("%s and %s are mutually exclusive"), "--pid", "--queue");
			argc--, argv++;
			arg = *argv;
			ctl->sigdata.sival_int = strtos32_or_err(arg, _("argument error"));
			ctl->use_sigval = 1;
			continue;
		}
#endif
#ifdef UL_HAVE_PIDFD
		if (!strcmp(arg, "--timeout")) {
			struct timeouts *next;

			ctl->timeout = 1;
			if (argc < 2)
				errx(EXIT_FAILURE, _("option '%s' requires an argument"), arg);
			argc--, argv++;
			arg = *argv;
			next = xcalloc(1, sizeof(*next));
			next->period = strtos32_or_err(arg, _("argument error"));
			INIT_LIST_HEAD(&next->follow_ups);
			argc--, argv++;
			arg = *argv;
			if ((next->sig = arg_to_signum(arg, 0)) < 0)
				err_nosig(arg);
			list_add_tail(&next->follow_ups, &ctl->follow_ups);
			continue;
		}
#endif
		/* 'arg' begins with a dash but is not a known option.
		 * So it's probably something like -HUP, or -1/-n try to
		 * deal with it.
		 *
		 * -n could be either signal n or pid -n (a process group
		 * number).  In case of doubt, POSIX tells us to assume a
		 * signal.  But if a signal has already been parsed, then
		 * assume it is a process group, so stop parsing options. */
		if (ctl->do_kill)
			break;
		arg++;
		if ((ctl->numsig = arg_to_signum(arg, 0)) < 0)
			errx(EXIT_FAILURE, _("invalid signal name or number: %s"), arg);
		ctl->do_kill = 1;
		if (ctl->do_pid)
			errx(EXIT_FAILURE, _("%s and %s are mutually exclusive"), "--pid", "--signal");
	}
	if (!*argv)
		errx(EXIT_FAILURE, _("not enough arguments"));
	return argv;
}

#ifdef UL_HAVE_PIDFD
static int kill_with_timeout(const struct kill_control *ctl)
{
	int pfd, n;
	struct pollfd p = { 0 };
	siginfo_t info = { 0 };
	struct list_head *entry;

	info.si_code = SI_QUEUE;
	info.si_signo = ctl->numsig;
	info.si_uid = getuid();
	info.si_pid = getpid();
	info.si_value.sival_int =
	    ctl->use_sigval != 0 ? ctl->use_sigval : ctl->numsig;

	if ((pfd = pidfd_open(ctl->pid, 0)) < 0)
		err(EXIT_FAILURE, _("pidfd_open() failed: %d"), ctl->pid);
	p.fd = pfd;
	p.events = POLLIN;

	if (pidfd_send_signal(pfd, ctl->numsig, &info, 0) < 0)
		err(EXIT_FAILURE, _("pidfd_send_signal() failed"));
	list_for_each(entry, &ctl->follow_ups) {
		struct timeouts *timeout;

		timeout = list_entry(entry, struct timeouts, follow_ups);
		n = poll(&p, 1, timeout->period);
		if (n < 0)
			err(EXIT_FAILURE, _("poll() failed"));
		if (n == 0) {
			info.si_signo = timeout->sig;
			if (ctl->verbose)
				printf(_("timeout, sending signal %d to pid %d\n"),
					 timeout->sig, ctl->pid);
			if (pidfd_send_signal(pfd, timeout->sig, &info, 0) < 0)
				err(EXIT_FAILURE, _("pidfd_send_signal() failed"));
		}
	}
	return 0;
}
#endif

static int kill_verbose(const struct kill_control *ctl)
{
	int rc = 0;

	if (ctl->verbose)
		printf(_("sending signal %d to pid %d\n"), ctl->numsig, ctl->pid);
	if (ctl->do_pid) {
		printf("%ld\n", (long) ctl->pid);
		return 0;
	}
#ifdef UL_HAVE_PIDFD
	if (ctl->timeout) {
		rc = kill_with_timeout(ctl);
	} else
#endif
#ifdef HAVE_SIGQUEUE
	if (ctl->use_sigval)
		rc = sigqueue(ctl->pid, ctl->numsig, ctl->sigdata);
	else
#endif
		rc = kill(ctl->pid, ctl->numsig);

	if (rc < 0)
		warn(_("sending signal to %s failed"), ctl->arg);
	return rc;
}

static int check_signal_handler(const struct kill_control *ctl)
{
	uintmax_t sigcgt = 0;
	int rc = 0, has_hnd = 0;
	struct path_cxt *pc;

	if (!ctl->require_handler)
		return 1;

	pc = ul_new_procfs_path(ctl->pid, NULL);
	if (!pc)
		return -ENOMEM;

	rc = procfs_process_get_stat_nth(pc, 34, &sigcgt);
	if (rc)
		return -EINVAL;

	ul_unref_path(pc);

	has_hnd = ((1UL << (ctl->numsig - 1)) & sigcgt) != 0;
	if (ctl->verbose && !has_hnd)
		printf(_("not signalling pid %d, it has no userspace handler for signal %d\n"), ctl->pid, ctl->numsig);

	return has_hnd;
}

int main(int argc, char **argv)
{
	struct kill_control ctl = { .numsig = SIGTERM };
	int nerrs = 0, ct = 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

#ifdef UL_HAVE_PIDFD
	INIT_LIST_HEAD(&ctl.follow_ups);
#endif
	argv = parse_arguments(argc, argv, &ctl);

	/* The rest of the arguments should be process ids and names. */
	for ( ; (ctl.arg = *argv) != NULL; argv++) {
		char *ep = NULL;

		errno = 0;
		ctl.pid = strtol(ctl.arg, &ep, 10);
		if (errno == 0 && ep && *ep == '\0' && ctl.arg < ep) {
			if (check_signal_handler(&ctl) <= 0)
				continue;
			if (kill_verbose(&ctl) != 0)
				nerrs++;
			ct++;
		} else {
			int found = 0;
			struct dirent *d;
			DIR *dir = opendir(_PATH_PROC);
			uid_t uid = !ctl.check_all ? getuid() : 0;

			if (!dir)
				continue;

			while ((d = xreaddir(dir))) {
				if (!ctl.check_all &&
				    !procfs_dirent_match_uid(dir, d, uid))
					continue;
				if (ctl.arg &&
				    !procfs_dirent_match_name(dir, d, ctl.arg))
					continue;
				if (procfs_dirent_get_pid(d, &ctl.pid) != 0)
					continue;
				if (check_signal_handler(&ctl) <= 0)
					continue;

				if (kill_verbose(&ctl) != 0)
					nerrs++;
				ct++;
				found = 1;
			}

			closedir(dir);
			if (!found) {
				nerrs++, ct++;
				warnx(_("cannot find process \"%s\""), ctl.arg);
			}
		}
	}

#ifdef UL_HAVE_PIDFD
	while (!list_empty(&ctl.follow_ups)) {
		struct timeouts *x = list_entry(ctl.follow_ups.next,
				                  struct timeouts, follow_ups);
		list_del(&x->follow_ups);
		free(x);
	}
#endif
	if (ct && nerrs == 0)
		return EXIT_SUCCESS;	/* full success */
	if (ct == nerrs)
		return EXIT_FAILURE;	/* all failed */

	return KILL_EXIT_SOMEOK;	/* partial success */
}
