/*
 * uclampset.c - change utilization clamping attributes of a task or the system
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
 * Copyright (C) 2020-2021 Qais Yousef
 * Copyright (C) 2020-2021 Arm Ltd
 */

#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

#include "closestream.h"
#include "path.h"
#include "pathnames.h"
#include "procfs.h"
#include "sched_attr.h"
#include "strutils.h"

#define NOT_SET		0xdeadbeef

struct uclampset {
	unsigned int util_min;
	unsigned int util_max;

	pid_t pid;
	unsigned int	all_tasks:1,		/* all threads of the PID */
			system:1,
			util_min_set:1,		/* indicates -m option was passed */
			util_max_set:1,		/* indicates -M option was passed */
			reset_on_fork:1,
			verbose:1;
	char *cmd;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %1$s [options]\n"
		  " %1$s [options] --pid <pid> | --system | <command> <arg>...\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show or change the utilization clamping attributes.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -m <value>           util_min value to set\n"), out);
	fputs(_(" -M <value>           util_max value to set\n"), out);
	fputs(_(" -a, --all-tasks      operate on all the tasks (threads) for a given pid\n"), out);
	fputs(_(" -p, --pid <pid>      operate on existing given pid\n"), out);
	fputs(_(" -s, --system         operate on system\n"), out);
	fputs(_(" -R, --reset-on-fork  set reset-on-fork flag\n"), out);
	fputs(_(" -v, --verbose        display status information\n"), out);

	fprintf(out, USAGE_HELP_OPTIONS(22));

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Utilization value range is [0:1024]. Use special -1 value to "
		"reset to system's default.\n"), out);

	fprintf(out, USAGE_MAN_TAIL("uclampset(1)"));
	exit(EXIT_SUCCESS);
}

static void show_uclamp_pid_info(pid_t pid, char *cmd)
{
	struct sched_attr sa;
	char *comm;

	/* don't display "pid 0" as that is confusing */
	if (!pid)
		pid = getpid();

	if (sched_getattr(pid, &sa, sizeof(sa), 0) != 0)
		err(EXIT_FAILURE, _("failed to get pid %d's uclamp values"), pid);

	if (cmd)
		comm = cmd;
	else
		comm = pid_get_cmdname(pid);

	printf(_("%s (%d) util_clamp: min: %d max: %d\n"),
	       comm ? : "unknown", pid, sa.sched_util_min, sa.sched_util_max);

	if (!cmd)
		free(comm);
}

static unsigned int read_uclamp_sysfs(char *filename)
{
	unsigned int val;

	if (ul_path_read_u32(NULL, &val, filename) != 0)
		err(EXIT_FAILURE, _("cannot read %s"), filename);

	return val;
}

static void write_uclamp_sysfs(char *filename, unsigned int val)
{
	if (ul_path_write_u64(NULL, val, filename) != 0)
		err(EXIT_FAILURE, _("cannot write %s"), filename);
}

static void show_uclamp_system_info(void)
{
	unsigned int min, max;

	min = read_uclamp_sysfs(_PATH_PROC_UCLAMP_MIN);
	max = read_uclamp_sysfs(_PATH_PROC_UCLAMP_MAX);

	printf(_("System util_clamp: min: %u max: %u\n"), min, max);
}

static void show_uclamp_info(struct uclampset *ctl)
{
	if (ctl->system) {
		show_uclamp_system_info();
	} else if (ctl->all_tasks) {
		DIR *sub = NULL;
		pid_t tid;
		struct path_cxt *pc = ul_new_procfs_path(ctl->pid, NULL);

		if (!pc)
			err(EXIT_FAILURE, _("cannot obtain the list of tasks"));

		while (procfs_process_next_tid(pc, &sub, &tid) == 0)
			show_uclamp_pid_info(tid, NULL);

		ul_unref_path(pc);
	} else {
		show_uclamp_pid_info(ctl->pid, ctl->cmd);
	}
}

static int set_uclamp_one(struct uclampset *ctl, pid_t pid)
{
	struct sched_attr sa;

	if (sched_getattr(pid, &sa, sizeof(sa), 0) != 0)
		err(EXIT_FAILURE, _("failed to get pid %d's uclamp values"), pid);

	if (ctl->util_min_set)
		sa.sched_util_min = ctl->util_min;
	if (ctl->util_max_set)
		sa.sched_util_max = ctl->util_max;

	sa.sched_flags = SCHED_FLAG_KEEP_POLICY |
			 SCHED_FLAG_KEEP_PARAMS |
			 SCHED_FLAG_UTIL_CLAMP_MIN |
			 SCHED_FLAG_UTIL_CLAMP_MAX;

	if (ctl->reset_on_fork)
		sa.sched_flags |= SCHED_FLAG_RESET_ON_FORK;

	return sched_setattr(pid, &sa, 0);
}

