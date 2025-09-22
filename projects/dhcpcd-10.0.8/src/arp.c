/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - ARP handler
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

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ELOOP_QUEUE	ELOOP_ARP
#include "config.h"
#include "arp.h"
#include "bpf.h"
#include "ipv4.h"
#include "common.h"
#include "dhcpcd.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4ll.h"
#include "logerr.h"
#include "privsep.h"

#if defined(ARP)
#define ARP_LEN								\
	(FRAMEHDRLEN_MAX +						\
	 sizeof(struct arphdr) + (2 * sizeof(uint32_t)) + (2 * HWADDR_LEN))

/* ARP debugging can be quite noisy. Enable this for more noise! */
//#define	ARP_DEBUG

/* Assert the correct structure size for on wire */
__CTASSERT(sizeof(struct arphdr) == 8);

static ssize_t
arp_request(const struct arp_state *astate,
    const struct in_addr *sip)
{
	const struct interface *ifp = astate->iface;
	const struct in_addr *tip = &astate->addr;
	uint8_t arp_buffer[ARP_LEN];
	struct arphdr ar;
	size_t len;
	uint8_t *p;

	ar.ar_hrd = htons(ifp->hwtype);
	ar.ar_pro = htons(ETHERTYPE_IP);
	ar.ar_hln = ifp->hwlen;
	ar.ar_pln = sizeof(tip->s_addr);
	ar.ar_op = htons(ARPOP_REQUEST);

	p = arp_buffer;
	len = 0;

#define CHECK(fun, b, l)						\
	do {								\
		if (len + (l) > sizeof(arp_buffer))			\
			goto eexit;					\
		fun(p, (b), (l));					\
		p += (l);						\
		len += (l);						\
	} while (/* CONSTCOND */ 0)
#define APPEND(b, l)	CHECK(memcpy, b, l)
#define ZERO(l)		CHECK(memset, 0, l)

	APPEND(&ar, sizeof(ar));
	APPEND(ifp->hwaddr, ifp->hwlen);
	if (sip != NULL)
		APPEND(&sip->s_addr, sizeof(sip->s_addr));
	else
		ZERO(sizeof(tip->s_addr));
	ZERO(ifp->hwlen);
	APPEND(&tip->s_addr, sizeof(tip->s_addr));

#ifdef PRIVSEP
	if (ifp->ctx->options & DHCPCD_PRIVSEP)
		return ps_bpf_sendarp(ifp, tip, arp_buffer, len);
#endif
	/* Note that well formed ethernet will add extra padding
	 * to ensure that the packet is at least 60 bytes (64 including FCS). */
	return bpf_send(astate->bpf, ETHERTYPE_ARP, arp_buffer, len);

eexit:
	errno = ENOBUFS;
	return -1;
}

static void
arp_report_conflicted(const struct arp_state *astate,
    const struct arp_msg *amsg)
{
	char abuf[HWADDR_LEN * 3];
	char fbuf[HWADDR_LEN * 3];

	if (amsg == NULL) {
		logerrx("%s: DAD detected %s",
		    astate->iface->name, inet_ntoa(astate->addr));
		return;
	}

	hwaddr_ntoa(amsg->sha, astate->iface->hwlen, abuf, sizeof(abuf));
	if (bpf_frame_header_len(astate->iface) == 0) {
		logwarnx("%s: %s claims %s",
		    astate->iface->name, abuf, inet_ntoa(astate->addr));
		return;
	}

	logwarnx("%s: %s(%s) claims %s",
	    astate->iface->name, abuf,
	    hwaddr_ntoa(amsg->fsha, astate->iface->hwlen, fbuf, sizeof(fbuf)),
	    inet_ntoa(astate->addr));
}

