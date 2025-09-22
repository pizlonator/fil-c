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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <fcntl.h> /* Needs to be here for old Linux */

#include "config.h"

#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#ifdef AF_LINK
#  include <net/if_dl.h>
#  include <net/if_types.h>
#  include <netinet/in_var.h>
#  undef AF_PACKET	/* Newer Illumos defines this */
#endif
#ifdef AF_PACKET
#  include <netpacket/packet.h>
#endif
#ifdef SIOCGIFMEDIA
#  include <net/if_media.h>
#endif
#include <net/route.h>

#include <ctype.h>
#include <errno.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <fnmatch.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define ELOOP_QUEUE	ELOOP_IF
#include "common.h"
#include "eloop.h"
#include "dev.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"

void
if_free(struct interface *ifp)
{

	if (ifp == NULL)
		return;
#ifdef IPV4LL
	ipv4ll_free(ifp);
#endif
#ifdef INET
	dhcp_free(ifp);
	ipv4_free(ifp);
#endif
#ifdef DHCP6
	dhcp6_free(ifp);
#endif
#ifdef INET6
	ipv6nd_free(ifp);
	ipv6_free(ifp);
#endif
	rt_freeif(ifp);
	free_options(ifp->ctx, ifp->options);
	free(ifp);
}

int
if_opensockets(struct dhcpcd_ctx *ctx)
{

	if (if_opensockets_os(ctx) == -1)
		return -1;

#ifdef IFLR_ACTIVE
	ctx->pf_link_fd = xsocket(PF_LINK, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (ctx->pf_link_fd == -1)
		return -1;
#ifdef HAVE_CAPSICUM
	if (ps_rights_limit_ioctl(ctx->pf_link_fd) == -1)
		return -1;
#endif
#endif

	/* We use this socket for some operations without INET. */
	ctx->pf_inet_fd = xsocket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (ctx->pf_inet_fd == -1)
		return -1;

	return 0;
}

void
if_closesockets(struct dhcpcd_ctx *ctx)
{

	if (ctx->link_fd != -1) {
		eloop_event_delete(ctx->eloop, ctx->link_fd);
		close(ctx->link_fd);
		ctx->link_fd = -1;
	}

	if (ctx->pf_inet_fd != -1) {
		close(ctx->pf_inet_fd);
		ctx->pf_inet_fd = -1;
	}

	if_closesockets_os(ctx);
}

int
if_ioctl(struct dhcpcd_ctx *ctx, ioctl_request_t req, void *data, size_t len)
{

#ifdef PRIVSEP
	if (ctx->options & DHCPCD_PRIVSEP)
		return (int)ps_root_ioctl(ctx, req, data, len);
#endif
	return ioctl(ctx->pf_inet_fd, req, data, len);
}

int
if_getflags(struct interface *ifp)
{
	struct ifreq ifr = { .ifr_flags = 0 };

	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFFLAGS, &ifr) == -1)
		return -1;
	ifp->flags = (unsigned int)ifr.ifr_flags;
	return 0;
}

int
if_setflag(struct interface *ifp, short setflag, short unsetflag)
{
	struct ifreq ifr = { .ifr_flags = 0 };
	short oflags;

	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFFLAGS, &ifr) == -1)
		return -1;

	oflags = ifr.ifr_flags;
	ifr.ifr_flags |= setflag;
	ifr.ifr_flags &= (short)~unsetflag;
	if (ifr.ifr_flags != oflags &&
	    if_ioctl(ifp->ctx, SIOCSIFFLAGS, &ifr, sizeof(ifr)) == -1)
		return -1;

	/*
	 * Do NOT set ifp->flags here.
	 * We need to listen for flag updates from the kernel as they
	 * need to sync with carrier.
	 */
	return 0;
}

bool
if_is_link_up(const struct interface *ifp)
{

	return ifp->flags & IFF_UP &&
	    (ifp->carrier != LINK_DOWN ||
	     (ifp->options != NULL && !(ifp->options->options & DHCPCD_LINK)));
}

