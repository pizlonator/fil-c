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

#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ELOOP_QUEUE	IPV4LL
#include "config.h"
#include "arp.h"
#include "common.h"
#include "dhcp.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "logerr.h"
#include "sa.h"
#include "script.h"

static const struct in_addr inaddr_llmask = {
	.s_addr = HTONL(LINKLOCAL_MASK)
};
static const struct in_addr inaddr_llbcast = {
	.s_addr = HTONL(LINKLOCAL_BCAST)
};

static void
ipv4ll_pickaddr(struct interface *ifp)
{
	struct in_addr addr = { .s_addr = 0 };
	struct ipv4ll_state *state;

	state = IPV4LL_STATE(ifp);
	setstate(state->randomstate);

	do {
		long r;

again:
		/* RFC 3927 Section 2.1 states that the first 256 and
		 * last 256 addresses are reserved for future use.
		 * See ipv4ll_start for why we don't use arc4random. */
		/* coverity[dont_call] */
		r = random();
		addr.s_addr = ntohl(LINKLOCAL_ADDR |
		    ((uint32_t)(r % 0xFD00) + 0x0100));

		/* No point using a failed address */
		if (IN_ARE_ADDR_EQUAL(&addr, &state->pickedaddr))
			goto again;
		/* Ensure we don't have the address on another interface */
	} while (ipv4_findaddr(ifp->ctx, &addr) != NULL);

	/* Restore the original random state */
	setstate(ifp->ctx->randomstate);
	state->pickedaddr = addr;
}

int
ipv4ll_subnetroute(rb_tree_t *routes, struct interface *ifp)
{
	struct ipv4ll_state *state;
	struct rt *rt;
	struct in_addr in;

	assert(ifp != NULL);
	if ((state = IPV4LL_STATE(ifp)) == NULL ||
	    state->addr == NULL)
		return 0;

	if ((rt = rt_new(ifp)) == NULL)
		return -1;

	in.s_addr = state->addr->addr.s_addr & state->addr->mask.s_addr;
	sa_in_init(&rt->rt_dest, &in);
	in.s_addr = state->addr->mask.s_addr;
	sa_in_init(&rt->rt_netmask, &in);
	in.s_addr = INADDR_ANY;
	sa_in_init(&rt->rt_gateway, &in);
	sa_in_init(&rt->rt_ifa, &state->addr->addr);
	rt->rt_dflags |= RTDF_IPV4LL;
	return rt_proto_add(routes, rt) ? 1 : 0;
}

int
ipv4ll_defaultroute(rb_tree_t *routes, struct interface *ifp)
{
	struct ipv4ll_state *state;
	struct rt *rt;
	struct in_addr in;

	assert(ifp != NULL);
	if ((state = IPV4LL_STATE(ifp)) == NULL ||
	    state->addr == NULL)
		return 0;

	if ((rt = rt_new(ifp)) == NULL)
		return -1;

	in.s_addr = INADDR_ANY;
	sa_in_init(&rt->rt_dest, &in);
	sa_in_init(&rt->rt_netmask, &in);
	sa_in_init(&rt->rt_gateway, &in);
	sa_in_init(&rt->rt_ifa, &state->addr->addr);
	rt->rt_dflags |= RTDF_IPV4LL;
#ifdef HAVE_ROUTE_METRIC
	rt->rt_metric += RTMETRIC_IPV4LL;
#endif
	return rt_proto_add(routes, rt) ? 1 : 0;
}

ssize_t
ipv4ll_env(FILE *fp, const char *prefix, const struct interface *ifp)
{
	const struct ipv4ll_state *state;
	const char *pf = prefix == NULL ? "" : "_";
	struct in_addr netnum;

	assert(ifp != NULL);
	if ((state = IPV4LL_CSTATE(ifp)) == NULL || state->addr == NULL)
		return 0;

	/* Emulate a DHCP environment */
	if (efprintf(fp, "%s%sip_address=%s",
	    prefix, pf, inet_ntoa(state->addr->addr)) == -1)
		return -1;
	if (efprintf(fp, "%s%ssubnet_mask=%s",
	    prefix, pf, inet_ntoa(state->addr->mask)) == -1)
		return -1;
	if (efprintf(fp, "%s%ssubnet_cidr=%d",
	    prefix, pf, inet_ntocidr(state->addr->mask)) == -1)
		return -1;
	if (efprintf(fp, "%s%sbroadcast_address=%s",
	    prefix, pf, inet_ntoa(state->addr->brd)) == -1)
		return -1;
	netnum.s_addr = state->addr->addr.s_addr & state->addr->mask.s_addr;
	if (efprintf(fp, "%s%snetwork_number=%s",
	    prefix, pf, inet_ntoa(netnum)) == -1)
		return -1;
	return 5;
}

