/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - IPv6 ND handling
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
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define ELOOP_QUEUE	ELOOP_IPV6ND
#include "common.h"
#include "dhcpcd.h"
#include "dhcp-common.h"
#include "dhcp6.h"
#include "eloop.h"
#include "if.h"
#include "ipv6.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "route.h"
#include "script.h"

/* Debugging Router Solicitations is a lot of spam, so disable it */
//#define DEBUG_RS

#ifndef ND_RA_FLAG_HOME_AGENT
#define	ND_RA_FLAG_HOME_AGENT	0x20	/* Home Agent flag in RA */
#endif
#ifndef ND_RA_FLAG_PROXY
#define	ND_RA_FLAG_PROXY	0x04	/* Proxy */
#endif
#ifndef ND_OPT_PI_FLAG_ROUTER
#define	ND_OPT_PI_FLAG_ROUTER	0x20	/* Router flag in PI */
#endif

#ifndef ND_OPT_RI
#define ND_OPT_RI	24
struct nd_opt_ri {		/* Route Information option RFC4191 */
	uint8_t	 nd_opt_ri_type;
	uint8_t	 nd_opt_ri_len;
	uint8_t	 nd_opt_ri_prefixlen;
	uint8_t	 nd_opt_ri_flags_reserved;
	uint32_t nd_opt_ri_lifetime;
	struct in6_addr nd_opt_ri_prefix;
};
__CTASSERT(sizeof(struct nd_opt_ri) == 24);
#define OPT_RI_FLAG_PREFERENCE(flags) ((flags & 0x18) >> 3)
#endif

#ifndef ND_OPT_RDNSS
#define ND_OPT_RDNSS			25
struct nd_opt_rdnss {           /* RDNSS option RFC 6106 */
	uint8_t		nd_opt_rdnss_type;
	uint8_t		nd_opt_rdnss_len;
	uint16_t	nd_opt_rdnss_reserved;
	uint32_t	nd_opt_rdnss_lifetime;
	/* followed by list of IP prefixes */
};
__CTASSERT(sizeof(struct nd_opt_rdnss) == 8);
#endif

#ifndef ND_OPT_DNSSL
#define ND_OPT_DNSSL			31
struct nd_opt_dnssl {		/* DNSSL option RFC 6106 */
	uint8_t		nd_opt_dnssl_type;
	uint8_t		nd_opt_dnssl_len;
	uint16_t	nd_opt_dnssl_reserved;
	uint32_t	nd_opt_dnssl_lifetime;
	/* followed by list of DNS servers */
};
__CTASSERT(sizeof(struct nd_opt_dnssl) == 8);
#endif

/* Impossible options, so we can easily add extras */
#define _ND_OPT_PREFIX_ADDR	255 + 1

/* Minimal IPv6 MTU */
#ifndef IPV6_MMTU
#define IPV6_MMTU 1280
#endif

#ifndef ND_RA_FLAG_RTPREF_HIGH
#define ND_RA_FLAG_RTPREF_MASK		0x18
#define ND_RA_FLAG_RTPREF_HIGH		0x08
#define ND_RA_FLAG_RTPREF_MEDIUM	0x00
#define ND_RA_FLAG_RTPREF_LOW		0x18
#define ND_RA_FLAG_RTPREF_RSV		0x10
#endif

#define	EXPIRED_MAX	5	/* Remember 5 expired routers to avoid
				   logspam. */

#define MIN_RANDOM_FACTOR	500				/* millisecs */
#define MAX_RANDOM_FACTOR	1500				/* millisecs */
#define MIN_RANDOM_FACTOR_U	MIN_RANDOM_FACTOR * 1000	/* usecs */
#define MAX_RANDOM_FACTOR_U	MAX_RANDOM_FACTOR * 1000	/* usecs */

#if BYTE_ORDER == BIG_ENDIAN
#define IPV6_ADDR_INT32_ONE     1
#define IPV6_ADDR_INT16_MLL     0xff02
#elif BYTE_ORDER == LITTLE_ENDIAN
#define IPV6_ADDR_INT32_ONE     0x01000000
#define IPV6_ADDR_INT16_MLL     0x02ff
#endif

/* Debugging Neighbor Solicitations is a lot of spam, so disable it */
//#define DEBUG_NS
//

static void ipv6nd_handledata(void *, unsigned short);
static struct routeinfo *routeinfo_findalloc(struct ra *, const struct in6_addr *, uint8_t);
static void routeinfohead_free(struct routeinfohead *);

/*
 * Android ships buggy ICMP6 filter headers.
 * Supply our own until they fix their shit.
 * References:
 *     https://android-review.googlesource.com/#/c/58438/
 *     http://code.google.com/p/android/issues/original?id=32621&seq=24
 */
#ifdef __ANDROID__
#undef ICMP6_FILTER_WILLPASS
#undef ICMP6_FILTER_WILLBLOCK
#undef ICMP6_FILTER_SETPASS
#undef ICMP6_FILTER_SETBLOCK
#undef ICMP6_FILTER_SETPASSALL
#undef ICMP6_FILTER_SETBLOCKALL
#define ICMP6_FILTER_WILLPASS(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) == 0)
#define ICMP6_FILTER_WILLBLOCK(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) != 0)
#define ICMP6_FILTER_SETPASS(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) &= ~(1 << ((type) & 31))))
#define ICMP6_FILTER_SETBLOCK(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) |=  (1 << ((type) & 31))))
#define ICMP6_FILTER_SETPASSALL(filterp) \
	memset(filterp, 0, sizeof(struct icmp6_filter));
#define ICMP6_FILTER_SETBLOCKALL(filterp) \
	memset(filterp, 0xff, sizeof(struct icmp6_filter));
#endif

/* Support older systems with different defines */
#if !defined(IPV6_RECVHOPLIMIT) && defined(IPV6_HOPLIMIT)
#define IPV6_RECVHOPLIMIT IPV6_HOPLIMIT
#endif
#if !defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define IPV6_RECVPKTINFO IPV6_PKTINFO
#endif

/* Handy defines */
#define ipv6nd_free_ra(ra) ipv6nd_freedrop_ra((ra),  0)
#define ipv6nd_drop_ra(ra) ipv6nd_freedrop_ra((ra),  1)

void
ipv6nd_printoptions(const struct dhcpcd_ctx *ctx,
    const struct dhcp_opt *opts, size_t opts_len)
{
	size_t i, j;
	const struct dhcp_opt *opt, *opt2;
	int cols;

	for (i = 0, opt = ctx->nd_opts;
	    i < ctx->nd_opts_len; i++, opt++)
	{
		for (j = 0, opt2 = opts; j < opts_len; j++, opt2++)
			if (opt2->option == opt->option)
				break;
		if (j == opts_len) {
			cols = printf("%03d %s", opt->option, opt->var);
			dhcp_print_option_encoding(opt, cols);
		}
	}
	for (i = 0, opt = opts; i < opts_len; i++, opt++) {
		cols = printf("%03d %s", opt->option, opt->var);
		dhcp_print_option_encoding(opt, cols);
	}
}

int
ipv6nd_open(bool recv)
{
	int fd, on;
	struct icmp6_filter filt;

	fd = xsocket(PF_INET6, SOCK_RAW | SOCK_CXNB, IPPROTO_ICMPV6);
	if (fd == -1)
		return -1;

	ICMP6_FILTER_SETBLOCKALL(&filt);

	/* RFC4861 4.1 */
	on = 255;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
	    &on, sizeof(on)) == -1)
		goto eexit;

	if (recv) {
		on = 1;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
		    &on, sizeof(on)) == -1)
			goto eexit;

		on = 1;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT,
		    &on, sizeof(on)) == -1)
			goto eexit;

		ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);

#ifdef SO_RERROR
		on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_RERROR,
		    &on, sizeof(on)) == -1)
			goto eexit;
#endif
	}

	if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER,
	    &filt, sizeof(filt)) == -1)
		goto eexit;

	return fd;

eexit:
	close(fd);
	return -1;
}

#ifdef __sun
int
ipv6nd_openif(struct interface *ifp)
{
	int fd;
	struct ipv6_mreq mreq = {
	    .ipv6mr_multiaddr = IN6ADDR_LINKLOCAL_ALLNODES_INIT,
	    .ipv6mr_interface = ifp->index
	};
	struct rs_state *state = RS_STATE(ifp);
	uint_t ifindex = ifp->index;

	if (state->nd_fd != -1)
		return state->nd_fd;

	fd = ipv6nd_open(true);
	if (fd == -1)
		return -1;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_BOUND_IF,
	    &ifindex, sizeof(ifindex)) == -1)
	{
		close(fd);
		return -1;
	}

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
	    &mreq, sizeof(mreq)) == -1)
	{
		close(fd);
		return -1;
	}

	if (eloop_event_add(ifp->ctx->eloop, fd, ELE_READ,
	    ipv6nd_handledata, ifp) == -1)
	{
		close(fd);
		return -1;
	}

	state->nd_fd = fd;
	return fd;
}
#endif

