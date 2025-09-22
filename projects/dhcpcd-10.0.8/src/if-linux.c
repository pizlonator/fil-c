/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Linux interface driver for dhcpcd
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

#include <asm/types.h> /* Needed for 2.4 kernels */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <linux/icmpv6.h>
#include <linux/if_addr.h>
#include <linux/if_link.h>
#include <linux/if_packet.h>
#include <linux/if_tun.h>
#include <linux/if_vlan.h>
#include <linux/filter.h>
#include <linux/netlink.h>
#include <linux/sockios.h>
#include <linux/rtnetlink.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <net/route.h>

/* musl has its own definition of struct ethhdr, so only include
 * netinet/if_ether.h on systems with GLIBC.  For the ARPHRD constants,
 * we must include linux/if_arp.h instead. */
#if defined(__GLIBC__)
#include <netinet/if_ether.h>
#else
#include <linux/if_arp.h>
#endif

/* Inlcude this *after* net/if.h so we get IFF_DORMANT */
#include <linux/if.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "bpf.h"
#include "common.h"
#include "dev.h"
#include "dhcp.h"
#include "if.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "route.h"
#include "sa.h"

#ifdef HAVE_NL80211_H
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#else
int if_getssid_wext(const char *ifname, uint8_t *ssid);
#endif

/* Support older kernels */
#ifndef IFLA_WIRELESS
#define IFLA_WIRELESS (IFLA_MASTER + 1)
#endif

/* For some reason, glibc doesn't include newer flags from linux/if.h
 * However, we cannot include linux/if.h directly as it conflicts
 * with the glibc version. D'oh! */
#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP	0x10000		/* driver signals L1 up		*/
#endif

/* Buggy CentOS and RedHat */
#ifndef SOL_NETLINK
#define	SOL_NETLINK	270
#endif

/*
 * Someone should fix kernel headers for clang alignment warnings.
 * But this is unlikely.
 * https://www.spinics.net/lists/netdev/msg646934.html
 */

#undef NLA_ALIGNTO
#undef NLA_ALIGN
#undef NLA_HDRLEN
#define NLA_ALIGNTO		4U
#define NLA_ALIGN(len)		(((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_HDRLEN		((int) NLA_ALIGN(sizeof(struct nlattr)))

#undef IFA_RTA
#define IFA_RTA(r)  ((struct rtattr *)(void *)(((char *)(r)) \
	+ NLMSG_ALIGN(sizeof(struct ifaddrmsg))))
#undef IFLA_RTA
#define IFLA_RTA(r)  ((struct rtattr *)(void *)(((char *)(r)) \
	+ NLMSG_ALIGN(sizeof(struct ifinfomsg))))
#undef NLMSG_NEXT
#define NLMSG_NEXT(nlh, len)	 ((len) -= NLMSG_ALIGN((nlh)->nlmsg_len), \
	(struct nlmsghdr *)(void *)(((char *)(nlh)) \
	+ NLMSG_ALIGN((nlh)->nlmsg_len)))
#undef RTM_RTA
#define RTM_RTA(r) (void *)(((char *)(r)) + NLMSG_ALIGN(sizeof(struct rtmsg)))
#undef RTA_NEXT
#define RTA_NEXT(rta, attrlen)	((attrlen) -= RTA_ALIGN((rta)->rta_len), \
	(struct rtattr *)(void *)(((char *)(rta)) \
	+ RTA_ALIGN((rta)->rta_len)))

/* We need this to send a broadcast for InfiniBand.
 * Our old code used sendto, but our new code writes to a raw BPF socket.
 * What header structure does IPoIB use? */
#if 0
/* Broadcast address for IPoIB */
static const uint8_t ipv4_bcast_addr[] = {
	0x00, 0xff, 0xff, 0xff,
	0xff, 0x12, 0x40, 0x1b, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};
#endif

static int if_addressexists(struct interface *, struct in_addr *);

#define PROC_PROMOTE	"/proc/sys/net/ipv4/conf/%s/promote_secondaries"
#define SYS_BRIDGE	"/sys/class/net/%s/bridge/bridge_id"
#define SYS_LAYER2	"/sys/class/net/%s/device/layer2"
#define SYS_TUNTAP	"/sys/class/net/%s/tun_flags"

#if defined(__aarch64__)
static const char *mproc = "AArch64";
int
if_machinearch(char *str, size_t len)
{

	return snprintf(str, len, "%s", mproc);
}
#else
static const char *mproc =
#if defined(__alpha__)
	"system type"
#elif defined(__arm__)
	"Hardware"
#elif defined(__avr32__)
	"cpu family"
#elif defined(__bfin__)
	"BOARD Name"
#elif defined(__cris__)
	"cpu model"
#elif defined(__frv__)
	"System"
#elif defined(__hppa__)
	"model"
#elif defined(__i386__) || defined(__x86_64__)
	"vendor_id"
#elif defined(__ia64__)
	"vendor"
#elif defined(__m68k__)
	"MMU"
#elif defined(__mips__)
	"system type"
#elif defined(__powerpc__) || defined(__powerpc64__)
	"machine"
#elif defined(__riscv)
	"uarch"
#elif defined(__s390__) || defined(__s390x__)
	"Manufacturer"
#elif defined(__sh__)
	"machine"
#elif defined(sparc) || defined(__sparc__)
	"cpu"
#elif defined(__vax__)
	"cpu"
#else
	NULL
#endif
	;

int
if_machinearch(char *str, size_t len)
{
	FILE *fp;
	char buf[256];

	if (mproc == NULL) {
		errno = EINVAL;
		return -1;
	}

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL)
		return -1;

	while (fscanf(fp, "%255s : ", buf) != EOF) {
		if (strncmp(buf, mproc, strlen(mproc)) == 0 &&
		    fscanf(fp, "%255s", buf) == 1)
		{
			fclose(fp);
			return snprintf(str, len, "%s", buf);
		}
	}
	fclose(fp);
	errno = ESRCH;
	return -1;
}
#endif

static int
check_proc_int(struct dhcpcd_ctx *ctx, const char *path)
{
	char buf[64];
	int error, i;

	if (dhcp_readfile(ctx, path, buf, sizeof(buf)) == -1)
		return -1;
	i = (int)strtoi(buf, NULL, 0, INT_MIN, INT_MAX, &error);
	if (error != 0 && error != ENOTSUP) {
		errno = error;
		return -1;
	}
	return i;
}

static int
check_proc_uint(struct dhcpcd_ctx *ctx, const char *path, unsigned int *u)
{
	char buf[64];
	int error;

	if (dhcp_readfile(ctx, path, buf, sizeof(buf)) == -1)
		return -1;
	*u = (unsigned int)strtou(buf, NULL, 0, 0, UINT_MAX, &error);
	if (error != 0 && error != ENOTSUP) {
		errno = error;
		return error;
	}
	return 0;
}

static ssize_t
if_writepathuint(struct dhcpcd_ctx *ctx, const char *path, unsigned int val)
{
	char buf[64];
	int len;

	len = snprintf(buf, sizeof(buf), "%u\n", val);
	if (len == -1)
		return -1;
	return dhcp_writefile(ctx, path, 0664, buf, (size_t)len);
}

int
if_init(struct interface *ifp)
{
	char path[sizeof(PROC_PROMOTE) + IF_NAMESIZE];
	int n;

	/* We enable promote_secondaries so that we can do this
	 * add 192.168.1.2/24
	 * add 192.168.1.3/24
	 * del 192.168.1.2/24
	 * and the subnet mask moves onto 192.168.1.3/24
	 * This matches the behaviour of BSD which makes coding dhcpcd
	 * a little easier as there's just one behaviour. */
	snprintf(path, sizeof(path), PROC_PROMOTE, ifp->name);
	n = check_proc_int(ifp->ctx, path);
	if (n == -1)
		return errno == ENOENT ? 0 : -1;
	if (n == 1)
		return 0;
	return if_writepathuint(ifp->ctx, path, 1) == -1 ? -1 : 0;
}

int
if_conf(struct interface *ifp)
{
	char path[sizeof(SYS_LAYER2) + IF_NAMESIZE];
	int n;

	/* Some qeth setups require the use of the broadcast flag. */
	snprintf(path, sizeof(path), SYS_LAYER2, ifp->name);
	n = check_proc_int(ifp->ctx, path);
	if (n == -1)
		return errno == ENOENT ? 0 : -1;
	if (n == 0)
		ifp->options->options |= DHCPCD_BROADCAST;
	return 0;
}

static bool
if_bridge(struct dhcpcd_ctx *ctx, const char *ifname)
{
	char path[sizeof(SYS_BRIDGE) + IF_NAMESIZE], buf[64];

	snprintf(path, sizeof(path), SYS_BRIDGE, ifname);
	if (dhcp_readfile(ctx, path, buf, sizeof(buf)) == -1)
		return false;
	return true;
}

