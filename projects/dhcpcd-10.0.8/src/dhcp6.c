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

#include <sys/utsname.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

#define ELOOP_QUEUE	ELOOP_DHCP6
#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "duid.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "script.h"

#ifdef HAVE_SYS_BITOPS_H
#include <sys/bitops.h>
#else
#include "compat/bitops.h"
#endif

/* DHCPCD Project has been assigned an IANA PEN of 40712 */
#define DHCPCD_IANA_PEN 40712

/* Unsure if I want this */
//#define VENDOR_SPLIT

/* Support older systems with different defines */
#if !defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define IPV6_RECVPKTINFO IPV6_PKTINFO
#endif

#ifdef DHCP6

/* Assert the correct structure size for on wire */
struct dhcp6_message {
	uint8_t type;
	uint8_t xid[3];
	/* followed by options */
};
__CTASSERT(sizeof(struct dhcp6_message) == 4);

struct dhcp6_option {
	uint16_t code;
	uint16_t len;
	/* followed by data */
};
__CTASSERT(sizeof(struct dhcp6_option) == 4);

struct dhcp6_ia_na {
	uint8_t iaid[4];
	uint32_t t1;
	uint32_t t2;
};
__CTASSERT(sizeof(struct dhcp6_ia_na) == 12);

struct dhcp6_ia_ta {
	uint8_t iaid[4];
};
__CTASSERT(sizeof(struct dhcp6_ia_ta) == 4);

struct dhcp6_ia_addr {
	struct in6_addr addr;
	uint32_t pltime;
	uint32_t vltime;
};
__CTASSERT(sizeof(struct dhcp6_ia_addr) == 16 + 8);

/* XXX FIXME: This is the only packed structure and it does not align.
 * Maybe manually decode it? */
struct dhcp6_pd_addr {
	uint32_t pltime;
	uint32_t vltime;
	uint8_t prefix_len;
	struct in6_addr prefix;
} __packed;
__CTASSERT(sizeof(struct dhcp6_pd_addr) == 8 + 1 + 16);

struct dhcp6_op {
	uint16_t type;
	const char *name;
};

static const struct dhcp6_op dhcp6_ops[] = {
	{ DHCP6_SOLICIT, "SOLICIT6" },
	{ DHCP6_ADVERTISE, "ADVERTISE6" },
	{ DHCP6_REQUEST, "REQUEST6" },
	{ DHCP6_REPLY, "REPLY6" },
	{ DHCP6_RENEW, "RENEW6" },
	{ DHCP6_REBIND, "REBIND6" },
	{ DHCP6_CONFIRM, "CONFIRM6" },
	{ DHCP6_INFORMATION_REQ, "INFORM6" },
	{ DHCP6_RELEASE, "RELEASE6" },
	{ DHCP6_RECONFIGURE, "RECONFIGURE6" },
	{ DHCP6_DECLINE, "DECLINE6" },
	{ 0, NULL }
};

struct dhcp_compat {
	uint8_t dhcp_opt;
	uint16_t dhcp6_opt;
};

/*
 * RFC 5908 deprecates OPTION_SNTP_SERVERS.
 * But we can support both as the hook scripts will uniqify the
 * results if the server returns both options.
 */
static const struct dhcp_compat dhcp_compats[] = {
	{ DHO_DNSSERVER,	D6_OPTION_DNS_SERVERS },
	{ DHO_HOSTNAME,		D6_OPTION_FQDN },
	{ DHO_DNSDOMAIN,	D6_OPTION_FQDN },
	{ DHO_NISSERVER,	D6_OPTION_NIS_SERVERS },
	{ DHO_NTPSERVER,	D6_OPTION_SNTP_SERVERS },
	{ DHO_NTPSERVER,	D6_OPTION_NTP_SERVER },
	{ DHO_RAPIDCOMMIT,	D6_OPTION_RAPID_COMMIT },
	{ DHO_FQDN,		D6_OPTION_FQDN },
	{ DHO_VIVCO,		D6_OPTION_VENDOR_CLASS },
	{ DHO_VIVSO,		D6_OPTION_VENDOR_OPTS },
	{ DHO_DNSSEARCH,	D6_OPTION_DOMAIN_LIST },
	{ 0, 0 }
};

static const char * const dhcp6_statuses[] = {
	"Success",
	"Unspecified Failure",
	"No Addresses Available",
	"No Binding",
	"Not On Link",
	"Use Multicast",
	"No Prefix Available"
};

static void dhcp6_bind(struct interface *, const char *, const char *);
static void dhcp6_failinform(void *);
static void dhcp6_recvaddr(void *, unsigned short);
static void dhcp6_startdecline(struct interface *);
static void dhcp6_startrequest(struct interface *);

#ifdef SMALL
#define dhcp6_hasprefixdelegation(a)	(0)
#else
static int dhcp6_hasprefixdelegation(struct interface *);
#endif

#define DECLINE_IA(ia) \
	((ia)->addr_flags & IN6_IFF_DUPLICATED && \
	(ia)->ia_type != 0 && (ia)->ia_type != D6_OPTION_IA_PD && \
	!((ia)->flags & IPV6_AF_STALE) && \
	(ia)->prefix_vltime != 0)

void
dhcp6_printoptions(const struct dhcpcd_ctx *ctx,
    const struct dhcp_opt *opts, size_t opts_len)
{
	size_t i, j;
	const struct dhcp_opt *opt, *opt2;
	int cols;

	for (i = 0, opt = ctx->dhcp6_opts;
	    i < ctx->dhcp6_opts_len; i++, opt++)
	{
		for (j = 0, opt2 = opts; j < opts_len; j++, opt2++)
			if (opt2->option == opt->option)
				break;
		if (j == opts_len) {
			cols = printf("%05d %s", opt->option, opt->var);
			dhcp_print_option_encoding(opt, cols);
		}
	}
	for (i = 0, opt = opts; i < opts_len; i++, opt++) {
		cols = printf("%05d %s", opt->option, opt->var);
		dhcp_print_option_encoding(opt, cols);
	}
}

static size_t
dhcp6_makeuser(void *data, const struct interface *ifp)
{
	const struct if_options *ifo = ifp->options;
	struct dhcp6_option o;
	uint8_t *p;
	const uint8_t *up, *ue;
	uint16_t ulen, unlen;
	size_t olen;

	/* Convert the DHCPv4 user class option to DHCPv6 */
	up = ifo->userclass;
	ulen = *up++;
	if (ulen == 0)
		return 0;

	p = data;
	olen = 0;
	if (p != NULL)
		p += sizeof(o);

	ue = up + ulen;
	for (; up < ue; up += ulen) {
		ulen = *up++;
		olen += sizeof(ulen) + ulen;
		if (data == NULL)
			continue;
		unlen = htons(ulen);
		memcpy(p, &unlen, sizeof(unlen));
		p += sizeof(unlen);
		memcpy(p, up, ulen);
		p += ulen;
	}
	if (data != NULL) {
		o.code = htons(D6_OPTION_USER_CLASS);
		o.len = htons((uint16_t)olen);
		memcpy(data, &o, sizeof(o));
	}

	return sizeof(o) + olen;
}

static size_t
dhcp6_makevendor(void *data, const struct interface *ifp)
{
	const struct if_options *ifo;
	size_t len, vlen, i;
	uint8_t *p;
	const struct vivco *vivco;
	struct dhcp6_option o;

	ifo = ifp->options;
	len = sizeof(uint32_t); /* IANA PEN */
	if (ifo->vivco_en) {
		vlen = 0;
		for (i = 0, vivco = ifo->vivco;
		    i < ifo->vivco_len;
		    i++, vivco++)
			vlen += sizeof(uint16_t) + vivco->len;
		len += vlen;
	} else if (ifo->vendorclassid[0] != '\0') {
		/* dhcpcd owns DHCPCD_IANA_PEN.
		 * If you need your own string, get your own IANA PEN. */
		vlen = strlen(ifp->ctx->vendor);
		len += sizeof(uint16_t) + vlen;
	} else
		return 0;

	if (len > UINT16_MAX) {
		logerrx("%s: DHCPv6 Vendor Class too big", ifp->name);
		return 0;
	}

	if (data != NULL) {
		uint32_t pen;
		uint16_t hvlen;

		p = data;
		o.code = htons(D6_OPTION_VENDOR_CLASS);
		o.len = htons((uint16_t)len);
		memcpy(p, &o, sizeof(o));
		p += sizeof(o);
		pen = htonl(ifo->vivco_en ? ifo->vivco_en : DHCPCD_IANA_PEN);
		memcpy(p, &pen, sizeof(pen));
		p += sizeof(pen);

		if (ifo->vivco_en) {
			for (i = 0, vivco = ifo->vivco;
			    i < ifo->vivco_len;
			    i++, vivco++)
			{
				hvlen = htons((uint16_t)vivco->len);
				memcpy(p, &hvlen, sizeof(hvlen));
				p += sizeof(hvlen);
				memcpy(p, vivco->data, vivco->len);
				p += vivco->len;
			}
		} else if (ifo->vendorclassid[0] != '\0') {
			hvlen = htons((uint16_t)vlen);
			memcpy(p, &hvlen, sizeof(hvlen));
			p += sizeof(hvlen);
			memcpy(p, ifp->ctx->vendor, vlen);
		}
	}

	return sizeof(o) + len;
}

static void *
dhcp6_findoption(void *data, size_t data_len, uint16_t code, uint16_t *len)
{
	uint8_t *d;
	struct dhcp6_option o;

	code = htons(code);
	for (d = data; data_len != 0; d += o.len, data_len -= o.len) {
		if (data_len < sizeof(o)) {
			errno = EINVAL;
			return NULL;
		}
		memcpy(&o, d, sizeof(o));
		d += sizeof(o);
		data_len -= sizeof(o);
		o.len = htons(o.len);
		if (data_len < o.len) {
			errno = EINVAL;
			return NULL;
		}
		if (o.code == code) {
			if (len != NULL)
				*len = o.len;
			return d;
		}
	}

	errno = ENOENT;
	return NULL;
}

static void *
dhcp6_findmoption(void *data, size_t data_len, uint16_t code,
    uint16_t *len)
{
	uint8_t *d;

	if (data_len < sizeof(struct dhcp6_message)) {
		errno = EINVAL;
		return false;
	}
	d = data;
	d += sizeof(struct dhcp6_message);
	data_len -= sizeof(struct dhcp6_message);
	return dhcp6_findoption(d, data_len, code, len);
}

static const uint8_t *
dhcp6_getoption(struct dhcpcd_ctx *ctx,
    size_t *os, unsigned int *code, size_t *len,
    const uint8_t *od, size_t ol, struct dhcp_opt **oopt)
{
	struct dhcp6_option o;
	size_t i;
	struct dhcp_opt *opt;

	if (od != NULL) {
		*os = sizeof(o);
		if (ol < *os) {
			errno = EINVAL;
			return NULL;
		}
		memcpy(&o, od, sizeof(o));
		*len = ntohs(o.len);
		if (*len > ol - *os) {
			errno = ERANGE;
			return NULL;
		}
		*code = ntohs(o.code);
	}

	*oopt = NULL;
	for (i = 0, opt = ctx->dhcp6_opts;
	    i < ctx->dhcp6_opts_len; i++, opt++)
	{
		if (opt->option == *code) {
			*oopt = opt;
			break;
		}
	}

	if (od != NULL)
		return od + sizeof(o);
	return NULL;
}

static bool
dhcp6_updateelapsed(struct interface *ifp, struct dhcp6_message *m, size_t len)
{
	uint8_t *opt;
	uint16_t opt_len;
	struct dhcp6_state *state;
	struct timespec tv;
	unsigned long long hsec;
	uint16_t sec;

	opt = dhcp6_findmoption(m, len, D6_OPTION_ELAPSED, &opt_len);
	if (opt == NULL)
		return false;
	if (opt_len != sizeof(sec)) {
		errno = EINVAL;
		return false;
	}

	state = D6_STATE(ifp);
	clock_gettime(CLOCK_MONOTONIC, &tv);
	if (state->RTC == 0) {
		/* An RTC of zero means we're the first message
		 * out of the door, so the elapsed time is zero. */
		state->started = tv;
		hsec = 0;
	} else {
		unsigned long long secs;
		unsigned int nsecs;

		secs = eloop_timespec_diff(&tv, &state->started, &nsecs);
		/* Elapsed time is measured in centiseconds.
		 * We need to be sure it will not potentially overflow. */
		if (secs >= (UINT16_MAX / CSEC_PER_SEC) + 1)
			hsec = UINT16_MAX;
		else {
			hsec = (secs * CSEC_PER_SEC) +
			    (nsecs / NSEC_PER_CSEC);
			if (hsec > UINT16_MAX)
				hsec = UINT16_MAX;
		}
	}
	sec = htons((uint16_t)hsec);
	memcpy(opt, &sec, sizeof(sec));
	return true;
}

static void
dhcp6_newxid(const struct interface *ifp, struct dhcp6_message *m)
{
	const struct interface *ifp1;
	const struct dhcp6_state *state1;
	uint32_t xid;

	if (ifp->options->options & DHCPCD_XID_HWADDR &&
	    ifp->hwlen >= sizeof(xid))
		/* The lower bits are probably more unique on the network */
		memcpy(&xid, (ifp->hwaddr + ifp->hwlen) - sizeof(xid),
		    sizeof(xid));
	else {
again:
		xid = arc4random();
	}

	m->xid[0] = (xid >> 16) & 0xff;
	m->xid[1] = (xid >> 8) & 0xff;
	m->xid[2] = xid & 0xff;

	/* Ensure it's unique */
	TAILQ_FOREACH(ifp1, ifp->ctx->ifaces, next) {
		if (ifp == ifp1)
			continue;
		if ((state1 = D6_CSTATE(ifp1)) == NULL)
			continue;
		if (state1->send != NULL &&
		    state1->send->xid[0] == m->xid[0] &&
		    state1->send->xid[1] == m->xid[1] &&
		    state1->send->xid[2] == m->xid[2])
			break;
	}

	if (ifp1 != NULL) {
		if (ifp->options->options & DHCPCD_XID_HWADDR &&
		    ifp->hwlen >= sizeof(xid))
		{
			logerrx("%s: duplicate xid on %s",
			    ifp->name, ifp1->name);
			    return;
		}
		goto again;
	}
}

#ifndef SMALL
static const struct if_sla *
dhcp6_findselfsla(struct interface *ifp)
{
	size_t i, j;
	struct if_ia *ia;

	for (i = 0; i < ifp->options->ia_len; i++) {
		ia = &ifp->options->ia[i];
		if (ia->ia_type != D6_OPTION_IA_PD)
			continue;
		for (j = 0; j < ia->sla_len; j++) {
			if (strcmp(ia->sla[j].ifname, ifp->name) == 0)
				return &ia->sla[j];
		}
	}
	return NULL;
}

static int
dhcp6_delegateaddr(struct in6_addr *addr, struct interface *ifp,
    const struct ipv6_addr *prefix, const struct if_sla *sla, struct if_ia *ia)
{
	struct dhcp6_state *state;
	struct if_sla asla;
	char sabuf[INET6_ADDRSTRLEN];
	const char *sa;

	state = D6_STATE(ifp);
	if (state == NULL) {
		ifp->if_data[IF_DATA_DHCP6] = calloc(1, sizeof(*state));
		state = D6_STATE(ifp);
		if (state == NULL) {
			logerr(__func__);
			return -1;
		}

		TAILQ_INIT(&state->addrs);
		state->state = DH6S_DELEGATED;
		state->reason = "DELEGATED6";
	}

	if (sla == NULL || !sla->sla_set) {
		/* No SLA set, so make an assumption of
		 * desired SLA and prefix length. */
		asla.sla = ifp->index;
		asla.prefix_len = 0;
		asla.sla_set = false;
		sla = &asla;
	} else if (sla->prefix_len == 0) {
		/* An SLA was given, but prefix length was not.
		 * We need to work out a suitable prefix length for
		 * potentially more than one interface. */
		asla.sla = sla->sla;
		asla.prefix_len = 0;
		asla.sla_set = sla->sla_set;
		sla = &asla;
	}

	if (sla->prefix_len == 0) {
		uint32_t sla_max;
		int bits;

		sla_max = ia->sla_max;
		if (sla_max == 0 && (sla == NULL || !sla->sla_set)) {
			const struct interface *ifi;

			TAILQ_FOREACH(ifi, ifp->ctx->ifaces, next) {
				if (ifi->index > sla_max)
					sla_max = ifi->index;
			}
		}

		bits = fls32(sla_max);

		if (prefix->prefix_len + bits > (int)UINT8_MAX)
			asla.prefix_len = UINT8_MAX;
		else {
			asla.prefix_len = (uint8_t)(prefix->prefix_len + bits);

			/* Make a 64 prefix by default, as this makes SLAAC
			 * possible.
			 * Otherwise round up to the nearest 4 bits. */
			if (asla.prefix_len <= 64)
				asla.prefix_len = 64;
			else
				asla.prefix_len =
				    (uint8_t)ROUNDUP4(asla.prefix_len);
		}

#define BIT(n) (1UL << (n))
#define BIT_MASK(len) (BIT(len) - 1)
		if (ia->sla_max == 0) {
			/* Work out the real sla_max from our bits used */
			bits = asla.prefix_len - prefix->prefix_len;
			/* Make static analysis happy.
			 * Bits cannot be bigger than 32 thanks to fls32. */
			assert(bits <= 32);
			ia->sla_max = (uint32_t)BIT_MASK(bits);
		}
	}

	if (ipv6_userprefix(&prefix->prefix, prefix->prefix_len,
		sla->sla, addr, sla->prefix_len) == -1)
	{
		sa = inet_ntop(AF_INET6, &prefix->prefix,
		    sabuf, sizeof(sabuf));
		logerr("%s: invalid prefix %s/%d + %d/%d",
		    ifp->name, sa, prefix->prefix_len,
		    sla->sla, sla->prefix_len);
		return -1;
	}

	if (prefix->prefix_exclude_len &&
	    IN6_ARE_ADDR_EQUAL(addr, &prefix->prefix_exclude))
	{
		sa = inet_ntop(AF_INET6, &prefix->prefix_exclude,
		    sabuf, sizeof(sabuf));
		logerrx("%s: cannot delegate excluded prefix %s/%d",
		    ifp->name, sa, prefix->prefix_exclude_len);
		return -1;
	}

	return sla->prefix_len;
}
#endif