static int
ipv6nd_makersprobe(struct interface *ifp)
{
	struct rs_state *state;
	struct nd_router_solicit *rs;

	state = RS_STATE(ifp);
	free(state->rs);
	state->rslen = sizeof(*rs);
	if (ifp->hwlen != 0)
		state->rslen += (size_t)ROUNDUP8(ifp->hwlen + 2);
	state->rs = calloc(1, state->rslen);
	if (state->rs == NULL)
		return -1;
	rs = state->rs;
	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	//rs->nd_rs_code = 0;
	//rs->nd_rs_cksum = 0;
	//rs->nd_rs_reserved = 0;

	if (ifp->hwlen != 0) {
		struct nd_opt_hdr *nd;

		nd = (struct nd_opt_hdr *)(state->rs + 1);
		nd->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
		nd->nd_opt_len = (uint8_t)((ROUNDUP8(ifp->hwlen + 2)) >> 3);
		memcpy(nd + 1, ifp->hwaddr, ifp->hwlen);
	}
	return 0;
}

static void
ipv6nd_sendrsprobe(void *arg)
{
	struct interface *ifp = arg;
	struct rs_state *state = RS_STATE(ifp);
	struct sockaddr_in6 dst = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_LINKLOCAL_ALLROUTERS_INIT,
		.sin6_scope_id = ifp->index,
	};
	struct iovec iov = { .iov_base = state->rs, .iov_len = state->rslen };
	union {
		struct cmsghdr hdr;
		uint8_t buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	} cmsgbuf = { .buf = { 0 } };
	struct msghdr msg = {
	    .msg_name = &dst, .msg_namelen = sizeof(dst),
	    .msg_iov = &iov, .msg_iovlen = 1,
	    .msg_control = cmsgbuf.buf, .msg_controllen = sizeof(cmsgbuf.buf),
	};
	struct cmsghdr *cm;
	struct in6_pktinfo pi = { .ipi6_ifindex = ifp->index };
	int s;
#ifndef __sun
	struct dhcpcd_ctx *ctx = ifp->ctx;
#endif

	if (ipv6_linklocal(ifp) == NULL) {
		logdebugx("%s: delaying Router Solicitation for LL address",
		    ifp->name);
		ipv6_addlinklocalcallback(ifp, ipv6nd_sendrsprobe, ifp);
		return;
	}

#ifdef HAVE_SA_LEN
	dst.sin6_len = sizeof(dst);
#endif

	/* Set the outbound interface */
	cm = CMSG_FIRSTHDR(&msg);
	if (cm == NULL) /* unlikely */
		return;
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(pi));
	memcpy(CMSG_DATA(cm), &pi, sizeof(pi));

	logdebugx("%s: sending Router Solicitation", ifp->name);
#ifdef PRIVSEP
	if (IN_PRIVSEP(ifp->ctx)) {
		if (ps_inet_sendnd(ifp, &msg) == -1)
			logerr(__func__);
		goto sent;
	}
#endif
#ifdef __sun
	if (state->nd_fd == -1) {
		if (ipv6nd_openif(ifp) == -1) {
			logerr(__func__);
			return;
		}
	}
	s = state->nd_fd;
#else
	if (ctx->nd_fd == -1) {
		ctx->nd_fd = ipv6nd_open(true);
		if (ctx->nd_fd == -1) {
			logerr(__func__);
			return;
		}
		if (eloop_event_add(ctx->eloop, ctx->nd_fd, ELE_READ,
		    ipv6nd_handledata, ctx) == -1)
			logerr("%s: eloop_event_add", __func__);
	}
	s = ifp->ctx->nd_fd;
#endif
	if (sendmsg(s, &msg, 0) == -1) {
		logerr(__func__);
		/* Allow IPv6ND to continue .... at most a few errors
		 * would be logged.
		 * Generally the error is ENOBUFS when struggling to
		 * associate with an access point. */
	}

#ifdef PRIVSEP
sent:
#endif
	if (state->rsprobes++ < MAX_RTR_SOLICITATIONS)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    RTR_SOLICITATION_INTERVAL, ipv6nd_sendrsprobe, ifp);
	else
		logwarnx("%s: no IPv6 Routers available", ifp->name);
}

#ifdef ND6_ADVERTISE
static void
ipv6nd_sendadvertisement(void *arg)
{
	struct ipv6_addr *ia = arg;
	struct interface *ifp = ia->iface;
	struct dhcpcd_ctx *ctx = ifp->ctx;
	struct sockaddr_in6 dst = {
	    .sin6_family = AF_INET6,
	    .sin6_addr = IN6ADDR_LINKLOCAL_ALLNODES_INIT,
	    .sin6_scope_id = ifp->index,
	};
	struct iovec iov = { .iov_base = ia->na, .iov_len = ia->na_len };
	union {
		struct cmsghdr hdr;
		uint8_t buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	} cmsgbuf = { .buf = { 0 } };
	struct msghdr msg = {
	    .msg_name = &dst, .msg_namelen = sizeof(dst),
	    .msg_iov = &iov, .msg_iovlen = 1,
	    .msg_control = cmsgbuf.buf, .msg_controllen = sizeof(cmsgbuf.buf),
	};
	struct cmsghdr *cm;
	struct in6_pktinfo pi = { .ipi6_ifindex = ifp->index };
	const struct rs_state *state = RS_CSTATE(ifp);
	int s;

	if (state == NULL || !if_is_link_up(ifp))
		goto freeit;

#ifdef SIN6_LEN
	dst.sin6_len = sizeof(dst);
#endif

	/* Set the outbound interface. */
	cm = CMSG_FIRSTHDR(&msg);
	assert(cm != NULL);
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(pi));
	memcpy(CMSG_DATA(cm), &pi, sizeof(pi));
	logdebugx("%s: sending NA for %s", ifp->name, ia->saddr);

#ifdef PRIVSEP
	if (IN_PRIVSEP(ifp->ctx)) {
		if (ps_inet_sendnd(ifp, &msg) == -1)
			logerr(__func__);
		goto sent;
	}
#endif
#ifdef __sun
	s = state->nd_fd;
#else
	s = ctx->nd_fd;
#endif
	if (sendmsg(s, &msg, 0) == -1)
		logerr(__func__);

#ifdef PRIVSEP
sent:
#endif
	if (++ia->na_count < MAX_NEIGHBOR_ADVERTISEMENT) {
		eloop_timeout_add_sec(ctx->eloop,
		    state->retrans / 1000, ipv6nd_sendadvertisement, ia);
		return;
	}

freeit:
	free(ia->na);
	ia->na = NULL;
	ia->na_count = 0;
}

void
ipv6nd_advertise(struct ipv6_addr *ia)
{
	struct dhcpcd_ctx *ctx;
	struct interface *ifp;
	struct ipv6_state *state;
	struct ipv6_addr *iap, *iaf;
	struct nd_neighbor_advert *na;

	if (IN6_IS_ADDR_MULTICAST(&ia->addr))
		return;

#ifdef __sun
	if (!(ia->flags & IPV6_AF_AUTOCONF) && ia->flags & IPV6_AF_RAPFX)
		return;
#endif

	ctx = ia->iface->ctx;
	/* Find the most preferred address to advertise. */
	iaf = NULL;
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		state = IPV6_STATE(ifp);
		if (state == NULL || !if_is_link_up(ifp))
			continue;

		TAILQ_FOREACH(iap, &state->addrs, next) {
			if (!IN6_ARE_ADDR_EQUAL(&iap->addr, &ia->addr))
				continue;

			/* Cancel any current advertisement. */
			eloop_timeout_delete(ctx->eloop,
			    ipv6nd_sendadvertisement, iap);

			/* Don't advertise what we can't use. */
			if (iap->prefix_vltime == 0 ||
			    iap->addr_flags & IN6_IFF_NOTUSEABLE)
				continue;

			if (iaf == NULL ||
			    iaf->iface->metric > iap->iface->metric)
				iaf = iap;
		}
	}
	if (iaf == NULL)
		return;

	/* Make the packet. */
	ifp = iaf->iface;
	iaf->na_len = sizeof(*na);
	if (ifp->hwlen != 0)
		iaf->na_len += (size_t)ROUNDUP8(ifp->hwlen + 2);
	na = calloc(1, iaf->na_len);
	if (na == NULL) {
		logerr(__func__);
		return;
	}

	na->nd_na_type = ND_NEIGHBOR_ADVERT;
	na->nd_na_flags_reserved = ND_NA_FLAG_OVERRIDE;
#if defined(PRIVSEP) && (defined(__linux__) || defined(HAVE_PLEDGE))
	if (IN_PRIVSEP(ctx)) {
		if (ps_root_ip6forwarding(ctx, ifp->name) != 0)
			na->nd_na_flags_reserved |= ND_NA_FLAG_ROUTER;
	} else
