
/*

  Set and restore VC screen for vlock, the VT locking program for linux.

  Copyright (C) 1997-1998  Juan Cespedes <cespedes@debian.org>
  Copyright (C) 2002, 2004, 2006  Dmitry V. Levin <ldv@altlinux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
*/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "vlock.h"

static unsigned char lines, columns;
static void *screen_buf = 0;
static int vcs          = -1;

void init_screen(void)
{
	int failed             = 1;
	const char clear_str[] = "\33[3J\33[H\33[J";

	vcs = -1;
	do {
		struct stat st;
		unsigned i, size;
		char path[16];

		if (fstat(STDIN_FILENO, &st) == -1)
			break;

		if (!S_ISCHR(st.st_mode))
			break;

		if ((st.st_rdev >> 8) != 4)
			break;

		i = st.st_rdev & 0xff;
		sprintf(path, "/dev/vcsa%u", i);
		vcs = open(path, O_RDWR);
		if (vcs < 0)
			break;

		if (read(vcs, &columns, 1) != 1)
			break;

		if (read(vcs, &lines, 1) != 1)
			break;

		size       = (unsigned) 2 * lines * columns + 2;
		screen_buf = malloc(size);
		if (!screen_buf)
			break;

		if (read(vcs, screen_buf, size) != (int)size) {
			free(screen_buf);
			screen_buf = NULL;
			break;
		}

		failed = 0;
	} while (0);

	if (failed && vcs >= 0) {
		close(vcs);
		vcs = -1;
	}

	/* There's no need to use ncurses */
	if (write(STDOUT_FILENO, clear_str, sizeof(clear_str) - 1) !=
	    sizeof(clear_str) - 1)
		return;
}

void restore_screen(void)
{
	if (screen_buf) {
		do {
			if (lseek(vcs, 0, SEEK_SET))
				break;
			if (write(vcs, &columns, 1) != 1)
				break;
			if (write(vcs, &lines, 1) != 1)
				break;
			if (write(vcs, screen_buf, (unsigned) 2 * lines * columns + 2) !=
			    2 * lines * columns + 2)
				break;
		} while (0);
		close(vcs);
	}
}
