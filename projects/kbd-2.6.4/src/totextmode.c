/*
 * totextmode.c - aeb - 2000-01-20
 */
#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "libcommon.h"

int main(int argc, char *argv[])
{
	int fd, num;

	set_progname(argv[0]);
	setuplocale();

	if (argc == 2 && !strcmp(argv[1], "-V"))
		print_version_and_exit();

	if (argc != 2) {
		kbd_error(EXIT_FAILURE, 0, _("Usage: %s [option...]\n"), get_progname());
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	num = atoi(argv[1]);
	if (ioctl(fd, KDSETMODE, KD_TEXT)) {
		kbd_error(EXIT_FAILURE, errno, "totextmode: KDSETMODE");
	}
	return EXIT_SUCCESS;
}