int
if_randomisemac(struct interface *ifp)
{
	uint32_t randnum;
	size_t hwlen = ifp->hwlen, rlen = 0;
	uint8_t buf[HWADDR_LEN], *bp = buf, *rp = (uint8_t *)&randnum;
	char sbuf[HWADDR_LEN * 3];
	int retval;

	if (hwlen == 0) {
		errno = ENOTSUP;
		return -1;
	}
	if (hwlen > sizeof(buf)) {
		errno = ENOBUFS;
		return -1;
	}

	for (; hwlen != 0; hwlen--) {
		if (rlen == 0) {
			randnum = arc4random();
			rp = (uint8_t *)&randnum;
			rlen = sizeof(randnum);
		}
		*bp++ = *rp++;
		rlen--;
	}

	/* Unicast address and locally administered. */
	buf[0] &= 0xFC;
	buf[0] |= 0x02;

	logdebugx("%s: hardware address randomised to %s",
	    ifp->name,
	    hwaddr_ntoa(buf, ifp->hwlen, sbuf, sizeof(sbuf)));
	retval = if_setmac(ifp, buf, ifp->hwlen);
	if (retval == 0)
		memcpy(ifp->hwaddr, buf, ifp->hwlen);
	return retval;
}

static int
if_hasconf(struct dhcpcd_ctx *ctx, const char *ifname)
{
	int i;

	for (i = 0; i < ctx->ifcc; i++) {
		if (strcmp(ctx->ifcv[i], ifname) == 0)
			return 1;
	}
	return 0;
}

void
if_markaddrsstale(struct if_head *ifs)
{
	struct interface *ifp;

	TAILQ_FOREACH(ifp, ifs, next) {
#ifdef INET
		ipv4_markaddrsstale(ifp);
#endif
#ifdef INET6
		ipv6_markaddrsstale(ifp, 0);
#endif
	}
}

void
if_learnaddrs(struct dhcpcd_ctx *ctx, struct if_head *ifs,
    struct ifaddrs **ifaddrs)
{
	struct ifaddrs *ifa;
	struct interface *ifp;
#ifdef INET
	const struct sockaddr_in *addr, *net, *brd;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6, *net6;
#endif
	int addrflags;

	for (ifa = *ifaddrs; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
		if ((ifp = if_find(ifs, ifa->ifa_name)) == NULL)
			continue;
#ifdef HAVE_IFADDRS_ADDRFLAGS
		addrflags = (int)ifa->ifa_addrflags;
#endif
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			addr = (void *)ifa->ifa_addr;
			net = (void *)ifa->ifa_netmask;
			if (ifa->ifa_flags & IFF_POINTOPOINT)
				brd = (void *)ifa->ifa_dstaddr;
			else
				brd = (void *)ifa->ifa_broadaddr;
#ifndef HAVE_IFADDRS_ADDRFLAGS
			addrflags = if_addrflags(ifp, &addr->sin_addr,
			    ifa->ifa_name);
			if (addrflags == -1) {
				if (errno != EEXIST && errno != EADDRNOTAVAIL) {
					char dbuf[INET_ADDRSTRLEN];
					const char *dbp;

					dbp = inet_ntop(AF_INET, &addr->sin_addr,
					    dbuf, sizeof(dbuf));
					logerr("%s: if_addrflags: %s%%%s",
					    __func__, dbp, ifp->name);
				}
				continue;
			}
#endif
			ipv4_handleifa(ctx, RTM_NEWADDR, ifs, ifa->ifa_name,
				&addr->sin_addr, &net->sin_addr,
				brd ? &brd->sin_addr : NULL, addrflags, 0);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			sin6 = (void *)ifa->ifa_addr;
			net6 = (void *)ifa->ifa_netmask;

#ifdef __KAME__
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				/* Remove the scope from the address */
				sin6->sin6_addr.s6_addr[2] =
				    sin6->sin6_addr.s6_addr[3] = '\0';
#endif
#ifndef HAVE_IFADDRS_ADDRFLAGS
			addrflags = if_addrflags6(ifp, &sin6->sin6_addr,
			    ifa->ifa_name);
			if (addrflags == -1) {
				if (errno != EEXIST && errno != EADDRNOTAVAIL) {
					char dbuf[INET6_ADDRSTRLEN];
					const char *dbp;

					dbp = inet_ntop(AF_INET6, &sin6->sin6_addr,
					    dbuf, sizeof(dbuf));
					logerr("%s: if_addrflags6: %s%%%s",
					    __func__, dbp, ifp->name);
				}
				continue;
			}
#endif
			ipv6_handleifa(ctx, RTM_NEWADDR, ifs,
			    ifa->ifa_name, &sin6->sin6_addr,
			    ipv6_prefixlen(&net6->sin6_addr), addrflags, 0);
			break;
#endif
		}
	}
}