static int
dhcp6_makemessage(struct interface *ifp)
{
	struct dhcp6_state *state;
	struct dhcp6_message *m;
	struct dhcp6_option o;
	uint8_t *p, *si, *unicast, IA;
	size_t n, l, len, ml, hl;
	uint8_t type;
	uint16_t si_len, uni_len, n_options;
	uint8_t *o_lenp;
	struct if_options *ifo = ifp->options;
	const struct dhcp_opt *opt, *opt2;
	const struct ipv6_addr *ap;
	char hbuf[HOSTNAME_MAX_LEN + 1];
	const char *hostname;
	int fqdn;
	struct dhcp6_ia_na ia_na;
	uint16_t ia_na_len;
	struct if_ia *ifia;
#ifdef AUTH
	uint16_t auth_len;
#endif
	uint8_t duid[DUID_LEN];
	size_t duid_len = 0;

	state = D6_STATE(ifp);
	if (state->send) {
		free(state->send);
		state->send = NULL;
	}

	switch(state->state) {
	case DH6S_INIT: /* FALLTHROUGH */
	case DH6S_DISCOVER:
		type = DHCP6_SOLICIT;
		break;
	case DH6S_REQUEST:
		type = DHCP6_REQUEST;
		break;
	case DH6S_CONFIRM:
		type = DHCP6_CONFIRM;
		break;
	case DH6S_REBIND:
		type = DHCP6_REBIND;
		break;
	case DH6S_RENEW:
		type = DHCP6_RENEW;
		break;
	case DH6S_INFORM:
		type = DHCP6_INFORMATION_REQ;
		break;
	case DH6S_RELEASE:
		type = DHCP6_RELEASE;
		break;
	case DH6S_DECLINE:
		type = DHCP6_DECLINE;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	/* RFC 4704 Section 5 says we can only send FQDN for these
	 * message types. */
	switch(type) {
	case DHCP6_SOLICIT:
	case DHCP6_REQUEST:
	case DHCP6_RENEW:
	case DHCP6_REBIND:
		fqdn = ifo->fqdn;
		break;
	default:
		fqdn = FQDN_DISABLE;
		break;
	}

	if (fqdn == FQDN_DISABLE && ifo->options & DHCPCD_HOSTNAME) {
		/* We're sending the DHCPv4 hostname option, so send FQDN as
		 * DHCPv6 has no FQDN option and DHCPv4 must not send
		 * hostname and FQDN according to RFC4702 */
		fqdn = FQDN_BOTH;
	}
	if (fqdn != FQDN_DISABLE)
		hostname = dhcp_get_hostname(hbuf, sizeof(hbuf), ifo);
	else
		hostname = NULL; /* appearse gcc */

	/* Work out option size first */
	n_options = 0;
	len = 0;
	si = NULL;
	hl = 0; /* Appease gcc */
	if (state->state != DH6S_RELEASE && state->state != DH6S_DECLINE) {
		for (l = 0, opt = ifp->ctx->dhcp6_opts;
		    l < ifp->ctx->dhcp6_opts_len;
		    l++, opt++)
		{
			for (n = 0, opt2 = ifo->dhcp6_override;
			    n < ifo->dhcp6_override_len;
			    n++, opt2++)
			{
				if (opt->option == opt2->option)
					break;
			}
			if (n < ifo->dhcp6_override_len)
				continue;
			if (!DHC_REQOPT(opt, ifo->requestmask6, ifo->nomask6))
				continue;
			n_options++;
			len += sizeof(o.len);
		}
#ifndef SMALL
		for (l = 0, opt = ifo->dhcp6_override;
		    l < ifo->dhcp6_override_len;
		    l++, opt++)
		{
			if (!DHC_REQOPT(opt, ifo->requestmask6, ifo->nomask6))
				continue;
			n_options++;
			len += sizeof(o.len);
		}
		if (dhcp6_findselfsla(ifp)) {
			n_options++;
			len += sizeof(o.len);
		}
#endif
		if (len)
			len += sizeof(o);

		if (fqdn != FQDN_DISABLE) {
			hl = encode_rfc1035(hostname, NULL);
			len += sizeof(o) + 1 + hl;
		}

		if (!has_option_mask(ifo->nomask6, D6_OPTION_MUDURL) &&
		    ifo->mudurl[0])
			len += sizeof(o) + ifo->mudurl[0];

#ifdef AUTH
		if ((ifo->auth.options & DHCPCD_AUTH_SENDREQUIRE) !=
		    DHCPCD_AUTH_SENDREQUIRE &&
		    DHC_REQ(ifo->requestmask6, ifo->nomask6,
		    D6_OPTION_RECONF_ACCEPT))
			len += sizeof(o); /* Reconfigure Accept */
#endif
	}

	len += sizeof(*state->send);
	len += sizeof(o) + sizeof(uint16_t); /* elapsed */

	if (ifo->options & DHCPCD_ANONYMOUS) {
		duid_len = duid_make(duid, ifp, DUID_LL);
		len += sizeof(o) + duid_len;
	} else {
		len += sizeof(o) + ifp->ctx->duid_len;
	}

	if (!has_option_mask(ifo->nomask6, D6_OPTION_USER_CLASS))
		len += dhcp6_makeuser(NULL, ifp);
	if (!has_option_mask(ifo->nomask6, D6_OPTION_VENDOR_CLASS))
		len += dhcp6_makevendor(NULL, ifp);

	/* IA */
	m = NULL;
	ml = 0;
	switch(state->state) {
	case DH6S_REQUEST:
		m = state->recv;
		ml = state->recv_len;
		/* FALLTHROUGH */
	case DH6S_DECLINE:
		/* FALLTHROUGH */
	case DH6S_RELEASE:
		/* FALLTHROUGH */
	case DH6S_RENEW:
		if (m == NULL) {
			m = state->new;
			ml = state->new_len;
		}
		si = dhcp6_findmoption(m, ml, D6_OPTION_SERVERID, &si_len);
		if (si == NULL)
			return -1;
		len += sizeof(o) + si_len;
		/* FALLTHROUGH */
	case DH6S_REBIND:
		/* FALLTHROUGH */
	case DH6S_CONFIRM:
		/* FALLTHROUGH */
	case DH6S_DISCOVER:
		if (m == NULL) {
			m = state->new;
			ml = state->new_len;
		}
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ap->flags & IPV6_AF_STALE)
				continue;
			if (!(ap->flags & IPV6_AF_REQUEST) &&
			    (ap->prefix_vltime == 0 ||
			    state->state == DH6S_DISCOVER))
				continue;
			if (DECLINE_IA(ap) && state->state != DH6S_DECLINE)
				continue;
			if (ap->ia_type == D6_OPTION_IA_PD) {
#ifndef SMALL
				len += sizeof(o) + sizeof(struct dhcp6_pd_addr);
				if (ap->prefix_exclude_len)
					len += sizeof(o) + 1 +
					    (uint8_t)((ap->prefix_exclude_len -
					    ap->prefix_len - 1) / NBBY) + 1;
#endif
			} else
				len += sizeof(o) + sizeof(struct dhcp6_ia_addr);
		}
		/* FALLTHROUGH */
	case DH6S_INIT:
		for (l = 0; l < ifo->ia_len; l++) {
			len += sizeof(o) + sizeof(uint32_t); /* IAID */
			/* IA_TA does not have T1 or T2 timers */
			if (ifo->ia[l].ia_type != D6_OPTION_IA_TA)
				len += sizeof(uint32_t) + sizeof(uint32_t);
		}
		IA = 1;
		break;
	default:
		IA = 0;
	}

	if (state->state == DH6S_DISCOVER &&
	    !(ifp->ctx->options & DHCPCD_TEST) &&
	    DHC_REQ(ifo->requestmask6, ifo->nomask6, D6_OPTION_RAPID_COMMIT))
		len += sizeof(o);

	if (m == NULL) {
		m = state->new;
		ml = state->new_len;
	}

	switch(state->state) {
	case DH6S_REQUEST: /* FALLTHROUGH */
	case DH6S_RENEW:   /* FALLTHROUGH */
	case DH6S_RELEASE:
		if (has_option_mask(ifo->nomask6, D6_OPTION_UNICAST)) {
			unicast = NULL;
			break;
		}
		unicast = dhcp6_findmoption(m, ml, D6_OPTION_UNICAST, &uni_len);
		break;
	default:
		unicast = NULL;
		break;
	}

	/* In non manager mode we listen and send from fixed addresses.
	 * We should try and match an address we have to unicast to,
	 * but for now this is the safest policy. */
	if (unicast != NULL && !(ifp->ctx->options & DHCPCD_MANAGER)) {
		logdebugx("%s: ignoring unicast option as not manager",
		    ifp->name);
		unicast = NULL;
	}

#ifdef AUTH
	auth_len = 0;
	if (ifo->auth.options & DHCPCD_AUTH_SEND) {
		ssize_t alen = dhcp_auth_encode(ifp->ctx, &ifo->auth,
		    state->auth.token, NULL, 0, 6, type, NULL, 0);
		if (alen != -1 && alen > UINT16_MAX) {
			errno = ERANGE;
			alen = -1;
		}
		if (alen == -1)
			logerr("%s: %s: dhcp_auth_encode", __func__, ifp->name);
		else if (alen != 0) {
			auth_len = (uint16_t)alen;
			len += sizeof(o) + auth_len;
		}
	}
#endif

	state->send = malloc(len);
	if (state->send == NULL)
		return -1;

	state->send_len = len;
	state->send->type = type;

	/* If we found a unicast option, copy it to our state for sending */
	if (unicast && uni_len == sizeof(state->unicast))
		memcpy(&state->unicast, unicast, sizeof(state->unicast));
	else
		state->unicast = in6addr_any;

	dhcp6_newxid(ifp, state->send);

#define COPYIN1(_code, _len)		{	\
	o.code = htons((_code));		\
	o.len = htons((_len));			\
	memcpy(p, &o, sizeof(o));		\
	p += sizeof(o);				\
}
#define COPYIN(_code, _data, _len)	do {	\
	COPYIN1((_code), (_len));		\
	if ((_len) != 0) {			\
		memcpy(p, (_data), (_len));	\
		p += (_len);			\
	}					\
} while (0 /* CONSTCOND */)
#define NEXTLEN (p + offsetof(struct dhcp6_option, len))

	/* Options are listed in numerical order as per RFC 7844 Section 4.1
	 * XXX: They should be randomised. */

	p = (uint8_t *)state->send + sizeof(*state->send);
	if (ifo->options & DHCPCD_ANONYMOUS)
		COPYIN(D6_OPTION_CLIENTID, duid,
		    (uint16_t)duid_len);
	else
		COPYIN(D6_OPTION_CLIENTID, ifp->ctx->duid,
		    (uint16_t)ifp->ctx->duid_len);

	if (si != NULL)
		COPYIN(D6_OPTION_SERVERID, si, si_len);

	for (l = 0; IA && l < ifo->ia_len; l++) {
		ifia = &ifo->ia[l];
		o_lenp = NEXTLEN;
		/* TA structure is the same as the others,
		 * it just lacks the T1 and T2 timers.
		 * These happen to be at the end of the struct,
		 * so we just don't copy them in. */
		if (ifia->ia_type == D6_OPTION_IA_TA)
			ia_na_len = sizeof(struct dhcp6_ia_ta);
		else
			ia_na_len = sizeof(ia_na);
		memcpy(ia_na.iaid, ifia->iaid, sizeof(ia_na.iaid));
		/* RFC 8415 21.4 and 21.21 state that T1 and T2 should be zero.
		 * An RFC compliant server MUST ignore them anyway. */
		ia_na.t1 = 0;
		ia_na.t2 = 0;
		COPYIN(ifia->ia_type, &ia_na, ia_na_len);
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ap->flags & IPV6_AF_STALE)
				continue;
			if (!(ap->flags & IPV6_AF_REQUEST) &&
			    (ap->prefix_vltime == 0 ||
			    state->state == DH6S_DISCOVER))
				continue;
			if (DECLINE_IA(ap) && state->state != DH6S_DECLINE)
				continue;
			if (ap->ia_type != ifia->ia_type)
				continue;
			if (memcmp(ap->iaid, ifia->iaid, sizeof(ap->iaid)))
				continue;
			if (ap->ia_type == D6_OPTION_IA_PD) {
#ifndef SMALL
				struct dhcp6_pd_addr pdp = {
				    .prefix_len = ap->prefix_len,
				    /*
				     * RFC 8415 21.22 states that the
				     * valid and preferred lifetimes sent by
				     * the client SHOULD be zero and MUST
				     * be ignored by the server.
				     */
				};

				/* pdp.prefix is not aligned, so copy it in. */
				memcpy(&pdp.prefix, &ap->prefix, sizeof(pdp.prefix));
				COPYIN(D6_OPTION_IAPREFIX, &pdp, sizeof(pdp));
				ia_na_len = (uint16_t)
				    (ia_na_len + sizeof(o) + sizeof(pdp));

				/* RFC6603 Section 4.2 */
				if (ap->prefix_exclude_len) {
					uint8_t exb[16], *ep, u8;
					const uint8_t *pp;

					n = (size_t)((ap->prefix_exclude_len -
					    ap->prefix_len - 1) / NBBY) + 1;
					ep = exb;
					*ep++ = (uint8_t)ap->prefix_exclude_len;
					pp = ap->prefix_exclude.s6_addr;
					pp += (size_t)
					    ((ap->prefix_len - 1) / NBBY) +
					    (n - 1);
					u8 = ap->prefix_len % NBBY;
					if (u8)
						n--;
					while (n-- > 0)
						*ep++ = *pp--;
					n = (size_t)(ep - exb);
					if (u8) {
						*ep = (uint8_t)(*pp << u8);
						n++;
					}
					COPYIN(D6_OPTION_PD_EXCLUDE, exb,
					    (uint16_t)n);
					ia_na_len = (uint16_t)
					    (ia_na_len + sizeof(o) + n);
				}
#endif
			} else {
				struct dhcp6_ia_addr ia = {
				    .addr = ap->addr,
				    /*
				     * RFC 8415 21.6 states that the
				     * valid and preferred lifetimes sent by
				     * the client SHOULD be zero and MUST
				     * be ignored by the server.
				     */
				};

				COPYIN(D6_OPTION_IA_ADDR, &ia, sizeof(ia));
				ia_na_len = (uint16_t)
				    (ia_na_len + sizeof(o) + sizeof(ia));
			}
		}

		/* Update the total option lenth. */
		ia_na_len = htons(ia_na_len);
		memcpy(o_lenp, &ia_na_len, sizeof(ia_na_len));
	}

	if (state->send->type != DHCP6_RELEASE &&
	    state->send->type != DHCP6_DECLINE &&
	    n_options)
	{
		o_lenp = NEXTLEN;
		o.len = 0;
		COPYIN1(D6_OPTION_ORO, 0);
		for (l = 0, opt = ifp->ctx->dhcp6_opts;
		    l < ifp->ctx->dhcp6_opts_len;
		    l++, opt++)
		{
#ifndef SMALL
			for (n = 0, opt2 = ifo->dhcp6_override;
			    n < ifo->dhcp6_override_len;
			    n++, opt2++)
			{
				if (opt->option == opt2->option)
					break;
			}
			if (n < ifo->dhcp6_override_len)
			    continue;
#endif
			if (!DHC_REQOPT(opt, ifo->requestmask6, ifo->nomask6))
				continue;
			o.code = htons((uint16_t)opt->option);
			memcpy(p, &o.code, sizeof(o.code));
			p += sizeof(o.code);
			o.len = (uint16_t)(o.len + sizeof(o.code));
		}
#ifndef SMALL
		for (l = 0, opt = ifo->dhcp6_override;
		    l < ifo->dhcp6_override_len;
		    l++, opt++)
		{
			if (!DHC_REQOPT(opt, ifo->requestmask6, ifo->nomask6))
				continue;
			o.code = htons((uint16_t)opt->option);
			memcpy(p, &o.code, sizeof(o.code));
			p += sizeof(o.code);
			o.len = (uint16_t)(o.len + sizeof(o.code));
		}
		if (dhcp6_findselfsla(ifp)) {
			o.code = htons(D6_OPTION_PD_EXCLUDE);
			memcpy(p, &o.code, sizeof(o.code));
			p += sizeof(o.code);
			o.len = (uint16_t)(o.len + sizeof(o.code));
		}
#endif
		o.len = htons(o.len);
		memcpy(o_lenp, &o.len, sizeof(o.len));
	}

	si_len = 0;
	COPYIN(D6_OPTION_ELAPSED, &si_len, sizeof(si_len));

	if (state->state == DH6S_DISCOVER &&
	    !(ifp->ctx->options & DHCPCD_TEST) &&
	    DHC_REQ(ifo->requestmask6, ifo->nomask6, D6_OPTION_RAPID_COMMIT))
		COPYIN1(D6_OPTION_RAPID_COMMIT, 0);

	if (!has_option_mask(ifo->nomask6, D6_OPTION_USER_CLASS))
		p += dhcp6_makeuser(p, ifp);
	if (!has_option_mask(ifo->nomask6, D6_OPTION_VENDOR_CLASS))
		p += dhcp6_makevendor(p, ifp);

	if (state->send->type != DHCP6_RELEASE &&
	    state->send->type != DHCP6_DECLINE)
	{
		if (fqdn != FQDN_DISABLE) {
			o_lenp = NEXTLEN;
			COPYIN1(D6_OPTION_FQDN, 0);
			if (hl == 0)
				*p = D6_FQDN_NONE;
			else {
				switch (fqdn) {
				case FQDN_BOTH:
					*p = D6_FQDN_BOTH;
					break;
				case FQDN_PTR:
					*p = D6_FQDN_PTR;
					break;
				default:
					*p = D6_FQDN_NONE;
					break;
				}
			}
			p++;
			encode_rfc1035(hostname, p);
			p += hl;
			o.len = htons((uint16_t)(hl + 1));
			memcpy(o_lenp, &o.len, sizeof(o.len));
		}

		if (!has_option_mask(ifo->nomask6, D6_OPTION_MUDURL) &&
		    ifo->mudurl[0])
			COPYIN(D6_OPTION_MUDURL,
			    ifo->mudurl + 1, ifo->mudurl[0]);

#ifdef AUTH
		if ((ifo->auth.options & DHCPCD_AUTH_SENDREQUIRE) !=
		    DHCPCD_AUTH_SENDREQUIRE &&
		    DHC_REQ(ifo->requestmask6, ifo->nomask6,
		    D6_OPTION_RECONF_ACCEPT))
			COPYIN1(D6_OPTION_RECONF_ACCEPT, 0);
#endif

	}