#endif
	if (ip6_forwarding(ifp->name) != 0)
		na->nd_na_flags_reserved |= ND_NA_FLAG_ROUTER;
	na->nd_na_target = ia->addr;

	if (ifp->hwlen != 0) {
		struct nd_opt_hdr *opt;

		opt = (struct nd_opt_hdr *)(na + 1);
		opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
		opt->nd_opt_len = (uint8_t)((ROUNDUP8(ifp->hwlen + 2)) >> 3);
		memcpy(opt + 1, ifp->hwaddr, ifp->hwlen);
	}

	iaf->na_count = 0;
	free(iaf->na);
	iaf->na = na;
	eloop_timeout_delete(ctx->eloop, ipv6nd_sendadvertisement, iaf);
	ipv6nd_sendadvertisement(iaf);
}
#elif !defined(SMALL)
#warning kernel does not support userland sending ND6 advertisements
#endif /* ND6_ADVERTISE */

static void
ipv6nd_expire(void *arg)
{
	struct interface *ifp = arg;
	struct ra *rap;

	if (ifp->ctx->ra_routers == NULL)
		return;

	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface == ifp && rap->willexpire)
			rap->doexpire = true;
	}
	ipv6nd_expirera(ifp);
}

void
ipv6nd_startexpire(struct interface *ifp)
{
	struct ra *rap;

	if (ifp->ctx->ra_routers == NULL)
		return;

	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface == ifp)
			rap->willexpire = true;
	}
	eloop_q_timeout_add_sec(ifp->ctx->eloop, ELOOP_IPV6RA_EXPIRE,
	    RTR_CARRIER_EXPIRE, ipv6nd_expire, ifp);
}

int
ipv6nd_rtpref(uint8_t flags)
{

	switch (flags & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		return RTPREF_HIGH;
	case ND_RA_FLAG_RTPREF_MEDIUM:
	case ND_RA_FLAG_RTPREF_RSV:
		return RTPREF_MEDIUM;
	case ND_RA_FLAG_RTPREF_LOW:
		return RTPREF_LOW;
	default:
		logerrx("%s: impossible RA flag %x", __func__, flags);
		return RTPREF_INVALID;
	}
	/* NOTREACHED */
}

static void
ipv6nd_sortrouters(struct dhcpcd_ctx *ctx)
{
	struct ra_head sorted_routers = TAILQ_HEAD_INITIALIZER(sorted_routers);
	struct ra *ra1, *ra2;

	while ((ra1 = TAILQ_FIRST(ctx->ra_routers)) != NULL) {
		TAILQ_REMOVE(ctx->ra_routers, ra1, next);
		TAILQ_FOREACH(ra2, &sorted_routers, next) {
			if (ra1->iface->metric > ra2->iface->metric)
				continue;
			if (ra1->expired && !ra2->expired)
				continue;
			if (ra1->willexpire && !ra2->willexpire)
				continue;
			if (ra1->lifetime == 0 && ra2->lifetime != 0)
				continue;
			if (!ra1->isreachable && ra2->reachable)
				continue;
			if (ipv6nd_rtpref(ra1->flags) <= ipv6nd_rtpref(ra2->flags))
				continue;
			/* All things being equal, prefer older routers. */
			/* We don't need to check time, becase newer
			 * routers are always added to the tail and then
			 * sorted. */
			TAILQ_INSERT_BEFORE(ra2, ra1, next);
			break;
		}
		if (ra2 == NULL)
			TAILQ_INSERT_TAIL(&sorted_routers, ra1, next);
	}

	TAILQ_CONCAT(ctx->ra_routers, &sorted_routers, next);
}

static void
ipv6nd_applyra(struct interface *ifp)
{
	struct ra *rap;
	struct rs_state *state = RS_STATE(ifp);
	struct ra defra = {
		.iface = ifp,
		.hoplimit = IPV6_DEFHLIM ,
		.reachable = REACHABLE_TIME,
		.retrans = RETRANS_TIMER,
	};

	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface == ifp)
			break;
	}

	/* If we have no Router Advertisement, then set default values. */
	if (rap == NULL || rap->expired || rap->willexpire)
		rap = &defra;

	state->retrans = rap->retrans;
	if (if_applyra(rap) == -1 && errno != ENOENT)
		logerr(__func__);
}

/*
 * Neighbour reachability.
 *
 * RFC 4681 6.2.5 says when a node is no longer a router it MUST
 * send a RA with a zero lifetime.
 * All OS's I know of set the NA router flag if they are a router
 * or not and disregard that they are actively advertising or
 * shutting down. If the interface is disabled, it cant't send a NA at all.
 *
 * As such we CANNOT rely on the NA Router flag and MUST use
 * unreachability or receive a RA with a lifetime of zero to remove
 * the node as a default router.
 */
void
ipv6nd_neighbour(struct dhcpcd_ctx *ctx, struct in6_addr *addr, bool reachable)
{
	struct ra *rap, *rapr;

	if (ctx->ra_routers == NULL)
		return;

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (IN6_ARE_ADDR_EQUAL(&rap->from, addr))
			break;
	}

	if (rap == NULL || rap->expired || rap->isreachable == reachable)
		return;

	rap->isreachable = reachable;
	loginfox("%s: %s is %s", rap->iface->name, rap->sfrom,
	    reachable ? "reachable again" : "unreachable");

	/* See if we can install a reachable default router. */
	ipv6nd_sortrouters(ctx);
	ipv6nd_applyra(rap->iface);
	rt_build(ctx, AF_INET6);

	if (reachable)
		return;

	/* If we have no reachable default routers, try and solicit one. */
	TAILQ_FOREACH(rapr, ctx->ra_routers, next) {
		if (rap == rapr || rap->iface != rapr->iface)
			continue;
		if (rapr->isreachable && !rapr->expired && rapr->lifetime)
			break;
	}

	if (rapr == NULL)
		ipv6nd_startrs(rap->iface);
}

const struct ipv6_addr *
ipv6nd_iffindaddr(const struct interface *ifp, const struct in6_addr *addr,
    unsigned int flags)
{
	struct ra *rap;
	struct ipv6_addr *ap;

	if (ifp->ctx->ra_routers == NULL)
		return NULL;

	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface != ifp)
			continue;
		TAILQ_FOREACH(ap, &rap->addrs, next) {
			if (ipv6_findaddrmatch(ap, addr, flags))
				return ap;
		}
	}
	return NULL;
}

struct ipv6_addr *
ipv6nd_findaddr(struct dhcpcd_ctx *ctx, const struct in6_addr *addr,
    unsigned int flags)
{
	struct ra *rap;
	struct ipv6_addr *ap;

	if (ctx->ra_routers == NULL)
		return NULL;

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		TAILQ_FOREACH(ap, &rap->addrs, next) {
			if (ipv6_findaddrmatch(ap, addr, flags))
				return ap;
		}
	}
	return NULL;
}

static struct ipv6_addr *
ipv6nd_rapfindprefix(struct ra *rap,
    const struct in6_addr *pfx, uint8_t pfxlen)
{
	struct ipv6_addr *ia;

	TAILQ_FOREACH(ia, &rap->addrs, next) {
		if (ia->prefix_vltime == 0)
			continue;
		if (ia->prefix_len == pfxlen &&
		    IN6_ARE_ADDR_EQUAL(&ia->prefix, pfx))
			break;
	}
	return ia;
}

struct ipv6_addr *
ipv6nd_iffindprefix(struct interface *ifp,
    const struct in6_addr *pfx, uint8_t pfxlen)
{
	struct ra *rap;
	struct ipv6_addr *ia;

	ia = NULL;
	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface != ifp)
			continue;
		ia = ipv6nd_rapfindprefix(rap, pfx, pfxlen);
		if (ia != NULL)
			break;
	}
	return ia;
}

static void
ipv6nd_removefreedrop_ra(struct ra *rap, int remove_ra, int drop_ra)
{

	eloop_timeout_delete(rap->iface->ctx->eloop, NULL, rap->iface);
	eloop_timeout_delete(rap->iface->ctx->eloop, NULL, rap);
	if (remove_ra)
		TAILQ_REMOVE(rap->iface->ctx->ra_routers, rap, next);
	ipv6_freedrop_addrs(&rap->addrs, drop_ra, NULL);
	routeinfohead_free(&rap->rinfos);
	free(rap->data);
	free(rap);
}

static void
ipv6nd_freedrop_ra(struct ra *rap, int drop)
{

	ipv6nd_removefreedrop_ra(rap, 1, drop);
}

ssize_t
ipv6nd_free(struct interface *ifp)
{
	struct rs_state *state;
	struct ra *rap, *ran;
	struct dhcpcd_ctx *ctx;
	ssize_t n;

	state = RS_STATE(ifp);
	if (state == NULL)
		return 0;

	ctx = ifp->ctx;
#ifdef __sun
	eloop_event_delete(ctx->eloop, state->nd_fd);
	close(state->nd_fd);
#endif
	free(state->rs);
	free(state);
	ifp->if_data[IF_DATA_IPV6ND] = NULL;
	n = 0;
	TAILQ_FOREACH_SAFE(rap, ifp->ctx->ra_routers, next, ran) {
		if (rap->iface == ifp) {
			ipv6nd_free_ra(rap);
			n++;
		}
	}

#ifndef __sun
	/* If we don't have any more IPv6 enabled interfaces,
	 * close the global socket and release resources */
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (RS_STATE(ifp))
			break;
	}
	if (ifp == NULL) {
		if (ctx->nd_fd != -1) {
			eloop_event_delete(ctx->eloop, ctx->nd_fd);
			close(ctx->nd_fd);
			ctx->nd_fd = -1;
		}
	}
