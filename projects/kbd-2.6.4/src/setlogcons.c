/*
 * setlogcons.c - aeb - 000523
 *
 * usage: setlogcons N
 */

/* Send kernel messages to the current console or to console N */
#include "config.h"

#include <linux/tiocl.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <sysexits.h>

#include "libcommon.h"

int main(int argc, char **argv)
{
	int fd, cons;
	struct {
		char fn, subarg;
	} arg;

	set_progname(argv[0]);
	setuplocale();

	cons = 0; /* current console */

	if (argc == 2) {
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			fprintf(stderr, "Usage: %s [N]\n", get_progname());
			fprintf(stderr, "Send kernel messages to the current console or to console N\n");
			return EX_USAGE;
		}

		if (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version"))
			print_version_and_exit();

		cons = atoi(argv[1]);
	}

	if (cons < 0)
		kbd_error(EX_DATAERR, 0, "Argument must be positive: %s",
				(argc == 2) ? argv[1] : "0");

	if (cons > CHAR_MAX)
		kbd_error(EX_DATAERR, 0, "Argument is too big: %s",
				(argc == 2) ? argv[1] : "0");

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EX_OSERR, 0, _("Couldn't get a file descriptor referring to the console."));

	arg.fn     = TIOCL_SETKMSGREDIRECT; /* redirect kernel messages */
	arg.subarg = (char) cons;           /* to specified console */

	if (ioctl(fd, TIOCLINUX, &arg))
		kbd_error(EX_OSERR, errno, "TIOCLINUX");

	return EX_OK;
}