static void
ipv4ll_announced_arp(struct arp_state *astate)
{
	struct ipv4ll_state *state = IPV4LL_STATE(astate->iface);

	state->conflicts = 0;
#ifdef KERNEL_RFC5227
	arp_free(astate);
#endif
}

#ifndef KERNEL_RFC5227
/* This is the callback by ARP freeing */
static void
ipv4ll_free_arp(struct arp_state *astate)
{
	struct ipv4ll_state *state;

	state = IPV4LL_STATE(astate->iface);
	if (state != NULL && state->arp == astate)
		state->arp = NULL;
}

/* This is us freeing any ARP state */
static void
ipv4ll_freearp(struct interface *ifp)
{
	struct ipv4ll_state *state;

	state = IPV4LL_STATE(ifp);
	if (state == NULL || state->arp == NULL)
		return;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, state->arp);
	arp_free(state->arp);
	state->arp = NULL;
}
#else
#define	ipv4ll_freearp(ifp)
#endif

static void
ipv4ll_not_found(struct interface *ifp)
{
	struct ipv4ll_state *state;
	struct ipv4_addr *ia;
	struct arp_state *astate;

	state = IPV4LL_STATE(ifp);
	ia = ipv4_iffindaddr(ifp, &state->pickedaddr, &inaddr_llmask);
#ifdef IN_IFF_NOTREADY
	if (ia == NULL || ia->addr_flags & IN_IFF_NOTREADY)
#endif
		loginfox("%s: using IPv4LL address %s",
		  ifp->name, inet_ntoa(state->pickedaddr));
	if (!(ifp->options->options & DHCPCD_CONFIGURE))
		goto run;
	if (ia == NULL) {
		if (ifp->ctx->options & DHCPCD_TEST)
			goto test;
		ia = ipv4_addaddr(ifp, &state->pickedaddr,
		    &inaddr_llmask, &inaddr_llbcast,
		    DHCP_INFINITE_LIFETIME, DHCP_INFINITE_LIFETIME);
	}
	if (ia == NULL)
		return;
#ifdef IN_IFF_NOTREADY
	if (ia->addr_flags & IN_IFF_NOTREADY)
		return;
	logdebugx("%s: DAD completed for %s", ifp->name, ia->saddr);
#endif

test:
	state->addr = ia;
	state->down = false;
	if (ifp->ctx->options & DHCPCD_TEST) {
		script_runreason(ifp, "TEST");
		eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
		return;
	}
	rt_build(ifp->ctx, AF_INET);
run:
	astate = arp_announceaddr(ifp->ctx, &ia->addr);
	if (astate != NULL)
		astate->announced_cb = ipv4ll_announced_arp;
	script_runreason(ifp, "IPV4LL");
	dhcpcd_daemonise(ifp->ctx);
}

static void
ipv4ll_found(struct interface *ifp)
{
	struct ipv4ll_state *state = IPV4LL_STATE(ifp);

	ipv4ll_freearp(ifp);
	if (++state->conflicts == MAX_CONFLICTS)
		logerrx("%s: failed to acquire an IPv4LL address",
		    ifp->name);
	ipv4ll_pickaddr(ifp);
	eloop_timeout_add_sec(ifp->ctx->eloop,
	    state->conflicts >= MAX_CONFLICTS ?
	    RATE_LIMIT_INTERVAL : PROBE_WAIT,
	    ipv4ll_start, ifp);
}

static void
ipv4ll_defend_failed(struct interface *ifp)
{
	struct ipv4ll_state *state = IPV4LL_STATE(ifp);

	ipv4ll_freearp(ifp);
	if (ifp->options->options & DHCPCD_CONFIGURE)
		ipv4_deladdr(state->addr, 1);
	state->addr = NULL;
	rt_build(ifp->ctx, AF_INET);
	script_runreason(ifp, "IPV4LL");
	ipv4ll_pickaddr(ifp);
	ipv4ll_start(ifp);
}

