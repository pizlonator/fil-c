/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - route management
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcpcd.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6.h"
#include "logerr.h"
#include "route.h"
#include "sa.h"

/* Needed for NetBSD-6, 7 and 8. */
#ifndef RB_TREE_FOREACH_SAFE
#ifndef RB_TREE_PREV
#define RB_TREE_NEXT(T, N) rb_tree_iterate((T), (N), RB_DIR_RIGHT)
#define RB_TREE_PREV(T, N) rb_tree_iterate((T), (N), RB_DIR_LEFT)
#endif
#define RB_TREE_FOREACH_SAFE(N, T, S) \
    for ((N) = RB_TREE_MIN(T); \
        (N) && ((S) = RB_TREE_NEXT((T), (N)), 1); \
        (N) = (S))
#define RB_TREE_FOREACH_REVERSE_SAFE(N, T, S) \
    for ((N) = RB_TREE_MAX(T); \
        (N) && ((S) = RB_TREE_PREV((T), (N)), 1); \
        (N) = (S))
#endif

#ifdef RT_FREE_ROUTE_TABLE_STATS
static size_t croutes;
static size_t nroutes;
static size_t froutes;
static size_t mroutes;
#endif

static void
rt_maskedaddr(struct sockaddr *dst,
	const struct sockaddr *addr, const struct sockaddr *netmask)
{
	const char *addrp = addr->sa_data, *netmaskp = netmask->sa_data;
	char *dstp = dst->sa_data;
	const char *addre = (char *)dst + sa_len(addr);
	const char *netmaske = (char *)dst + MIN(sa_len(addr), sa_len(netmask));

	dst->sa_family = addr->sa_family;
#ifdef HAVE_SA_LEN
	dst->sa_len = addr->sa_len;
#endif

	if (sa_is_unspecified(netmask)) {
		if (addre > dstp)
			memcpy(dstp, addrp, (size_t)(addre - dstp));
		return;
	}

	while (dstp < netmaske)
		*dstp++ = *addrp++ & *netmaskp++;
	if (dstp < addre)
		memset(dstp, 0, (size_t)(addre - dstp));
}

/*
 * On some systems, host routes have no need for a netmask.
 * However DHCP specifies host routes using an all-ones netmask.
 * This handy function allows easy comparison when the two
 * differ.
 */
static int
rt_cmp_netmask(const struct rt *rt1, const struct rt *rt2)
{

	if (rt1->rt_flags & RTF_HOST && rt2->rt_flags & RTF_HOST)
		return 0;
	return sa_cmp(&rt1->rt_netmask, &rt2->rt_netmask);
}

int
rt_cmp_dest(const struct rt *rt1, const struct rt *rt2)
{
	union sa_ss ma1 = { .sa.sa_family = AF_UNSPEC };
	union sa_ss ma2 = { .sa.sa_family = AF_UNSPEC };
	int c;

	rt_maskedaddr(&ma1.sa, &rt1->rt_dest, &rt1->rt_netmask);
	rt_maskedaddr(&ma2.sa, &rt2->rt_dest, &rt2->rt_netmask);
	c = sa_cmp(&ma1.sa, &ma2.sa);
	if (c != 0)
		return c;

	return rt_cmp_netmask(rt1, rt2);
}

static int
rt_compare_os(__unused void *context, const void *node1, const void *node2)
{
	const struct rt *rt1 = node1, *rt2 = node2;
	int c;

	/* Sort by masked destination. */
	c = rt_cmp_dest(rt1, rt2);
	if (c != 0)
		return c;

#ifdef HAVE_ROUTE_METRIC
	c = (int)(rt1->rt_ifp->metric - rt2->rt_ifp->metric);
#endif
	return c;
}

static int
rt_compare_list(__unused void *context, const void *node1, const void *node2)
{
	const struct rt *rt1 = node1, *rt2 = node2;

	if (rt1->rt_order > rt2->rt_order)
		return 1;
	if (rt1->rt_order < rt2->rt_order)
		return -1;
	return 0;
}

