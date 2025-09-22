/*
From: faith@cs.unc.edu (Rik Faith)
Subject: User mode keyboard rate changer
Date: 27 Apr 92 13:44:26 GMT

I put together this program, called kbdrate.c, which will reset the keyboard
repeat rate and delay in user mode.  The program must have read/write
access to /dev/port, so if /dev/port is only read/writeable by group port,
then kbdrate must run setgid to group port (for example).

The "rate" is the rate in characters per second

The "delay" is the amount of time the key must remain depressed before it
will start to repeat.

Usage examples:

kbdrate                 set rate to IBM default (10.9 cps, 250ms delay)
kbdrate -r 30.0         set rate to 30 cps and delay to 250ms
kbdrate -r 20.0 -s      set rate to 20 cps (delay 250ms) -- don't print message
kbdrate -r 0 -d 0       set rate to 2.0 cps and delay to 250 ms

I find it useful to put kbdrate in my /etc/rc file so that the keyboard
rate is set to something that I find comfortable at boot time.  This sure
beats rebuilding the kernel!


  kbdrate.c -- Set keyboard typematic rate (and delay)
  Created: Thu Apr 23 12:24:30 1992
  Author: Rickard E. Faith, faith@cs.unc.edu

  Copyright 1992 Rickard E. Faith.  Distributed under the GPL.
  This program comes with ABSOLUTELY NO WARRANTY.
  Usage: kbdrate [-r rate] [-d delay] [-s]
         Rate can range from 2.0 to 30.0 (units are characters per second)
         Delay can range from 250 to 1000 (units are milliseconds)
         -s suppressed message
  Compiles under gcc 2.1 for Linux (tested with the pre-0.96 kernel)
 
  Wed Jun 22 21:35:43 1994, faith@cs.unc.edu:
            Changed valid_rates per suggestion by Andries.Brouwer@cwi.nl.
  Wed Jun 22 22:18:29 1994, faith@cs.unc.edu:
            Added patch for AUSTIN notebooks from John Bowman
            (bowman@hagar.ph.utexas.edu)
 
  Linux/68k modifications by Roman Hodek 
 				(Roman.Hodek@informatik.uni-erlangen.de):
 
  Reading/writing the Intel I/O ports via /dev/port is not the
  English way... Such hardware dependent stuff can never work on
  other architectures.
  
  Linux/68k has an new ioctl for setting the keyboard repeat rate
  and delay. Both values are counted in msecs, the kernel will do
  any rounding to values possible with the underlying hardware.
 
  kbdrate now first tries if the KDKBDREP ioctl is available. If it
  is, it is used, else the old method is applied.

  1999-02-22 Arkadiusz Miśkiewicz <misiek@misiek.eu.org>
  - added Native Language Support

  1999-03-17
  Linux/SPARC modifications by Jeffrey Connell <ankh@canuck.gen.nz>:
  It seems that the KDKBDREP ioctl is not available on this platform.
  However, Linux/SPARC has its own ioctl for this (since 2.1.30),
  with yet another measurement system.  Thus, try for KIOCSRATE, too.

*/
#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sysexits.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#ifdef __sparc__
#include <asm/param.h>
#endif

#ifdef COMPAT_HEADERS
#include "compat/linux-kd.h"
#endif

/* Equal to kernel version, but field names vary. */
struct my_kbd_repeat {
	int delay;  /* in msec; <= 0: don't change */
	int period; /* in msec; <= 0: don't change */
	            /* earlier this field was misnamed "rate" */
};

#include <signal.h>

#include "libcommon.h"

static int valid_rates[] = { 300, 267, 240, 218, 200, 185, 171, 160, 150,
	                     133, 120, 109, 100, 92, 86, 80, 75, 67,
	                     60, 55, 50, 46, 43, 40, 37, 33, 30, 27,
	                     25, 23, 21, 20 };
#define RATE_COUNT (sizeof(valid_rates) / sizeof(int))

static int valid_delays[] = { 250, 500, 750, 1000 };
#define DELAY_COUNT (sizeof(valid_delays) / sizeof(int))