#ifndef KERNEL_RFC5227
static void
ipv4ll_not_found_arp(struct arp_state *astate)
{

	ipv4ll_not_found(astate->iface);
}

static void
ipv4ll_found_arp(struct arp_state *astate, __unused const struct arp_msg *amsg)
{

	ipv4ll_found(astate->iface);
}

static void
ipv4ll_defend_failed_arp(struct arp_state *astate)
{

	ipv4ll_defend_failed(astate->iface);
}
#endif

void
ipv4ll_start(void *arg)
{
	struct interface *ifp = arg;
	struct ipv4ll_state *state;
	struct ipv4_addr *ia;
	bool repick;
#ifndef KERNEL_RFC5227
	struct arp_state *astate;
#endif

	if ((state = IPV4LL_STATE(ifp)) == NULL) {
		ifp->if_data[IF_DATA_IPV4LL] = calloc(1, sizeof(*state));
		if ((state = IPV4LL_STATE(ifp)) == NULL) {
			logerr(__func__);
			return;
		}
	}

	if (state->running)
		return;
	state->running = true;

	/* RFC 3927 Section 2.1 states that the random number generator
	 * SHOULD be seeded with a value derived from persistent information
	 * such as the IEEE 802 MAC address so that it usually picks
	 * the same address without persistent storage. */
	if (!state->seeded) {
		unsigned int seed;
		char *orig;

		if (sizeof(seed) > ifp->hwlen) {
			seed = 0;
			memcpy(&seed, ifp->hwaddr, ifp->hwlen);
		} else
			memcpy(&seed, ifp->hwaddr + ifp->hwlen - sizeof(seed),
			    sizeof(seed));
		/* coverity[dont_call] */
		orig = initstate(seed,
		    state->randomstate, sizeof(state->randomstate));

		/* Save the original state. */
		if (ifp->ctx->randomstate == NULL)
			ifp->ctx->randomstate = orig;

		/* Set back the original state until we need the seeded one. */
		setstate(ifp->ctx->randomstate);
		state->seeded = true;
	}

	/* Find the previosuly used address. */
	if (state->pickedaddr.s_addr != INADDR_ANY)
		ia = ipv4_iffindaddr(ifp, &state->pickedaddr, NULL);
	else
		ia = NULL;

	/* Find an existing IPv4LL address and ensure we can work with it. */
	if (ia == NULL)
		ia = ipv4_iffindlladdr(ifp);

	repick = false;
#ifdef IN_IFF_DUPLICATED
	if (ia != NULL && ia->addr_flags & IN_IFF_DUPLICATED) {
		state->pickedaddr = ia->addr; /* So it's not picked again. */
		repick = true;
		if (ifp->options->options & DHCPCD_CONFIGURE)
			ipv4_deladdr(ia, 0);
		ia = NULL;
	}
#endif

	state->addr = ia;
	state->down = true;
	if (ia != NULL) {
		state->pickedaddr = ia->addr;
#ifdef IN_IFF_TENTATIVE
		if (ia->addr_flags & (IN_IFF_TENTATIVE | IN_IFF_DETACHED)) {
			loginfox("%s: waiting for DAD to complete on %s",
			    ifp->name, inet_ntoa(ia->addr));
			return;
		}
#endif
#ifdef IN_IFF_DUPLICATED
		loginfox("%s: using IPv4LL address %s", ifp->name, ia->saddr);
#endif
	} else {
		loginfox("%s: probing for an IPv4LL address", ifp->name);
		if (repick || state->pickedaddr.s_addr == INADDR_ANY)
			ipv4ll_pickaddr(ifp);
	}

#ifdef KERNEL_RFC5227
	ipv4ll_not_found(ifp);
#else
	ipv4ll_freearp(ifp);
	state->arp = astate = arp_new(ifp, &state->pickedaddr);
	if (state->arp == NULL)
		return;

	astate->found_cb = ipv4ll_found_arp;
	astate->not_found_cb = ipv4ll_not_found_arp;
	astate->announced_cb = ipv4ll_announced_arp;
	astate->defend_failed_cb = ipv4ll_defend_failed_arp;
	astate->free_cb = ipv4ll_free_arp;
	arp_probe(astate);
#endif
}

