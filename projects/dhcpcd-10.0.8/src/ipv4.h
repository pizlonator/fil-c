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

#ifndef IPV4_H
#define IPV4_H

#include "dhcpcd.h"

/* Prefer our macro */
#ifdef HTONL
#undef HTONL
#endif

#ifndef BYTE_ORDER
#define	BIG_ENDIAN	1234
#define	LITTLE_ENDIAN	4321
#if defined(_BIG_ENDIAN)
#define	BYTE_ORDER	BIG_ENDIAN
#elif defined(_LITTLE_ENDIAN)
#define	BYTE_ORDER	LITTLE_ENDIAN
#else
#error Endian unknown
#endif
#endif

#if BYTE_ORDER == BIG_ENDIAN
#define HTONL(A) (A)
#elif BYTE_ORDER == LITTLE_ENDIAN
#define HTONL(A) \
    ((((uint32_t)(A) & 0xff000000) >> 24) | \
    (((uint32_t)(A) & 0x00ff0000) >> 8) | \
    (((uint32_t)(A) & 0x0000ff00) << 8) | \
    (((uint32_t)(A) & 0x000000ff) << 24))
#endif /* BYTE_ORDER */

#ifdef __sun
   /* Solaris lacks these defines.
    * While it supports DaD, to seems to only expose IFF_DUPLICATE
    * so we have no way of knowing if it's tentative or not.
    * I don't even know if Solaris has any special treatment for tentative. */
#  define IN_IFF_TENTATIVE	0x01
#  define IN_IFF_DUPLICATED	0x02
#  define IN_IFF_DETACHED	0x00
#endif

#ifdef IN_IFF_TENTATIVE
#define IN_IFF_NOTUSEABLE \
        (IN_IFF_TENTATIVE | IN_IFF_DUPLICATED | IN_IFF_DETACHED)
#endif

#define IN_ARE_ADDR_EQUAL(a, b)		((a)->s_addr == (b)->s_addr)
#define IN_IS_ADDR_UNSPECIFIED(a)	((a)->s_addr == INADDR_ANY)

#ifdef __linux__
#define IP_LIFETIME
#endif

struct ipv4_addr {
	TAILQ_ENTRY(ipv4_addr) next;
	struct in_addr addr;
	struct in_addr mask;
	struct in_addr brd;
	struct interface *iface;
	int addr_flags;
	unsigned int flags;
#ifdef IP_LIFETIME
	uint32_t vltime;
	uint32_t pltime;
#endif
	char saddr[INET_ADDRSTRLEN + 3];
#ifdef ALIAS_ADDR
	char alias[IF_NAMESIZE];
#endif
};
TAILQ_HEAD(ipv4_addrhead, ipv4_addr);

#define	IPV4_AF_STALE		(1U << 0)
#define	IPV4_AF_NEW		(1U << 1)

#define	IPV4_ADDR_EQ(a1, a2)	((a1) && (a1)->addr.s_addr == (a2)->addr.s_addr)
#define	IPV4_MASK1_EQ(a1, a2)	((a1) && (a1)->mask.s_addr == (a2)->mask.s_addr)
#define	IPV4_MASK_EQ(a1, a2)	(IPV4_ADDR_EQ(a1, a2) && IPV4_MASK1_EQ(a1, a2))
#define	IPV4_BRD1_EQ(a1, a2)	((a1) && (a1)->brd.s_addr == (a2)->brd.s_addr)
#define	IPV4_BRD_EQ(a1, a2)	(IPV4_MASK_EQ(a1, a2) && IPV4_BRD1_EQ(a1, a2))

struct ipv4_state {
	struct ipv4_addrhead addrs;
};

#define IPV4_STATE(ifp)							       \
	((struct ipv4_state *)(ifp)->if_data[IF_DATA_IPV4])
#define IPV4_CSTATE(ifp)						       \
	((const struct ipv4_state *)(ifp)->if_data[IF_DATA_IPV4])

#ifdef INET
struct ipv4_state *ipv4_getstate(struct interface *);
int ipv4_ifcmp(const struct interface *, const struct interface *);
uint8_t inet_ntocidr(struct in_addr);
int inet_cidrtoaddr(int, struct in_addr *);
uint32_t ipv4_getnetmask(uint32_t);
int ipv4_hasaddr(const struct interface *);

bool inet_getroutes(struct dhcpcd_ctx *, rb_tree_t *);

#define STATE_ADDED		0x01
#define STATE_FAKE		0x02
#define STATE_EXPIRED		0x04

int ipv4_deladdr(struct ipv4_addr *, int);
struct ipv4_addr *ipv4_addaddr(struct interface *,
    const struct in_addr *, const struct in_addr *, const struct in_addr *,
    uint32_t, uint32_t);
struct ipv4_addr *ipv4_applyaddr(void *);

struct ipv4_addr *ipv4_iffindaddr(struct interface *,
    const struct in_addr *, const struct in_addr *);
struct ipv4_addr *ipv4_iffindlladdr(struct interface *);
struct ipv4_addr *ipv4_findaddr(struct dhcpcd_ctx *, const struct in_addr *);
struct ipv4_addr *ipv4_findmaskaddr(struct dhcpcd_ctx *,
    const struct in_addr *);
struct ipv4_addr *ipv4_findmaskbrd(struct dhcpcd_ctx *,
    const struct in_addr *);
void ipv4_markaddrsstale(struct interface *);
void ipv4_deletestaleaddrs(struct interface *);
void ipv4_handleifa(struct dhcpcd_ctx *, int, struct if_head *, const char *,
    const struct in_addr *, const struct in_addr *, const struct in_addr *,
    int, pid_t);

void ipv4_free(struct interface *);
#endif /* INET */

#endif /* IPV4_H */
