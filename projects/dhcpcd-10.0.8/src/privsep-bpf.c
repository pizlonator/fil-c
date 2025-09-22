/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation BPF Initiator
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
#include <sys/types.h>

/* Need these headers just for if_ether on some OS. */
#ifndef __NetBSD__
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#endif
#include <netinet/if_ether.h>

#include <assert.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arp.h"
#include "bpf.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "eloop.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"

/* We expect to have open 3 SEQPACKET and one RAW fd */

static void
ps_bpf_recvbpf(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;
	struct bpf *bpf = psp->psp_bpf;
	uint8_t buf[FRAMELEN_MAX];
	ssize_t len;
	struct ps_msghdr psm = {
		.ps_id = psp->psp_id,
		.ps_cmd = psp->psp_id.psi_cmd,
	};

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	bpf->bpf_flags &= ~BPF_EOF;
	/* A BPF read can read more than one filtered packet at time.
	 * This mechanism allows us to read each packet from the buffer. */
	while (!(bpf->bpf_flags & BPF_EOF)) {
		len = bpf_read(bpf, buf, sizeof(buf));
		if (len == -1) {
			int error = errno;

			if (errno != ENETDOWN)
				logerr("%s: %s", psp->psp_ifname, __func__);
			if (error != ENXIO)
				break;
			/* If the interface has departed, close the BPF
			 * socket. This stops log spam if RTM_IFANNOUNCE is
			 * delayed in announcing the departing interface. */
			eloop_event_delete(psp->psp_ctx->eloop, bpf->bpf_fd);
			bpf_close(bpf);
			psp->psp_bpf = NULL;
			break;
		}
		if (len == 0)
			break;
		psm.ps_flags = bpf->bpf_flags;
		len = ps_sendpsmdata(psp->psp_ctx, psp->psp_ctx->ps_data_fd,
		    &psm, buf, (size_t)len);
		if (len == -1)
			logerr(__func__);
		if (len == -1 || len == 0)
			break;
	}
}

static ssize_t
ps_bpf_recvmsgcb(void *arg, struct ps_msghdr *psm, struct msghdr *msg)
{
	struct ps_process *psp = arg;
	struct iovec *iov = msg->msg_iov;

#ifdef PRIVSEP_DEBUG
	logerrx("%s: IN cmd %x, psp %p", __func__, psm->ps_cmd, psp);
#endif

	switch(psm->ps_cmd) {
#ifdef ARP
	case PS_BPF_ARP:	/* FALLTHROUGH */
#endif
	case PS_BPF_BOOTP:
		break;
	default:
		/* IPC failure, we should not be processing any commands
		 * at this point!/ */
		errno = EINVAL;
		return -1;
	}

	/* We might have had an earlier ENXIO error. */
	if (psp->psp_bpf == NULL) {
		errno = ENXIO;
		return -1;
	}

	return bpf_send(psp->psp_bpf, psp->psp_proto,
	    iov->iov_base, iov->iov_len);
}

static void
ps_bpf_recvmsg(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events,
	    ps_bpf_recvmsgcb, arg) == -1)
		logerr(__func__);
}

static int
ps_bpf_start_bpf(struct ps_process *psp)
{
	struct dhcpcd_ctx *ctx = psp->psp_ctx;
	char *addr;
	struct in_addr *ia = &psp->psp_id.psi_addr.psa_in_addr;

	if (ia->s_addr == INADDR_ANY) {
		ia = NULL;
		addr = NULL;
	} else
		addr = inet_ntoa(*ia);
	setproctitle("[BPF %s] %s%s%s", psp->psp_protostr, psp->psp_ifname,
	    addr != NULL ? " " : "", addr != NULL ? addr : "");
	ps_freeprocesses(ctx, psp);

	psp->psp_bpf = bpf_open(&psp->psp_ifp, psp->psp_filter, ia);
#ifdef DEBUG_FD
	logdebugx("pid %d bpf_fd=%d", getpid(), psp->psp_bpf->bpf_fd);
#endif
	if (psp->psp_bpf == NULL)
		logerr("%s: bpf_open",__func__);
#ifdef PRIVSEP_RIGHTS
	else if (ps_rights_limit_fd(psp->psp_bpf->bpf_fd) == -1)
		logerr("%s: ps_rights_limit_fd", __func__);
#endif
	else if (eloop_event_add(ctx->eloop, psp->psp_bpf->bpf_fd, ELE_READ,
	    ps_bpf_recvbpf, psp) == -1)
		logerr("%s: eloop_event_add", __func__);
	else {
		psp->psp_work_fd = psp->psp_bpf->bpf_fd;
		return 0;
	}

	eloop_exit(ctx->eloop, EXIT_FAILURE);
	return -1;
}