static bool
if_tap(struct dhcpcd_ctx *ctx, const char *ifname)
{
	char path[sizeof(SYS_TUNTAP) + IF_NAMESIZE];
	unsigned int u;

	snprintf(path, sizeof(path), SYS_TUNTAP, ifname);
	if (check_proc_uint(ctx, path, &u) == -1)
		return false;
	return u & IFF_TAP;
}

bool
if_ignore(struct dhcpcd_ctx *ctx, const char *ifname)
{

	if (if_tap(ctx, ifname) || if_bridge(ctx, ifname))
		return true;
	return false;
}

/* XXX work out Virtal Interface Masters */
int
if_vimaster(__unused struct dhcpcd_ctx *ctx, __unused const char *ifname)
{

	return 0;
}

unsigned short
if_vlanid(const struct interface *ifp)
{
	struct vlan_ioctl_args v;

	memset(&v, 0, sizeof(v));
	strlcpy(v.device1, ifp->name, sizeof(v.device1));
	v.cmd = GET_VLAN_VID_CMD;
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFVLAN, &v) != 0)
		return 0; /* 0 means no VLANID */
	return (unsigned short)v.u.VID;
}

int
if_linksocket(struct sockaddr_nl *nl, int protocol, int flags)
{
	int fd;

	fd = xsocket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC | flags, protocol);
	if (fd == -1)
		return -1;
	nl->nl_family = AF_NETLINK;
	if (bind(fd, (struct sockaddr *)nl, sizeof(*nl)) == -1) {
		close(fd);
		return -1;
	}
	return fd;
}

char *
if_getnetworknamespace(char *buf, size_t len)
{
	struct stat sb_self, sb_netns;
	DIR *dir;
	struct dirent *de;
	char file[PATH_MAX], *bufp = NULL;

	if (stat("/proc/self/ns/net", &sb_self) == -1)
		return NULL;

	dir = opendir("/var/run/netns");
	if (dir == NULL)
		return NULL;

	while ((de = readdir(dir)) != NULL) {
		snprintf(file, sizeof(file), "/var/run/netns/%s", de->d_name);
		if (stat(file, &sb_netns) == -1)
			continue;
		if (sb_self.st_dev != sb_netns.st_dev ||
		    sb_self.st_ino != sb_netns.st_ino)
			continue;
		strlcpy(buf, de->d_name, len);
		bufp = buf;
		break;
	}
	closedir(dir);
	return bufp;
}

int
os_init(void)
{
	char netns[PATH_MAX], *p;

	p = if_getnetworknamespace(netns, sizeof(netns));
	if (p != NULL)
		loginfox("network namespace: %s", p);

	return 0;
}

int
if_opensockets_os(struct dhcpcd_ctx *ctx)
{
	struct priv *priv;
	struct sockaddr_nl snl;
	socklen_t len;
#ifdef NETLINK_BROADCAST_ERROR
	int on = 1;
#endif

#ifdef PRIVSEP
	if (ctx->options & DHCPCD_PRIVSEPROOT) {
		ctx->link_fd = -1;
		goto setup_priv;
	}
#endif

	/* Open the link socket first so it gets pid() for the socket.
	 * Then open our persistent route socket so we get a unique
	 * pid that doesn't clash with a process id for after we fork. */
	memset(&snl, 0, sizeof(snl));
	snl.nl_groups = RTMGRP_LINK;

#ifdef INET
	snl.nl_groups |= RTMGRP_IPV4_ROUTE | RTMGRP_IPV4_IFADDR;
#endif
#ifdef INET6
	snl.nl_groups |= RTMGRP_IPV6_ROUTE | RTMGRP_IPV6_IFADDR | RTMGRP_NEIGH;
#endif

	ctx->link_fd = if_linksocket(&snl, NETLINK_ROUTE, SOCK_NONBLOCK);
	if (ctx->link_fd == -1)
		return -1;
#ifdef NETLINK_BROADCAST_ERROR
	if (setsockopt(ctx->link_fd, SOL_NETLINK, NETLINK_BROADCAST_ERROR,
	    &on, sizeof(on)) == -1)
		logerr("%s: NETLINK_BROADCAST_ERROR", __func__);
#endif

#ifdef PRIVSEP
setup_priv:
#endif
	if ((priv = calloc(1, sizeof(*priv))) == NULL)
		return -1;

	ctx->priv = priv;
	memset(&snl, 0, sizeof(snl));
	priv->route_fd = if_linksocket(&snl, NETLINK_ROUTE, 0);
	if (priv->route_fd == -1)
		return -1;
	len = sizeof(snl);
	if (getsockname(priv->route_fd, (struct sockaddr *)&snl, &len) == -1)
		return -1;
	priv->route_pid = snl.nl_pid;

	memset(&snl, 0, sizeof(snl));
	priv->generic_fd = if_linksocket(&snl, NETLINK_GENERIC, 0);
	if (priv->generic_fd == -1)
		return -1;

	return 0;
}

void
if_closesockets_os(struct dhcpcd_ctx *ctx)
{
	struct priv *priv = ctx->priv;

	if (priv == NULL)
		return;

	if (priv->route_fd != -1) {
		close(priv->route_fd);
		priv->route_fd = -1;
	}
	if (priv->generic_fd != -1) {
		close(priv->generic_fd);
		priv->generic_fd = -1;
	}

	free(priv);
	ctx->priv = NULL;
}

int
if_setmac(struct interface *ifp, void *mac, uint8_t maclen)
{
	struct ifreq ifr = {
		.ifr_hwaddr.sa_family = ifp->hwtype,
	};

	if (ifp->hwlen != maclen || maclen > sizeof(ifr.ifr_hwaddr.sa_data)) {
		errno = EINVAL;
		return -1;
	}

	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	memcpy(ifr.ifr_hwaddr.sa_data, mac, maclen);
	return if_ioctl(ifp->ctx, SIOCSIFHWADDR, &ifr, sizeof(ifr));
}

static int if_carrier_from_flags(unsigned int flags)
{

#ifdef IFF_LOWER_UP
	return ((flags & (IFF_LOWER_UP | IFF_RUNNING)) ==
		(IFF_LOWER_UP | IFF_RUNNING))
#ifdef IFF_DORMANT
		&& !(flags & IFF_DORMANT)
#endif
		? LINK_UP : LINK_DOWN;
#else
	return flags & IFF_RUNNING ? LINK_UP : LINK_DOWN;
#endif
}

int
if_carrier(struct interface *ifp, __unused const void *ifadata)
{
	return if_carrier_from_flags(ifp->flags);
}

bool
if_roaming(struct interface *ifp)
{

	if (!ifp->wireless ||
	    ifp->flags & IFF_RUNNING)
		return false;

#if defined(IFF_DORMANT)
	return ifp->flags & IFF_DORMANT;
#elif defined(IFF_LOWER_UP)
	return (ifp->flags & (IFF_UP|IFF_LOWER_UP)) == (IFF_UP|IFF_LOWER_UP);
#else
	return false;
#endif
}

int
if_getnetlink(struct dhcpcd_ctx *ctx, struct iovec *iov, int fd, int flags,
    int (*cb)(struct dhcpcd_ctx *, void *, struct nlmsghdr *), void *cbarg)
{
	struct sockaddr_nl nladdr = { .nl_pid = 0 };
	struct msghdr msg = {
	    .msg_name = &nladdr, .msg_namelen = sizeof(nladdr),
	    .msg_iov = iov, .msg_iovlen = 1,
	};
	size_t len;
	struct nlmsghdr *nlm;
	int r = 0;
	unsigned int again;
	bool terminated;

recv_again:
	len = (size_t)recvmsg(fd, &msg, flags);
	if (len == 0 || (ssize_t)len == -1)
		return (int)len;

	/* Check sender */
	if (msg.msg_namelen != sizeof(nladdr)) {
		errno = EINVAL;
		return -1;
	}

	/* Ignore message if it is not from kernel */
	if (nladdr.nl_pid != 0)
		return 0;

	again = 0;
	terminated = false;
	for (nlm = iov->iov_base;
	     nlm && NLMSG_OK(nlm, len);
	     nlm = NLMSG_NEXT(nlm, len))
	{
		again = (nlm->nlmsg_flags & NLM_F_MULTI);
		if (nlm->nlmsg_type == NLMSG_NOOP)
			continue;

		if (nlm->nlmsg_type == NLMSG_ERROR) {
			struct nlmsgerr *err;

			if (nlm->nlmsg_len - sizeof(*nlm) < sizeof(*err)) {
				errno = EBADMSG;
				return -1;
			}
			err = (struct nlmsgerr *)NLMSG_DATA(nlm);
			if (err->error != 0) {
				errno = -err->error;
				return -1;
			}
			again = 0;
			terminated = true;
			break;
		}
		if (nlm->nlmsg_type == NLMSG_DONE) {
			again = 0;
			terminated = true;
			break;
		}
		if (cb == NULL)
			continue;
		if (nlm->nlmsg_seq != (uint32_t)ctx->seq && fd != ctx->link_fd)
			logwarnx("%s: received sequence %u, expecting %d",
			    __func__, nlm->nlmsg_seq, ctx->seq);
		else
			r = cb(ctx, cbarg, nlm);
	}

	if ((again || !terminated) && (ctx != NULL && ctx->link_fd != fd))
		goto recv_again;

	return r;
}

