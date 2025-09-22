/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd, network proxy
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

#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <assert.h>
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

/* We expect to have open 2 SEQPACKET, 1 udp, 1 udp6 and 1 raw6 fds */

#ifdef INET
static void
ps_inet_recvbootp(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	if (ps_recvmsg(ctx->udp_rfd, events,
	    PS_BOOTP, ctx->ps_inet->psp_fd) == -1)
		logerr(__func__);
}
#endif

#ifdef INET6
static void
ps_inet_recvra(void *arg, unsigned short events)
{
#ifdef __sun
	struct interface *ifp = arg;
	struct rs_state *state = RS_STATE(ifp);
	struct dhcpcd_ctx *ctx = ifp->ctx;

	if (ps_recvmsg(state->nd_fd, events,
	    PS_ND, ctx->ps_inet->psp_fd) == -1)
		logerr(__func__);
#else
	struct dhcpcd_ctx *ctx = arg;

	if (ps_recvmsg(ctx->nd_fd, events,
	    PS_ND, ctx->ps_inet->psp_fd) == -1)
		logerr(__func__);
#endif
}
#endif

#ifdef DHCP6
static void
ps_inet_recvdhcp6(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	if (ps_recvmsg(ctx->dhcp6_rfd, events,
	    PS_DHCP6, ctx->ps_inet->psp_fd) == -1)
		logerr(__func__);
}
#endif

bool
ps_inet_canstart(const struct dhcpcd_ctx *ctx)
{

#ifdef INET
	if ((ctx->options & (DHCPCD_IPV4 | DHCPCD_MANAGER)) ==
	    (DHCPCD_IPV4 | DHCPCD_MANAGER))
		return true;
#endif
#if defined(INET6) && !defined(__sun)
	if (ctx->options & DHCPCD_IPV6)
		return true;
#endif
#ifdef DHCP6
	if ((ctx->options & (DHCPCD_IPV6 | DHCPCD_MANAGER)) ==
	    (DHCPCD_IPV6 | DHCPCD_MANAGER))
		return true;
#endif

	return false;
}