static void
arp_found(struct arp_state *astate, const struct arp_msg *amsg)
{
	struct interface *ifp;
	struct ipv4_addr *ia;
#ifndef KERNEL_RFC5227
	struct timespec now;
#endif

	arp_report_conflicted(astate, amsg);
	ifp = astate->iface;

	/* If we haven't added the address we're doing a probe. */
	ia = ipv4_iffindaddr(ifp, &astate->addr, NULL);
	if (ia == NULL) {
		if (astate->found_cb != NULL)
			astate->found_cb(astate, amsg);
		return;
	}

#ifndef KERNEL_RFC5227
	/* RFC 3927 Section 2.5 says a defence should
	 * broadcast an ARP announcement.
	 * Because the kernel will also unicast a reply to the
	 * hardware address which requested the IP address
	 * the other IPv4LL client will receieve two ARP
	 * messages.
	 * If another conflict happens within DEFEND_INTERVAL
	 * then we must drop our address and negotiate a new one.
	 * If DHCPCD_ARP_PERSISTDEFENCE is set, that enables
	 * RFC5227 section 2.4.c behaviour. Upon conflict
	 * detection, the host records the time that the
	 * conflicting ARP packet was received, and then
	 * broadcasts one single ARP Announcement. The host then
	 * continues to use the address normally. All further
	 * conflict notifications within the DEFEND_INTERVAL are
	 * ignored. */
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (timespecisset(&astate->defend) &&
	    eloop_timespec_diff(&now, &astate->defend, NULL) < DEFEND_INTERVAL)
	{
		logwarnx("%s: %d second defence failed for %s",
		    ifp->name, DEFEND_INTERVAL, inet_ntoa(astate->addr));
		if (ifp->options->options & DHCPCD_ARP_PERSISTDEFENCE)
			return;
	}
	else if (arp_request(astate, &astate->addr) == -1)
		logerr(__func__);
	else {
		logdebugx("%s: defended address %s",
		    ifp->name, inet_ntoa(astate->addr));
		astate->defend = now;
		return;
	}
#endif

	if (astate->defend_failed_cb != NULL)
		astate->defend_failed_cb(astate);
}

static bool
arp_validate(const struct interface *ifp, struct arphdr *arp)
{

	/* Address type must match */
	if (arp->ar_hrd != htons(ifp->hwtype))
		return false;

	/* Protocol must be IP. */
	if (arp->ar_pro != htons(ETHERTYPE_IP))
		return false;

	/* lladdr length matches */
	if (arp->ar_hln != ifp->hwlen)
		return false;

	/* Protocol length must match in_addr_t */
	if (arp->ar_pln != sizeof(in_addr_t))
		return false;

	/* Only these types are recognised */
	if (arp->ar_op != htons(ARPOP_REPLY) &&
	    arp->ar_op != htons(ARPOP_REQUEST))
		return false;

	return true;
}

void
arp_packet(struct interface *ifp, uint8_t *data, size_t len,
    unsigned int bpf_flags)
{
	size_t fl = bpf_frame_header_len(ifp), falen;
	const struct interface *ifn;
	struct arphdr ar;
	struct arp_msg arm;
	const struct iarp_state *state;
	struct arp_state *astate, *astaten;
	uint8_t *hw_s, *hw_t;
#ifndef KERNEL_RFC5227
	bool is_probe;
#endif /* KERNEL_RFC5227 */

	/* Copy the frame header source and destination out */
	memset(&arm, 0, sizeof(arm));
	if (fl != 0) {
		hw_s = bpf_frame_header_src(ifp, data, &falen);
		if (hw_s != NULL && falen <= sizeof(arm.fsha))
			memcpy(arm.fsha, hw_s, falen);
		hw_t = bpf_frame_header_dst(ifp, data, &falen);
		if (hw_t != NULL && falen <= sizeof(arm.ftha))
			memcpy(arm.ftha, hw_t, falen);

		/* Skip past the frame header */
		data += fl;
		len -= fl;
	}

	/* We must have a full ARP header */
	if (len < sizeof(ar))
		return;
	memcpy(&ar, data, sizeof(ar));

	if (!arp_validate(ifp, &ar)) {
#ifdef BPF_DEBUG
		logerrx("%s: ARP BPF validation failure", ifp->name);
#endif
		return;
	}

	/* Get pointers to the hardware addresses */
	hw_s = data + sizeof(ar);
	hw_t = hw_s + ar.ar_hln + ar.ar_pln;
	/* Ensure we got all the data */
	if ((size_t)((hw_t + ar.ar_hln + ar.ar_pln) - data) > len)
		return;
	/* Ignore messages from ourself */
	TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
		if (ar.ar_hln == ifn->hwlen &&
		    memcmp(hw_s, ifn->hwaddr, ifn->hwlen) == 0)
			break;
	}
	if (ifn) {
#ifdef ARP_DEBUG
		logdebugx("%s: ignoring ARP from self", ifp->name);
#endif
		return;
	}
	/* Copy out the HW and IP addresses */
	memcpy(&arm.sha, hw_s, ar.ar_hln);
	memcpy(&arm.sip.s_addr, hw_s + ar.ar_hln, ar.ar_pln);
	memcpy(&arm.tha, hw_t, ar.ar_hln);
	memcpy(&arm.tip.s_addr, hw_t + ar.ar_hln, ar.ar_pln);

