/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcpcd.h"
#include "control.h"
#include "eloop.h"
#include "if.h"
#include "logerr.h"
#include "privsep.h"

#ifndef SUN_LEN
#define SUN_LEN(su) \
	    (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

static void control_handle_data(void *, unsigned short);

static void
control_queue_free(struct fd_list *fd)
{
	struct fd_data *fdp;

	while ((fdp = TAILQ_FIRST(&fd->queue))) {
		TAILQ_REMOVE(&fd->queue, fdp, next);
		if (fdp->data_size != 0)
			free(fdp->data);
		free(fdp);
	}

#ifdef CTL_FREE_LIST
	while ((fdp = TAILQ_FIRST(&fd->free_queue))) {
		TAILQ_REMOVE(&fd->free_queue, fdp, next);
		if (fdp->data_size != 0)
			free(fdp->data);
		free(fdp);
	}
#endif
}

void
control_free(struct fd_list *fd)
{

#ifdef PRIVSEP
	if (fd->ctx->ps_control_client == fd)
		fd->ctx->ps_control_client = NULL;
#endif

	eloop_event_delete(fd->ctx->eloop, fd->fd);
	close(fd->fd);
	TAILQ_REMOVE(&fd->ctx->control_fds, fd, next);
	control_queue_free(fd);
	free(fd);
}

static void
control_hangup(struct fd_list *fd)
{

#ifdef PRIVSEP
	if (IN_PRIVSEP(fd->ctx)) {
		if (ps_ctl_sendeof(fd) == -1)
			logerr(__func__);
	}
#endif
	control_free(fd);
}

static int
control_handle_read(struct fd_list *fd)
{
	char buffer[1024];
	ssize_t bytes;

	bytes = read(fd->fd, buffer, sizeof(buffer) - 1);
	if (bytes == -1)
		logerr(__func__);
	if (bytes == -1 || bytes == 0) {
		control_hangup(fd);
		return -1;
	}

#ifdef PRIVSEP
	if (IN_PRIVSEP(fd->ctx)) {
		ssize_t err;

		fd->flags |= FD_SENDLEN;
		err = ps_ctl_handleargs(fd, buffer, (size_t)bytes);
		fd->flags &= ~FD_SENDLEN;
		if (err == -1) {
			logerr(__func__);
			return 0;
		}
		if (err == 1 &&
		    ps_ctl_sendargs(fd, buffer, (size_t)bytes) == -1) {
			logerr(__func__);
			control_free(fd);
			return -1;
		}
		return 0;
	}
#endif

	control_recvdata(fd, buffer, (size_t)bytes);
	return 0;
}

static int
control_handle_write(struct fd_list *fd)
{
	struct iovec iov[2];
	int iov_len;
	struct fd_data *data;

	data = TAILQ_FIRST(&fd->queue);

	if (data->data_flags & FD_SENDLEN) {
		iov[0].iov_base = &data->data_len;
		iov[0].iov_len = sizeof(size_t);
		iov[1].iov_base = data->data;
		iov[1].iov_len = data->data_len;
		iov_len = 2;
	} else {
		iov[0].iov_base = data->data;
		iov[0].iov_len = data->data_len;
		iov_len = 1;
	}

	if (writev(fd->fd, iov, iov_len) == -1) {
		if (errno != EPIPE && errno != ENOTCONN) {
			// We don't get ELE_HANGUP for some reason
			logerr("%s: write", __func__);
		}
		control_hangup(fd);
		return -1;
	}

	TAILQ_REMOVE(&fd->queue, data, next);
#ifdef CTL_FREE_LIST
	TAILQ_INSERT_TAIL(&fd->free_queue, data, next);
#else
	if (data->data_size != 0)
		free(data->data);
	free(data);
#endif

	if (TAILQ_FIRST(&fd->queue) != NULL)
		return 0;

#ifdef PRIVSEP
	if (IN_PRIVSEP_SE(fd->ctx) && !(fd->flags & FD_LISTEN)) {
		if (ps_ctl_sendeof(fd) == -1)
			logerr(__func__);
	}
#endif

	/* Done sending data, stop watching write to fd */
	if (eloop_event_add(fd->ctx->eloop, fd->fd, ELE_READ,
	    control_handle_data, fd) == -1)
		logerr("%s: eloop_event_add", __func__);
	return 0;
}

static void
control_handle_data(void *arg, unsigned short events)
{
	struct fd_list *fd = arg;

	if (!(events & (ELE_READ | ELE_WRITE | ELE_HANGUP)))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	if (events & ELE_WRITE && !(events & ELE_HANGUP)) {
		if (control_handle_write(fd) == -1)
			return;
	}
	if (events & ELE_READ) {
		if (control_handle_read(fd) == -1)
			return;
	}
	if (events & ELE_HANGUP)
		control_hangup(fd);
}

void
control_recvdata(struct fd_list *fd, char *data, size_t len)
{
	char *p = data, *e;
	char *argvp[255], **ap;
	int argc;

	/* Each command is \n terminated
	 * Each argument is NULL separated */
	while (len != 0) {
		argc = 0;
		ap = argvp;
		while (len != 0) {
			if (*p == '\0') {
				p++;
				len--;
				continue;
			}
			e = memchr(p, '\0', len);
			if (e == NULL) {
				errno = EINVAL;
				logerrx("%s: no terminator", __func__);
				return;
			}
			if ((size_t)argc >= sizeof(argvp) / sizeof(argvp[0])) {
				errno = ENOBUFS;
				logerrx("%s: no arg buffer", __func__);
				return;
			}
			*ap++ = p;
			argc++;
			e++;
			len -= (size_t)(e - p);
			p = e;
			e--;
			if (*(--e) == '\n') {
				*e = '\0';
				break;
			}
		}
		if (argc == 0) {
			logerrx("%s: no args", __func__);
			continue;
		}
		*ap = NULL;
		if (dhcpcd_handleargs(fd->ctx, fd, argc, argvp) == -1) {
			logerr(__func__);
			if (errno != EINTR && errno != EAGAIN) {
				control_free(fd);
				return;
			}
		}
	}
}

struct fd_list *
control_new(struct dhcpcd_ctx *ctx, int fd, unsigned int flags)
{
	struct fd_list *l;

	l = malloc(sizeof(*l));
	if (l == NULL)
		return NULL;

	l->ctx = ctx;
	l->fd = fd;
	l->flags = flags;
	TAILQ_INIT(&l->queue);
#ifdef CTL_FREE_LIST
	TAILQ_INIT(&l->free_queue);
#endif
	TAILQ_INSERT_TAIL(&ctx->control_fds, l, next);
	return l;
}

static void
control_handle1(struct dhcpcd_ctx *ctx, int lfd, unsigned int fd_flags,
    unsigned short events)
{
	struct sockaddr_un run;
	socklen_t len;
	struct fd_list *l;
	int fd, flags;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = sizeof(run);
	if ((fd = accept(lfd, (struct sockaddr *)&run, &len)) == -1)
		goto error;
	if ((flags = fcntl(fd, F_GETFD, 0)) == -1 ||
	    fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
		goto error;
	if ((flags = fcntl(fd, F_GETFL, 0)) == -1 ||
	    fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		goto error;

#ifdef PRIVSEP
	if (IN_PRIVSEP(ctx) && !IN_PRIVSEP_SE(ctx))
		;
	else
#endif
	fd_flags |= FD_SENDLEN;

	l = control_new(ctx, fd, fd_flags);
	if (l == NULL)
		goto error;

	if (eloop_event_add(ctx->eloop, l->fd, ELE_READ,
	    control_handle_data, l) == -1)
		logerr("%s: eloop_event_add", __func__);
	return;

error:
	logerr(__func__);
	if (fd != -1)
		close(fd);
}

static void
control_handle(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	control_handle1(ctx, ctx->control_fd, 0, events);
}

static void
control_handle_unpriv(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	control_handle1(ctx, ctx->control_unpriv_fd, FD_UNPRIV, events);
}

static int
make_path(char *path, size_t len, const char *ifname, sa_family_t family,
    bool unpriv)
{
	const char *per;
	const char *sunpriv;

	switch(family) {
	case AF_INET:
		per = "-4";
		break;
	case AF_INET6:
		per = "-6";
		break;
	default:
		per = "";
		break;
	}
	if (unpriv)
		sunpriv = ifname ? ".unpriv" : "unpriv.";
	else
		sunpriv = "";
	return snprintf(path, len, CONTROLSOCKET,
	    ifname ? ifname : "", ifname ? per : "",
	    sunpriv, ifname ? "." : "");
}

static int
make_sock(struct sockaddr_un *sa, const char *ifname, sa_family_t family,
    bool unpriv)
{
	int fd;

	if ((fd = xsocket(AF_UNIX, SOCK_STREAM | SOCK_CXNB, 0)) == -1)
		return -1;
	memset(sa, 0, sizeof(*sa));
	sa->sun_family = AF_UNIX;
	make_path(sa->sun_path, sizeof(sa->sun_path), ifname, family, unpriv);
	return fd;
}

#define S_PRIV (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define S_UNPRIV (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

static int
control_start1(struct dhcpcd_ctx *ctx, const char *ifname, sa_family_t family,
    mode_t fmode)
{
	struct sockaddr_un sa;
	int fd;
	socklen_t len;

	fd = make_sock(&sa, ifname, family, (fmode & S_UNPRIV) == S_UNPRIV);
	if (fd == -1)
		return -1;

	len = (socklen_t)SUN_LEN(&sa);
	unlink(sa.sun_path);
	if (bind(fd, (struct sockaddr *)&sa, len) == -1 ||
	    chmod(sa.sun_path, fmode) == -1 ||
	    (ctx->control_group &&
	    chown(sa.sun_path, geteuid(), ctx->control_group) == -1) ||
	    listen(fd, sizeof(ctx->control_fds)) == -1)
	{
		close(fd);
		unlink(sa.sun_path);
		return -1;
	}

#ifdef PRIVSEP_RIGHTS
	if (IN_PRIVSEP(ctx) && ps_rights_limit_fd_fctnl(fd) == -1) {
		close(fd);
		unlink(sa.sun_path);
		return -1;
	}
#endif

	if ((fmode & S_UNPRIV) == S_UNPRIV)
		strlcpy(ctx->control_sock_unpriv, sa.sun_path,
		    sizeof(ctx->control_sock_unpriv));
	else
		strlcpy(ctx->control_sock, sa.sun_path,
		    sizeof(ctx->control_sock));
	return fd;
}

int
control_start(struct dhcpcd_ctx *ctx, const char *ifname, sa_family_t family)
{
	int fd;

#ifdef PRIVSEP
	if (IN_PRIVSEP_SE(ctx)) {
		make_path(ctx->control_sock, sizeof(ctx->control_sock),
		    ifname, family, false);
		make_path(ctx->control_sock_unpriv,
		    sizeof(ctx->control_sock_unpriv),
		    ifname, family, true);
		return 0;
	}
#endif

	if ((fd = control_start1(ctx, ifname, family, S_PRIV)) == -1)
		return -1;

	ctx->control_fd = fd;
	if (eloop_event_add(ctx->eloop, fd, ELE_READ,
	    control_handle, ctx) == -1)
		logerr("%s: eloop_event_add", __func__);

	if ((fd = control_start1(ctx, ifname, family, S_UNPRIV)) != -1) {
		ctx->control_unpriv_fd = fd;
		if (eloop_event_add(ctx->eloop, fd, ELE_READ,
		    control_handle_unpriv, ctx) == -1)
			logerr("%s: eloop_event_add", __func__);
	}
	return ctx->control_fd;
}

static int
control_unlink(struct dhcpcd_ctx *ctx, const char *file)
{
	int retval = 0;

	errno = 0;
#ifdef PRIVSEP
	if (IN_PRIVSEP(ctx))
		retval = (int)ps_root_unlink(ctx, file);
	else
#else
		UNUSED(ctx);
#endif
		retval = unlink(file);

	return retval == -1 && errno != ENOENT ? -1 : 0;
}

int
control_stop(struct dhcpcd_ctx *ctx)
{
	int retval = 0;
	struct fd_list *l;

	while ((l = TAILQ_FIRST(&ctx->control_fds)) != NULL) {
		control_free(l);
	}

#ifdef PRIVSEP
	if (IN_PRIVSEP_SE(ctx)) {
		if (ctx->control_sock[0] != '\0' &&
		    ps_root_unlink(ctx, ctx->control_sock) == -1)
			retval = -1;
		if (ctx->control_sock_unpriv[0] != '\0' &&
		    ps_root_unlink(ctx, ctx->control_sock_unpriv) == -1)
			retval = -1;
		return retval;
	} else if (ctx->options & DHCPCD_FORKED)
		return retval;
#endif

	if (ctx->control_fd != -1) {
		eloop_event_delete(ctx->eloop, ctx->control_fd);
		close(ctx->control_fd);
		ctx->control_fd = -1;
		if (control_unlink(ctx, ctx->control_sock) == -1)
			retval = -1;
	}

	if (ctx->control_unpriv_fd != -1) {
		eloop_event_delete(ctx->eloop, ctx->control_unpriv_fd);
		close(ctx->control_unpriv_fd);
		ctx->control_unpriv_fd = -1;
		if (control_unlink(ctx, ctx->control_sock_unpriv) == -1)
			retval = -1;
	}

	return retval;
}

int
control_open(const char *ifname, sa_family_t family, bool unpriv)
{
	struct sockaddr_un sa;
	int fd;

	if ((fd = make_sock(&sa, ifname, family, unpriv)) != -1) {
		socklen_t len;

		len = (socklen_t)SUN_LEN(&sa);
		if (connect(fd, (struct sockaddr *)&sa, len) == -1) {
			close(fd);
			fd = -1;
		}
	}
	return fd;
}

ssize_t
control_send(struct dhcpcd_ctx *ctx, int argc, char * const *argv)
{
	char buffer[1024];
	int i;
	size_t len, l;

	if (argc > 255) {
		errno = ENOBUFS;
		return -1;
	}
	len = 0;
	for (i = 0; i < argc; i++) {
		l = strlen(argv[i]) + 1;
		if (len + l > sizeof(buffer)) {
			errno = ENOBUFS;
			return -1;
		}
		memcpy(buffer + len, argv[i], l);
		len += l;
	}
	return write(ctx->control_fd, buffer, len);
}

int
control_queue(struct fd_list *fd, void *data, size_t data_len)
{
	struct fd_data *d;
	unsigned short events;

	if (data_len == 0) {
		errno = EINVAL;
		return -1;
	}

#ifdef CTL_FREE_LIST
	struct fd_data *df;

	d = NULL;
	TAILQ_FOREACH(df, &fd->free_queue, next) {
		if (d == NULL || d->data_size < df->data_size) {
			d = df;
			if (d->data_size <= data_len)
				break;
		}
	}
	if (d != NULL)
		TAILQ_REMOVE(&fd->free_queue, d, next);
	else
#endif
	{
		d = calloc(1, sizeof(*d));
		if (d == NULL)
			return -1;
	}

	if (d->data_size == 0)
		d->data = NULL;
	if (d->data_size < data_len) {
		void *nbuf = realloc(d->data, data_len);
		if (nbuf == NULL) {
			free(d->data);
			free(d);
			return -1;
		}
		d->data = nbuf;
		d->data_size = data_len;
	}
	memcpy(d->data, data, data_len);
	d->data_len = data_len;
	d->data_flags = fd->flags & FD_SENDLEN;

	TAILQ_INSERT_TAIL(&fd->queue, d, next);
	events = ELE_WRITE;
	if (fd->flags & FD_LISTEN)
		events |= ELE_READ;
	return eloop_event_add(fd->ctx->eloop, fd->fd, events,
	    control_handle_data, fd);
}