static int
ps_inet_startcb(struct ps_process *psp)
{
	struct dhcpcd_ctx *ctx = psp->psp_ctx;
	int ret = 0;

	if (ctx->options & DHCPCD_MANAGER)
		setproctitle("[network proxy]");
	else
		setproctitle("[network proxy] %s%s%s",
		    ctx->ifc != 0 ? ctx->ifv[0] : "",
		    ctx->options & DHCPCD_IPV4 ? " [ip4]" : "",
		    ctx->options & DHCPCD_IPV6 ? " [ip6]" : "");

	/* This end is the main engine, so it's useless for us. */
	close(ctx->ps_data_fd);
	ctx->ps_data_fd = -1;

	errno = 0;

#ifdef INET
	if ((ctx->options & (DHCPCD_IPV4 | DHCPCD_MANAGER)) ==
	    (DHCPCD_IPV4 | DHCPCD_MANAGER))
	{
		ctx->udp_rfd = dhcp_openudp(NULL);
		if (ctx->udp_rfd == -1)
			logerr("%s: dhcp_open", __func__);
#ifdef PRIVSEP_RIGHTS
		else if (ps_rights_limit_fd_rdonly(ctx->udp_rfd) == -1) {
			logerr("%s: ps_rights_limit_fd_rdonly", __func__);
			close(ctx->udp_rfd);
			ctx->udp_rfd = -1;
		}
#endif
		else if (eloop_event_add(ctx->eloop, ctx->udp_rfd, ELE_READ,
		    ps_inet_recvbootp, ctx) == -1)
		{
			logerr("%s: eloop_event_add DHCP", __func__);
			close(ctx->udp_rfd);
			ctx->udp_rfd = -1;
		} else
			ret++;
	}
#endif
#if defined(INET6) && !defined(__sun)
	if (ctx->options & DHCPCD_IPV6) {
		ctx->nd_fd = ipv6nd_open(true);
		if (ctx->nd_fd == -1)
			logerr("%s: ipv6nd_open", __func__);
#ifdef PRIVSEP_RIGHTS
		else if (ps_rights_limit_fd_rdonly(ctx->nd_fd) == -1) {
			logerr("%s: ps_rights_limit_fd_rdonly", __func__);
			close(ctx->nd_fd);
			ctx->nd_fd = -1;
		}
#endif
		else if (eloop_event_add(ctx->eloop, ctx->nd_fd, ELE_READ,
		    ps_inet_recvra, ctx) == -1)
		{
			logerr("%s: eloop_event_add RA", __func__);
			close(ctx->nd_fd);
			ctx->nd_fd = -1;
		} else
			ret++;
	}
#endif
#ifdef DHCP6
	if ((ctx->options & (DHCPCD_IPV6 | DHCPCD_MANAGER)) ==
	    (DHCPCD_IPV6 | DHCPCD_MANAGER))
	{
		ctx->dhcp6_rfd = dhcp6_openudp(0, NULL);
		if (ctx->dhcp6_rfd == -1)
			logerr("%s: dhcp6_open", __func__);
#ifdef PRIVSEP_RIGHTS
		else if (ps_rights_limit_fd_rdonly(ctx->dhcp6_rfd) == -1) {
			logerr("%s: ps_rights_limit_fd_rdonly", __func__);
			close(ctx->dhcp6_rfd);
			ctx->dhcp6_rfd = -1;
		}
#endif
		else if (eloop_event_add(ctx->eloop, ctx->dhcp6_rfd, ELE_READ,
		    ps_inet_recvdhcp6, ctx) == -1)
		{
			logerr("%s: eloop_event_add DHCP6", __func__);
			close(ctx->dhcp6_rfd);
			ctx->dhcp6_rfd = -1;
		} else
			ret++;
	}
#endif

	if (ret == 0 && errno == 0) {
		errno = ENXIO;
		return -1;
	}
	return ret;
}

static bool
ps_inet_validudp(struct msghdr *msg, uint16_t sport, uint16_t dport)
{
	struct udphdr udp;
	struct iovec *iov = msg->msg_iov;

	if (msg->msg_iovlen == 0 || iov->iov_len < sizeof(udp)) {
		errno = EINVAL;
		return false;
	}

	memcpy(&udp, iov->iov_base, sizeof(udp));
	if (udp.uh_sport != htons(sport) || udp.uh_dport != htons(dport)) {
		errno = EPERM;
		return false;
	}
	return true;
}

#ifdef INET6
static bool
ps_inet_validnd(struct msghdr *msg)
{
	struct icmp6_hdr icmp6;
	struct iovec *iov = msg->msg_iov;

	if (msg->msg_iovlen == 0 || iov->iov_len < sizeof(icmp6)) {
		errno = EINVAL;
		return false;
	}

	memcpy(&icmp6, iov->iov_base, sizeof(icmp6));
	switch(icmp6.icmp6_type) {
	case ND_ROUTER_SOLICIT:
	case ND_NEIGHBOR_ADVERT:
		break;
	default:
		errno = EPERM;
		return false;
	}

	return true;
}
#endif

static ssize_t
ps_inet_sendmsg(struct dhcpcd_ctx *ctx,
    struct ps_msghdr *psm, struct msghdr *msg)
{
	struct ps_process *psp;
	int s;

	psp = ps_findprocess(ctx, &psm->ps_id);
	if (psp != NULL) {
		s = psp->psp_work_fd;
		goto dosend;
	}

