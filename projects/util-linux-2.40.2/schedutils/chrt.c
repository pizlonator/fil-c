/*
 * chrt.c - manipulate a task's real-time attributes
 *
 * 27-Apr-2002: initial version - Robert Love <rml@tech9.net>
 * 04-May-2011: make it thread-aware - Davidlohr Bueso <dave@gnu.org>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"
#include "strutils.h"
#include "procfs.h"
#include "sched_attr.h"


/* control struct */
struct chrt_ctl {
	pid_t	pid;
	int	policy;				/* SCHED_* */
	int	priority;

	uint64_t runtime;			/* --sched-* options */
	uint64_t deadline;
	uint64_t period;

	unsigned int all_tasks : 1,		/* all threads of the PID */
		     reset_on_fork : 1,		/* SCHED_RESET_ON_FORK or SCHED_FLAG_RESET_ON_FORK */
		     altered : 1,		/* sched_set**() used */
		     verbose : 1;		/* verbose output */
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(_("Show or change the real-time scheduling attributes of a process.\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("Set policy:\n"
	" chrt [options] <priority> <command> [<arg>...]\n"
	" chrt [options] --pid <priority> <pid>\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("Get policy:\n"
	" chrt [options] -p <pid>\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Policy options:\n"), out);
	fputs(_(" -b, --batch          set policy to SCHED_BATCH\n"), out);
	fputs(_(" -d, --deadline       set policy to SCHED_DEADLINE\n"), out);
	fputs(_(" -f, --fifo           set policy to SCHED_FIFO\n"), out);
	fputs(_(" -i, --idle           set policy to SCHED_IDLE\n"), out);
	fputs(_(" -o, --other          set policy to SCHED_OTHER\n"), out);
	fputs(_(" -r, --rr             set policy to SCHED_RR (default)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Scheduling options:\n"), out);
	fputs(_(" -R, --reset-on-fork       set reset-on-fork flag\n"), out);
	fputs(_(" -T, --sched-runtime <ns>  runtime parameter for DEADLINE\n"), out);
	fputs(_(" -P, --sched-period <ns>   period parameter for DEADLINE\n"), out);
	fputs(_(" -D, --sched-deadline <ns> deadline parameter for DEADLINE\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Other options:\n"), out);
	fputs(_(" -a, --all-tasks      operate on all the tasks (threads) for a given pid\n"), out);
	fputs(_(" -m, --max            show min and max valid priorities\n"), out);
	fputs(_(" -p, --pid            operate on existing given pid\n"), out);
	fputs(_(" -v, --verbose        display status information\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(22));

	fprintf(out, USAGE_MAN_TAIL("chrt(1)"));
	exit(EXIT_SUCCESS);
}

static const char *get_policy_name(int policy)
{
#ifdef SCHED_RESET_ON_FORK
	policy &= ~SCHED_RESET_ON_FORK;
#endif
	switch (policy) {
	case SCHED_OTHER:
		return "SCHED_OTHER";
	case SCHED_FIFO:
		return "SCHED_FIFO";
#ifdef SCHED_IDLE
	case SCHED_IDLE:
		return "SCHED_IDLE";
#endif
	case SCHED_RR:
		return "SCHED_RR";
#ifdef SCHED_BATCH
	case SCHED_BATCH:
		return "SCHED_BATCH";
#endif
#ifdef SCHED_DEADLINE
	case SCHED_DEADLINE:
		return "SCHED_DEADLINE";
#endif
	default:
		break;
	}

	return _("unknown");
}

static void show_sched_pid_info(struct chrt_ctl *ctl, pid_t pid)
{
	int policy = -1, reset_on_fork = 0, prio = 0;
#ifdef SCHED_DEADLINE
	uint64_t deadline = 0, runtime = 0, period = 0;
#endif

	/* don't display "pid 0" as that is confusing */
	if (!pid)
		pid = getpid();

	errno = 0;

	/*
	 * New way
	 */
#ifdef HAVE_SCHED_SETATTR
	{
		struct sched_attr sa;

		if (sched_getattr(pid, &sa, sizeof(sa), 0) != 0) {
			if (errno == ENOSYS)
				goto fallback;
			err(EXIT_FAILURE, _("failed to get pid %d's policy"), pid);
		}

		policy = sa.sched_policy;
		prio = sa.sched_priority;
		reset_on_fork = sa.sched_flags & SCHED_FLAG_RESET_ON_FORK;
		deadline = sa.sched_deadline;
		runtime = sa.sched_runtime;
		period = sa.sched_period;
	}

	/*
	 * Old way
	 */
fallback:
	if (errno == ENOSYS)
#endif
	{
		struct sched_param sp;

		policy = sched_getscheduler(pid);
		if (policy == -1)
			err(EXIT_FAILURE, _("failed to get pid %d's policy"), pid);

		if (sched_getparam(pid, &sp) != 0)
			err(EXIT_FAILURE, _("failed to get pid %d's attributes"), pid);
		else
			prio = sp.sched_priority;
# ifdef SCHED_RESET_ON_FORK
		if (policy & SCHED_RESET_ON_FORK)
			reset_on_fork = 1;
# endif
	}

	if (ctl->altered)
		printf(_("pid %d's new scheduling policy: %s"), pid, get_policy_name(policy));
	else
		printf(_("pid %d's current scheduling policy: %s"), pid, get_policy_name(policy));

	if (reset_on_fork)
		printf("|SCHED_RESET_ON_FORK");
	putchar('\n');

	if (ctl->altered)
		printf(_("pid %d's new scheduling priority: %d\n"), pid, prio);
	else
		printf(_("pid %d's current scheduling priority: %d\n"), pid, prio);

#ifdef SCHED_DEADLINE
	if (policy == SCHED_DEADLINE) {
		if (ctl->altered)
			printf(_("pid %d's new runtime/deadline/period parameters: %ju/%ju/%ju\n"),
					pid, runtime, deadline, period);
		else
			printf(_("pid %d's current runtime/deadline/period parameters: %ju/%ju/%ju\n"),
					pid, runtime, deadline, period);
	}
#endif
}


static void show_sched_info(struct chrt_ctl *ctl)
{
	if (ctl->all_tasks) {
#ifdef __linux__
		DIR *sub = NULL;
		pid_t tid;
		struct path_cxt *pc = ul_new_procfs_path(ctl->pid, NULL);

		while (pc && procfs_process_next_tid(pc, &sub, &tid) == 0)
			show_sched_pid_info(ctl, tid);

		ul_unref_path(pc);
#else
		err(EXIT_FAILURE, _("cannot obtain the list of tasks"));
#endif
	} else
		show_sched_pid_info(ctl, ctl->pid);
}

static void show_min_max(void)
{
	unsigned long i;
	int policies[] = {
		SCHED_OTHER,
		SCHED_FIFO,
		SCHED_RR,
#ifdef SCHED_BATCH
		SCHED_BATCH,
#endif
#ifdef SCHED_IDLE
		SCHED_IDLE,
#endif
#ifdef SCHED_DEADLINE
		SCHED_DEADLINE,
#endif
	};

	for (i = 0; i < ARRAY_SIZE(policies); i++) {
		int plc = policies[i];
		int max = sched_get_priority_max(plc);
		int min = sched_get_priority_min(plc);

		if (max >= 0 && min >= 0)
			printf(_("%s min/max priority\t: %d/%d\n"),
					get_policy_name(plc), min, max);
		else
			printf(_("%s not supported?\n"), get_policy_name(plc));
	}
}

static int set_sched_one_by_setscheduler(struct chrt_ctl *ctl, pid_t pid)
{
	struct sched_param sp = { .sched_priority = ctl->priority };
	int policy = ctl->policy;

	errno = 0;
# ifdef SCHED_RESET_ON_FORK
	if (ctl->reset_on_fork)
		policy |= SCHED_RESET_ON_FORK;
# endif

#if defined (__linux__) && defined(SYS_sched_setscheduler)
	/* musl libc returns ENOSYS for its sched_setscheduler library
	 * function, because the sched_setscheduler Linux kernel system call
	 * does not conform to Posix; so we use the system call directly
	 */
	return syscall(SYS_sched_setscheduler, pid, policy, &sp);
#else
	return sched_setscheduler(pid, policy, &sp);
#endif
}


#ifndef HAVE_SCHED_SETATTR
static int set_sched_one(struct chrt_ctl *ctl, pid_t pid)
{
	return set_sched_one_by_setscheduler(ctl, pid);
}

#else /* !HAVE_SCHED_SETATTR */
static int set_sched_one(struct chrt_ctl *ctl, pid_t pid)
{
	struct sched_attr sa = { .size = sizeof(struct sched_attr) };

	/* old API is good enough for non-deadline */
	if (ctl->policy != SCHED_DEADLINE)
		return set_sched_one_by_setscheduler(ctl, pid);

	/* no changeed by chrt, follow the current setting */
	sa.sched_nice = getpriority(PRIO_PROCESS, pid);

	/* use main() to check if the setting makes sense */
	sa.sched_policy	  = ctl->policy;
	sa.sched_priority = ctl->priority;
	sa.sched_runtime  = ctl->runtime;
	sa.sched_period   = ctl->period;
	sa.sched_deadline = ctl->deadline;

# ifdef SCHED_FLAG_RESET_ON_FORK
	/* Don't use SCHED_RESET_ON_FORK for sched_setattr()! */
	if (ctl->reset_on_fork)
		sa.sched_flags |= SCHED_FLAG_RESET_ON_FORK;
# endif
	errno = 0;
	return sched_setattr(pid, &sa, 0);
}
#endif /* HAVE_SCHED_SETATTR */

static void set_sched(struct chrt_ctl *ctl)
{
	if (ctl->all_tasks) {
#ifdef __linux__
		DIR *sub = NULL;
		pid_t tid;
		struct path_cxt *pc = ul_new_procfs_path(ctl->pid, NULL);

		if (!pc)
			err(EXIT_FAILURE, _("cannot obtain the list of tasks"));

		while (procfs_process_next_tid(pc, &sub, &tid) == 0) {
			if (set_sched_one(ctl, tid) == -1)
				err(EXIT_FAILURE, _("failed to set tid %d's policy"), tid);
		}
		ul_unref_path(pc);
#else
		err(EXIT_FAILURE, _("cannot obtain the list of tasks"));
#endif
	} else if (set_sched_one(ctl, ctl->pid) == -1)
		err(EXIT_FAILURE, _("failed to set pid %d's policy"), ctl->pid);

	ctl->altered = 1;
}

int main(int argc, char **argv)
{
	struct chrt_ctl _ctl = { .pid = -1, .policy = SCHED_RR }, *ctl = &_ctl;
	int c;

	static const struct option longopts[] = {
		{ "all-tasks",  no_argument, NULL, 'a' },
		{ "batch",	no_argument, NULL, 'b' },
		{ "deadline",   no_argument, NULL, 'd' },
		{ "fifo",	no_argument, NULL, 'f' },
		{ "idle",	no_argument, NULL, 'i' },
		{ "pid",	no_argument, NULL, 'p' },
		{ "help",	no_argument, NULL, 'h' },
		{ "max",        no_argument, NULL, 'm' },
		{ "other",	no_argument, NULL, 'o' },
		{ "rr",		no_argument, NULL, 'r' },
		{ "sched-runtime",  required_argument, NULL, 'T' },
		{ "sched-period",   required_argument, NULL, 'P' },
		{ "sched-deadline", required_argument, NULL, 'D' },
		{ "reset-on-fork",  no_argument,       NULL, 'R' },
		{ "verbose",	no_argument, NULL, 'v' },
		{ "version",	no_argument, NULL, 'V' },
		{ NULL,		no_argument, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while((c = getopt_long(argc, argv, "+abdD:fiphmoP:T:rRvV", longopts, NULL)) != -1)
	{
		switch (c) {
		case 'a':
			ctl->all_tasks = 1;
			break;
		case 'b':
#ifdef SCHED_BATCH
			ctl->policy = SCHED_BATCH;
#endif
			break;

		case 'd':
#ifdef SCHED_DEADLINE
			ctl->policy = SCHED_DEADLINE;
#endif
			break;
		case 'f':
			ctl->policy = SCHED_FIFO;
			break;
		case 'R':
			ctl->reset_on_fork = 1;
			break;
		case 'i':
#ifdef SCHED_IDLE
			ctl->policy = SCHED_IDLE;
#endif
			break;
		case 'm':
			show_min_max();
			return EXIT_SUCCESS;
		case 'o':
			ctl->policy = SCHED_OTHER;
			break;
		case 'p':
			errno = 0;
			ctl->pid = strtos32_or_err(argv[argc - 1], _("invalid PID argument"));
			break;
		case 'r':
			ctl->policy = SCHED_RR;
			break;
		case 'v':
			ctl->verbose = 1;
			break;
		case 'T':
			ctl->runtime = strtou64_or_err(optarg, _("invalid runtime argument"));
			break;
		case 'P':
			ctl->period = strtou64_or_err(optarg, _("invalid period argument"));
			break;
		case 'D':
			ctl->deadline = strtou64_or_err(optarg, _("invalid deadline argument"));
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (((ctl->pid > -1) && argc - optind < 1) ||
	    ((ctl->pid == -1) && argc - optind < 2)) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
}

	if ((ctl->pid > -1) && (ctl->verbose || argc - optind == 1)) {
		show_sched_info(ctl);
		if (argc - optind == 1)
			return EXIT_SUCCESS;
	}

	errno = 0;
	ctl->priority = strtos32_or_err(argv[optind], _("invalid priority argument"));

#ifdef SCHED_DEADLINE
	if ((ctl->runtime || ctl->deadline || ctl->period) && ctl->policy != SCHED_DEADLINE)
		errx(EXIT_FAILURE, _("--sched-{runtime,deadline,period} options "
				     "are supported for SCHED_DEADLINE only"));
	if (ctl->policy == SCHED_DEADLINE) {
		/* The basic rule is runtime <= deadline <= period, so we can
		 * make deadline and runtime optional on command line. Note we
		 * don't check any values or set any defaults, it's kernel
		 * responsibility.
		 */
		if (ctl->deadline == 0)
			ctl->deadline = ctl->period;
		if (ctl->runtime == 0)
			ctl->runtime = ctl->deadline;
	}
#else
	if (ctl->runtime || ctl->deadline || ctl->period)
		errx(EXIT_FAILURE, _("SCHED_DEADLINE is unsupported"));
#endif
	if (ctl->pid == -1)
		ctl->pid = 0;
	if (ctl->priority < sched_get_priority_min(ctl->policy) ||
	    sched_get_priority_max(ctl->policy) < ctl->priority)
		errx(EXIT_FAILURE,
		     _("unsupported priority value for the policy: %d: see --max for valid range"),
		     ctl->priority);
	set_sched(ctl);

	if (ctl->verbose)
		show_sched_info(ctl);

	if (!ctl->pid) {
		argv += optind + 1;
		if (strcmp(argv[0], "--") == 0)
			argv++;
		execvp(argv[0], argv);
		errexec(argv[0]);
	}

	return EXIT_SUCCESS;
}
