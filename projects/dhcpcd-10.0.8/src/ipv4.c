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
#include <sys/types.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "arp.h"
#include "common.h"
#include "dhcpcd.h"
#include "dhcp.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "logerr.h"
#include "route.h"
#include "script.h"
#include "sa.h"

#define IPV4_LOOPBACK_ROUTE
#if defined(__linux__) || defined(__sun) || (defined(BSD) && defined(RTF_LOCAL))
/* Linux has had loopback routes in the local table since 2.2
 * Solaris does not seem to support loopback routes. */
#undef IPV4_LOOPBACK_ROUTE
#endif

uint8_t
inet_ntocidr(struct in_addr address)
{
	uint8_t cidr = 0;
	uint32_t mask = htonl(address.s_addr);

	while (mask) {
		cidr++;
		mask <<= 1;
	}
	return cidr;
}

int
inet_cidrtoaddr(int cidr, struct in_addr *addr)
{
	int ocets;

	if (cidr < 1 || cidr > 32) {
		errno = EINVAL;
		return -1;
	}
	ocets = (cidr + 7) / NBBY;

	addr->s_addr = 0;
	if (ocets > 0) {
		memset(&addr->s_addr, 255, (size_t)ocets - 1);
		memset((unsigned char *)&addr->s_addr + (ocets - 1),
		    (256 - (1 << (32 - cidr) % NBBY)), 1);
	}

	return 0;
}

uint32_t
ipv4_getnetmask(uint32_t addr)
{
	uint32_t dst;

	if (addr == 0)
		return 0;

	dst = htonl(addr);
	if (IN_CLASSA(dst))
		return ntohl(IN_CLASSA_NET);
	if (IN_CLASSB(dst))
		return ntohl(IN_CLASSB_NET);
	if (IN_CLASSC(dst))
		return ntohl(IN_CLASSC_NET);

	return 0;
}

struct ipv4_addr *
ipv4_iffindaddr(struct interface *ifp,
    const struct in_addr *addr, const struct in_addr *mask)
{
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	state = IPV4_STATE(ifp);
	if (state) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if ((addr == NULL || ap->addr.s_addr == addr->s_addr) &&
			    (mask == NULL || ap->mask.s_addr == mask->s_addr))
				return ap;
		}
	}
	return NULL;
}

struct ipv4_addr *
ipv4_iffindlladdr(struct interface *ifp)
{
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	state = IPV4_STATE(ifp);
	if (state) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (IN_LINKLOCAL(ntohl(ap->addr.s_addr)))
				return ap;
		}
	}
	return NULL;
}

static struct ipv4_addr *
ipv4_iffindmaskaddr(struct interface *ifp, const struct in_addr *addr)
{
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	state = IPV4_STATE(ifp);
	if (state) {
		TAILQ_FOREACH (ap, &state->addrs, next) {
			if ((ap->addr.s_addr & ap->mask.s_addr) ==
			    (addr->s_addr & ap->mask.s_addr))
				return ap;
		}
	}
	return NULL;
}

static struct ipv4_addr *
ipv4_iffindmaskbrd(struct interface *ifp, const struct in_addr *addr)
{
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	state = IPV4_STATE(ifp);
	if (state) {
		TAILQ_FOREACH (ap, &state->addrs, next) {
			if ((ap->brd.s_addr & ap->mask.s_addr) ==
			    (addr->s_addr & ap->mask.s_addr))
				return ap;
		}
	}
	return NULL;
}

struct ipv4_addr *
ipv4_findaddr(struct dhcpcd_ctx *ctx, const struct in_addr *addr)
{
	struct interface *ifp;
	struct ipv4_addr *ap;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		ap = ipv4_iffindaddr(ifp, addr, NULL);
		if (ap)
			return ap;
	}
	return NULL;
}

struct ipv4_addr *
ipv4_findmaskaddr(struct dhcpcd_ctx *ctx, const struct in_addr *addr)
{
	struct interface *ifp;
	struct ipv4_addr *ap;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		ap = ipv4_iffindmaskaddr(ifp, addr);
		if (ap)
			return ap;
	}
	return NULL;
}

