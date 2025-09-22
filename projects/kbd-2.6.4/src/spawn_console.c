/*
 * Tiny test program for the `spawn console' key
 * (should not use signal; should not use sleep)
 * aeb - 941025
 *
 * Note: this functionality will probably go away and become
 * part of init. For the time being, be very careful when
 * you use this - if you have this in /etc/rc.local you should
 * start getty, not openvt, or anybody will have a root shell
 * with a single keystroke.
 */
#include "config.h"

#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <linux/kd.h>
#include <stdio.h>
#include <stdlib.h>    /* system */
#include <fcntl.h>     /* open */
#include <sys/ioctl.h> /* ioctl */
#include <unistd.h>    /* sleep */

#include "libcommon.h"

static void
sighup(int n __attribute__((unused)))
{
	if (system("openvt -s -l bash") == -1) {
		kbd_error(EXIT_FAILURE, errno, "system");
	}
	signal(SIGHUP, sighup);
}

int main(int argc __attribute__((unused)), char *argv[])
{
	int fd;

	set_progname(argv[0]);
	setuplocale();

	fd = open("/dev/tty0", 0);

	if (fd < 0 && errno == ENOENT)
		fd = open("/dev/vc/0", 0);

	if (fd < 0)
		fd = 0;

	signal(SIGHUP, sighup);

	if (ioctl(fd, KDSIGACCEPT, (long)SIGHUP)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl KDSIGACCEPT");
	}

	while (1)
		sleep(3600);

	return EXIT_SUCCESS;
}