#ifdef AUTH
	/* This has to be the last option */
	if (ifo->auth.options & DHCPCD_AUTH_SEND && auth_len != 0) {
		COPYIN1(D6_OPTION_AUTH, auth_len);
		/* data will be filled at send message time */
	}
#endif

	return 0;
}

static const char *
dhcp6_get_op(uint16_t type)
{
	const struct dhcp6_op *d;

	for (d = dhcp6_ops; d->name; d++)
		if (d->type == type)
			return d->name;
	return NULL;
}

static void
dhcp6_freedrop_addrs(struct interface *ifp, int drop,
    const struct interface *ifd)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state) {
		ipv6_freedrop_addrs(&state->addrs, drop, ifd);
		if (drop)
			rt_build(ifp->ctx, AF_INET6);
	}
}

#ifndef SMALL
static void dhcp6_delete_delegates(struct interface *ifp)
{
	struct interface *ifp0;

	if (ifp->ctx->ifaces) {
		TAILQ_FOREACH(ifp0, ifp->ctx->ifaces, next) {
			if (ifp0 != ifp)
				dhcp6_freedrop_addrs(ifp0, 1, ifp);
		}
	}
}
#endif

#ifdef AUTH
static ssize_t
dhcp6_update_auth(struct interface *ifp, struct dhcp6_message *m, size_t len)
{
	struct dhcp6_state *state;
	uint8_t *opt;
	uint16_t opt_len;

	opt = dhcp6_findmoption(m, len, D6_OPTION_AUTH, &opt_len);
	if (opt == NULL)
		return -1;

	state = D6_STATE(ifp);
	return dhcp_auth_encode(ifp->ctx, &ifp->options->auth,
	    state->auth.token, (uint8_t *)state->send, state->send_len, 6,
	    state->send->type, opt, opt_len);
}
#endif

static const struct in6_addr alldhcp = IN6ADDR_LINKLOCAL_ALLDHCP_INIT;
static int
dhcp6_sendmessage(struct interface *ifp, void (*callback)(void *))
{
	struct dhcp6_state *state = D6_STATE(ifp);
	struct dhcpcd_ctx *ctx = ifp->ctx;
	unsigned int RT;
	bool multicast = true;
	struct sockaddr_in6 dst = {
	    .sin6_family = AF_INET6,
	    /* Setting the port on Linux gives EINVAL when sending.
	     * This looks like a kernel bug as the equivalent works
	     * fine with the DHCP counterpart. */
#ifndef __linux__
	    .sin6_port = htons(DHCP6_SERVER_PORT),
#endif
	};
	struct udphdr udp = {
	    .uh_sport = htons(DHCP6_CLIENT_PORT),
	    .uh_dport = htons(DHCP6_SERVER_PORT),
	    .uh_ulen = htons((uint16_t)(sizeof(udp) + state->send_len)),
	};
	struct iovec iov[] = {
	    { .iov_base = &udp, .iov_len = sizeof(udp), },
	    { .iov_base = state->send, .iov_len = state->send_len, },
	};
	union {
		struct cmsghdr hdr;
		uint8_t buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	} cmsgbuf = { .buf = { 0 } };
	struct msghdr msg = {
	    .msg_name = &dst, .msg_namelen = sizeof(dst),
	    .msg_iov = iov, .msg_iovlen = __arraycount(iov),
	};
	char uaddr[INET6_ADDRSTRLEN];

	if (!callback && !if_is_link_up(ifp))
		return 0;

	if (!IN6_IS_ADDR_UNSPECIFIED(&state->unicast)) {
		switch (state->send->type) {
		case DHCP6_SOLICIT:	/* FALLTHROUGH */
		case DHCP6_CONFIRM:	/* FALLTHROUGH */
		case DHCP6_REBIND:
			/* Unicasting is denied for these types. */
			break;
		default:
			multicast = false;
			inet_ntop(AF_INET6, &state->unicast, uaddr,
			    sizeof(uaddr));
			break;
		}
	}
	dst.sin6_addr = multicast ? alldhcp : state->unicast;

	if (!callback) {
		logdebugx("%s: %s %s with xid 0x%02x%02x%02x%s%s",
		    ifp->name,
		    multicast ? "multicasting" : "unicasting",
		    dhcp6_get_op(state->send->type),
		    state->send->xid[0],
		    state->send->xid[1],
		    state->send->xid[2],
		    !multicast ? " " : "",
		    !multicast ? uaddr : "");
		RT = 0;
	} else {
		if (state->IMD &&
		    !(ifp->options->options & DHCPCD_INITIAL_DELAY))
			state->IMD = 0;
		if (state->IMD) {
			state->RT = state->IMD * MSEC_PER_SEC;
			/* Some buggy PPP servers close the link too early
			 * after sending an invalid status in their reply
			 * which means this host won't see it.
			 * 1 second grace seems to be the sweet spot. */
			if (ifp->flags & IFF_POINTOPOINT)
				state->RT += MSEC_PER_SEC;
		} else if (state->RTC == 0)
			state->RT = state->IRT * MSEC_PER_SEC;

		if (state->MRT != 0) {
			unsigned int mrt = state->MRT * MSEC_PER_SEC;

			if (state->RT > mrt)
				state->RT = mrt;
		}

		/* Add -.1 to .1 * RT randomness as per RFC8415 section 15 */
		uint32_t lru = arc4random_uniform(
		    state->RTC == 0 ? DHCP6_RAND_MAX
		    : DHCP6_RAND_MAX - DHCP6_RAND_MIN);
		int lr = (int)lru - (state->RTC == 0 ? 0 : DHCP6_RAND_MAX);
		RT = state->RT
		    + (unsigned int)((float)state->RT
		    * ((float)lr / DHCP6_RAND_DIV));

		if (if_is_link_up(ifp))
			logdebugx("%s: %s %s (xid 0x%02x%02x%02x)%s%s,"
			    " next in %0.1f seconds",
			    ifp->name,
			    state->IMD != 0 ? "delaying" :
			    multicast ? "multicasting" : "unicasting",
			    dhcp6_get_op(state->send->type),
			    state->send->xid[0],
			    state->send->xid[1],
			    state->send->xid[2],
			    state->IMD == 0 && !multicast ? " " : "",
			    state->IMD == 0 && !multicast ? uaddr : "",
			    (float)RT / MSEC_PER_SEC);

		/* Wait the initial delay */
		if (state->IMD != 0) {
			state->IMD = 0;
			eloop_timeout_add_msec(ctx->eloop, RT, callback, ifp);
			return 0;
		}
	}

	if (!if_is_link_up(ifp))
		return 0;

	/* Update the elapsed time */
	dhcp6_updateelapsed(ifp, state->send, state->send_len);
#ifdef AUTH
	if (ifp->options->auth.options & DHCPCD_AUTH_SEND &&
	    dhcp6_update_auth(ifp, state->send, state->send_len) == -1)
	{
		logerr("%s: %s: dhcp6_updateauth", __func__, ifp->name);
		if (errno != ESRCH)
			return -1;
	}
#endif

	/* Set the outbound interface */
	if (multicast) {
		struct cmsghdr *cm;
		struct in6_pktinfo pi = { .ipi6_ifindex = ifp->index };

		dst.sin6_scope_id = ifp->index;
		msg.msg_control = cmsgbuf.buf;
		msg.msg_controllen = sizeof(cmsgbuf.buf);
		cm = CMSG_FIRSTHDR(&msg);
		if (cm == NULL) /* unlikely */
			return -1;
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_PKTINFO;
		cm->cmsg_len = CMSG_LEN(sizeof(pi));
		memcpy(CMSG_DATA(cm), &pi, sizeof(pi));
	}

#ifdef PRIVSEP
	if (IN_PRIVSEP(ifp->ctx)) {
		if (ps_inet_senddhcp6(ifp, &msg) == -1)
			logerr(__func__);
		goto sent;
	}
#endif

	if (sendmsg(ctx->dhcp6_wfd, &msg, 0) == -1) {
		logerr("%s: %s: sendmsg", __func__, ifp->name);
		/* Allow DHCPv6 to continue .... the errors
		 * would be rate limited by the protocol.
		 * Generally the error is ENOBUFS when struggling to
		 * associate with an access point. */
	}

#ifdef PRIVSEP
sent:
#endif
	state->RTC++;
	if (callback) {
		state->RT = RT * 2;
		if (state->RT < RT) /* Check overflow */
			state->RT = RT;
		if (state->MRC == 0 || state->RTC < state->MRC)
			eloop_timeout_add_msec(ctx->eloop,
			    RT, callback, ifp);
		else if (state->MRC != 0 && state->MRCcallback)
			eloop_timeout_add_msec(ctx->eloop,
			    RT, state->MRCcallback, ifp);
		else
			logwarnx("%s: sent %d times with no reply",
			    ifp->name, state->RTC);
	}
	return 0;
}

static void
dhcp6_sendinform(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendinform);
}

static void
dhcp6_senddiscover2(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_senddiscover2);
}

static void
dhcp6_senddiscover1(void *arg)
{
	/*
	 * So the initial RT has elapsed.
	 * If we have any ADVERTs we can now REQUEST them.
	 * RFC 8415 15 and 18.2.1
	 */
	struct interface *ifp = arg;
	struct dhcp6_state *state = D6_STATE(ifp);

	if (state->recv == NULL || state->recv->type != DHCP6_ADVERTISE)
		dhcp6_sendmessage(arg, dhcp6_senddiscover2);
	else
		dhcp6_startrequest(ifp);
}

static void
dhcp6_senddiscover(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp6_state *state = D6_STATE(ifp);

	dhcp6_sendmessage(arg,
	    state->IMD != 0 ? dhcp6_senddiscover : dhcp6_senddiscover1);
}

static void
dhcp6_sendrequest(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrequest);
}

static void
dhcp6_sendrebind(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrebind);
}

static void
dhcp6_sendrenew(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrenew);
}

static void
dhcp6_sendconfirm(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendconfirm);
}

static void
dhcp6_senddecline(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_senddecline);
}

static void
dhcp6_sendrelease(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrelease);
}

static void
dhcp6_startrenew(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	ifp = arg;
	if ((state = D6_STATE(ifp)) == NULL)
		return;

	/* Only renew in the bound or renew states */
	if (state->state != DH6S_BOUND &&
	    state->state != DH6S_RENEW)
		return;

	/* Remove the timeout as the renew may have been forced. */
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_startrenew, ifp);

	state->state = DH6S_RENEW;
	state->RTC = 0;
	state->IMD = REN_MAX_DELAY;
	state->IRT = REN_TIMEOUT;
	state->MRT = REN_MAX_RT;
	state->MRC = 0;

	if (dhcp6_makemessage(ifp) == -1)
		logerr("%s: %s", __func__, ifp->name);
	else
		dhcp6_sendrenew(ifp);
}

void dhcp6_renew(struct interface *ifp)
{

	dhcp6_startrenew(ifp);
}

bool
dhcp6_dadcompleted(const struct interface *ifp)
{
	const struct dhcp6_state *state;
	const struct ipv6_addr *ap;

	state = D6_CSTATE(ifp);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->flags & IPV6_AF_ADDED &&
		    !(ap->flags & IPV6_AF_DADCOMPLETED))
			return false;
	}
	return true;
}

static void
dhcp6_dadcallback(void *arg)
{
	struct ipv6_addr *ia = arg;
	struct interface *ifp;
	struct dhcp6_state *state;
	struct ipv6_addr *ia2;
	bool completed, valid, oneduplicated;

	completed = (ia->flags & IPV6_AF_DADCOMPLETED);
	ia->flags |= IPV6_AF_DADCOMPLETED;
	if (ia->addr_flags & IN6_IFF_DUPLICATED)
		logwarnx("%s: DAD detected %s", ia->iface->name, ia->saddr);

#ifdef ND6_ADVERTISE
	else
		ipv6nd_advertise(ia);
#endif
	if (completed)
		return;

	ifp = ia->iface;
	state = D6_STATE(ifp);
	if (state->state != DH6S_BOUND && state->state != DH6S_DELEGATED)
		return;

#ifdef SMALL
	valid = true;
#else
	valid = (ia->delegating_prefix == NULL);
#endif
	completed = true;
	oneduplicated = false;
	TAILQ_FOREACH(ia2, &state->addrs, next) {
		if (ia2->flags & IPV6_AF_ADDED &&
		    !(ia2->flags & IPV6_AF_DADCOMPLETED))
		{
			completed = false;
			break;
		}
		if (DECLINE_IA(ia))
			oneduplicated = true;
	}
	if (!completed)
		return;

	logdebugx("%s: DHCPv6 DAD completed", ifp->name);

	if (oneduplicated && state->state == DH6S_BOUND) {
		dhcp6_startdecline(ifp);
		return;
	}

	script_runreason(ifp,
#ifndef SMALL
	    ia->delegating_prefix ? "DELEGATED6" :
#endif
	    state->reason);
	if (valid)
		dhcpcd_daemonise(ifp->ctx);
}

static void
dhcp6_addrequestedaddrs(struct interface *ifp)
{
	struct dhcp6_state *state;
	size_t i;
	struct if_ia *ia;
	struct ipv6_addr *a;

	state = D6_STATE(ifp);
	/* Add any requested prefixes / addresses */
	for (i = 0; i < ifp->options->ia_len; i++) {
		ia = &ifp->options->ia[i];
		if (!((ia->ia_type == D6_OPTION_IA_PD && ia->prefix_len) ||
		    !IN6_IS_ADDR_UNSPECIFIED(&ia->addr)))
			continue;
		a = ipv6_newaddr(ifp, &ia->addr,
			/*
			 * RFC 5942 Section 5
			 * We cannot assume any prefix length, nor tie the
			 * address to an existing one as it could expire
			 * before the address.
			 * As such we just give it a 128 prefix.
			 */
		    ia->ia_type == D6_OPTION_IA_PD ? ia->prefix_len : 128,
		    IPV6_AF_REQUEST);
		if (a == NULL)
			continue;
		a->dadcallback = dhcp6_dadcallback;
		memcpy(&a->iaid, &ia->iaid, sizeof(a->iaid));
		a->ia_type = ia->ia_type;
		TAILQ_INSERT_TAIL(&state->addrs, a, next);
	}
}