#ifndef KERNEL_RFC5227
	/* During ARP probe the 'sender hardware address' MUST contain the hardware
	 * address of the interface sending the packet. RFC5227, 1.1 */
	is_probe = ar.ar_op == htons(ARPOP_REQUEST) && IN_IS_ADDR_UNSPECIFIED(&arm.sip) &&
	    bpf_flags & BPF_BCAST;
	if (is_probe && falen > 0 && (falen != ar.ar_hln ||
	    memcmp(&arm.sha, &arm.fsha, ar.ar_hln))) {
		char abuf[HWADDR_LEN * 3];
		char fbuf[HWADDR_LEN * 3];
		hwaddr_ntoa(&arm.sha, ar.ar_hln, abuf, sizeof(abuf));
		hwaddr_ntoa(&arm.fsha, falen, fbuf, sizeof(fbuf));
		logwarnx("%s: invalid ARP probe, sender hw address mismatch (%s, %s)",
		    ifp->name, abuf, fbuf);
		return;
	}
#endif /* KERNEL_RFC5227 */

	/* Match the ARP probe to our states.
	 * Ignore Unicast Poll, RFC1122. */
	state = ARP_CSTATE(ifp);
	if (state == NULL)
		return;
	TAILQ_FOREACH_SAFE(astate, &state->arp_states, next, astaten) {
		if (IN_ARE_ADDR_EQUAL(&arm.sip, &astate->addr) ||
		    (IN_IS_ADDR_UNSPECIFIED(&arm.sip) &&
		    IN_ARE_ADDR_EQUAL(&arm.tip, &astate->addr) &&
		    bpf_flags & BPF_BCAST))
			arp_found(astate, &arm);
	}
}

static void
arp_read(void *arg, unsigned short events)
{
	struct arp_state *astate = arg;
	struct bpf *bpf = astate->bpf;
	struct interface *ifp = astate->iface;
	uint8_t buf[ARP_LEN];
	ssize_t bytes;
	struct in_addr addr = astate->addr;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	/* Some RAW mechanisms are generic file descriptors, not sockets.
	 * This means we have no kernel call to just get one packet,
	 * so we have to process the entire buffer. */
	bpf->bpf_flags &= ~BPF_EOF;
	while (!(bpf->bpf_flags & BPF_EOF)) {
		bytes = bpf_read(bpf, buf, sizeof(buf));
		if (bytes == -1) {
			logerr("%s: %s", __func__, ifp->name);
			arp_free(astate);
			return;
		}
		arp_packet(ifp, buf, (size_t)bytes, bpf->bpf_flags);
		/* Check we still have a state after processing. */
		if ((astate = arp_find(ifp, &addr)) == NULL)
			break;
		if ((bpf = astate->bpf) == NULL)
			break;
	}
}

static void
arp_probed(void *arg)
{
	struct arp_state *astate = arg;

	timespecclear(&astate->defend);
	astate->not_found_cb(astate);
}

static void
arp_probe1(void *arg)
{
	struct arp_state *astate = arg;
	struct interface *ifp = astate->iface;
	unsigned int delay;

	if (++astate->probes < PROBE_NUM) {
		delay = (PROBE_MIN * MSEC_PER_SEC) +
		    (arc4random_uniform(
		    (PROBE_MAX - PROBE_MIN) * MSEC_PER_SEC));
		eloop_timeout_add_msec(ifp->ctx->eloop, delay, arp_probe1, astate);
	} else {
		delay = ANNOUNCE_WAIT *	MSEC_PER_SEC;
		eloop_timeout_add_msec(ifp->ctx->eloop, delay, arp_probed, astate);
	}
	logdebugx("%s: ARP probing %s (%d of %d), next in %0.1f seconds",
	    ifp->name, inet_ntoa(astate->addr),
	    astate->probes ? astate->probes : PROBE_NUM, PROBE_NUM,
	    (float)delay / MSEC_PER_SEC);
	if (arp_request(astate, NULL) == -1)
		logerr(__func__);
}