#endif

	return n;
}

static void
ipv6nd_scriptrun(struct ra *rap)
{
	int hasdns, hasaddress;
	struct ipv6_addr *ap;

	hasaddress = 0;
	/* If all addresses have completed DAD run the script */
	TAILQ_FOREACH(ap, &rap->addrs, next) {
		if ((ap->flags & (IPV6_AF_AUTOCONF | IPV6_AF_ADDED)) ==
		    (IPV6_AF_AUTOCONF | IPV6_AF_ADDED))
		{
			hasaddress = 1;
			if (!(ap->flags & IPV6_AF_DADCOMPLETED) &&
			    ipv6_iffindaddr(ap->iface, &ap->addr,
			    IN6_IFF_TENTATIVE))
				ap->flags |= IPV6_AF_DADCOMPLETED;
			if ((ap->flags & IPV6_AF_DADCOMPLETED) == 0) {
				logdebugx("%s: waiting for Router Advertisement"
				    " DAD to complete",
				    rap->iface->name);
				return;
			}
		}
	}

	/* If we don't require RDNSS then set hasdns = 1 so we fork */
	if (!(rap->iface->options->options & DHCPCD_IPV6RA_REQRDNSS))
		hasdns = 1;
	else {
		hasdns = rap->hasdns;
	}

	script_runreason(rap->iface, "ROUTERADVERT");
	if (hasdns && (hasaddress ||
	    !(rap->flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER))))
		dhcpcd_daemonise(rap->iface->ctx);
#if 0
	else if (options & DHCPCD_DAEMONISE &&
	    !(options & DHCPCD_DAEMONISED) && new_data)
		logwarnx("%s: did not fork due to an absent"
		    " RDNSS option in the RA",
		    ifp->name);
#endif
}

static void
ipv6nd_addaddr(void *arg)
{
	struct ipv6_addr *ap = arg;

	ipv6_addaddr(ap, NULL);
}

int
ipv6nd_dadcompleted(const struct interface *ifp)
{
	const struct ra *rap;
	const struct ipv6_addr *ap;

	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface != ifp)
			continue;
		TAILQ_FOREACH(ap, &rap->addrs, next) {
			if (ap->flags & IPV6_AF_AUTOCONF &&
			    ap->flags & IPV6_AF_ADDED &&
			    !(ap->flags & IPV6_AF_DADCOMPLETED))
				return 0;
		}
	}
	return 1;
}

static void
ipv6nd_dadcallback(void *arg)
{
	struct ipv6_addr *ia = arg, *rapap;
	struct interface *ifp;
	struct ra *rap;
	int wascompleted, found;
	char buf[INET6_ADDRSTRLEN];
	const char *p;
	int dadcounter;

	ifp = ia->iface;
	wascompleted = (ia->flags & IPV6_AF_DADCOMPLETED);
	ia->flags |= IPV6_AF_DADCOMPLETED;
	if (ia->addr_flags & IN6_IFF_DUPLICATED) {
		ia->dadcounter++;
		logwarnx("%s: DAD detected %s", ifp->name, ia->saddr);

		/* Try and make another stable private address.
		 * Because ap->dadcounter is always increamented,
		 * a different address is generated. */
		/* XXX Cache DAD counter per prefix/id/ssid? */
		if (ifp->options->options & DHCPCD_SLAACPRIVATE &&
		    IA6_CANAUTOCONF(ia))
		{
			unsigned int delay;

			if (ia->dadcounter >= IDGEN_RETRIES) {
				logerrx("%s: unable to obtain a"
				    " stable private address",
				    ifp->name);
				goto try_script;
			}
			loginfox("%s: deleting address %s",
			    ifp->name, ia->saddr);
			if (if_address6(RTM_DELADDR, ia) == -1 &&
			    errno != EADDRNOTAVAIL && errno != ENXIO)
				logerr(__func__);
			dadcounter = ia->dadcounter;
			if (ipv6_makestableprivate(&ia->addr,
			    &ia->prefix, ia->prefix_len,
			    ifp, &dadcounter) == -1)
			{
				logerr("ipv6_makestableprivate");
				return;
			}
			ia->dadcounter = dadcounter;
			ia->flags &= ~(IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED);
			ia->flags |= IPV6_AF_NEW;
			p = inet_ntop(AF_INET6, &ia->addr, buf, sizeof(buf));
			if (p)
				snprintf(ia->saddr,
				    sizeof(ia->saddr),
				    "%s/%d",
				    p, ia->prefix_len);
			else
				ia->saddr[0] = '\0';
			delay = arc4random_uniform(IDGEN_DELAY * MSEC_PER_SEC);
			eloop_timeout_add_msec(ifp->ctx->eloop, delay,
			    ipv6nd_addaddr, ia);
			return;
		}
	}

try_script:
	if (!wascompleted) {
		TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
			if (rap->iface != ifp)
				continue;
			wascompleted = 1;
			found = 0;
			TAILQ_FOREACH(rapap, &rap->addrs, next) {
				if (rapap->flags & IPV6_AF_AUTOCONF &&
				    rapap->flags & IPV6_AF_ADDED &&
				    (rapap->flags & IPV6_AF_DADCOMPLETED) == 0)
				{
					wascompleted = 0;
					break;
				}
				if (rapap == ia)
					found = 1;
			}

			if (wascompleted && found) {
				logdebugx("%s: Router Advertisement DAD "
				    "completed",
				    rap->iface->name);
				ipv6nd_scriptrun(rap);
			}
		}
#ifdef ND6_ADVERTISE
		ipv6nd_advertise(ia);
#endif
	}
}

static struct ipv6_addr *
ipv6nd_findmarkstale(struct ra *rap, struct ipv6_addr *ia, bool mark)
{
	struct dhcpcd_ctx *ctx = ia->iface->ctx;
	struct ra *rap2;
	struct ipv6_addr *ia2;

	TAILQ_FOREACH(rap2, ctx->ra_routers, next) {
		if (rap2 == rap ||
		    rap2->iface != rap->iface ||
		    rap2->expired)
			continue;
		TAILQ_FOREACH(ia2, &rap2->addrs, next) {
			if (!IN6_ARE_ADDR_EQUAL(&ia->prefix, &ia2->prefix))
				continue;
			if (!(ia2->flags & IPV6_AF_STALE))
				return ia2;
			if (mark)
				ia2->prefix_pltime = 0;
		}
	}
	return NULL;
}

#ifndef DHCP6
/* If DHCPv6 is compiled out, supply a shim to provide an error message
 * if IPv6RA requests DHCPv6. */
enum DH6S {
	DH6S_REQUEST,
	DH6S_INFORM,
};
static int
dhcp6_start(__unused struct interface *ifp, __unused enum DH6S init_state)
{

	errno = ENOTSUP;
	return -1;
}
#endif

static void
ipv6nd_handlera(struct dhcpcd_ctx *ctx,
    const struct sockaddr_in6 *from, const char *sfrom,
    struct interface *ifp, struct icmp6_hdr *icp, size_t len, int hoplimit)
{
	size_t i, olen;
	struct nd_router_advert *nd_ra;
	struct nd_opt_hdr ndo;
	struct nd_opt_prefix_info pi;
	struct nd_opt_mtu mtu;
	struct nd_opt_rdnss rdnss;
	struct nd_opt_ri ri;
	struct routeinfo *rinfo;
	uint8_t *p;
	struct ra *rap;
	struct in6_addr pi_prefix;
	struct ipv6_addr *ia;
	struct dhcp_opt *dho;
	bool new_rap, new_data, has_address;
	uint32_t old_lifetime;
	int ifmtu;
	int loglevel;
	unsigned int flags;
#ifdef IPV6_MANAGETEMPADDR
	bool new_ia;
#endif

	if (ifp == NULL || RS_STATE(ifp) == NULL) {
#ifdef DEBUG_RS
		logdebugx("RA for unexpected interface from %s", sfrom);
#endif
		return;
	}

	if (len < sizeof(struct nd_router_advert)) {
		logerrx("IPv6 RA packet too short from %s", sfrom);
		return;
	}

	/* RFC 4861 7.1.2 */
	if (hoplimit != 255) {
		logerrx("invalid hoplimit(%d) in RA from %s", hoplimit, sfrom);
		return;
	}
	if (!IN6_IS_ADDR_LINKLOCAL(&from->sin6_addr)) {
		logerrx("RA from non local address %s", sfrom);
		return;
	}

	if (!(ifp->options->options & DHCPCD_IPV6RS)) {
#ifdef DEBUG_RS
		logerrx("%s: unexpected RA from %s", ifp->name, sfrom);
#endif
		return;
	}

	/* We could receive a RA before we sent a RS*/
	if (ipv6_linklocal(ifp) == NULL) {
#ifdef DEBUG_RS
		logdebugx("%s: received RA from %s (no link-local)",
		    ifp->name, sfrom);
#endif
		return;
	}

