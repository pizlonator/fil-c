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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "config.h"

#ifdef HAVE_SYS_BITOPS_H
#include <sys/bitops.h>
#else
#include "compat/bitops.h"
#endif

#ifdef BSD
/* Purely for the ND6_IFF_AUTO_LINKLOCAL #define which is solely used
 * to generate our CAN_ADD_LLADDR #define. */
#  include <netinet6/in6_var.h>
#  include <netinet6/nd6.h>
#endif

#include <errno.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define ELOOP_QUEUE	ELOOP_IPV6
#include "common.h"
#include "if.h"
#include "dhcpcd.h"
#include "dhcp6.h"
#include "eloop.h"
#include "ipv6.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "sa.h"
#include "script.h"

#ifdef HAVE_MD5_H
#  ifndef DEPGEN
#    include <md5.h>
#  endif
#endif

#ifdef SHA2_H
#  include SHA2_H
#endif

#ifndef SHA256_DIGEST_LENGTH
#  define SHA256_DIGEST_LENGTH		32
#endif

#ifdef IPV6_POLLADDRFLAG
#  warning kernel does not report IPv6 address flag changes
#  warning polling tentative address flags periodically
#endif

/* Hackery at it's finest. */
#ifndef s6_addr32
#  ifdef __sun
#    define s6_addr32	_S6_un._S6_u32
#  else
#    define s6_addr32	__u6_addr.__u6_addr32
#  endif
#endif

#if defined(HAVE_IN6_ADDR_GEN_MODE_NONE) || defined(ND6_IFF_AUTO_LINKLOCAL) || \
    defined(IFF_NOLINKLOCAL)
/* Only add the LL address if we have a carrier, so DaD works. */
#define	CAN_ADD_LLADDR(ifp) \
    (!((ifp)->options->options & DHCPCD_LINK) || if_is_link_up((ifp)))
#ifdef __sun
/* Although we can add our own LL address, we cannot drop it
 * without unplumbing the if which is a lot of code.
 * So just keep it for the time being. */
#define	CAN_DROP_LLADDR(ifp)	(0)
#else
#define	CAN_DROP_LLADDR(ifp)	(1)
#endif
#else
/* We have no control over the OS adding the LLADDR, so just let it do it
 * as we cannot force our own view on it. */
#define	CAN_ADD_LLADDR(ifp)	(0)
#define	CAN_DROP_LLADDR(ifp)	(0)
#endif

#ifdef IPV6_MANAGETEMPADDR
static void ipv6_regentempaddr(void *);
#endif

int
ipv6_init(struct dhcpcd_ctx *ctx)
{

	if (ctx->ra_routers != NULL)
		return 0;

	ctx->ra_routers = malloc(sizeof(*ctx->ra_routers));
	if (ctx->ra_routers == NULL)
		return -1;
	TAILQ_INIT(ctx->ra_routers);

#ifndef __sun
	ctx->nd_fd = -1;
#endif
#ifdef DHCP6
	ctx->dhcp6_rfd = -1;
	ctx->dhcp6_wfd = -1;
#endif
	return 0;
}

static ssize_t
ipv6_readsecret(struct dhcpcd_ctx *ctx)
{
	char line[1024];
	unsigned char *p;
	size_t len;
	uint32_t r;

	ctx->secret_len = dhcp_read_hwaddr_aton(ctx, &ctx->secret, SECRET);
	if (ctx->secret_len != 0)
		return (ssize_t)ctx->secret_len;

	if (errno != ENOENT)
		logerr("%s: cannot read secret", __func__);

	/* Chaining arc4random should be good enough.
	 * RFC7217 section 5.1 states the key SHOULD be at least 128 bits.
	 * To attempt and future proof ourselves, we'll generate a key of
	 * 512 bits (64 bytes). */
	if (ctx->secret_len < 64) {
		if ((ctx->secret = malloc(64)) == NULL) {
			logerr(__func__);
			return -1;
		}
		ctx->secret_len = 64;
	}
	p = ctx->secret;
	for (len = 0; len < 512 / NBBY; len += sizeof(r)) {
		r = arc4random();
		memcpy(p, &r, sizeof(r));
		p += sizeof(r);
	}

	hwaddr_ntoa(ctx->secret, ctx->secret_len, line, sizeof(line));
	len = strlen(line);
	if (len < sizeof(line) - 2) {
		line[len++] = '\n';
		line[len] = '\0';
	}
	if (dhcp_writefile(ctx, SECRET, S_IRUSR, line, len) == -1) {
		logerr("%s: cannot write secret", __func__);
		ctx->secret_len = 0;
		return -1;
	}
	return (ssize_t)ctx->secret_len;
}

/* http://www.iana.org/assignments/ipv6-interface-ids/ipv6-interface-ids.xhtml
 * RFC5453 */
static const struct reslowhigh {
	const uint8_t high[8];
	const uint8_t low[8];
} reslowhigh[] = {
	/* RFC4291 + RFC6543 */
	{ { 0x02, 0x00, 0x5e, 0xff, 0xfe, 0x00, 0x00, 0x00 },
	  { 0x02, 0x00, 0x5e, 0xff, 0xfe, 0xff, 0xff, 0xff } },
	/* RFC2526 */
	{ { 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80 },
	  { 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } }
};

static bool
ipv6_reserved(const struct in6_addr *addr)
{
	uint64_t id, low, high;
	size_t i;
	const struct reslowhigh *r;

	id = be64dec(addr->s6_addr + sizeof(id));
	if (id == 0) /* RFC4291 */
		return 1;
	for (i = 0; i < __arraycount(reslowhigh); i++) {
		r = &reslowhigh[i];
		low = be64dec(r->low);
		high = be64dec(r->high);
		if (id >= low && id <= high)
			return true;
	}
	return false;
}

/* RFC7217 */
static int
ipv6_makestableprivate1(struct dhcpcd_ctx *ctx,
    struct in6_addr *addr, const struct in6_addr *prefix, int prefix_len,
    const unsigned char *netiface, size_t netiface_len,
    const unsigned char *netid, size_t netid_len,
    unsigned short vlanid,
    uint32_t *dad_counter)
{
	unsigned char buf[2048], *p, digest[SHA256_DIGEST_LENGTH];
	size_t len, l;
	SHA256_CTX sha_ctx;

	if (prefix_len < 0 || prefix_len > 120) {
		errno = EINVAL;
		return -1;
	}

	if (ctx->secret_len == 0) {
		if (ipv6_readsecret(ctx) == -1)
			return -1;
	}

	l = (size_t)(ROUNDUP8(prefix_len) / NBBY);
	len = l + netiface_len + netid_len + sizeof(*dad_counter) +
	    ctx->secret_len;
	if (vlanid != 0)
		len += sizeof(vlanid);
	if (len > sizeof(buf)) {
		errno = ENOBUFS;
		return -1;
	}

	for (;; (*dad_counter)++) {
		/* Combine all parameters into one buffer */
		p = buf;
		memcpy(p, prefix, l);
		p += l;
		memcpy(p, netiface, netiface_len);
		p += netiface_len;
		memcpy(p, netid, netid_len);
		p += netid_len;
		/* Don't use a vlanid if not set.
		 * This ensures prior versions have the same unique address. */
		if (vlanid != 0) {
			memcpy(p, &vlanid, sizeof(vlanid));
			p += sizeof(vlanid);
		}
		memcpy(p, dad_counter, sizeof(*dad_counter));
		p += sizeof(*dad_counter);
		memcpy(p, ctx->secret, ctx->secret_len);

		/* Make an address using the digest of the above.
		 * RFC7217 Section 5.1 states that we shouldn't use MD5.
		 * Pity as we use that for HMAC-MD5 which is still deemed OK.
		 * SHA-256 is recommended */
		SHA256_Init(&sha_ctx);
		SHA256_Update(&sha_ctx, buf, len);
		SHA256_Final(digest, &sha_ctx);

		p = addr->s6_addr;
		memcpy(p, prefix, l);
		/* RFC7217 section 5.2 says we need to start taking the id from
		 * the least significant bit */
		len = sizeof(addr->s6_addr) - l;
		memcpy(p + l, digest + (sizeof(digest) - len), len);

		/* Ensure that the Interface ID does not match a reserved one,
		 * if it does then treat it as a DAD failure.
		 * RFC7217 section 5.2 */
		if (prefix_len != 64)
			break;
		if (!ipv6_reserved(addr))
			break;
	}

	return 0;
}

int
ipv6_makestableprivate(struct in6_addr *addr,
    const struct in6_addr *prefix, int prefix_len,
    const struct interface *ifp,
    int *dad_counter)
{
	uint32_t dad;
	int r;

	dad = (uint32_t)*dad_counter;

	/* For our implementation, we shall set the hardware address
	 * as the interface identifier */
	r = ipv6_makestableprivate1(ifp->ctx, addr, prefix, prefix_len,
	    ifp->hwaddr, ifp->hwlen,
	    ifp->ssid, ifp->ssid_len,
	    ifp->vlanid, &dad);

	if (r == 0)
		*dad_counter = (int)dad;
	return r;
}