void
arp_probe(struct arp_state *astate)
{

	astate->probes = 0;
	logdebugx("%s: probing for %s",
	    astate->iface->name, inet_ntoa(astate->addr));
	arp_probe1(astate);
}
#endif	/* ARP */

struct arp_state *
arp_find(struct interface *ifp, const struct in_addr *addr)
{
	struct iarp_state *state;
	struct arp_state *astate;

	if ((state = ARP_STATE(ifp)) == NULL)
		goto out;
	TAILQ_FOREACH(astate, &state->arp_states, next) {
		if (astate->addr.s_addr == addr->s_addr && astate->iface == ifp)
			return astate;
	}
out:
	errno = ESRCH;
	return NULL;
}

static void
arp_announced(void *arg)
{
	struct arp_state *astate = arg;

	if (astate->announced_cb) {
		astate->announced_cb(astate);
		return;
	}

	/* Keep the ARP state open to handle ongoing ACD. */
}

static void
arp_announce1(void *arg)
{
	struct arp_state *astate = arg;
	struct interface *ifp = astate->iface;
	struct ipv4_addr *ia;

	if (++astate->claims < ANNOUNCE_NUM)
		logdebugx("%s: ARP announcing %s (%d of %d), "
		    "next in %d.0 seconds",
		    ifp->name, inet_ntoa(astate->addr),
		    astate->claims, ANNOUNCE_NUM, ANNOUNCE_WAIT);
	else
		logdebugx("%s: ARP announcing %s (%d of %d)",
		    ifp->name, inet_ntoa(astate->addr),
		    astate->claims, ANNOUNCE_NUM);

	/* The kernel will send a Gratuitous ARP for newly added addresses.
	 * So we can avoid sending the same.
	 * Linux is special and doesn't send one. */
	ia = ipv4_iffindaddr(ifp, &astate->addr, NULL);
#ifndef __linux__
	if (astate->claims == 1 && ia != NULL && ia->flags & IPV4_AF_NEW)
		goto skip_request;
#endif

	if (arp_request(astate, &astate->addr) == -1)
		logerr(__func__);

#ifndef __linux__
skip_request:
#endif
	/* No longer a new address. */
	if (ia != NULL)
		ia->flags |= ~IPV4_AF_NEW;

	eloop_timeout_add_sec(ifp->ctx->eloop, ANNOUNCE_WAIT,
	    astate->claims < ANNOUNCE_NUM ? arp_announce1 : arp_announced,
	    astate);
}

static void
arp_announce(struct arp_state *astate)
{
	struct iarp_state *state;
	struct interface *ifp;
	struct arp_state *a2;
	int r;

	/* Cancel any other ARP announcements for this address. */
	TAILQ_FOREACH(ifp, astate->iface->ctx->ifaces, next) {
		state = ARP_STATE(ifp);
		if (state == NULL)
			continue;
		TAILQ_FOREACH(a2, &state->arp_states, next) {
			if (astate == a2 ||
			    a2->addr.s_addr != astate->addr.s_addr)
				continue;
			r = eloop_timeout_delete(a2->iface->ctx->eloop,
			    a2->claims < ANNOUNCE_NUM
			    ? arp_announce1 : arp_announced,
			    a2);
			if (r == -1)
				logerr(__func__);
			else if (r != 0) {
				logdebugx("%s: ARP announcement "
				    "of %s cancelled",
				    a2->iface->name,
				    inet_ntoa(a2->addr));
				arp_announced(a2);
			}
		}
	}

	astate->claims = 0;
	arp_announce1(astate);
}

