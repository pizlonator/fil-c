/*
 * chvt.c - aeb - 940227 - Change virtual terminal
 */
#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sysexits.h>

#include "libcommon.h"

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;
	fprintf(stderr, _("Usage: %s [option...] N\n"), get_progname());
	if (options) {
		int max = 0;

		fprintf(stderr, "\n");
		fprintf(stderr, _("Options:"));
		fprintf(stderr, "\n");

		for (h = options; h && h->opts; h++) {
			int len = (int) strlen(h->opts);
			if (max < len)
				max = len;
		}
		max += 2;

		for (h = options; h && h->opts; h++)
			fprintf(stderr, "  %-*s %s\n", max, h->opts, h->desc);
	}

	fprintf(stderr, "\n");
	fprintf(stderr, _("Report bugs to authors.\n"));
	fprintf(stderr, "\n");

	exit(rc);
}

static void
sighandler(int sig __attribute__((unused)),
           siginfo_t *si __attribute__((unused)),
           void *uc __attribute__((unused)))
{
	return;
}

int main(int argc, char *argv[])
{
	int c, fd, num;
	timer_t timerid;
	struct sigaction sa;
	struct sigevent sev;
	struct itimerspec its;

	const char *const short_opts = "hV";
	const struct option long_opts[] = {
		{ "help",    no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	set_progname(argv[0]);
	setuplocale();

	const struct kbd_help opthelp[] = {
		{ "-h, --help",    _("print this usage message.") },
		{ "-V, --version", _("print version number.")     },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'V':
				print_version_and_exit();
				break;
			case 'h':
				usage(EXIT_SUCCESS, opthelp);
				break;
			case '?':
				usage(EX_USAGE, opthelp);
				break;
		}
	}

	if (argc == optind) {
		kbd_warning(0, _("Not enough arguments."));
		usage(EX_USAGE, opthelp);
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	num = atoi(argv[optind]);

	sa.sa_flags     = SA_SIGINFO;
	sa.sa_sigaction = sighandler;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGALRM, &sa, NULL) < 0)
		kbd_error(EXIT_FAILURE, errno, _("Unable to set signal handler"));

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGALRM;
	sev.sigev_value.sival_ptr = &timerid;

	if (timer_create(CLOCK_REALTIME, &sev, &timerid) < 0)
		kbd_error(EXIT_FAILURE, errno, _("Unable to create timer"));

	its.it_value.tv_sec     = 1;
	its.it_value.tv_nsec    = 0;
	its.it_interval.tv_sec  = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	if (timer_settime(timerid, 0, &its, NULL) < 0)
		kbd_error(EXIT_FAILURE, errno, _("Unable to set timer"));

	do {
		errno = 0;

		if (ioctl(fd, VT_ACTIVATE, num) < 0 && errno != EINTR)
			kbd_error(EXIT_FAILURE, errno,  _("Couldn't activate vt %d"), num);

		if (ioctl(fd, VT_WAITACTIVE, num) < 0 && errno != EINTR)
			kbd_error(EXIT_FAILURE, errno, "ioctl(%d,VT_WAITACTIVE)", num);

	} while (errno == EINTR);

	timer_delete(timerid);
	signal(SIGALRM, SIG_DFL);

	return EXIT_SUCCESS;
}