struct ipv4_addr *
ipv4_findmaskbrd(struct dhcpcd_ctx *ctx, const struct in_addr *addr)
{
	struct interface *ifp;
	struct ipv4_addr *ap;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		ap = ipv4_iffindmaskbrd(ifp, addr);
		if (ap)
			return ap;
	}
	return NULL;
}

int
ipv4_hasaddr(const struct interface *ifp)
{
	const struct dhcp_state *dstate;

#ifdef IPV4LL
	if (IPV4LL_STATE_RUNNING(ifp))
		return 1;
#endif

	dstate = D_CSTATE(ifp);
	return (dstate &&
	    dstate->added == STATE_ADDED &&
	    dstate->addr != NULL);
}

/* Interface comparer for working out ordering. */
int
ipv4_ifcmp(const struct interface *si, const struct interface *ti)
{
	const struct dhcp_state *sis, *tis;

	sis = D_CSTATE(si);
	tis = D_CSTATE(ti);
	if (sis && !tis)
		return -1;
	if (!sis && tis)
		return 1;
	if (!sis && !tis)
		return 0;
	/* If one has a lease and the other not, it takes precedence. */
	if (sis->new && !tis->new)
		return -1;
	if (!sis->new && tis->new)
		return 1;
	/* Always prefer proper leases */
	if (!(sis->added & STATE_FAKE) && (tis->added & STATE_FAKE))
		return -1;
	if ((sis->added & STATE_FAKE) && !(tis->added & STATE_FAKE))
		return 1;
	/* If we are either, they neither have a lease, or they both have.
	 * We need to check for IPv4LL and make it non-preferred. */
	if (sis->new && tis->new) {
		if (IS_DHCP(sis->new) && !IS_DHCP(tis->new))
			return -1;
		if (!IS_DHCP(sis->new) && IS_DHCP(tis->new))
			return 1;
	}
	return 0;
}

static int
inet_dhcproutes(rb_tree_t *routes, struct interface *ifp, bool *have_default)
{
	const struct dhcp_state *state;
	rb_tree_t nroutes;
	struct rt *rt, *r = NULL;
	struct in_addr in;
	uint16_t mtu;
	int n;

	state = D_CSTATE(ifp);
	if (state == NULL || state->state != DHS_BOUND || !state->added)
		return 0;

	/* An address does have to exist. */
	assert(state->addr);

	rb_tree_init(&nroutes, &rt_compare_proto_ops);

	/* First, add a subnet route. */
	if (state->addr->mask.s_addr != INADDR_ANY
#ifndef BSD
	    /* BSD adds a route in this instance */
	    && state->addr->mask.s_addr != INADDR_BROADCAST
#endif
	) {
		if ((rt = rt_new(ifp)) == NULL)
			return -1;
		rt->rt_dflags |= RTDF_IFA_ROUTE;
		in.s_addr = state->addr->addr.s_addr & state->addr->mask.s_addr;
		sa_in_init(&rt->rt_dest, &in);
		in.s_addr = state->addr->mask.s_addr;
		sa_in_init(&rt->rt_netmask, &in);
		//in.s_addr = INADDR_ANY;
		//sa_in_init(&rt->rt_gateway, &in);
		rt->rt_gateway.sa_family = AF_UNSPEC;
		rt_proto_add(&nroutes, rt);
	}

	/* If any set routes, grab them, otherwise DHCP routes. */
	if (RB_TREE_MIN(&ifp->options->routes)) {
		RB_TREE_FOREACH(r, &ifp->options->routes) {
			if (sa_is_unspecified(&r->rt_gateway))
				break;
			if ((rt = rt_new0(ifp->ctx)) == NULL)
				return -1;
			memcpy(rt, r, sizeof(*rt));
			rt_setif(rt, ifp);
			rt->rt_dflags = RTDF_STATIC;
			rt_proto_add(&nroutes, rt);
		}
	} else {
		if (dhcp_get_routes(&nroutes, ifp) == -1)
			return -1;
	}

	/* If configured, install a gateway to the desintion
	 * for P2P interfaces. */
	if (ifp->flags & IFF_POINTOPOINT &&
	    has_option_mask(ifp->options->dstmask, DHO_ROUTER))
	{
		if ((rt = rt_new(ifp)) == NULL)
			return -1;
		in.s_addr = INADDR_ANY;
		sa_in_init(&rt->rt_dest, &in);
		sa_in_init(&rt->rt_netmask, &in);
		sa_in_init(&rt->rt_gateway, &state->addr->brd);
		sa_in_init(&rt->rt_ifa, &state->addr->addr);
		rt_proto_add(&nroutes, rt);
	}

	/* Copy our address as the source address and set mtu */
	mtu = dhcp_get_mtu(ifp);
	n = 0;
	while ((rt = RB_TREE_MIN(&nroutes)) != NULL) {
		rb_tree_remove_node(&nroutes, rt);
		rt->rt_mtu = mtu;
		if (!(rt->rt_dflags & RTDF_STATIC))
			rt->rt_dflags |= RTDF_DHCP;
		sa_in_init(&rt->rt_ifa, &state->addr->addr);
		if (rb_tree_insert_node(routes, rt) != rt) {
			rt_free(rt);
			continue;
		}
		if (rt_is_default(rt))
			*have_default = true;
		n = 1;
	}

	return n;
}