#ifdef IPV6_AF_TEMPORARY
static int
ipv6_maketemporaryaddress(struct in6_addr *addr,
    const struct in6_addr *prefix, int prefix_len,
    const struct interface *ifp)
{
	struct in6_addr mask;
	struct interface *ifpn;

	if (ipv6_mask(&mask, prefix_len) == -1)
		return -1;
	*addr = *prefix;

again:
	addr->s6_addr32[2] |= (arc4random() & ~mask.s6_addr32[2]);
	addr->s6_addr32[3] |= (arc4random() & ~mask.s6_addr32[3]);

	TAILQ_FOREACH(ifpn, ifp->ctx->ifaces, next) {
		if (ipv6_iffindaddr(ifpn, addr, 0) != NULL)
			break;
	}
	if (ifpn != NULL)
		goto again;
	if (ipv6_reserved(addr))
		goto again;
	return 0;
}
#endif

static int
ipv6_makeprefix(struct in6_addr *prefix, const struct in6_addr *addr, int len)
{
	struct in6_addr mask;
	size_t i;

	if (ipv6_mask(&mask, len) == -1)
		return -1;
	*prefix = *addr;
	for (i = 0; i < sizeof(prefix->s6_addr); i++)
		prefix->s6_addr[i] &= mask.s6_addr[i];
	return 0;
}

int
ipv6_mask(struct in6_addr *mask, int len)
{
	static const unsigned char masks[NBBY] =
	    { 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
	int bytes, bits, i;

	if (len < 0 || len > 128) {
		errno = EINVAL;
		return -1;
	}

	memset(mask, 0, sizeof(*mask));
	bytes = len / NBBY;
	bits = len % NBBY;
	for (i = 0; i < bytes; i++)
		mask->s6_addr[i] = 0xff;
	if (bits != 0) {
		/* Coverify false positive.
		 * bytelen cannot be 16 if bitlen is non zero */
		/* coverity[overrun-local] */
		mask->s6_addr[bytes] = masks[bits - 1];
	}
	return 0;
}

uint8_t
ipv6_prefixlen(const struct in6_addr *mask)
{
	int x = 0, y;
	const unsigned char *lim, *p;

	lim = (const unsigned char *)mask + sizeof(*mask);
	for (p = (const unsigned char *)mask; p < lim; x++, p++) {
		if (*p != 0xff)
			break;
	}
	y = 0;
	if (p < lim) {
		for (y = 0; y < NBBY; y++) {
			if ((*p & (0x80 >> y)) == 0)
				break;
		}
	}

	/*
	 * when the limit pointer is given, do a stricter check on the
	 * remaining bits.
	 */
	if (p < lim) {
		if (y != 0 && (*p & (0x00ff >> y)) != 0)
			return 0;
		for (p = p + 1; p < lim; p++)
			if (*p != 0)
				return 0;
	}

	return (uint8_t)(x * NBBY + y);
}

int
ipv6_makeaddr(struct in6_addr *addr, struct interface *ifp,
    const struct in6_addr *prefix, int prefix_len, unsigned int flags)
{
	const struct ipv6_addr *ap;
	const struct if_options *ifo = ifp->options;
	int dad;

	if (prefix_len < 0 || prefix_len > 120) {
		errno = EINVAL;
		return -1;
	}

#ifdef IPV6_AF_TEMPORARY
	if (flags & IPV6_AF_TEMPORARY)
		return ipv6_maketemporaryaddress(addr, prefix, prefix_len, ifp);
#else
	UNUSED(flags);
#endif

	if (ifo->options & DHCPCD_SLAACPRIVATE) {
		dad = 0;
		if (ipv6_makestableprivate(addr,
		    prefix, prefix_len, ifp, &dad) == -1)
			return -1;
		return dad;
	} else if (!IN6_IS_ADDR_UNSPECIFIED(&ifo->token)) {
		int bytes = prefix_len / NBBY;
		int bits = prefix_len % NBBY;

		// Copy the token into the address.
		*addr = ifo->token;

		// If we have any dangling bits, just copy that in also.
		// XXX Can we preserve part of the token still?
		if (bits != 0)
			bytes++;

		// Copy the prefix in.
		if (bytes > 0)
			memcpy(addr->s6_addr, prefix->s6_addr, (size_t)bytes);
		return 0;
	}

	if (prefix_len > 64) {
		errno = EINVAL;
		return -1;
	}
	if ((ap = ipv6_linklocal(ifp)) == NULL) {
		/* We delay a few functions until we get a local-link address
		 * so this should never be hit. */
		errno = ENOENT;
		return -1;
	}

	/* Make the address from the first local-link address */
	memcpy(addr, prefix, sizeof(*prefix));
	addr->s6_addr32[2] = ap->addr.s6_addr32[2];
	addr->s6_addr32[3] = ap->addr.s6_addr32[3];
	return 0;
}

static void
in6_to_h64(uint64_t *vhigh, uint64_t *vlow, const struct in6_addr *addr)
{

	*vhigh = be64dec(addr->s6_addr);
	*vlow = be64dec(addr->s6_addr + 8);
}

static void
h64_to_in6(struct in6_addr *addr, uint64_t vhigh, uint64_t vlow)
{

	be64enc(addr->s6_addr, vhigh);
	be64enc(addr->s6_addr + 8, vlow);
}

int
ipv6_userprefix(
	const struct in6_addr *prefix,	// prefix from router
	short prefix_len,		// length of prefix received
	uint64_t user_number,		// "random" number from user
	struct in6_addr *result,	// resultant prefix
	short result_len)		// desired prefix length
{
	uint64_t vh, vl, user_low, user_high;

	if (prefix_len < 1 || prefix_len > 128 ||
	    result_len < 1 || result_len > 128)
	{
		errno = EINVAL;
		return -1;
	}

	/* Check that the user_number fits inside result_len less prefix_len */
	if (result_len < prefix_len ||
	    fls64(user_number) > result_len - prefix_len)
	{
	       errno = ERANGE;
	       return -1;
	}

	/* If user_number is zero, just copy the prefix into the result. */
	if (user_number == 0) {
		*result = *prefix;
		return 0;
	}

	/* Shift user_number so it fit's just inside result_len.
	 * Shifting by 0 or sizeof(user_number) is undefined,
	 * so we cater for that. */
	if (result_len == 128) {
		user_high = 0;
		user_low = user_number;
	} else if (result_len > 64) {
		if (prefix_len >= 64)
			user_high = 0;
		else
			user_high = user_number >> (result_len - prefix_len);
		user_low = user_number << (128 - result_len);
	} else if (result_len == 64) {
		user_high = user_number;
		user_low = 0;
	} else {
		user_high = user_number << (64 - result_len);
		user_low = 0;
	}

	/* convert to two 64bit host order values */
	in6_to_h64(&vh, &vl, prefix);

	vh |= user_high;
	vl |= user_low;

	/* copy back result */
	h64_to_in6(result, vh, vl);

	return 0;
}

#ifdef IPV6_POLLADDRFLAG
void
ipv6_checkaddrflags(void *arg)
{
	struct ipv6_addr *ia;
	int flags;
	const char *alias;

	ia = arg;
#ifdef ALIAS_ADDR
	alias = ia->alias;
#else
	alias = NULL;
#endif
	if ((flags = if_addrflags6(ia->iface, &ia->addr, alias)) == -1) {
		if (errno != EEXIST && errno != EADDRNOTAVAIL)
			logerr("%s: if_addrflags6", __func__);
		return;
	}

	if (!(flags & IN6_IFF_TENTATIVE)) {
		/* Simulate the kernel announcing the new address. */
		ipv6_handleifa(ia->iface->ctx, RTM_NEWADDR,
		    ia->iface->ctx->ifaces, ia->iface->name,
		    &ia->addr, ia->prefix_len, flags, 0);
	} else {
		/* Still tentative? Check again in a bit. */
		eloop_timeout_add_msec(ia->iface->ctx->eloop,
		    RETRANS_TIMER / 2, ipv6_checkaddrflags, ia);
	}
}
#endif

static void
ipv6_deletedaddr(struct ipv6_addr *ia)
{

#ifdef DHCP6
#ifdef PRIVSEP
	if (!(ia->iface->ctx->options & DHCPCD_MANAGER))
		ps_inet_closedhcp6(ia);
#elif defined(SMALL)
	UNUSED(ia);
#else
	/* NOREJECT is set if we delegated exactly the prefix to another
	 * address.
	 * This can only be one address, so just clear the flag.
	 * This should ensure the reject route will be restored. */
	if (ia->delegating_prefix != NULL)
		ia->delegating_prefix->flags &= ~IPV6_AF_NOREJECT;
#endif
#else
	UNUSED(ia);
#endif
}

void
ipv6_deleteaddr(struct ipv6_addr *ia)
{
	struct ipv6_state *state;
	struct ipv6_addr *ap;

	loginfox("%s: deleting address %s", ia->iface->name, ia->saddr);
	if (if_address6(RTM_DELADDR, ia) == -1 &&
	    errno != EADDRNOTAVAIL && errno != ESRCH &&
	    errno != ENXIO && errno != ENODEV)
		logerr(__func__);

	ipv6_deletedaddr(ia);

	state = IPV6_STATE(ia->iface);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (IN6_ARE_ADDR_EQUAL(&ap->addr, &ia->addr)) {
			TAILQ_REMOVE(&state->addrs, ap, next);
			ipv6_freeaddr(ap);
			break;
		}
	}

#ifdef ND6_ADVERTISE
	/* Advertise the address if it exists on another interface. */
	ipv6nd_advertise(ia);
#endif
}