static int
if_copyrt(struct dhcpcd_ctx *ctx, struct rt *rt, struct nlmsghdr *nlm)
{
	size_t len;
	struct rtmsg *rtm;
	struct rtattr *rta;
	unsigned int ifindex;
	struct sockaddr *sa;

	len = nlm->nlmsg_len - sizeof(*nlm);
	if (len < sizeof(*rtm)) {
		errno = EBADMSG;
		return -1;
	}
	rtm = (struct rtmsg *)NLMSG_DATA(nlm);
	if (rtm->rtm_table != RT_TABLE_MAIN)
		return -1;

	memset(rt, 0, sizeof(*rt));
	if (rtm->rtm_type == RTN_UNREACHABLE)
		rt->rt_flags |= RTF_REJECT;

	rta = RTM_RTA(rtm);
	len = RTM_PAYLOAD(nlm);
	for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
		sa = NULL;
		switch (rta->rta_type) {
		case RTA_DST:
			sa = &rt->rt_dest;
			break;
		case RTA_GATEWAY:
			sa = &rt->rt_gateway;
			break;
		case RTA_PREFSRC:
			sa = &rt->rt_ifa;
			break;
		case RTA_OIF:
			ifindex = *(unsigned int *)RTA_DATA(rta);
			rt->rt_ifp = if_findindex(ctx->ifaces, ifindex);
			break;
		case RTA_PRIORITY:
			rt->rt_metric = *(unsigned int *)RTA_DATA(rta);
			break;
		case RTA_METRICS:
		{
			struct rtattr *r2;
			size_t l2;

			l2 = rta->rta_len;
			r2 = (struct rtattr *)RTA_DATA(rta);
			for (; RTA_OK(r2, l2); r2 = RTA_NEXT(r2, l2)) {
				switch (r2->rta_type) {
				case RTAX_MTU:
					rt->rt_mtu = *(unsigned int *)RTA_DATA(r2);
					break;
				}
			}
			break;
		}
		}

		if (sa != NULL) {
			socklen_t salen;

			sa->sa_family = rtm->rtm_family;
			salen = sa_addrlen(sa);
			/* sa is a union where sockaddr_in6 is the biggest. */
			/* coverity[overrun-buffer-arg] */
			memcpy((char *)sa + sa_addroffset(sa), RTA_DATA(rta),
			    MIN(salen, RTA_PAYLOAD(rta)));
		}
	}

	/* If no RTA_DST set the unspecified address for the family. */
	if (rt->rt_dest.sa_family == AF_UNSPEC)
		rt->rt_dest.sa_family = rtm->rtm_family;

	rt->rt_netmask.sa_family = rtm->rtm_family;
	sa_fromprefix(&rt->rt_netmask, rtm->rtm_dst_len);
	if (sa_is_allones(&rt->rt_netmask))
		rt->rt_flags |= RTF_HOST;

	#if 0
	if (rt->rtp_ifp == NULL && rt->src.s_addr != INADDR_ANY) {
		struct ipv4_addr *ap;

		/* For some reason the default route comes back with the
		 * loopback interface in RTA_OIF? Lets find it by
		 * preferred source address */
		if ((ap = ipv4_findaddr(ctx, &rt->src)))
			rt->iface = ap->iface;
	}
	#endif

	if (rt->rt_ifp == NULL) {
		errno = ESRCH;
		return -1;
	}
	return 0;
}

static int
link_route(struct dhcpcd_ctx *ctx, __unused struct interface *ifp,
    struct nlmsghdr *nlm)
{
	size_t len;
	int cmd;
	struct priv *priv;
	struct rt rt;

	switch (nlm->nlmsg_type) {
	case RTM_NEWROUTE:
		cmd = RTM_ADD;
		break;
	case RTM_DELROUTE:
		cmd = RTM_DELETE;
		break;
	default:
		return 0;
	}

	len = nlm->nlmsg_len - sizeof(*nlm);
	if (len < sizeof(struct rtmsg)) {
		errno = EBADMSG;
		return -1;
	}

	/* Ignore messages we sent. */
#ifdef PRIVSEP
	if (ctx->ps_root != NULL &&
	    nlm->nlmsg_pid == (uint32_t)ctx->ps_root->psp_pid)
		return 0;
#endif
	priv = (struct priv *)ctx->priv;
	if (nlm->nlmsg_pid == priv->route_pid)
		return 0;

	if (if_copyrt(ctx, &rt, nlm) == 0)
		rt_recvrt(cmd, &rt, (pid_t)nlm->nlmsg_pid);

	return 0;
}

static int
link_addr(struct dhcpcd_ctx *ctx, struct interface *ifp, struct nlmsghdr *nlm)
{
	size_t len;
	struct rtattr *rta;
	struct ifaddrmsg *ifa;
	struct priv *priv;
#ifdef INET
	struct in_addr addr, net, brd;
	int ret;
#endif
#ifdef INET6
	struct in6_addr addr6;
	int flags;
#endif

	if (nlm->nlmsg_type != RTM_DELADDR && nlm->nlmsg_type != RTM_NEWADDR)
		return 0;

	len = nlm->nlmsg_len - sizeof(*nlm);
	if (len < sizeof(*ifa)) {
		errno = EBADMSG;
		return -1;
	}

	/* Ignore address deletions from ourself.
	 * We need to process address flag changes though. */
	if (nlm->nlmsg_type == RTM_DELADDR) {
#ifdef PRIVSEP
		if (ctx->ps_root != NULL &&
		    nlm->nlmsg_pid == (uint32_t)ctx->ps_root->psp_pid)
			return 0;
#endif
		priv = (struct priv*)ctx->priv;
		if (nlm->nlmsg_pid == priv->route_pid)
			return 0;
	}

	ifa = NLMSG_DATA(nlm);
	if ((ifp = if_findindex(ctx->ifaces, ifa->ifa_index)) == NULL) {
		/* We don't know about the interface the address is for
		 * so it's not really an error */
		return 1;
	}
	rta = IFA_RTA(ifa);
	len = NLMSG_PAYLOAD(nlm, sizeof(*ifa));
	switch (ifa->ifa_family) {
#ifdef INET
	case AF_INET:
		addr.s_addr = brd.s_addr = INADDR_ANY;
		inet_cidrtoaddr(ifa->ifa_prefixlen, &net);
		for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
			switch (rta->rta_type) {
			case IFA_ADDRESS:
				if (ifp->flags & IFF_POINTOPOINT) {
					memcpy(&brd.s_addr, RTA_DATA(rta),
					    sizeof(brd.s_addr));
				}
				break;
			case IFA_BROADCAST:
				memcpy(&brd.s_addr, RTA_DATA(rta),
				    sizeof(brd.s_addr));
				break;
			case IFA_LOCAL:
				memcpy(&addr.s_addr, RTA_DATA(rta),
				    sizeof(addr.s_addr));
				break;
			}
		}

		/* Validate RTM_DELADDR really means address deleted
		 * and anything else really means address exists. */
		ret = if_addressexists(ifp, &addr);
		if (ret == -1) {
			logerr("if_addressexists: %s", inet_ntoa(addr));
			break;
		} else if (ret == 1) {
			if (nlm->nlmsg_type == RTM_DELADDR)
				break;
		} else {
			if (nlm->nlmsg_type != RTM_DELADDR)
				break;
		}

		ipv4_handleifa(ctx, nlm->nlmsg_type, NULL, ifp->name,
		    &addr, &net, &brd, ifa->ifa_flags, (pid_t)nlm->nlmsg_pid);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		memset(&addr6, 0, sizeof(addr6));
		for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
			switch (rta->rta_type) {
			case IFA_ADDRESS:
				memcpy(&addr6.s6_addr, RTA_DATA(rta),
				       sizeof(addr6.s6_addr));
				break;
			}
		}

		/* Validate RTM_DELADDR really means address deleted
		 * and anything else really means address exists. */
		flags = if_addrflags6(ifp, &addr6, NULL);
		if (nlm->nlmsg_type == RTM_DELADDR) {
			if (flags != -1)
				break;
		} else {
			if (flags == -1)
				break;
		}

		ipv6_handleifa(ctx, nlm->nlmsg_type, NULL, ifp->name,
		    &addr6, ifa->ifa_prefixlen, ifa->ifa_flags,
		    (pid_t)nlm->nlmsg_pid);
		break;
#endif
	}
	return 0;
}

