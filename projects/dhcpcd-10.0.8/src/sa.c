/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Socket Address handling for dhcpcd
 * Copyright (c) 2015-2023 Roy Marples <roy@marples.name>
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
#ifdef AF_LINK
#include <net/if_dl.h>
#elif defined(AF_PACKET)
#include <linux/if_packet.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "sa.h"

#ifndef NDEBUG
static bool sa_inprefix;
#endif

socklen_t
sa_addroffset(const struct sockaddr *sa)
{

	assert(sa != NULL);
	switch(sa->sa_family) {
#ifdef INET
	case AF_INET:
		return offsetof(struct sockaddr_in, sin_addr) +
		       offsetof(struct in_addr, s_addr);
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		return offsetof(struct sockaddr_in6, sin6_addr) +
		       offsetof(struct in6_addr, s6_addr);
#endif /* INET6 */
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

socklen_t
sa_addrlen(const struct sockaddr *sa)
{
#define membersize(type, member) sizeof(((type *)0)->member)
	assert(sa != NULL);
	switch(sa->sa_family) {
#ifdef INET
	case AF_INET:
		return membersize(struct in_addr, s_addr);
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		return membersize(struct in6_addr, s6_addr);
#endif /* INET6 */
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

#ifndef HAVE_SA_LEN
socklen_t
sa_len(const struct sockaddr *sa)
{

	switch (sa->sa_family) {
#ifdef AF_LINK
	case AF_LINK:
		return sizeof(struct sockaddr_dl);
#endif
#ifdef AF_PACKET
	case AF_PACKET:
		return sizeof(struct sockaddr_ll);
#endif
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	default:
		return sizeof(struct sockaddr);
	}
}
#endif

bool
sa_is_unspecified(const struct sockaddr *sa)
{

	assert(sa != NULL);
	switch(sa->sa_family) {
	case AF_UNSPEC:
		return true;
#ifdef INET
	case AF_INET:
		return satocsin(sa)->sin_addr.s_addr == INADDR_ANY;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		return IN6_IS_ADDR_UNSPECIFIED(&satocsin6(sa)->sin6_addr);
#endif /* INET6 */
	default:
		errno = EAFNOSUPPORT;
		return false;
	}
}

#ifdef INET6
#ifndef IN6MASK128
#define IN6MASK128 {{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
		       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }}}
#endif
static const struct in6_addr in6allones = IN6MASK128;
#endif

bool
sa_is_allones(const struct sockaddr *sa)
{

	assert(sa != NULL);
	switch(sa->sa_family) {
	case AF_UNSPEC:
		return false;
#ifdef INET
	case AF_INET:
	{
		const struct sockaddr_in *sin;

		sin = satocsin(sa);
		return sin->sin_addr.s_addr == INADDR_BROADCAST;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6:
	{
		const struct sockaddr_in6 *sin6;

		sin6 = satocsin6(sa);
		return IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &in6allones);
	}
#endif /* INET6 */
	default:
		errno = EAFNOSUPPORT;
		return false;
	}
}

bool
sa_is_loopback(const struct sockaddr *sa)
{

	assert(sa != NULL);
	switch(sa->sa_family) {
	case AF_UNSPEC:
		return false;
#ifdef INET
	case AF_INET:
	{
		const struct sockaddr_in *sin;

		sin = satocsin(sa);
		return sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK);
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6:
	{
		const struct sockaddr_in6 *sin6;

		sin6 = satocsin6(sa);
		return IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr);
	}
#endif /* INET6 */
	default:
		errno = EAFNOSUPPORT;
		return false;
	}
}

