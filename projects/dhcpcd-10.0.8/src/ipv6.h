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

#ifndef IPV6_H
#define IPV6_H

#include <sys/uio.h>
#include <netinet/in.h>

#include "config.h"
#include "if.h"

#ifndef __linux__
#  if !defined(__QNX__) && !defined(__sun)
#    include <sys/endian.h>
#  endif
#  include <net/if.h>
#  ifndef __sun
#    include <netinet6/in6_var.h>
#  endif
#endif

#define EUI64_GBIT		0x01
#define EUI64_UBIT		0x02
#define EUI64_TO_IFID(in6)	do {(in6)->s6_addr[8] ^= EUI64_UBIT; } while (0)
#define EUI64_GROUP(in6)	((in6)->s6_addr[8] & EUI64_GBIT)

#ifndef ND6_INFINITE_LIFETIME
#  define ND6_INFINITE_LIFETIME		((uint32_t)~0)
#endif

/* RFC4941 constants */
#define TEMP_VALID_LIFETIME	604800	/* 1 week */
#define TEMP_PREFERRED_LIFETIME	86400	/* 1 day */
#define REGEN_ADVANCE		5	/* seconds */
#define MAX_DESYNC_FACTOR	600	/* 10 minutes */
#define TEMP_IDGEN_RETRIES	3

/* RFC7217 constants */
#define IDGEN_RETRIES	3
#define IDGEN_DELAY	1 /* second */

/* Interface identifier length. Prefix + this == 128 for autoconf */
#define ipv6_ifidlen(ifp)	64
#define	IA6_CANAUTOCONF(ia)	\
	((ia)->prefix_len + ipv6_ifidlen((ia)->iface) == 128)

#ifndef IN6_ARE_MASKED_ADDR_EQUAL
#define IN6_ARE_MASKED_ADDR_EQUAL(d, a, m)	(	\
	(((d)->s6_addr32[0] ^ (a)->s6_addr32[0]) & (m)->s6_addr32[0]) == 0 && \
	(((d)->s6_addr32[1] ^ (a)->s6_addr32[1]) & (m)->s6_addr32[1]) == 0 && \
	(((d)->s6_addr32[2] ^ (a)->s6_addr32[2]) & (m)->s6_addr32[2]) == 0 && \
	(((d)->s6_addr32[3] ^ (a)->s6_addr32[3]) & (m)->s6_addr32[3]) == 0 )
#endif

#ifndef IN6ADDR_LINKLOCAL_ALLNODES_INIT
#define	IN6ADDR_LINKLOCAL_ALLNODES_INIT				\
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}}
#endif
#ifndef IN6ADDR_LINKLOCAL_ALLROUTERS_INIT
#define	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT			\
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }}}
#endif

/*
 * BSD kernels don't inform userland of DAD results.
 * See the discussion here:
 *    http://mail-index.netbsd.org/tech-net/2013/03/15/msg004019.html
 */
#ifndef __linux__
/* We guard here to avoid breaking a compile on linux ppc-64 headers */
#  include <sys/param.h>
#endif
#ifdef BSD
#  define IPV6_POLLADDRFLAG
#endif

/* This was fixed in NetBSD */
#if (defined(__DragonFly_version) && __DragonFly_version >= 500704) || \
    (defined(__NetBSD_Version__) && __NetBSD_Version__ >= 699002000)
#  undef IPV6_POLLADDRFLAG
#endif

/* Of course OpenBSD has their own special name. */
#if !defined(IN6_IFF_TEMPORARY) && defined(IN6_IFF_PRIVACY)
#define	IN6_IFF_TEMPORARY IN6_IFF_PRIVACY
#endif

#ifdef __sun
   /* Solaris lacks these defines.
    * While it supports DaD, to seems to only expose IFF_DUPLICATE
    * so we have no way of knowing if it's tentative or not.
    * I don't even know if Solaris has any special treatment for tentative. */
#  define IN6_IFF_TENTATIVE	0x02
#  define IN6_IFF_DUPLICATED	0x04
#  define IN6_IFF_DETACHED	0x00
#endif

#define IN6_IFF_NOTUSEABLE \
	(IN6_IFF_TENTATIVE | IN6_IFF_DUPLICATED | IN6_IFF_DETACHED)

