/*
 * setvesablank.c - aeb - 941230
 *
 * usage: setvesablank ON|on|off
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "libcommon.h"

int main(int argc, char *argv[])
{
	int fd;
	struct {
		char ten, onoff;
	} arg;

	set_progname(argv[0]);
	setuplocale();

	if (argc != 2) {
		fprintf(stderr, _("Usage: %s ON|on|off\n"), get_progname());
		return EXIT_FAILURE;
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	arg.ten   = 10;
	arg.onoff = 0;

	if (!strcmp(argv[1], "on"))
		arg.onoff = 1;
	else if (!strcmp(argv[1], "ON"))
		arg.onoff = 2;

	if (ioctl(fd, TIOCLINUX, &arg)) {
		kbd_error(EXIT_FAILURE, errno, "setvesablank: TIOCLINUX");
	}

	return EXIT_SUCCESS;
}
