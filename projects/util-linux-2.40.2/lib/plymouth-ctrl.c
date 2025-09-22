/*
 * plymouth-ctrl.c	Simply communications with plymouthd
 *			to avoid forked sub processes and/or
 *			missed plymouth send commands tool
 *			due a plymouthd replacement.
 *
 * Copyright (c) 2016 SUSE Linux GmbH, All rights reserved.
 * Copyright (c) 2016 Werner Fink <werner@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Author: Werner Fink <werner@suse.de>
 */

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "all-io.h"
#include "c.h"
#include "nls.h"
#include "plymouth-ctrl.h"

static int can_read(int fd, const long timeout)
{
	struct pollfd fds = {
		.fd = fd,
		.events = POLLIN|POLLPRI,
		.revents = 0,
	};
	int ret;

	do {
		ret = poll(&fds, 1, timeout);
	} while ((ret < 0) && (errno == EINTR));

	return (ret == 1) && (fds.revents & (POLLIN|POLLPRI));
}

static int open_un_socket_and_connect(void)
{
	/* The abstract UNIX socket of plymouth */
	struct sockaddr_un su = {
		.sun_family = AF_UNIX,
		.sun_path = PLYMOUTH_SOCKET_PATH,
	};
	const int one = 1;
	int fd, ret;

	fd = socket(PF_UNIX, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);
	if (fd < 0) {
		warnx(_("cannot open UNIX socket"));
		goto err;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, sizeof(one));
	if (ret < 0) {
		warnx(_("cannot set option for UNIX socket"));
		close(fd);
		fd = -1;
		goto err;
	}

	/* Note, the abstract PLYMOUTH_SOCKET_PATH has a leading NULL byte */
	ret = connect(fd, (struct sockaddr *) &su,
		offsetof(struct sockaddr_un, sun_path) + 1 + strlen(su.sun_path+1));
	if (ret < 0) {
		if (errno != ECONNREFUSED)
			warnx(_("cannot connect on UNIX socket"));
		close(fd);
		fd = -1;
		goto err;
	}
err:
	return fd;
}

int plymouth_command(int cmd, ...)
{
	uint8_t answer[2], command[2];
	struct sigaction sp, op;
	int fdsock = -1, ret = 0;

	sigemptyset (&sp.sa_mask);
	sp.sa_handler = SIG_IGN;
	sp.sa_flags = SA_RESTART;
	sigaction(SIGPIPE, &sp, &op);

	/* The plymouthd does read at least two bytes. */
	command[1] = '\0';
	switch (cmd) {
	case MAGIC_PING:
		fdsock = open_un_socket_and_connect();
		if (fdsock >= 0) {
			command[0] = cmd;
			write_all(fdsock, command, sizeof(command));
		}
		break;
	case MAGIC_QUIT:
		fdsock = open_un_socket_and_connect();
		if (fdsock >= 0) {
			command[0] = cmd;
			write_all(fdsock, command, sizeof(command));
		}
		break;
	default:
		warnx(_("the plymouth request %c is not implemented"), cmd);
	case '?':
		goto err;
	}

	answer[0] = '\0';
	if (fdsock >= 0) {
		if (can_read(fdsock, 1000))
			read_all(fdsock, (char *) &answer[0], sizeof(answer));
		close(fdsock);
	}
	sigaction(SIGPIPE, &op, NULL);
	ret = (answer[0] == ANSWER_ACK) ? 1 : 0;
err:
	return ret;
}

