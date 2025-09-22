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

#ifndef IPV6ND_H
#define IPV6ND_H

#ifdef INET6

#include <time.h>

#include "config.h"
#include "dhcpcd.h"
#include "ipv6.h"

/* rfc4191 */
struct routeinfo {
	TAILQ_ENTRY(routeinfo) next;
	struct in6_addr prefix;
	uint8_t prefix_len;
	uint32_t lifetime;
	uint8_t flags;
	struct timespec acquired;
	char sprefix[INET6_ADDRSTRLEN];
};

TAILQ_HEAD(routeinfohead, routeinfo);


struct ra {
	TAILQ_ENTRY(ra) next;
	struct interface *iface;
	struct in6_addr from;
	char sfrom[INET6_ADDRSTRLEN];
	uint8_t *data;
	size_t data_len;
	struct timespec acquired;
	uint8_t flags;
	uint32_t lifetime;
	uint32_t reachable;
	uint32_t retrans;
	uint32_t mtu;
	uint8_t hoplimit;
	struct ipv6_addrhead addrs;
	struct routeinfohead rinfos;
	bool hasdns;
	bool expired;
	bool willexpire;
	bool doexpire;
	bool isreachable;
};

TAILQ_HEAD(ra_head, ra);

struct rs_state {
	struct nd_router_solicit *rs;
	size_t rslen;
	int rsprobes;
	uint32_t retrans;
#ifdef __sun
	int nd_fd;
#endif
};

#define	RS_STATE(a) ((struct rs_state *)(ifp)->if_data[IF_DATA_IPV6ND])
#define	RS_CSTATE(a) ((const struct rs_state *)(ifp)->if_data[IF_DATA_IPV6ND])
#define	RS_STATE_RUNNING(a) (ipv6nd_hasra((a)) && ipv6nd_dadcompleted((a)))

#ifndef MAX_RTR_SOLICITATION_DELAY
#define	MAX_RTR_SOLICITATION_DELAY	1	/* seconds */
#define	MAX_UNICAST_SOLICIT		3	/* 3 transmissions */
#define	RTR_SOLICITATION_INTERVAL	4	/* seconds */
#define	MAX_RTR_SOLICITATIONS		3	/* times */
#define	MAX_NEIGHBOR_ADVERTISEMENT	3	/* 3 transmissions */

#ifndef IPV6_DEFHLIM
#define	IPV6_DEFHLIM			64
#endif
#endif

/* On carrier up, expire known routers after RTR_CARRIER_EXPIRE seconds. */
#define RTR_CARRIER_EXPIRE		\
    (MAX_RTR_SOLICITATION_DELAY +	\
    (MAX_RTR_SOLICITATIONS + 1) *	\
    RTR_SOLICITATION_INTERVAL)

#define	MAX_REACHABLE_TIME		3600000	/* milliseconds */
#define	REACHABLE_TIME			30000	/* milliseconds */
#define	RETRANS_TIMER			1000	/* milliseconds */
#define	DELAY_FIRST_PROBE_TIME		5	/* seconds */

#define	MIN_EXTENDED_VLTIME		7200	/* seconds */

int ipv6nd_open(bool);
#ifdef __sun
int ipv6nd_openif(struct interface *);
#endif
void ipv6nd_recvmsg(struct dhcpcd_ctx *, struct msghdr *);
int ipv6nd_rtpref(uint8_t);
void ipv6nd_printoptions(const struct dhcpcd_ctx *,
    const struct dhcp_opt *, size_t);
void ipv6nd_startrs(struct interface *);
ssize_t ipv6nd_env(FILE *, const struct interface *);
const struct ipv6_addr *ipv6nd_iffindaddr(const struct interface *ifp,
    const struct in6_addr *addr, unsigned int flags);
struct ipv6_addr *ipv6nd_findaddr(struct dhcpcd_ctx *,
    const struct in6_addr *, unsigned int);
struct ipv6_addr *ipv6nd_iffindprefix(struct interface *,
    const struct in6_addr *, uint8_t);
ssize_t ipv6nd_free(struct interface *);
void ipv6nd_expirera(void *arg);
bool ipv6nd_hasralifetime(const struct interface *, bool);
#define	ipv6nd_hasra(i)		ipv6nd_hasralifetime((i), false)
bool ipv6nd_hasradhcp(const struct interface *, bool);
void ipv6nd_handleifa(int, struct ipv6_addr *, pid_t);
int ipv6nd_dadcompleted(const struct interface *);
void ipv6nd_advertise(struct ipv6_addr *);
void ipv6nd_startexpire(struct interface *);
void ipv6nd_drop(struct interface *);
void ipv6nd_neighbour(struct dhcpcd_ctx *, struct in6_addr *, bool);
#endif /* INET6 */

#endif /* IPV6ND_H */