static uint8_t
l2addr_len(unsigned short if_type)
{

	switch (if_type) {
	case ARPHRD_ETHER: /* FALLTHROUGH */
	case ARPHRD_IEEE802: /*FALLTHROUGH */
	case ARPHRD_IEEE80211:
		return 6;
	case ARPHRD_IEEE1394:
		return 8;
	case ARPHRD_INFINIBAND:
		return 20;
	}

	/* Impossible */
	return 0;
}

#ifdef INET6
static int
link_neigh(struct dhcpcd_ctx *ctx, __unused struct interface *ifp,
    struct nlmsghdr *nlm)
{
	struct ndmsg *r;
	struct rtattr *rta;
	size_t len;

	if (nlm->nlmsg_type != RTM_NEWNEIGH && nlm->nlmsg_type != RTM_DELNEIGH)
		return 0;
	if (nlm->nlmsg_len < sizeof(*r))
		return -1;

	r = NLMSG_DATA(nlm);
	rta = RTM_RTA(r);
	len = RTM_PAYLOAD(nlm);
	if (r->ndm_family == AF_INET6) {
		bool unreachable;
		struct in6_addr addr6;

		unreachable = (nlm->nlmsg_type == RTM_NEWNEIGH &&
		    r->ndm_state & NUD_FAILED);
		memset(&addr6, 0, sizeof(addr6));
		for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
			switch (rta->rta_type) {
			case NDA_DST:
				memcpy(&addr6.s6_addr, RTA_DATA(rta),
				       sizeof(addr6.s6_addr));
				break;
			}
		}
		ipv6nd_neighbour(ctx, &addr6, !unreachable);
	}

	return 0;
}
#endif

static int
link_netlink(struct dhcpcd_ctx *ctx, void *arg, struct nlmsghdr *nlm)
{
	struct interface *ifp = arg;
	int r;
	size_t len;
	struct rtattr *rta, *hwaddr;
	struct ifinfomsg *ifi;
	char ifn[IF_NAMESIZE + 1];

	r = link_route(ctx, ifp, nlm);
	if (r != 0)
		return r;
	r = link_addr(ctx, ifp, nlm);
	if (r != 0)
		return r;
#ifdef INET6
	r = link_neigh(ctx, ifp, nlm);
	if (r != 0)
		return r;
#endif

	if (nlm->nlmsg_type != RTM_NEWLINK && nlm->nlmsg_type != RTM_DELLINK)
		return 0;
	len = nlm->nlmsg_len - sizeof(*nlm);
	if ((size_t)len < sizeof(*ifi)) {
		errno = EBADMSG;
		return -1;
	}
	ifi = NLMSG_DATA(nlm);
	if (ifi->ifi_flags & IFF_LOOPBACK)
		return 0;
	rta = (void *)((char *)ifi + NLMSG_ALIGN(sizeof(*ifi)));
	len = NLMSG_PAYLOAD(nlm, sizeof(*ifi));
	*ifn = '\0';
	hwaddr = NULL;

	for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
		switch (rta->rta_type) {
		case IFLA_WIRELESS:
			/* Ignore wireless messages */
			if (nlm->nlmsg_type == RTM_NEWLINK &&
			    ifi->ifi_change == 0)
				return 0;
			break;
		case IFLA_IFNAME:
			strlcpy(ifn, (char *)RTA_DATA(rta), sizeof(ifn));
			break;
		case IFLA_ADDRESS:
			hwaddr = rta;
			break;
		}
	}

	if (nlm->nlmsg_type == RTM_DELLINK) {
#ifdef PLUGIN_DEV
		/* If are listening to a dev manager, let that remove
		 * the interface rather than the kernel. */
		if (dev_listening(ctx) < 1)
#endif
			dhcpcd_handleinterface(ctx, -1, ifn);
		return 0;
	}

	/* Virtual interfaces may not get a valid hardware address
	 * at this point.
	 * To trigger a valid hardware address pickup we need to pretend
	 * that that don't exist until they have one. */
	if (ifi->ifi_flags & IFF_MASTER && !hwaddr) {
		dhcpcd_handleinterface(ctx, -1, ifn);
		return 0;
	}

	/* Check for a new interface */
	ifp = if_findindex(ctx->ifaces, (unsigned int)ifi->ifi_index);
	if (ifp == NULL) {
#ifdef PLUGIN_DEV
		/* If are listening to a dev manager, let that announce
		 * the interface rather than the kernel. */
		if (dev_listening(ctx) < 1)
#endif
			dhcpcd_handleinterface(ctx, 1, ifn);
		return 0;
	}

	/* Handle interface being renamed */
	if (strcmp(ifp->name, ifn) != 0) {
		dhcpcd_handleinterface(ctx, -1, ifn);
		dhcpcd_handleinterface(ctx, 1, ifn);
		return 0;
	}

	/* Re-read hardware address and friends */
	if (!(ifi->ifi_flags & IFF_UP)) {
		void *hwa = hwaddr != NULL ? RTA_DATA(hwaddr) : NULL;
		uint8_t hwl = l2addr_len(ifi->ifi_type);

		if (hwaddr != NULL && hwaddr->rta_len != RTA_LENGTH(hwl))
			hwa = NULL;
		dhcpcd_handlehwaddr(ifp, ifi->ifi_type, hwa, hwl);
	}

	dhcpcd_handlecarrier(ifp,
	    if_carrier_from_flags(ifi->ifi_flags),
	    ifi->ifi_flags);
	return 0;
}

int
if_handlelink(struct dhcpcd_ctx *ctx)
{
	unsigned char buf[16 * 1024];
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};

	return if_getnetlink(ctx, &iov, ctx->link_fd, MSG_DONTWAIT,
	    &link_netlink, NULL);
}

#ifdef PRIVSEP
static bool
if_netlinkpriv(int protocol, struct nlmsghdr *nlm)
{

	if (protocol != NETLINK_ROUTE)
		return false;

	switch(nlm->nlmsg_type) {
	case RTM_NEWADDR:	/* FALLTHROUGH */
	case RTM_DELADDR:	/* FALLTHROUGH */
	case RTM_NEWROUTE:	/* FALLTHROUGH */
	case RTM_DELROUTE:	/* FALLTHROUGH */
	case RTM_NEWLINK:
		return true;
	default:
		return false;
	}
}
#endif

static int
if_sendnetlink(struct dhcpcd_ctx *ctx, int protocol, struct nlmsghdr *hdr,
    int (*cb)(struct dhcpcd_ctx *, void *, struct nlmsghdr *), void *cbarg)
{
	int s;
	struct sockaddr_nl snl = { .nl_family = AF_NETLINK };
	struct iovec iov = { .iov_base = hdr, .iov_len = hdr->nlmsg_len };
	struct msghdr msg = {
	    .msg_name = &snl, .msg_namelen = sizeof(snl),
	    .msg_iov = &iov, .msg_iovlen = 1
	};
	struct priv *priv = (struct priv *)ctx->priv;
	unsigned char buf[16 * 1024];
	struct iovec riov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};

	/* Request a reply */
	hdr->nlmsg_flags |= NLM_F_ACK;
	hdr->nlmsg_seq = (uint32_t)++ctx->seq;
	if ((unsigned int)ctx->seq > UINT32_MAX)
		ctx->seq = 0;

#ifdef PRIVSEP
	if (ctx->options & DHCPCD_PRIVSEP && if_netlinkpriv(protocol, hdr))
		return (int)ps_root_sendnetlink(ctx, protocol, &msg);
#endif

	switch (protocol) {
	case NETLINK_ROUTE:
		s = priv->route_fd;
		break;
	case NETLINK_GENERIC:
		s = priv->generic_fd;
#if 0
#ifdef NETLINK_GET_STRICT_CHK
		if (hdr->nlmsg_type == RTM_GETADDR) {
			int on = 1;

			if (setsockopt(s, SOL_NETLINK, NETLINK_GET_STRICT_CHK,
			    &on, sizeof(on)) == -1 && errno != ENOPROTOOPT)
				logerr("%s: NETLINK_GET_STRICT_CHK", __func__);
		}
#endif
#endif
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if (sendmsg(s, &msg, 0) == -1)
		return -1;

	return if_getnetlink(ctx, &riov, s, 0, cb, cbarg);
}

#define NLMSG_TAIL(nmsg)						\
	((struct rtattr *)(void *)(((char *)(nmsg)) + \
	    NLMSG_ALIGN((nmsg)->nlmsg_len)))

static int
add_attr_l(struct nlmsghdr *n, unsigned short maxlen, unsigned short type,
    const void *data, unsigned short alen)
{
	unsigned short len = (unsigned short)RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
		errno = ENOBUFS;
		return -1;
	}

	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	if (alen)
		memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);

	return 0;
}

static int
add_attr_8(struct nlmsghdr *n, unsigned short maxlen, unsigned short type,
    uint8_t data)
{

	return add_attr_l(n, maxlen, type, &data, sizeof(data));
}