/* We should check to ensure the routers are on the same subnet
 * OR supply a host route. If not, warn and add a host route. */
static int
inet_routerhostroute(rb_tree_t *routes, struct interface *ifp)
{
	struct rt *rt, *rth, *rtp;
	struct sockaddr_in *dest, *netmask, *gateway;
	const char *cp, *cp2, *cp3, *cplim;
	struct if_options *ifo;
	const struct dhcp_state *state;
	struct in_addr in;
	rb_tree_t troutes;

	/* Don't add a host route for these interfaces. */
	if (ifp->flags & (IFF_LOOPBACK | IFF_POINTOPOINT))
		return 0;

	rb_tree_init(&troutes, &rt_compare_proto_ops);

	RB_TREE_FOREACH(rt, routes) {
		if (rt->rt_dest.sa_family != AF_INET)
			continue;
		if (!sa_is_unspecified(&rt->rt_dest) ||
		    sa_is_unspecified(&rt->rt_gateway))
			continue;
		gateway = satosin(&rt->rt_gateway);
		/* Scan for a route to match */
		RB_TREE_FOREACH(rth, routes) {
			if (rth == rt)
				break;
			/* match host */
			if (sa_cmp(&rth->rt_dest, &rt->rt_gateway) == 0)
				break;
			/* match subnet */
			/* XXX ADD TO RT_COMARE? XXX */
			cp = (const char *)&gateway->sin_addr.s_addr;
			dest = satosin(&rth->rt_dest);
			cp2 = (const char *)&dest->sin_addr.s_addr;
			netmask = satosin(&rth->rt_netmask);
			cp3 = (const char *)&netmask->sin_addr.s_addr;
			cplim = cp3 + sizeof(netmask->sin_addr.s_addr);
			while (cp3 < cplim) {
				if ((*cp++ ^ *cp2++) & *cp3++)
					break;
			}
			if (cp3 == cplim)
				break;
		}
		if (rth != rt)
			continue;
		if ((state = D_CSTATE(ifp)) == NULL)
			continue;
		ifo = ifp->options;
		if (ifp->flags & IFF_NOARP) {
			if (!(ifo->options & DHCPCD_ROUTER_HOST_ROUTE_WARNED) &&
			    !(state->added & STATE_FAKE))
			{
				char buf[INET_MAX_ADDRSTRLEN];

				ifo->options |= DHCPCD_ROUTER_HOST_ROUTE_WARNED;
				logwarnx("%s: forcing router %s through "
				    "interface",
				    ifp->name,
				    sa_addrtop(&rt->rt_gateway,
				    buf, sizeof(buf)));
			}
			gateway->sin_addr.s_addr = INADDR_ANY;
			continue;
		}
		if (!(ifo->options & DHCPCD_ROUTER_HOST_ROUTE_WARNED) &&
		    !(state->added & STATE_FAKE))
		{
			char buf[INET_MAX_ADDRSTRLEN];

			ifo->options |= DHCPCD_ROUTER_HOST_ROUTE_WARNED;
			logwarnx("%s: router %s requires a host route",
			    ifp->name,
			    sa_addrtop(&rt->rt_gateway, buf, sizeof(buf)));
		}

		if ((rth = rt_new(ifp)) == NULL)
			return -1;
		rth->rt_flags |= RTF_HOST;
		sa_in_init(&rth->rt_dest, &gateway->sin_addr);
		in.s_addr = INADDR_BROADCAST;
		sa_in_init(&rth->rt_netmask, &in);
		in.s_addr = INADDR_ANY;
		sa_in_init(&rth->rt_gateway, &in);
		rth->rt_mtu = dhcp_get_mtu(ifp);
		if (state->addr != NULL)
			sa_in_init(&rth->rt_ifa, &state->addr->addr);
		else
			rth->rt_ifa.sa_family = AF_UNSPEC;

		/* We need to insert the host route just before the router. */
		while ((rtp = RB_TREE_MAX(routes)) != NULL) {
			rb_tree_remove_node(routes, rtp);
			rt_proto_add(&troutes, rtp);
			if (rtp == rt)
				break;
		}
		rt_proto_add(routes, rth);
		/* troutes is now reversed, so add backwards again. */
		while ((rtp = RB_TREE_MAX(&troutes)) != NULL) {
			rb_tree_remove_node(&troutes, rtp);
			rt_proto_add(routes, rtp);
		}
	}
	return 0;
}