static void
dhcp6_startdiscover(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;
	int llevel;
	struct ipv6_addr *ia;

	ifp = arg;
	state = D6_STATE(ifp);
#ifndef SMALL
	if (state->reason == NULL || strcmp(state->reason, "TIMEOUT6") != 0)
		dhcp6_delete_delegates(ifp);
#endif
	if (state->new == NULL && !state->failed)
		llevel = LOG_INFO;
	else
		llevel = LOG_DEBUG;
	logmessage(llevel, "%s: soliciting a DHCPv6 lease", ifp->name);
	state->state = DH6S_DISCOVER;
	state->RTC = 0;
	state->IMD = SOL_MAX_DELAY;
	state->IRT = SOL_TIMEOUT;
	state->MRT = state->sol_max_rt;
	state->MRC = SOL_MAX_RC;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	free(state->new);
	state->new = NULL;
	state->new_len = 0;

	/* If we fail to renew or confirm, our requested addreses will
	 * be marked as stale.
	 To re-request them, just mark them as not stale. */
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (ia->flags & IPV6_AF_REQUEST)
			ia->flags &= ~IPV6_AF_STALE;
	}

	if (dhcp6_makemessage(ifp) == -1)
		logerr("%s: %s", __func__, ifp->name);
	else
		dhcp6_senddiscover(ifp);
}

static void
dhcp6_startinform(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;
	int llevel;

	ifp = arg;
	state = D6_STATE(ifp);
	llevel = state->failed ? LOG_DEBUG : LOG_INFO;
	logmessage(llevel, "%s: requesting DHCPv6 information", ifp->name);
	state->state = DH6S_INFORM;
	state->RTC = 0;
	state->IMD = INF_MAX_DELAY;
	state->IRT = INF_TIMEOUT;
	state->MRT = state->inf_max_rt;
	state->MRC = 0;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	if (dhcp6_makemessage(ifp) == -1) {
		logerr("%s: %s", __func__, ifp->name);
		return;
	}
	dhcp6_sendinform(ifp);
	/* RFC3315 18.1.2 says that if CONFIRM failed then the prior addresses
	 * SHOULD be used. The wording here is poor, because the addresses are
	 * merely one facet of the lease as a whole.
	 * This poor wording might explain the lack of similar text for INFORM
	 * in 18.1.5 because there are no addresses in the INFORM message. */
	eloop_timeout_add_sec(ifp->ctx->eloop,
	    INF_MAX_RD, dhcp6_failinform, ifp);
}

static bool
dhcp6_startdiscoinform(struct interface *ifp)
{
	unsigned long long opts = ifp->options->options;

	if (opts & DHCPCD_IA_FORCED || ipv6nd_hasradhcp(ifp, true))
		dhcp6_startdiscover(ifp);
	else if (opts & DHCPCD_INFORM6 || ipv6nd_hasradhcp(ifp, false))
		dhcp6_startinform(ifp);
	else
		return false;
	return true;
}

static void
dhcp6_leaseextend(struct interface *ifp)
{
	struct dhcp6_state *state = D6_STATE(ifp);
	struct ipv6_addr *ia;

	logwarnx("%s: extending DHCPv6 lease", ifp->name);
	TAILQ_FOREACH(ia, &state->addrs, next) {
		ia->flags |= IPV6_AF_EXTENDED;
		/* Set infinite lifetimes. */
		ia->prefix_pltime = ND6_INFINITE_LIFETIME;
		ia->prefix_vltime = ND6_INFINITE_LIFETIME;
	}
}

static void
dhcp6_fail(struct interface *ifp)
{
	struct dhcp6_state *state = D6_STATE(ifp);

	state->failed = true;

	/* RFC3315 18.1.2 says that prior addresses SHOULD be used on failure.
	 * RFC2131 3.2.3 says that MAY chose to use the prior address.
	 * Because dhcpcd was written first for RFC2131, we have the LASTLEASE
	 * option which defaults to off as that makes the most sense for
	 * mobile clients.
	 * dhcpcd also has LASTLEASE_EXTEND to extend this lease past it's
	 * expiry, but this is strictly not RFC compliant in any way or form. */
	if (state->new != NULL &&
	    ifp->options->options & DHCPCD_LASTLEASE_EXTEND)
	{
		dhcp6_leaseextend(ifp);
		dhcp6_bind(ifp, NULL, NULL);
	} else {
		dhcp6_freedrop_addrs(ifp, 1, NULL);
#ifndef SMALL
		dhcp6_delete_delegates(ifp);
#endif
		free(state->old);
		state->old = state->new;
		state->old_len = state->new_len;
		state->new = NULL;
		state->new_len = 0;
		if (state->old != NULL)
			script_runreason(ifp, "EXPIRE6");
		dhcp_unlink(ifp->ctx, state->leasefile);
		dhcp6_addrequestedaddrs(ifp);
	}

	if (!dhcp6_startdiscoinform(ifp)) {
		logwarnx("%s: no advertising IPv6 router wants DHCP",ifp->name);
		state->state = DH6S_INIT;
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	}
}

static int
dhcp6_failloglevel(struct interface *ifp)
{
	const struct dhcp6_state *state = D6_CSTATE(ifp);

	return state->failed ? LOG_DEBUG : LOG_ERR;
}

static void
dhcp6_failconfirm(void *arg)
{
	struct interface *ifp = arg;
	int llevel = dhcp6_failloglevel(ifp);

	logmessage(llevel, "%s: failed to confirm prior DHCPv6 address",
	    ifp->name);
	dhcp6_fail(ifp);
}

static void
dhcp6_failrequest(void *arg)
{
	struct interface *ifp = arg;
	int llevel = dhcp6_failloglevel(ifp);

	logmessage(llevel, "%s: failed to request DHCPv6 address", ifp->name);
	dhcp6_fail(ifp);
}

static void
dhcp6_failinform(void *arg)
{
	struct interface *ifp = arg;
	int llevel = dhcp6_failloglevel(ifp);

	logmessage(llevel, "%s: failed to request DHCPv6 information",
	    ifp->name);
	dhcp6_fail(ifp);
}

#ifndef SMALL
static void
dhcp6_failrebind(void *arg)
{
	struct interface *ifp = arg;

	logerrx("%s: failed to rebind prior DHCPv6 delegation", ifp->name);
	dhcp6_fail(ifp);
}

static int
dhcp6_hasprefixdelegation(struct interface *ifp)
{
	size_t i;
	uint16_t t;

	t = 0;
	for (i = 0; i < ifp->options->ia_len; i++) {
		if (t && t != ifp->options->ia[i].ia_type) {
			if (t == D6_OPTION_IA_PD ||
			    ifp->options->ia[i].ia_type == D6_OPTION_IA_PD)
				return 2;
		}
		t = ifp->options->ia[i].ia_type;
	}
	return t == D6_OPTION_IA_PD ? 1 : 0;
}
#endif

static void
dhcp6_startrebind(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;
#ifndef SMALL
	int pd;
#endif

	ifp = arg;
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_sendrenew, ifp);
	state = D6_STATE(ifp);
	if (state->state == DH6S_RENEW)
		logwarnx("%s: failed to renew DHCPv6, rebinding", ifp->name);
	else
		loginfox("%s: rebinding prior DHCPv6 lease", ifp->name);
	state->state = DH6S_REBIND;
	state->RTC = 0;
	state->MRC = 0;

#ifndef SMALL
	/* RFC 3633 section 12.1 */
	pd = dhcp6_hasprefixdelegation(ifp);
	if (pd) {
		state->IMD = CNF_MAX_DELAY;
		state->IRT = CNF_TIMEOUT;
		state->MRT = CNF_MAX_RT;
	} else
#endif
	{
		state->IMD = REB_MAX_DELAY;
		state->IRT = REB_TIMEOUT;
		state->MRT = REB_MAX_RT;
	}

	if (dhcp6_makemessage(ifp) == -1)
		logerr("%s: %s", __func__, ifp->name);
	else
		dhcp6_sendrebind(ifp);

#ifndef SMALL
	/* RFC 3633 section 12.1 */
	if (pd)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    CNF_MAX_RD, dhcp6_failrebind, ifp);
#endif
}

static void
dhcp6_startrequest(struct interface *ifp)
{
	struct dhcp6_state *state;

	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_senddiscover, ifp);
	state = D6_STATE(ifp);
	state->state = DH6S_REQUEST;
	state->RTC = 0;
	state->IMD = 0;
	state->IRT = REQ_TIMEOUT;
	state->MRT = REQ_MAX_RT;
	state->MRC = REQ_MAX_RC;
	state->MRCcallback = dhcp6_failrequest;

	if (dhcp6_makemessage(ifp) == -1) {
		logerr("%s: %s", __func__, ifp->name);
		return;
	}

	dhcp6_sendrequest(ifp);
}

static void
dhcp6_startconfirm(struct interface *ifp)
{
	struct dhcp6_state *state;
	struct ipv6_addr *ia;

	state = D6_STATE(ifp);

	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (!DECLINE_IA(ia))
			continue;
		logerrx("%s: prior DHCPv6 has a duplicated address", ifp->name);
		dhcp6_startdecline(ifp);
		return;
	}

	state->state = DH6S_CONFIRM;
	state->RTC = 0;
	state->IMD = CNF_MAX_DELAY;
	state->IRT = CNF_TIMEOUT;
	state->MRT = CNF_MAX_RT;
	state->MRC = CNF_MAX_RC;

	loginfox("%s: confirming prior DHCPv6 lease", ifp->name);

	if (dhcp6_makemessage(ifp) == -1) {
		logerr("%s: %s", __func__, ifp->name);
		return;
	}
	dhcp6_sendconfirm(ifp);
	eloop_timeout_add_sec(ifp->ctx->eloop,
	    CNF_MAX_RD, dhcp6_failconfirm, ifp);
}

static void
dhcp6_startexpire(void *arg)
{
	struct interface *ifp;

	ifp = arg;
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_sendrebind, ifp);

	logerrx("%s: DHCPv6 lease expired", ifp->name);
	dhcp6_fail(ifp);
}

static void
dhcp6_faildecline(void *arg)
{
	struct interface *ifp = arg;

	logerrx("%s: failed to decline duplicated DHCPv6 addresses", ifp->name);
	dhcp6_fail(ifp);
}

static void
dhcp6_startdecline(struct interface *ifp)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	loginfox("%s: declining failed DHCPv6 addresses", ifp->name);
	state->state = DH6S_DECLINE;
	state->RTC = 0;
	state->IMD = 0;
	state->IRT = DEC_TIMEOUT;
	state->MRT = 0;
	state->MRC = DEC_MAX_RC;
	state->MRCcallback = dhcp6_faildecline;

	if (dhcp6_makemessage(ifp) == -1)
		logerr("%s: %s", __func__, ifp->name);
	else
		dhcp6_senddecline(ifp);
}

static void
dhcp6_finishrelease(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	ifp = (struct interface *)arg;
	if ((state = D6_STATE(ifp)) != NULL) {
		state->state = DH6S_RELEASED;
		dhcp6_drop(ifp, "RELEASE6");
	}
}

static void
dhcp6_startrelease(struct interface *ifp)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state->state != DH6S_BOUND)
		return;

	state->state = DH6S_RELEASE;
	state->RTC = 0;
	state->IMD = REL_MAX_DELAY;
	state->IRT = REL_TIMEOUT;
	state->MRT = REL_MAX_RT;
	/* MRC of REL_MAX_RC is optional in RFC 3315 18.1.6 */
#if 0
	state->MRC = REL_MAX_RC;
	state->MRCcallback = dhcp6_finishrelease;
#else
	state->MRC = 0;
	state->MRCcallback = NULL;
#endif

	if (dhcp6_makemessage(ifp) == -1)
		logerr("%s: %s", __func__, ifp->name);
	else {
		dhcp6_sendrelease(ifp);
		dhcp6_finishrelease(ifp);
	}
}

static int
dhcp6_checkstatusok(const struct interface *ifp,
    struct dhcp6_message *m, uint8_t *p, size_t len)
{
	struct dhcp6_state *state;
	uint8_t *opt;
	uint16_t opt_len, code;
	size_t mlen;
	void * (*f)(void *, size_t, uint16_t, uint16_t *), *farg;
	char buf[32], *sbuf;
	const char *status;
	int loglevel;

	state = D6_STATE(ifp);
	f = p ? dhcp6_findoption : dhcp6_findmoption;
	if (p)
		farg = p;
	else
		farg = m;
	if ((opt = f(farg, len, D6_OPTION_STATUS_CODE, &opt_len)) == NULL) {
		//logdebugx("%s: no status", ifp->name);
		state->lerror = 0;
		errno = ESRCH;
		return 0;
	}

	if (opt_len < sizeof(code)) {
		logerrx("%s: status truncated", ifp->name);
		return -1;
	}
	memcpy(&code, opt, sizeof(code));
	code = ntohs(code);
	if (code == D6_STATUS_OK) {
		state->lerror = 0;
		errno = 0;
		return 0;
	}

	/* Anything after the code is a message. */
	opt += sizeof(code);
	mlen = opt_len - sizeof(code);
	if (mlen == 0) {
		sbuf = NULL;
		if (code < sizeof(dhcp6_statuses) / sizeof(char *))
			status = dhcp6_statuses[code];
		else {
			snprintf(buf, sizeof(buf), "Unknown Status (%d)", code);
			status = buf;
		}
	} else {
		if ((sbuf = malloc(mlen + 1)) == NULL) {
			logerr(__func__);
			return -1;
		}
		memcpy(sbuf, opt, mlen);
		sbuf[mlen] = '\0';
		status = sbuf;
	}

	if (state->lerror == code || state->state == DH6S_INIT)
		loglevel = LOG_DEBUG;
	else
		loglevel = LOG_ERR;
	logmessage(loglevel, "%s: DHCPv6 REPLY: %s", ifp->name, status);
	free(sbuf);
	state->lerror = code;
	errno = 0;

	/* code cannot be D6_STATUS_OK, so there is a failure */
	if (ifp->ctx->options & DHCPCD_TEST)
		eloop_exit(ifp->ctx->eloop, EXIT_FAILURE);

	return (int)code;
}

const struct ipv6_addr *
dhcp6_iffindaddr(const struct interface *ifp, const struct in6_addr *addr,
    unsigned int flags)
{
	const struct dhcp6_state *state;
	const struct ipv6_addr *ap;

	if ((state = D6_STATE(ifp)) != NULL) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ipv6_findaddrmatch(ap, addr, flags))
				return ap;
		}
	}
	return NULL;
}

struct ipv6_addr *
dhcp6_findaddr(struct dhcpcd_ctx *ctx, const struct in6_addr *addr,
    unsigned int flags)
{
	struct interface *ifp;
	struct ipv6_addr *ap;
	struct dhcp6_state *state;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if ((state = D6_STATE(ifp)) != NULL) {
			TAILQ_FOREACH(ap, &state->addrs, next) {
				if (ipv6_findaddrmatch(ap, addr, flags))
					return ap;
			}
		}
	}
	return NULL;
}

static int
dhcp6_findna(struct interface *ifp, uint16_t ot, const uint8_t *iaid,
    uint8_t *d, size_t l, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	uint8_t *o, *nd;
	uint16_t ol;
	struct ipv6_addr *a;
	int i;
	struct dhcp6_ia_addr ia;

	i = 0;
	state = D6_STATE(ifp);
	while ((o = dhcp6_findoption(d, l, D6_OPTION_IA_ADDR, &ol))) {
		/* Set d and l first to ensure we find the next option. */
		nd = o + ol;
		l -= (size_t)(nd - d);
		d = nd;
		if (ol < sizeof(ia)) {
			errno = EINVAL;
			logerrx("%s: IA Address option truncated", ifp->name);
			continue;
		}
		memcpy(&ia, o, sizeof(ia));
		ia.pltime = ntohl(ia.pltime);
		ia.vltime = ntohl(ia.vltime);
		/* RFC 3315 22.6 */
		if (ia.pltime > ia.vltime) {
			errno = EINVAL;
			logerr("%s: IA Address pltime %"PRIu32
			    " > vltime %"PRIu32,
			    ifp->name, ia.pltime, ia.vltime);
			continue;
		}
		TAILQ_FOREACH(a, &state->addrs, next) {
			if (ipv6_findaddrmatch(a, &ia.addr, 0))
				break;
		}
		if (a == NULL) {
			/*
			 * RFC 5942 Section 5
			 * We cannot assume any prefix length, nor tie the
			 * address to an existing one as it could expire
			 * before the address.
			 * As such we just give it a 128 prefix.
			 */
			a = ipv6_newaddr(ifp, &ia.addr, 128, IPV6_AF_ONLINK);
			a->dadcallback = dhcp6_dadcallback;
			a->ia_type = ot;
			memcpy(a->iaid, iaid, sizeof(a->iaid));
			a->created = *acquired;

			TAILQ_INSERT_TAIL(&state->addrs, a, next);
		} else {
			if (!(a->flags & IPV6_AF_ONLINK))
				a->flags |= IPV6_AF_ONLINK | IPV6_AF_NEW;
			a->flags &= ~(IPV6_AF_STALE | IPV6_AF_EXTENDED);
		}
		a->acquired = *acquired;
		a->prefix_pltime = ia.pltime;
		if (a->prefix_vltime != ia.vltime) {
			a->flags |= IPV6_AF_NEW;
			a->prefix_vltime = ia.vltime;
		}
		if (a->prefix_pltime && a->prefix_pltime < state->lowpl)
		    state->lowpl = a->prefix_pltime;
		if (a->prefix_vltime && a->prefix_vltime > state->expire)
		    state->expire = a->prefix_vltime;
		i++;
	}
	return i;
}