static int
ipv6_addaddr1(struct ipv6_addr *ia, const struct timespec *now)
{
	struct interface *ifp;
	uint32_t pltime, vltime;
	int loglevel;
#ifdef ND6_ADVERTISE
	bool vltime_was_zero = ia->prefix_vltime == 0;
#endif
#ifdef __sun
	struct ipv6_state *state;
	struct ipv6_addr *ia2;

	/* If we re-add then address on Solaris then the prefix
	 * route will be scrubbed and re-added. Something might
	 * be using it, so let's avoid it. */
	if (ia->flags & IPV6_AF_DADCOMPLETED) {
		logdebugx("%s: IP address %s already exists",
		    ia->iface->name, ia->saddr);
#ifdef ND6_ADVERTISE
		goto advertise;
#else
		return 0;
#endif
	}
#endif

	/* Remember the interface of the address. */
	ifp = ia->iface;

	if (!(ia->flags & IPV6_AF_DADCOMPLETED) &&
	    ipv6_iffindaddr(ifp, &ia->addr, IN6_IFF_NOTUSEABLE))
		ia->flags |= IPV6_AF_DADCOMPLETED;

	/* Adjust plftime and vltime based on acquired time */
	pltime = ia->prefix_pltime;
	vltime = ia->prefix_vltime;

	if (ifp->options->options & DHCPCD_LASTLEASE_EXTEND) {
		/* We don't want the kernel to expire the address.
		 * The saved times will be re-applied to the ia
		 * before exiting this function. */
		ia->prefix_vltime = ia->prefix_pltime = ND6_INFINITE_LIFETIME;
	}

	if (timespecisset(&ia->acquired) &&
	    (ia->prefix_pltime != ND6_INFINITE_LIFETIME ||
	    ia->prefix_vltime != ND6_INFINITE_LIFETIME))
	{
		uint32_t elapsed;
		struct timespec n;

		if (now == NULL) {
			clock_gettime(CLOCK_MONOTONIC, &n);
			now = &n;
		}
		elapsed = (uint32_t)eloop_timespec_diff(now, &ia->acquired,
		    NULL);
		if (ia->prefix_pltime != ND6_INFINITE_LIFETIME) {
			if (elapsed > ia->prefix_pltime)
				ia->prefix_pltime = 0;
			else
				ia->prefix_pltime -= elapsed;
		}
		if (ia->prefix_vltime != ND6_INFINITE_LIFETIME) {
			if (elapsed > ia->prefix_vltime)
				ia->prefix_vltime = 0;
			else
				ia->prefix_vltime -= elapsed;
		}
	}

	loglevel = ia->flags & IPV6_AF_NEW ? LOG_INFO : LOG_DEBUG;
	logmessage(loglevel, "%s: adding %saddress %s", ifp->name,
#ifdef IPV6_AF_TEMPORARY
	    ia->flags & IPV6_AF_TEMPORARY ? "temporary " : "",
#else
	    "",
#endif
	    ia->saddr);
	if (ia->prefix_pltime == ND6_INFINITE_LIFETIME &&
	    ia->prefix_vltime == ND6_INFINITE_LIFETIME)
		logdebugx("%s: pltime infinity, vltime infinity",
		    ifp->name);
	else if (ia->prefix_pltime == ND6_INFINITE_LIFETIME)
		logdebugx("%s: pltime infinity, vltime %"PRIu32" seconds",
		    ifp->name, ia->prefix_vltime);
	else if (ia->prefix_vltime == ND6_INFINITE_LIFETIME)
		logdebugx("%s: pltime %"PRIu32"seconds, vltime infinity",
		    ifp->name, ia->prefix_pltime);
	else
		logdebugx("%s: pltime %"PRIu32" seconds, vltime %"PRIu32
		    " seconds",
		    ifp->name, ia->prefix_pltime, ia->prefix_vltime);

	if (if_address6(RTM_NEWADDR, ia) == -1) {
		logerr(__func__);
		/* Restore real pltime and vltime */
		ia->prefix_pltime = pltime;
		ia->prefix_vltime = vltime;
		return -1;
	}

#ifdef IPV6_MANAGETEMPADDR
	/* RFC4941 Section 3.4 */
	if (ia->flags & IPV6_AF_TEMPORARY &&
	    ia->prefix_pltime &&
	    ia->prefix_vltime &&
	    ifp->options->options & DHCPCD_SLAACTEMP)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    ia->prefix_pltime - REGEN_ADVANCE,
		    ipv6_regentempaddr, ia);
#endif

	/* Restore real pltime and vltime */
	ia->prefix_pltime = pltime;
	ia->prefix_vltime = vltime;

	ia->flags &= ~IPV6_AF_NEW;
	ia->flags |= IPV6_AF_ADDED;
#ifndef SMALL
	if (ia->delegating_prefix != NULL)
		ia->flags |= IPV6_AF_DELEGATED;
#endif

#ifdef IPV6_POLLADDRFLAG
	eloop_timeout_delete(ifp->ctx->eloop,
		ipv6_checkaddrflags, ia);
	if (!(ia->flags & IPV6_AF_DADCOMPLETED)) {
		eloop_timeout_add_msec(ifp->ctx->eloop,
		    RETRANS_TIMER / 2, ipv6_checkaddrflags, ia);
	}
#endif

#ifdef __sun
	/* Solaris does not announce new addresses which need DaD
	 * so we need to take a copy and add it to our list.
	 * Otherwise aliasing gets confused if we add another
	 * address during DaD. */

	state = IPV6_STATE(ifp);
	TAILQ_FOREACH(ia2, &state->addrs, next) {
		if (IN6_ARE_ADDR_EQUAL(&ia2->addr, &ia->addr))
			break;
	}
	if (ia2 == NULL) {
		if ((ia2 = malloc(sizeof(*ia2))) == NULL) {
			logerr(__func__);
			return 0; /* Well, we did add the address */
		}
		memcpy(ia2, ia, sizeof(*ia2));
		TAILQ_INSERT_TAIL(&state->addrs, ia2, next);
	}
#endif

#ifdef ND6_ADVERTISE
#ifdef __sun
advertise:
#endif
	/* Re-advertise the preferred address to be safe. */
	if (!vltime_was_zero)
		ipv6nd_advertise(ia);
#endif

	return 0;
}

#ifdef ALIAS_ADDR
/* Find the next logical alias address we can use. */
static int
ipv6_aliasaddr(struct ipv6_addr *ia, struct ipv6_addr **repl)
{
	struct ipv6_state *state;
	struct ipv6_addr *iap;
	unsigned int lun;
	char alias[IF_NAMESIZE];

	if (ia->alias[0] != '\0')
		return 0;
	state = IPV6_STATE(ia->iface);

	/* First find an existng address.
	 * This can happen when dhcpcd restarts as ND and DHCPv6
	 * maintain their own lists of addresses. */
	TAILQ_FOREACH(iap, &state->addrs, next) {
		if (iap->alias[0] != '\0' &&
		    IN6_ARE_ADDR_EQUAL(&iap->addr, &ia->addr))
		{
			strlcpy(ia->alias, iap->alias, sizeof(ia->alias));
			return 0;
		}
	}

	lun = 0;
find_unit:
	if (if_makealias(alias, IF_NAMESIZE, ia->iface->name, lun) >=
	    IF_NAMESIZE)
	{
		errno = ENOMEM;
		return -1;
	}
	TAILQ_FOREACH(iap, &state->addrs, next) {
		if (iap->alias[0] == '\0')
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&iap->addr)) {
			/* No address assigned? Lets use it. */
			strlcpy(ia->alias, iap->alias, sizeof(ia->alias));
			if (repl)
				*repl = iap;
			return 1;
		}
		if (strcmp(iap->alias, alias) == 0)
			break;
	}

	if (iap != NULL) {
		if (lun == UINT_MAX) {
			errno = ERANGE;
			return -1;
		}
		lun++;
		goto find_unit;
	}

	strlcpy(ia->alias, alias, sizeof(ia->alias));
	return 0;
}
#endif

int
ipv6_addaddr(struct ipv6_addr *ia, const struct timespec *now)
{
	int r;
#ifdef ALIAS_ADDR
	int replaced, blank;
	struct ipv6_addr *replaced_ia;

	blank = (ia->alias[0] == '\0');
	if ((replaced = ipv6_aliasaddr(ia, &replaced_ia)) == -1)
		return -1;
	if (blank)
		logdebugx("%s: aliased %s", ia->alias, ia->saddr);
#endif

	if ((r = ipv6_addaddr1(ia, now)) == 0) {
#ifdef ALIAS_ADDR
		if (replaced) {
			struct ipv6_state *state;

			state = IPV6_STATE(ia->iface);
			TAILQ_REMOVE(&state->addrs, replaced_ia, next);
			ipv6_freeaddr(replaced_ia);
		}
#endif
	}
	return r;
}

int
ipv6_findaddrmatch(const struct ipv6_addr *addr, const struct in6_addr *match,
    unsigned int flags)
{

	if (match == NULL) {
		if ((addr->flags &
		    (IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED)) ==
		    (IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED))
			return 1;
	} else if (addr->prefix_vltime &&
	    IN6_ARE_ADDR_EQUAL(&addr->addr, match) &&
	    (!flags || addr->flags & flags))
		return 1;

	return 0;
}

struct ipv6_addr *
ipv6_findaddr(struct dhcpcd_ctx *ctx, const struct in6_addr *addr, unsigned int flags)
{
	struct ipv6_addr *nap;
#ifdef DHCP6
	struct ipv6_addr *dap;
#endif

	nap = ipv6nd_findaddr(ctx, addr, flags);
#ifdef DHCP6
	dap = dhcp6_findaddr(ctx, addr, flags);
	if (!dap && !nap)
		return NULL;
	if (dap && !nap)
		return dap;
	if (nap && !dap)
		return nap;
	if (nap->iface->metric < dap->iface->metric)
		return nap;
	return dap;
#else
	return nap;
#endif
}

