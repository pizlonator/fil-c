/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Daan De Meyer <daan.j.demeyer@gmail.com>
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "closestream.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %s [options] <program> [arguments ...]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Run a program in a new process group.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -f, --foregound     Make a foreground process group\n"), out);

	fprintf(out, USAGE_HELP_OPTIONS(16));

	fprintf(out, USAGE_MAN_TAIL("setpgid(1)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int ch, foreground = 0, fd;
	sigset_t s, old;

	static const struct option longopts[] = {
		{"foreground", no_argument, NULL, 'f'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((ch = getopt_long(argc, argv, "+fVh", longopts, NULL)) != -1)
		switch (ch) {
		case 'f':
			foreground = 1;
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

	if (setpgid(0, 0) < 0)
		err(EXIT_FAILURE, _("setpgid failed"));

	if (foreground) {
		fd = open("/dev/tty", O_RDONLY|O_CLOEXEC);
		if (fd >= 0) {
			if (sigemptyset(&s) < 0)
				err(EXIT_FAILURE, _("sigemptyset failed"));
			if (sigaddset(&s, SIGTTOU) < 0)
				err(EXIT_FAILURE, _("sigaddset failed"));
			if (sigprocmask(SIG_BLOCK, &s, &old) < 0)
				err(EXIT_FAILURE, _("sigprocmask failed"));
			if (tcsetpgrp(fd, getpgid(0)) < 0)
				err(EXIT_FAILURE, _("tcsetpgrp failed"));
			if (sigprocmask(SIG_SETMASK, &old, NULL) < 0)
				err(EXIT_FAILURE, _("sigprocmask failed"));
		}
	}

	execvp(argv[optind], argv + optind);
	errexec(argv[optind]);
}