#ifndef SMALL
static int
dhcp6_findpd(struct interface *ifp, const uint8_t *iaid,
    uint8_t *d, size_t l, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	uint8_t *o, *nd;
	struct ipv6_addr *a;
	int i;
	uint8_t nb, *pw;
	uint16_t ol;
	struct dhcp6_pd_addr pdp;
	struct in6_addr pdp_prefix;

	i = 0;
	state = D6_STATE(ifp);
	while ((o = dhcp6_findoption(d, l, D6_OPTION_IAPREFIX, &ol))) {
		/* Set d and l first to ensure we find the next option. */
		nd = o + ol;
		l -= (size_t)(nd - d);
		d = nd;
		if (ol < sizeof(pdp)) {
			errno = EINVAL;
			logerrx("%s: IA Prefix option truncated", ifp->name);
			continue;
		}

		memcpy(&pdp, o, sizeof(pdp));
		pdp.pltime = ntohl(pdp.pltime);
		pdp.vltime = ntohl(pdp.vltime);
		/* RFC 3315 22.6 */
		if (pdp.pltime > pdp.vltime) {
			errno = EINVAL;
			logerrx("%s: IA Prefix pltime %"PRIu32
			    " > vltime %"PRIu32,
			    ifp->name, pdp.pltime, pdp.vltime);
			continue;
		}

		o += sizeof(pdp);
		ol = (uint16_t)(ol - sizeof(pdp));

		/* pdp.prefix is not aligned so copy it out. */
		memcpy(&pdp_prefix, &pdp.prefix, sizeof(pdp_prefix));
		TAILQ_FOREACH(a, &state->addrs, next) {
			if (IN6_ARE_ADDR_EQUAL(&a->prefix, &pdp_prefix))
				break;
		}

		if (a == NULL) {
			a = ipv6_newaddr(ifp, &pdp_prefix, pdp.prefix_len,
			    IPV6_AF_DELEGATEDPFX);
			if (a == NULL)
				break;
			a->created = *acquired;
			a->dadcallback = dhcp6_dadcallback;
			a->ia_type = D6_OPTION_IA_PD;
			memcpy(a->iaid, iaid, sizeof(a->iaid));
			TAILQ_INSERT_TAIL(&state->addrs, a, next);
		} else {
			if (!(a->flags & IPV6_AF_DELEGATEDPFX))
				a->flags |= IPV6_AF_NEW | IPV6_AF_DELEGATEDPFX;
			a->flags &= ~(IPV6_AF_STALE | IPV6_AF_EXTENDED);
			if (a->prefix_vltime != pdp.vltime)
				a->flags |= IPV6_AF_NEW;
		}

		a->acquired = *acquired;
		a->prefix_pltime = pdp.pltime;
		a->prefix_vltime = pdp.vltime;

		if (a->prefix_pltime && a->prefix_pltime < state->lowpl)
			state->lowpl = a->prefix_pltime;
		if (a->prefix_vltime && a->prefix_vltime > state->expire)
			state->expire = a->prefix_vltime;
		i++;

		a->prefix_exclude_len = 0;
		memset(&a->prefix_exclude, 0, sizeof(a->prefix_exclude));
		o = dhcp6_findoption(o, ol, D6_OPTION_PD_EXCLUDE, &ol);
		if (o == NULL)
			continue;

		/* RFC 6603 4.2 says option length MUST be between 2 and 17.
		 * This allows 1 octet for prefix length and 16 for the
		 * subnet ID. */
		if (ol < 2 || ol > 17) {
			logerrx("%s: invalid PD Exclude option", ifp->name);
			continue;
		}

		/* RFC 6603 4.2 says prefix length MUST be between the
		 * length of the IAPREFIX prefix length + 1 and 128. */
		if (*o < a->prefix_len + 1 || *o > 128) {
			logerrx("%s: invalid PD Exclude length", ifp->name);
			continue;
		}

		ol--;
		/* Check option length matches prefix length. */
		if (((*o - a->prefix_len - 1) / NBBY) + 1 != ol) {
			logerrx("%s: PD Exclude length mismatch", ifp->name);
			continue;
		}
		a->prefix_exclude_len = *o++;

		memcpy(&a->prefix_exclude, &a->prefix,
		    sizeof(a->prefix_exclude));
		nb = a->prefix_len % NBBY;
		if (nb)
			ol--;
		pw = a->prefix_exclude.s6_addr +
		    (a->prefix_exclude_len / NBBY) - 1;
		while (ol-- > 0)
			*pw-- = *o++;
		if (nb)
			*pw = (uint8_t)(*pw | (*o >> nb));
	}
	return i;
}
#endif

static int
dhcp6_findia(struct interface *ifp, struct dhcp6_message *m, size_t l,
    const char *sfrom, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	const struct if_options *ifo;
	struct dhcp6_option o;
	uint8_t *d, *p;
	struct dhcp6_ia_na ia;
	int i, e, error;
	size_t j;
	uint16_t nl;
	uint8_t iaid[4];
	char buf[sizeof(iaid) * 3];
	struct ipv6_addr *ap;
	struct if_ia *ifia;

	if (l < sizeof(*m)) {
		/* Should be impossible with guards at packet in
		 * and reading leases */
		errno = EINVAL;
		return -1;
	}

	ifo = ifp->options;
	i = e = 0;
	state = D6_STATE(ifp);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (!(ap->flags & IPV6_AF_DELEGATED))
			ap->flags |= IPV6_AF_STALE;
	}

	d = (uint8_t *)m + sizeof(*m);
	l -= sizeof(*m);
	while (l > sizeof(o)) {
		memcpy(&o, d, sizeof(o));
		o.len = ntohs(o.len);
		if (o.len > l || sizeof(o) + o.len > l) {
			errno = EINVAL;
			logerrx("%s: option overflow", ifp->name);
			break;
		}
		p = d + sizeof(o);
		d = p + o.len;
		l -= sizeof(o) + o.len;

		o.code = ntohs(o.code);
		switch(o.code) {
		case D6_OPTION_IA_TA:
			nl = 4;
			break;
		case D6_OPTION_IA_NA:
		case D6_OPTION_IA_PD:
			nl = 12;
			break;
		default:
			continue;
		}
		if (o.len < nl) {
			errno = EINVAL;
			logerrx("%s: IA option truncated", ifp->name);
			continue;
		}

		memcpy(&ia, p, nl);
		p += nl;
		o.len = (uint16_t)(o.len - nl);

		for (j = 0; j < ifo->ia_len; j++) {
			ifia = &ifo->ia[j];
			if (ifia->ia_type == o.code &&
			    memcmp(ifia->iaid, ia.iaid, sizeof(ia.iaid)) == 0)
				break;
		}
		if (j == ifo->ia_len &&
		    !(ifo->ia_len == 0 && ifp->ctx->options & DHCPCD_DUMPLEASE))
		{
			logdebugx("%s: ignoring unrequested IAID %s",
			    ifp->name,
			    hwaddr_ntoa(ia.iaid, sizeof(ia.iaid),
			    buf, sizeof(buf)));
			continue;
		}

		if (o.code != D6_OPTION_IA_TA) {
			ia.t1 = ntohl(ia.t1);
			ia.t2 = ntohl(ia.t2);
			/* RFC 3315 22.4 */
			if (ia.t2 > 0 && ia.t1 > ia.t2) {
				logwarnx("%s: IAID %s T1(%d) > T2(%d) from %s",
				    ifp->name,
				    hwaddr_ntoa(iaid, sizeof(iaid), buf,
						sizeof(buf)),
				    ia.t1, ia.t2, sfrom);
				continue;
			}
		} else
			ia.t1 = ia.t2 = 0; /* appease gcc */
		if ((error = dhcp6_checkstatusok(ifp, NULL, p, o.len)) != 0) {
			if (error == D6_STATUS_NOBINDING)
				state->has_no_binding = true;
			e = 1;
			continue;
		}
		if (o.code == D6_OPTION_IA_PD) {
#ifndef SMALL
			if (dhcp6_findpd(ifp, ia.iaid, p, o.len,
					 acquired) == 0)
			{
				logwarnx("%s: %s: DHCPv6 REPLY missing Prefix",
				    ifp->name, sfrom);
				continue;
			}
#endif
		} else {
			if (dhcp6_findna(ifp, o.code, ia.iaid, p, o.len,
					 acquired) == 0)
			{
				logwarnx("%s: %s: DHCPv6 REPLY missing "
				    "IA Address",
				    ifp->name, sfrom);
				continue;
			}
		}
		if (o.code != D6_OPTION_IA_TA) {
			if (ia.t1 != 0 &&
			    (ia.t1 < state->renew || state->renew == 0))
				state->renew = ia.t1;
			if (ia.t2 != 0 &&
			    (ia.t2 < state->rebind || state->rebind == 0))
				state->rebind = ia.t2;
		}
		i++;
	}

	if (i == 0 && e)
		return -1;
	return i;
}

#ifndef SMALL
static void
dhcp6_deprecatedele(struct ipv6_addr *ia)
{
	struct ipv6_addr *da, *dan, *dda;
	struct timespec now;
	struct dhcp6_state *state;

	timespecclear(&now);
	TAILQ_FOREACH_SAFE(da, &ia->pd_pfxs, pd_next, dan) {
		if (ia->prefix_vltime == 0) {
			if (da->prefix_vltime != 0)
				da->prefix_vltime = 0;
			else
				continue;
		} else if (da->prefix_pltime != 0)
			da->prefix_pltime = 0;
		else
			continue;

		if (ipv6_doaddr(da, &now) != -1)
			continue;

		/* Delegation deleted, forget it. */
		TAILQ_REMOVE(&ia->pd_pfxs, da, pd_next);

		/* Delete it from the interface. */
		state = D6_STATE(da->iface);
		TAILQ_FOREACH(dda, &state->addrs, next) {
			if (IN6_ARE_ADDR_EQUAL(&dda->addr, &da->addr))
				break;
		}
		if (dda != NULL) {
			TAILQ_REMOVE(&state->addrs, dda, next);
			ipv6_freeaddr(dda);
		}
	}
}
#endif

static void
dhcp6_deprecateaddrs(struct ipv6_addrhead *addrs)
{
	struct ipv6_addr *ia, *ian;

	TAILQ_FOREACH_SAFE(ia, addrs, next, ian) {
		if (ia->flags & IPV6_AF_EXTENDED)
			;
		else if (ia->flags & IPV6_AF_STALE) {
			if (ia->prefix_vltime != 0)
				logdebugx("%s: %s: became stale",
				    ia->iface->name, ia->saddr);
			/* Technically this violates RFC 8415 18.2.10.1,
			 * but we need a mechanism to tell the kernel to
			 * try and prefer other addresses. */
			ia->prefix_pltime = 0;
		} else if (ia->prefix_vltime == 0)
			loginfox("%s: %s: no valid lifetime",
			    ia->iface->name, ia->saddr);
		else
			continue;

#ifndef SMALL
		/* If we delegated from this prefix, deprecate or remove
		 * the delegations. */
		if (ia->flags & IPV6_AF_DELEGATEDPFX)
			dhcp6_deprecatedele(ia);
#endif

		if (ia->flags & IPV6_AF_REQUEST) {
			ia->prefix_vltime = ia->prefix_pltime = 0;
			eloop_q_timeout_delete(ia->iface->ctx->eloop,
			    ELOOP_QUEUE_ALL, NULL, ia);
			continue;
		}
		TAILQ_REMOVE(addrs, ia, next);
		if (ia->flags & IPV6_AF_EXTENDED)
			ipv6_deleteaddr(ia);
		ipv6_freeaddr(ia);
	}
}

static int
dhcp6_validatelease(struct interface *ifp,
    struct dhcp6_message *m, size_t len,
    const char *sfrom, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	int nia, ok_errno;
	struct timespec aq;

	if (len <= sizeof(*m)) {
		logerrx("%s: DHCPv6 lease truncated", ifp->name);
		return -1;
	}

	state = D6_STATE(ifp);
	errno = 0;
	if (dhcp6_checkstatusok(ifp, m, NULL, len) != 0)
		return -1;
	ok_errno = errno;

	state->renew = state->rebind = state->expire = 0;
	state->lowpl = ND6_INFINITE_LIFETIME;
	if (!acquired) {
		clock_gettime(CLOCK_MONOTONIC, &aq);
		acquired = &aq;
	}
	state->has_no_binding = false;
	nia = dhcp6_findia(ifp, m, len, sfrom, acquired);
	if (nia == 0 && state->state == DH6S_CONFIRM && ok_errno == 0 &&
	    state->new && state->new_len)
	{
		state->has_no_binding = false;
		nia = dhcp6_findia(ifp, state->new, state->new_len,
		    sfrom, acquired);
	}
	if (nia == 0) {
		logerrx("%s: no useable IA found in lease", ifp->name);
		return -1;
	}
	return nia;
}

static ssize_t
dhcp6_readlease(struct interface *ifp, int validate)
{
	union {
		struct dhcp6_message dhcp6;
		uint8_t buf[UDPLEN_MAX];
	} buf;
	struct dhcp6_state *state;
	ssize_t bytes;
	int fd;
	time_t mtime, now;
#ifdef AUTH
	uint8_t *o;
	uint16_t ol;
#endif

	state = D6_STATE(ifp);
	if (state->leasefile[0] == '\0') {
		logdebugx("reading standard input");
		bytes = read(fileno(stdin), buf.buf, sizeof(buf.buf));
	} else {
		logdebugx("%s: reading lease: %s",
		    ifp->name, state->leasefile);
		bytes = dhcp_readfile(ifp->ctx, state->leasefile,
		    buf.buf, sizeof(buf.buf));
	}
	if (bytes == -1)
		goto ex;

	if (ifp->ctx->options & DHCPCD_DUMPLEASE || state->leasefile[0] == '\0')
		goto out;

	if (bytes == 0)
		goto ex;

	/* If not validating IA's and if they have expired,
	 * skip to the auth check. */
	if (!validate)
		goto auth;

	if (dhcp_filemtime(ifp->ctx, state->leasefile, &mtime) == -1)
		goto ex;
	clock_gettime(CLOCK_MONOTONIC, &state->acquired);
	if ((now = time(NULL)) == -1)
		goto ex;
	state->acquired.tv_sec -= now - mtime;

	/* Check to see if the lease is still valid */
	fd = dhcp6_validatelease(ifp, &buf.dhcp6, (size_t)bytes, NULL,
	    &state->acquired);
	if (fd == -1) {
		bytes = 0; /* We have already reported the error */
		goto ex;
	}

	if (state->expire != ND6_INFINITE_LIFETIME &&
	    (time_t)state->expire < now - mtime &&
	    !(ifp->options->options & DHCPCD_LASTLEASE_EXTEND))
	{
		logdebugx("%s: discarding expired lease", ifp->name);
		bytes = 0;
		goto ex;
	}

auth:
#ifdef AUTH
	/* Authenticate the message */
	o = dhcp6_findmoption(&buf.dhcp6, (size_t)bytes, D6_OPTION_AUTH, &ol);
	if (o) {
		if (dhcp_auth_validate(&state->auth, &ifp->options->auth,
		    buf.buf, (size_t)bytes, 6, buf.dhcp6.type, o, ol) == NULL)
		{
			logerr("%s: authentication failed", ifp->name);
			bytes = 0;
			goto ex;
		}
		if (state->auth.token)
			logdebugx("%s: validated using 0x%08" PRIu32,
			    ifp->name, state->auth.token->secretid);
		else
			loginfox("%s: accepted reconfigure key", ifp->name);
	} else if ((ifp->options->auth.options & DHCPCD_AUTH_SENDREQUIRE) ==
	    DHCPCD_AUTH_SENDREQUIRE)
	{
		logerrx("%s: authentication now required", ifp->name);
		goto ex;
	}
#endif

out:
	free(state->new);
	state->new = malloc((size_t)bytes);
	if (state->new == NULL) {
		logerr(__func__);
		goto ex;
	}

	memcpy(state->new, buf.buf, (size_t)bytes);
	state->new_len = (size_t)bytes;
	return bytes;

ex:
	dhcp6_freedrop_addrs(ifp, 0, NULL);
	dhcp_unlink(ifp->ctx, state->leasefile);
	free(state->new);
	state->new = NULL;
	state->new_len = 0;
	dhcp6_addrequestedaddrs(ifp);
	return bytes == 0 ? 0 : -1;
}

static void
dhcp6_startinit(struct interface *ifp)
{
	struct dhcp6_state *state;
	ssize_t r;
	uint8_t has_ta, has_non_ta;
	size_t i;

	state = D6_STATE(ifp);
	state->state = DH6S_INIT;
	state->expire = ND6_INFINITE_LIFETIME;
	state->lowpl = ND6_INFINITE_LIFETIME;

	dhcp6_addrequestedaddrs(ifp);
	has_ta = has_non_ta = 0;
	for (i = 0; i < ifp->options->ia_len; i++) {
		switch (ifp->options->ia[i].ia_type) {
		case D6_OPTION_IA_TA:
			has_ta = 1;
			break;
		default:
			has_non_ta = 1;
		}
	}

	if (!(ifp->ctx->options & DHCPCD_TEST) &&
	    !(has_ta && !has_non_ta) &&
	    ifp->options->reboot != 0)
	{
		r = dhcp6_readlease(ifp, 1);
		if (r == -1) {
			if (errno != ENOENT && errno != ESRCH)
				logerr("%s: %s", __func__, state->leasefile);
		} else if (r != 0 &&
		    !(ifp->options->options & DHCPCD_ANONYMOUS))
		{
			/* RFC 3633 section 12.1 */
#ifndef SMALL
			if (dhcp6_hasprefixdelegation(ifp))
				dhcp6_startrebind(ifp);
			else
#endif
				dhcp6_startconfirm(ifp);
			return;
		}
	}
	dhcp6_startdiscoinform(ifp);
}