static int
add_attr_32(struct nlmsghdr *n, unsigned short maxlen, unsigned short type,
    uint32_t data)
{
	unsigned short len = RTA_LENGTH(sizeof(data));
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen) {
		errno = ENOBUFS;
		return -1;
	}

	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), &data, sizeof(data));
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;

	return 0;
}

static int
rta_add_attr_32(struct rtattr *rta, unsigned short maxlen,
    unsigned short type, uint32_t data)
{
	unsigned short len = RTA_LENGTH(sizeof(data));
	struct rtattr *subrta;

	if (RTA_ALIGN(rta->rta_len) + len > maxlen) {
		errno = ENOBUFS;
		return -1;
	}

	subrta = (void *)((char*)rta + RTA_ALIGN(rta->rta_len));
	subrta->rta_type = type;
	subrta->rta_len = len;
	memcpy(RTA_DATA(subrta), &data, sizeof(data));
	rta->rta_len = (unsigned short)(NLMSG_ALIGN(rta->rta_len) + len);
	return 0;
}

#ifdef HAVE_NL80211_H
static struct nlattr *
nla_next(struct nlattr *nla, size_t *rem)
{

	*rem -= (size_t)NLA_ALIGN(nla->nla_len);
	return (void *)((char *)nla + NLA_ALIGN(nla->nla_len));
}

#define NLA_TYPE(nla) ((nla)->nla_type & NLA_TYPE_MASK)
#define NLA_LEN(nla) (unsigned int)((nla)->nla_len - NLA_HDRLEN)
#define NLA_OK(nla, rem) \
	((rem) >= sizeof(struct nlattr) && \
	(nla)->nla_len >= sizeof(struct nlattr) && \
	(nla)->nla_len <= rem)
#define NLA_DATA(nla) (void *)((char *)(nla) + NLA_HDRLEN)
#define NLA_FOR_EACH_ATTR(pos, head, len, rem) \
	for (pos = head, rem = len; \
	     NLA_OK(pos, rem); \
	     pos = nla_next(pos, &(rem)))

struct nlmg
{
	struct nlmsghdr hdr;
	struct genlmsghdr ghdr;
	char buffer[64];
};

static int
nla_put_32(struct nlmsghdr *n, unsigned short maxlen,
    unsigned short type, uint32_t data)
{
	unsigned short len;
	struct nlattr *nla;

	len = NLA_ALIGN(NLA_HDRLEN + sizeof(data));
	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen) {
		errno = ENOBUFS;
		return -1;
	}

	nla = (struct nlattr *)NLMSG_TAIL(n);
	nla->nla_type = type;
	nla->nla_len = len;
	memcpy(NLA_DATA(nla), &data, sizeof(data));
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;

	return 0;
}

static int
nla_put_string(struct nlmsghdr *n, unsigned short maxlen,
    unsigned short type, const char *data)
{
	struct nlattr *nla;
	size_t len, sl;

	sl = strlen(data) + 1;
	len = NLA_ALIGN(NLA_HDRLEN + sl);
	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen) {
		errno = ENOBUFS;
		return -1;
	}

	nla = (struct nlattr *)NLMSG_TAIL(n);
	nla->nla_type = type;
	nla->nla_len = (unsigned short)len;
	memcpy(NLA_DATA(nla), data, sl);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + (unsigned short)len;
	return 0;
}

static int
nla_parse(struct nlattr *tb[], struct nlattr *head, size_t len, int maxtype)
{
	struct nlattr *nla;
	size_t rem;
	int type;

	memset(tb, 0, sizeof(*tb) * ((unsigned int)maxtype + 1));
	NLA_FOR_EACH_ATTR(nla, head, len, rem) {
		type = NLA_TYPE(nla);
		if (type > maxtype)
			continue;
		tb[type] = nla;
	}
	return 0;
}

static int
genl_parse(struct nlmsghdr *nlm, struct nlattr *tb[], int maxtype)
{
	struct genlmsghdr *ghdr;
	struct nlattr *head;
	size_t len;

	ghdr = NLMSG_DATA(nlm);
	head = (void *)((char *)ghdr + GENL_HDRLEN);
	len = nlm->nlmsg_len - GENL_HDRLEN - NLMSG_HDRLEN;
	return nla_parse(tb, head, len, maxtype);
}

static int
_gnl_getfamily(__unused struct dhcpcd_ctx *ctx, __unused void *arg,
    struct nlmsghdr *nlm)
{
	struct nlattr *tb[CTRL_ATTR_FAMILY_ID + 1];
	uint16_t family;

	if (genl_parse(nlm, tb, CTRL_ATTR_FAMILY_ID) == -1)
		return -1;
	if (tb[CTRL_ATTR_FAMILY_ID] == NULL) {
		errno = ENOENT;
		return -1;
	}
	memcpy(&family, NLA_DATA(tb[CTRL_ATTR_FAMILY_ID]), sizeof(family));
	return (int)family;
}

static int
gnl_getfamily(struct dhcpcd_ctx *ctx, const char *name)
{
	struct nlmg nlm;

	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct genlmsghdr));
	nlm.hdr.nlmsg_type = GENL_ID_CTRL;
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST;
	nlm.ghdr.cmd = CTRL_CMD_GETFAMILY;
	nlm.ghdr.version = 1;
	if (nla_put_string(&nlm.hdr, sizeof(nlm),
	    CTRL_ATTR_FAMILY_NAME, name) == -1)
		return -1;
	return if_sendnetlink(ctx, NETLINK_GENERIC, &nlm.hdr,
	    &_gnl_getfamily, NULL);
}

static int
_if_getssid_nl80211(__unused struct dhcpcd_ctx *ctx, void *arg,
    struct nlmsghdr *nlm)
{
	struct interface *ifp = arg;
	struct nlattr *tb[NL80211_ATTR_BSS + 1];
	struct nlattr *bss[NL80211_BSS_STATUS + 1];
	uint32_t status;
	unsigned char *ie;
	int ie_len;

	if (genl_parse(nlm, tb, NL80211_ATTR_BSS) == -1)
		return 0;

	if (tb[NL80211_ATTR_BSS] == NULL)
		return 0;

	if (nla_parse(bss,
	    NLA_DATA(tb[NL80211_ATTR_BSS]),
	    NLA_LEN(tb[NL80211_ATTR_BSS]),
	    NL80211_BSS_STATUS) == -1)
		return 0;

	if (bss[NL80211_BSS_BSSID] == NULL || bss[NL80211_BSS_STATUS] == NULL)
		return 0;

	memcpy(&status, NLA_DATA(bss[NL80211_BSS_STATUS]), sizeof(status));
	if (status != NL80211_BSS_STATUS_ASSOCIATED)
		return 0;

	if (bss[NL80211_BSS_INFORMATION_ELEMENTS] == NULL)
		return 0;

	ie = NLA_DATA(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
	ie_len = (int)NLA_LEN(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
	/* ie[0] is type, ie[1] is lenth, ie[2..] is data */
	while (ie_len >= 2 && ie_len >= ie[1]) {
		if (ie[0] == 0) {
			/* SSID */
			if (ie[1] > IF_SSIDLEN) {
				errno = ENOBUFS;
				return -1;
			}
			ifp->ssid_len = ie[1];
			memcpy(ifp->ssid, ie + 2, ifp->ssid_len);
			return (int)ifp->ssid_len;
		}
		ie_len -= ie[1] + 2;
		ie += ie[1] + 2;
	}

	return 0;
}

static int
if_getssid_nl80211(struct interface *ifp)
{
	int family;
	struct nlmg nlm;

	errno = 0;
	family = gnl_getfamily(ifp->ctx, "nl80211");
	if (family == -1)
		return -1;

	/* Is this a wireless interface? */
	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct genlmsghdr));
	nlm.hdr.nlmsg_type = (unsigned short)family;
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST;
	nlm.ghdr.cmd = NL80211_CMD_GET_WIPHY;
	nla_put_32(&nlm.hdr, sizeof(nlm), NL80211_ATTR_IFINDEX, ifp->index);
	if (if_sendnetlink(ifp->ctx, NETLINK_GENERIC, &nlm.hdr,
	    NULL, NULL) == -1)
		return -1;

	/* We need to parse out the list of scan results and find the one
	 * we are connected to. */
	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct genlmsghdr));
	nlm.hdr.nlmsg_type = (unsigned short)family;
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlm.ghdr.cmd = NL80211_CMD_GET_SCAN;
	nla_put_32(&nlm.hdr, sizeof(nlm), NL80211_ATTR_IFINDEX, ifp->index);

	return if_sendnetlink(ifp->ctx, NETLINK_GENERIC, &nlm.hdr,
	    &_if_getssid_nl80211, ifp);
}
#endif

