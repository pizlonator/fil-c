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

#ifndef INTERFACE_H
#define INTERFACE_H

#include <net/if.h>
#include <net/route.h>		/* for RTM_ADD et all */
#include <netinet/in.h>
#ifdef BSD
#include <netinet/in_var.h>	/* for IN_IFF_TENTATIVE et all */
#endif

#include <ifaddrs.h>

/* If the interface does not support carrier status (ie PPP),
 * dhcpcd can poll it for the relevant flags periodically */
#define IF_POLL_UP	100	/* milliseconds */

/*
 * Systems which handle 1 address per alias.
 * Currenly this is just Solaris.
 * While Linux can do aliased addresses, it is only useful for their
 * legacy ifconfig(8) tool which cannot display >1 IPv4 address
 * (it can display many IPv6 addresses which makes the limitation odd).
 * Linux has ip(8) which is a more feature rich tool, without the above
 * restriction.
 */
#ifndef ALIAS_ADDR
#  ifdef __sun
#    define ALIAS_ADDR
#  endif
#endif

#include "config.h"

/* POSIX defines ioctl request as an int, which Solaris and musl use.
 * Everyone else use an unsigned long, which happens to be the bigger one
 * so we use that in our on wire API. */
#ifdef IOCTL_REQUEST_TYPE
typedef IOCTL_REQUEST_TYPE	ioctl_request_t;
#else
typedef unsigned long		ioctl_request_t;
#endif

#include "dhcpcd.h"
#include "ipv4.h"
#include "ipv6.h"
#include "route.h"

#define EUI64_ADDR_LEN			8
#define INFINIBAND_ADDR_LEN		20

/* Linux 2.4 doesn't define this */
#ifndef ARPHRD_IEEE1394
#  define ARPHRD_IEEE1394		24
#endif

/* The BSD's don't define this yet */
#ifndef ARPHRD_INFINIBAND
#  define ARPHRD_INFINIBAND		32
#endif

/* Maximum frame length.
 * Support jumbo frames and some extra. */
#define	FRAMEHDRLEN_MAX			14	/* only ethernet support */
#define	FRAMELEN_MAX			(FRAMEHDRLEN_MAX + 9216)

#define UDPLEN_MAX			64 * 1024

/* Work out if we have a private address or not
 * 10/8
 * 172.16/12
 * 192.168/16
 */
#ifndef IN_PRIVATE
# define IN_PRIVATE(addr) (((addr & IN_CLASSA_NET) == 0x0a000000) ||	      \
	    ((addr & 0xfff00000)    == 0xac100000) ||			      \
	    ((addr & IN_CLASSB_NET) == 0xc0a80000))
#endif

#ifndef CLLADDR
#ifdef AF_LINK
#  define CLLADDR(sdl) (const void *)((sdl)->sdl_data + (sdl)->sdl_nlen)
#endif
#endif

#ifdef __sun
/* Solaris stupidly defines this for compat with BSD
 * but then ignores it. */
#undef RTF_CLONING

/* This interface is busted on DilOS at least.
 * It used to work, but lukily Solaris can fall back to
 * IP_PKTINFO. */
#undef IP_RECVIF
#endif

/* Private structures specific to an OS */
#ifdef BSD
struct priv {
#ifdef INET6
	int pf_inet6_fd;
#endif
#if defined(SIOCALIFADDR) && defined(IFLR_ACTIVE) /*NetBSD */
	int pf_link_fd;
#endif
};
#endif
#ifdef __linux__
struct priv {
	int route_fd;
	int generic_fd;
	uint32_t route_pid;
};
#endif
#ifdef __sun
struct priv {
#ifdef INET6
	int pf_inet6_fd;
#endif
};
#endif

#ifdef __sun
/* Solaris getifaddrs is very un-suitable for dhcpcd.
 * See if-sun.c for details why. */
struct ifaddrs;
int if_getifaddrs(struct ifaddrs **);
#define	getifaddrs	if_getifaddrs
int if_getsubnet(struct dhcpcd_ctx *, const char *, int, void *, size_t);
#endif

int if_ioctl(struct dhcpcd_ctx *, ioctl_request_t, void *, size_t);
#ifdef HAVE_PLEDGE
#define	pioctl(ctx, req, data, len) if_ioctl((ctx), (req), (data), (len))
#else
#define	pioctl(ctx, req, data, len) ioctl((ctx)->pf_inet_fd, (req),(data),(len))
#endif
int if_getflags(struct interface *);
int if_setflag(struct interface *, short, short);
#define if_up(ifp) if_setflag((ifp), (IFF_UP | IFF_RUNNING), 0)
#define if_down(ifp) if_setflag((ifp), 0, IFF_UP);
bool if_is_link_up(const struct interface *);
bool if_valid_hwaddr(const uint8_t *, size_t);
struct if_head *if_discover(struct dhcpcd_ctx *, struct ifaddrs **,
    int, char * const *);