static int
rt_compare_proto(void *context, const void *node1, const void *node2)
{
	const struct rt *rt1 = node1, *rt2 = node2;
	int c;
	struct interface *ifp1, *ifp2;

	assert(rt1->rt_ifp != NULL);
	assert(rt2->rt_ifp != NULL);
	ifp1 = rt1->rt_ifp;
	ifp2 = rt2->rt_ifp;

	/* Prefer interfaces with a carrier. */
	c = ifp1->carrier - ifp2->carrier;
	if (c != 0)
		return -c;

	/* Prefer roaming over non roaming if both carriers are down. */
	if (ifp1->carrier == LINK_DOWN && ifp2->carrier == LINK_DOWN) {
		bool roam1 = if_roaming(ifp1);
		bool roam2 = if_roaming(ifp2);

		if (roam1 != roam2)
			return roam1 ? 1 : -1;
	}

#ifdef INET
	/* IPv4LL routes always come last */
	if (rt1->rt_dflags & RTDF_IPV4LL && !(rt2->rt_dflags & RTDF_IPV4LL))
		return -1;
	else if (!(rt1->rt_dflags & RTDF_IPV4LL) && rt2->rt_dflags & RTDF_IPV4LL)
		return 1;
#endif

	/* Lower metric interfaces come first. */
	c = (int)(ifp1->metric - ifp2->metric);
	if (c != 0)
		return c;

	/* Finally the order in which the route was given to us. */
	return rt_compare_list(context, rt1, rt2);
}

static const rb_tree_ops_t rt_compare_os_ops = {
	.rbto_compare_nodes = rt_compare_os,
	.rbto_compare_key = rt_compare_os,
	.rbto_node_offset = offsetof(struct rt, rt_tree),
	.rbto_context = NULL
};

const rb_tree_ops_t rt_compare_list_ops = {
	.rbto_compare_nodes = rt_compare_list,
	.rbto_compare_key = rt_compare_list,
	.rbto_node_offset = offsetof(struct rt, rt_tree),
	.rbto_context = NULL
};

const rb_tree_ops_t rt_compare_proto_ops = {
	.rbto_compare_nodes = rt_compare_proto,
	.rbto_compare_key = rt_compare_proto,
	.rbto_node_offset = offsetof(struct rt, rt_tree),
	.rbto_context = NULL
};

#ifdef RT_FREE_ROUTE_TABLE
static int
rt_compare_free(__unused void *context, const void *node1, const void *node2)
{

	return node1 == node2 ? 0 : node1 < node2 ? -1 : 1;
}

static const rb_tree_ops_t rt_compare_free_ops = {
	.rbto_compare_nodes = rt_compare_free,
	.rbto_compare_key = rt_compare_free,
	.rbto_node_offset = offsetof(struct rt, rt_tree),
	.rbto_context = NULL
};
#endif

void
rt_init(struct dhcpcd_ctx *ctx)
{

	rb_tree_init(&ctx->routes, &rt_compare_os_ops);
#ifdef RT_FREE_ROUTE_TABLE
	rb_tree_init(&ctx->froutes, &rt_compare_free_ops);
#endif
}

bool
rt_is_default(const struct rt *rt)
{

	return sa_is_unspecified(&rt->rt_dest) &&
	    sa_is_unspecified(&rt->rt_netmask);
}

static void
rt_desc(const char *cmd, const struct rt *rt)
{
	char dest[INET_MAX_ADDRSTRLEN], gateway[INET_MAX_ADDRSTRLEN];
	int prefix;
	const char *ifname;
	bool gateway_unspec;

	assert(cmd != NULL);
	assert(rt != NULL);

	sa_addrtop(&rt->rt_dest, dest, sizeof(dest));
	prefix = sa_toprefix(&rt->rt_netmask);
	sa_addrtop(&rt->rt_gateway, gateway, sizeof(gateway));
	gateway_unspec = sa_is_unspecified(&rt->rt_gateway);
	ifname = rt->rt_ifp == NULL ? "(null)" : rt->rt_ifp->name;

	if (rt->rt_flags & RTF_HOST) {
		if (gateway_unspec)
			loginfox("%s: %s host route to %s",
			    ifname, cmd, dest);
		else
			loginfox("%s: %s host route to %s via %s",
			    ifname, cmd, dest, gateway);
	} else if (rt_is_default(rt)) {
		if (gateway_unspec)
			loginfox("%s: %s default route",
			    ifname, cmd);
		else
			loginfox("%s: %s default route via %s",
			    ifname, cmd, gateway);
	} else if (gateway_unspec)
		loginfox("%s: %s%s route to %s/%d",
		    ifname, cmd,
		    rt->rt_flags & RTF_REJECT ? " reject" : "",
		    dest, prefix);
	else
		loginfox("%s: %s%s route to %s/%d via %s",
		    ifname, cmd,
		    rt->rt_flags & RTF_REJECT ? " reject" : "",
		    dest, prefix, gateway);
}