static void set_uclamp_pid(struct uclampset *ctl)
{
	if (ctl->all_tasks) {
		DIR *sub = NULL;
		pid_t tid;
		struct path_cxt *pc = ul_new_procfs_path(ctl->pid, NULL);

		if (!pc)
			err(EXIT_FAILURE, _("cannot obtain the list of tasks"));

		while (procfs_process_next_tid(pc, &sub, &tid) == 0) {
			if (set_uclamp_one(ctl, tid) == -1)
				err(EXIT_FAILURE, _("failed to set tid %d's uclamp values"), tid);
		}
		ul_unref_path(pc);

	} else if (set_uclamp_one(ctl, ctl->pid) == -1) {
		err(EXIT_FAILURE, _("failed to set pid %d's uclamp values"), ctl->pid);
	}
}

static void set_uclamp_system(struct uclampset *ctl)
{
	if (!ctl->util_min_set)
		ctl->util_min = read_uclamp_sysfs(_PATH_PROC_UCLAMP_MIN);

	if (!ctl->util_max_set)
		ctl->util_max = read_uclamp_sysfs(_PATH_PROC_UCLAMP_MAX);

	if (ctl->util_min > ctl->util_max) {
		errno = EINVAL;
		err(EXIT_FAILURE, _("util_min must be <= util_max"));
	}

	write_uclamp_sysfs(_PATH_PROC_UCLAMP_MIN, ctl->util_min);
	write_uclamp_sysfs(_PATH_PROC_UCLAMP_MAX, ctl->util_max);
}

int main(int argc, char **argv)
{
	struct uclampset _ctl = {
		.pid = -1,
		.util_min = NOT_SET,
		.util_max = NOT_SET,
		.cmd = NULL
	};
	struct uclampset *ctl = &_ctl;
	int c;

	static const struct option longopts[] = {
		{ "all-tasks",		no_argument, NULL, 'a' },
		{ "pid",		required_argument, NULL, 'p' },
		{ "system",		no_argument, NULL, 's' },
		{ "reset-on-fork",	no_argument, NULL, 'R' },
		{ "help",		no_argument, NULL, 'h' },
		{ "verbose",		no_argument, NULL, 'v' },
		{ "version",		no_argument, NULL, 'V' },
		{ NULL,			no_argument, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while((c = getopt_long(argc, argv, "+asRp:hm:M:vV", longopts, NULL)) != -1)
	{
		switch (c) {
		case 'a':
			ctl->all_tasks = 1;
			break;
		case 'p':
			errno = 0;
			ctl->pid = strtos32_or_err(optarg, _("invalid PID argument"));
			break;
		case 's':
			ctl->system = 1;
			break;
		case 'R':
			ctl->reset_on_fork = 1;
			break;
		case 'v':
			ctl->verbose = 1;
			break;
		case 'm':
			ctl->util_min = strtos32_or_err(optarg, _("invalid util_min argument"));
			ctl->util_min_set = 1;
			break;
		case 'M':
			ctl->util_max = strtos32_or_err(optarg, _("invalid util_max argument"));
			ctl->util_max_set = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
			/* fallthrough */
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (argc == 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	/* all_tasks implies --pid */
	if (ctl->all_tasks && ctl->pid == -1) {
		errno = EINVAL;
		err(EXIT_FAILURE, _("missing -p option"));
	}

	if (!ctl->util_min_set && !ctl->util_max_set) {
		/* -p or -s must be passed */
		if (!ctl->system && ctl->pid == -1) {
			usage();
			exit(EXIT_FAILURE);
		}

		show_uclamp_info(ctl);
		return EXIT_SUCCESS;
	}

	/* ensure there's a command to execute if no -s or -p */
	if (!ctl->system && ctl->pid == -1) {
		if (argc <= optind) {
			errno = EINVAL;
			err(EXIT_FAILURE, _("no cmd to execute"));
		}

		argv += optind;
		ctl->cmd = argv[0];
	}

	if (ctl->pid == -1)
		ctl->pid = 0;

	if (ctl->system)
		set_uclamp_system(ctl);
	else
		set_uclamp_pid(ctl);

	if (ctl->verbose)
		show_uclamp_info(ctl);

	if (ctl->cmd) {
		execvp(ctl->cmd, argv);
		errexec(ctl->cmd);
	}

	return EXIT_SUCCESS;
}