int
if_getssid(struct interface *ifp)
{
	int r;

#ifdef HAVE_NL80211_H
	r = if_getssid_nl80211(ifp);
	if (r == -1)
		ifp->ssid_len = 0;
#else
	r = if_getssid_wext(ifp->name, ifp->ssid);
	if (r != -1)
		ifp->ssid_len = (unsigned int)r;
#endif

	ifp->ssid[ifp->ssid_len] = '\0';
	return r;
}

struct nlma
{
	struct nlmsghdr hdr;
	struct ifaddrmsg ifa;
	char buffer[64];
};

#ifdef INET
struct ifiaddr
{
	unsigned int ifa_ifindex;
	struct in_addr ifa_addr;
	bool ifa_found;
};

static int
_if_addressexists(__unused struct dhcpcd_ctx *ctx,
    void *arg, struct nlmsghdr *nlm)
{
	struct ifiaddr *ia = arg;
	in_addr_t this_addr;
	size_t len;
	struct rtattr *rta;
	struct ifaddrmsg *ifa;

	ifa = NLMSG_DATA(nlm);
	if (ifa->ifa_index != ia->ifa_ifindex || ifa->ifa_family != AF_INET)
		return 0;

	rta = IFA_RTA(ifa);
	len = NLMSG_PAYLOAD(nlm, sizeof(*ifa));
	for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
		switch (rta->rta_type) {
		case IFA_LOCAL:
			memcpy(&this_addr, RTA_DATA(rta), sizeof(this_addr));
			if (this_addr == ia->ifa_addr.s_addr) {
				ia->ifa_found = true;
				return 1;
			}
			break;
		}
	}
	return 0;
}

static int
if_addressexists(struct interface *ifp, struct in_addr *addr)
{
	struct ifiaddr ia = {
		.ifa_ifindex = ifp->index,
		.ifa_addr = *addr,
		.ifa_found = false,
	};
	struct nlma nlm = {
	    .hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg)),
	    .hdr.nlmsg_type = RTM_GETADDR,
	    .hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH,
	    .ifa.ifa_family = AF_INET,
	    .ifa.ifa_index = ifp->index,
	};

	int error = if_sendnetlink(ifp->ctx, NETLINK_ROUTE, &nlm.hdr,
	    &_if_addressexists, &ia);
	if (error == -1)
		return -1;
	return ia.ifa_found ? 1 : 0;
}
#endif

struct nlmr
{
	struct nlmsghdr hdr;
	struct rtmsg rt;
	char buffer[256];
};

int
if_route(unsigned char cmd, const struct rt *rt)
{
	struct nlmr nlm;
	bool gateway_unspec;

	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	switch (cmd) {
	case RTM_CHANGE:
		nlm.hdr.nlmsg_type = RTM_NEWROUTE;
		nlm.hdr.nlmsg_flags = NLM_F_CREATE | NLM_F_REPLACE;
		break;
	case RTM_ADD:
		nlm.hdr.nlmsg_type = RTM_NEWROUTE;
		nlm.hdr.nlmsg_flags = NLM_F_CREATE | NLM_F_EXCL;
		break;
	case RTM_DELETE:
		nlm.hdr.nlmsg_type = RTM_DELROUTE;
		break;
	}
	nlm.hdr.nlmsg_flags |= NLM_F_REQUEST;
	nlm.rt.rtm_family = (unsigned char)rt->rt_dest.sa_family;
	nlm.rt.rtm_table = RT_TABLE_MAIN;

	gateway_unspec = sa_is_unspecified(&rt->rt_gateway);

	if (cmd == RTM_DELETE) {
		nlm.rt.rtm_scope = RT_SCOPE_NOWHERE;
	} else {
		/* Address generated routes are RTPROT_KERNEL,
		 * otherwise RTPROT_BOOT */
#ifdef RTPROT_RA
		if (rt->rt_dflags & RTDF_RA)
			nlm.rt.rtm_protocol = RTPROT_RA;
		else
#endif
#ifdef RTPROT_DHCP
		if (rt->rt_dflags & RTDF_DHCP)
			nlm.rt.rtm_protocol = RTPROT_DHCP;
		else
#endif
		if (rt->rt_dflags & RTDF_IFA_ROUTE)
			nlm.rt.rtm_protocol = RTPROT_KERNEL;
		else
			nlm.rt.rtm_protocol = RTPROT_BOOT;
		if (rt->rt_ifp->flags & IFF_LOOPBACK)
			nlm.rt.rtm_scope = RT_SCOPE_HOST;
		else if (gateway_unspec)
			nlm.rt.rtm_scope = RT_SCOPE_LINK;
		else
			nlm.rt.rtm_scope = RT_SCOPE_UNIVERSE;
		if (rt->rt_flags & RTF_REJECT)
			nlm.rt.rtm_type = RTN_UNREACHABLE;
		else
			nlm.rt.rtm_type = RTN_UNICAST;
	}

#define ADDSA(type, sa)							\
	add_attr_l(&nlm.hdr, sizeof(nlm), (type),			\
	    (const char *)(sa) + sa_addroffset((sa)),			\
	    (unsigned short)sa_addrlen((sa)));
	nlm.rt.rtm_dst_len = (unsigned char)sa_toprefix(&rt->rt_netmask);
	/* rt->rt_dest and rt->gateway are unions where sockaddr_in6
	 * is the biggest member. However, we access them as the
	 * generic sockaddr and coverity thinks this will overrun. */
	/* coverity[overrun-buffer-arg] */
	ADDSA(RTA_DST, &rt->rt_dest);
	if (cmd == RTM_ADD || cmd == RTM_CHANGE) {
		if (!gateway_unspec) {
			/* coverity[overrun-buffer-arg] */
			ADDSA(RTA_GATEWAY, &rt->rt_gateway);
		}
		/* Cannot add tentative source addresses.
		 * We don't know this here, so just skip INET6 ifa's.*/
		if (!sa_is_unspecified(&rt->rt_ifa) &&
		    rt->rt_ifa.sa_family != AF_INET6)
			ADDSA(RTA_PREFSRC, &rt->rt_ifa);
		if (rt->rt_mtu) {
			char metricsbuf[32];
			struct rtattr *metrics = (void *)metricsbuf;

			metrics->rta_type = RTA_METRICS;
			metrics->rta_len = RTA_LENGTH(0);
			rta_add_attr_32(metrics, sizeof(metricsbuf),
			    RTAX_MTU, rt->rt_mtu);
			add_attr_l(&nlm.hdr, sizeof(nlm), RTA_METRICS,
			    RTA_DATA(metrics),
			    (unsigned short)RTA_PAYLOAD(metrics));
		}

#ifdef HAVE_ROUTE_PREF
		if (rt->rt_dflags & RTDF_RA) {
			uint8_t pref;

			switch(rt->rt_pref) {
			case RTPREF_LOW:
				pref = ICMPV6_ROUTER_PREF_LOW;
				break;
			case RTPREF_MEDIUM:
				pref = ICMPV6_ROUTER_PREF_MEDIUM;
				break;
			case RTPREF_HIGH:
				pref = ICMPV6_ROUTER_PREF_HIGH;
				break;
			default:
				pref = ICMPV6_ROUTER_PREF_INVALID;
				break;
			}
			add_attr_8(&nlm.hdr, sizeof(nlm), RTA_PREF, pref);
		}
#endif
	}

	if (!sa_is_loopback(&rt->rt_gateway))
		add_attr_32(&nlm.hdr, sizeof(nlm), RTA_OIF, rt->rt_ifp->index);

	if (rt->rt_metric != 0)
		add_attr_32(&nlm.hdr, sizeof(nlm), RTA_PRIORITY,
		    rt->rt_metric);

	return if_sendnetlink(rt->rt_ifp->ctx, NETLINK_ROUTE, &nlm.hdr,
	    NULL, NULL);
}

static int
_if_initrt(struct dhcpcd_ctx *ctx, void *arg,
    struct nlmsghdr *nlm)
{
	struct rt rt, *rtn;
	rb_tree_t *kroutes = arg;

	if (if_copyrt(ctx, &rt, nlm) != 0)
		return 0;
	if ((rtn = rt_new(rt.rt_ifp)) == NULL) {
		logerr(__func__);
		return 0;
	}
	memcpy(rtn, &rt, sizeof(*rtn));
	if (rb_tree_insert_node(kroutes, rtn) != rtn)
		rt_free(rtn);
	return 0;
}

int
if_initrt(struct dhcpcd_ctx *ctx, rb_tree_t *kroutes, int af)
{
	struct nlmr nlm = {
	    .hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)),
	    .hdr.nlmsg_type = RTM_GETROUTE,
	    .hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH,
	    .rt.rtm_table = RT_TABLE_MAIN,
	    .rt.rtm_family = (unsigned char)af,
	};

	return if_sendnetlink(ctx, NETLINK_ROUTE, &nlm.hdr,
	    &_if_initrt, kroutes);
}

