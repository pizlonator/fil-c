#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/keyboard.h>

#include "libcommon.h"

int tmp; /* for debugging */

int fd;
int oldkbmode;
struct termios old;

/*
 * version 0.81 of showkey would restore kbmode unconditially to XLATE,
 * thus making the console unusable when it was called under X.
 */
static void
get_mode(void)
{
	const char *m;

	if (ioctl(fd, KDGKBMODE, &oldkbmode))
		kbd_error(EXIT_FAILURE, errno, _("Unable to read keyboard mode"));

	switch (oldkbmode) {
		case K_RAW:
			m = "RAW";
			break;
		case K_XLATE:
			m = "XLATE";
			break;
		case K_MEDIUMRAW:
			m = "MEDIUMRAW";
			break;
		case K_UNICODE:
			m = "UNICODE";
			break;
		default:
			m = _("?UNKNOWN?");
			break;
	}
	printf(_("kb mode was %s\n"), m);
	if (oldkbmode != K_XLATE) {
		printf(_("[ if you are trying this under X, it might not work\n"
		         "since the X server is also reading /dev/console ]\n"));
	}
	printf("\n");
}

static void
clean_up(void)
{
	if (ioctl(fd, KDSKBMODE, oldkbmode)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl KDSKBMODE");
	}
	if (tcsetattr(fd, 0, &old) == -1)
		kbd_warning(errno, "tcsetattr");
	close(fd);
}

static void __attribute__((noreturn))
die(int x)
{
	printf(_("caught signal %d, cleaning up...\n"), x);
	clean_up();
	exit(EXIT_FAILURE);
}

static void __attribute__((noreturn))
watch_dog(int x __attribute__((unused)))
{
	clean_up();
	exit(EXIT_SUCCESS);
}

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;
	fprintf(stderr, _("Usage: %s [option...]\n"), get_progname());
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

