/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd, Solaris driver
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

#include <sys/ioctl.h>

#include <errno.h>
#include <unistd.h>

#include "dhcpcd.h"
#include "logerr.h"
#include "privsep.h"

#warning Solaris privsep should compile but wont work,
#warning no DLPI support, ioctl support need rework
/* We should implement privileges(5) as well.
 * https://illumos.org/man/5/privileges */

static ssize_t
ps_root_doioctl6(unsigned long req, void *data, size_t len)
{
	int s, err;

	s = xsocket(PF_INET6, SOCK_DGRAM, 0);
	if (s != -1)
		err = ioctl(s, req, data, len);
	else
		err = -1;
	if (err == -1)
		logerr(__func__);
	if (s != -1)
		close(s);
	return err;
}

static ssize_t
ps_root_doroute(void *data, size_t len)
{
	int s;
	ssize_t err;

	s = xsocket(PF_ROUTE, SOCK_RAW, 0);
	if (s != -1)
		err = write(s, data, len);
	else
		err = -1;
	if (err == -1)
		logerr(__func__);
	if (s != -1)
		close(s);
	return err;
}

ssize_t
ps_root_os(struct ps_msghdr *psm, struct msghdr *msg,
    void **rdata, size_t *rlen, __unused bool *free_rdata)
{
	struct iovec *iov = msg->msg_iov;
	void *data = iov->iov_base;
	size_t len = iov->iov_len;
	ssize_t err;

	switch (psm->ps_cmd) {
	case PS_IOCTL6:
		err = ps_root_doioctl6(psm->ps_flags, data, len);
	case PS_ROUTE:
		return ps_root_doroute(data, len);
	default:
		errno = ENOTSUP;
		return -1;
	}

	if (err != -1) {
		*rdata = data;
		*rlen = len;
	}
	return err;
}

ssize_t
ps_root_ioctl6(struct dhcpcd_ctx *ctx, unsigned long request, void *data, size_t len)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_IOCTL6,
	    request, data, len) == -1)
		return -1;
	return ps_root_readerror(ctx, data, len);
}

ssize_t
ps_root_route(struct dhcpcd_ctx *ctx, void *data, size_t len)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_ROUTE, 0, data, len) == -1)
		return -1;
	return ps_root_readerror(ctx, data, len);
}