void if_freeifaddrs(struct dhcpcd_ctx *ctx, struct ifaddrs **ifaddrs)
{
#ifndef PRIVSEP_GETIFADDRS
	UNUSED(ctx);
#endif

	if (ifaddrs == NULL)
		return;

#ifdef PRIVSEP_GETIFADDRS
	if (IN_PRIVSEP(ctx))
		free(*ifaddrs);
	else
#endif
		freeifaddrs(*ifaddrs);
}

void
if_deletestaleaddrs(struct if_head *ifs)
{
	struct interface *ifp;

	TAILQ_FOREACH(ifp, ifs, next) {
#ifdef INET
		ipv4_deletestaleaddrs(ifp);
#endif
#ifdef INET6
		ipv6_deletestaleaddrs(ifp);
#endif
	}
}

bool
if_valid_hwaddr(const uint8_t *hwaddr, size_t hwlen)
{
	size_t i;
	bool all_zeros, all_ones;

	all_zeros = all_ones = true;
	for (i = 0; i < hwlen; i++) {
		if (hwaddr[i] != 0x00)
			all_zeros = false;
		if (hwaddr[i] != 0xff)
			all_ones = false;
		if (!all_zeros && !all_ones)
			return true;
	}
	return false;
}

#if defined(AF_PACKET) && !defined(AF_LINK)
static unsigned int
if_check_arphrd(struct interface *ifp, unsigned int active, bool if_noconf)
{

	switch(ifp->hwtype) {
	case ARPHRD_ETHER:	/* FALLTHROUGH */
	case ARPHRD_IEEE1394:	/* FALLTHROUGH */
	case ARPHRD_INFINIBAND:	/* FALLTHROUGH */
	case ARPHRD_NONE:	/* FALLTHROUGH */
		break;
	case ARPHRD_LOOPBACK:
	case ARPHRD_PPP:
		if (if_noconf && active) {
			logdebugx("%s: ignoring due to interface type and"
			    " no config",
			    ifp->name);
			active = IF_INACTIVE;
		}
		break;
	default:
		if (active) {
			int i;

			if (if_noconf)
				active = IF_INACTIVE;
			i = active ? LOG_WARNING : LOG_DEBUG;
			logmessage(i, "%s: unsupported"
			    " interface type 0x%.2x",
			    ifp->name, ifp->hwtype);
		}
		break;
	}

	return active;
}
#endif

