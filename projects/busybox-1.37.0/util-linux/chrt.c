/* vi: set sw=4 ts=4: */
/*
 * chrt - manipulate real-time attributes of a process
 * Copyright (c) 2006-2007 Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config CHRT
//config:	bool "chrt (5.1 kb)"
//config:	default y
//config:	help
//config:	Manipulate real-time attributes of a process.
//config:	This requires sched_{g,s}etparam support in your libc.

//applet:IF_CHRT(APPLET_NOEXEC(chrt, chrt, BB_DIR_USR_BIN, BB_SUID_DROP, chrt))

//kbuild:lib-$(CONFIG_CHRT) += chrt.o

//usage:#define chrt_trivial_usage
//usage:       "-m | -p [PRIO] PID | [-rfobi] PRIO PROG ARGS"
//usage:#define chrt_full_usage "\n\n"
//usage:       "Change scheduling priority and class for a process\n"
//usage:     "\n	-m	Show min/max priorities"
//usage:     "\n	-p	Operate on PID"
//usage:     "\n	-r	Set SCHED_RR class"
//usage:     "\n	-f	Set SCHED_FIFO class"
//usage:     "\n	-o	Set SCHED_OTHER class"
//usage:     "\n	-b	Set SCHED_BATCH class"
//usage:     "\n	-i	Set SCHED_IDLE class"
//usage:
//usage:#define chrt_example_usage
//usage:       "$ chrt -r 4 sleep 900; x=$!\n"
//usage:       "$ chrt -f -p 3 $x\n"
//usage:       "You need CAP_SYS_NICE privileges to set scheduling attributes of a process"

#include <sched.h>
#include "libbb.h"
#ifndef SCHED_IDLE
# define SCHED_IDLE 5
#endif

//musl has no __MUSL__ or similar define to check for,
//but its <sys/types.h> has these lines:
// #define __NEED_fsblkcnt_t
// #define __NEED_fsfilcnt_t
#if defined(__linux__) && defined(__NEED_fsblkcnt_t) && defined(__NEED_fsfilcnt_t)
# define LIBC_IS_MUSL 1
# include <sys/syscall.h>
#else
# define LIBC_IS_MUSL 0
#endif

static const char *policy_name(int pol)
{
	if (pol > 6)
		return utoa(pol);
	return nth_string(
		"OTHER"   "\0" /* 0:SCHED_OTHER */
		"FIFO"    "\0" /* 1:SCHED_FIFO */
		"RR"      "\0" /* 2:SCHED_RR */
		"BATCH"   "\0" /* 3:SCHED_BATCH */
		"ISO"     "\0" /* 4:SCHED_ISO */
		"IDLE"    "\0" /* 5:SCHED_IDLE */
		"DEADLINE",    /* 6:SCHED_DEADLINE */
		pol
	);
}

static void show_min_max(int pol)
{
	const char *fmt = "SCHED_%s min/max priority\t: %u/%u\n";
	int max, min;

	max = sched_get_priority_max(pol);
	min = sched_get_priority_min(pol);
	if ((max|min) < 0)
		fmt = "SCHED_%s not supported\n";
	printf(fmt, policy_name(pol), min, max);
}

#define OPT_m (1<<0)
#define OPT_p (1<<1)
#define OPT_r (1<<2)
#define OPT_f (1<<3)
#define OPT_o (1<<4)
#define OPT_b (1<<5)
#define OPT_i (1<<6)

int chrt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chrt_main(int argc UNUSED_PARAM, char **argv)
{
	pid_t pid = 0;
	unsigned opt;
	struct sched_param sp;
	char *pid_str;
	char *priority = NULL;
	const char *current_new;
	int policy = SCHED_RR;
	int ret;

	opt = getopt32(argv, "^"
			"+" "mprfobi"
			"\0"
			/* only one policy accepted: */
			"r--fobi:f--robi:o--rfbi:b--rfoi:i--rfob"
	);
	if (opt & OPT_m) { /* print min/max and exit */
		show_min_max(SCHED_OTHER);
		show_min_max(SCHED_FIFO);
		show_min_max(SCHED_RR);
		show_min_max(SCHED_BATCH);
		show_min_max(SCHED_IDLE);
		fflush_stdout_and_exit_SUCCESS();
	}
	//if (opt & OPT_r)
	//	policy = SCHED_RR; - default, already set
	if (opt & OPT_f)
		policy = SCHED_FIFO;
	if (opt & OPT_o)
		policy = SCHED_OTHER;
	if (opt & OPT_b)
		policy = SCHED_BATCH;
	if (opt & OPT_i)
		policy = SCHED_IDLE;

	argv += optind;
	if (!argv[0])
		bb_show_usage();
	if (opt & OPT_p) {
		pid_str = *argv++;
		if (*argv) { /* "-p PRIO PID [...]" */
			priority = pid_str;
			pid_str = *argv;
		}
		/* else "-p PID", and *argv == NULL */
		pid = xatoul_range(pid_str, 1, ((unsigned)(pid_t)ULONG_MAX) >> 1);
	} else {
		priority = *argv++;
		if (!*argv)
			bb_show_usage();
	}

	current_new = "current\0new";
	if (opt & OPT_p) {
		int pol;
 print_rt_info:
#if LIBC_IS_MUSL
		/* musl libc returns ENOSYS for its sched_getscheduler library
		 * function, because the sched_getscheduler Linux kernel system call
		 * does not conform to Posix; so we use the system call directly
		 */
		pol = syscall(SYS_sched_getscheduler, pid);
#else
		pol = sched_getscheduler(pid);
#endif
		if (pol < 0)
			bb_perror_msg_and_die("can't %cet pid %u's policy", 'g', (int)pid);
#ifdef SCHED_RESET_ON_FORK
		/* "Since Linux 2.6.32, the SCHED_RESET_ON_FORK flag
		 * can be ORed in policy when calling sched_setscheduler().
		 * As a result of including this flag, children created by
		 * fork(2) do not inherit privileged scheduling policies"
		 *
		 * This bit is also returned by sched_getscheduler()!
		 * (TODO: do we want to show it?)
		 */
		pol &= ~SCHED_RESET_ON_FORK;
#endif
		printf("pid %u's %s scheduling policy: SCHED_%s\n",
			pid, current_new, policy_name(pol)
		);
#if LIBC_IS_MUSL
		ret = syscall(SYS_sched_getparam, pid, &sp);
#else
		ret = sched_getparam(pid, &sp);
#endif
		if (ret)
			bb_perror_msg_and_die("can't get pid %u's attributes", (int)pid);
		printf("pid %u's %s scheduling priority: %d\n",
			(int)pid, current_new, sp.sched_priority
		);
		if (!*argv) {
			/* Either it was just "-p PID",
			 * or it was "-p PRIO PID" and we came here
			 * for the second time (see goto below) */
			return EXIT_SUCCESS;
		}
		*argv = NULL;
		current_new += 8;
	}

	sp.sched_priority = xstrtou_range(priority, 0,
		sched_get_priority_min(policy), sched_get_priority_max(policy)
	);

#if LIBC_IS_MUSL
	ret = syscall(SYS_sched_setscheduler, pid, policy, &sp);
#else
	ret = sched_setscheduler(pid, policy, &sp);
#endif
	if (ret)
		bb_perror_msg_and_die("can't %cet pid %u's policy", 's', (int)pid);

	if (!argv[0]) /* "-p PRIO PID [...]" */
		goto print_rt_info;

	BB_EXECVP_or_die(argv);
}