static int print_only = 0;

static int
KDKBDREP_ioctl_ok(double rate, int delay, int silent)
{
	/*
	 * This ioctl is defined in <linux/kd.h> but is not
	 * implemented anywhere - must be in some m68k patches.
	 * Since 2.4.9 also on i386.
	 */
	struct my_kbd_repeat kbdrep_s;

	/* don't change, just test */
	kbdrep_s.period = -1;
	kbdrep_s.delay  = -1;

	if (ioctl(0, KDKBDREP, &kbdrep_s)) {
		if (errno == EINVAL || errno == ENOTTY)
			return 0;
		kbd_error(EXIT_FAILURE, errno, "ioctl KDKBDREP");
	}

	if (print_only) {
		rate = (kbdrep_s.period > 0)
			? 1000.0 / (double)kbdrep_s.period
			: 0;

		printf(_("Typematic Rate is %.1f cps\n"), rate);
		printf(_("Current keyboard delay %d ms\n"), kbdrep_s.delay);
		printf(_("Current keyboard period %d ms\n"), kbdrep_s.period);

		return 1;
	}

	printf("old delay %d, period %d\n", kbdrep_s.delay, kbdrep_s.period);

	/* do the change */
	kbdrep_s.period = (rate != 0)
		? (int) (1000.0 / rate) /* convert cps to msec */
		: 0;                    /* switch repeat off */

	if (kbdrep_s.period < 1)
		kbdrep_s.period = 1;

	kbdrep_s.delay = delay;

	if (kbdrep_s.delay < 1)
		kbdrep_s.delay = 1;

	if (ioctl(0, KDKBDREP, &kbdrep_s))
		kbd_error(EXIT_FAILURE, errno, "ioctl KDKBDREP");

	/* report */
	kbdrep_s.period = -1;
	kbdrep_s.delay  = -1;

	if (ioctl(0, KDKBDREP, &kbdrep_s)) {
		if (errno == EINVAL)
			return 0;
		kbd_error(EXIT_FAILURE, errno, "ioctl KDKBDREP");
	}

	if (!silent) {
		rate = (kbdrep_s.period != 0)
			? 1000.0 / (double)kbdrep_s.period
			: 0;

		printf(_("Typematic Rate set to %.1f cps (delay = %d ms)\n"),
		       rate, kbdrep_s.delay);
	}

	return 1; /* success! */
}

#ifdef KIOCSRATE
static int
KIOCSRATE_ioctl_ok(double rate, int delay, int silent)
{
	struct kbd_rate kbdrate_s;
	int fd;

	fd = open("/dev/kbd", O_RDONLY);
	if (fd == -1)
		kbd_error(EXIT_FAILURE, errno, "open /dev/kbd");

	if (print_only) {
		kbdrate_s.rate  = 0;
		kbdrate_s.delay = 0;

		if (ioctl(fd, KIOCGRATE, &kbdrate_s))
			kbd_error(EXIT_FAILURE, errno, "ioctl KIOCGRATE");

		printf(_("Typematic Rate is %.1f cps\n"), kbdrep_s.rate);
		printf(_("Current keyboard delay %d ms\n"), kbdrate_s.delay * 1000 / HZ);

		return 1;
	}

	kbdrate_s.rate  = (int)(rate + 0.5); /* round up */
	kbdrate_s.delay = delay * HZ / 1000; /* convert ms to Hz */

	if (kbdrate_s.rate > 50)
		kbdrate_s.rate = 50;

	if (ioctl(fd, KIOCSRATE, &kbdrate_s))
		kbd_error(EXIT_FAILURE, errno, "ioctl KIOCSRATE");

	close(fd);

	if (!silent)
		printf("Typematic Rate set to %d cps (delay = %d ms)\n",
		       kbdrate_s.rate, kbdrate_s.delay * 1000 / HZ);

	return 1;
}
#else
#	define KIOCSRATE_ioctl_ok(a,b,c) (0)
#endif /* KIOCSRATE */

static void
sigalrmhandler(int sig __attribute__((unused)))
{
	kbd_warning(0, "Failed waiting for kbd controller!\n");
	raise(SIGINT);
}

