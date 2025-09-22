// SPDX-FileCopyrightText: 2024, Skyler Ferrante <sjf5462@rit.edu>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * To protect against file descriptor omission attacks, we open the std file
 * descriptors with /dev/null if they are not already open. Code is based on
 * fix_fds from sudo.c.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "prototypes.h"

static void check_fd(int fd);

void
check_fds(void)
{
	/**
	 * Make sure stdin, stdout, stderr are open
	 * If they are closed, set them to /dev/null
	 */
	check_fd(STDIN_FILENO);
	check_fd(STDOUT_FILENO);
	check_fd(STDERR_FILENO);
}

static void
check_fd(int fd)
{
	int  devnull;

	if (fcntl(fd, F_GETFL, 0) != -1)
		return;

	devnull = open("/dev/null", O_RDWR);
	if (devnull != fd)
		abort();
}