bool
inet_getroutes(struct dhcpcd_ctx *ctx, rb_tree_t *routes)
{
	struct interface *ifp;
	bool have_default = false;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (!ifp->active)
			continue;
		if (inet_dhcproutes(routes, ifp, &have_default) == -1)
			return false;
#ifdef IPV4LL
		if (ipv4ll_subnetroute(routes, ifp) == -1)
			return false;
#endif
		if (inet_routerhostroute(routes, ifp) == -1)
			return false;
	}

#ifdef IPV4LL
	/* If there is no default route, see if we can use an IPv4LL one. */
	if (have_default)
		return true;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (ifp->active &&
		    ipv4ll_defaultroute(routes, ifp) == 1)
			break;
	}
#endif

	return true;
}

int
ipv4_deladdr(struct ipv4_addr *addr, int keeparp)
{
	int r;
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	logdebugx("%s: deleting IP address %s",
	    addr->iface->name, addr->saddr);

	r = if_address(RTM_DELADDR, addr);
	if (r == -1 &&
	    errno != EADDRNOTAVAIL && errno != ESRCH &&
	    errno != ENXIO && errno != ENODEV)
		logerr("%s: %s", addr->iface->name, __func__);

#ifdef ARP
	if (!keeparp)
		arp_freeaddr(addr->iface, &addr->addr);
#else
	UNUSED(keeparp);
#endif

	state = IPV4_STATE(addr->iface);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (IPV4_MASK_EQ(ap, addr)) {
			struct dhcp_state *dstate;

			dstate = D_STATE(ap->iface);
			TAILQ_REMOVE(&state->addrs, ap, next);
			free(ap);

			if (dstate && dstate->addr == ap) {
				dstate->added = 0;
				dstate->addr = NULL;
			}
			break;
		}
	}

	return r;
}

static int
delete_address(struct interface *ifp)
{
	int r;
	struct if_options *ifo;
	struct dhcp_state *state;

	state = D_STATE(ifp);
	ifo = ifp->options;
	/* The lease could have been added, but the address deleted
	 * by a 3rd party. */
	if (state->addr == NULL ||
	    ifo->options & DHCPCD_INFORM ||
	    (ifo->options & DHCPCD_STATIC && ifo->req_addr.s_addr == 0))
		return 0;
#ifdef ARP
	arp_freeaddr(ifp, &state->addr->addr);
#endif
	r = ipv4_deladdr(state->addr, 0);
	return r;
}

struct ipv4_state *
ipv4_getstate(struct interface *ifp)
{
	struct ipv4_state *state;

	state = IPV4_STATE(ifp);
	if (state == NULL) {
	        ifp->if_data[IF_DATA_IPV4] = malloc(sizeof(*state));
		state = IPV4_STATE(ifp);
		if (state == NULL) {
			logerr(__func__);
			return NULL;
		}
		TAILQ_INIT(&state->addrs);
	}
	return state;
}

