/*
 * ionice: set or get process io scheduling class and priority
 *
 * Copyright (C) 2005 Jens Axboe <jens@axboe.dk>
 *
 * Released under the terms of the GNU General Public License version 2
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif
#include <ctype.h>

#include "nls.h"
#include "strutils.h"
#include "c.h"
#include "closestream.h"

static int tolerant;

static inline int ioprio_set(int which, int who, int ioprio)
{
	return syscall(SYS_ioprio_set, which, who, ioprio);
}

static inline int ioprio_get(int which, int who)
{
	return syscall(SYS_ioprio_get, which, who);
}

enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE,
};

enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER,
};

#define IOPRIO_CLASS_SHIFT	(13)
#define IOPRIO_PRIO_MASK	((1UL << IOPRIO_CLASS_SHIFT) - 1)

#define IOPRIO_PRIO_CLASS(mask)	((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask)	((mask) & IOPRIO_PRIO_MASK)
#define IOPRIO_PRIO_VALUE(class, data)	(((class) << IOPRIO_CLASS_SHIFT) | data)

static const char *to_prio[] = {
	[IOPRIO_CLASS_NONE] = "none",
	[IOPRIO_CLASS_RT]   = "realtime",
	[IOPRIO_CLASS_BE]   = "best-effort",
	[IOPRIO_CLASS_IDLE] = "idle"
};

static int parse_ioclass(const char *str)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(to_prio); i++)
		if (!strcasecmp(str, to_prio[i]))
			return i;
	return -1;
}

static void ioprio_print(int pid, int who)
{
	int ioprio = ioprio_get(who, pid);

	if (ioprio == -1)
		err(EXIT_FAILURE, _("ioprio_get failed"));
	else {
		int ioclass = IOPRIO_PRIO_CLASS(ioprio);
		const char *name = _("unknown");

		if (ioclass >= 0 && (size_t) ioclass < ARRAY_SIZE(to_prio))
			name = to_prio[ioclass];

		if (ioclass != IOPRIO_CLASS_IDLE)
			printf(_("%s: prio %lu\n"), name,
			       IOPRIO_PRIO_DATA(ioprio));
		else
			printf("%s\n", name);
	}
}

static void ioprio_setid(int which, int ioclass, int data, int who)
{
	int rc = ioprio_set(who, which,
			    IOPRIO_PRIO_VALUE(ioclass, data));

	if (rc == -1 && !tolerant)
		err(EXIT_FAILURE, _("ioprio_set failed"));
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,  _(" %1$s [options] -p <pid>...\n"
			" %1$s [options] -P <pgid>...\n"
			" %1$s [options] -u <uid>...\n"
			" %1$s [options] <command>\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show or change the I/O-scheduling class and priority of a process.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -c, --class <class>    name or number of scheduling class,\n"
		"                          0: none, 1: realtime, 2: best-effort, 3: idle\n"), out);
	fputs(_(" -n, --classdata <num>  priority (0..7) in the specified scheduling class,\n"
		"                          only for the realtime and best-effort classes\n"), out);
	fputs(_(" -p, --pid <pid>...     act on these already running processes\n"), out);
	fputs(_(" -P, --pgid <pgrp>...   act on already running processes in these groups\n"), out);
	fputs(_(" -t, --ignore           ignore failures\n"), out);
	fputs(_(" -u, --uid <uid>...     act on already running processes owned by these users\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(24));

	fprintf(out, USAGE_MAN_TAIL("ionice(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int data = 4, set = 0, ioclass = IOPRIO_CLASS_BE, c;
	int which = 0, who = 0;
	const char *invalid_msg = NULL;

	static const struct option longopts[] = {
		{ "classdata", required_argument, NULL, 'n' },
		{ "class",     required_argument, NULL, 'c' },
		{ "help",      no_argument,       NULL, 'h' },
		{ "ignore",    no_argument,       NULL, 't' },
		{ "pid",       required_argument, NULL, 'p' },
		{ "pgid",      required_argument, NULL, 'P' },
		{ "uid",       required_argument, NULL, 'u' },
		{ "version",   no_argument,       NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "+n:c:p:P:u:tVh", longopts, NULL)) != EOF)
		switch (c) {
		case 'n':
			data = strtos32_or_err(optarg, _("invalid class data argument"));
			set |= 1;
			break;
		case 'c':
			if (isdigit(*optarg))
				ioclass = strtos32_or_err(optarg,
						_("invalid class argument"));
			else {
				ioclass = parse_ioclass(optarg);
				if (ioclass < 0)
					errx(EXIT_FAILURE,
						_("unknown scheduling class: '%s'"),
						optarg);
			}
			set |= 2;
			break;
		case 'p':
			if (who)
				errx(EXIT_FAILURE,
				     _("can handle only one of pid, pgid or uid at once"));
			invalid_msg = _("invalid PID argument");
			which = strtos32_or_err(optarg, invalid_msg);
			who = IOPRIO_WHO_PROCESS;
			break;
		case 'P':
			if (who)
				errx(EXIT_FAILURE,
				     _("can handle only one of pid, pgid or uid at once"));
			invalid_msg = _("invalid PGID argument");
			which = strtos32_or_err(optarg, invalid_msg);
			who = IOPRIO_WHO_PGRP;
			break;
		case 'u':
			if (who)
				errx(EXIT_FAILURE,
				     _("can handle only one of pid, pgid or uid at once"));
			invalid_msg = _("invalid UID argument");
			which = strtos32_or_err(optarg, invalid_msg);
			who = IOPRIO_WHO_USER;
			break;
		case 't':
			tolerant = 1;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	switch (ioclass) {
		case IOPRIO_CLASS_NONE:
			if ((set & 1) && !tolerant)
				warnx(_("ignoring given class data for none class"));
			data = 0;
			break;
		case IOPRIO_CLASS_RT:
		case IOPRIO_CLASS_BE:
			break;
		case IOPRIO_CLASS_IDLE:
			if ((set & 1) && !tolerant)
				warnx(_("ignoring given class data for idle class"));
			data = 7;
			break;
		default:
			if (!tolerant)
				warnx(_("unknown prio class %d"), ioclass);
			break;
	}

	if (!set && !which && optind == argc)
		/*
		 * ionice without options, print the current ioprio
		 */
		ioprio_print(0, IOPRIO_WHO_PROCESS);
	else if (!set && who) {
		/*
		 * ionice -p|-P|-u ID [ID ...]
		 */
		ioprio_print(which, who);

		for(; argv[optind]; ++optind) {
			which = strtos32_or_err(argv[optind], invalid_msg);
			ioprio_print(which, who);
		}
	} else if (set && who) {
		/*
		 * ionice -c CLASS -p|-P|-u ID [ID ...]
		 */
		ioprio_setid(which, ioclass, data, who);

		for(; argv[optind]; ++optind) {
			which = strtos32_or_err(argv[optind], invalid_msg);
			ioprio_setid(which, ioclass, data, who);
		}
	} else if (argv[optind]) {
		/*
		 * ionice [-c CLASS] COMMAND
		 */
		ioprio_setid(0, ioclass, data, IOPRIO_WHO_PROCESS);
		execvp(argv[optind], &argv[optind]);
		errexec(argv[optind]);
	} else {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
