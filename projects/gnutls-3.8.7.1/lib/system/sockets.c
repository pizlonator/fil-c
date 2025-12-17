/*
 * Copyright (C) 2010-2016 Free Software Foundation, Inc.
 * Copyright (C) 2015-2016 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#include "config.h"
#include "system.h"
#include "gnutls_int.h"
#include "errors.h"

#include <sys/socket.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#else /* !_WIN32 */
#include <poll.h>
#endif

/* System specific socket function wrappers.
 */

#ifdef _WIN32
/* Do not use the gnulib functions for sending and receiving data.
 * Using them makes gnutls only working with gnulib applications.
 */
#undef send
#undef recv
#undef select

int system_errno(gnutls_transport_ptr p)
{
	int tmperr = WSAGetLastError();
	int ret = 0;
	switch (tmperr) {
	case WSAEWOULDBLOCK:
		ret = EAGAIN;
		break;
	case NO_ERROR:
		ret = 0;
		break;
	case WSAEINTR:
		ret = EINTR;
		break;
	case WSAEMSGSIZE:
		ret = EMSGSIZE;
		break;
	default:
		ret = EIO;
		break;
	}
	WSASetLastError(tmperr);

	return ret;
}

ssize_t system_write(gnutls_transport_ptr ptr, const void *data,
		     size_t data_size)
{
	return send(GNUTLS_POINTER_TO_INT(ptr), data, data_size, 0);
}

ssize_t system_writev(gnutls_transport_ptr_t ptr, const giovec_t *iovec,
		      int iovec_cnt)
{
	WSABUF bufs[32];
	DWORD bytes_sent;
	DWORD to_send_cnt = 0;
	size_t to_send_bytes = 0;

	if ((size_t)iovec_cnt > sizeof(bufs) / sizeof(bufs[0]))
		iovec_cnt = sizeof(bufs) / sizeof(bufs[0]);

	while (to_send_cnt < (DWORD)iovec_cnt && to_send_bytes < SSIZE_MAX) {
		bufs[to_send_cnt].buf = iovec[to_send_cnt].iov_base;

		if (to_send_bytes + iovec[to_send_cnt].iov_len > SSIZE_MAX) {
			/* Return value limit: successful result value cannot
			 * exceed SSIZE_MAX */
			size_t space_left = (size_t)SSIZE_MAX - to_send_bytes;
			bufs[to_send_cnt].len =
				(unsigned long)(space_left > ULONG_MAX ?
							ULONG_MAX :
							space_left);
			to_send_cnt++;
			break;
		}
#ifdef _WIN64
		if (iovec[to_send_cnt].iov_len > ULONG_MAX) {
			/* WSASend() limitation */
			bufs[to_send_cnt].len = ULONG_MAX;
			to_send_cnt++;
			break;
		}
#endif
		bufs[to_send_cnt].len =
			(unsigned long)iovec[to_send_cnt].iov_len;
		to_send_bytes += iovec[to_send_cnt].iov_len;
		to_send_cnt++;
	}

	if (WSASend(GNUTLS_POINTER_TO_INT(ptr), bufs, to_send_cnt, &bytes_sent,
		    0, NULL, NULL) != 0)
		return -1;

	return (ssize_t)bytes_sent;
}

#else /* POSIX */
int system_errno(gnutls_transport_ptr_t ptr)
{
#if defined(_AIX) || defined(AIX)
	if (errno == 0)
		errno = EAGAIN;
#endif

	return errno;
}

static ssize_t _system_writev(gnutls_transport_ptr_t ptr, const giovec_t *iovec,
			      int iovec_cnt, int flags)
{
	struct msghdr hdr;

	memset(&hdr, 0, sizeof(hdr));
	hdr.msg_iov = (struct iovec *)iovec;
	hdr.msg_iovlen = iovec_cnt;

	return sendmsg(GNUTLS_POINTER_TO_INT(ptr), &hdr, flags);
}

#ifdef MSG_NOSIGNAL
ssize_t system_writev_nosignal(gnutls_transport_ptr_t ptr,
			       const giovec_t *iovec, int iovec_cnt)
{
	return _system_writev(ptr, iovec, iovec_cnt, MSG_NOSIGNAL);
}

#endif

ssize_t system_writev(gnutls_transport_ptr_t ptr, const giovec_t *iovec,
		      int iovec_cnt)
{
	return _system_writev(ptr, iovec, iovec_cnt, 0);
}

#endif

ssize_t system_read(gnutls_transport_ptr_t ptr, void *data, size_t data_size)
{
	return recv(GNUTLS_POINTER_TO_INT(ptr), data, data_size, 0);
}

/**
 * gnutls_system_recv_timeout:
 * @ptr: A file descriptor (wrapped in a gnutls_transport_ptr_t pointer)
 * @ms: The number of milliseconds to wait.
 *
 * Wait for data to be received from the provided socket (@ptr) within a
 * timeout period in milliseconds, using select() on the provided @ptr.
 *
 * This function is provided as a helper for constructing custom
 * callbacks for gnutls_transport_set_pull_timeout_function(),
 * which can be used if you rely on socket file descriptors.
 *
 * Returns -1 on error, 0 on timeout, positive value if data are available for reading.
 *
 * Since: 3.4.0
 **/
int gnutls_system_recv_timeout(gnutls_transport_ptr_t ptr, unsigned int ms)
{
	int ret;
	int fd = GNUTLS_POINTER_TO_INT(ptr);
#ifndef _WIN32
	int timeo;
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	if (ms == GNUTLS_INDEFINITE_TIMEOUT)
		timeo = -1;
	else
		timeo = ms;
	do {
		ret = poll(&pfd, 1, timeo);
	} while (ret == -1 && errno == EINTR);
#else
	fd_set rfds;
	TIMEVAL _tv, *tv = NULL;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	if (ms != GNUTLS_INDEFINITE_TIMEOUT) {
		_tv.tv_sec = ms / 1000;
		_tv.tv_usec = (ms % 1000) * 1000;
		tv = &_tv;
	}

	ret = select(fd + 1, &rfds, NULL, NULL, tv);
#endif
	if (ret <= 0)
		return ret;

	return ret;
}