	switch (psm->ps_cmd) {
#ifdef INET
	case PS_BOOTP:
		if (!ps_inet_validudp(msg, BOOTPC, BOOTPS))
			return -1;
		s = ctx->udp_wfd;
		break;
#endif
#if defined(INET6) && !defined(__sun)
	case PS_ND:
		if (!ps_inet_validnd(msg))
			return -1;
		s = ctx->nd_fd;
		break;
#endif
#ifdef DHCP6
	case PS_DHCP6:
		if (!ps_inet_validudp(msg, DHCP6_CLIENT_PORT,DHCP6_SERVER_PORT))
			return -1;
		s = ctx->dhcp6_wfd;
		break;
#endif
	default:
		errno = EINVAL;
		return -1;
	}

dosend:
	return sendmsg(s, msg, 0);
}

static void
ps_inet_recvmsg(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	/* Receive shutdown */
	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events, NULL, NULL) == -1)
		logerr(__func__);
}

ssize_t
ps_inet_dispatch(void *arg, struct ps_msghdr *psm, struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = arg;

	switch (psm->ps_cmd) {
#ifdef INET
	case PS_BOOTP:
		dhcp_recvmsg(ctx, msg);
		break;
#endif
#ifdef INET6
	case PS_ND:
		ipv6nd_recvmsg(ctx, msg);
		break;
#endif
#ifdef DHCP6
	case PS_DHCP6:
		dhcp6_recvmsg(ctx, msg, NULL);
		break;
#endif
	default:
		errno = ENOTSUP;
		return -1;
	}
	return 1;
}

static void
ps_inet_dodispatch(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events,
	    ps_inet_dispatch, psp->psp_ctx) == -1)
		logerr(__func__);
}

pid_t
ps_inet_start(struct dhcpcd_ctx *ctx)
{
	struct ps_id id = {
		.psi_ifindex = 0,
		.psi_cmd = PS_INET,
	};
	struct ps_process *psp;
	pid_t pid;

	psp = ctx->ps_inet = ps_newprocess(ctx, &id);
	if (psp == NULL)
		return -1;

	strlcpy(psp->psp_name, "network proxy", sizeof(psp->psp_name));
	pid = ps_startprocess(psp, ps_inet_recvmsg, ps_inet_dodispatch,
	    ps_inet_startcb, NULL, PSF_DROPPRIVS);

	if (pid == 0)
		ps_entersandbox("stdio", NULL);

	return pid;
}

int
ps_inet_stop(struct dhcpcd_ctx *ctx)
{

	return ps_stopprocess(ctx->ps_inet);
}

#ifdef INET
static void
ps_inet_recvinbootp(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvmsg(psp->psp_work_fd, events,
	    PS_BOOTP, psp->psp_ctx->ps_data_fd) == -1)
		logerr(__func__);
}

static int
ps_inet_listenin(struct ps_process *psp)
{
	struct in_addr *ia = &psp->psp_id.psi_addr.psa_in_addr;
	char buf[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, ia, buf, sizeof(buf));
	setproctitle("[%s proxy] %s", psp->psp_protostr, buf);

	psp->psp_work_fd = dhcp_openudp(ia);
	if (psp->psp_work_fd == -1) {
		logerr(__func__);
		return -1;
	}

#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fd_rdonly(psp->psp_work_fd) == -1) {
		logerr("%s: ps_rights_limit_fd_rdonly", __func__);
		return -1;
	}
#endif

	if (eloop_event_add(psp->psp_ctx->eloop, psp->psp_work_fd, ELE_READ,
	    ps_inet_recvinbootp, psp) == -1)
	{
		logerr("%s: eloop_event_add DHCP", __func__);
		return -1;
	}
	return 0;
}
#endif

#if defined(INET6) && defined(__sun)
static void
ps_inet_recvin6nd(void *arg)
{
	struct ps_process *psp = arg;

	if (ps_recvmsg(psp->psp_work_fd,
	    PS_ND, psp->psp_ctx->ps_data_fd) == -1)
		logerr(__func__);
}

static int
ps_inet_listennd(struct ps_process *psp)
{

	setproctitle("[ND network proxy]");

	psp->psp_work_fd = ipv6nd_open(&psp->psp_ifp);
	if (psp->psp_work_fd == -1) {
		logerr(__func__);
		return -1;
	}

#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fd_rdonly(psp->psp_work_fd) == -1) {
		logerr("%s: ps_rights_limit_fd_rdonly", __func__);
		return -1;
	}