#ifdef INET
/* Linux is a special snowflake when it comes to BPF. */
const char *bpf_name = "Packet Socket";

/* Linux is a special snowflake for opening BPF. */
struct bpf *
bpf_open(const struct interface *ifp,
    int (*filter)(const struct bpf *, const struct in_addr *),
    const struct in_addr *ia)
{
	struct bpf *bpf;
	union sockunion {
		struct sockaddr sa;
		struct sockaddr_ll sll;
		struct sockaddr_storage ss;
	} su = {
		.sll = {
			.sll_family = PF_PACKET,
			.sll_protocol = htons(ETH_P_ALL),
			.sll_ifindex = (int)ifp->index,
		}
	};
#ifdef PACKET_AUXDATA
	int n;
#endif

	bpf = calloc(1, sizeof(*bpf));
	if (bpf == NULL)
		return NULL;
	bpf->bpf_ifp = ifp;

	/* Allocate a suitably large buffer for a single packet. */
	bpf->bpf_size = ETH_DATA_LEN;
	bpf->bpf_buffer = malloc(bpf->bpf_size);
	if (bpf->bpf_buffer == NULL)
		goto eexit;

	bpf->bpf_fd = xsocket(PF_PACKET, SOCK_RAW|SOCK_CXNB,htons(ETH_P_ALL));
	if (bpf->bpf_fd == -1)
		goto eexit;

	/* We cannot validate the correct interface,
	 * so we MUST set this first. */
	if (bind(bpf->bpf_fd, &su.sa, sizeof(su.sll)) == -1)
		goto eexit;

	if (filter(bpf, ia) != 0)
		goto eexit;

	/* In the ideal world, this would be set before the bind and filter. */
#ifdef PACKET_AUXDATA
	n = 1;
	if (setsockopt(bpf->bpf_fd, SOL_PACKET, PACKET_AUXDATA,
	    &n, sizeof(n)) != 0) {
		if (errno != ENOPROTOOPT)
			goto eexit;
	}
#endif

	/*
	 * At this point we could have received packets for the wrong
	 * interface or which don't pass the filter.
	 * Linux should flush upon setting the filter like every other OS.
	 * There is no way of flushing them from userland.
	 * As such, consumers need to inspect each packet to ensure it's valid.
	 * Or to put it another way, don't trust the Linux BPF filter.
	*/

	return bpf;

eexit:
	if (bpf->bpf_fd != -1)
		close(bpf->bpf_fd);
	free(bpf->bpf_buffer);
	free(bpf);
	return NULL;
}

/* BPF requires that we read the entire buffer.
 * So we pass the buffer in the API so we can loop on >1 packet. */
ssize_t
bpf_read(struct bpf *bpf, void *data, size_t len)
{
	ssize_t bytes;
	struct iovec iov = {
		.iov_base = bpf->bpf_buffer,
		.iov_len = bpf->bpf_size,
	};
	struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1 };
#ifdef PACKET_AUXDATA
	union {
		struct cmsghdr hdr;
		uint8_t buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
	} cmsgbuf = { .buf = { 0 } };
	struct cmsghdr *cmsg;
	struct tpacket_auxdata *aux;
#endif

#ifdef PACKET_AUXDATA
	msg.msg_control = cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);
#endif

	bytes = recvmsg(bpf->bpf_fd, &msg, 0);
	if (bytes == -1)
		return -1;
	bpf->bpf_flags |= BPF_EOF; /* We only ever read one packet. */
	bpf->bpf_flags &= ~BPF_PARTIALCSUM;
	if (bytes) {
		if (bpf_frame_bcast(bpf->bpf_ifp, bpf->bpf_buffer) == 0)
			bpf->bpf_flags |= BPF_BCAST;
		else
			bpf->bpf_flags &= ~BPF_BCAST;
		if ((size_t)bytes > len)
			bytes = (ssize_t)len;
		memcpy(data, bpf->bpf_buffer, (size_t)bytes);
#ifdef PACKET_AUXDATA
		for (cmsg = CMSG_FIRSTHDR(&msg);
		     cmsg;
		     cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
			if (cmsg->cmsg_level == SOL_PACKET &&
			    cmsg->cmsg_type == PACKET_AUXDATA) {
				aux = (void *)CMSG_DATA(cmsg);
				if (aux->tp_status & TP_STATUS_CSUMNOTREADY)
					bpf->bpf_flags |= BPF_PARTIALCSUM;
			}
		}
#endif
	}
	return bytes;
}

int
bpf_attach(int s, void *filter, unsigned int filter_len)
{
	struct sock_fprog pf = {
		.filter = filter,
		.len = (unsigned short)filter_len,
	};

	/* Install the filter. */
	if (setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER, &pf, sizeof(pf)) == -1)
		return -1;

#ifdef SO_LOCK_FILTER
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_LOCK_FILTER, &on, sizeof(on)) == -1)
		return -1;
#endif

	return 0;
}

int
if_address(unsigned char cmd, const struct ipv4_addr *ia)
{
	struct nlma nlm;
	struct ifa_cacheinfo cinfo;
	int retval = 0;
#ifdef IFA_F_NOPREFIXROUTE
	uint32_t flags = 0;
#endif

	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST;
	nlm.hdr.nlmsg_type = cmd;
	if (cmd == RTM_NEWADDR)
		nlm.hdr.nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
	nlm.ifa.ifa_index = ia->iface->index;
	nlm.ifa.ifa_family = AF_INET;

	nlm.ifa.ifa_prefixlen = inet_ntocidr(ia->mask);

#if 0
	/* This creates the aliased interface */
	add_attr_l(&nlm.hdr, sizeof(nlm), IFA_LABEL,
	    ia->iface->alias,
	    (unsigned short)(strlen(ia->iface->alias) + 1));
#endif

	add_attr_l(&nlm.hdr, sizeof(nlm), IFA_LOCAL,
	    &ia->addr.s_addr, sizeof(ia->addr.s_addr));

	if (cmd == RTM_NEWADDR) {
#ifdef IFA_F_NOPREFIXROUTE
		if (nlm.ifa.ifa_prefixlen < 32)
			flags |= IFA_F_NOPREFIXROUTE;
		add_attr_32(&nlm.hdr, sizeof(nlm), IFA_FLAGS, flags);
#endif

		add_attr_l(&nlm.hdr, sizeof(nlm), IFA_BROADCAST,
		    &ia->brd.s_addr, sizeof(ia->brd.s_addr));

		memset(&cinfo, 0, sizeof(cinfo));
		cinfo.ifa_prefered = ia->pltime;
		cinfo.ifa_valid = ia->vltime;
		add_attr_l(&nlm.hdr, sizeof(nlm), IFA_CACHEINFO,
		    &cinfo, sizeof(cinfo));
	}

	if (if_sendnetlink(ia->iface->ctx, NETLINK_ROUTE, &nlm.hdr,
	    NULL, NULL) == -1)
		retval = -1;
	return retval;
}

int
if_addrflags(__unused const struct interface *ifp,
__unused const struct in_addr *addr, __unused const char *alias)
{

	/* Linux has no support for IPv4 address flags */
	return 0;
}
#endif

#ifdef INET6
int
if_address6(unsigned char cmd, const struct ipv6_addr *ia)
{
	struct nlma nlm;
	struct ifa_cacheinfo cinfo;
#if defined(IFA_F_MANAGETEMPADDR) || defined(IFA_F_NOPREFIXROUTE)
	uint32_t flags = 0;
#endif

	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST;
	nlm.hdr.nlmsg_type = cmd;
	if (cmd == RTM_NEWADDR)
		nlm.hdr.nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
	nlm.ifa.ifa_index = ia->iface->index;
	nlm.ifa.ifa_family = AF_INET6;

	/* Add as /128 if no IFA_F_NOPREFIXROUTE ? */
	nlm.ifa.ifa_prefixlen = ia->prefix_len;

#if 0
	/* This creates the aliased interface */
	add_attr_l(&nlm.hdr, sizeof(nlm), IFA_LABEL,
	    ia->iface->alias, (unsigned short)(strlen(ia->iface->alias) + 1));
#endif
	add_attr_l(&nlm.hdr, sizeof(nlm), IFA_LOCAL,
	    &ia->addr.s6_addr, sizeof(ia->addr.s6_addr));

	if (cmd == RTM_NEWADDR) {
#ifdef IPV6_MANAGETEMPADDR
		if (ia->flags & IPV6_AF_TEMPORARY) {
			/* Currently the kernel filters out these flags */
#ifdef IFA_F_NOPREFIXROUTE
			flags |= IFA_F_TEMPORARY;
#else
			nlm.ifa.ifa_flags |= IFA_F_TEMPORARY;
#endif
		}
#elif IFA_F_MANAGETEMPADDR
		if (ia->flags & IPV6_AF_AUTOCONF && IA6_CANAUTOCONF(ia))
			flags |= IFA_F_MANAGETEMPADDR;
#endif
#ifdef IFA_F_NOPREFIXROUTE
		if (!IN6_IS_ADDR_LINKLOCAL(&ia->addr))
			flags |= IFA_F_NOPREFIXROUTE;
#endif
#if defined(IFA_F_MANAGETEMPADDR) || defined(IFA_F_NOPREFIXROUTE)
		add_attr_32(&nlm.hdr, sizeof(nlm), IFA_FLAGS, flags);
#endif

		memset(&cinfo, 0, sizeof(cinfo));
		cinfo.ifa_prefered = ia->prefix_pltime;
		cinfo.ifa_valid = ia->prefix_vltime;
		add_attr_l(&nlm.hdr, sizeof(nlm), IFA_CACHEINFO,
		    &cinfo, sizeof(cinfo));
	}

	return if_sendnetlink(ia->iface->ctx, NETLINK_ROUTE, &nlm.hdr,
	    NULL, NULL);
}

