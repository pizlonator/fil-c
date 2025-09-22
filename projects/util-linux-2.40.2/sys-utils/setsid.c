/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Rick Sladkey <jrs@world.std.com>
 *
 * setsid.c -- execute a command in a new session
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2001-01-18 John Fremlin <vii@penguinpowered.com>
 * - fork in case we are process group leader
 *
 * 2008-08-20 Daniel Kahn Gillmor <dkg@fifthhorseman.net>
 * - if forked, wait on child process and emit its return code.
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %s [options] <program> [arguments ...]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Run a program in a new session.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -c, --ctty     set the controlling terminal to the current one\n"), out);
	fputs(_(" -f, --fork     always fork\n"), out);
	fputs(_(" -w, --wait     wait program to exit, and use the same return\n"), out);

	fprintf(out, USAGE_HELP_OPTIONS(16));

	fprintf(out, USAGE_MAN_TAIL("setsid(1)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int ch, forcefork = 0;
	int ctty = 0;
	pid_t pid;
	int status = 0;

	static const struct option longopts[] = {
		{"ctty", no_argument, NULL, 'c'},
		{"fork", no_argument, NULL, 'f'},
		{"wait", no_argument, NULL, 'w'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((ch = getopt_long(argc, argv, "+Vhcfw", longopts, NULL)) != -1)
		switch (ch) {
		case 'c':
			ctty=1;
			break;
		case 'f':
			forcefork = 1;
			break;
		case 'w':
			status = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}

	if (argc - optind < 1) {
		warnx(_("no command specified"));
		errtryhelp(EXIT_FAILURE);
	}

	if (forcefork || getpgrp() == getpid()) {
		pid = fork();
		switch (pid) {
		case -1:
			err(EXIT_FAILURE, _("fork"));
		case 0:
			/* child */
			break;
		default:
			/* parent */
			if (!status)
				return EXIT_SUCCESS;
			if (wait(&status) != pid)
				err(EXIT_FAILURE, "wait");
			if (WIFEXITED(status))
				return WEXITSTATUS(status);
			err(status, _("child %d did not exit normally"), pid);
		}
	}
	if (setsid() < 0)
		/* cannot happen */
		err(EXIT_FAILURE, _("setsid failed"));

	if (ctty && ioctl(STDIN_FILENO, TIOCSCTTY, 1))
		err(EXIT_FAILURE, _("failed to set the controlling terminal"));
	execvp(argv[optind], argv + optind);
	errexec(argv[optind]);
}