void
rt_headclear0(struct dhcpcd_ctx *ctx, rb_tree_t *rts, int af)
{
	struct rt *rt, *rtn;

	if (rts == NULL)
		return;
	assert(ctx != NULL);
#ifdef RT_FREE_ROUTE_TABLE
	assert(&ctx->froutes != rts);
#endif

	RB_TREE_FOREACH_SAFE(rt, rts, rtn) {
		if (af != AF_UNSPEC &&
		    rt->rt_dest.sa_family != af &&
		    rt->rt_gateway.sa_family != af)
			continue;
		rb_tree_remove_node(rts, rt);
		rt_free(rt);
	}
}

void
rt_headclear(rb_tree_t *rts, int af)
{
	struct rt *rt;

	if (rts == NULL || (rt = RB_TREE_MIN(rts)) == NULL)
		return;
	rt_headclear0(rt->rt_ifp->ctx, rts, af);
}

static void
rt_headfree(rb_tree_t *rts)
{
	struct rt *rt;

	while ((rt = RB_TREE_MIN(rts)) != NULL) {
		rb_tree_remove_node(rts, rt);
		free(rt);
	}
}

void
rt_dispose(struct dhcpcd_ctx *ctx)
{

	assert(ctx != NULL);
	rt_headfree(&ctx->routes);
#ifdef RT_FREE_ROUTE_TABLE
	rt_headfree(&ctx->froutes);
#ifdef RT_FREE_ROUTE_TABLE_STATS
	logdebugx("free route list used %zu times", froutes);
	logdebugx("new routes from route free list %zu", nroutes);
	logdebugx("maximum route free list size %zu", mroutes);
#endif
#endif
}

struct rt *
rt_new0(struct dhcpcd_ctx *ctx)
{
	struct rt *rt;

	assert(ctx != NULL);
#ifdef RT_FREE_ROUTE_TABLE
	if ((rt = RB_TREE_MIN(&ctx->froutes)) != NULL) {
		rb_tree_remove_node(&ctx->froutes, rt);
#ifdef RT_FREE_ROUTE_TABLE_STATS
		croutes--;
		nroutes++;
#endif
	} else
#endif
	if ((rt = malloc(sizeof(*rt))) == NULL) {
		logerr(__func__);
		return NULL;
	}
	memset(rt, 0, sizeof(*rt));
	return rt;
}

void
rt_setif(struct rt *rt, struct interface *ifp)
{

	assert(rt != NULL);
	assert(ifp != NULL);
	rt->rt_ifp = ifp;
#ifdef HAVE_ROUTE_METRIC
	rt->rt_metric = ifp->metric;
	if (if_roaming(ifp))
		rt->rt_metric += RTMETRIC_ROAM;
#endif
}

struct rt *
rt_new(struct interface *ifp)
{
	struct rt *rt;

	assert(ifp != NULL);
	if ((rt = rt_new0(ifp->ctx)) == NULL)
		return NULL;
	rt_setif(rt, ifp);
	return rt;
}

struct rt *
rt_proto_add_ctx(rb_tree_t *tree, struct rt *rt, struct dhcpcd_ctx *ctx)
{

	rt->rt_order = ctx->rt_order++;
	if (rb_tree_insert_node(tree, rt) == rt)
		return rt;

	rt_free(rt);
	errno = EEXIST;
	return NULL;
}