#ifdef ALIAS_ADDR
/* Find the next logical aliase address we can use. */
static int
ipv4_aliasaddr(struct ipv4_addr *ia, struct ipv4_addr **repl)
{
	struct ipv4_state *state;
	struct ipv4_addr *iap;
	unsigned int lun;
	char alias[IF_NAMESIZE];

	if (ia->alias[0] != '\0')
		return 0;

	lun = 0;
	state = IPV4_STATE(ia->iface);
find_lun:
	if (if_makealias(alias, IF_NAMESIZE, ia->iface->name, lun) >=
	    IF_NAMESIZE)
	{
		errno = ENOMEM;
		return -1;
	}
	TAILQ_FOREACH(iap, &state->addrs, next) {
		if (iap->alias[0] != '\0' && iap->addr.s_addr == INADDR_ANY) {
			/* No address assigned? Lets use it. */
			strlcpy(ia->alias, iap->alias, sizeof(ia->alias));
			if (repl)
				*repl = iap;
			return 1;
		}
		if (strcmp(iap->alias, alias) == 0)
			break;
	}

	if (iap != NULL) {
		if (lun == UINT_MAX) {
			errno = ERANGE;
			return -1;
		}
		lun++;
		goto find_lun;
	}

	strlcpy(ia->alias, alias, sizeof(ia->alias));
	return 0;
}
#endif

struct ipv4_addr *
ipv4_addaddr(struct interface *ifp, const struct in_addr *addr,
    const struct in_addr *mask, const struct in_addr *bcast,
    uint32_t vltime, uint32_t pltime)
{
	struct ipv4_state *state;
	struct ipv4_addr *ia;
#ifdef ALIAS_ADDR
	int replaced, blank;
	struct ipv4_addr *replaced_ia;
#endif

	if ((state = ipv4_getstate(ifp)) == NULL) {
		logerr(__func__);
		return NULL;
	}
	if (ifp->options->options & DHCPCD_NOALIAS) {
		struct ipv4_addr *ian;

		TAILQ_FOREACH_SAFE(ia, &state->addrs, next, ian) {
			if (ia->addr.s_addr != addr->s_addr)
				ipv4_deladdr(ia, 0);
		}
	}

	ia = ipv4_iffindaddr(ifp, addr, NULL);
	if (ia == NULL) {
		ia = malloc(sizeof(*ia));
		if (ia == NULL) {
			logerr(__func__);
			return NULL;
		}
		ia->iface = ifp;
		ia->addr = *addr;
#ifdef IN_IFF_TENTATIVE
		ia->addr_flags = IN_IFF_TENTATIVE;
#endif
		ia->flags = IPV4_AF_NEW;
	} else
		ia->flags &= ~IPV4_AF_NEW;

	ia->mask = *mask;
	ia->brd = *bcast;
#ifdef IP_LIFETIME
	if (ifp->options->options & DHCPCD_LASTLEASE_EXTEND) {
		/* We don't want the kernel to expire the address. */
		ia->vltime = ia->pltime = DHCP_INFINITE_LIFETIME;
	} else {
		ia->vltime = vltime;
		ia->pltime = pltime;
	}
#else
	UNUSED(vltime);
	UNUSED(pltime);
#endif
	snprintf(ia->saddr, sizeof(ia->saddr), "%s/%d",
	    inet_ntoa(*addr), inet_ntocidr(*mask));

#ifdef ALIAS_ADDR
	blank = (ia->alias[0] == '\0');
	if ((replaced = ipv4_aliasaddr(ia, &replaced_ia)) == -1) {
		logerr("%s: ipv4_aliasaddr", ifp->name);
		free(ia);
		return NULL;
	}
	if (blank)
		logdebugx("%s: aliased %s", ia->alias, ia->saddr);
#endif

	logdebugx("%s: adding IP address %s %s %s",
	    ifp->name, ia->saddr,
	    ifp->flags & IFF_POINTOPOINT ? "destination" : "broadcast",
	    inet_ntoa(*bcast));
	if (if_address(RTM_NEWADDR, ia) == -1) {
		if (errno != EEXIST)
			logerr("%s: if_addaddress",
			    __func__);
		if (ia->flags & IPV4_AF_NEW)
			free(ia);
		return NULL;
	}

#ifdef ALIAS_ADDR
	if (replaced) {
		TAILQ_REMOVE(&state->addrs, replaced_ia, next);
		free(replaced_ia);
	}
#endif

	if (ia->flags & IPV4_AF_NEW)
		TAILQ_INSERT_TAIL(&state->addrs, ia, next);
	return ia;
}

