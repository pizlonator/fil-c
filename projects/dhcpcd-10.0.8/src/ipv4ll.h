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

#ifndef IPV4LL_H
#define IPV4LL_H

#define	LINKLOCAL_ADDR		0xa9fe0000
#define	LINKLOCAL_MASK		IN_CLASSB_NET
#define	LINKLOCAL_BCAST		(LINKLOCAL_ADDR | ~LINKLOCAL_MASK)

#ifndef IN_LINKLOCAL
# define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == LINKLOCAL_ADDR)
#endif

#ifdef IPV4LL
#include "arp.h"

struct ipv4ll_state {
	struct in_addr pickedaddr;
	struct ipv4_addr *addr;
	char randomstate[128];
	bool running;
	bool seeded;
	bool down;
	size_t conflicts;
#ifndef KERNEL_RFC5227
	struct arp_state *arp;
#endif
};

#define	IPV4LL_STATE(ifp)						       \
	((struct ipv4ll_state *)(ifp)->if_data[IF_DATA_IPV4LL])
#define	IPV4LL_CSTATE(ifp)						       \
	((const struct ipv4ll_state *)(ifp)->if_data[IF_DATA_IPV4LL])
#define	IPV4LL_STATE_RUNNING(ifp)					       \
	(IPV4LL_CSTATE((ifp)) && !IPV4LL_CSTATE((ifp))->down &&		       \
	(IPV4LL_CSTATE((ifp))->addr != NULL))

int ipv4ll_subnetroute(rb_tree_t *, struct interface *);
int ipv4ll_defaultroute(rb_tree_t *,struct interface *);
ssize_t ipv4ll_env(FILE *, const char *, const struct interface *);
void ipv4ll_start(void *);
void ipv4ll_claimed(void *);
void ipv4ll_handle_failure(void *);
struct ipv4_addr *ipv4ll_handleifa(int, struct ipv4_addr *, pid_t pid);
#ifdef HAVE_ROUTE_METRIC
int ipv4ll_recvrt(int, const struct rt *);
#endif

void ipv4ll_reset(struct interface *);
void ipv4ll_drop(struct interface *);
void ipv4ll_free(struct interface *);
#endif

#endif