struct if_head *
if_discover(struct dhcpcd_ctx *ctx, struct ifaddrs **ifaddrs,
    int argc, char * const *argv)
{
	struct ifaddrs *ifa;
	int i;
	unsigned int active;
	struct if_head *ifs;
	struct interface *ifp;
	struct if_spec spec;
	bool if_noconf;
#ifdef AF_LINK
	const struct sockaddr_dl *sdl;
#ifdef IFLR_ACTIVE
	struct if_laddrreq iflr = { .flags = IFLR_PREFIX };
#endif
#elif defined(AF_PACKET)
	const struct sockaddr_ll *sll;
#endif
#if defined(SIOCGIFPRIORITY)
	struct ifreq ifr;
#endif

	if ((ifs = malloc(sizeof(*ifs))) == NULL) {
		logerr(__func__);
		return NULL;
	}
	TAILQ_INIT(ifs);

#ifdef PRIVSEP_GETIFADDRS
	if (ctx->options & DHCPCD_PRIVSEP) {
		if (ps_root_getifaddrs(ctx, ifaddrs) == -1) {
			logerr("ps_root_getifaddrs");
			free(ifs);
			return NULL;
		}
	} else
#endif
	if (getifaddrs(ifaddrs) == -1) {
		logerr("getifaddrs");
		free(ifs);
		return NULL;
	}

	for (ifa = *ifaddrs; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != NULL) {
#ifdef AF_LINK
			if (ifa->ifa_addr->sa_family != AF_LINK)
				continue;
#elif defined(AF_PACKET)
			if (ifa->ifa_addr->sa_family != AF_PACKET)
				continue;
#endif
		}
		if (if_nametospec(ifa->ifa_name, &spec) != 0)
			continue;

		/* It's possible for an interface to have >1 AF_LINK.
		 * For our purposes, we use the first one. */
		TAILQ_FOREACH(ifp, ifs, next) {
			if (strcmp(ifp->name, spec.devname) == 0)
				break;
		}
		if (ifp)
			continue;

		if (argc > 0) {
			for (i = 0; i < argc; i++) {
				if (strcmp(argv[i], spec.devname) == 0)
					break;
			}
			active = (i == argc) ? IF_INACTIVE : IF_ACTIVE_USER;
		} else {
			/* -1 means we're discovering against a specific
			 * interface, but we still need the below rules
			 * to apply. */
			if (argc == -1 && strcmp(argv[0], spec.devname) != 0)
				continue;
			active = ctx->options & DHCPCD_INACTIVE ?
			    IF_INACTIVE: IF_ACTIVE_USER;
		}

		for (i = 0; i < ctx->ifdc; i++)
			if (fnmatch(ctx->ifdv[i], spec.devname, 0) == 0)
				break;
		if (i < ctx->ifdc)
			active = IF_INACTIVE;
		for (i = 0; i < ctx->ifc; i++)
			if (fnmatch(ctx->ifv[i], spec.devname, 0) == 0)
				break;
		if (ctx->ifc && i == ctx->ifc)
			active = IF_INACTIVE;
		for (i = 0; i < ctx->ifac; i++)
			if (fnmatch(ctx->ifav[i], spec.devname, 0) == 0)
				break;
		if (ctx->ifac && i == ctx->ifac)
			active = IF_INACTIVE;

#ifdef PLUGIN_DEV
		/* Ensure that the interface name has settled */
		if (!dev_initialised(ctx, spec.devname)) {
			logdebugx("%s: waiting for interface to initialise",
			    spec.devname);
			continue;
		}
#endif

		if (if_vimaster(ctx, spec.devname) == 1) {
			int loglevel = argc != 0 ? LOG_ERR : LOG_DEBUG;
			logmessage(loglevel,
			    "%s: is a Virtual Interface Master, skipping",
			    spec.devname);
			continue;
		}

		if_noconf = ((argc == 0 || argc == -1) && ctx->ifac == 0 &&
		    !if_hasconf(ctx, spec.devname));

		/* Don't allow some reserved interface names unless explicit. */
		if (if_noconf && if_ignore(ctx, spec.devname)) {
			logdebugx("%s: ignoring due to interface type and"
			    " no config", spec.devname);
			active = IF_INACTIVE;
		}

		ifp = calloc(1, sizeof(*ifp));
		if (ifp == NULL) {
			logerr(__func__);
			break;
		}
		ifp->ctx = ctx;
		strlcpy(ifp->name, spec.devname, sizeof(ifp->name));
		ifp->flags = ifa->ifa_flags;

		if (ifa->ifa_addr != NULL) {
#ifdef AF_LINK
			sdl = (const void *)ifa->ifa_addr;

#ifdef IFLR_ACTIVE
			/* We need to check for active address */
			strlcpy(iflr.iflr_name, ifp->name,
			    sizeof(iflr.iflr_name));
			memcpy(&iflr.addr, ifa->ifa_addr,
			    MIN(ifa->ifa_addr->sa_len, sizeof(iflr.addr)));
			iflr.flags = IFLR_PREFIX;
			iflr.prefixlen = (unsigned int)sdl->sdl_alen * NBBY;
			if (ioctl(ctx->pf_link_fd, SIOCGLIFADDR, &iflr) == -1 ||
			    !(iflr.flags & IFLR_ACTIVE))
			{
				if_free(ifp);
				continue;
			}
#endif

			ifp->index = sdl->sdl_index;
			switch(sdl->sdl_type) {
#ifdef IFT_BRIDGE
			case IFT_BRIDGE: /* FALLTHROUGH */
#endif
#ifdef IFT_PROPVIRTUAL
			case IFT_PROPVIRTUAL: /* FALLTHROUGH */
#endif
#ifdef IFT_TUNNEL
			case IFT_TUNNEL: /* FALLTHROUGH */
#endif
			case IFT_LOOP: /* FALLTHROUGH */
			case IFT_PPP:
				/* Don't allow unless explicit */
				if (if_noconf && active) {
					logdebugx("%s: ignoring due to"
					    " interface type and"
					    " no config",
					    ifp->name);
					active = IF_INACTIVE;
				}
				__fallthrough; /* appease gcc */
				/* FALLTHROUGH */
#ifdef IFT_L2VLAN
			case IFT_L2VLAN: /* FALLTHROUGH */
#endif
#ifdef IFT_L3IPVLAN
			case IFT_L3IPVLAN: /* FALLTHROUGH */
#endif
			case IFT_ETHER:
				ifp->hwtype = ARPHRD_ETHER;
				break;
#ifdef IFT_IEEE1394
			case IFT_IEEE1394:
				ifp->hwtype = ARPHRD_IEEE1394;
				break;
#endif
#ifdef IFT_INFINIBAND
			case IFT_INFINIBAND:
				ifp->hwtype = ARPHRD_INFINIBAND;
				break;
#endif
			default:
				/* Don't allow unless explicit */
				if (active) {
					if (if_noconf)
						active = IF_INACTIVE;
					i = active ? LOG_WARNING : LOG_DEBUG;
					logmessage(i, "%s: unsupported"
					    " interface type 0x%.2x",
					    ifp->name, sdl->sdl_type);
				}
				/* Pretend it's ethernet */
				ifp->hwtype = ARPHRD_ETHER;
				break;
			}
			ifp->hwlen = sdl->sdl_alen;
			memcpy(ifp->hwaddr, CLLADDR(sdl), ifp->hwlen);
#elif defined(AF_PACKET)
			sll = (const void *)ifa->ifa_addr;
			ifp->index = (unsigned int)sll->sll_ifindex;
			ifp->hwtype = sll->sll_hatype;
			ifp->hwlen = sll->sll_halen;
			if (ifp->hwlen != 0)
				memcpy(ifp->hwaddr, sll->sll_addr, ifp->hwlen);
			active = if_check_arphrd(ifp, active, if_noconf);
#endif
		}
#ifdef __linux__
		else {
			struct ifreq ifr = { .ifr_flags = 0 };

			/* This is a huge bug in getifaddrs(3) as there
			 * is no reason why this can't be returned in
			 * ifa_addr. */
			strlcpy(ifr.ifr_name, ifa->ifa_name,
			    sizeof(ifr.ifr_name));
			if (ioctl(ctx->pf_inet_fd, SIOCGIFHWADDR, &ifr) == -1)
				logerr("%s: SIOCGIFHWADDR", ifa->ifa_name);
			ifp->hwtype = ifr.ifr_hwaddr.sa_family;
			if (ioctl(ctx->pf_inet_fd, SIOCGIFINDEX, &ifr) == -1)
				logerr("%s: SIOCGIFINDEX", ifa->ifa_name);
			ifp->index = (unsigned int)ifr.ifr_ifindex;
			if_check_arphrd(ifp, active, if_noconf);
		}
#endif

		if (!(ctx->options & (DHCPCD_DUMPLEASE | DHCPCD_TEST))) {
			/* Handle any platform init for the interface */
			if (active != IF_INACTIVE && if_init(ifp) == -1) {
				logerr("%s: if_init", ifp->name);
				if_free(ifp);
				continue;
			}
		}

		ifp->vlanid = if_vlanid(ifp);

#ifdef SIOCGIFPRIORITY
		/* Respect the interface priority */
		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
		if (pioctl(ctx, SIOCGIFPRIORITY, &ifr, sizeof(ifr)) == 0)
			ifp->metric = (unsigned int)ifr.ifr_metric;
		if_getssid(ifp);
#else
		/* Leave a low portion for user config */
		ifp->metric = RTMETRIC_BASE + ifp->index;
		if (if_getssid(ifp) != -1) {
			ifp->wireless = true;
			ifp->metric += RTMETRIC_WIRELESS;
		}
#endif

		ifp->active = active;
		ifp->carrier = if_carrier(ifp, ifa->ifa_data);
		TAILQ_INSERT_TAIL(ifs, ifp, next);
	}

	return ifs;
}

