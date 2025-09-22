/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd, control proxy
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dhcpcd.h"
#include "control.h"
#include "eloop.h"
#include "logerr.h"
#include "privsep.h"

/* We expect to have open 2 SEQPACKET, 2 STREAM and 2 file STREAM fds */

static int
ps_ctl_startcb(struct ps_process *psp)
{
	struct dhcpcd_ctx *ctx = psp->psp_ctx;
	sa_family_t af;

	if (ctx->options & DHCPCD_MANAGER) {
		setproctitle("[control proxy]");
		af = AF_UNSPEC;
	} else {
		setproctitle("[control proxy] %s%s%s",
		    ctx->ifv[0],
		    ctx->options & DHCPCD_IPV4 ? " [ip4]" : "",
		    ctx->options & DHCPCD_IPV6 ? " [ip6]" : "");
		if ((ctx->options &
		    (DHCPCD_IPV4 | DHCPCD_IPV6)) == DHCPCD_IPV4)
			af = AF_INET;
		else if ((ctx->options &
		    (DHCPCD_IPV4 | DHCPCD_IPV6)) == DHCPCD_IPV6)
			af = AF_INET6;
		else
			af = AF_UNSPEC;
	}

	return control_start(ctx,
	    ctx->options & DHCPCD_MANAGER ? NULL : *ctx->ifv, af);
}

static void
ps_ctl_recvmsg(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events, NULL, NULL) == -1)
		logerr(__func__);
}

ssize_t
ps_ctl_handleargs(struct fd_list *fd, char *data, size_t len)
{

	/* Make any change here in dhcpcd.c as well. */
	if (strncmp(data, "--version",
	    MIN(strlen("--version"), len)) == 0) {
		return control_queue(fd, UNCONST(VERSION),
		    strlen(VERSION) + 1);
	} else if (strncmp(data, "--getconfigfile",
	    MIN(strlen("--getconfigfile"), len)) == 0) {
		return control_queue(fd, UNCONST(fd->ctx->cffile),
		    strlen(fd->ctx->cffile) + 1);
	} else if (strncmp(data, "--listen",
	    MIN(strlen("--listen"), len)) == 0) {
		fd->flags |= FD_LISTEN;
		return 0;
	}

	if (fd->ctx->ps_control_client != NULL &&
	    fd->ctx->ps_control_client != fd)
	{
		logerrx("%s: cannot handle another client", __func__);
		return 0;
	}
	return 1;
}

static ssize_t
ps_ctl_dispatch(void *arg, struct ps_msghdr *psm, struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = arg;
	struct iovec *iov = msg->msg_iov;
	struct fd_list *fd;
	unsigned int fd_flags = FD_SENDLEN;

	switch (psm->ps_flags) {
	case PS_CTL_PRIV:
		break;
	case PS_CTL_UNPRIV:
		fd_flags |= FD_UNPRIV;
		break;
	}

	switch (psm->ps_cmd) {
	case PS_CTL:
		if (msg->msg_iovlen != 1) {
			errno = EINVAL;
			return -1;
		}
		if (ctx->ps_control_client != NULL) {
			logerrx("%s: cannot handle another client", __func__);
			return 0;
		}
		fd = control_new(ctx, ctx->ps_ctl->psp_work_fd, fd_flags);
		if (fd == NULL)
			return -1;
		ctx->ps_control_client = fd;
		control_recvdata(fd, iov->iov_base, iov->iov_len);
		break;
	case PS_CTL_EOF:
		ctx->ps_control_client = NULL;
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}
	return 0;
}

static void
ps_ctl_dodispatch(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events,
	    ps_ctl_dispatch, psp->psp_ctx) == -1)
		logerr(__func__);
}

static void
ps_ctl_recv(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;
	char buf[BUFSIZ];
	ssize_t len;

	if (!(events & (ELE_READ | ELE_HANGUP)))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	if (events & ELE_READ) {
		len = read(ctx->ps_ctl->psp_work_fd, buf, sizeof(buf));
		if (len == -1)
			logerr("%s: read", __func__);
		else if (len == 0)
			// FIXME: Why does this happen?
			;
		else if (ctx->ps_control_client == NULL)
			logerrx("%s: clientfd #%d disconnected (len=%zd)",
			    __func__, ctx->ps_ctl->psp_work_fd, len);
		else {
			errno = 0;
			if (control_queue(ctx->ps_control_client,
			    buf, (size_t)len) == -1)
				logerr("%s: control_queue", __func__);
		}
	}
}