struct arp_state *
arp_ifannounceaddr(struct interface *ifp, const struct in_addr *ia)
{
	struct arp_state *astate;

	if (ifp->flags & IFF_NOARP || !(ifp->options->options & DHCPCD_ARP))
		return NULL;

	astate = arp_find(ifp, ia);
	if (astate == NULL) {
		astate = arp_new(ifp, ia);
		if (astate == NULL)
			return NULL;
		astate->announced_cb = arp_free;
	}
	arp_announce(astate);
	return astate;
}

struct arp_state *
arp_announceaddr(struct dhcpcd_ctx *ctx, const struct in_addr *ia)
{
	struct interface *ifp, *iff = NULL;
	struct ipv4_addr *iap;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (!ifp->active || !if_is_link_up(ifp))
			continue;
		iap = ipv4_iffindaddr(ifp, ia, NULL);
		if (iap == NULL)
			continue;
#ifdef IN_IFF_NOTUSEABLE
		if (iap->addr_flags & IN_IFF_NOTUSEABLE)
			continue;
#endif
		if (iff != NULL && iff->metric < ifp->metric)
			continue;
		iff = ifp;
	}
	if (iff == NULL)
		return NULL;

	return arp_ifannounceaddr(iff, ia);
}

struct arp_state *
arp_new(struct interface *ifp, const struct in_addr *addr)
{
	struct iarp_state *state;
	struct arp_state *astate;

	if ((state = ARP_STATE(ifp)) == NULL) {
		ifp->if_data[IF_DATA_ARP] = malloc(sizeof(*state));
		state = ARP_STATE(ifp);
		if (state == NULL) {
			logerr(__func__);
			return NULL;
		}
		TAILQ_INIT(&state->arp_states);
	} else {
		if ((astate = arp_find(ifp, addr)) != NULL)
			return astate;
	}

	if ((astate = calloc(1, sizeof(*astate))) == NULL) {
		logerr(__func__);
		return NULL;
	}
	astate->iface = ifp;
	astate->addr = *addr;

#ifdef PRIVSEP
	if (IN_PRIVSEP(ifp->ctx)) {
		if (ps_bpf_openarp(ifp, addr) == -1) {
			logerr(__func__);
			free(astate);
			return NULL;
		}
	} else
#endif
	{
		astate->bpf = bpf_open(ifp, bpf_arp, addr);
		if (astate->bpf == NULL) {
			logerr(__func__);
			free(astate);
			return NULL;
		}
		if (eloop_event_add(ifp->ctx->eloop, astate->bpf->bpf_fd, ELE_READ,
		    arp_read, astate) == -1)
			logerr("%s: eloop_event_add", __func__);
	}


	state = ARP_STATE(ifp);
	TAILQ_INSERT_TAIL(&state->arp_states, astate, next);
	return astate;
}

void
arp_free(struct arp_state *astate)
{
	struct interface *ifp;
	struct dhcpcd_ctx *ctx;
	struct iarp_state *state;

	if (astate == NULL)
		return;

	ifp = astate->iface;
	ctx = ifp->ctx;
	eloop_timeout_delete(ctx->eloop, NULL, astate);

	state =	ARP_STATE(ifp);
	TAILQ_REMOVE(&state->arp_states, astate, next);
	if (astate->free_cb)
		astate->free_cb(astate);

#ifdef PRIVSEP
	if (IN_PRIVSEP(ctx) && ps_bpf_closearp(ifp, &astate->addr) == -1)
		logerr(__func__);
#endif
	if (astate->bpf != NULL) {
		eloop_event_delete(ctx->eloop, astate->bpf->bpf_fd);
		bpf_close(astate->bpf);
	}

	free(astate);

	if (TAILQ_FIRST(&state->arp_states) == NULL) {
		free(state);
		ifp->if_data[IF_DATA_ARP] = NULL;
	}
}

void
arp_freeaddr(struct interface *ifp, const struct in_addr *ia)
{
	struct arp_state *astate;

	astate = arp_find(ifp, ia);
	arp_free(astate);
}

void
arp_drop(struct interface *ifp)
{
	struct iarp_state *state;
	struct arp_state *astate;

	while ((state = ARP_STATE(ifp)) != NULL &&
	    (astate = TAILQ_FIRST(&state->arp_states)) != NULL)
		arp_free(astate);
}
