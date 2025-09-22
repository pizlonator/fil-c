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

#ifndef ARP_H
#define ARP_H

/* ARP timings from RFC5227 */
#define PROBE_WAIT		 1
#define PROBE_NUM		 3
#define PROBE_MIN		 1
#define PROBE_MAX		 2
#define ANNOUNCE_WAIT		 2
#define ANNOUNCE_NUM		 2
#define ANNOUNCE_INTERVAL	 2
#define MAX_CONFLICTS		10
#define RATE_LIMIT_INTERVAL	60
#define DEFEND_INTERVAL		10

#include "bpf.h"
#include "dhcpcd.h"
#include "if.h"

#ifdef IN_IFF_DUPLICATED
/* NetBSD gained RFC 5227 support in the kernel.
 * This means dhcpcd doesn't need ARP except for ARPing support
 * and ARP announcing an address. */
#if defined(__NetBSD_Version__) && __NetBSD_Version__ >= 799003900
#define KERNEL_RFC5227
#endif
#endif

struct arp_msg {
	uint16_t op;
	uint8_t sha[HWADDR_LEN];
	struct in_addr sip;
	uint8_t tha[HWADDR_LEN];
	struct in_addr tip;
	/* Frame header and sender to diagnose failures */
	uint8_t fsha[HWADDR_LEN];
	uint8_t ftha[HWADDR_LEN];
};

struct arp_state {
	TAILQ_ENTRY(arp_state) next;
	struct interface *iface;
	struct in_addr addr;
	struct bpf *bpf;

	int probes;
	int claims;
	struct timespec defend;

	void (*found_cb)(struct arp_state *, const struct arp_msg *);
	void (*not_found_cb)(struct arp_state *);
	void (*announced_cb)(struct arp_state *);
	void (*defend_failed_cb)(struct arp_state *);
	void (*free_cb)(struct arp_state *);
};
TAILQ_HEAD(arp_statehead, arp_state);

struct iarp_state {
	struct arp_statehead arp_states;
};

#define ARP_STATE(ifp)							       \
	((struct iarp_state *)(ifp)->if_data[IF_DATA_ARP])
#define ARP_CSTATE(ifp)							       \
	((const struct iarp_state *)(ifp)->if_data[IF_DATA_ARP])

#ifdef ARP
void arp_packet(struct interface *, uint8_t *, size_t, unsigned int);
struct arp_state *arp_new(struct interface *, const struct in_addr *);
void arp_probe(struct arp_state *);
struct arp_state *arp_announceaddr(struct dhcpcd_ctx *, const struct in_addr *);
struct arp_state *arp_ifannounceaddr(struct interface *, const struct in_addr *);
struct arp_state * arp_find(struct interface *, const struct in_addr *);
void arp_free(struct arp_state *);
void arp_freeaddr(struct interface *, const struct in_addr *);
void arp_drop(struct interface *);
#endif /* ARP */
#endif /* ARP_H */
