/*
 * Tiny test program for the `spawn console' key
 * (should not use signal; should not use sleep)
 * aeb - 941025
 *
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "libcommon.h"

static void
sighup(int n __attribute__((unused)))
{
	if (system("openvt -s -l -- login -h spawn") == -1) {
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

	if (ioctl(fd, KDSIGACCEPT, (long)SIGHUP))
		kbd_error(EXIT_FAILURE, errno, "ioctl KDSIGACCEPT");

	while (1)
		sleep(3600);

	return EXIT_SUCCESS;
}