struct rt *
rt_proto_add(rb_tree_t *tree, struct rt *rt)
{

	assert (rt->rt_ifp != NULL);
	return rt_proto_add_ctx(tree, rt, rt->rt_ifp->ctx);
}

void
rt_free(struct rt *rt)
{
#ifdef RT_FREE_ROUTE_TABLE
	struct dhcpcd_ctx *ctx;

	assert(rt != NULL);
	if (rt->rt_ifp == NULL) {
		free(rt);
		return;
	}

	ctx = rt->rt_ifp->ctx;
	rb_tree_insert_node(&ctx->froutes, rt);
#ifdef RT_FREE_ROUTE_TABLE_STATS
	croutes++;
	froutes++;
	if (croutes > mroutes)
		mroutes = croutes;
#endif
#else
	free(rt);
#endif
}

void
rt_freeif(struct interface *ifp)
{
	struct dhcpcd_ctx *ctx;
	struct rt *rt, *rtn;

	if (ifp == NULL)
		return;
	ctx = ifp->ctx;
	RB_TREE_FOREACH_SAFE(rt, &ctx->routes, rtn) {
		if (rt->rt_ifp == ifp) {
			rb_tree_remove_node(&ctx->routes, rt);
			rt_free(rt);
		}
	}
}

/* If something other than dhcpcd removes a route,
 * we need to remove it from our internal table. */
void
rt_recvrt(int cmd, const struct rt *rt, pid_t pid)
{
	struct dhcpcd_ctx *ctx;
	struct rt *f;

	assert(rt != NULL);
	assert(rt->rt_ifp != NULL);
	assert(rt->rt_ifp->ctx != NULL);

	ctx = rt->rt_ifp->ctx;

	switch(cmd) {
	case RTM_DELETE:
		f = rb_tree_find_node(&ctx->routes, rt);
		if (f != NULL) {
			char buf[32];

			rb_tree_remove_node(&ctx->routes, f);
			snprintf(buf, sizeof(buf), "pid %d deleted", pid);
			rt_desc(buf, f);
			rt_free(f);
		}
		break;
	}

#if defined(IPV4LL) && defined(HAVE_ROUTE_METRIC)
	if (rt->rt_dest.sa_family == AF_INET)
		ipv4ll_recvrt(cmd, rt);
#endif
}