int
ipv6_doaddr(struct ipv6_addr *ia, struct timespec *now)
{

	/* A delegated prefix is not an address. */
	if (ia->flags & IPV6_AF_DELEGATEDPFX)
		return 0;

	if (ia->prefix_vltime == 0) {
		if (ia->flags & IPV6_AF_ADDED)
			ipv6_deleteaddr(ia);
		eloop_q_timeout_delete(ia->iface->ctx->eloop,
		    ELOOP_QUEUE_ALL, NULL, ia);
		if (ia->flags & IPV6_AF_REQUEST) {
			ia->flags &= ~IPV6_AF_ADDED;
			return 0;
		}
		return -1;
	}

	if (ia->flags & IPV6_AF_STALE ||
	    IN6_IS_ADDR_UNSPECIFIED(&ia->addr))
		return 0;

	if (!timespecisset(now))
		clock_gettime(CLOCK_MONOTONIC, now);
	ipv6_addaddr(ia, now);
	return ia->flags & IPV6_AF_NEW ? 1 : 0;
}

ssize_t
ipv6_addaddrs(struct ipv6_addrhead *iaddrs)
{
	struct timespec now;
	struct ipv6_addr *ia, *ian;
	ssize_t i, r;

	i = 0;
	timespecclear(&now);
	TAILQ_FOREACH_SAFE(ia, iaddrs, next, ian) {
		r = ipv6_doaddr(ia, &now);
		if (r != 0)
			i++;
		if (r == -1) {
			TAILQ_REMOVE(iaddrs, ia, next);
			ipv6_freeaddr(ia);
		}
	}
	return i;
}

void
ipv6_freeaddr(struct ipv6_addr *ia)
{
	struct eloop *eloop = ia->iface->ctx->eloop;
#ifndef SMALL
	struct ipv6_addr *iad;

	/* Forget the reference */
	if (ia->flags & IPV6_AF_DELEGATEDPFX) {
		TAILQ_FOREACH(iad, &ia->pd_pfxs, pd_next) {
			iad->delegating_prefix = NULL;
		}
	} else if (ia->delegating_prefix != NULL) {
		TAILQ_REMOVE(&ia->delegating_prefix->pd_pfxs, ia, pd_next);
	}
#endif

	if (ia->dhcp6_fd != -1) {
		close(ia->dhcp6_fd);
		eloop_event_delete(eloop, ia->dhcp6_fd);
	}

	eloop_q_timeout_delete(eloop, ELOOP_QUEUE_ALL, NULL, ia);
	free(ia->na);
	free(ia);
}

void
ipv6_freedrop_addrs(struct ipv6_addrhead *addrs, int drop,
    const struct interface *ifd)
{
	struct ipv6_addr *ap, *apn, *apf;
	struct timespec now;

#ifdef SMALL
	UNUSED(ifd);
#endif
	timespecclear(&now);
	TAILQ_FOREACH_SAFE(ap, addrs, next, apn) {
#ifndef SMALL
		if (ifd != NULL &&
		    (ap->delegating_prefix == NULL ||
		    ap->delegating_prefix->iface != ifd))
			continue;
#endif
		if (drop != 2)
			TAILQ_REMOVE(addrs, ap, next);
		if (drop && ap->flags & IPV6_AF_ADDED &&
		    (ap->iface->options->options &
		    (DHCPCD_EXITING | DHCPCD_PERSISTENT)) !=
		    (DHCPCD_EXITING | DHCPCD_PERSISTENT))
		{
			/* Don't drop link-local addresses. */
			if (!IN6_IS_ADDR_LINKLOCAL(&ap->addr) ||
			    CAN_DROP_LLADDR(ap->iface))
			{
				if (drop == 2)
					TAILQ_REMOVE(addrs, ap, next);
				/* Find the same address somewhere else */
				apf = ipv6_findaddr(ap->iface->ctx, &ap->addr,
				    0);
				if ((apf == NULL ||
				    (apf->iface != ap->iface)))
					ipv6_deleteaddr(ap);
				if (!(ap->iface->options->options &
				    DHCPCD_EXITING) && apf)
				{
					if (!timespecisset(&now))
						clock_gettime(CLOCK_MONOTONIC,
						    &now);
					ipv6_addaddr(apf, &now);
				}
				if (drop == 2)
					ipv6_freeaddr(ap);
			}
		}
		if (drop != 2)
			ipv6_freeaddr(ap);
	}
}

static struct ipv6_state *
ipv6_getstate(struct interface *ifp)
{
	struct ipv6_state *state;

	state = IPV6_STATE(ifp);
	if (state == NULL) {
	        ifp->if_data[IF_DATA_IPV6] = calloc(1, sizeof(*state));
		state = IPV6_STATE(ifp);
		if (state == NULL) {
			logerr(__func__);
			return NULL;
		}
		TAILQ_INIT(&state->addrs);
		TAILQ_INIT(&state->ll_callbacks);
	}
	return state;
}

struct ipv6_addr *
ipv6_anyglobal(struct interface *sifp)
{
	struct interface *ifp;
	struct ipv6_state *state;
	struct ipv6_addr *ia;
	bool forwarding;

	/* BSD forwarding is either on or off.
	 * Linux forwarding is technically the same as it's
	 * configured by the "all" interface.
	 * Per interface only affects IsRouter of NA messages. */
#if defined(PRIVSEP) && (defined(HAVE_PLEDGE) || defined(__linux__))
	if (IN_PRIVSEP(sifp->ctx))
		forwarding = ps_root_ip6forwarding(sifp->ctx, NULL) != 0;
	else
#endif
		forwarding = ip6_forwarding(NULL) != 0;

	TAILQ_FOREACH(ifp, sifp->ctx->ifaces, next) {
		if (ifp != sifp && !forwarding)
			continue;

		state = IPV6_STATE(ifp);
		if (state == NULL)
			continue;

		TAILQ_FOREACH(ia, &state->addrs, next) {
			if (IN6_IS_ADDR_LINKLOCAL(&ia->addr))
				continue;
			/* Let's be optimistic.
			 * Any decent OS won't forward or accept traffic
			 * from/to tentative or detached addresses. */
			if (!(ia->addr_flags & IN6_IFF_DUPLICATED))
				return ia;
		}
	}
	return NULL;
}

void
ipv6_handleifa(struct dhcpcd_ctx *ctx,
    int cmd, struct if_head *ifs, const char *ifname,
    const struct in6_addr *addr, uint8_t prefix_len, int addrflags, pid_t pid)
{
	struct interface *ifp;
	struct ipv6_state *state;
	struct ipv6_addr *ia;
	struct ll_callback *cb;
	bool anyglobal;

#ifdef __sun
	struct sockaddr_in6 subnet;

	/* Solaris on-link route is an unspecified address! */
	if (IN6_IS_ADDR_UNSPECIFIED(addr)) {
		if (if_getsubnet(ctx, ifname, AF_INET6,
		    &subnet, sizeof(subnet)) == -1)
		{
			logerr(__func__);
			return;
		}
		addr = &subnet.sin6_addr;
	}
#endif

#if 0
	char dbuf[INET6_ADDRSTRLEN];
	const char *dbp;

	dbp = inet_ntop(AF_INET6, &addr->s6_addr,
	    dbuf, INET6_ADDRSTRLEN);
	loginfox("%s: cmd %d addr %s addrflags %d",
	    ifname, cmd, dbp, addrflags);
#endif

	if (ifs == NULL)
		ifs = ctx->ifaces;
	if (ifs == NULL)
		return;
	if ((ifp = if_find(ifs, ifname)) == NULL)
		return;
	if ((state = ipv6_getstate(ifp)) == NULL)
		return;
	anyglobal = ipv6_anyglobal(ifp) != NULL;

	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (IN6_ARE_ADDR_EQUAL(&ia->addr, addr))
			break;
	}

	switch (cmd) {
	case RTM_DELADDR:
		if (ia != NULL) {
			TAILQ_REMOVE(&state->addrs, ia, next);
#ifdef ND6_ADVERTISE
			/* Advertise the address if it exists on
			 * another interface. */
			ipv6nd_advertise(ia);
#endif
			/* We'll free it at the end of the function. */
		}
		break;
	case RTM_NEWADDR:
		if (ia == NULL) {
			ia = ipv6_newaddr(ifp, addr, prefix_len, 0);
#ifdef ALIAS_ADDR
			strlcpy(ia->alias, ifname, sizeof(ia->alias));
#endif
			if (if_getlifetime6(ia) == -1) {
				/* No support or address vanished.
				 * Either way, just set a deprecated
				 * infinite time lifetime and continue.
				 * This is fine because we only want
				 * to know this when trying to extend
				 * temporary addresses.
				 * As we can't extend infinite, we'll
				 * create a new temporary address. */
				ia->prefix_pltime = 0;
				ia->prefix_vltime =
				    ND6_INFINITE_LIFETIME;
			}
			/* This is a minor regression against RFC 4941
			 * because the kernel only knows when the
			 * lifetimes were last updated, not when the
			 * address was initially created.
			 * Provided dhcpcd is not restarted, this
			 * won't be a problem.
			 * If we don't like it, we can always
			 * pretend lifetimes are infinite and always
			 * generate a new temporary address on
			 * restart. */
			ia->acquired = ia->created;
			TAILQ_INSERT_TAIL(&state->addrs, ia, next);
		}
		ia->addr_flags = addrflags;
		ia->flags &= ~IPV6_AF_STALE;
#ifdef IPV6_MANAGETEMPADDR
		if (ia->addr_flags & IN6_IFF_TEMPORARY)
			ia->flags |= IPV6_AF_TEMPORARY;
#endif
		if (IN6_IS_ADDR_LINKLOCAL(&ia->addr) || ia->dadcallback) {
#ifdef IPV6_POLLADDRFLAG
			if (ia->addr_flags & IN6_IFF_TENTATIVE) {
				eloop_timeout_add_msec(
				    ia->iface->ctx->eloop,
				    RETRANS_TIMER / 2, ipv6_checkaddrflags, ia);
				break;
			}
#endif

			if (ia->dadcallback)
				ia->dadcallback(ia);

			if (IN6_IS_ADDR_LINKLOCAL(&ia->addr) &&
			    !(ia->addr_flags & IN6_IFF_NOTUSEABLE))
			{
				/* Now run any callbacks.
				 * Typically IPv6RS or DHCPv6 */
				while ((cb =
				    TAILQ_FIRST(&state->ll_callbacks)))
				{
					TAILQ_REMOVE(
					    &state->ll_callbacks,
					    cb, next);
					cb->callback(cb->arg);
					free(cb);
				}
			}
		}
		break;
	}

	if (ia == NULL)
		return;

	ctx->options &= ~DHCPCD_RTBUILD;
	ipv6nd_handleifa(cmd, ia, pid);