static void
ps_ctl_listen(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;
	char buf[BUFSIZ];
	ssize_t len;
	struct fd_list *fd;

	if (!(events & ELE_READ))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = read(ctx->ps_control->fd, buf, sizeof(buf));
	if (len == -1) {
		logerr("%s: read", __func__);
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}

	/* Send to our listeners */
	TAILQ_FOREACH(fd, &ctx->control_fds, next) {
		if (!(fd->flags & FD_LISTEN))
			continue;
		if (control_queue(fd, buf, (size_t)len)== -1)
			logerr("%s: control_queue", __func__);
	}
}

pid_t
ps_ctl_start(struct dhcpcd_ctx *ctx)
{
	struct ps_id id = {
		.psi_ifindex = 0,
		.psi_cmd = PS_CTL,
	};
	struct ps_process *psp;
	int work_fd[2], listen_fd[2];
	pid_t pid;

	if_closesockets(ctx);

	if (xsocketpair(AF_UNIX, SOCK_STREAM | SOCK_CXNB, 0, work_fd) == -1 ||
	    xsocketpair(AF_UNIX, SOCK_STREAM | SOCK_CXNB, 0, listen_fd) == -1)
		return -1;
#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fdpair(work_fd) == -1 ||
	    ps_rights_limit_fdpair(listen_fd) == -1)
		return -1;
#endif

	psp = ctx->ps_ctl = ps_newprocess(ctx, &id);
	strlcpy(psp->psp_name, "control proxy", sizeof(psp->psp_name));
	pid = ps_startprocess(psp, ps_ctl_recvmsg, ps_ctl_dodispatch,
	    ps_ctl_startcb, NULL, PSF_DROPPRIVS);

	if (pid == -1)
		return -1;
	else if (pid != 0) {
		psp->psp_work_fd = work_fd[0];
		close(work_fd[1]);
		close(listen_fd[1]);
		ctx->ps_control = control_new(ctx,
		    listen_fd[0], FD_SENDLEN | FD_LISTEN);
		if (ctx->ps_control == NULL)
			return -1;
		return pid;
	}

	close(work_fd[0]);
	close(listen_fd[0]);

	psp->psp_work_fd = work_fd[1];
	if (eloop_event_add(ctx->eloop, psp->psp_work_fd, ELE_READ,
	    ps_ctl_recv, ctx) == -1)
		return -1;

	ctx->ps_control = control_new(ctx, listen_fd[1], 0);
	if (ctx->ps_control == NULL)
		return -1;
	if (eloop_event_add(ctx->eloop, ctx->ps_control->fd, ELE_READ,
	    ps_ctl_listen, ctx) == -1)
		return -1;

	ps_entersandbox("stdio inet", NULL);
	return 0;
}

int
ps_ctl_stop(struct dhcpcd_ctx *ctx)
{

	return ps_stopprocess(ctx->ps_ctl);
}

ssize_t
ps_ctl_sendargs(struct fd_list *fd, void *data, size_t len)
{
	struct dhcpcd_ctx *ctx = fd->ctx;

	if (ctx->ps_control_client != NULL && ctx->ps_control_client != fd)
		logerrx("%s: cannot deal with another client", __func__);
	ctx->ps_control_client = fd;
	return ps_sendcmd(ctx, ctx->ps_ctl->psp_fd, PS_CTL,
	    fd->flags & FD_UNPRIV ? PS_CTL_UNPRIV : PS_CTL_PRIV,
	    data, len);
}

ssize_t
ps_ctl_sendeof(struct fd_list *fd)
{
	struct dhcpcd_ctx *ctx = fd->ctx;

	return ps_sendcmd(ctx, ctx->ps_ctl->psp_fd, PS_CTL_EOF, 0, NULL, 0);
}
