/*
 * Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main(int argc, char **argv)
{
	int r;

	if (argc != 2) {
		fprintf(stderr, "invalid options\n");
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "fallocate") == 0) {
		errno = 0;
		r = fallocate(-1, 0, 0, 0);
		errx(EXIT_SUCCESS, "fallocate r=%d errno=%s", r, strerror(errno));
	} else if (strcmp(argv[1], "exec") == 0) {
		char *const cmd[] = {
			"/bin/false",
			NULL
		};
		execve(cmd[0], cmd, NULL);
		err(EXIT_FAILURE, "exec failed");
	} else if (strcmp(argv[1], "ioctl") == 0) {
		r = ioctl(0, FIOCLEX);
		errx(EXIT_SUCCESS, "ioctl r=%d errno=%s", r, strerror(errno));
	}

	errx(EXIT_FAILURE, "invalid mode %s", argv[1]);
}