static bool
rt_add(rb_tree_t *kroutes, struct rt *nrt, struct rt *ort)
{
	struct dhcpcd_ctx *ctx;
	bool change, kroute, result;

	assert(nrt != NULL);
	ctx = nrt->rt_ifp->ctx;

	/*
	 * Don't install a gateway if not asked to.
	 * This option is mainly for VPN users who want their VPN to be the
	 * default route.
	 * Because VPN's generally don't care about route management
	 * beyond their own, a longer term solution would be to remove this
	 * and get the VPN to inject the default route into dhcpcd somehow.
	 */
	if (((nrt->rt_ifp->active &&
	    !(nrt->rt_ifp->options->options & DHCPCD_GATEWAY)) ||
	    (!nrt->rt_ifp->active && !(ctx->options & DHCPCD_GATEWAY))) &&
	    sa_is_unspecified(&nrt->rt_dest) &&
	    sa_is_unspecified(&nrt->rt_netmask))
		return false;

	rt_desc(ort == NULL ? "adding" : "changing", nrt);

	change = kroute = result = false;
	if (ort == NULL) {
		ort = rb_tree_find_node(kroutes, nrt);
		if (ort != NULL &&
		    ((ort->rt_flags & RTF_REJECT &&
		      nrt->rt_flags & RTF_REJECT) ||
		     (ort->rt_ifp == nrt->rt_ifp &&
#ifdef HAVE_ROUTE_METRIC
		    ort->rt_metric == nrt->rt_metric &&
#endif
		    sa_cmp(&ort->rt_gateway, &nrt->rt_gateway) == 0)))
		{
			if (ort->rt_mtu == nrt->rt_mtu)
				return true;
			change = true;
			kroute = true;
		}
	} else if (ort->rt_dflags & RTDF_FAKE &&
	    !(nrt->rt_dflags & RTDF_FAKE) &&
	    ort->rt_ifp == nrt->rt_ifp &&
#ifdef HAVE_ROUTE_METRIC
	    ort->rt_metric == nrt->rt_metric &&
#endif
	    sa_cmp(&ort->rt_dest, &nrt->rt_dest) == 0 &&
	    rt_cmp_netmask(ort, nrt) == 0 &&
	    sa_cmp(&ort->rt_gateway, &nrt->rt_gateway) == 0)
	{
		if (ort->rt_mtu == nrt->rt_mtu)
			return true;
		change = true;
	}

#ifdef RTF_CLONING
	/* BSD can set routes to be cloning routes.
	 * Cloned routes inherit the parent flags.
	 * As such, we need to delete and re-add the route to flush children
	 * to correct the flags. */
	if (change && ort != NULL && ort->rt_flags & RTF_CLONING)
		change = false;
#endif

	if (change) {
		if (if_route(RTM_CHANGE, nrt) != -1) {
			result = true;
			goto out;
		}
		if (errno != ESRCH)
			logerr("if_route (CHG)");
	}

#ifdef HAVE_ROUTE_METRIC
	/* With route metrics, we can safely add the new route before
	 * deleting the old route. */
	if (if_route(RTM_ADD, nrt) != -1) {
		if (ort != NULL) {
			if (if_route(RTM_DELETE, ort) == -1 && errno != ESRCH)
				logerr("if_route (DEL)");
		}
		result = true;
		goto out;
	}

	/* If the kernel claims the route exists we need to rip out the
	 * old one first. */
	if (errno != EEXIST || ort == NULL)
		goto logerr;
#endif

	/* No route metrics, we need to delete the old route before
	 * adding the new one. */
#ifdef ROUTE_PER_GATEWAY
	errno = 0;
#endif
	if (ort != NULL) {
		if (if_route(RTM_DELETE, ort) == -1 && errno != ESRCH)
			logerr("if_route (DEL)");
		else
			kroute = false;
	}
#ifdef ROUTE_PER_GATEWAY
	/* The OS allows many routes to the same dest with different gateways.
	 * dhcpcd does not support this yet, so for the time being just keep on
	 * deleting the route until there is an error. */
	if (ort != NULL && errno == 0) {
		for (;;) {
			if (if_route(RTM_DELETE, ort) == -1)
				break;
		}
	}
#endif

	/* Shouldn't need to check for EEXIST, but some kernels don't
	 * dump the subnet route just after we added the address. */
	if (if_route(RTM_ADD, nrt) != -1 || errno == EEXIST) {
		result = true;
		goto out;
	}

#ifdef HAVE_ROUTE_METRIC
logerr:
#endif
	logerr("if_route (ADD)");

out:
	if (kroute) {
		rb_tree_remove_node(kroutes, ort);
		rt_free(ort);
	}
	return result;
}

static bool
rt_delete(struct rt *rt)
{
	int retval;

	rt_desc("deleting", rt);
	retval = if_route(RTM_DELETE, rt) == -1 ? false : true;
	if (!retval && errno != ENOENT && errno != ESRCH)
		logerr(__func__);
	return retval;
}

static bool
rt_cmp(const struct rt *r1, const struct rt *r2)
{

	return (r1->rt_ifp == r2->rt_ifp &&
#ifdef HAVE_ROUTE_METRIC
	    r1->rt_metric == r2->rt_metric &&
#endif
	    sa_cmp(&r1->rt_gateway, &r2->rt_gateway) == 0);
}

static bool
rt_doroute(rb_tree_t *kroutes, struct rt *rt)
{
	struct dhcpcd_ctx *ctx;
	struct rt *or;

	ctx = rt->rt_ifp->ctx;
	/* Do we already manage it? */
	or = rb_tree_find_node(&ctx->routes, rt);
	if (or != NULL) {
		if (rt->rt_dflags & RTDF_FAKE)
			return true;
		if (or->rt_dflags & RTDF_FAKE ||
		    !rt_cmp(rt, or) ||
		    (rt->rt_ifa.sa_family != AF_UNSPEC &&
		    sa_cmp(&or->rt_ifa, &rt->rt_ifa) != 0) ||
		    or->rt_mtu != rt->rt_mtu)
		{
			if (!rt_add(kroutes, rt, or))
				return false;
		}
		rb_tree_remove_node(&ctx->routes, or);
		rt_free(or);
	} else {
		if (rt->rt_dflags & RTDF_FAKE) {
			or = rb_tree_find_node(kroutes, rt);
			if (or == NULL)
				return false;
			if (!rt_cmp(rt, or))
				return false;
		} else {
			if (!rt_add(kroutes, rt, NULL))
				return false;
		}
	}

	return true;
}