	if (ipv6_iffindaddr(ifp, &from->sin6_addr, IN6_IFF_TENTATIVE)) {
		logdebugx("%s: ignoring RA from ourself %s",
		    ifp->name, sfrom);
		return;
	}

	/*
	 * Because we preserve RA's and expire them quickly after
	 * carrier up, it's important to reset the kernels notion of
	 * reachable timers back to default values before applying
	 * new RA values.
	 */
	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (ifp == rap->iface)
			break;
	}
	if (rap != NULL && rap->willexpire)
		ipv6nd_applyra(ifp);

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (ifp == rap->iface &&
		    IN6_ARE_ADDR_EQUAL(&rap->from, &from->sin6_addr))
			break;
	}

	nd_ra = (struct nd_router_advert *)icp;

	/* We don't want to spam the log with the fact we got an RA every
	 * 30 seconds or so, so only spam the log if it's different. */
	if (rap == NULL || (rap->data_len != len ||
	     memcmp(rap->data, (unsigned char *)icp, rap->data_len) != 0))
	{
		if (rap) {
			free(rap->data);
			rap->data_len = 0;
		}
		new_data = true;
	} else
		new_data = false;
	if (rap == NULL) {
		rap = calloc(1, sizeof(*rap));
		if (rap == NULL) {
			logerr(__func__);
			return;
		}
		rap->iface = ifp;
		rap->from = from->sin6_addr;
		strlcpy(rap->sfrom, sfrom, sizeof(rap->sfrom));
		TAILQ_INIT(&rap->addrs);
		TAILQ_INIT(&rap->rinfos);
		new_rap = true;
		rap->isreachable = true;
	} else
		new_rap = false;
	if (rap->data_len == 0) {
		rap->data = malloc(len);
		if (rap->data == NULL) {
			logerr(__func__);
			if (new_rap)
				free(rap);
			return;
		}
		memcpy(rap->data, icp, len);
		rap->data_len = len;
	}

	/* We could change the debug level based on new_data, but some
	 * routers like to decrease the advertised valid and preferred times
	 * in accordance with the own prefix times which would result in too
	 * much needless log spam. */
	if (rap->willexpire)
		new_data = true;
	loglevel = new_rap || rap->willexpire || !rap->isreachable ?
	    LOG_INFO : LOG_DEBUG;
	logmessage(loglevel, "%s: Router Advertisement from %s",
	    ifp->name, rap->sfrom);

	clock_gettime(CLOCK_MONOTONIC, &rap->acquired);
	rap->flags = nd_ra->nd_ra_flags_reserved;
	old_lifetime = rap->lifetime;
	rap->lifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	if (nd_ra->nd_ra_curhoplimit != 0)
		rap->hoplimit = nd_ra->nd_ra_curhoplimit;
	else
		rap->hoplimit = IPV6_DEFHLIM;
	if (nd_ra->nd_ra_reachable != 0) {
		rap->reachable = ntohl(nd_ra->nd_ra_reachable);
		if (rap->reachable > MAX_REACHABLE_TIME)
			rap->reachable = 0;
	} else
		rap->reachable = REACHABLE_TIME;
	if (nd_ra->nd_ra_retransmit != 0)
		rap->retrans = ntohl(nd_ra->nd_ra_retransmit);
	else
		rap->retrans = RETRANS_TIMER;
	rap->expired = rap->willexpire = rap->doexpire = false;
	rap->hasdns = false;
	rap->isreachable = true;
	has_address = false;
	rap->mtu = 0;

#ifdef IPV6_AF_TEMPORARY
	ipv6_markaddrsstale(ifp, IPV6_AF_TEMPORARY);