/*
 * eth0.100:2 OR eth0i100:2 (seems to be NetBSD xvif(4) only)
 *
 * drvname == eth
 * devname == eth0.100 OR eth0i100
 * ppa = 0
 * lun = 2
 */
int
if_nametospec(const char *ifname, struct if_spec *spec)
{
	char *ep, *pp;
	int e;

	if (ifname == NULL || *ifname == '\0' ||
	    strlcpy(spec->ifname, ifname, sizeof(spec->ifname)) >=
	    sizeof(spec->ifname) ||
	    strlcpy(spec->drvname, ifname, sizeof(spec->drvname)) >=
	    sizeof(spec->drvname))
	{
		errno = EINVAL;
		return -1;
	}

	/* :N is an alias */
	ep = strchr(spec->drvname, ':');
	if (ep) {
		spec->lun = (int)strtoi(ep + 1, NULL, 10, 0, INT_MAX, &e);
		if (e != 0) {
			errno = e;
			return -1;
		}
		*ep = '\0';
#ifdef __sun
		ep--;
#endif
	} else {
		spec->lun = -1;
#ifdef __sun
		ep = spec->drvname + strlen(spec->drvname) - 1;
#endif
	}

	strlcpy(spec->devname, spec->drvname, sizeof(spec->devname));
#ifdef __sun
	/* Solaris has numbers in the driver name, such as e1000g */
	while (ep > spec->drvname && isdigit((int)*ep))
		ep--;
	if (*ep++ == ':') {
		errno = EINVAL;
		return -1;
	}
#else
	/* BSD and Linux no not have numbers in the driver name */
	for (ep = spec->drvname; *ep != '\0' && !isdigit((int)*ep); ep++) {
		if (*ep == ':') {
			errno = EINVAL;
			return -1;
		}
	}
#endif
	spec->ppa = (int)strtoi(ep, &pp, 10, 0, INT_MAX, &e);
	*ep = '\0';

#ifndef __sun
	/*
	 * . is used for VLAN style names
	 * i is used on NetBSD for xvif interfaces
	 */
	if (pp != NULL && (*pp == '.' || *pp == 'i')) {
		spec->vlid = (int)strtoi(pp + 1, NULL, 10, 0, INT_MAX, &e);
		if (e)
			spec->vlid = -1;
	} else
#endif
		spec->vlid = -1;

	return 0;
}