void
ipv4ll_drop(struct interface *ifp)
{
	struct ipv4ll_state *state;
	bool dropped = false;
	struct ipv4_state *istate;

	assert(ifp != NULL);

	ipv4ll_freearp(ifp);

	if ((ifp->options->options & DHCPCD_NODROP) == DHCPCD_NODROP)
		return;

	state = IPV4LL_STATE(ifp);
	if (state) {
		state->running = false;
		if (state->addr != NULL) {
			if (ifp->options->options & DHCPCD_CONFIGURE)
				ipv4_deladdr(state->addr, 1);
			state->addr = NULL;
			dropped = true;
		}
	}

	/* Free any other link local addresses that might exist. */
	if ((istate = IPV4_STATE(ifp)) != NULL) {
		struct ipv4_addr *ia, *ian;

		TAILQ_FOREACH_SAFE(ia, &istate->addrs, next, ian) {
			if (IN_LINKLOCAL(ntohl(ia->addr.s_addr))) {
				if (ifp->options->options & DHCPCD_CONFIGURE)
					ipv4_deladdr(ia, 0);
				dropped = true;
			}
		}
	}

	if (dropped) {
		rt_build(ifp->ctx, AF_INET);
		script_runreason(ifp, "IPV4LL");
	}
}

void
ipv4ll_reset(struct interface *ifp)
{
	struct ipv4ll_state *state = IPV4LL_STATE(ifp);

	if (state == NULL)
		return;
	ipv4ll_freearp(ifp);
	state->pickedaddr.s_addr = INADDR_ANY;
	state->seeded = false;
}

void
ipv4ll_free(struct interface *ifp)
{

	assert(ifp != NULL);

	ipv4ll_freearp(ifp);
	free(IPV4LL_STATE(ifp));
	ifp->if_data[IF_DATA_IPV4LL] = NULL;
}

/* This may cause issues in BSD systems, where running as a single dhcpcd
 * daemon would solve this issue easily. */
#ifdef HAVE_ROUTE_METRIC
int
ipv4ll_recvrt(__unused int cmd, const struct rt *rt)
{
	struct dhcpcd_ctx *ctx;
	struct interface *ifp;

	/* Only interested in default route changes. */
	if (sa_is_unspecified(&rt->rt_dest))
		return 0;

	/* If any interface is running IPv4LL, rebuild our routing table. */
	ctx = rt->rt_ifp->ctx;
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (IPV4LL_STATE_RUNNING(ifp)) {
			rt_build(ctx, AF_INET);
			break;
		}
	}

	return 0;
}
#endif

struct ipv4_addr *
ipv4ll_handleifa(int cmd, struct ipv4_addr *ia, pid_t pid)
{
	struct interface *ifp;
	struct ipv4ll_state *state;

	ifp = ia->iface;
	state = IPV4LL_STATE(ifp);
	if (state == NULL)
		return ia;

	if (cmd == RTM_DELADDR &&
	    state->addr != NULL &&
	    IN_ARE_ADDR_EQUAL(&state->addr->addr, &ia->addr))
	{
		loginfox("%s: pid %d deleted IP address %s",
		    ifp->name, pid, ia->saddr);
		ipv4ll_defend_failed(ifp);
		return ia;
	}

#ifdef IN_IFF_DUPLICATED
	if (cmd != RTM_NEWADDR)
		return ia;
	if (!IN_ARE_ADDR_EQUAL(&state->pickedaddr, &ia->addr))
		return ia;
	if (!(ia->addr_flags & IN_IFF_NOTUSEABLE))
		ipv4ll_not_found(ifp);
	else if (ia->addr_flags & IN_IFF_DUPLICATED) {
		logerrx("%s: DAD detected %s", ifp->name, ia->saddr);
		ipv4ll_freearp(ifp);
		if (ifp->options->options & DHCPCD_CONFIGURE)
			ipv4_deladdr(ia, 1);
		state->addr = NULL;
		rt_build(ifp->ctx, AF_INET);
		ipv4ll_found(ifp);
		return NULL;
	}
#endif

	return ia;
}
