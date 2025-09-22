/*
 * taskset.c - set or retrieve a task's CPU affinity
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2004 Robert Love
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sched.h>
#include <stddef.h>
#include <string.h>

#include "cpuset.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"
#include "procfs.h"
#include "c.h"
#include "closestream.h"

#ifndef PF_NO_SETAFFINITY
# define PF_NO_SETAFFINITY 0x04000000
#endif

struct taskset {
	pid_t		pid;		/* task PID */
	cpu_set_t	*set;		/* task CPU mask */
	size_t		setsize;
	char		*buf;		/* buffer for conversion from mask to string */
	size_t		buflen;
	unsigned int	use_list:1,	/* use list rather than masks */
			get_only:1;	/* print the mask, but not modify */
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fprintf(out,
		_("Usage: %s [options] [mask | cpu-list] [pid|cmd [args...]]\n\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show or change the CPU affinity of a process.\n"), out);
	fputs(USAGE_SEPARATOR, out);

	fprintf(out, _(
		"Options:\n"
		" -a, --all-tasks         operate on all the tasks (threads) for a given pid\n"
		" -p, --pid               operate on existing given pid\n"
		" -c, --cpu-list          display and specify cpus in list format\n"
		));
	fprintf(out, USAGE_HELP_OPTIONS(25));

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, _(
		"The default behavior is to run a new command:\n"
		"    %1$s 03 sshd -b 1024\n"
		"You can retrieve the mask of an existing task:\n"
		"    %1$s -p 700\n"
		"Or set it:\n"
		"    %1$s -p 03 700\n"
		"List format uses a comma-separated list instead of a mask:\n"
		"    %1$s -pc 0,3,7-11 700\n"
		"Ranges in list format can take a stride argument:\n"
		"    e.g. 0-31:2 is equivalent to mask 0x55555555\n"),
		program_invocation_short_name);

	fprintf(out, USAGE_MAN_TAIL("taskset(1)"));
	exit(EXIT_SUCCESS);
}

static void print_affinity(struct taskset *ts, int isnew)
{
	char *str, *msg;

	if (ts->use_list) {
		str = cpulist_create(ts->buf, ts->buflen, ts->set, ts->setsize);
		msg = isnew ? _("pid %d's new affinity list: %s\n") :
			      _("pid %d's current affinity list: %s\n");
	} else {
		str = cpumask_create(ts->buf, ts->buflen, ts->set, ts->setsize);
		msg = isnew ? _("pid %d's new affinity mask: %s\n") :
			      _("pid %d's current affinity mask: %s\n");
	}

	if (!str)
		errx(EXIT_FAILURE, _("internal error: conversion from cpuset to string failed"));

	printf(msg, ts->pid ? ts->pid : getpid(), str);
}

static void __attribute__((__noreturn__)) err_affinity(pid_t pid, int set)
{
	char *msg;

	msg = set ? _("failed to set pid %d's affinity") :
		    _("failed to get pid %d's affinity");

	err(EXIT_FAILURE, msg, pid ? pid : getpid());
}


static void do_taskset(struct taskset *ts, size_t setsize, cpu_set_t *set)
{
	/* read the current mask */
	if (ts->pid) {
		if (sched_getaffinity(ts->pid, ts->setsize, ts->set) < 0)
			err_affinity(ts->pid, 0);
		print_affinity(ts, FALSE);
	}

	if (ts->get_only)
		return;

	/* set new mask */
	if (sched_setaffinity(ts->pid, setsize, set) < 0) {
		uintmax_t flags = 0;
		struct path_cxt *pc;
		int errsv = errno;

		if (errno != EPERM
		    && (pc = ul_new_procfs_path(ts->pid, NULL))
		    && procfs_process_get_stat_nth(pc, 9, &flags) == 0
		    && (flags & PF_NO_SETAFFINITY)) {
			warnx(_("affinity cannot be set due to PF_NO_SETAFFINITY flag set"));
			errno = EINVAL;
		} else
			errno = errsv;

		err_affinity(ts->pid, 1);
	}

	/* re-read the current mask */
	if (ts->pid) {
		if (sched_getaffinity(ts->pid, ts->setsize, ts->set) < 0)
			err_affinity(ts->pid, 0);
		print_affinity(ts, TRUE);
	}
}

int main(int argc, char **argv)
{
	cpu_set_t *new_set;
	pid_t pid = 0;
	int c, all_tasks = 0;
	int ncpus;
	size_t new_setsize, nbits;
	struct taskset ts;

	static const struct option longopts[] = {
		{ "all-tasks",	0, NULL, 'a' },
		{ "pid",	0, NULL, 'p' },
		{ "cpu-list",	0, NULL, 'c' },
		{ "help",	0, NULL, 'h' },
		{ "version",	0, NULL, 'V' },
		{ NULL,		0, NULL,  0  }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	memset(&ts, 0, sizeof(ts));

	while ((c = getopt_long(argc, argv, "+apchV", longopts, NULL)) != -1) {
		switch (c) {
		case 'a':
			all_tasks = 1;
			break;
		case 'p':
			pid = strtos32_or_err(argv[argc - 1],
					    _("invalid PID argument"));
			break;
		case 'c':
			ts.use_list = 1;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if ((!pid && argc - optind < 2)
	    || (pid && (argc - optind < 1 || argc - optind > 2))) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	ncpus = get_max_number_of_cpus();
	if (ncpus <= 0)
		errx(EXIT_FAILURE, _("cannot determine NR_CPUS; aborting"));

	/*
	 * the ts->set is always used for the sched_getaffinity call
	 * On the sched_getaffinity the kernel demands a user mask of
	 * at least the size of its own cpumask_t.
	 */
	ts.set = cpuset_alloc(ncpus, &ts.setsize, &nbits);
	if (!ts.set)
		err(EXIT_FAILURE, _("cpuset_alloc failed"));

	/* buffer for conversion from mask to string */
	ts.buflen = 7 * nbits;
	ts.buf = xmalloc(ts.buflen);

	/*
	 * new_set is always used for the sched_setaffinity call
	 * On the sched_setaffinity the kernel will zero-fill its
	 * cpumask_t if the user's mask is shorter.
	 */
	new_set = cpuset_alloc(ncpus, &new_setsize, NULL);
	if (!new_set)
		err(EXIT_FAILURE, _("cpuset_alloc failed"));

	if (argc - optind == 1)
		ts.get_only = 1;

	else if (ts.use_list) {
		if (cpulist_parse(argv[optind], new_set, new_setsize, 0))
			errx(EXIT_FAILURE, _("failed to parse CPU list: %s"),
			     argv[optind]);
	} else if (cpumask_parse(argv[optind], new_set, new_setsize)) {
		errx(EXIT_FAILURE, _("failed to parse CPU mask: %s"),
		     argv[optind]);
	}

	if (all_tasks && pid) {
		DIR *sub = NULL;
		struct path_cxt *pc = ul_new_procfs_path(pid, NULL);

		while (pc && procfs_process_next_tid(pc, &sub, &ts.pid) == 0)
			do_taskset(&ts, new_setsize, new_set);

		ul_unref_path(pc);
	} else {
		ts.pid = pid;
		do_taskset(&ts, new_setsize, new_set);
	}

	free(ts.buf);
	cpuset_free(ts.set);
	cpuset_free(new_set);

	if (!pid) {
		argv += optind + 1;
		execvp(argv[0], argv);
		errexec(argv[0]);
	}

	return EXIT_SUCCESS;
}