#endif
	TAILQ_FOREACH(ia, &rap->addrs, next) {
		ia->flags |= IPV6_AF_STALE;
	}

	len -= sizeof(struct nd_router_advert);
	p = ((uint8_t *)icp) + sizeof(struct nd_router_advert);
	for (; len > 0; p += olen, len -= olen) {
		if (len < sizeof(ndo)) {
			logerrx("%s: short option", ifp->name);
			break;
		}
		memcpy(&ndo, p, sizeof(ndo));
		olen = (size_t)ndo.nd_opt_len * 8;
		if (olen == 0) {
			logerrx("%s: zero length option", ifp->name);
			break;
		}
		if (olen > len) {
			logerrx("%s: option length exceeds message",
			    ifp->name);
			break;
		}

		if (has_option_mask(ifp->options->rejectmasknd,
		    ndo.nd_opt_type))
		{
			for (i = 0, dho = ctx->nd_opts;
			    i < ctx->nd_opts_len;
			    i++, dho++)
			{
				if (dho->option == ndo.nd_opt_type)
					break;
			}
			if (dho != NULL)
				logwarnx("%s: reject RA (option %s) from %s",
				    ifp->name, dho->var, rap->sfrom);
			else
				logwarnx("%s: reject RA (option %d) from %s",
				    ifp->name, ndo.nd_opt_type, rap->sfrom);
			if (new_rap)
				ipv6nd_removefreedrop_ra(rap, 0, 0);
			else
				ipv6nd_free_ra(rap);
			return;
		}

		if (has_option_mask(ifp->options->nomasknd, ndo.nd_opt_type))
			continue;

		switch (ndo.nd_opt_type) {
		case ND_OPT_PREFIX_INFORMATION:
		{
			uint32_t vltime, pltime;

			loglevel = new_data ? LOG_ERR : LOG_DEBUG;
			if (ndo.nd_opt_len != 4) {
				logmessage(loglevel,
				    "%s: invalid option len for prefix",
				    ifp->name);
				continue;
			}
			memcpy(&pi, p, sizeof(pi));
			if (pi.nd_opt_pi_prefix_len > 128) {
				logmessage(loglevel, "%s: invalid prefix len",
				    ifp->name);
				continue;
			}
			/* nd_opt_pi_prefix is not aligned. */
			memcpy(&pi_prefix, &pi.nd_opt_pi_prefix,
			    sizeof(pi_prefix));
			if (IN6_IS_ADDR_MULTICAST(&pi_prefix) ||
			    IN6_IS_ADDR_LINKLOCAL(&pi_prefix))
			{
				logmessage(loglevel, "%s: invalid prefix in RA",
				    ifp->name);
				continue;
			}

			vltime = ntohl(pi.nd_opt_pi_valid_time);
			pltime = ntohl(pi.nd_opt_pi_preferred_time);
			if (pltime > vltime) {
				logmessage(loglevel, "%s: pltime > vltime",
				    ifp->name);
				continue;
			}

			flags = IPV6_AF_RAPFX;
			/* If no flags are set, that means the prefix is
			 * available via the router. */
			if (pi.nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK)
				flags |= IPV6_AF_ONLINK;
			if (pi.nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO &&
			    rap->iface->options->options &
			    DHCPCD_IPV6RA_AUTOCONF)
				flags |= IPV6_AF_AUTOCONF;
			if (pi.nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ROUTER)
				flags |= IPV6_AF_ROUTER;

			ia = ipv6nd_rapfindprefix(rap,
			    &pi_prefix, pi.nd_opt_pi_prefix_len);
			if (ia == NULL) {
				ia = ipv6_newaddr(rap->iface,
				    &pi_prefix, pi.nd_opt_pi_prefix_len, flags);
				if (ia == NULL)
					break;

				ia->prefix = pi_prefix;
				ia->created = ia->acquired = rap->acquired;
				ia->prefix_vltime = vltime;
				ia->prefix_pltime = pltime;

				if (flags & IPV6_AF_AUTOCONF)
					ia->dadcallback = ipv6nd_dadcallback;

				TAILQ_INSERT_TAIL(&rap->addrs, ia, next);

#ifdef IPV6_MANAGETEMPADDR
				/* New address to dhcpcd RA handling.
				 * If the address already exists and a valid
				 * temporary address also exists then
				 * extend the existing one rather than
				 * create a new one */
				if (flags & IPV6_AF_AUTOCONF &&
				    ipv6_iffindaddr(ifp, &ia->addr,
				    IN6_IFF_NOTUSEABLE) &&
				    ipv6_settemptime(ia, 0))
					new_ia = false;
				else
					new_ia = true;
#endif

			} else {
				uint32_t rmtime;

				/*
				 * RFC 4862 5.5.3.e
				 * Don't terminate existing connections.
				 * This means that to actually remove the
				 * existing prefix, the RA needs to stop
				 * broadcasting the prefix and just let it
				 * expire in 2 hours.
				 * It might want to broadcast it to reduce
				 * the vltime if it was greater than 2 hours
				 * to start with/
				 */
				ia->prefix_pltime = pltime;
				if (ia->prefix_vltime) {
					uint32_t elapsed;

					elapsed = (uint32_t)eloop_timespec_diff(
						&rap->acquired, &ia->acquired,
						NULL);
					rmtime = ia->prefix_vltime - elapsed;
					if (rmtime > ia->prefix_vltime)
						rmtime = 0;
				} else
					rmtime = 0;
				if (vltime > MIN_EXTENDED_VLTIME ||
				    vltime > rmtime)
					ia->prefix_vltime = vltime;
				else if (rmtime <= MIN_EXTENDED_VLTIME)
					/* No SEND support from RFC 3971 so
					 * leave vltime alone */
					ia->prefix_vltime = rmtime;
				else
					ia->prefix_vltime = MIN_EXTENDED_VLTIME;

				/* Ensure pltime still fits */
				if (pltime < ia->prefix_vltime)
					ia->prefix_pltime = pltime;
				else
					ia->prefix_pltime = ia->prefix_vltime;

				ia->flags |= flags;
				ia->flags &= ~IPV6_AF_STALE;
				ia->acquired = rap->acquired;

#ifdef IPV6_MANAGETEMPADDR
				new_ia = false;
#endif
			}

			if (ia->prefix_vltime != 0 &&
			    ia->flags & IPV6_AF_AUTOCONF)
				has_address = true;

#ifdef IPV6_MANAGETEMPADDR
			/* RFC4941 Section 3.3.3 */
			if (ia->flags & IPV6_AF_AUTOCONF &&
			    ia->iface->options->options & DHCPCD_SLAACTEMP &&
			    IA6_CANAUTOCONF(ia))
			{
				if (!new_ia) {
					if (ipv6_settemptime(ia, 1) == NULL)
						new_ia = true;
				}
				if (new_ia && ia->prefix_pltime) {
					if (ipv6_createtempaddr(ia,
					    &ia->acquired) == NULL)
						logerr("ipv6_createtempaddr");
				}
			}
#endif
			break;
		}

		case ND_OPT_MTU:
			if (len < sizeof(mtu)) {
				logmessage(loglevel, "%s: short MTU option", ifp->name);
				break;
			}
			memcpy(&mtu, p, sizeof(mtu));
			mtu.nd_opt_mtu_mtu = ntohl(mtu.nd_opt_mtu_mtu);
			if (mtu.nd_opt_mtu_mtu < IPV6_MMTU) {
				logmessage(loglevel, "%s: invalid MTU %d",
				    ifp->name, mtu.nd_opt_mtu_mtu);
				break;
			}
			ifmtu = if_getmtu(ifp);
			if (ifmtu == -1)
				logerr("if_getmtu");
			else if (mtu.nd_opt_mtu_mtu > (uint32_t)ifmtu) {
				logmessage(loglevel, "%s: advertised MTU %d"
				    " is greater than link MTU %d",
				    ifp->name, mtu.nd_opt_mtu_mtu, ifmtu);
				rap->mtu = (uint32_t)ifmtu;
			} else
				rap->mtu = mtu.nd_opt_mtu_mtu;
			break;
		case ND_OPT_RDNSS:
			if (len < sizeof(rdnss)) {
				logmessage(loglevel, "%s: short RDNSS option", ifp->name);
				break;
			}
			memcpy(&rdnss, p, sizeof(rdnss));
			if (rdnss.nd_opt_rdnss_lifetime &&
			    rdnss.nd_opt_rdnss_len > 1)
				rap->hasdns = 1;
			break;
		case ND_OPT_RI:
			if (ndo.nd_opt_len > 3) {
				logmessage(loglevel, "%s: invalid route info option",
				    ifp->name);
				break;
			}
			memset(&ri, 0, sizeof(ri));
			memcpy(&ri, p, olen); /* may be smaller than sizeof(ri), pad with zero */
			if(ri.nd_opt_ri_prefixlen > 128) {
				logmessage(loglevel, "%s: invalid route info prefix length",
				    ifp->name);
				break;
			}

			/* rfc4191 3.1 - RI for ::/0 applies to default route */
			if(ri.nd_opt_ri_prefixlen == 0) {
				rap->lifetime = ntohl(ri.nd_opt_ri_lifetime);

				/* Update preference leaving other flags intact */
				rap->flags = ((rap->flags & (~ (unsigned int)ND_RA_FLAG_RTPREF_MASK))
					| ri.nd_opt_ri_flags_reserved) & 0xff;

				break;
			}

			/* Update existing route info instead of rebuilding all routes so that
			previously announced but now absent routes can stay alive.  To kill a
			route early, an RI with lifetime=0 needs to be received (rfc4191 3.1)*/
			rinfo = routeinfo_findalloc(rap, &ri.nd_opt_ri_prefix, ri.nd_opt_ri_prefixlen);
			if(rinfo == NULL) {
				logerr(__func__);
				break;
			}

			/* Update/initialize other route info params */
			rinfo->flags = ri.nd_opt_ri_flags_reserved;
			rinfo->lifetime = ntohl(ri.nd_opt_ri_lifetime);
			rinfo->acquired = rap->acquired;

			break;
		default:
			continue;
		}
	}

	for (i = 0, dho = ctx->nd_opts;
	    i < ctx->nd_opts_len;
	    i++, dho++)
	{
		if (has_option_mask(ifp->options->requiremasknd,
		    dho->option))
		{
			logwarnx("%s: reject RA (no option %s) from %s",
			    ifp->name, dho->var, rap->sfrom);
			if (new_rap)
				ipv6nd_removefreedrop_ra(rap, 0, 0);
			else
				ipv6nd_free_ra(rap);
			return;
		}
	}

	TAILQ_FOREACH(ia, &rap->addrs, next) {
		if (!(ia->flags & IPV6_AF_STALE) || ia->prefix_pltime == 0)
			continue;
		if (ipv6nd_findmarkstale(rap, ia, false) != NULL)
			continue;
		ipv6nd_findmarkstale(rap, ia, true);
		logdebugx("%s: %s: became stale", ifp->name, ia->saddr);
		/* Technically this violates RFC 4861 6.3.4,
		 * but we need a mechanism to tell the kernel to
		 * try and prefer other addresses. */
		ia->prefix_pltime = 0;
	}

	if (!new_rap && rap->lifetime == 0 && old_lifetime != 0)
		logwarnx("%s: %s: no longer a default router (lifetime = 0)",
		    ifp->name, rap->sfrom);

	if (new_data && !has_address && rap->lifetime && !ipv6_anyglobal(ifp))
		logwarnx("%s: no global addresses for default route",
		    ifp->name);

	if (new_rap)
		TAILQ_INSERT_TAIL(ctx->ra_routers, rap, next);
	if (new_data)
		ipv6nd_sortrouters(ifp->ctx);

	if (ifp->ctx->options & DHCPCD_TEST) {
		script_runreason(ifp, "TEST");
		goto handle_flag;
	}

	if (!(ifp->options->options & DHCPCD_CONFIGURE))
		goto run;

	ipv6nd_applyra(ifp);
	ipv6_addaddrs(&rap->addrs);
#ifdef IPV6_MANAGETEMPADDR
	ipv6_addtempaddrs(ifp, &rap->acquired);
#endif
	rt_build(ifp->ctx, AF_INET6);

run:
	ipv6nd_scriptrun(rap);

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	eloop_timeout_delete(ifp->ctx->eloop, NULL, rap); /* reachable timer */

handle_flag:
	if (!(ifp->options->options & DHCPCD_DHCP6))
		goto nodhcp6;
/* Only log a DHCPv6 start error if compiled in or debugging is enabled. */
#ifdef DHCP6
#define LOG_DHCP6	logerr
#else
#define LOG_DHCP6	logdebug
#endif
	if (rap->flags & ND_RA_FLAG_MANAGED) {
		if (new_data && dhcp6_start(ifp, DH6S_REQUEST) == -1)
			LOG_DHCP6("dhcp6_start: %s", ifp->name);
	} else if (rap->flags & ND_RA_FLAG_OTHER) {
		if (new_data && dhcp6_start(ifp, DH6S_INFORM) == -1)
			LOG_DHCP6("dhcp6_start: %s", ifp->name);
	} else {
#ifdef DHCP6
		if (new_data)
			logdebugx("%s: No DHCPv6 instruction in RA", ifp->name);
#endif
nodhcp6:
		if (ifp->ctx->options & DHCPCD_TEST) {
			eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
			return;
		}
	}

	/* Expire should be called last as the rap object could be destroyed */
	ipv6nd_expirera(ifp);
}

bool
ipv6nd_hasralifetime(const struct interface *ifp, bool lifetime)
{
	const struct ra *rap;

	if (ifp->ctx->ra_routers) {
		TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next)
			if (rap->iface == ifp &&
			    !rap->expired &&
			    (!lifetime ||rap->lifetime))
				return true;
	}
	return false;
}

bool
ipv6nd_hasradhcp(const struct interface *ifp, bool managed)
{
	const struct ra *rap;

	if (ifp->ctx->ra_routers) {
		TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
			if (rap->iface == ifp &&
			    !rap->expired && !rap->willexpire &&
			    ((managed && rap->flags & ND_RA_FLAG_MANAGED) ||
			    (!managed && rap->flags & ND_RA_FLAG_OTHER)))
				return true;
		}
	}
	return false;
}