#ifdef DHCP6
	dhcp6_handleifa(cmd, ia, pid);
#endif

	/* Done with the ia now, so free it. */
	if (cmd == RTM_DELADDR)
		ipv6_freeaddr(ia);
	else if (!(ia->addr_flags & IN6_IFF_NOTUSEABLE))
		ia->flags |= IPV6_AF_DADCOMPLETED;

	/* If we've not already called rt_build via the IPv6ND
	 * or DHCP6 handlers and the existance of any useable
	 * global address on the interface has changed,
	 * call rt_build to add/remove the default route. */
	if (ifp->active &&
	    ((ifp->options != NULL && ifp->options->options & DHCPCD_IPV6) ||
	     (ifp->options == NULL && ctx->options & DHCPCD_IPV6)) &&
	    !(ctx->options & DHCPCD_RTBUILD) &&
	    (ipv6_anyglobal(ifp) != NULL) != anyglobal)
		rt_build(ctx, AF_INET6);
}

int
ipv6_hasaddr(const struct interface *ifp)
{

	if (ipv6nd_iffindaddr(ifp, NULL, 0) != NULL)
		return 1;
#ifdef DHCP6
	if (dhcp6_iffindaddr(ifp, NULL, 0) != NULL)
		return 1;
#endif
	return 0;
}

struct ipv6_addr *
ipv6_iffindaddr(struct interface *ifp, const struct in6_addr *addr,
    int revflags)
{
	struct ipv6_state *state;
	struct ipv6_addr *ap;

	state = IPV6_STATE(ifp);
	if (state) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (addr == NULL) {
				if (IN6_IS_ADDR_LINKLOCAL(&ap->addr) &&
				    (!revflags || !(ap->addr_flags & revflags)))
					return ap;
			} else {
				if (IN6_ARE_ADDR_EQUAL(&ap->addr, addr) &&
				    (!revflags || !(ap->addr_flags & revflags)))
					return ap;
			}
		}
	}
	return NULL;
}

static struct ipv6_addr *
ipv6_iffindmaskaddr(const struct interface *ifp, const struct in6_addr *addr)
{
	struct ipv6_state *state;
	struct ipv6_addr *ap;
	struct in6_addr mask;

	state = IPV6_STATE(ifp);
	if (state) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ipv6_mask(&mask, ap->prefix_len) == -1)
				continue;
			if (IN6_ARE_MASKED_ADDR_EQUAL(&ap->addr, addr, &mask))
				return ap;
		}
	}
	return NULL;
}

struct ipv6_addr *
ipv6_findmaskaddr(struct dhcpcd_ctx *ctx, const struct in6_addr *addr)
{
	struct interface *ifp;
	struct ipv6_addr *ap;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		ap = ipv6_iffindmaskaddr(ifp, addr);
		if (ap != NULL)
			return ap;
	}
	return NULL;
}

int
ipv6_addlinklocalcallback(struct interface *ifp,
    void (*callback)(void *), void *arg)
{
	struct ipv6_state *state;
	struct ll_callback *cb;

	state = ipv6_getstate(ifp);
	TAILQ_FOREACH(cb, &state->ll_callbacks, next) {
		if (cb->callback == callback && cb->arg == arg)
			break;
	}
	if (cb == NULL) {
		cb = malloc(sizeof(*cb));
		if (cb == NULL) {
			logerr(__func__);
			return -1;
		}
		cb->callback = callback;
		cb->arg = arg;
		TAILQ_INSERT_TAIL(&state->ll_callbacks, cb, next);
	}
	return 0;
}

static struct ipv6_addr *
ipv6_newlinklocal(struct interface *ifp)
{
	struct ipv6_addr *ia;
	struct in6_addr in6;

	memset(&in6, 0, sizeof(in6));
	in6.s6_addr32[0] = htonl(0xfe800000);
	ia = ipv6_newaddr(ifp, &in6, 64, 0);
	if (ia != NULL) {
		ia->prefix_pltime = ND6_INFINITE_LIFETIME;
		ia->prefix_vltime = ND6_INFINITE_LIFETIME;
	}
	return ia;
}

static const uint8_t allzero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t allone[8] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int
ipv6_addlinklocal(struct interface *ifp)
{
	struct ipv6_state *state;
	struct ipv6_addr *ap, *ap2;
	int dadcounter;

	if (!(ifp->options->options & DHCPCD_CONFIGURE))
		return 0;

	/* Check sanity before malloc */
	if (!(ifp->options->options & DHCPCD_SLAACPRIVATE)) {
		switch (ifp->hwtype) {
		case ARPHRD_ETHER:
			/* Check for a valid hardware address */
			if (ifp->hwlen != 6 && ifp->hwlen != 8) {
				errno = ENOTSUP;
				return -1;
			}
			if (memcmp(ifp->hwaddr, allzero, ifp->hwlen) == 0 ||
			    memcmp(ifp->hwaddr, allone, ifp->hwlen) == 0)
			{
				errno = EINVAL;
				return -1;
			}
			break;
		default:
			errno = ENOTSUP;
			return -1;
		}
	}

	state = ipv6_getstate(ifp);
	if (state == NULL)
		return -1;

	ap = ipv6_newlinklocal(ifp);
	if (ap == NULL)
		return -1;

	dadcounter = 0;
	if (ifp->options->options & DHCPCD_SLAACPRIVATE) {
nextslaacprivate:
		if (ipv6_makestableprivate(&ap->addr,
			&ap->prefix, ap->prefix_len, ifp, &dadcounter) == -1)
		{
			free(ap);
			return -1;
		}
		ap->dadcounter = dadcounter;
	} else {
		memcpy(ap->addr.s6_addr, ap->prefix.s6_addr, 8);
		switch (ifp->hwtype) {
		case ARPHRD_ETHER:
			if (ifp->hwlen == 6) {
				ap->addr.s6_addr[ 8] = ifp->hwaddr[0];
				ap->addr.s6_addr[ 9] = ifp->hwaddr[1];
				ap->addr.s6_addr[10] = ifp->hwaddr[2];
				ap->addr.s6_addr[11] = 0xff;
				ap->addr.s6_addr[12] = 0xfe;
				ap->addr.s6_addr[13] = ifp->hwaddr[3];
				ap->addr.s6_addr[14] = ifp->hwaddr[4];
				ap->addr.s6_addr[15] = ifp->hwaddr[5];
			} else if (ifp->hwlen == 8)
				memcpy(&ap->addr.s6_addr[8], ifp->hwaddr, 8);
			else {
				free(ap);
				errno = ENOTSUP;
				return -1;
			}
			break;
		}

		/* Sanity check: g bit must not indciate "group" */
		if (EUI64_GROUP(&ap->addr)) {
			free(ap);
			errno = EINVAL;
			return -1;
		}
		EUI64_TO_IFID(&ap->addr);
	}

	/* Do we already have this address? */
	TAILQ_FOREACH(ap2, &state->addrs, next) {
		if (IN6_ARE_ADDR_EQUAL(&ap->addr, &ap2->addr)) {
			if (ap2->addr_flags & IN6_IFF_DUPLICATED) {
				if (ifp->options->options &
				    DHCPCD_SLAACPRIVATE)
				{
					dadcounter++;
					goto nextslaacprivate;
				}
				free(ap);
				errno = EADDRNOTAVAIL;
				return -1;
			}

			logwarnx("%s: waiting for %s to complete",
			    ap2->iface->name, ap2->saddr);
			free(ap);
			errno =	EEXIST;
			return 0;
		}
	}

	inet_ntop(AF_INET6, &ap->addr, ap->saddr, sizeof(ap->saddr));
	TAILQ_INSERT_TAIL(&state->addrs, ap, next);
	ipv6_addaddr(ap, NULL);
	return 1;
}