/*
 * If dhcpcd handles RA processing instead of the kernel, the kernel needs
 * to either allow userland to set temporary addresses or mark an address
 * for the kernel to manage temporary addresses from.
 * If the kernel allows the former, a global #define is needed, otherwise
 * the address marking will be handled in the platform specific address handler.
 *
 * Some BSDs do not allow userland to set temporary addresses.
 * Linux-3.18 allows the marking of addresses from which to manage temp addrs.
 */
#if defined(IN6_IFF_TEMPORARY) && !defined(__linux__)
#define	IPV6_MANAGETEMPADDR
#endif

#ifdef __linux__
   /* Match Linux defines to BSD */
#  ifdef IFA_F_TEMPORARY
#    define IN6_IFF_TEMPORARY	IFA_F_TEMPORARY
#  endif
#  ifdef IFA_F_OPTIMISTIC
#    define IN6_IFF_TENTATIVE	(IFA_F_TENTATIVE | IFA_F_OPTIMISTIC)
#  else
#    define IN6_IFF_TENTATIVE   (IFA_F_TENTATIVE | 0x04)
#  endif
#  ifdef IF_F_DADFAILED
#    define IN6_IFF_DUPLICATED	IFA_F_DADFAILED
#  else
#    define IN6_IFF_DUPLICATED	0x08
#  endif
#  define IN6_IFF_DETACHED	0
#endif

/*
 * ND6 Advertising is only used for IP address sharing to prefer
 * the address on a specific interface.
 * This just fails to work on OpenBSD and causes erroneous duplicate
 * address messages on BSD's other then DragonFly and NetBSD.
 */
#if !defined(SMALL) && \
    ((defined(__DragonFly_version) && __DragonFly_version >= 500703) || \
    (defined(__NetBSD_Version__) && __NetBSD_Version__ >= 899002800) || \
    defined(__linux__) || defined(__sun))
#  define ND6_ADVERTISE
#endif

#ifdef INET6
TAILQ_HEAD(ipv6_addrhead, ipv6_addr);
struct ipv6_addr {
	TAILQ_ENTRY(ipv6_addr) next;
	struct interface *iface;
	struct in6_addr prefix;
	uint8_t prefix_len;
	uint32_t prefix_vltime;
	uint32_t prefix_pltime;
	struct timespec created;
	struct timespec acquired;
	struct in6_addr addr;
	int addr_flags;
	unsigned int flags;
	char saddr[INET6_ADDRSTRLEN];
	uint8_t iaid[4];
	uint16_t ia_type;
	int dhcp6_fd;

#ifndef SMALL
	struct ipv6_addr *delegating_prefix;
	struct ipv6_addrhead pd_pfxs;
	TAILQ_ENTRY(ipv6_addr) pd_next;

	uint8_t prefix_exclude_len;
	struct in6_addr prefix_exclude;
#endif

	void (*dadcallback)(void *);
	int dadcounter;

	struct nd_neighbor_advert *na;
	size_t na_len;
	int na_count;

#ifdef ALIAS_ADDR
	char alias[IF_NAMESIZE];
#endif
};

#define	IPV6_AF_ONLINK		(1U << 0)
#define	IPV6_AF_NEW		(1U << 1)
#define	IPV6_AF_STALE		(1U << 2)
#define	IPV6_AF_ADDED		(1U << 3)
#define	IPV6_AF_AUTOCONF	(1U << 4)
#define	IPV6_AF_DADCOMPLETED	(1U << 5)
#define	IPV6_AF_DELEGATED	(1U << 6)
#define	IPV6_AF_DELEGATEDPFX	(1U << 7)
#define	IPV6_AF_NOREJECT	(1U << 8)
#define	IPV6_AF_REQUEST		(1U << 9)
#define	IPV6_AF_STATIC		(1U << 10)
#define	IPV6_AF_DELEGATEDLOG	(1U << 11)
#define	IPV6_AF_RAPFX		(1U << 12)
#define	IPV6_AF_EXTENDED	(1U << 13)
#define	IPV6_AF_REGEN		(1U << 14)
#define	IPV6_AF_ROUTER		(1U << 15)
#ifdef IPV6_MANAGETEMPADDR
#define	IPV6_AF_TEMPORARY	(1U << 16)
#endif