static int
ioport_set(double rate, int delay, int silent)
{
	int value = 0x7f; /* Maximum delay with slowest rate */
	                  /* DO NOT CHANGE this value */
	int fd;
	char data;
	int i;

	if (print_only) {
		printf(_("Not supported\n"));
		return 0;
	}

	/* https://wiki.osdev.org/PS/2_Keyboard */

	for (i = 0; i < (int) RATE_COUNT; i++)
		if (rate * 10 >= valid_rates[i]) {
			value &= 0x60;
			value |= i;
			break;
		}

	for (i = 0; i < (int) DELAY_COUNT; i++)
		if (delay <= valid_delays[i]) {
			value &= 0x1f;
			value |= i << 5;
			break;
		}

	if ((fd = open("/dev/port", O_RDWR)) < 0)
		kbd_error(EXIT_FAILURE, errno, _("Cannot open /dev/port"));

	signal(SIGALRM, sigalrmhandler);
	alarm(3);

	do {
		lseek(fd, 0x64, 0);
		if (read(fd, &data, 1) == -1)
			kbd_error(EXIT_FAILURE, errno, "read");
	} while ((data & 2) == 2); /* wait */

	lseek(fd, 0x60, 0);
	data = (char) 0xf3; /* set typematic rate */

	if (write(fd, &data, 1) == -1)
		kbd_error(EXIT_FAILURE, errno, "write");

	do {
		lseek(fd, 0x64, 0);
		if (read(fd, &data, 1) == -1)
			kbd_error(EXIT_FAILURE, errno, "read");
	} while ((data & 2) == 2); /* wait */

	alarm(0);

	lseek(fd, 0x60, 0);
	sleep(1);

	if (write(fd, &value, 1) == -1)
		kbd_error(EXIT_FAILURE, errno, "write");

	close(fd);

	if (!silent)
		printf(_("Typematic Rate set to %.1f cps (delay = %d ms)\n"),
		       valid_rates[value & 0x1f] / 10.0,
		       valid_delays[(value & 0x60) >> 5]);

	return 1;
}

#ifdef __sparc__
double rate = 5.0; /* Default rate */
int delay   = 200; /* Default delay */
#else
double rate = 10.9; /* Default rate */
int delay   = 250; /* Default delay */
#endif

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;

	fprintf(stderr, _("Usage: %s [option...]\n"), get_progname());
	fprintf(stderr, "\n");
	fprintf(stderr, _("The program sets the keyboard repeat rate and delay in user mode.\n"));

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

int main(int argc, char **argv)
{
	int silent = 0;
	int c;

	set_progname(argv[0]);
	setuplocale();

	const char *short_opts = "r:d:pshV";
	const struct option long_opts[] = {
		{ "rate", required_argument, NULL, 'r' },
		{ "delay", required_argument, NULL, 'd' },
		{ "print", no_argument, NULL, 'p' },
		{ "silent", no_argument, NULL, 's' },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-r, --rate=NUM",    _("set the rate in characters per second.") },
		{ "-d, --delay=NUM",   _("set the amount of time the key must remain depressed before it will start to repeat.") },
		{ "-p, --print",       _("do not set new values, but only display the current ones.") },
		{ "-s, --silent",      _("suppress all normal output.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			case 'r':
				rate = atof(optarg);
				break;
			case 'd':
				delay = atoi(optarg);
				break;
			case 'p':
				print_only = 1;
				break;
			case 's':
				silent = 1;
				break;
			case 'V':
				print_version_and_exit();
				break;
			case 'h':
				usage(EXIT_SUCCESS, opthelp);
			case '?':
				usage(EX_USAGE, opthelp);
		}
	}

	if (KDKBDREP_ioctl_ok(rate, delay, silent)) /* m68k/i386? */
		return EXIT_SUCCESS;

	if (KIOCSRATE_ioctl_ok(rate, delay, silent)) /* sparc? */
		return EXIT_SUCCESS;

	if (ioport_set(rate, delay, silent)) /* The ioport way */
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}