static int
ipv6_tryaddlinklocal(struct interface *ifp)
{
	struct ipv6_addr *ia;

	/* We can't assign a link-locak address to this,
	 * the ppp process has to. */
	if (ifp->flags & IFF_POINTOPOINT)
		return 0;

	ia = ipv6_iffindaddr(ifp, NULL, IN6_IFF_DUPLICATED);
	if (ia != NULL) {
#ifdef IPV6_POLLADDRFLAG
		if (ia->addr_flags & IN6_IFF_TENTATIVE) {
			eloop_timeout_add_msec(
			    ia->iface->ctx->eloop,
			    RETRANS_TIMER / 2, ipv6_checkaddrflags, ia);
		}
#endif
		return 0;
	}
	if (!CAN_ADD_LLADDR(ifp))
		return 0;

	return ipv6_addlinklocal(ifp);
}

void
ipv6_setscope(struct sockaddr_in6 *sin, unsigned int ifindex)
{

#ifdef __KAME__
	/* KAME based systems want to store the scope inside the sin6_addr
	 * for link local addresses */
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
		uint16_t scope = htons((uint16_t)ifindex);
		memcpy(&sin->sin6_addr.s6_addr[2], &scope,
		    sizeof(scope));
	}
	sin->sin6_scope_id = 0;
#else
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr))
		sin->sin6_scope_id = ifindex;
	else
		sin->sin6_scope_id = 0;
#endif
}

unsigned int
ipv6_getscope(const struct sockaddr_in6 *sin)
{
#ifdef __KAME__
	uint16_t scope;
#endif

	if (!IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr))
		return 0;
#ifdef __KAME__
	memcpy(&scope, &sin->sin6_addr.s6_addr[2], sizeof(scope));
	return (unsigned int)ntohs(scope);
#else
	return (unsigned int)sin->sin6_scope_id;
#endif
}

struct ipv6_addr *
ipv6_newaddr(struct interface *ifp, const struct in6_addr *addr,
    uint8_t prefix_len, unsigned int flags)
{
	struct ipv6_addr *ia, *iaf;
	char buf[INET6_ADDRSTRLEN];
	const char *cbp;
	bool tempaddr;
	int addr_flags;

#ifdef IPV6_AF_TEMPORARY
	tempaddr = flags & IPV6_AF_TEMPORARY;
#else
	tempaddr = false;
#endif

	/* If adding a new DHCP / RA derived address, check current flags
	 * from an existing address. */
	if (tempaddr)
		iaf = NULL;
	else if (flags & IPV6_AF_AUTOCONF)
		iaf = ipv6nd_iffindprefix(ifp, addr, prefix_len);
	else
		iaf = ipv6_iffindaddr(ifp, addr, 0);
	if (iaf != NULL) {
		addr_flags = iaf->addr_flags;
		flags |= IPV6_AF_ADDED;
	} else
		addr_flags = IN6_IFF_TENTATIVE;

	ia = calloc(1, sizeof(*ia));
	if (ia == NULL)
		goto err;

	ia->iface = ifp;
	ia->addr_flags = addr_flags;
	ia->flags = IPV6_AF_NEW | flags;
	if (!(ia->addr_flags & IN6_IFF_NOTUSEABLE))
		ia->flags |= IPV6_AF_DADCOMPLETED;
	ia->prefix_len = prefix_len;
	ia->dhcp6_fd = -1;

#ifndef SMALL
	TAILQ_INIT(&ia->pd_pfxs);
#endif

	if (prefix_len == 128)
		goto makepfx;
	else if (ia->flags & IPV6_AF_AUTOCONF) {
		ia->prefix = *addr;
		if (iaf != NULL)
			memcpy(&ia->addr, &iaf->addr, sizeof(ia->addr));
		else {
			ia->dadcounter = ipv6_makeaddr(&ia->addr, ifp,
			                               &ia->prefix,
						       ia->prefix_len,
						       ia->flags);
			if (ia->dadcounter == -1)
				goto err;
		}
	} else if (ia->flags & IPV6_AF_RAPFX) {
		ia->prefix = *addr;
#ifdef __sun
		ia->addr = *addr;
		cbp = inet_ntop(AF_INET6, &ia->addr, buf, sizeof(buf));
		goto paddr;
#else
		return ia;
#endif
	} else if (ia->flags & (IPV6_AF_REQUEST | IPV6_AF_DELEGATEDPFX)) {
		ia->prefix = *addr;
		cbp = inet_ntop(AF_INET6, &ia->prefix, buf, sizeof(buf));
		goto paddr;
	} else {
makepfx:
		ia->addr = *addr;
		if (ipv6_makeprefix(&ia->prefix,
		                    &ia->addr, ia->prefix_len) == -1)
			goto err;
	}

	cbp = inet_ntop(AF_INET6, &ia->addr, buf, sizeof(buf));
paddr:
	if (cbp == NULL)
		goto err;
	snprintf(ia->saddr, sizeof(ia->saddr), "%s/%d", cbp, ia->prefix_len);

	return ia;

err:
	logerr(__func__);
	free(ia);
	return NULL;
}

static void
ipv6_staticdadcallback(void *arg)
{
	struct ipv6_addr *ia = arg;
	int wascompleted;

	wascompleted = (ia->flags & IPV6_AF_DADCOMPLETED);
	ia->flags |= IPV6_AF_DADCOMPLETED;
	if (ia->addr_flags & IN6_IFF_DUPLICATED)
		logwarnx("%s: DAD detected %s", ia->iface->name,
		    ia->saddr);
	else if (!wascompleted) {
		logdebugx("%s: IPv6 static DAD completed",
		    ia->iface->name);
	}

#define FINISHED (IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED)
	if (!wascompleted) {
		struct interface *ifp;
		struct ipv6_state *state;

		ifp = ia->iface;
		state = IPV6_STATE(ifp);
		TAILQ_FOREACH(ia, &state->addrs, next) {
			if (ia->flags & IPV6_AF_STATIC &&
			    (ia->flags & FINISHED) != FINISHED)
			{
				wascompleted = 1;
				break;
			}
		}
		if (!wascompleted)
			script_runreason(ifp, "STATIC6");
	}
#undef FINISHED
}

ssize_t
ipv6_env(FILE *fp, const char *prefix, const struct interface *ifp)
{
	struct ipv6_addr *ia;

	ia = ipv6_iffindaddr(UNCONST(ifp), &ifp->options->req_addr6,
	    IN6_IFF_NOTUSEABLE);
	if (ia == NULL)
		return 0;
	if (efprintf(fp, "%s_ip6_address=%s", prefix, ia->saddr) == -1)
		return -1;
	return 1;
}

int
ipv6_staticdadcompleted(const struct interface *ifp)
{
	const struct ipv6_state *state;
	const struct ipv6_addr *ia;
	int n;

	if ((state = IPV6_CSTATE(ifp)) == NULL)
		return 0;
	n = 0;
#define COMPLETED (IPV6_AF_STATIC | IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED)
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if ((ia->flags & COMPLETED) == COMPLETED &&
		    !(ia->addr_flags & IN6_IFF_NOTUSEABLE))
			n++;
	}
	return n;
}

int
ipv6_startstatic(struct interface *ifp)
{
	struct ipv6_addr *ia;
	int run_script;

	if (IN6_IS_ADDR_UNSPECIFIED(&ifp->options->req_addr6))
		return 0;

	ia = ipv6_iffindaddr(ifp, &ifp->options->req_addr6, 0);
	if (ia != NULL &&
	    (ia->prefix_len != ifp->options->req_prefix_len ||
	    ia->addr_flags & IN6_IFF_NOTUSEABLE))
	{
		ipv6_deleteaddr(ia);
		ia = NULL;
	}
	if (ia == NULL) {
		struct ipv6_state *state;

		ia = ipv6_newaddr(ifp, &ifp->options->req_addr6,
		    ifp->options->req_prefix_len, 0);
		if (ia == NULL)
			return -1;
		state = IPV6_STATE(ifp);
		TAILQ_INSERT_TAIL(&state->addrs, ia, next);
		run_script = 0;
	} else
		run_script = 1;
	ia->flags |= IPV6_AF_STATIC | IPV6_AF_ONLINK;
	ia->prefix_vltime = ND6_INFINITE_LIFETIME;
	ia->prefix_pltime = ND6_INFINITE_LIFETIME;
	ia->dadcallback = ipv6_staticdadcallback;
	ipv6_addaddr(ia, NULL);
	rt_build(ifp->ctx, AF_INET6);
	if (run_script)
		script_runreason(ifp, "STATIC6");
	return 1;
}

/* Ensure the interface has a link-local address */
int
ipv6_start(struct interface *ifp)
{
#ifdef IPV6_POLLADDRFLAG
	struct ipv6_state *state;

	/* We need to update the address flags. */
	if ((state = IPV6_STATE(ifp)) != NULL) {
		struct ipv6_addr *ia;
		const char *alias;
		int flags;

		TAILQ_FOREACH(ia, &state->addrs, next) {
#ifdef ALIAS_ADDR
			alias = ia->alias;
#else
			alias = NULL;
#endif
			flags = if_addrflags6(ia->iface, &ia->addr, alias);
			if (flags != -1)
				ia->addr_flags = flags;
		}
	}
#endif

	if (ipv6_tryaddlinklocal(ifp) == -1)
		return -1;

	return 0;
}