int
sa_toprefix(const struct sockaddr *sa)
{
	int prefix;

	assert(sa != NULL);
	switch(sa->sa_family) {
#ifdef INET
	case AF_INET:
	{
		const struct sockaddr_in *sin;
		uint32_t mask;

		sin = satocsin(sa);
		if (sin->sin_addr.s_addr == INADDR_ANY) {
			prefix = 0;
			break;
		}
		mask = ntohl(sin->sin_addr.s_addr);
		prefix = 33 - ffs((int)mask);	/* 33 - (1 .. 32) -> 32 .. 1 */
		if (prefix < 32) {		/* more than 1 bit in mask */
			/* check for non-contig netmask */
			if ((mask^(((1U << prefix)-1) << (32 - prefix))) != 0) {
				errno = EINVAL;
				return -1;	/* noncontig, no pfxlen */
			}
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		const struct sockaddr_in6 *sin6;
		int x, y;
		const uint8_t *lim, *p;

		sin6 = satocsin6(sa);
		p = (const uint8_t *)sin6->sin6_addr.s6_addr;
		lim = p + sizeof(sin6->sin6_addr.s6_addr);
		for (x = 0; p < lim; x++, p++) {
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

		prefix = x * NBBY + y;
		break;
	}
#endif
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

#ifndef NDEBUG
	/* Ensure the calculation is correct */
	if (!sa_inprefix) {
		union sa_ss ss = { .sa = { .sa_family = sa->sa_family } };

		sa_inprefix = true;
		sa_fromprefix(&ss.sa, prefix);
		assert(sa_cmp(sa, &ss.sa) == 0);
		sa_inprefix = false;
	}
#endif

	return prefix;
}

static void
ipbytes_fromprefix(uint8_t *ap, int prefix, int max_prefix)
{
	int bytes, bits, i;

	bytes = prefix / NBBY;
	bits = prefix % NBBY;

	for (i = 0; i < bytes; i++)
		*ap++ = 0xff;
	if (bits) {
		uint8_t a;

		a = 0xff;
		a  = (uint8_t)(a << (8 - bits));
		*ap++ = a;
	}
	bytes = (max_prefix - prefix) / NBBY;
	for (i = 0; i < bytes; i++)
		*ap++ = 0x00;
}

void
in6_addr_fromprefix(struct in6_addr *addr, int prefix)
{
	ipbytes_fromprefix((uint8_t *)addr, prefix, 128);
}

int
sa_fromprefix(struct sockaddr *sa, int prefix)
{
	uint8_t *ap;
	int max_prefix;

	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		max_prefix = 32;
#ifdef HAVE_SA_LEN
		sa->sa_len = sizeof(struct sockaddr_in);
#endif
		break;
#endif
#ifdef INET6
	case AF_INET6:
		max_prefix = 128;
#ifdef HAVE_SA_LEN
		sa->sa_len = sizeof(struct sockaddr_in6);
#endif
		break;
#endif
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	ap = (uint8_t *)sa + sa_addroffset(sa);
	ipbytes_fromprefix(ap, prefix, max_prefix);

#ifndef NDEBUG
	/* Ensure the calculation is correct */
	if (!sa_inprefix) {
		sa_inprefix = true;
		assert(sa_toprefix(sa) == prefix);
		sa_inprefix = false;
	}
#endif
	return 0;
}

/* inet_ntop, but for sockaddr. */
const char *
sa_addrtop(const struct sockaddr *sa, char *buf, socklen_t len)
{
	const void *addr;

	assert(buf != NULL);
	assert(len > 0);

	if (sa->sa_family == 0) {
		*buf = '\0';
		return NULL;
	}

#ifdef AF_LINK
#ifndef CLLADDR
#define CLLADDR(sdl) (const void *)((sdl)->sdl_data + (sdl)->sdl_nlen)
#endif
	if (sa->sa_family == AF_LINK) {
		const struct sockaddr_dl *sdl;

		sdl = (const void *)sa;
		if (sdl->sdl_alen == 0) {
			if (snprintf(buf, len, "link#%d", sdl->sdl_index) == -1)
				return NULL;
			return buf;
		}
		return hwaddr_ntoa(CLLADDR(sdl), sdl->sdl_alen, buf, len);
	}
#elif defined(AF_PACKET)
	if (sa->sa_family == AF_PACKET) {
		const struct sockaddr_ll *sll;

		sll = (const void *)sa;
		return hwaddr_ntoa(sll->sll_addr, sll->sll_halen, buf, len);
	}
#endif
	addr = (const char *)sa + sa_addroffset(sa);
	return inet_ntop(sa->sa_family, addr, buf, len);
}

int
sa_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	socklen_t offset, len;

	assert(sa1 != NULL);
	assert(sa2 != NULL);

	/* Treat AF_UNSPEC as the unspecified address. */
	if ((sa1->sa_family == AF_UNSPEC || sa2->sa_family == AF_UNSPEC) &&
	    sa_is_unspecified(sa1) && sa_is_unspecified(sa2))
		return 0;

	if (sa1->sa_family != sa2->sa_family)
		return sa1->sa_family - sa2->sa_family;

#ifdef HAVE_SA_LEN
	len = MIN(sa1->sa_len, sa2->sa_len);
#endif

	switch (sa1->sa_family) {
#ifdef INET
	case AF_INET:
		offset = offsetof(struct sockaddr_in, sin_addr);
#ifdef HAVE_SA_LEN
		len -= offset;
		len = MIN(len, sizeof(struct in_addr));
#else
		len = sizeof(struct in_addr);
#endif
		break;
#endif
#ifdef INET6
	case AF_INET6:
		offset = offsetof(struct sockaddr_in6, sin6_addr);
#ifdef HAVE_SA_LEN
		len -= offset;
		len = MIN(len, sizeof(struct in6_addr));
#else
		len = sizeof(struct in6_addr);
#endif
		break;
#endif
	default:
		offset = 0;
#ifndef HAVE_SA_LEN
		len = sizeof(struct sockaddr);
#endif
		break;
	}

	return memcmp((const char *)sa1 + offset,
	    (const char *)sa2 + offset,
	    len);
}

#ifdef INET
void
sa_in_init(struct sockaddr *sa, const struct in_addr *addr)
{
	struct sockaddr_in *sin;

	assert(sa != NULL);
	assert(addr != NULL);
	sin = satosin(sa);
	sin->sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	sin->sin_len = sizeof(*sin);
#endif
	sin->sin_addr.s_addr = addr->s_addr;
}
#endif

#ifdef INET6
void
sa_in6_init(struct sockaddr *sa, const struct in6_addr *addr)
{
	struct sockaddr_in6 *sin6;

	assert(sa != NULL);
	assert(addr != NULL);
	sin6 = satosin6(sa);
	sin6->sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
	sin6->sin6_len = sizeof(*sin6);
#endif
	memcpy(&sin6->sin6_addr.s6_addr, &addr->s6_addr,
	    sizeof(sin6->sin6_addr.s6_addr));
}
#endif