#ifndef SMALL
static struct ipv6_addr *
dhcp6_ifdelegateaddr(struct interface *ifp, struct ipv6_addr *prefix,
    const struct if_sla *sla, struct if_ia *if_ia)
{
	struct dhcp6_state *state;
	struct in6_addr addr, daddr;
	struct ipv6_addr *ia;
	int pfxlen, dadcounter;
	uint64_t vl;

	/* RFC6603 Section 4.2 */
	if (strcmp(ifp->name, prefix->iface->name) == 0) {
		if (prefix->prefix_exclude_len == 0) {
			/* Don't spam the log automatically */
			if (sla != NULL)
				logwarnx("%s: DHCPv6 server does not support "
				    "OPTION_PD_EXCLUDE",
				    ifp->name);
			return NULL;
		}
		pfxlen = prefix->prefix_exclude_len;
		memcpy(&addr, &prefix->prefix_exclude, sizeof(addr));
	} else if ((pfxlen = dhcp6_delegateaddr(&addr, ifp, prefix,
	    sla, if_ia)) == -1)
		return NULL;

	if (sla != NULL && fls64(sla->suffix) > 128 - pfxlen) {
		logerrx("%s: suffix %" PRIu64 " + prefix_len %d > 128",
		    ifp->name, sla->suffix, pfxlen);
		return NULL;
	}

	/* Add our suffix */
	if (sla != NULL && sla->suffix != 0) {
		daddr = addr;
		vl = be64dec(addr.s6_addr + 8);
		vl |= sla->suffix;
		be64enc(daddr.s6_addr + 8, vl);
	} else {
		dadcounter = ipv6_makeaddr(&daddr, ifp, &addr, pfxlen, 0);
		if (dadcounter == -1) {
			logerrx("%s: error adding slaac to prefix_len %d",
			    ifp->name, pfxlen);
			return NULL;
		}
	}

	/* Find an existing address */
	state = D6_STATE(ifp);
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (IN6_ARE_ADDR_EQUAL(&ia->addr, &daddr))
			break;
	}
	if (ia == NULL) {
		ia = ipv6_newaddr(ifp, &daddr, (uint8_t)pfxlen, IPV6_AF_ONLINK);
		if (ia == NULL)
			return NULL;
		ia->dadcallback = dhcp6_dadcallback;
		memcpy(&ia->iaid, &prefix->iaid, sizeof(ia->iaid));
		ia->created = prefix->acquired;

		TAILQ_INSERT_TAIL(&state->addrs, ia, next);
		TAILQ_INSERT_TAIL(&prefix->pd_pfxs, ia, pd_next);
	}
	ia->delegating_prefix = prefix;
	ia->prefix = addr;
	ia->prefix_len = (uint8_t)pfxlen;
	ia->acquired = prefix->acquired;
	ia->prefix_pltime = prefix->prefix_pltime;
	ia->prefix_vltime = prefix->prefix_vltime;

	/* If the prefix length hasn't changed,
	 * don't install a reject route. */
	if (prefix->prefix_len == pfxlen)
		prefix->flags |= IPV6_AF_NOREJECT;
	else
		prefix->flags &= ~IPV6_AF_NOREJECT;

	return ia;
}
#endif

static void
dhcp6_script_try_run(struct interface *ifp, int delegated)
{
	struct dhcp6_state *state;
	struct ipv6_addr *ap;
	int completed;

	state = D6_STATE(ifp);
	completed = 1;
	/* If all addresses have completed DAD run the script */
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (!(ap->flags & IPV6_AF_ADDED))
			continue;
		if (ap->flags & IPV6_AF_ONLINK) {
			if (!(ap->flags & IPV6_AF_DADCOMPLETED) &&
			    ipv6_iffindaddr(ap->iface, &ap->addr,
					    IN6_IFF_TENTATIVE))
				ap->flags |= IPV6_AF_DADCOMPLETED;
			if ((ap->flags & IPV6_AF_DADCOMPLETED) == 0
#ifndef SMALL
			    && ((delegated && ap->delegating_prefix) ||
			    (!delegated && !ap->delegating_prefix))
#endif
			    )
			{
				completed = 0;
				break;
			}
		}
	}
	if (completed) {
		script_runreason(ifp, delegated ? "DELEGATED6" : state->reason);
		if (!delegated)
			dhcpcd_daemonise(ifp->ctx);
	} else
		logdebugx("%s: waiting for DHCPv6 DAD to complete", ifp->name);
}

#ifdef SMALL
size_t
dhcp6_find_delegates(__unused struct interface *ifp)
{

	return 0;
}
#else
static void
dhcp6_delegate_prefix(struct interface *ifp)
{
	struct if_options *ifo;
	struct dhcp6_state *state;
	struct ipv6_addr *ap;
	size_t i, j, k;
	struct if_ia *ia;
	struct if_sla *sla;
	struct interface *ifd;
	bool carrier_warned;

	ifo = ifp->options;
	state = D6_STATE(ifp);

	/* Clear the logged flag. */
	TAILQ_FOREACH(ap, &state->addrs, next) {
		ap->flags &= ~IPV6_AF_DELEGATEDLOG;
	}

	TAILQ_FOREACH(ifd, ifp->ctx->ifaces, next) {
		if (!ifd->active)
			continue;
		if (!(ifd->options->options & DHCPCD_CONFIGURE))
			continue;
		k = 0;
		carrier_warned = false;
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (!(ap->flags & IPV6_AF_DELEGATEDPFX))
				continue;
			if (!(ap->flags & IPV6_AF_DELEGATEDLOG)) {
				int loglevel;

				if (ap->flags & IPV6_AF_NEW)
					loglevel = LOG_INFO;
				else
					loglevel = LOG_DEBUG;
				/* We only want to log this the once as we loop
				 * through many interfaces first. */
				ap->flags |= IPV6_AF_DELEGATEDLOG;
				logmessage(loglevel, "%s: delegated prefix %s",
				    ifp->name, ap->saddr);
				ap->flags &= ~IPV6_AF_NEW;
			}
			for (i = 0; i < ifo->ia_len; i++) {
				ia = &ifo->ia[i];
				if (ia->ia_type != D6_OPTION_IA_PD)
					continue;
				if (memcmp(ia->iaid, ap->iaid,
				    sizeof(ia->iaid)))
					continue;
				if (ia->sla_len == 0) {
					/* no SLA configured, so lets
					 * automate it */
					if (!if_is_link_up(ifd)) {
						logdebugx(
						    "%s: has no carrier, cannot"
						    " delegate addresses",
						    ifd->name);
						carrier_warned = true;
						break;
					}
					if (dhcp6_ifdelegateaddr(ifd, ap,
					    NULL, ia))
						k++;
				}
				for (j = 0; j < ia->sla_len; j++) {
					sla = &ia->sla[j];
					if (strcmp(ifd->name, sla->ifname))
						continue;
					if (!if_is_link_up(ifd)) {
						logdebugx(
						    "%s: has no carrier, cannot"
						    " delegate addresses",
						    ifd->name);
						carrier_warned = true;
						break;
					}
					if (dhcp6_ifdelegateaddr(ifd, ap,
					    sla, ia))
						k++;
				}
				if (carrier_warned)
					break;
			}
			if (carrier_warned)
				break;
		}
		if (k && !carrier_warned) {
			struct dhcp6_state *s = D6_STATE(ifd);

			ipv6_addaddrs(&s->addrs);
			dhcp6_script_try_run(ifd, 1);
		}
	}

	/* Now all addresses have been added, rebuild the routing table. */
	rt_build(ifp->ctx, AF_INET6);
}

static void
dhcp6_find_delegates1(void *arg)
{

	dhcp6_find_delegates(arg);
}

size_t
dhcp6_find_delegates(struct interface *ifp)
{
	struct if_options *ifo;
	struct dhcp6_state *state;
	struct ipv6_addr *ap;
	size_t i, j, k;
	struct if_ia *ia;
	struct if_sla *sla;
	struct interface *ifd;

	if (ifp->options != NULL &&
	    !(ifp->options->options & DHCPCD_CONFIGURE))
		return 0;

	k = 0;
	TAILQ_FOREACH(ifd, ifp->ctx->ifaces, next) {
		ifo = ifd->options;
		state = D6_STATE(ifd);
		if (state == NULL || state->state != DH6S_BOUND)
			continue;
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (!(ap->flags & IPV6_AF_DELEGATEDPFX))
				continue;
			for (i = 0; i < ifo->ia_len; i++) {
				ia = &ifo->ia[i];
				if (ia->ia_type != D6_OPTION_IA_PD)
					continue;
				if (memcmp(ia->iaid, ap->iaid,
				    sizeof(ia->iaid)))
					continue;
				for (j = 0; j < ia->sla_len; j++) {
					sla = &ia->sla[j];
					if (strcmp(ifp->name, sla->ifname))
						continue;
					if (ipv6_linklocal(ifp) == NULL) {
						logdebugx(
						    "%s: delaying adding"
						    " delegated addresses for"
						    " LL address",
						    ifp->name);
						ipv6_addlinklocalcallback(ifp,
						    dhcp6_find_delegates1, ifp);
						return 1;
					}
					if (dhcp6_ifdelegateaddr(ifp, ap,
					    sla, ia))
					    k++;
				}
			}
		}
	}

	if (k) {
		loginfox("%s: adding delegated prefixes", ifp->name);
		state = D6_STATE(ifp);
		state->state = DH6S_DELEGATED;
		ipv6_addaddrs(&state->addrs);
		rt_build(ifp->ctx, AF_INET6);
		dhcp6_script_try_run(ifp, 1);
	}
	return k;
}
#endif

static void
dhcp6_bind(struct interface *ifp, const char *op, const char *sfrom)
{
	struct dhcp6_state *state = D6_STATE(ifp);
	bool timedout = (op == NULL), confirmed;
	struct ipv6_addr *ia;
	int loglevel;
	struct timespec now;

	if (state->state == DH6S_RENEW) {
		loglevel = LOG_DEBUG;
		TAILQ_FOREACH(ia, &state->addrs, next) {
			if (ia->flags & IPV6_AF_NEW) {
				loglevel = LOG_INFO;
				break;
			}
		}
	} else if (state->state == DH6S_INFORM)
		loglevel = state->new_start ? LOG_INFO : LOG_DEBUG;
	else
		loglevel = LOG_INFO;
	state->new_start = false;

	if (!timedout) {
		logmessage(loglevel, "%s: %s received from %s",
		    ifp->name, op, sfrom);
#ifndef SMALL
		/* If we delegated from an unconfirmed lease we MUST drop
		 * them now. Hopefully we have new delegations. */
		if (state->reason != NULL &&
		    strcmp(state->reason, "TIMEOUT6") == 0)
			dhcp6_delete_delegates(ifp);
#endif
		state->reason = NULL;
	} else
		state->reason = "TIMEOUT6";

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	clock_gettime(CLOCK_MONOTONIC, &now);

	switch(state->state) {
	case DH6S_INFORM:
	{
		struct dhcp6_option *o;
		uint16_t ol;

		if (state->reason == NULL)
			state->reason = "INFORM6";
		o = dhcp6_findmoption(state->new, state->new_len,
				      D6_OPTION_INFO_REFRESH_TIME, &ol);
		if (o == NULL || ol != sizeof(uint32_t))
			state->renew = IRT_DEFAULT;
		else {
			memcpy(&state->renew, o, ol);
			state->renew = ntohl(state->renew);
			if (state->renew < IRT_MINIMUM)
				state->renew = IRT_MINIMUM;
		}
		state->rebind = 0;
		state->expire = ND6_INFINITE_LIFETIME;
		state->lowpl = ND6_INFINITE_LIFETIME;
	}
		break;

	case DH6S_REQUEST:
		if (state->reason == NULL)
			state->reason = "BOUND6";
		/* FALLTHROUGH */
	case DH6S_RENEW:
		if (state->reason == NULL)
			state->reason = "RENEW6";
		/* FALLTHROUGH */
	case DH6S_REBIND:
		if (state->reason == NULL)
			state->reason = "REBIND6";
		/* FALLTHROUGH */
	case DH6S_CONFIRM:
		if (state->reason == NULL)
			state->reason = "REBOOT6";
		if (state->renew != 0) {
			bool all_expired = true;

			TAILQ_FOREACH(ia, &state->addrs, next) {
				if (ia->flags & IPV6_AF_STALE)
					continue;
				if (!(state->renew == ND6_INFINITE_LIFETIME
				    && ia->prefix_vltime == ND6_INFINITE_LIFETIME)
				    && ia->prefix_vltime != 0
				    && ia->prefix_vltime <= state->renew)
					logwarnx(
					    "%s: %s will expire before renewal",
					    ifp->name, ia->saddr);
				else
					all_expired = false;
			}
			if (all_expired) {
				/* All address's vltime happens at or before
				 * the configured T1 in the IA.
				 * This is a badly configured server and we
				 * have to use our own notion of what
				 * T1 and T2 should be as a result.
				 *
				 * Doing this violates RFC 3315 22.4:
				 * In a message sent by a server to a client,
				 * the client MUST use the values in the T1
				 * and T2 fields for the T1 and T2 parameters,
				 * unless those values in those fields are 0.
				 */
				logwarnx("%s: ignoring T1 %"PRIu32
				    " due to address expiry",
				    ifp->name, state->renew);
				state->renew = state->rebind = 0;
			}
		}
		if (state->renew == 0 && state->lowpl != ND6_INFINITE_LIFETIME)
			state->renew = (uint32_t)(state->lowpl * 0.5);
		if (state->rebind == 0 && state->lowpl != ND6_INFINITE_LIFETIME)
			state->rebind = (uint32_t)(state->lowpl * 0.8);
		break;
	default:
		state->reason = "UNKNOWN6";
		break;
	}

	if (state->state != DH6S_CONFIRM && !timedout) {
		state->acquired = now;
		free(state->old);
		state->old = state->new;
		state->old_len = state->new_len;
		state->new = state->recv;
		state->new_len = state->recv_len;
		state->recv = NULL;
		state->recv_len = 0;
		confirmed = false;
	} else {
		/* Reduce timers based on when we got the lease. */
		uint32_t elapsed;

		elapsed = (uint32_t)eloop_timespec_diff(&now,
		    &state->acquired, NULL);
		if (state->renew && state->renew != ND6_INFINITE_LIFETIME) {
			if (state->renew > elapsed)
				state->renew -= elapsed;
			else
				state->renew = 0;
		}
		if (state->rebind && state->rebind != ND6_INFINITE_LIFETIME) {
			if (state->rebind > elapsed)
				state->rebind -= elapsed;
			else
				state->rebind = 0;
		}
		if (state->expire && state->expire != ND6_INFINITE_LIFETIME) {
			if (state->expire > elapsed)
				state->expire -= elapsed;
			else
				state->expire = 0;
		}
		confirmed = true;
	}

	if (ifp->ctx->options & DHCPCD_TEST)
		script_runreason(ifp, "TEST");
	else {
		if (state->state == DH6S_INFORM)
			state->state = DH6S_INFORMED;
		else
			state->state = DH6S_BOUND;
		state->failed = false;

		if (state->renew && state->renew != ND6_INFINITE_LIFETIME)
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    state->renew,
			    state->state == DH6S_INFORMED ?
			    dhcp6_startinform : dhcp6_startrenew, ifp);
		if (state->rebind && state->rebind != ND6_INFINITE_LIFETIME)
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    state->rebind, dhcp6_startrebind, ifp);
		if (state->expire != ND6_INFINITE_LIFETIME)
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    state->expire, dhcp6_startexpire, ifp);

		if (ifp->options->options & DHCPCD_CONFIGURE) {
			ipv6_addaddrs(&state->addrs);
			if (!timedout)
				dhcp6_deprecateaddrs(&state->addrs);
		}

		if (state->state == DH6S_INFORMED)
			logmessage(loglevel, "%s: refresh in %"PRIu32" seconds",
			    ifp->name, state->renew);
		else if (state->renew == ND6_INFINITE_LIFETIME)
			logmessage(loglevel, "%s: leased for infinity",
			    ifp->name);
		else if (state->renew || state->rebind)
			logmessage(loglevel, "%s: renew in %"PRIu32", "
			    "rebind in %"PRIu32", "
			    "expire in %"PRIu32" seconds",
			    ifp->name,
			    state->renew, state->rebind, state->expire);
		else if (state->expire == 0)
			logmessage(loglevel, "%s: will expire", ifp->name);
		else
			logmessage(loglevel, "%s: expire in %"PRIu32" seconds",
			    ifp->name, state->expire);
		rt_build(ifp->ctx, AF_INET6);
		if (!confirmed && !timedout) {
			logdebugx("%s: writing lease: %s",
			    ifp->name, state->leasefile);
			if (dhcp_writefile(ifp->ctx, state->leasefile, 0640,
			    state->new, state->new_len) == -1)
				logerr("dhcp_writefile: %s",state->leasefile);
		}
#ifndef SMALL
		dhcp6_delegate_prefix(ifp);