static int
ipv4_daddaddr(struct interface *ifp, const struct dhcp_lease *lease)
{
	struct dhcp_state *state;
	struct ipv4_addr *ia;

	ia = ipv4_addaddr(ifp, &lease->addr, &lease->mask, &lease->brd,
	    lease->leasetime, lease->rebindtime);
	if (ia == NULL)
		return -1;

	state = D_STATE(ifp);
	state->added = STATE_ADDED;
	state->addr = ia;
	return 0;
}

struct ipv4_addr *
ipv4_applyaddr(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);
	struct dhcp_lease *lease;
	struct if_options *ifo = ifp->options;
	struct ipv4_addr *ia;

	if (state == NULL)
		return NULL;

	lease = &state->lease;
	if (state->new == NULL) {
		if ((ifo->options & (DHCPCD_EXITING | DHCPCD_PERSISTENT)) !=
		    (DHCPCD_EXITING | DHCPCD_PERSISTENT))
		{
			if (state->added) {
				delete_address(ifp);
				rt_build(ifp->ctx, AF_INET);
#ifdef ARP
				/* Announce the preferred address to
				 * kick ARP caches. */
				arp_announceaddr(ifp->ctx,&lease->addr);
#endif
			}
			script_runreason(ifp, state->reason);
		} else
			rt_build(ifp->ctx, AF_INET);
		return NULL;
	}

	ia = ipv4_iffindaddr(ifp, &lease->addr, NULL);
	/* If the netmask or broadcast is different, re-add the addresss.
	 * If IP addresses do not have lifetimes, there is a very real chance
	 * that re-adding them will scrub the subnet route temporarily
	 * which is a bad thing, so avoid it. */
	if (ia != NULL &&
	    ia->mask.s_addr == lease->mask.s_addr &&
	    ia->brd.s_addr == lease->brd.s_addr)
	{
#ifndef IP_LIFETIME
		logdebugx("%s: IP address %s already exists",
		    ifp->name, ia->saddr);
#endif
	} else {
#ifdef __linux__
		/* Linux does not change netmask/broadcast address
		 * for re-added addresses, so we need to delete the old one
		 * first. */
		if (ia != NULL)
			ipv4_deladdr(ia, 0);
#endif
#ifndef IP_LIFETIME
		if (ipv4_daddaddr(ifp, lease) == -1 && errno != EEXIST)
			return NULL;
#endif
	}
#ifdef IP_LIFETIME
	if (ipv4_daddaddr(ifp, lease) == -1 && errno != EEXIST)
		return NULL;
#endif

	ia = ipv4_iffindaddr(ifp, &lease->addr, NULL);
	if (ia == NULL) {
		logerrx("%s: added address vanished", ifp->name);
		return NULL;
	}
#if defined(ARP) && defined(IN_IFF_NOTUSEABLE)
	if (ia->addr_flags & IN_IFF_NOTUSEABLE)
		return NULL;
#endif

	/* Delete the old address if different */
	if (state->addr &&
	    state->addr->addr.s_addr != lease->addr.s_addr &&
	    ipv4_iffindaddr(ifp, &lease->addr, NULL))
		delete_address(ifp);

	state->addr = ia;
	state->added = STATE_ADDED;

	rt_build(ifp->ctx, AF_INET);

#ifdef ARP
	arp_announceaddr(ifp->ctx, &state->addr->addr);
#endif

	if (state->state == DHS_BOUND) {
		script_runreason(ifp, state->reason);
		dhcpcd_daemonise(ifp->ctx);
	}
	return ia;
}

void
ipv4_markaddrsstale(struct interface *ifp)
{
	struct ipv4_state *state;
	struct ipv4_addr *ia;

	state = IPV4_STATE(ifp);
	if (state == NULL)
		return;

	TAILQ_FOREACH(ia, &state->addrs, next) {
		ia->flags |= IPV4_AF_STALE;
	}
}