void
ipv6_freedrop(struct interface *ifp, int drop)
{
	struct ipv6_state *state;
	struct ll_callback *cb;

	if (ifp == NULL)
		return;

	if ((state = IPV6_STATE(ifp)) == NULL)
		return;

	/* If we got here, we can get rid of any LL callbacks. */
	while ((cb = TAILQ_FIRST(&state->ll_callbacks))) {
		TAILQ_REMOVE(&state->ll_callbacks, cb, next);
		free(cb);
	}

	ipv6_freedrop_addrs(&state->addrs, drop ? 2 : 0, NULL);
	if (drop) {
		if (ifp->ctx->ra_routers != NULL)
			rt_build(ifp->ctx, AF_INET6);
	} else {
		/* Because we need to cache the addresses we don't control,
		 * we only free the state on when NOT dropping addresses. */
		free(state);
		ifp->if_data[IF_DATA_IPV6] = NULL;
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	}
}

void
ipv6_ctxfree(struct dhcpcd_ctx *ctx)
{

	free(ctx->ra_routers);
	free(ctx->secret);
}

int
ipv6_handleifa_addrs(int cmd,
    struct ipv6_addrhead *addrs, const struct ipv6_addr *addr, pid_t pid)
{
	struct ipv6_addr *ia, *ian;
	uint8_t found, alldadcompleted;

	alldadcompleted = 1;
	found = 0;
	TAILQ_FOREACH_SAFE(ia, addrs, next, ian) {
		if (!IN6_ARE_ADDR_EQUAL(&addr->addr, &ia->addr)) {
			if (ia->flags & IPV6_AF_ADDED &&
			    !(ia->flags & IPV6_AF_DADCOMPLETED))
				alldadcompleted = 0;
			continue;
		}
		switch (cmd) {
		case RTM_DELADDR:
			if (ia->flags & IPV6_AF_ADDED) {
				logwarnx("%s: pid %d deleted address %s",
				    ia->iface->name, pid, ia->saddr);
				ia->flags &= ~IPV6_AF_ADDED;
			}
			ipv6_deletedaddr(ia);
			if (ia->flags & IPV6_AF_DELEGATED) {
				TAILQ_REMOVE(addrs, ia, next);
				ipv6_freeaddr(ia);
			}
			break;
		case RTM_NEWADDR:
			ia->addr_flags = addr->addr_flags;
			/* Safety - ignore tentative announcements */
			if (ia->addr_flags &
			    (IN6_IFF_DETACHED | IN6_IFF_TENTATIVE))
				break;
			if ((ia->flags & IPV6_AF_DADCOMPLETED) == 0) {
				found++;
				if (ia->dadcallback)
					ia->dadcallback(ia);
				/* We need to set this here in-case the
				 * dadcallback function checks it */
				ia->flags |= IPV6_AF_DADCOMPLETED;
			}
			break;
		}
	}

	return alldadcompleted ? found : 0;
}

#ifdef IPV6_MANAGETEMPADDR
static void
ipv6_regen_desync(struct interface *ifp, bool force)
{
	struct ipv6_state *state;
	unsigned int max;

	state = IPV6_STATE(ifp);

	/* RFC4941 Section 5 states that DESYNC_FACTOR must never be
	 * greater than TEMP_VALID_LIFETIME - REGEN_ADVANCE.
	 * I believe this is an error and it should be never be greater than
	 * TEMP_PREFERRED_LIFETIME - REGEN_ADVANCE. */
	max = TEMP_PREFERRED_LIFETIME - REGEN_ADVANCE;
	if (state->desync_factor && !force && state->desync_factor < max)
		return;
	if (state->desync_factor == 0)
		state->desync_factor =
		    arc4random_uniform(MIN(MAX_DESYNC_FACTOR, max));
	max = TEMP_PREFERRED_LIFETIME - state->desync_factor - REGEN_ADVANCE;
	eloop_timeout_add_sec(ifp->ctx->eloop, max, ipv6_regentempaddrs, ifp);
}

/* RFC4941 Section 3.3.7 */
static void
ipv6_tempdadcallback(void *arg)
{
	struct ipv6_addr *ia = arg;

	if (ia->addr_flags & IN6_IFF_DUPLICATED) {
		struct ipv6_addr *ia1;
		struct timespec tv;

		if (++ia->dadcounter == TEMP_IDGEN_RETRIES) {
			logerrx("%s: too many duplicate temporary addresses",
			    ia->iface->name);
			return;
		}
		clock_gettime(CLOCK_MONOTONIC, &tv);
		if ((ia1 = ipv6_createtempaddr(ia, &tv)) == NULL)
			logerr(__func__);
		else
			ia1->dadcounter = ia->dadcounter;
		ipv6_deleteaddr(ia);
		if (ia1)
			ipv6_addaddr(ia1, &ia1->acquired);
	}
}

struct ipv6_addr *
ipv6_createtempaddr(struct ipv6_addr *ia0, const struct timespec *now)
{
	struct ipv6_state *state;
	struct interface *ifp = ia0->iface;
	struct ipv6_addr *ia;

	ia = ipv6_newaddr(ifp, &ia0->prefix, ia0->prefix_len,
	    IPV6_AF_AUTOCONF | IPV6_AF_TEMPORARY);
	if (ia == NULL)
		return NULL;

	ia->dadcallback = ipv6_tempdadcallback;
	ia->created = ia->acquired = now ? *now : ia0->acquired;

	/* Ensure desync is still valid */
	ipv6_regen_desync(ifp, false);

	/* RFC4941 Section 3.3.4 */
	state = IPV6_STATE(ia->iface);
	ia->prefix_pltime = MIN(ia0->prefix_pltime,
	    TEMP_PREFERRED_LIFETIME - state->desync_factor);
	ia->prefix_vltime = MIN(ia0->prefix_vltime, TEMP_VALID_LIFETIME);
	if (ia->prefix_pltime <= REGEN_ADVANCE ||
	    ia->prefix_pltime > ia0->prefix_vltime)
	{
		errno =	EINVAL;
		free(ia);
		return NULL;
	}

	TAILQ_INSERT_TAIL(&state->addrs, ia, next);
	return ia;
}

struct ipv6_addr *
ipv6_settemptime(struct ipv6_addr *ia, int flags)
{
	struct ipv6_state *state;
	struct ipv6_addr *ap, *first;

	state = IPV6_STATE(ia->iface);
	first = NULL;
	TAILQ_FOREACH_REVERSE(ap, &state->addrs, ipv6_addrhead, next) {
		if (ap->flags & IPV6_AF_TEMPORARY &&
		    ap->prefix_pltime &&
		    IN6_ARE_ADDR_EQUAL(&ia->prefix, &ap->prefix))
		{
			unsigned int max, ext;

			if (flags == 0) {
				if (ap->prefix_pltime -
				    (uint32_t)(ia->acquired.tv_sec -
				    ap->acquired.tv_sec)
				    < REGEN_ADVANCE)
					continue;

				return ap;
			}

			if (!(ap->flags & IPV6_AF_ADDED))
				ap->flags |= IPV6_AF_NEW | IPV6_AF_AUTOCONF;
			ap->flags &= ~IPV6_AF_STALE;

			/* RFC4941 Section 3.4
			 * Deprecated prefix, deprecate the temporary address */
			if (ia->prefix_pltime == 0) {
				ap->prefix_pltime = 0;
				goto valid;
			}

			/* Ensure desync is still valid */
			ipv6_regen_desync(ap->iface, false);

			/* RFC4941 Section 3.3.2
			 * Extend temporary times, but ensure that they
			 * never last beyond the system limit. */
			ext = (unsigned int)ia->acquired.tv_sec
			    + ia->prefix_pltime;
			max = (unsigned int)(ap->created.tv_sec +
			    TEMP_PREFERRED_LIFETIME -
			    state->desync_factor);
			if (ext < max)
				ap->prefix_pltime = ia->prefix_pltime;
			else
				ap->prefix_pltime =
				    (uint32_t)(max - ia->acquired.tv_sec);

valid:
			ext = (unsigned int)ia->acquired.tv_sec +
			    ia->prefix_vltime;
			max = (unsigned int)(ap->created.tv_sec +
			    TEMP_VALID_LIFETIME);
			if (ext < max)
				ap->prefix_vltime = ia->prefix_vltime;
			else
				ap->prefix_vltime =
				    (uint32_t)(max - ia->acquired.tv_sec);

			/* Just extend the latest matching prefix */
			ap->acquired = ia->acquired;

			/* If extending return the last match as
			 * it's the most current.
			 * If deprecating, deprecate any other addresses we
			 * may have, although this should not be needed */
			if (ia->prefix_pltime)
				return ap;
			if (first == NULL)
				first = ap;
		}
	}
	return first;
}

void
ipv6_addtempaddrs(struct interface *ifp, const struct timespec *now)
{
	struct ipv6_state *state;
	struct ipv6_addr *ia;

	state = IPV6_STATE(ifp);
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (ia->flags & IPV6_AF_TEMPORARY &&
		    !(ia->flags & IPV6_AF_STALE))
			ipv6_addaddr(ia, now);
	}
}

static void
ipv6_regentempaddr0(struct ipv6_addr *ia, struct timespec *tv)
{
	struct ipv6_addr *ia1;

	logdebugx("%s: regen temp addr %s", ia->iface->name, ia->saddr);
	ia1 = ipv6_createtempaddr(ia, tv);
	if (ia1)
		ipv6_addaddr(ia1, tv);
	else
		logerr(__func__);
}