void if_freeifaddrs(struct dhcpcd_ctx *ctx, struct ifaddrs **);
void if_markaddrsstale(struct if_head *);
void if_learnaddrs(struct dhcpcd_ctx *, struct if_head *, struct ifaddrs **);
void if_deletestaleaddrs(struct if_head *);
struct interface *if_find(struct if_head *, const char *);
struct interface *if_findindex(struct if_head *, unsigned int);
struct interface *if_loopback(struct dhcpcd_ctx *);
void if_free(struct interface *);
int if_domtu(const struct interface *, short int);
#define if_getmtu(ifp) if_domtu((ifp), 0)
#define if_setmtu(ifp, mtu) if_domtu((ifp), (mtu))
int if_carrier(struct interface *, const void *);
bool if_roaming(struct interface *);

#ifdef ALIAS_ADDR
int if_makealias(char *, size_t, const char *, int);
#endif

int if_mtu_os(const struct interface *);

/*
 * Helper to decode an interface name of bge0:1 to
 * devname = bge0, drvname = bge0, ppa = 0, lun = 1.
 * If ppa or lun are invalid they are set to -1.
 */
struct if_spec {
	char ifname[IF_NAMESIZE];
	char devname[IF_NAMESIZE];
	char drvname[IF_NAMESIZE];
	int ppa;
	int vlid;
	int lun;
};
int if_nametospec(const char *, struct if_spec *);

/* The below functions are provided by if-KERNEL.c */
int os_init(void);
int if_conf(struct interface *);
int if_init(struct interface *);
int if_getssid(struct interface *);
int if_ignoregroup(int, const char *);
bool if_ignore(struct dhcpcd_ctx *, const char *);
int if_vimaster(struct dhcpcd_ctx *ctx, const char *);
unsigned short if_vlanid(const struct interface *);
char * if_getnetworknamespace(char *, size_t);
int if_opensockets(struct dhcpcd_ctx *);
int if_opensockets_os(struct dhcpcd_ctx *);
void if_closesockets(struct dhcpcd_ctx *);
void if_closesockets_os(struct dhcpcd_ctx *);
int if_handlelink(struct dhcpcd_ctx *);
int if_randomisemac(struct interface *);
int if_setmac(struct interface *ifp, void *, uint8_t);

/* dhcpcd uses the same routing flags as BSD.
 * If the platform doesn't use these flags,
 * map them in the platform interace file. */
#ifndef RTM_ADD
#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#endif

/* Define SOCK_CLOEXEC and SOCK_NONBLOCK for systems that lack it.
 * xsocket() in if.c will map them to fctnl FD_CLOEXEC and O_NONBLOCK. */
#ifdef SOCK_CLOEXEC
# define HAVE_SOCK_CLOEXEC
#else
# define SOCK_CLOEXEC	0x10000000
#endif
#ifdef SOCK_NONBLOCK
# define HAVE_SOCK_NONBLOCK
#else
# define SOCK_NONBLOCK	0x20000000
#endif
#ifndef SOCK_CXNB
#define	SOCK_CXNB	SOCK_CLOEXEC | SOCK_NONBLOCK
#endif
int xsocket(int, int, int);
int xsocketpair(int, int, int, int[2]);

int if_route(unsigned char, const struct rt *rt);
int if_initrt(struct dhcpcd_ctx *, rb_tree_t *, int);

int if_missfilter(struct interface *, struct sockaddr *);
int if_missfilter_apply(struct dhcpcd_ctx *);

#ifdef INET
int if_address(unsigned char, const struct ipv4_addr *);
int if_addrflags(const struct interface *, const struct in_addr *,
    const char *);

#endif

#ifdef INET6
void if_disable_rtadv(void);
void if_setup_inet6(const struct interface *);
int ip6_forwarding(const char *ifname);

struct ra;
struct ipv6_addr;

int if_applyra(const struct ra *);
int if_address6(unsigned char, const struct ipv6_addr *);
int if_addrflags6(const struct interface *, const struct in6_addr *,
    const char *);
int if_getlifetime6(struct ipv6_addr *);

#else
#define if_checkipv6(a, b, c) (-1)
#endif

int if_machinearch(char *, size_t);
struct interface *if_findifpfromcmsg(struct dhcpcd_ctx *,
    struct msghdr *, int *);

#ifdef __linux__
int if_linksocket(struct sockaddr_nl *, int, int);
int if_getnetlink(struct dhcpcd_ctx *, struct iovec *, int, int,
    int (*)(struct dhcpcd_ctx *, void *, struct nlmsghdr *), void *);
#endif
#endif