#endif

	if (eloop_event_add(psp->psp_ctx->eloop, psp->psp_work_fd,
	    ps_inet_recvin6nd, psp) == -1)
	{
		logerr(__func__);
		return -1;
	}
	return 0;
}
#endif

#ifdef DHCP6
static void
ps_inet_recvin6dhcp6(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvmsg(psp->psp_work_fd, events,
	    PS_DHCP6, psp->psp_ctx->ps_data_fd) == -1)
		logerr(__func__);
}

static int
ps_inet_listenin6(struct ps_process *psp)
{
	struct in6_addr *ia = &psp->psp_id.psi_addr.psa_in6_addr;
	char buf[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, ia, buf, sizeof(buf));
	setproctitle("[%s proxy] %s", psp->psp_protostr, buf);

	psp->psp_work_fd = dhcp6_openudp(psp->psp_id.psi_ifindex, ia);
	if (psp->psp_work_fd == -1) {
		logerr(__func__);
		return -1;
	}

#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fd_rdonly(psp->psp_work_fd) == -1) {
		logerr("%s: ps_rights_limit_fd_rdonly", __func__);
		return -1;
	}
#endif

	if (eloop_event_add(psp->psp_ctx->eloop, psp->psp_work_fd, ELE_READ,
	    ps_inet_recvin6dhcp6, psp) == -1)
	{
		logerr("%s: eloop_event_add DHCP", __func__);
		return -1;
	}
	return 0;
}
#endif

static void
ps_inet_recvmsgpsp(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	/* Receive shutdown. */
	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events, NULL, NULL) == -1)
		logerr(__func__);
}