static struct interface *
if_findindexname(struct if_head *ifaces, unsigned int idx, const char *name)
{

	if (ifaces != NULL) {
		struct if_spec spec;
		struct interface *ifp;

		if (name && if_nametospec(name, &spec) == -1)
			return NULL;

		TAILQ_FOREACH(ifp, ifaces, next) {
			if ((name && strcmp(ifp->name, spec.devname) == 0) ||
			    (!name && ifp->index == idx))
				return ifp;
		}
	}

	errno = ENXIO;
	return NULL;
}

struct interface *
if_find(struct if_head *ifaces, const char *name)
{

	return if_findindexname(ifaces, 0, name);
}

struct interface *
if_findindex(struct if_head *ifaces, unsigned int idx)
{

	return if_findindexname(ifaces, idx, NULL);
}

struct interface *
if_loopback(struct dhcpcd_ctx *ctx)
{
	struct interface *ifp;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (ifp->flags & IFF_LOOPBACK)
			return ifp;
	}
	return NULL;
}

int
if_domtu(const struct interface *ifp, short int mtu)
{
	int r;
	struct ifreq ifr;

#ifdef __sun
	if (mtu == 0)
		return if_mtu_os(ifp);
#endif

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = mtu;
	if (mtu != 0)
		r = if_ioctl(ifp->ctx, SIOCSIFMTU, &ifr, sizeof(ifr));
	else
		r = pioctl(ifp->ctx, SIOCGIFMTU, &ifr, sizeof(ifr));

	if (r == -1)
		return -1;
	return ifr.ifr_mtu;
}