static const uint8_t *
ipv6nd_getoption(struct dhcpcd_ctx *ctx,
    size_t *os, unsigned int *code, size_t *len,
    const uint8_t *od, size_t ol, struct dhcp_opt **oopt)
{
	struct nd_opt_hdr ndo;
	size_t i;
	struct dhcp_opt *opt;

	if (od) {
		*os = sizeof(ndo);
		if (ol < *os) {
			errno = EINVAL;
			return NULL;
		}
		memcpy(&ndo, od, sizeof(ndo));
		i = (size_t)(ndo.nd_opt_len * 8);
		if (i > ol) {
			errno = EINVAL;
			return NULL;
		}
		*len = i;
		*code = ndo.nd_opt_type;
	}

	for (i = 0, opt = ctx->nd_opts;
	    i < ctx->nd_opts_len; i++, opt++)
	{
		if (opt->option == *code) {
			*oopt = opt;
			break;
		}
	}

	if (od)
		return od + sizeof(ndo);
	return NULL;
}

ssize_t
ipv6nd_env(FILE *fp, const struct interface *ifp)
{
	size_t i, j, n, len, olen;
	struct ra *rap;
	char ndprefix[32];
	struct dhcp_opt *opt;
	uint8_t *p;
	struct nd_opt_hdr ndo;
	struct ipv6_addr *ia;
	struct timespec now;
	int pref;

	clock_gettime(CLOCK_MONOTONIC, &now);
	i = n = 0;
	TAILQ_FOREACH(rap, ifp->ctx->ra_routers, next) {
		if (rap->iface != ifp || rap->expired)
			continue;
		i++;
		snprintf(ndprefix, sizeof(ndprefix), "nd%zu", i);
		if (efprintf(fp, "%s_from=%s", ndprefix, rap->sfrom) == -1)
			return -1;
		if (efprintf(fp, "%s_acquired=%lld", ndprefix,
		    (long long)rap->acquired.tv_sec) == -1)
			return -1;
		if (efprintf(fp, "%s_now=%lld", ndprefix,
		    (long long)now.tv_sec) == -1)
			return -1;
		if (efprintf(fp, "%s_hoplimit=%u", ndprefix, rap->hoplimit) == -1)
			return -1;
		pref = ipv6nd_rtpref(rap->flags);
		if (efprintf(fp, "%s_flags=%s%s%s%s%s", ndprefix,
		    rap->flags & ND_RA_FLAG_MANAGED    ? "M" : "",
		    rap->flags & ND_RA_FLAG_OTHER      ? "O" : "",
		    rap->flags & ND_RA_FLAG_HOME_AGENT ? "H" : "",
		    pref == RTPREF_HIGH ? "h" : pref == RTPREF_LOW ? "l" : "",
		    rap->flags & ND_RA_FLAG_PROXY      ? "P" : "") == -1)
			return -1;
		if (efprintf(fp, "%s_lifetime=%u", ndprefix, rap->lifetime) == -1)
			return -1;

		/* Zero our indexes */
		for (j = 0, opt = rap->iface->ctx->nd_opts;
		    j < rap->iface->ctx->nd_opts_len;
		    j++, opt++)
			dhcp_zero_index(opt);
		for (j = 0, opt = rap->iface->options->nd_override;
		    j < rap->iface->options->nd_override_len;
		    j++, opt++)
			dhcp_zero_index(opt);

		/* Unlike DHCP, ND6 options *may* occur more than once.
		 * There is also no provision for option concatenation
		 * unlike DHCP. */
		len = rap->data_len - sizeof(struct nd_router_advert);
		for (p = rap->data + sizeof(struct nd_router_advert);
		    len >= sizeof(ndo);
		    p += olen, len -= olen)
		{
			memcpy(&ndo, p, sizeof(ndo));
			olen = (size_t)(ndo.nd_opt_len * 8);
			if (olen > len) {
				errno =	EINVAL;
				break;
			}
			if (has_option_mask(rap->iface->options->nomasknd,
			    ndo.nd_opt_type))
				continue;
			for (j = 0, opt = rap->iface->options->nd_override;
			    j < rap->iface->options->nd_override_len;
			    j++, opt++)
				if (opt->option == ndo.nd_opt_type)
					break;
			if (j == rap->iface->options->nd_override_len) {
				for (j = 0, opt = rap->iface->ctx->nd_opts;
				    j < rap->iface->ctx->nd_opts_len;
				    j++, opt++)
					if (opt->option == ndo.nd_opt_type)
						break;
				if (j == rap->iface->ctx->nd_opts_len)
					opt = NULL;
			}
			if (opt == NULL)
				continue;
			dhcp_envoption(rap->iface->ctx, fp,
			    ndprefix, rap->iface->name,
			    opt, ipv6nd_getoption,
			    p + sizeof(ndo), olen - sizeof(ndo));
		}

		/* We need to output the addresses we actually made
		 * from the prefix information options as well. */
		j = 0;
		TAILQ_FOREACH(ia, &rap->addrs, next) {
			if (!(ia->flags & IPV6_AF_AUTOCONF) ||
#ifdef IPV6_AF_TEMPORARY
			    ia->flags & IPV6_AF_TEMPORARY ||
#endif
			    !(ia->flags & IPV6_AF_ADDED) ||
			    ia->prefix_vltime == 0)
				continue;
			if (efprintf(fp, "%s_addr%zu=%s",
			    ndprefix, ++j, ia->saddr) == -1)
				return -1;
		}
	}
	return 1;
}

void
ipv6nd_handleifa(int cmd, struct ipv6_addr *addr, pid_t pid)
{
	struct ra *rap;

	/* IPv6 init may not have happened yet if we are learning
	 * existing addresses when dhcpcd starts. */
	if (addr->iface->ctx->ra_routers == NULL)
		return;

	TAILQ_FOREACH(rap, addr->iface->ctx->ra_routers, next) {
		if (rap->iface != addr->iface)
			continue;
		ipv6_handleifa_addrs(cmd, &rap->addrs, addr, pid);
	}
}

void
ipv6nd_expirera(void *arg)
{
	struct interface *ifp;
	struct ra *rap, *ran;
	struct timespec now;
	uint32_t elapsed;
	bool expired, valid;
	struct ipv6_addr *ia;
	struct routeinfo *rinfo, *rinfob;
	size_t len, olen;
	uint8_t *p;
	struct nd_opt_hdr ndo;
#if 0
	struct nd_opt_prefix_info pi;
#endif
	struct nd_opt_dnssl dnssl;
	struct nd_opt_rdnss rdnss;
	unsigned int next = 0, ltime;
	size_t nexpired = 0;

	ifp = arg;
	clock_gettime(CLOCK_MONOTONIC, &now);
	expired = false;

	TAILQ_FOREACH_SAFE(rap, ifp->ctx->ra_routers, next, ran) {
		if (rap->iface != ifp || rap->expired)
			continue;
		valid = false;
		/* lifetime may be set to infinite by rfc4191 route information */
		if (rap->lifetime && rap->lifetime != ND6_INFINITE_LIFETIME) {
			elapsed = (uint32_t)eloop_timespec_diff(&now,
			    &rap->acquired, NULL);
			if (elapsed >= rap->lifetime || rap->doexpire) {
				if (!rap->expired) {
					logwarnx("%s: %s: router expired",
					    ifp->name, rap->sfrom);
					rap->lifetime = 0;
					expired = true;
				}
			} else {
				valid = true;
				ltime = rap->lifetime - elapsed;
				if (next == 0 || ltime < next)
					next = ltime;
			}
		}

		/* Not every prefix is tied to an address which
		 * the kernel can expire, so we need to handle it ourself.
		 * Also, some OS don't support address lifetimes (Solaris). */
		TAILQ_FOREACH(ia, &rap->addrs, next) {
			if (ia->prefix_vltime == 0)
				continue;
			if (ia->prefix_vltime == ND6_INFINITE_LIFETIME &&
			    !rap->doexpire)
			{
				valid = true;
				continue;
			}
			elapsed = (uint32_t)eloop_timespec_diff(&now,
			    &ia->acquired, NULL);
			if (elapsed >= ia->prefix_vltime || rap->doexpire) {
				if (ia->flags & IPV6_AF_ADDED) {
					logwarnx("%s: expired %s %s",
					    ia->iface->name,
					    ia->flags & IPV6_AF_AUTOCONF ?
					    "address" : "prefix",
					    ia->saddr);
					if (if_address6(RTM_DELADDR, ia)== -1 &&
					    errno != EADDRNOTAVAIL &&
					    errno != ENXIO)
						logerr(__func__);
				}
				ia->prefix_vltime = ia->prefix_pltime = 0;
				ia->flags &=
				    ~(IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED);
				expired = true;
			} else {
				valid = true;
				ltime = ia->prefix_vltime - elapsed;
				if (next == 0 || ltime < next)
					next = ltime;
			}
		}

		/* Expire route information */
		TAILQ_FOREACH_SAFE(rinfo, &rap->rinfos, next, rinfob) {
			if (rinfo->lifetime == ND6_INFINITE_LIFETIME &&
			    !rap->doexpire)
				continue;
			elapsed = (uint32_t)eloop_timespec_diff(&now,
			    &rinfo->acquired, NULL);
			if (elapsed >= rinfo->lifetime || rap->doexpire) {
				logwarnx("%s: expired route %s",
				    rap->iface->name, rinfo->sprefix);
				TAILQ_REMOVE(&rap->rinfos, rinfo, next);
			}
		}

		/* Work out expiry for ND options */
		elapsed = (uint32_t)eloop_timespec_diff(&now,
		    &rap->acquired, NULL);
		len = rap->data_len - sizeof(struct nd_router_advert);
		for (p = rap->data + sizeof(struct nd_router_advert);
		    len >= sizeof(ndo);
		    p += olen, len -= olen)
		{
			memcpy(&ndo, p, sizeof(ndo));
			olen = (size_t)(ndo.nd_opt_len * 8);
			if (olen > len) {
				errno =	EINVAL;
				break;
			}

			if (has_option_mask(rap->iface->options->nomasknd,
			    ndo.nd_opt_type))
				continue;

			switch (ndo.nd_opt_type) {
			/* Prefix info is already checked in the above loop. */
#if 0
			case ND_OPT_PREFIX_INFORMATION:
				if (len < sizeof(pi))
					break;
				memcpy(&pi, p, sizeof(pi));
				ltime = pi.nd_opt_pi_valid_time;
				break;
#endif
			case ND_OPT_DNSSL:
				if (len < sizeof(dnssl))
					continue;
				memcpy(&dnssl, p, sizeof(dnssl));
				ltime = dnssl.nd_opt_dnssl_lifetime;
				break;
			case ND_OPT_RDNSS:
				if (len < sizeof(rdnss))
					continue;
				memcpy(&rdnss, p, sizeof(rdnss));
				ltime = rdnss.nd_opt_rdnss_lifetime;
				break;
			default:
				continue;
			}

			if (ltime == 0)
				continue;
			if (rap->doexpire) {
				expired = true;
				continue;
			}
			if (ltime == ND6_INFINITE_LIFETIME) {
				valid = true;
				continue;
			}

			ltime = ntohl(ltime);
			if (elapsed >= ltime) {
				expired = true;
				continue;
			}

			valid = true;
			ltime -= elapsed;
			if (next == 0 || ltime < next)
				next = ltime;
		}

		if (valid)
			continue;

		/* Router has expired. Let's not keep a lot of them. */
		rap->expired = true;
		if (++nexpired > EXPIRED_MAX)
			ipv6nd_free_ra(rap);
	}

	if (next != 0)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    next, ipv6nd_expirera, ifp);
	if (expired) {
		logwarnx("%s: part of a Router Advertisement expired",
		    ifp->name);
		ipv6nd_sortrouters(ifp->ctx);
		ipv6nd_applyra(ifp);
		rt_build(ifp->ctx, AF_INET6);
		script_runreason(ifp, "ROUTERADVERT");
	}
}