#endif
		dhcp6_script_try_run(ifp, 0);
	}

	if (ifp->ctx->options & DHCPCD_TEST ||
	    (ifp->options->options & DHCPCD_INFORM &&
	    !(ifp->ctx->options & DHCPCD_MANAGER)))
	{
		eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
	}
}

static void
dhcp6_recvif(struct interface *ifp, const char *sfrom,
    struct dhcp6_message *r, size_t len)
{
	struct dhcpcd_ctx *ctx;
	size_t i;
	const char *op;
	struct dhcp6_state *state;
	uint8_t *o;
	uint16_t ol;
	const struct dhcp_opt *opt;
	const struct if_options *ifo;
	bool valid_op;
#ifdef AUTH
	uint8_t *auth;
	uint16_t auth_len;
#endif

	ctx = ifp->ctx;
	state = D6_STATE(ifp);
	if (state == NULL || state->send == NULL) {
		logdebugx("%s: DHCPv6 reply received but not running",
		    ifp->name);
		return;
	}

	/* We're already bound and this message is for another machine */
	/* XXX DELEGATED? */
	if (r->type != DHCP6_RECONFIGURE &&
	    (state->state == DH6S_BOUND || state->state == DH6S_INFORMED))
	{
		logdebugx("%s: DHCPv6 reply received but already bound",
		    ifp->name);
		return;
	}

	if (dhcp6_findmoption(r, len, D6_OPTION_SERVERID, NULL) == NULL) {
		logdebugx("%s: no DHCPv6 server ID from %s", ifp->name, sfrom);
		return;
	}

	ifo = ifp->options;
	for (i = 0, opt = ctx->dhcp6_opts;
	    i < ctx->dhcp6_opts_len;
	    i++, opt++)
	{
		if (has_option_mask(ifo->requiremask6, opt->option) &&
		    !dhcp6_findmoption(r, len, (uint16_t)opt->option, NULL))
		{
			logwarnx("%s: reject DHCPv6 (no option %s) from %s",
			    ifp->name, opt->var, sfrom);
			return;
		}
		if (has_option_mask(ifo->rejectmask6, opt->option) &&
		    dhcp6_findmoption(r, len, (uint16_t)opt->option, NULL))
		{
			logwarnx("%s: reject DHCPv6 (option %s) from %s",
			    ifp->name, opt->var, sfrom);
			return;
		}
	}

#ifdef AUTH
	/* Authenticate the message */
	auth = dhcp6_findmoption(r, len, D6_OPTION_AUTH, &auth_len);
	if (auth != NULL) {
		if (dhcp_auth_validate(&state->auth, &ifo->auth,
		    (uint8_t *)r, len, 6, r->type, auth, auth_len) == NULL)
		{
			logerr("%s: authentication failed from %s",
			    ifp->name, sfrom);
			return;
		}
		if (state->auth.token)
			logdebugx("%s: validated using 0x%08" PRIu32,
			    ifp->name, state->auth.token->secretid);
		else
			loginfox("%s: accepted reconfigure key", ifp->name);
	} else if (ifo->auth.options & DHCPCD_AUTH_SEND) {
		if (ifo->auth.options & DHCPCD_AUTH_REQUIRE) {
			logerrx("%s: no authentication from %s",
			    ifp->name, sfrom);
			return;
		}
		logwarnx("%s: no authentication from %s", ifp->name, sfrom);
	}
#endif

	op = dhcp6_get_op(r->type);
	valid_op = op != NULL;
	switch(r->type) {
	case DHCP6_REPLY:
		switch(state->state) {
		case DH6S_INFORM:
			if (dhcp6_checkstatusok(ifp, r, NULL, len) != 0)
				return;
			break;
		case DH6S_CONFIRM:
			if (dhcp6_validatelease(ifp, r, len, sfrom, NULL) == -1)
			{
				dhcp6_startdiscoinform(ifp);
				return;
			}
			break;
		case DH6S_DISCOVER:
			/* Only accept REPLY in DISCOVER for RAPID_COMMIT.
			 * Normally we get an ADVERTISE for a DISCOVER. */
			if (!has_option_mask(ifo->requestmask6,
			    D6_OPTION_RAPID_COMMIT) ||
			    !dhcp6_findmoption(r, len, D6_OPTION_RAPID_COMMIT,
					      NULL))
			{
				valid_op = false;
				break;
			}
			/* Validate lease before setting state to REQUEST. */
			/* FALLTHROUGH */
		case DH6S_REQUEST: /* FALLTHROUGH */
		case DH6S_RENEW: /* FALLTHROUGH */
		case DH6S_REBIND:
			if (dhcp6_validatelease(ifp, r, len, sfrom, NULL) == -1)
			{
				/*
				 * If we can't use the lease, fallback to
				 * DISCOVER and try and get a new one.
				 *
				 * This is needed become some servers
				 * renumber the prefix or address
				 * and deny the current one before it expires
				 * rather than sending it back with a zero
				 * lifetime along with the new prefix or
				 * address to use.
				 * This behavior is wrong, but moving to the
				 * DISCOVER phase works around it.
				 *
				 * The currently held lease is still valid
				 * until a new one is found.
				 */
				if (state->state != DH6S_DISCOVER)
					dhcp6_startdiscoinform(ifp);
				return;
			}
			/* RFC8415 18.2.10.1 */
			if ((state->state == DH6S_RENEW ||
			    state->state == DH6S_REBIND) &&
			    state->has_no_binding)
			{
				dhcp6_startrequest(ifp);
				return;
			}
			if (state->state == DH6S_DISCOVER)
				state->state = DH6S_REQUEST;
			break;
		case DH6S_DECLINE:
			/* This isnt really a failure, but an
			 * acknowledgement of one. */
			loginfox("%s: %s acknowledged DECLINE6",
			    ifp->name, sfrom);
			dhcp6_fail(ifp);
			return;
		default:
			valid_op = false;
			break;
		}
		break;
	case DHCP6_ADVERTISE:
		if (state->state != DH6S_DISCOVER) {
			valid_op = false;
			break;
		}
		if (state->recv_len && state->recv->type == DHCP6_ADVERTISE) {
			/* We already have an advertismemnt.
			 * RFC 8415 says we have to wait for the IRT to elapse.
			 * To keep the same behaviour we won't do anything with
			 * this. In the future we should make a lists of
			 * ADVERTS and pick the "best" one. */
			logdebugx("%s: discarding ADVERTISEMENT from %s",
			    ifp->name, sfrom);
			return;
		}
		/* RFC7083 */
		o = dhcp6_findmoption(r, len, D6_OPTION_SOL_MAX_RT, &ol);
		if (o && ol == sizeof(uint32_t)) {
			uint32_t max_rt;

			memcpy(&max_rt, o, sizeof(max_rt));
			max_rt = ntohl(max_rt);
			if (max_rt >= 60 && max_rt <= 86400) {
				logdebugx("%s: SOL_MAX_RT %llu -> %u",
				    ifp->name,
				    (unsigned long long)state->sol_max_rt,
				    max_rt);
				state->sol_max_rt = max_rt;
			} else
				logerr("%s: invalid SOL_MAX_RT %u",
				    ifp->name, max_rt);
		}
		o = dhcp6_findmoption(r, len, D6_OPTION_INF_MAX_RT, &ol);
		if (o && ol == sizeof(uint32_t)) {
			uint32_t max_rt;

			memcpy(&max_rt, o, sizeof(max_rt));
			max_rt = ntohl(max_rt);
			if (max_rt >= 60 && max_rt <= 86400) {
				logdebugx("%s: INF_MAX_RT %llu -> %u",
				    ifp->name,
				    (unsigned long long)state->inf_max_rt,
				    max_rt);
				state->inf_max_rt = max_rt;
			} else
				logerrx("%s: invalid INF_MAX_RT %u",
				    ifp->name, max_rt);
		}
		if (dhcp6_validatelease(ifp, r, len, sfrom, NULL) == -1)
			return;
		break;
	case DHCP6_RECONFIGURE:
#ifdef AUTH
		if (auth == NULL) {
#endif
			logerrx("%s: unauthenticated %s from %s",
			    ifp->name, op, sfrom);
			if (ifo->auth.options & DHCPCD_AUTH_REQUIRE)
				return;
#ifdef AUTH
		}
		loginfox("%s: %s from %s", ifp->name, op, sfrom);
		o = dhcp6_findmoption(r, len, D6_OPTION_RECONF_MSG, &ol);
		if (o == NULL) {
			logerrx("%s: missing Reconfigure Message option",
			    ifp->name);
			return;
		}
		if (ol != 1) {
			logerrx("%s: missing Reconfigure Message type",
			    ifp->name);
			return;
		}
		switch(*o) {
		case DHCP6_RENEW:
			if (state->state != DH6S_BOUND) {
				logerrx("%s: not bound, ignoring %s",
				    ifp->name, op);
				return;
			}
			dhcp6_startrenew(ifp);
			break;
		case DHCP6_INFORMATION_REQ:
			if (state->state != DH6S_INFORMED) {
				logerrx("%s: not informed, ignoring %s",
				    ifp->name, op);
				return;
			}
			eloop_timeout_delete(ifp->ctx->eloop,
			    dhcp6_sendinform, ifp);
			dhcp6_startinform(ifp);
			break;
		default:
			logerr("%s: unsupported %s type %d",
			    ifp->name, op, *o);
			break;
		}
		return;
#else
		break;
#endif
	default:
		logerrx("%s: invalid DHCP6 type %s (%d)",
		    ifp->name, op, r->type);
		return;
	}
	if (!valid_op) {
		logwarnx("%s: invalid state for DHCP6 type %s (%d)",
		    ifp->name, op, r->type);
		return;
	}

	if (state->recv_len < (size_t)len) {
		free(state->recv);
		state->recv = malloc(len);
		if (state->recv == NULL) {
			logerr(__func__);
			return;
		}
	}
	memcpy(state->recv, r, len);
	state->recv_len = len;

	if (r->type == DHCP6_ADVERTISE) {
		struct ipv6_addr *ia;

		if (state->state == DH6S_REQUEST) /* rapid commit */
			goto bind;
		TAILQ_FOREACH(ia, &state->addrs, next) {
			if (!(ia->flags & (IPV6_AF_STALE | IPV6_AF_REQUEST)))
				break;
		}
		if (ia == NULL)
			ia = TAILQ_FIRST(&state->addrs);
		if (ia == NULL)
			loginfox("%s: ADV (no address) from %s",
			    ifp->name, sfrom);
		else
			loginfox("%s: ADV %s from %s",
			    ifp->name, ia->saddr, sfrom);
		// We will request when the IRT elapses
		return;
	}

bind:
	dhcp6_bind(ifp, op, sfrom);
}

void
dhcp6_recvmsg(struct dhcpcd_ctx *ctx, struct msghdr *msg, struct ipv6_addr *ia)
{
	struct sockaddr_in6 *from = msg->msg_name;
	size_t len = msg->msg_iov[0].iov_len;
	char sfrom[INET6_ADDRSTRLEN];
	struct interface *ifp;
	struct dhcp6_message *r;
	const struct dhcp6_state *state;
	uint8_t *o;
	uint16_t ol;

	inet_ntop(AF_INET6, &from->sin6_addr, sfrom, sizeof(sfrom));
	if (len < sizeof(struct dhcp6_message)) {
		logerrx("DHCPv6 packet too short from %s", sfrom);
		return;
	}

	if (ia != NULL)
		ifp = ia->iface;
	else {
		ifp = if_findifpfromcmsg(ctx, msg, NULL);
		if (ifp == NULL) {
			logerr(__func__);
			return;
		}
	}

	r = (struct dhcp6_message *)msg->msg_iov[0].iov_base;

	uint8_t duid[DUID_LEN], *dp;
	size_t duid_len;
	o = dhcp6_findmoption(r, len, D6_OPTION_CLIENTID, &ol);
	if (ifp->options->options & DHCPCD_ANONYMOUS) {
		duid_len = duid_make(duid, ifp, DUID_LL);
		dp = duid;
	} else {
		duid_len = ctx->duid_len;
		dp = ctx->duid;
	}
	if (o == NULL || ol != duid_len || memcmp(o, dp, ol) != 0) {
		logdebugx("%s: incorrect client ID from %s",
		    ifp->name, sfrom);
		return;
	}

	if (dhcp6_findmoption(r, len, D6_OPTION_SERVERID, NULL) == NULL) {
		logdebugx("%s: no DHCPv6 server ID from %s",
		    ifp->name, sfrom);
		return;
	}

	if (r->type == DHCP6_RECONFIGURE) {
		if (!IN6_IS_ADDR_LINKLOCAL(&from->sin6_addr)) {
			logerrx("%s: RECONFIGURE6 recv from %s, not LL",
			    ifp->name, sfrom);
			return;
		}
		goto recvif;
	}

	state = D6_CSTATE(ifp);
	if (state == NULL ||
	    r->xid[0] != state->send->xid[0] ||
	    r->xid[1] != state->send->xid[1] ||
	    r->xid[2] != state->send->xid[2])
	{
		struct interface *ifp1;
		const struct dhcp6_state *state1;

		/* Find an interface with a matching xid. */
		TAILQ_FOREACH(ifp1, ctx->ifaces, next) {
			state1 = D6_CSTATE(ifp1);
			if (state1 == NULL || state1->send == NULL)
				continue;
			if (r->xid[0] == state1->send->xid[0] &&
			    r->xid[1] == state1->send->xid[1] &&
			    r->xid[2] == state1->send->xid[2])
				break;
		}

		if (ifp1 == NULL) {
			if (state != NULL)
				logdebugx("%s: wrong xid 0x%02x%02x%02x"
				    " (expecting 0x%02x%02x%02x) from %s",
				    ifp->name,
				    r->xid[0], r->xid[1], r->xid[2],
				    state->send->xid[0],
				    state->send->xid[1],
				    state->send->xid[2],
				    sfrom);
			return;
		}
		logdebugx("%s: redirecting DHCP6 message to %s",
		    ifp->name, ifp1->name);
		ifp = ifp1;
	}

#if 0
	/*
	 * Handy code to inject raw DHCPv6 packets over responses
	 * from our server.
	 * This allows me to take a 3rd party wireshark trace and
	 * replay it in my code.
	 */
	static int replyn = 0;
	char fname[PATH_MAX], tbuf[UDPLEN_MAX];
	int fd;
	ssize_t tlen;
	uint8_t *si1, *si2;
	uint16_t si_len1, si_len2;

	snprintf(fname, sizeof(fname),
	    "/tmp/dhcp6.reply%d.raw", replyn++);
	fd = open(fname, O_RDONLY, 0);
	if (fd == -1) {
		logerr("%s: open: %s", __func__, fname);
		return;
	}
	tlen = read(fd, tbuf, sizeof(tbuf));
	if (tlen == -1)
		logerr("%s: read: %s", __func__, fname);
	close(fd);

	/* Copy across ServerID so we can work with our own server. */
	si1 = dhcp6_findmoption(r, len, D6_OPTION_SERVERID, &si_len1);
	si2 = dhcp6_findmoption(tbuf, (size_t)tlen,
	    D6_OPTION_SERVERID, &si_len2);
	if (si1 != NULL && si2 != NULL && si_len1 == si_len2)
		memcpy(si2, si1, si_len2);
	r = (struct dhcp6_message *)tbuf;
	len = (size_t)tlen;
#endif

recvif:
	dhcp6_recvif(ifp, sfrom, r, len);
}

static void
dhcp6_recv(struct dhcpcd_ctx *ctx, struct ipv6_addr *ia, unsigned short events)
{
	struct sockaddr_in6 from;
	union {
		struct dhcp6_message dhcp6;
		uint8_t buf[UDPLEN_MAX]; /* Maximum UDP message size */
	} iovbuf;
	struct iovec iov = {
		.iov_base = iovbuf.buf, .iov_len = sizeof(iovbuf.buf),
	};
	union {
		struct cmsghdr hdr;
		uint8_t buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	} cmsgbuf = { .buf = { 0 } };
	struct msghdr msg = {
	    .msg_name = &from, .msg_namelen = sizeof(from),
	    .msg_iov = &iov, .msg_iovlen = 1,
	    .msg_control = cmsgbuf.buf, .msg_controllen = sizeof(cmsgbuf.buf),
	};
	int s;
	ssize_t bytes;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	s = ia != NULL ? ia->dhcp6_fd : ctx->dhcp6_rfd;
	bytes = recvmsg(s, &msg, 0);
	if (bytes == -1) {
		logerr(__func__);
		return;
	}

	iov.iov_len = (size_t)bytes;
	dhcp6_recvmsg(ctx, &msg, ia);
}

static void

dhcp6_recvaddr(void *arg, unsigned short events)
{
	struct ipv6_addr *ia = arg;

	dhcp6_recv(ia->iface->ctx, ia, events);
}

static void
dhcp6_recvctx(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	dhcp6_recv(ctx, NULL, events);
}

int
dhcp6_openraw(void)
{
	int fd, v;

	fd = xsocket(PF_INET6, SOCK_RAW | SOCK_CXNB, IPPROTO_UDP);
	if (fd == -1)
		return -1;

	v = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &v, sizeof(v)) == -1)
		goto errexit;

	v = offsetof(struct udphdr, uh_sum);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &v, sizeof(v)) == -1)
		goto errexit;

	return fd;