ssize_t
ps_bpf_cmd(struct dhcpcd_ctx *ctx, struct ps_msghdr *psm, struct msghdr *msg)
{
	uint16_t cmd;
	struct ps_process *psp;
	pid_t start;
	struct iovec *iov = msg->msg_iov;
	struct interface *ifp;
	struct in_addr *ia = &psm->ps_id.psi_addr.psa_in_addr;
	const char *addr;

	cmd = (uint16_t)(psm->ps_cmd & ~(PS_START | PS_STOP));
	psp = ps_findprocess(ctx, &psm->ps_id);

#ifdef PRIVSEP_DEBUG
	logerrx("%s: IN cmd %x, psp %p", __func__, psm->ps_cmd, psp);
#endif

	switch (cmd) {
#ifdef ARP
	case PS_BPF_ARP:	/* FALLTHROUGH */
#endif
	case PS_BPF_BOOTP:
		break;
	default:
		logerrx("%s: unknown command %x", __func__, psm->ps_cmd);
		errno = ENOTSUP;
		return -1;
	}

	if (!(psm->ps_cmd & PS_START)) {
		errno = EINVAL;
		return -1;
	}

	if (psp != NULL)
		return 1;

	psp = ps_newprocess(ctx, &psm->ps_id);
	if (psp == NULL)
		return -1;

	ifp = &psp->psp_ifp;
	assert(msg->msg_iovlen == 1);
	assert(iov->iov_len == sizeof(*ifp));
	memcpy(ifp, iov->iov_base, sizeof(*ifp));
	ifp->ctx = psp->psp_ctx;
	ifp->options = NULL;
	memset(ifp->if_data, 0, sizeof(ifp->if_data));

	memcpy(psp->psp_ifname, ifp->name, sizeof(psp->psp_ifname));

	switch (cmd) {
#ifdef ARP
	case PS_BPF_ARP:
		psp->psp_proto = ETHERTYPE_ARP;
		psp->psp_protostr = "ARP";
		psp->psp_filter = bpf_arp;
		break;
#endif
	case PS_BPF_BOOTP:
		psp->psp_proto = ETHERTYPE_IP;
		psp->psp_protostr = "BOOTP";
		psp->psp_filter = bpf_bootp;
		break;
	}

	if (ia->s_addr == INADDR_ANY)
		addr = NULL;
	else
		addr = inet_ntoa(*ia);
	snprintf(psp->psp_name, sizeof(psp->psp_name), "BPF %s%s%s",
	    psp->psp_protostr,
	    addr != NULL ? " " : "", addr != NULL ? addr : "");

	start = ps_startprocess(psp, ps_bpf_recvmsg, NULL,
	    ps_bpf_start_bpf, NULL, PSF_DROPPRIVS);
	switch (start) {
	case -1:
		ps_freeprocess(psp);
		return -1;
	case 0:
		ps_entersandbox("stdio", NULL);
		break;
	default:
		logdebugx("%s: spawned %s on PID %d",
		    psp->psp_ifname, psp->psp_name, psp->psp_pid);
		break;
	}
	return start;
}

ssize_t
ps_bpf_dispatch(struct dhcpcd_ctx *ctx,
    struct ps_msghdr *psm, struct msghdr *msg)
{
	struct iovec *iov = msg->msg_iov;
	struct interface *ifp;
	uint8_t *bpf;
	size_t bpf_len;

	switch (psm->ps_cmd) {
#ifdef ARP
	case PS_BPF_ARP:
#endif
	case PS_BPF_BOOTP:
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}

	ifp = if_findindex(ctx->ifaces, psm->ps_id.psi_ifindex);
	/* interface may have departed .... */
	if (ifp == NULL)
		return -1;

	bpf = iov->iov_base;
	bpf_len = iov->iov_len;

	switch (psm->ps_cmd) {
#ifdef ARP
	case PS_BPF_ARP:
		arp_packet(ifp, bpf, bpf_len, (unsigned int)psm->ps_flags);
		break;
#endif
	case PS_BPF_BOOTP:
		dhcp_packet(ifp, bpf, bpf_len, (unsigned int)psm->ps_flags);
		break;
	}

	return 1;
}

static ssize_t
ps_bpf_send(const struct interface *ifp, const struct in_addr *ia,
    uint16_t cmd, const void *data, size_t len)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;
	struct ps_msghdr psm = {
		.ps_cmd = cmd,
		.ps_id = {
			.psi_ifindex = ifp->index,
			.psi_cmd = (uint8_t)(cmd & ~(PS_START | PS_STOP)),
		},
	};

	if (ia != NULL)
		psm.ps_id.psi_addr.psa_in_addr = *ia;

	return ps_sendpsmdata(ctx, PS_ROOT_FD(ctx), &psm, data, len);
}

#ifdef ARP
ssize_t
ps_bpf_openarp(const struct interface *ifp, const struct in_addr *ia)
{

	assert(ia != NULL);
	return ps_bpf_send(ifp, ia, PS_BPF_ARP | PS_START,
	    ifp, sizeof(*ifp));
}

ssize_t
ps_bpf_closearp(const struct interface *ifp, const struct in_addr *ia)
{

	return ps_bpf_send(ifp, ia, PS_BPF_ARP | PS_STOP, NULL, 0);
}

ssize_t
ps_bpf_sendarp(const struct interface *ifp, const struct in_addr *ia,
    const void *data, size_t len)
{

	assert(ia != NULL);
	return ps_bpf_send(ifp, ia, PS_BPF_ARP, data, len);
}
#endif

ssize_t
ps_bpf_openbootp(const struct interface *ifp)
{

	return ps_bpf_send(ifp, NULL, PS_BPF_BOOTP | PS_START,
	    ifp, sizeof(*ifp));
}

ssize_t
ps_bpf_closebootp(const struct interface *ifp)
{

	return ps_bpf_send(ifp, NULL, PS_BPF_BOOTP | PS_STOP, NULL, 0);
}

ssize_t
ps_bpf_sendbootp(const struct interface *ifp, const void *data, size_t len)
{

	return ps_bpf_send(ifp, NULL, PS_BPF_BOOTP, data, len);
}
