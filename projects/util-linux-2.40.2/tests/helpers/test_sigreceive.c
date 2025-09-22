/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_sigreceive - wait for signal and exit with value of it
 *
 * Written by Sami Kerola <kerolasa@iki.fi>
 */

#include <err.h>
#include <getopt.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "strutils.h"

#define TEST_SIGRECEIVE_FAILURE 0

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs("Usage: test_sigreceive [-s|--setuid <login|uid>]\n", out);
	exit(TEST_SIGRECEIVE_FAILURE);
}

static __attribute__((__noreturn__))
void exiter(int signo __attribute__((__unused__)),
	    siginfo_t *info,
	    void *context __attribute__((__unused__)))
{
	int ret = info->si_signo;

	if (info->si_code == SI_QUEUE && info->si_value.sival_int != 0)
		ret = info->si_value.sival_int;
	_exit(ret);
}

int main(int argc, char **argv)
{
	struct sigaction sigact;
	fd_set rfds;
	struct timeval timeout;
	char *user = NULL;
	int c;

	static const struct option longopts[] = {
		{"setuid", required_argument, NULL, 's'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "s:h", longopts, NULL)) != -1)
		switch (c) {
		case 's':
			user = optarg;
			break;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (user) {
		struct passwd *pw;
		uid_t uid;

		pw = getpwnam(user);
		if (pw)
			uid = pw->pw_uid;
		else
			uid = strtou32_or_err(user, "failed to parse uid");
		if (setuid(uid) < 0)
			err(TEST_SIGRECEIVE_FAILURE, "setuid failed");
	}

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = exiter;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGILL, &sigact, NULL);
#ifdef SIGTRAP
	sigaction(SIGTRAP, &sigact, NULL);
#endif
	sigaction(SIGABRT, &sigact, NULL);
#ifdef SIGIOT
	sigaction(SIGIOT, &sigact, NULL);
#endif
#ifdef SIGEMT
	sigaction(SIGEMT, &sigact, NULL);
#endif
#ifdef SIGBUS
	sigaction(SIGBUS, &sigact, NULL);
#endif
	sigaction(SIGFPE, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL);
	sigaction(SIGUSR2, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
	sigaction(SIGALRM, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
#ifdef SIGSTKFLT
	sigaction(SIGSTKFLT, &sigact, NULL);
#endif
	sigaction(SIGCHLD, &sigact, NULL);
#ifdef SIGCLD
	sigaction(SIGCLD, &sigact, NULL);
#endif
	sigaction(SIGCONT, &sigact, NULL);
	sigaction(SIGTSTP, &sigact, NULL);
	sigaction(SIGTTIN, &sigact, NULL);
	sigaction(SIGTTOU, &sigact, NULL);
#ifdef SIGURG
	sigaction(SIGURG, &sigact, NULL);
#endif
#ifdef SIGXCPU
	sigaction(SIGXCPU, &sigact, NULL);
#endif
#ifdef SIGXFSZ
	sigaction(SIGXFSZ, &sigact, NULL);
#endif
#ifdef SIGVTALRM
	sigaction(SIGVTALRM, &sigact, NULL);
#endif
#ifdef SIGPROF
	sigaction(SIGPROF, &sigact, NULL);
#endif
#ifdef SIGWINCH
	sigaction(SIGWINCH, &sigact, NULL);
#endif
#ifdef SIGIO
	sigaction(SIGIO, &sigact, NULL);
#endif
#ifdef SIGPOLL
	sigaction(SIGPOLL, &sigact, NULL);
#endif
#ifdef SIGINFO
	sigaction(SIGINFO, &sigact, NULL);
#endif
#ifdef SIGLOST
	sigaction(SIGLOST, &sigact, NULL);
#endif
#ifdef SIGPWR
	sigaction(SIGPWR, &sigact, NULL);
#endif
#ifdef SIGUNUSED
	sigaction(SIGUNUSED, &sigact, NULL);
#endif
#ifdef SIGSYS
	sigaction(SIGSYS, &sigact, NULL);
#endif
#ifdef SIGRTMIN
	sigaction(SIGRTMIN, &sigact, NULL);
	sigaction(SIGRTMAX, &sigact, NULL);
#endif
	/* Keep SIGHUP last, the bit it flips tells to check script the
	 * helper is ready to be killed.  */
	sigaction(SIGHUP, &sigact, NULL);

	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	select(0, &rfds, NULL, NULL, &timeout);

	exit(TEST_SIGRECEIVE_FAILURE);
}