int main(int argc, char *argv[])
{
	const char *short_opts          = "haskVt:";
	const struct option long_opts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "ascii", no_argument, NULL, 'a' },
		{ "scancodes", no_argument, NULL, 's' },
		{ "keycodes", no_argument, NULL, 'k' },
		{ "timeout", required_argument, NULL, 't' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	int c;
	int show_keycodes = 1;
	int print_ascii   = 0;
	int timeout = 10;

	struct termios new = { 0 };
	unsigned char buf[18]; /* divisible by 3 */
	int i;
	ssize_t n;

	set_progname(argv[0]);
	setuplocale();

	const struct kbd_help opthelp[] = {
		{ "-a, --ascii",     _("display the decimal/octal/hex values of the keys.") },
		{ "-s, --scancodes", _("display only the raw scan-codes.") },
		{ "-k, --keycodes",  _("display only the interpreted keycodes (default).") },
		{ "-t, --timeout",   _("set timeout, default 10")     },
		{ "-h, --help",      _("print this usage message.") },
		{ "-V, --version",   _("print version number.")     },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 's':
				show_keycodes = 0;
				break;
			case 'k':
				show_keycodes = 1;
				break;
			case 'a':
				print_ascii = 1;
				break;
			case 'V':
				print_version_and_exit();
				break;
			case 'h':
				usage(EXIT_SUCCESS, opthelp);
				break;
			case 't':
				timeout = atoi(optarg);
				/*  in anycase if conversion wrong */
				if (timeout < 1)
					timeout = 10;
				break;
			case '?':
				usage(EX_USAGE, opthelp);
				break;
		}
	}

	if (optind < argc)
		usage(EX_USAGE, opthelp);

	if (print_ascii) {
		/* no mode and signal and timer stuff - just read stdin */
		fd = 0;

		if (tcgetattr(fd, &old) == -1)
			kbd_warning(errno, "tcgetattr");
		if (tcgetattr(fd, &new) == -1)
			kbd_warning(errno, "tcgetattr");

		new.c_lflag &= ~((tcflag_t)(ICANON | ISIG));
		new.c_lflag |= (ECHO | ECHOCTL);
		new.c_iflag     = 0;
		new.c_cc[VMIN]  = 1;
		new.c_cc[VTIME] = 0;

		if (tcsetattr(fd, TCSAFLUSH, &new) == -1)
			kbd_warning(errno, "tcgetattr");
		printf(_("\nPress any keys - "
		         "Ctrl-D will terminate this program\n\n"));

		while (1) {
			n = read(fd, buf, 1);
			if (n == 1)
				printf(" \t%3d 0%03o 0x%02x\n",
				       buf[0], buf[0], buf[0]);
			if (n != 1 || buf[0] == 04)
				break;
		}

		if (tcsetattr(fd, 0, &old) == -1)
			kbd_warning(errno, "tcsetattr");
		return EXIT_SUCCESS;
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	/* the program terminates when there is no input for 10 secs */
	signal(SIGALRM, watch_dog);

	/*
	  if we receive a signal, we want to exit nicely, in
	  order not to leave the keyboard in an unusable mode
	*/
	signal(SIGHUP, die);
	signal(SIGINT, die);
	signal(SIGQUIT, die);
	signal(SIGILL, die);
	signal(SIGTRAP, die);
	signal(SIGABRT, die);
	signal(SIGIOT, die);
	signal(SIGFPE, die);
	signal(SIGKILL, die);
	signal(SIGUSR1, die);
	signal(SIGSEGV, die);
	signal(SIGUSR2, die);
	signal(SIGPIPE, die);
	signal(SIGTERM, die);
#ifdef SIGSTKFLT
	signal(SIGSTKFLT, die);
#endif
	signal(SIGCHLD, die);
	signal(SIGCONT, die);
	signal(SIGSTOP, die);
	signal(SIGTSTP, die);
	signal(SIGTTIN, die);
	signal(SIGTTOU, die);

	get_mode();
	if (tcgetattr(fd, &old) == -1)
		kbd_warning(errno, "tcgetattr");
	if (tcgetattr(fd, &new) == -1)
		kbd_warning(errno, "tcgetattr");

	new.c_lflag &= ~((tcflag_t)(ICANON | ECHO | ISIG));
	new.c_iflag     = 0;
	new.c_cc[VMIN]  = sizeof(buf);
	new.c_cc[VTIME] = 1; /* 0.1 sec intercharacter timeout */

	if (tcsetattr(fd, TCSAFLUSH, &new) == -1)
		kbd_warning(errno, "tcsetattr");
	if (ioctl(fd, KDSKBMODE, show_keycodes ? K_MEDIUMRAW : K_RAW)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl KDSKBMODE");
	}

	printf(_("press any key (program terminates %ds after last keypress)...\n"), timeout);

	/* show scancodes */
	if (!show_keycodes) {
		while (1) {
			alarm(timeout);
			n = read(fd, buf, sizeof(buf));
			for (i = 0; i < n; i++)
				printf("0x%02x ", buf[i]);
			printf("\n");
		}
		clean_up();
		return EXIT_SUCCESS;
	}

	/* show keycodes - 2.6 allows 3-byte reports */
	while (1) {
		alarm(timeout);
		n = read(fd, buf, sizeof(buf));
		i = 0;
		while (i < n) {
			int kc;
			const char *s;

			s = (buf[i] & 0x80) ? _("release") : _("press");

			if (i + 2 < n && (buf[i] & 0x7f) == 0 && (buf[i + 1] & 0x80) != 0 && (buf[i + 2] & 0x80) != 0) {
				kc = ((buf[i + 1] & 0x7f) << 7) |
				     (buf[i + 2] & 0x7f);
				i += 3;
			} else {
				kc = (buf[i] & 0x7f);
				i++;
			}
			printf(_("keycode %3d %s\n"), kc, s);
		}
	}

	clean_up();
	return EXIT_SUCCESS;
}