void
ipv6nd_drop(struct interface *ifp)
{
	struct ra *rap, *ran;
	bool expired = false;

	if (ifp->ctx->ra_routers == NULL)
		return;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	TAILQ_FOREACH_SAFE(rap, ifp->ctx->ra_routers, next, ran) {
		if (rap->iface == ifp) {
			rap->expired = expired = true;
			ipv6nd_drop_ra(rap);
		}
	}
	if (expired) {
		ipv6nd_applyra(ifp);
		rt_build(ifp->ctx, AF_INET6);
		if ((ifp->options->options & DHCPCD_NODROP) != DHCPCD_NODROP)
			script_runreason(ifp, "ROUTERADVERT");
	}
}

void
ipv6nd_recvmsg(struct dhcpcd_ctx *ctx, struct msghdr *msg)
{
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)msg->msg_name;
	char sfrom[INET6_ADDRSTRLEN];
	int hoplimit = 0;
	struct icmp6_hdr *icp;
	struct interface *ifp;
	size_t len = msg->msg_iov[0].iov_len;

	inet_ntop(AF_INET6, &from->sin6_addr, sfrom, sizeof(sfrom));
	if ((size_t)len < sizeof(struct icmp6_hdr)) {
		logerrx("IPv6 ICMP packet too short from %s", sfrom);
		return;
	}

	ifp = if_findifpfromcmsg(ctx, msg, &hoplimit);
	if (ifp == NULL) {
		logerr(__func__);
		return;
	}

	/* Don't do anything if the user hasn't configured it. */
	if (ifp->active != IF_ACTIVE_USER ||
	    !(ifp->options->options & DHCPCD_IPV6))
		return;

	icp = (struct icmp6_hdr *)msg->msg_iov[0].iov_base;
	if (icp->icmp6_code == 0) {
		switch(icp->icmp6_type) {
			case ND_ROUTER_ADVERT:
				ipv6nd_handlera(ctx, from, sfrom,
				    ifp, icp, (size_t)len, hoplimit);
				return;
		}
	}

	logerrx("invalid IPv6 type %d or code %d from %s",
	    icp->icmp6_type, icp->icmp6_code, sfrom);
}

static void
ipv6nd_handledata(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx;
	int fd;
	struct sockaddr_in6 from;
	union {
		struct icmp6_hdr hdr;
		uint8_t buf[64 * 1024]; /* Maximum ICMPv6 size */
	} iovbuf;
	struct iovec iov = {
		.iov_base = iovbuf.buf, .iov_len = sizeof(iovbuf.buf),
	};
	union {
		struct cmsghdr hdr;
		uint8_t buf[CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		    CMSG_SPACE(sizeof(int))];
	} cmsgbuf = { .buf = { 0 } };
	struct msghdr msg = {
	    .msg_name = &from, .msg_namelen = sizeof(from),
	    .msg_iov = &iov, .msg_iovlen = 1,
	    .msg_control = cmsgbuf.buf, .msg_controllen = sizeof(cmsgbuf.buf),
	};
	ssize_t len;

#ifdef __sun
	struct interface *ifp;
	struct rs_state *state;

	ifp = arg;
	state = RS_STATE(ifp);
	ctx = ifp->ctx;
	fd = state->nd_fd;
#else
	ctx = arg;
	fd = ctx->nd_fd;
#endif

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = recvmsg(fd, &msg, 0);
	if (len == -1) {
		logerr(__func__);
		return;
	}

	iov.iov_len = (size_t)len;
	ipv6nd_recvmsg(ctx, &msg);
}

static void
ipv6nd_startrs1(void *arg)
{
	struct interface *ifp = arg;
	struct rs_state *state;

	loginfox("%s: soliciting an IPv6 router", ifp->name);
	state = RS_STATE(ifp);
	if (state == NULL) {
		ifp->if_data[IF_DATA_IPV6ND] = calloc(1, sizeof(*state));
		state = RS_STATE(ifp);
		if (state == NULL) {
			logerr(__func__);
			return;
		}
#ifdef __sun
		state->nd_fd = -1;
#endif
	}

	/* Always make a new probe as the underlying hardware
	 * address could have changed. */
	ipv6nd_makersprobe(ifp);
	if (state->rs == NULL) {
		logerr(__func__);
		return;
	}

	state->retrans = RETRANS_TIMER;
	state->rsprobes = 0;
	ipv6nd_sendrsprobe(ifp);
}

void
ipv6nd_startrs(struct interface *ifp)
{
	unsigned int delay;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	if (!(ifp->options->options & DHCPCD_INITIAL_DELAY)) {
		ipv6nd_startrs1(ifp);
		return;
	}

	delay = arc4random_uniform(MAX_RTR_SOLICITATION_DELAY * MSEC_PER_SEC);
	logdebugx("%s: delaying IPv6 router solicitation for %0.1f seconds",
	    ifp->name, (float)delay / MSEC_PER_SEC);
	eloop_timeout_add_msec(ifp->ctx->eloop, delay, ipv6nd_startrs1, ifp);
	return;
}

static struct routeinfo *routeinfo_findalloc(struct ra *rap, const struct in6_addr *prefix, uint8_t prefix_len)
{
	struct routeinfo *ri;
	char buf[INET6_ADDRSTRLEN];
	const char *p;

	TAILQ_FOREACH(ri, &rap->rinfos, next) {
		if (ri->prefix_len == prefix_len &&
		    IN6_ARE_ADDR_EQUAL(&ri->prefix, prefix))
			return ri;
	}

	ri = malloc(sizeof(struct routeinfo));
	if (ri == NULL)
		return NULL;

	memcpy(&ri->prefix, prefix, sizeof(ri->prefix));
	ri->prefix_len = prefix_len;
	p = inet_ntop(AF_INET6, prefix, buf, sizeof(buf));
	if (p)
		snprintf(ri->sprefix,
			sizeof(ri->sprefix),
			"%s/%d",
			p, prefix_len);
	else
		ri->sprefix[0] = '\0';
	TAILQ_INSERT_TAIL(&rap->rinfos, ri, next);
	return ri;
}

static void routeinfohead_free(struct routeinfohead *head)
{
	struct routeinfo *ri;

	while ((ri = TAILQ_FIRST(head))) {
		TAILQ_REMOVE(head, ri, next);
		free(ri);
	}
}