#ifdef ALIAS_ADDR
int
if_makealias(char *alias, size_t alias_len, const char *ifname, int lun)
{

	if (lun == 0)
		return strlcpy(alias, ifname, alias_len);
	return snprintf(alias, alias_len, "%s:%u", ifname, lun);
}
#endif

struct interface *
if_findifpfromcmsg(struct dhcpcd_ctx *ctx, struct msghdr *msg, int *hoplimit)
{
	struct cmsghdr *cm;
	unsigned int ifindex = 0;
	struct interface *ifp;
#ifdef INET
#ifdef IP_RECVIF
	struct sockaddr_dl sdl;
#else
	struct in_pktinfo ipi;
#endif
#endif
#ifdef INET6
	struct in6_pktinfo ipi6;
#else
	UNUSED(hoplimit);
#endif

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(msg);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(msg, cm))
	{
#ifdef INET
		if (cm->cmsg_level == IPPROTO_IP) {
			switch(cm->cmsg_type) {
#ifdef IP_RECVIF
			case IP_RECVIF:
				if (cm->cmsg_len <
				    offsetof(struct sockaddr_dl, sdl_index) +
				    sizeof(sdl.sdl_index))
					continue;
				memcpy(&sdl, CMSG_DATA(cm),
				    MIN(sizeof(sdl), cm->cmsg_len));
				ifindex = sdl.sdl_index;
				break;
#else
			case IP_PKTINFO:
				if (cm->cmsg_len != CMSG_LEN(sizeof(ipi)))
					continue;
				memcpy(&ipi, CMSG_DATA(cm), sizeof(ipi));
				ifindex = (unsigned int)ipi.ipi_ifindex;
				break;
#endif
			}
		}
#endif
#ifdef INET6
		if (cm->cmsg_level == IPPROTO_IPV6) {
			switch(cm->cmsg_type) {
			case IPV6_PKTINFO:
				if (cm->cmsg_len != CMSG_LEN(sizeof(ipi6)))
					continue;
				memcpy(&ipi6, CMSG_DATA(cm), sizeof(ipi6));
				ifindex = (unsigned int)ipi6.ipi6_ifindex;
				break;
			case IPV6_HOPLIMIT:
				if (cm->cmsg_len != CMSG_LEN(sizeof(int)))
					continue;
				if (hoplimit == NULL)
					break;
				memcpy(hoplimit, CMSG_DATA(cm), sizeof(int));
				break;
			}
		}
#endif
	}

	/* Find the receiving interface */
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (ifp->index == ifindex)
			break;
	}
	if (ifp == NULL)
		errno = ESRCH;
	return ifp;
}