ssize_t
ps_inet_cmd(struct dhcpcd_ctx *ctx, struct ps_msghdr *psm, struct msghdr *msg)
{
	uint16_t cmd;
	struct ps_process *psp;
	int (*start_func)(struct ps_process *);
	pid_t start;
	struct ps_addr *psa = &psm->ps_id.psi_addr;
	void *ia;
	char buf[INET_MAX_ADDRSTRLEN];

	cmd = (uint16_t)(psm->ps_cmd & ~(PS_START | PS_STOP));
	if (cmd == psm->ps_cmd)
		return ps_inet_sendmsg(ctx, psm, msg);

	psp = ps_findprocess(ctx, &psm->ps_id);

#ifdef PRIVSEP_DEBUG
	logerrx("%s: IN cmd %x, psp %p", __func__, psm->ps_cmd, psp);
#endif

	if (psm->ps_cmd & PS_STOP) {
		assert(psp == NULL);
		return 0;
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


	switch (cmd) {
#ifdef INET
	case PS_BOOTP:
		start_func = ps_inet_listenin;
		psp->psp_protostr = "BOOTP";
		ia = &psa->psa_in_addr;
		break;
#endif
#ifdef INET6
#ifdef __sun
	case PS_ND:
		start_func = ps_inet_listennd;
		psp->psp_protostr = "ND";
		ia = &psa->psa_in6_addr;
		break;
#endif
#ifdef DHCP6
	case PS_DHCP6:
		start_func = ps_inet_listenin6;
		psp->psp_protostr = "DHCP6";
		ia = &psa->psa_in6_addr;
		break;
#endif
#endif
	default:
		logerrx("%s: unknown command %x", __func__, psm->ps_cmd);
		errno = ENOTSUP;
		return -1;
	}

	snprintf(psp->psp_name, sizeof(psp->psp_name),
	    "%s proxy %s", psp->psp_protostr,
	    inet_ntop(psa->psa_family, ia, buf, sizeof(buf)));
	start = ps_startprocess(psp, ps_inet_recvmsgpsp, NULL,
	    start_func, NULL, PSF_DROPPRIVS);
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

#ifdef INET
static ssize_t
ps_inet_in_docmd(struct ipv4_addr *ia, uint16_t cmd, const struct msghdr *msg)
{
	assert(ia != NULL);
	struct dhcpcd_ctx *ctx = ia->iface->ctx;
	struct ps_msghdr psm = {
		.ps_cmd = cmd,
		.ps_id = {
			.psi_cmd = (uint8_t)(cmd & ~(PS_START | PS_STOP)),
			.psi_ifindex = ia->iface->index,
			.psi_addr.psa_family = AF_INET,
			.psi_addr.psa_in_addr = ia->addr,
		},
	};

	return ps_sendpsmmsg(ctx, PS_ROOT_FD(ctx), &psm, msg);
}

ssize_t
ps_inet_openbootp(struct ipv4_addr *ia)
{

	return ps_inet_in_docmd(ia, PS_START | PS_BOOTP, NULL);
}

ssize_t
ps_inet_closebootp(struct ipv4_addr *ia)
{

	return ps_inet_in_docmd(ia, PS_STOP | PS_BOOTP, NULL);
}

ssize_t
ps_inet_sendbootp(struct interface *ifp, const struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;

	return ps_sendmsg(ctx, PS_ROOT_FD(ctx), PS_BOOTP, 0, msg);
}
#endif /* INET */

#ifdef INET6
#ifdef __sun
static ssize_t
ps_inet_ifp_docmd(struct interface *ifp, uint16_t cmd, const struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;
	struct ps_msghdr psm = {
		.ps_cmd = cmd,
		.ps_id = {
			.psi_cmd = (uint8_t)(cmd & ~(PS_START | PS_STOP)),
			.psi_ifindex = ifp->index,
			.psi_addr.psa_family = AF_INET6,
		},
	};

	return ps_sendpsmmsg(ctx, PS_ROOT_FD(ctx), &psm, msg);
}

ssize_t
ps_inet_opennd(struct interface *ifp)
{

	return ps_inet_ifp_docmd(ifp, PS_ND | PS_START, NULL);
}

ssize_t
ps_inet_closend(struct interface *ifp)
{

	return ps_inet_ifp_docmd(ifp, PS_ND | PS_STOP, NULL);
}

ssize_t
ps_inet_sendnd(struct interface *ifp, const struct msghdr *msg)
{

	return ps_inet_ifp_docmd(ifp, PS_ND, msg);
}
#else
ssize_t
ps_inet_sendnd(struct interface *ifp, const struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;

	return ps_sendmsg(ctx, PS_ROOT_FD(ctx), PS_ND, 0, msg);
}
#endif

#ifdef DHCP6
static ssize_t
ps_inet_in6_docmd(struct ipv6_addr *ia, uint16_t cmd, const struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = ia->iface->ctx;
	struct ps_msghdr psm = {
		.ps_cmd = cmd,
		.ps_id = {
			.psi_cmd = (uint8_t)(cmd & ~(PS_START | PS_STOP)),
			.psi_ifindex = ia->iface->index,
			.psi_addr.psa_family = AF_INET6,
			.psi_addr.psa_in6_addr = ia->addr,
		},
	};

	return ps_sendpsmmsg(ctx, PS_ROOT_FD(ctx), &psm, msg);
}

ssize_t
ps_inet_opendhcp6(struct ipv6_addr *ia)
{

	return ps_inet_in6_docmd(ia, PS_DHCP6 | PS_START, NULL);
}

ssize_t
ps_inet_closedhcp6(struct ipv6_addr *ia)
{

	return ps_inet_in6_docmd(ia, PS_DHCP6 | PS_STOP, NULL);
}

ssize_t
ps_inet_senddhcp6(struct interface *ifp, const struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;

	return ps_sendmsg(ctx, PS_ROOT_FD(ctx), PS_DHCP6, 0, msg);
}
#endif /* DHCP6 */
#endif /* INET6 */