errexit:
	close(fd);
	return -1;
}

int
dhcp6_openudp(unsigned int ifindex, struct in6_addr *ia)
{
	struct sockaddr_in6 sa;
	int n, s;

	s = xsocket(PF_INET6, SOCK_DGRAM | SOCK_CXNB, IPPROTO_UDP);
	if (s == -1)
		goto errexit;

	memset(&sa, 0, sizeof(sa));
	sa.sin6_family = AF_INET6;
	sa.sin6_port = htons(DHCP6_CLIENT_PORT);
#ifdef BSD
	sa.sin6_len = sizeof(sa);
#endif

	if (ia != NULL) {
		memcpy(&sa.sin6_addr, ia, sizeof(sa.sin6_addr));
		ipv6_setscope(&sa, ifindex);
	}

	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == -1)
		goto errexit;

	n = 1;
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, &n, sizeof(n)) == -1)
		goto errexit;

#ifdef SO_RERROR
	n = 1;
	if (setsockopt(s, SOL_SOCKET, SO_RERROR, &n, sizeof(n)) == -1)
		goto errexit;
#endif

	return s;

errexit:
	logerr(__func__);
	if (s != -1)
		close(s);
	return -1;
}

#ifndef SMALL
static void
dhcp6_activateinterfaces(struct interface *ifp)
{
	struct interface *ifd;
	size_t i, j;
	struct if_ia *ia;
	struct if_sla *sla;

	for (i = 0; i < ifp->options->ia_len; i++) {
		ia = &ifp->options->ia[i];
		if (ia->ia_type != D6_OPTION_IA_PD)
			continue;
		for (j = 0; j < ia->sla_len; j++) {
			sla = &ia->sla[j];
			ifd = if_find(ifp->ctx->ifaces, sla->ifname);
			if (ifd == NULL) {
				if (*sla->ifname != '-')
					logwarn("%s: cannot delegate to %s",
					    ifp->name, sla->ifname);
				continue;
			}
			if (!ifd->active) {
				loginfox("%s: activating for delegation",
				    sla->ifname);
				dhcpcd_activateinterface(ifd,
				    DHCPCD_IPV6 | DHCPCD_DHCP6);
			}
		}
	}
}
#endif

static void
dhcp6_start1(void *arg)
{
	struct interface *ifp = arg;
	struct dhcpcd_ctx *ctx = ifp->ctx;
	struct if_options *ifo = ifp->options;
	struct dhcp6_state *state;
	size_t i;
	const struct dhcp_compat *dhc;

	if ((ctx->options & (DHCPCD_MANAGER|DHCPCD_PRIVSEP)) == DHCPCD_MANAGER &&
	    ctx->dhcp6_rfd == -1)
	{
		ctx->dhcp6_rfd = dhcp6_openudp(0, NULL);
		if (ctx->dhcp6_rfd == -1) {
			logerr(__func__);
			return;
		}
		if (eloop_event_add(ctx->eloop, ctx->dhcp6_rfd, ELE_READ,
		    dhcp6_recvctx, ctx) == -1)
			logerr("%s: eloop_event_add", __func__);
	}

	if (!IN_PRIVSEP(ctx) && ctx->dhcp6_wfd == -1) {
		ctx->dhcp6_wfd = dhcp6_openraw();
		if (ctx->dhcp6_wfd == -1) {
			logerr(__func__);
			return;
		}
	}

	state = D6_STATE(ifp);
	/* If no DHCPv6 options are configured,
	   match configured DHCPv4 options to DHCPv6 equivalents. */
	for (i = 0; i < sizeof(ifo->requestmask6); i++) {
		if (ifo->requestmask6[i] != '\0')
			break;
	}
	if (i == sizeof(ifo->requestmask6)) {
		for (dhc = dhcp_compats; dhc->dhcp_opt; dhc++) {
			if (DHC_REQ(ifo->requestmask, ifo->nomask, dhc->dhcp_opt))
				add_option_mask(ifo->requestmask6,
				    dhc->dhcp6_opt);
		}
		if (ifo->fqdn != FQDN_DISABLE || ifo->options & DHCPCD_HOSTNAME)
			add_option_mask(ifo->requestmask6, D6_OPTION_FQDN);
	}

#ifndef SMALL
	/* Rapid commit won't work with Prefix Delegation Exclusion */
	if (dhcp6_findselfsla(ifp))
		del_option_mask(ifo->requestmask6, D6_OPTION_RAPID_COMMIT);
#endif

	if (state->state == DH6S_INFORM) {
		add_option_mask(ifo->requestmask6, D6_OPTION_INFO_REFRESH_TIME);
		dhcp6_startinform(ifp);
	} else {
		del_option_mask(ifo->requestmask6, D6_OPTION_INFO_REFRESH_TIME);
		dhcp6_startinit(ifp);
	}

#ifndef SMALL
	dhcp6_activateinterfaces(ifp);
#endif
}

int
dhcp6_start(struct interface *ifp, enum DH6S init_state)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state != NULL) {
		switch (init_state) {
		case DH6S_INIT:
			goto gogogo;
		case DH6S_INFORM:
			/* RFC 8415 21.23
			 * If D6_OPTION_INFO_REFRESH_TIME does not exist
			 * then we MUST refresh by IRT_DEFAULT seconds
			 * and should not be influenced by only the
			 * pl/vl time of the RA changing. */
			if (state->state == DH6S_INIT ||
			    (state->state == DH6S_DISCOVER &&
			    !(ifp->options->options & DHCPCD_IA_FORCED) &&
			    !ipv6nd_hasradhcp(ifp, true)))
				dhcp6_startinform(ifp);
			break;
		case DH6S_REQUEST:
			if (ifp->options->options & DHCPCD_DHCP6 &&
			    (state->state == DH6S_INIT ||
			     state->state == DH6S_INFORM ||
			     state->state == DH6S_INFORMED ||
			     state->state == DH6S_DELEGATED))
			{
				/* Change from stateless to stateful */
				init_state = DH6S_INIT;
				goto gogogo;
			}
			break;
		case DH6S_CONFIRM:
			init_state = DH6S_INIT;
			goto gogogo;
		default:
			/* Not possible, but sushes some compiler warnings. */
			break;
		}
		return 0;
	} else {
		switch (init_state) {
		case DH6S_CONFIRM:
			/* No DHCPv6 config, no existing state
			 * so nothing to do. */
			return 0;
		case DH6S_INFORM:
			break;
		default:
			init_state = DH6S_INIT;
			break;
		}
	}

	if (!(ifp->options->options & DHCPCD_DHCP6))
		return 0;

	ifp->if_data[IF_DATA_DHCP6] = calloc(1, sizeof(*state));
	state = D6_STATE(ifp);
	if (state == NULL)
		return -1;

	state->sol_max_rt = SOL_MAX_RT;
	state->inf_max_rt = INF_MAX_RT;
	TAILQ_INIT(&state->addrs);

gogogo:
	state->new_start = true;
	state->state = init_state;
	state->lerror = 0;
	state->failed = false;
	dhcp_set_leasefile(state->leasefile, sizeof(state->leasefile),
	    AF_INET6, ifp);
	if (ipv6_linklocal(ifp) == NULL) {
		logdebugx("%s: delaying DHCPv6 for LL address", ifp->name);
		ipv6_addlinklocalcallback(ifp, dhcp6_start1, ifp);
		return 0;
	}

	dhcp6_start1(ifp);
	return 0;
}

void
dhcp6_reboot(struct interface *ifp)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state == NULL)
		return;

	state->lerror = 0;
	switch (state->state) {
	case DH6S_BOUND:
		dhcp6_startrebind(ifp);
		break;
	default:
		dhcp6_startdiscoinform(ifp);
		break;
	}
}

static void
dhcp6_freedrop(struct interface *ifp, int drop, const char *reason)
{
	struct dhcp6_state *state;
	struct dhcpcd_ctx *ctx;
	unsigned long long options;

	if (ifp->options)
		options = ifp->options->options;
	else
		options = ifp->ctx->options;

	if (ifp->ctx->eloop)
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);

#ifndef SMALL
	/* If we're dropping the lease, drop delegated addresses.
	 * If, for whatever reason, we don't drop them in the future
	 * then they should at least be marked as deprecated (pltime 0). */
	if (drop && (options & DHCPCD_NODROP) != DHCPCD_NODROP)
		dhcp6_delete_delegates(ifp);
#endif

	state = D6_STATE(ifp);
	if (state) {
		/* Failure to send the release may cause this function to
		 * re-enter */
		if (state->state == DH6S_RELEASE) {
			dhcp6_finishrelease(ifp);
			return;
		}

		if (drop && options & DHCPCD_RELEASE &&
		    state->state != DH6S_DELEGATED)
		{
			if (if_is_link_up(ifp) &&
			    state->state != DH6S_RELEASED &&
			    state->state != DH6S_INFORMED)
			{
				dhcp6_startrelease(ifp);
				return;
			}
			dhcp_unlink(ifp->ctx, state->leasefile);
		}
#ifdef AUTH
		else if (state->auth.reconf != NULL) {
			/*
			 * Drop the lease as the token may only be present
			 * in the initial reply message and not subsequent
			 * renewals.
			 * If dhcpcd is restarted, the token is lost.
			 * XXX persist this in another file?
			 */
			dhcp_unlink(ifp->ctx, state->leasefile);
		}
#endif

		dhcp6_freedrop_addrs(ifp, drop, NULL);
		free(state->old);
		state->old = state->new;
		state->old_len = state->new_len;
		state->new = NULL;
		state->new_len = 0;
		if (drop && state->old &&
		    (options & DHCPCD_NODROP) != DHCPCD_NODROP)
		{
			if (reason == NULL)
				reason = "STOP6";
			script_runreason(ifp, reason);
		}
		free(state->old);
		free(state->send);
		free(state->recv);
		free(state);
		ifp->if_data[IF_DATA_DHCP6] = NULL;
	}

	/* If we don't have any more DHCP6 enabled interfaces,
	 * close the global socket and release resources */
	ctx = ifp->ctx;
	if (ctx->ifaces) {
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (D6_STATE(ifp))
				break;
		}
	}
	if (ifp == NULL && ctx->dhcp6_rfd != -1) {
		eloop_event_delete(ctx->eloop, ctx->dhcp6_rfd);
		close(ctx->dhcp6_rfd);
		ctx->dhcp6_rfd = -1;
	}
}

void
dhcp6_drop(struct interface *ifp, const char *reason)
{

	dhcp6_freedrop(ifp, 1, reason);
}

void
dhcp6_free(struct interface *ifp)
{

	dhcp6_freedrop(ifp, 0, NULL);
}

void
dhcp6_abort(struct interface *ifp)
{
	struct dhcp6_state *state;
#ifdef ND6_ADVERTISE
	struct ipv6_addr *ia;
#endif

	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_start1, ifp);
	state = D6_STATE(ifp);
	if (state == NULL)
		return;

#ifdef ND6_ADVERTISE
	TAILQ_FOREACH(ia, &state->addrs, next) {
		ipv6nd_advertise(ia);
	}
#endif

	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_startdiscover, ifp);
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_senddiscover, ifp);
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_startinform, ifp);
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_sendinform, ifp);

	switch (state->state) {
	case DH6S_DISCOVER:	/* FALLTHROUGH */
	case DH6S_REQUEST:	/* FALLTHROUGH */
	case DH6S_INFORM:
		state->state = DH6S_INIT;
		break;
	default:
		break;
	}
}

void
dhcp6_handleifa(int cmd, struct ipv6_addr *ia, pid_t pid)
{
	struct dhcp6_state *state;
	struct interface *ifp = ia->iface;

	/* If not running in manager mode, listen to this address */
	if (cmd == RTM_NEWADDR &&
	    !(ia->addr_flags & IN6_IFF_NOTUSEABLE) &&
	    ifp->active == IF_ACTIVE_USER &&
	    !(ifp->ctx->options & DHCPCD_MANAGER) &&
	    ifp->options->options & DHCPCD_DHCP6)
	{
#ifdef PRIVSEP
		if (IN_PRIVSEP_SE(ifp->ctx)) {
			if (ps_inet_opendhcp6(ia) == -1)
				logerr(__func__);
		} else
#endif
		{
			if (ia->dhcp6_fd == -1)
				ia->dhcp6_fd = dhcp6_openudp(ia->iface->index,
				    &ia->addr);
			if (ia->dhcp6_fd != -1 &&
			    eloop_event_add(ia->iface->ctx->eloop,
			    ia->dhcp6_fd, ELE_READ, dhcp6_recvaddr, ia) == -1)
				logerr("%s: eloop_event_add", __func__);
		}
	}

	if ((state = D6_STATE(ifp)) != NULL)
		ipv6_handleifa_addrs(cmd, &state->addrs, ia, pid);
}

ssize_t
dhcp6_env(FILE *fp, const char *prefix, const struct interface *ifp,
    const struct dhcp6_message *m, size_t len)
{
	const struct if_options *ifo;
	struct dhcp_opt *opt, *vo;
	const uint8_t *p;
	struct dhcp6_option o;
	size_t i;
	char *pfx;
	uint32_t en;
	const struct dhcpcd_ctx *ctx;
#ifndef SMALL
	const struct dhcp6_state *state;
	const struct ipv6_addr *ap;
	bool first;
#endif

	if (m == NULL)
		goto delegated;

	if (len < sizeof(*m)) {
		/* Should be impossible with guards at packet in
		 * and reading leases */
		errno = EINVAL;
		return -1;
	}

	ifo = ifp->options;
	ctx = ifp->ctx;

	/* Zero our indexes */
	for (i = 0, opt = ctx->dhcp6_opts;
	    i < ctx->dhcp6_opts_len;
	    i++, opt++)
		dhcp_zero_index(opt);
	for (i = 0, opt = ifp->options->dhcp6_override;
	    i < ifp->options->dhcp6_override_len;
	    i++, opt++)
		dhcp_zero_index(opt);
	for (i = 0, opt = ctx->vivso;
	    i < ctx->vivso_len;
	    i++, opt++)
		dhcp_zero_index(opt);
	if (asprintf(&pfx, "%s_dhcp6", prefix) == -1)
		return -1;

	/* Unlike DHCP, DHCPv6 options *may* occur more than once.
	 * There is also no provision for option concatenation unlike DHCP. */
	p = (const uint8_t *)m + sizeof(*m);
	len -= sizeof(*m);
	for (; len != 0; p += o.len, len -= o.len) {
		if (len < sizeof(o)) {
			errno = EINVAL;
			break;
		}
		memcpy(&o, p, sizeof(o));
		p += sizeof(o);
		len -= sizeof(o);
		o.len = ntohs(o.len);
		if (len < o.len) {
			errno =	EINVAL;
			break;
		}
		o.code = ntohs(o.code);
		if (has_option_mask(ifo->nomask6, o.code))
			continue;
		for (i = 0, opt = ifo->dhcp6_override;
		    i < ifo->dhcp6_override_len;
		    i++, opt++)
			if (opt->option == o.code)
				break;
		if (i == ifo->dhcp6_override_len &&
		    o.code == D6_OPTION_VENDOR_OPTS &&
		    o.len > sizeof(en))
		{
			memcpy(&en, p, sizeof(en));
			en = ntohl(en);
			vo = vivso_find(en, ifp);
		} else
			vo = NULL;
		if (i == ifo->dhcp6_override_len) {
			for (i = 0, opt = ctx->dhcp6_opts;
			    i < ctx->dhcp6_opts_len;
			    i++, opt++)
				if (opt->option == o.code)
					break;
			if (i == ctx->dhcp6_opts_len)
				opt = NULL;
		}
		if (opt) {
			dhcp_envoption(ifp->ctx,
			    fp, pfx, ifp->name,
			    opt, dhcp6_getoption, p, o.len);
		}
		if (vo) {
			dhcp_envoption(ifp->ctx,
			    fp, pfx, ifp->name,
			    vo, dhcp6_getoption,
			    p + sizeof(en),
			    o.len - sizeof(en));
		}
	}
	free(pfx);

delegated:
#ifndef SMALL
	/* Needed for Delegated Prefixes */
	state = D6_CSTATE(ifp);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->delegating_prefix)
			break;
	}
	if (ap == NULL)
		return 1;
	if (fprintf(fp, "%s_delegated_dhcp6_prefix=", prefix) == -1)
		return -1;
	first = true;
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->delegating_prefix == NULL)
			continue;
		if (first)
			first = false;
		else {
			if (fputc(' ', fp) == EOF)
				return -1;
		}
		if (fprintf(fp, "%s", ap->saddr) == -1)
			return -1;
	}
	if (fputc('\0', fp) == EOF)
		return -1;
#endif

	return 1;
}
#endif

#ifndef SMALL
int
dhcp6_dump(struct interface *ifp)
{
	struct dhcp6_state *state;

	ifp->if_data[IF_DATA_DHCP6] = state = calloc(1, sizeof(*state));
	if (state == NULL) {
		logerr(__func__);
		return -1;
	}
	TAILQ_INIT(&state->addrs);
	if (dhcp6_readlease(ifp, 0) == -1) {
		logerr("dhcp6_readlease");
		return -1;
	}
	state->reason = "DUMP6";
	return script_runreason(ifp, state->reason);
}
#endif
