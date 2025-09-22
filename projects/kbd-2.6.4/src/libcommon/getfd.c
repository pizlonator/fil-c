#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "libcommon.h"

static const char *conspath[] = {
	"/proc/self/fd/0",
	"/dev/tty",
	"/dev/tty0",
	"/dev/vc/0",
	"/dev/systty",
	"/dev/console",
	NULL
};

/*
 * getfd.c
 *
 * Get an fd for use with kbd/console ioctls.
 * We try several things because opening /dev/console will fail
 * if someone else used X (which does a chown on /dev/console).
 */

static int
is_a_console(int fd)
{
	char arg;

	arg = 0;
	return (isatty(fd) && ioctl(fd, KDGKBTYPE, &arg) == 0 && ((arg == KB_101) || (arg == KB_84)));
}

static int
open_a_console(const char *fnam)
{
	int fd;

	/*
	 * For ioctl purposes we only need some fd and permissions
	 * do not matter. But setfont:activatemap() does a write.
	 */
	fd = open(fnam, O_RDWR);
	if (fd < 0)
		fd = open(fnam, O_WRONLY);
	if (fd < 0)
		fd = open(fnam, O_RDONLY);
	if (fd < 0)
		return -1;
	return fd;
}

int
getfd(const char *fnam)
{
	int fd, i;

	if (fnam) {
		if ((fd = open_a_console(fnam)) >= 0) {
			if (is_a_console(fd))
				return fd;
			close(fd);
		}
		fprintf(stderr, _("Couldn't open %s"), fnam);
		fprintf(stderr, "\n");
		exit(1);
	}

	for (i = 0; conspath[i]; i++) {
		if ((fd = open_a_console(conspath[i])) >= 0) {
			if (is_a_console(fd))
				return fd;
			close(fd);
		}
	}

	for (fd = 0; fd < 3; fd++)
		if (is_a_console(fd))
			return fd;

	fprintf(stderr,
	        _("Couldn't get a file descriptor referring to the console."));
	fprintf(stderr, "\n");

	/* total failure */
	exit(1);
}