struct ifiaddr6
{
	unsigned int ifa_ifindex;
	struct in6_addr ifa_addr;
	uint32_t ifa_flags;
	bool ifa_found;
};

static int
_if_addrflags6(__unused struct dhcpcd_ctx *ctx,
    void *arg, struct nlmsghdr *nlm)
{
	struct ifiaddr6 *ia = arg;
	size_t len;
	struct rtattr *rta;
	struct ifaddrmsg *ifa;
	struct in6_addr *local = NULL, *address = NULL;
	uint32_t *flags = NULL;

	ifa = NLMSG_DATA(nlm);
	if (ifa->ifa_index != ia->ifa_ifindex || ifa->ifa_family != AF_INET6)
		return 0;

	rta = IFA_RTA(ifa);
	len = NLMSG_PAYLOAD(nlm, sizeof(*ifa));
	for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
		switch (rta->rta_type) {
		case IFA_ADDRESS:
			address = (struct in6_addr *)RTA_DATA(rta);
			break;
		case IFA_LOCAL:
			local = (struct in6_addr *)RTA_DATA(rta);
			break;
		case IFA_FLAGS:
			flags = (uint32_t *)RTA_DATA(rta);
			break;
		}
	}

	if (local) {
	       if (IN6_ARE_ADDR_EQUAL(&ia->ifa_addr, local))
			ia->ifa_found = true;
	} else if (address) {
	       if (IN6_ARE_ADDR_EQUAL(&ia->ifa_addr, address))
			ia->ifa_found = true;
	}
	if (flags && ia->ifa_found)
		memcpy(&ia->ifa_flags, flags, sizeof(ia->ifa_flags));
	return 0;
}

int
if_addrflags6(const struct interface *ifp, const struct in6_addr *addr,
    __unused const char *alias)
{
	struct ifiaddr6 ia = {
		.ifa_ifindex = ifp->index,
		.ifa_addr = *addr,
		.ifa_found = false,
	};
	struct nlma nlm = {
	    .hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg)),
	    .hdr.nlmsg_type = RTM_GETADDR,
	    .hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH,
	    .ifa.ifa_family = AF_INET6,
	    .ifa.ifa_index = ifp->index,
	};

	int error = if_sendnetlink(ifp->ctx, NETLINK_ROUTE, &nlm.hdr,
	    &_if_addrflags6, &ia);
	if (error == -1)
		return -1;
	if (!ia.ifa_found) {
		errno = ESRCH;
		return -1;
	}
	return (int)ia.ifa_flags;
}

int
if_getlifetime6(__unused struct ipv6_addr *ia)
{

	/* God knows how to work out address lifetimes on Linux */
	errno = ENOTSUP;
	return -1;
}

struct nlml
{
	struct nlmsghdr hdr;
	struct ifinfomsg i;
	char buffer[32];
};

#ifdef HAVE_IN6_ADDR_GEN_MODE_NONE
static struct rtattr *
add_attr_nest(struct nlmsghdr *n, unsigned short maxlen, unsigned short type)
{
	struct rtattr *nest;

	nest = NLMSG_TAIL(n);
	add_attr_l(n, maxlen, type, NULL, 0);
	return nest;
}

static void
add_attr_nest_end(struct nlmsghdr *n, struct rtattr *nest)
{

	nest->rta_len = (unsigned short)((char *)NLMSG_TAIL(n) - (char *)nest);
}
#endif

static int
if_disable_autolinklocal(struct dhcpcd_ctx *ctx, unsigned int ifindex)
{
#ifdef HAVE_IN6_ADDR_GEN_MODE_NONE
	struct nlml nlm;
	struct rtattr *afs, *afs6;

	memset(&nlm, 0, sizeof(nlm));
	nlm.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlm.hdr.nlmsg_type = RTM_NEWLINK;
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST;
	nlm.i.ifi_family = AF_INET6;
	nlm.i.ifi_index = (int)ifindex;
	afs = add_attr_nest(&nlm.hdr, sizeof(nlm), IFLA_AF_SPEC);
	afs6 = add_attr_nest(&nlm.hdr, sizeof(nlm), AF_INET6);
	add_attr_8(&nlm.hdr, sizeof(nlm), IFLA_INET6_ADDR_GEN_MODE,
	    IN6_ADDR_GEN_MODE_NONE);
	add_attr_nest_end(&nlm.hdr, afs6);
	add_attr_nest_end(&nlm.hdr, afs);

	return if_sendnetlink(ctx, NETLINK_ROUTE, &nlm.hdr, NULL, NULL);
#else
	UNUSED(ctx);
	UNUSED(ifindex);
	errno = ENOTSUP;
	return -1;
#endif
}

static const char *p_conf = "/proc/sys/net/ipv6/conf";
static const char *p_neigh = "/proc/sys/net/ipv6/neigh";

void
if_setup_inet6(const struct interface *ifp)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;
	int ra;
	char path[256];

	/* Modern linux kernels can make a stable private address.
	 * However, a lot of distros ship newer kernel headers than
	 * the kernel itself so we sweep that error under the table
	 * from old kernels and just make them ourself regardless. */
	if (if_disable_autolinklocal(ctx, ifp->index) == -1 &&
	    errno != ENODEV && errno != ENOTSUP && errno != EINVAL)
		logdebug("%s: if_disable_autolinklocal", ifp->name);

	/* If not doing autoconf, don't disable the kernel from doing it.
	 * If we need to, we should have another option actively disable it. */
	if (!(ifp->options->options & DHCPCD_IPV6RS))
		return;

	snprintf(path, sizeof(path), "%s/%s/autoconf", p_conf, ifp->name);
	ra = check_proc_int(ctx, path);
	if (ra == -1) {
		/* The sysctl probably doesn't exist, but this isn't an
		 * error as such so just log it and continue */
		if (errno != ENOENT)
			logerr("%s: %s", __func__, path);
	} else if (ra != 0) {
		if (if_writepathuint(ctx, path, 0) == -1)
			logerr("%s: %s", __func__, path);
	}

	snprintf(path, sizeof(path), "%s/%s/accept_ra", p_conf, ifp->name);
	ra = check_proc_int(ctx, path);
	if (ra == -1) {
		/* The sysctl probably doesn't exist, but this isn't an
		 * error as such so just log it and continue */
		if (errno != ENOENT)
			logerr("%s: %s", __func__, path);
	} else if (ra != 0) {
		if (if_writepathuint(ctx, path, 0) == -1)
			logerr("%s: %s", __func__, path);
	}
}

int
if_applyra(const struct ra *rap)
{
	char path[256];
	const char *ifname = rap->iface->name;
	struct dhcpcd_ctx *ctx = rap->iface->ctx;
	int error = 0;

	if (rap->hoplimit != 0) {
		snprintf(path, sizeof(path), "%s/%s/hop_limit", p_conf, ifname);
		if (if_writepathuint(ctx, path, rap->hoplimit) == -1)
			error = -1;
	}

	if (rap->retrans != 0) {
		snprintf(path, sizeof(path), "%s/%s/retrans_time_ms",
		    p_neigh, ifname);
		if (if_writepathuint(ctx, path, rap->retrans) == -1)
			error = -1;
	}

	if (rap->reachable != 0) {
		snprintf(path, sizeof(path), "%s/%s/base_reachable_time_ms",
		    p_neigh, ifname);
		if (if_writepathuint(ctx, path, rap->reachable) == -1)
			error = -1;
	}

	return error;
}

int
ip6_forwarding(const char *ifname)
{
	char path[256], buf[64];
	int error, i;

	if (ifname == NULL)
		ifname = "all";
	snprintf(path, sizeof(path), "%s/%s/forwarding", p_conf, ifname);
	if (readfile(path, buf, sizeof(buf)) == -1)
		return 0;
	i = (int)strtoi(buf, NULL, 0, INT_MIN, INT_MAX, &error);
	if (error != 0 && error != ENOTSUP)
		return 0;
	return i;
}

#endif /* INET6 */