void
rt_build(struct dhcpcd_ctx *ctx, int af)
{
	rb_tree_t routes, added, kroutes;
	struct rt *rt, *rtn;
	unsigned long long o;

	rb_tree_init(&routes, &rt_compare_proto_ops);
	rb_tree_init(&added, &rt_compare_os_ops);
	rb_tree_init(&kroutes, &rt_compare_os_ops);
	if (if_initrt(ctx, &kroutes, af) != 0)
		logerr("%s: if_initrt", __func__);
	ctx->rt_order = 0;
	ctx->options |= DHCPCD_RTBUILD;

#ifdef INET
	if (!inet_getroutes(ctx, &routes))
		goto getfail;
#endif
#ifdef INET6
	if (!inet6_getroutes(ctx, &routes))
		goto getfail;
#endif

#ifdef BSD
	/* Rewind the miss filter */
	ctx->rt_missfilterlen = 0;
#endif

	RB_TREE_FOREACH_SAFE(rt, &routes, rtn) {
		if (rt->rt_ifp->active) {
			if (!(rt->rt_ifp->options->options & DHCPCD_CONFIGURE))
				continue;
		} else if (!(ctx->options & DHCPCD_CONFIGURE))
			continue;
#ifdef BSD
		if (rt_is_default(rt) &&
		    if_missfilter(rt->rt_ifp, &rt->rt_gateway) == -1)
			logerr("if_missfilter");
#endif
		if ((rt->rt_dest.sa_family != af &&
		    rt->rt_dest.sa_family != AF_UNSPEC) ||
		    (rt->rt_gateway.sa_family != af &&
		    rt->rt_gateway.sa_family != AF_UNSPEC))
			continue;
		/* Is this route already in our table? */
		if (rb_tree_find_node(&added, rt) != NULL)
			continue;
		if (rt_doroute(&kroutes, rt)) {
			rb_tree_remove_node(&routes, rt);
			if (rb_tree_insert_node(&added, rt) != rt) {
				errno = EEXIST;
				logerr(__func__);
				rt_free(rt);
			}
		}
	}

#ifdef BSD
	if (if_missfilter_apply(ctx) == -1 && errno != ENOTSUP)
		logerr("if_missfilter_apply");
#endif

	/* Remove old routes we used to manage. */
	RB_TREE_FOREACH_REVERSE_SAFE(rt, &ctx->routes, rtn) {
		if ((rt->rt_dest.sa_family != af &&
		    rt->rt_dest.sa_family != AF_UNSPEC) ||
		    (rt->rt_gateway.sa_family != af &&
		    rt->rt_gateway.sa_family != AF_UNSPEC))
			continue;
		rb_tree_remove_node(&ctx->routes, rt);
		if (rb_tree_find_node(&added, rt) == NULL) {
			o = rt->rt_ifp->options ?
			    rt->rt_ifp->options->options :
			    ctx->options;
			if ((o &
				(DHCPCD_EXITING | DHCPCD_PERSISTENT)) !=
				(DHCPCD_EXITING | DHCPCD_PERSISTENT))
				rt_delete(rt);
		}
		rt_free(rt);
	}

	/* XXX This needs to be optimised. */
	while ((rt = RB_TREE_MIN(&added)) != NULL) {
		rb_tree_remove_node(&added, rt);
		if (rb_tree_insert_node(&ctx->routes, rt) != rt) {
			errno = EEXIST;
			logerr(__func__);
			rt_free(rt);
		}
	}

getfail:
	rt_headclear(&routes, AF_UNSPEC);
	rt_headclear(&kroutes, AF_UNSPEC);
}
