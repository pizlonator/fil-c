/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - route management
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 * All rights reserved

 * rEDISTRIBUTION AND USE IN SOURCE AND BINARY FORMS, WITH OR WITHOUT
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

#ifndef ROUTE_H
#define ROUTE_H

#ifdef HAVE_SYS_RBTREE_H
#include <sys/rbtree.h>
#endif

#include <sys/socket.h>
#include <net/route.h>

#include <stdbool.h>

#include "dhcpcd.h"
#include "sa.h"

/*
 * Enable the route free list by default as
 * memory usage is still reported as low/unchanged even
 * when dealing with millions of routes.
 */
#if !defined(RT_FREE_ROUTE_TABLE)
#define	RT_FREE_ROUTE_TABLE 1
#elif RT_FREE_ROUTE_TABLE == 0
#undef	RT_FREE_ROUTE_TABLE
#endif

/* Some systems have route metrics.
 * OpenBSD route priority is not this. */
#ifndef HAVE_ROUTE_METRIC
# if defined(__linux__)
#  define HAVE_ROUTE_METRIC 1
# endif
#endif

#ifdef __linux__
# include <linux/version.h> /* RTA_PREF is only an enum.... */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#  define HAVE_ROUTE_PREF
# endif
#endif

#if defined(__OpenBSD__) || defined (__sun)
#  define ROUTE_PER_GATEWAY
/* XXX dhcpcd doesn't really support this yet.
 * But that's generally OK if only dhcpcd is managing routes. */
#endif

/* OpenBSD defines this as a "convienience" ..... we work around it. */
#ifdef __OpenBSD__
#undef rt_mtu
#endif

struct rt {
	union sa_ss		rt_ss_dest;
#define rt_dest			rt_ss_dest.sa
	union sa_ss		rt_ss_netmask;
#define rt_netmask		rt_ss_netmask.sa
	union sa_ss		rt_ss_gateway;
#define rt_gateway		rt_ss_gateway.sa
	struct interface	*rt_ifp;
	union sa_ss		rt_ss_ifa;
#define rt_ifa			rt_ss_ifa.sa
	unsigned int		rt_flags;
	unsigned int		rt_mtu;
#ifdef HAVE_ROUTE_METRIC
	unsigned int		rt_metric;
#endif
/* Maximum interface index is generally USHORT_MAX or 65535.
 * Add some padding for other stuff and we get offsets for the
 * below that should work automatically.
 * This is only an issue if the user defines higher metrics in
 * their configuration, but then they might wish to override also. */
#define	RTMETRIC_BASE		   1000U
#define	RTMETRIC_WIRELESS	   2000U
#define	RTMETRIC_IPV4LL		1000000U
#define	RTMETRIC_ROAM		2000000U
#ifdef HAVE_ROUTE_PREF
	int			rt_pref;
#endif
#define RTPREF_HIGH	1
#define RTPREF_MEDIUM	0	/* has to be zero */
#define RTPREF_LOW	(-1)
#define RTPREF_RESERVED	(-2)
#define RTPREF_INVALID	(-3)	/* internal */
	unsigned int		rt_dflags;
#define	RTDF_IFA_ROUTE		0x01		/* Address generated route */
#define	RTDF_FAKE		0x02		/* Maybe us on lease reboot */
#define	RTDF_IPV4LL		0x04		/* IPv4LL route */
#define	RTDF_RA			0x08		/* Router Advertisement */
#define	RTDF_DHCP		0x10		/* DHCP route */
#define	RTDF_STATIC		0x20		/* Configured in dhcpcd */
#define	RTDF_GATELINK		0x40		/* Gateway is on link */
	size_t			rt_order;
	rb_node_t		rt_tree;
};

extern const rb_tree_ops_t rt_compare_list_ops;
extern const rb_tree_ops_t rt_compare_proto_ops;

void rt_init(struct dhcpcd_ctx *);
void rt_dispose(struct dhcpcd_ctx *);
void rt_free(struct rt *);
void rt_freeif(struct interface *);
bool rt_is_default(const struct rt *);
void rt_headclear0(struct dhcpcd_ctx *, rb_tree_t *, int);
void rt_headclear(rb_tree_t *, int);
void rt_headfreeif(rb_tree_t *);
struct rt * rt_new0(struct dhcpcd_ctx *);
void rt_setif(struct rt *, struct interface *);
struct rt * rt_new(struct interface *);
struct rt * rt_proto_add_ctx(rb_tree_t *, struct rt *, struct dhcpcd_ctx *);
struct rt * rt_proto_add(rb_tree_t *, struct rt *);
int rt_cmp_dest(const struct rt *, const struct rt *);
void rt_recvrt(int, const struct rt *, pid_t);
void rt_build(struct dhcpcd_ctx *, int);

#endif