struct ll_callback {
	TAILQ_ENTRY(ll_callback) next;
	void (*callback)(void *);
	void *arg;
};
TAILQ_HEAD(ll_callback_head, ll_callback);

struct ipv6_state {
	struct ipv6_addrhead addrs;
	struct ll_callback_head ll_callbacks;

#ifdef IPV6_MANAGETEMPADDR
	uint32_t desync_factor;
#endif
};

#define IPV6_STATE(ifp)							       \
	((struct ipv6_state *)(ifp)->if_data[IF_DATA_IPV6])
#define IPV6_CSTATE(ifp)						       \
	((const struct ipv6_state *)(ifp)->if_data[IF_DATA_IPV6])
#define IPV6_STATE_RUNNING(ifp) ipv6_staticdadcompleted((ifp))


int ipv6_init(struct dhcpcd_ctx *);
int ipv6_makestableprivate(struct in6_addr *,
    const struct in6_addr *, int, const struct interface *, int *);
int ipv6_makeaddr(struct in6_addr *, struct interface *,
    const struct in6_addr *, int, unsigned int);
int ipv6_mask(struct in6_addr *, int);
uint8_t ipv6_prefixlen(const struct in6_addr *);
int ipv6_userprefix( const struct in6_addr *, short prefix_len,
    uint64_t user_number, struct in6_addr *result, short result_len);
void ipv6_checkaddrflags(void *);
void ipv6_markaddrsstale(struct interface *, unsigned int);
void ipv6_deletestaleaddrs(struct interface *);
int ipv6_addaddr(struct ipv6_addr *, const struct timespec *);
int ipv6_doaddr(struct ipv6_addr *, struct timespec *);
ssize_t ipv6_addaddrs(struct ipv6_addrhead *addrs);
void ipv6_deleteaddr(struct ipv6_addr *);
void ipv6_freedrop_addrs(struct ipv6_addrhead *, int,
    const struct interface *);
void ipv6_handleifa(struct dhcpcd_ctx *ctx, int, struct if_head *,
    const char *, const struct in6_addr *, uint8_t, int, pid_t);
int ipv6_handleifa_addrs(int, struct ipv6_addrhead *, const struct ipv6_addr *,
    pid_t);
struct ipv6_addr *ipv6_iffindaddr(struct interface *,
    const struct in6_addr *, int);
int ipv6_hasaddr(const struct interface *);
struct ipv6_addr *ipv6_anyglobal(struct interface *);
int ipv6_findaddrmatch(const struct ipv6_addr *, const struct in6_addr *,
    unsigned int);
struct ipv6_addr *ipv6_findaddr(struct dhcpcd_ctx *,
    const struct in6_addr *, unsigned int);
struct ipv6_addr *ipv6_findmaskaddr(struct dhcpcd_ctx *,
    const struct in6_addr *);
#define ipv6_linklocal(ifp) ipv6_iffindaddr((ifp), NULL, IN6_IFF_NOTUSEABLE)
int ipv6_addlinklocalcallback(struct interface *, void (*)(void *), void *);
void ipv6_setscope(struct sockaddr_in6 *, unsigned int);
unsigned int ipv6_getscope(const struct sockaddr_in6 *);
struct ipv6_addr *ipv6_newaddr(struct interface *, const struct in6_addr *,
    uint8_t, unsigned int);
void ipv6_freeaddr(struct ipv6_addr *);
void ipv6_freedrop(struct interface *, int);
#define ipv6_free(ifp) ipv6_freedrop((ifp), 0)
#define ipv6_drop(ifp) ipv6_freedrop((ifp), 2)

#ifdef IPV6_MANAGETEMPADDR
struct ipv6_addr *ipv6_createtempaddr(struct ipv6_addr *,
    const struct timespec *);
struct ipv6_addr *ipv6_settemptime(struct ipv6_addr *, int);
void ipv6_addtempaddrs(struct interface *, const struct timespec *);
void ipv6_regentempaddrs(void *);
#endif

int ipv6_start(struct interface *);
int ipv6_staticdadcompleted(const struct interface *);
int ipv6_startstatic(struct interface *);
ssize_t ipv6_env(FILE *, const char *, const struct interface *);
void ipv6_ctxfree(struct dhcpcd_ctx *);
bool inet6_getroutes(struct dhcpcd_ctx *, rb_tree_t *);
#endif /* INET6 */

#endif /* INET6_H */
