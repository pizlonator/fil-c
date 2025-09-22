/*
 * waitpid(1) - wait for process termination
 *
 * Copyright (C) 2022 Thomas Weißschuh <thomas@t-8ch.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include "pidfd-utils.h"
#include "c.h"
#include "nls.h"
#include "xalloc.h"
#include "strutils.h"
#include "exitcodes.h"
#include "timeutils.h"
#include "optutils.h"

#define EXIT_TIMEOUT_EXPIRED 3

#define TIMEOUT_SOCKET_IDX UINT64_MAX

static bool verbose = false;
static struct timespec timeout;
static bool allow_exited = false;
static size_t count;

static pid_t *parse_pids(size_t n_strings, char * const *strings)
{
	pid_t *pids = xcalloc(n_strings, sizeof(*pids));

	for (size_t i = 0; i < n_strings; i++)
		pids[i] = strtopid_or_err(strings[i], _("failed to parse pid"));

	return pids;
}

static int *open_pidfds(size_t n_pids, pid_t *pids)
{
	int *pidfds = xcalloc(n_pids, sizeof(*pidfds));

	for (size_t i = 0; i < n_pids; i++) {
		pidfds[i] = pidfd_open(pids[i], 0);
		if (pidfds[i] == -1) {
			if (allow_exited && errno == ESRCH) {
				if (verbose)
					warnx(_("PID %d has exited, skipping"), pids[i]);
				continue;
			}
			err_nosys(EXIT_FAILURE, _("could not open pid %u"), pids[i]);
		}
	}

	return pidfds;
}

static int open_timeoutfd(void)
{
	int fd;
	struct itimerspec timer = {};

	if (!timeout.tv_sec && !timeout.tv_nsec)
		return -1;

	timer.it_value = timeout;

	fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (fd == -1)
		err_nosys(EXIT_FAILURE, _("could not create timerfd"));

	if (timerfd_settime(fd, 0, &timer, NULL))
		err_nosys(EXIT_FAILURE, _("could not set timer"));

	return fd;

}

static size_t add_listeners(int epll, size_t n_pids, int * const pidfds, int timeoutfd)
{
	size_t skipped = 0;
	struct epoll_event evt = {
		.events = EPOLLIN,
	};

	if (timeoutfd != -1) {
		evt.data.u64 = TIMEOUT_SOCKET_IDX;
		if (epoll_ctl(epll, EPOLL_CTL_ADD, timeoutfd, &evt))
			err_nosys(EXIT_FAILURE, _("could not add timerfd"));
	}

	for (size_t i = 0; i < n_pids; i++) {
		if (pidfds[i] == -1) {
			skipped++;
			continue;
		}
		evt.data.u64 = i;
		if (epoll_ctl(epll, EPOLL_CTL_ADD, pidfds[i], &evt))
			err_nosys(EXIT_FAILURE, _("could not add listener"));
	}

	return n_pids - skipped;
}

static void wait_for_exits(int epll, size_t active_pids, pid_t * const pids,
			   int * const pidfds)
{
	while (active_pids) {
		struct epoll_event evt;
		int ret, fd;

		ret = epoll_wait(epll, &evt, 1, -1);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			else
				err_nosys(EXIT_FAILURE, _("failure during wait"));
		}
		if (evt.data.u64 == TIMEOUT_SOCKET_IDX) {
			if (verbose)
				printf(_("Timeout expired\n"));
			exit(EXIT_TIMEOUT_EXPIRED);
		}
		if (verbose)
			printf(_("PID %d finished\n"), pids[evt.data.u64]);
		assert((size_t) ret <= active_pids);
		fd = pidfds[evt.data.u64];
		epoll_ctl(epll, EPOLL_CTL_DEL, fd, NULL);
		active_pids -= ret;
	}
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] pid...\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -v, --verbose           be more verbose\n"), out);
	fputs(_(" -t, --timeout=<timeout> wait at most timeout seconds\n"), out);
	fputs(_(" -e, --exited            allow exited PIDs\n"), out);
	fputs(_(" -c, --count=<count>     number of process exits to wait for\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(25));

	fprintf(out, USAGE_MAN_TAIL("waitpid(1)"));

	exit(EXIT_SUCCESS);
}

static int parse_options(int argc, char **argv)
{
	int c;
	static const struct option longopts[] = {
		{ "verbose", no_argument,       NULL, 'v' },
		{ "timeout", required_argument, NULL, 't' },
		{ "exited",  no_argument,       NULL, 'e' },
		{ "count",   required_argument, NULL, 'c' },
		{ "version", no_argument,       NULL, 'V' },
		{ "help",    no_argument,       NULL, 'h' },
		{ 0 }
	};
	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'c', 'e' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	while ((c = getopt_long (argc, argv, "vVht:c:e", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'v':
			verbose = true;
			break;
		case 't':
			strtotimespec_or_err(optarg, &timeout,
					     _("Could not parse timeout"));
			break;
		case 'e':
			allow_exited = true;
			break;
		case 'c':
			count = str2num_or_err(optarg, 10, _("Invalid count"),
					       1, INT64_MAX);
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	return optind;
}

int main(int argc, char **argv)
{
	int pid_idx, epoll, timeoutfd, *pidfds;
	size_t n_pids, active_pids;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	pid_idx = parse_options(argc, argv);
	n_pids = argc - pid_idx;
	if (!n_pids)
		errx(EXIT_FAILURE, _("no PIDs specified"));

	if (count && count > n_pids)
		errx(EXIT_FAILURE,
		     _("can't wait for %zu of %zu PIDs"), count, n_pids);

	pid_t *pids = parse_pids(argc - pid_idx, argv + pid_idx);

	pidfds = open_pidfds(n_pids, pids);
	timeoutfd = open_timeoutfd();
	epoll = epoll_create(n_pids);
	if (epoll == -1)
		err_nosys(EXIT_FAILURE, _("could not create epoll"));

	active_pids = add_listeners(epoll, n_pids, pidfds, timeoutfd);
	if (count)
		active_pids = min(active_pids, count);
	wait_for_exits(epoll, active_pids, pids, pidfds);
}