void
ipv4_deletestaleaddrs(struct interface *ifp)
{
	struct ipv4_state *state;
	struct ipv4_addr *ia, *ia1;

	state = IPV4_STATE(ifp);
	if (state == NULL)
		return;

	TAILQ_FOREACH_SAFE(ia, &state->addrs, next, ia1) {
		if (!(ia->flags & IPV4_AF_STALE))
			continue;
		ipv4_handleifa(ifp->ctx, RTM_DELADDR,
		    ifp->ctx->ifaces, ifp->name,
		    &ia->addr, &ia->mask, &ia->brd, 0, getpid());
	}
}

void
ipv4_handleifa(struct dhcpcd_ctx *ctx,
    int cmd, struct if_head *ifs, const char *ifname,
    const struct in_addr *addr, const struct in_addr *mask,
    const struct in_addr *brd, int addrflags, pid_t pid)
{
	struct interface *ifp;
	struct ipv4_state *state;
	struct ipv4_addr *ia;
	bool ia_is_new;

#if 0
	char sbrdbuf[INET_ADDRSTRLEN];
	const char *sbrd;

	if (brd)
		sbrd = inet_ntop(AF_INET, brd, sbrdbuf, sizeof(sbrdbuf));
	else
		sbrd = NULL;
	logdebugx("%s: %s %s/%d %s %d", ifname,
	    cmd == RTM_NEWADDR ? "RTM_NEWADDR" :
	    cmd == RTM_DELADDR ? "RTM_DELADDR" : "???",
	    inet_ntoa(*addr), inet_ntocidr(*mask), sbrd, addrflags);
#endif

	if (ifs == NULL)
		ifs = ctx->ifaces;
	if (ifs == NULL) {
		errno = ESRCH;
		return;
	}
	if ((ifp = if_find(ifs, ifname)) == NULL)
		return;
	if ((state = ipv4_getstate(ifp)) == NULL) {
		errno = ENOENT;
		return;
	}

	ia = ipv4_iffindaddr(ifp, addr, NULL);
	switch (cmd) {
	case RTM_NEWADDR:
		if (ia == NULL) {
			if ((ia = malloc(sizeof(*ia))) == NULL) {
				logerr(__func__);
				return;
			}
			ia->iface = ifp;
			ia->addr = *addr;
			ia->mask = *mask;
			ia->flags = 0;
			ia_is_new = true;
#ifdef ALIAS_ADDR
			strlcpy(ia->alias, ifname, sizeof(ia->alias));
#endif
			TAILQ_INSERT_TAIL(&state->addrs, ia, next);
		} else
			ia_is_new = false;
		/* Mask could have changed */
		if (ia_is_new ||
		    (mask->s_addr != INADDR_ANY &&
		    mask->s_addr != ia->mask.s_addr))
		{
			ia->mask = *mask;
			snprintf(ia->saddr, sizeof(ia->saddr), "%s/%d",
			    inet_ntoa(*addr), inet_ntocidr(*mask));
		}
		if (brd != NULL)
			ia->brd = *brd;
		else
			ia->brd.s_addr = INADDR_ANY;
		ia->addr_flags = addrflags;
		ia->flags &= ~IPV4_AF_STALE;
		break;
	case RTM_DELADDR:
		if (ia == NULL)
			return;
		if (mask->s_addr != INADDR_ANY &&
		    mask->s_addr != ia->mask.s_addr)
			return;
		TAILQ_REMOVE(&state->addrs, ia, next);
		break;
	default:
		return;
	}

	if (addr->s_addr != INADDR_ANY && addr->s_addr != INADDR_BROADCAST) {
		ia = dhcp_handleifa(cmd, ia, pid);
#ifdef IPV4LL
		if (ia != NULL)
			ipv4ll_handleifa(cmd, ia, pid);
#endif
	}

	if (cmd == RTM_DELADDR)
		free(ia);
}

void
ipv4_free(struct interface *ifp)
{
	struct ipv4_state *state;
	struct ipv4_addr *ia;

	if (ifp == NULL || (state = IPV4_STATE(ifp)) == NULL)
		return;

	while ((ia = TAILQ_FIRST(&state->addrs))) {
		TAILQ_REMOVE(&state->addrs, ia, next);
		free(ia);
	}
	free(state);
}
