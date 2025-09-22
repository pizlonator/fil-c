#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "libcommon.h"

int main(int argc, char **argv)
{
	int fd, indx, red, green, blue;
	unsigned char cmap[48];

	set_progname(argv[0]);
	setuplocale();

	if (argc != 5) {
		fprintf(stderr, "usage: %s index red green blue\n", get_progname());
		exit(EXIT_FAILURE);
	}

	indx  = atoi(argv[1]);
	red   = atoi(argv[2]);
	green = atoi(argv[3]);
	blue  = atoi(argv[4]);

	if (indx < 0 || red < 0 || green < 0 || blue < 0 ||
	    indx > 15 || red > 255 || green > 255 || blue > 255) {
		fprintf(stderr, "indx must be in 0..15, colors in 0..255\n");
		exit(EXIT_FAILURE);
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (ioctl(fd, GIO_CMAP, cmap))
		kbd_error(EXIT_FAILURE, errno, "ioctl GIO_CMAP");

	cmap[3 * indx]     = (unsigned char) red;
	cmap[3 * indx + 1] = (unsigned char) green;
	cmap[3 * indx + 2] = (unsigned char) blue;

	if (ioctl(fd, PIO_CMAP, cmap))
		kbd_error(EXIT_FAILURE, errno, "ioctl PIO_CMAP");

	return EXIT_SUCCESS;
}