static void
ipv6_regentempaddr(void *arg)
{
	struct timespec tv;

	clock_gettime(CLOCK_MONOTONIC, &tv);
	ipv6_regentempaddr0(arg, &tv);
}

void
ipv6_regentempaddrs(void *arg)
{
	struct interface *ifp = arg;
	struct timespec tv;
	struct ipv6_state *state;
	struct ipv6_addr *ia;

	state = IPV6_STATE(ifp);
	if (state == NULL)
		return;

	ipv6_regen_desync(ifp, true);

	clock_gettime(CLOCK_MONOTONIC, &tv);

	/* Mark addresses for regen so we don't infinite loop. */
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (ia->flags & IPV6_AF_TEMPORARY &&
		    ia->flags & IPV6_AF_ADDED &&
		    !(ia->flags & IPV6_AF_STALE))
			ia->flags |= IPV6_AF_REGEN;
		else
			ia->flags &= ~IPV6_AF_REGEN;
	}

	/* Now regen temp addrs */
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (ia->flags & IPV6_AF_REGEN) {
			ipv6_regentempaddr0(ia, &tv);
			ia->flags &= ~IPV6_AF_REGEN;
		}
	}
}
#endif /* IPV6_MANAGETEMPADDR */

void
ipv6_markaddrsstale(struct interface *ifp, unsigned int flags)
{
	struct ipv6_state *state;
	struct ipv6_addr *ia;

	state = IPV6_STATE(ifp);
	if (state == NULL)
		return;

	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (flags == 0 || ia->flags & flags)
			ia->flags |= IPV6_AF_STALE;
	}
}

void
ipv6_deletestaleaddrs(struct interface *ifp)
{
	struct ipv6_state *state;
	struct ipv6_addr *ia, *ia1;

	state = IPV6_STATE(ifp);
	if (state == NULL)
		return;

	TAILQ_FOREACH_SAFE(ia, &state->addrs, next, ia1) {
		if (ia->flags & IPV6_AF_STALE)
			ipv6_handleifa(ifp->ctx, RTM_DELADDR,
			    ifp->ctx->ifaces, ifp->name,
			    &ia->addr, ia->prefix_len, 0, getpid());
	}
}


static struct rt *
inet6_makeroute(struct interface *ifp, const struct ra *rap)
{
	struct rt *rt;

	if ((rt = rt_new(ifp)) == NULL)
		return NULL;

#ifdef HAVE_ROUTE_METRIC
	rt->rt_metric = ifp->metric;
#endif
	if (rap != NULL)
		rt->rt_mtu = rap->mtu;
	return rt;
}

static struct rt *
inet6_makeprefix(struct interface *ifp, const struct ra *rap,
    const struct ipv6_addr *addr)
{
	struct rt *rt;
	struct in6_addr netmask;

	if (addr == NULL || addr->prefix_len > 128) {
		errno = EINVAL;
		return NULL;
	}

	/* There is no point in trying to manage a /128 prefix,
	 * ones without a lifetime.  */
	if (addr->prefix_len == 128 || addr->prefix_vltime == 0)
		return NULL;

	/* Don't install a reject route when not creating bigger prefixes. */
	if (addr->flags & IPV6_AF_NOREJECT)
		return NULL;

	/* This address is the delegated prefix, so add a reject route for
	 * it via the loopback interface. */
	if (addr->flags & IPV6_AF_DELEGATEDPFX) {
		struct interface *lo0;

		TAILQ_FOREACH(lo0, ifp->ctx->ifaces, next) {
			if (lo0->flags & IFF_LOOPBACK)
				break;
		}
		if (lo0 == NULL)
			logwarnx("cannot find a loopback interface "
			    "to reject via");
		else
			ifp = lo0;
	}

	if ((rt = inet6_makeroute(ifp, rap)) == NULL)
		return NULL;

	sa_in6_init(&rt->rt_dest, &addr->prefix);
	ipv6_mask(&netmask, addr->prefix_len);
	sa_in6_init(&rt->rt_netmask, &netmask);
	if (addr->flags & IPV6_AF_DELEGATEDPFX) {
		rt->rt_flags |= RTF_REJECT;
		/* Linux does not like a gateway for a reject route. */
#ifndef __linux__
		sa_in6_init(&rt->rt_gateway, &in6addr_loopback);
#endif
	} else if (!(addr->flags & IPV6_AF_ONLINK))
		sa_in6_init(&rt->rt_gateway, &rap->from);
	else
		rt->rt_gateway.sa_family = AF_UNSPEC;
	sa_in6_init(&rt->rt_ifa, &addr->addr);
	return rt;
}

static struct rt *
inet6_makerouter(struct ra *rap)
{
	struct rt *rt;

	if ((rt = inet6_makeroute(rap->iface, rap)) == NULL)
		return NULL;
	sa_in6_init(&rt->rt_dest, &in6addr_any);
	sa_in6_init(&rt->rt_netmask, &in6addr_any);
	sa_in6_init(&rt->rt_gateway, &rap->from);
	return rt;
}

#define RT_IS_DEFAULT(rtp) \
	(IN6_ARE_ADDR_EQUAL(&((rtp)->dest), &in6addr_any) &&		      \
	    IN6_ARE_ADDR_EQUAL(&((rtp)->mask), &in6addr_any))

static int
inet6_staticroutes(rb_tree_t *routes, struct dhcpcd_ctx *ctx)
{
	struct interface *ifp;
	struct ipv6_state *state;
	struct ipv6_addr *ia;
	struct rt *rt;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if ((state = IPV6_STATE(ifp)) == NULL)
			continue;
		TAILQ_FOREACH(ia, &state->addrs, next) {
			if ((ia->flags & (IPV6_AF_ADDED | IPV6_AF_STATIC)) ==
			    (IPV6_AF_ADDED | IPV6_AF_STATIC))
			{
				rt = inet6_makeprefix(ifp, NULL, ia);
				if (rt)
					rt_proto_add(routes, rt);
			}
		}
	}
	return 0;
}

static int
inet6_raroutes(rb_tree_t *routes, struct dhcpcd_ctx *ctx)
{
	struct rt *rt;
	struct ra *rap;
	const struct routeinfo *rinfo;
	const struct ipv6_addr *addr;
	struct in6_addr netmask;

	if (ctx->ra_routers == NULL)
		return 0;

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (rap->expired)
			continue;

		/* add rfc4191 route information routes */
		TAILQ_FOREACH (rinfo, &rap->rinfos, next) {
			if(rinfo->lifetime == 0)
				continue;
			if ((rt = inet6_makeroute(rap->iface, rap)) == NULL)
				continue;

			in6_addr_fromprefix(&netmask, rinfo->prefix_len);

			sa_in6_init(&rt->rt_dest, &rinfo->prefix);
			sa_in6_init(&rt->rt_netmask, &netmask);
			sa_in6_init(&rt->rt_gateway, &rap->from);
#ifdef HAVE_ROUTE_PREF
			rt->rt_pref = ipv6nd_rtpref(rinfo->flags);
#endif

			rt_proto_add(routes, rt);
		}

		/* add subnet routes */
		TAILQ_FOREACH(addr, &rap->addrs, next) {
			if (addr->prefix_vltime == 0)
				continue;
			rt = inet6_makeprefix(rap->iface, rap, addr);
			if (rt) {
				rt->rt_dflags |= RTDF_RA;
#ifdef HAVE_ROUTE_PREF
				rt->rt_pref = ipv6nd_rtpref(rap->flags);
#endif
				rt_proto_add(routes, rt);
			}
		}

		/* add default route */
		if (rap->lifetime == 0)
			continue;
		if (ipv6_anyglobal(rap->iface) == NULL)
			continue;
		rt = inet6_makerouter(rap);
		if (rt == NULL)
			continue;
		rt->rt_dflags |= RTDF_RA;
#ifdef HAVE_ROUTE_PREF
		rt->rt_pref = ipv6nd_rtpref(rap->flags);
#endif
		rt_proto_add(routes, rt);
	}
	return 0;
}

#ifdef DHCP6
static int
inet6_dhcproutes(rb_tree_t *routes, struct dhcpcd_ctx *ctx,
    enum DH6S dstate)
{
	struct interface *ifp;
	const struct dhcp6_state *d6_state;
	const struct ipv6_addr *addr;
	struct rt *rt;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		d6_state = D6_CSTATE(ifp);
		if (d6_state && d6_state->state == dstate) {
			TAILQ_FOREACH(addr, &d6_state->addrs, next) {
				rt = inet6_makeprefix(ifp, NULL, addr);
				if (rt == NULL)
					continue;
				rt->rt_dflags |= RTDF_DHCP;
				rt_proto_add(routes, rt);
			}
		}
	}
	return 0;
}
#endif

bool
inet6_getroutes(struct dhcpcd_ctx *ctx, rb_tree_t *routes)
{

	/* Should static take priority? */
	if (inet6_staticroutes(routes, ctx) == -1)
		return false;

	/* First add reachable routers and their prefixes */
	if (inet6_raroutes(routes, ctx) == -1)
		return false;

#ifdef DHCP6
	/* We have no way of knowing if prefixes added by DHCP are reachable
	 * or not, so we have to assume they are.
	 * Add bound before delegated so we can prefer interfaces better. */
	if (inet6_dhcproutes(routes, ctx, DH6S_BOUND) == -1)
		return false;
	if (inet6_dhcproutes(routes, ctx, DH6S_DELEGATED) == -1)
		return false;
#endif

	return true;
}