int
xsocket(int domain, int type, int protocol)
{
	int s;
#if !defined(HAVE_SOCK_CLOEXEC) || !defined(HAVE_SOCK_NONBLOCK)
	int xflags, xtype = type;
#endif

#ifndef HAVE_SOCK_CLOEXEC
	if (xtype & SOCK_CLOEXEC)
		type &= ~SOCK_CLOEXEC;
#endif
#ifndef HAVE_SOCK_NONBLOCK
	if (xtype & SOCK_NONBLOCK)
		type &= ~SOCK_NONBLOCK;
#endif

	if ((s = socket(domain, type, protocol)) == -1)
		return -1;
#ifdef DEBUG_FD
	logerrx("pid %d fd=%d domain=%d type=%d protocol=%d",
	    getpid(), s, domain, type, protocol);
#endif

#ifndef HAVE_SOCK_CLOEXEC
	if ((xtype & SOCK_CLOEXEC) && ((xflags = fcntl(s, F_GETFD)) == -1 ||
	    fcntl(s, F_SETFD, xflags | FD_CLOEXEC) == -1))
		goto out;
#endif
#ifndef HAVE_SOCK_NONBLOCK
	if ((xtype & SOCK_NONBLOCK) && ((xflags = fcntl(s, F_GETFL)) == -1 ||
	    fcntl(s, F_SETFL, xflags | O_NONBLOCK) == -1))
		goto out;
#endif

	return s;

#if !defined(HAVE_SOCK_CLOEXEC) || !defined(HAVE_SOCK_NONBLOCK)
out:
	close(s);
	return -1;
#endif
}

int
xsocketpair(int domain, int type, int protocol, int fd[2])
{
	int s;
#if !defined(HAVE_SOCK_CLOEXEC) || !defined(HAVE_SOCK_NONBLOCK)
	int xflags, xtype = type;
#endif

#ifndef HAVE_SOCK_CLOEXEC
	if (xtype & SOCK_CLOEXEC)
		type &= ~SOCK_CLOEXEC;
#endif
#ifndef HAVE_SOCK_NONBLOCK
	if (xtype & SOCK_NONBLOCK)
		type &= ~SOCK_NONBLOCK;
#endif

	if ((s = socketpair(domain, type, protocol, fd)) == -1)
		return -1;

#ifdef DEBUG_FD
	logerrx("pid %d fd[0]=%d fd[1]=%d", getpid(), fd[0], fd[1]);
#endif

#ifndef HAVE_SOCK_CLOEXEC
	if ((xtype & SOCK_CLOEXEC) && ((xflags = fcntl(fd[0], F_GETFD)) == -1 ||
	    fcntl(fd[0], F_SETFD, xflags | FD_CLOEXEC) == -1))
		goto out;
	if ((xtype & SOCK_CLOEXEC) && ((xflags = fcntl(fd[1], F_GETFD)) == -1 ||
	    fcntl(fd[1], F_SETFD, xflags | FD_CLOEXEC) == -1))
		goto out;
#endif
#ifndef HAVE_SOCK_NONBLOCK
	if ((xtype & SOCK_NONBLOCK) && ((xflags = fcntl(fd[0], F_GETFL)) == -1 ||
	    fcntl(fd[0], F_SETFL, xflags | O_NONBLOCK) == -1))
		goto out;
	if ((xtype & SOCK_NONBLOCK) && ((xflags = fcntl(fd[1], F_GETFL)) == -1 ||
	    fcntl(fd[1], F_SETFL, xflags | O_NONBLOCK) == -1))
		goto out;
#endif

	return s;

#if !defined(HAVE_SOCK_CLOEXEC) || !defined(HAVE_SOCK_NONBLOCK)
out:
	close(fd[0]);
	close(fd[1]);
	return -1;
#endif
}
