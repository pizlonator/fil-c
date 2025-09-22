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

#ifndef DHCPCOMMON_H
#define	DHCPCOMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdint.h>

#include <arpa/nameser.h> /* after normal includes for sunos */

#include "common.h"
#include "dhcpcd.h"

/* Support very old arpa/nameser.h as found in OpenBSD */
#ifndef NS_MAXDNAME
#define NS_MAXCDNAME MAXCDNAME
#define NS_MAXDNAME MAXDNAME
#define NS_MAXLABEL MAXLABEL
#endif

/* Max MTU - defines dhcp option length */
#define	IP_UDP_SIZE		  28
#define	MTU_MAX			1500 - IP_UDP_SIZE
#define	MTU_MIN			 576 + IP_UDP_SIZE

#define	OT_REQUEST		(1 << 0)
#define	OT_UINT8		(1 << 1)
#define	OT_INT8			(1 << 2)
#define	OT_UINT16		(1 << 3)
#define	OT_INT16		(1 << 4)
#define	OT_UINT32		(1 << 5)
#define	OT_INT32		(1 << 6)
#define	OT_ADDRIPV4		(1 << 7)
#define	OT_STRING		(1 << 8)
#define	OT_ARRAY		(1 << 9)
#define	OT_RFC3361		(1 << 10)
#define	OT_RFC1035		(1 << 11)
#define	OT_RFC3442		(1 << 12)
#define	OT_OPTIONAL		(1 << 13)
#define	OT_ADDRIPV6		(1 << 14)
#define	OT_BINHEX		(1 << 15)
#define	OT_FLAG			(1 << 16)
#define	OT_NOREQ		(1 << 17)
#define	OT_EMBED		(1 << 18)
#define	OT_ENCAP		(1 << 19)
#define	OT_INDEX		(1 << 20)
#define	OT_OPTION		(1 << 21)
#define	OT_DOMAIN		(1 << 22)
#define	OT_ASCII		(1 << 23)
#define	OT_RAW			(1 << 24)
#define	OT_ESCSTRING		(1 << 25)
#define	OT_ESCFILE		(1 << 26)
#define	OT_BITFLAG		(1 << 27)
#define	OT_RESERVED		(1 << 28)
#define	OT_URI			(1 << 29)

#define	DHC_REQ(r, n, o) \
	(has_option_mask((r), (o)) && !has_option_mask((n), (o)))

#define DHC_REQOPT(o, r, n)						  \
	(!((o)->type & OT_NOREQ) &&					  \
	 ((o)->type & OT_REQUEST || has_option_mask((r), (o)->option)) && \
	  !has_option_mask((n), (o)->option))

struct dhcp_opt {
	uint32_t option; /* Also used for IANA Enterpise Number */
	int type;
	size_t len;
	char *var;

	int index; /* Index counter for many instances of the same option */
	char bitflags[8];

	/* Embedded options.
	 * The option code is irrelevant here. */
	struct dhcp_opt *embopts;
	size_t embopts_len;

	/* Encapsulated options */
	struct dhcp_opt *encopts;
	size_t encopts_len;
};

const char *dhcp_get_hostname(char *, size_t, const struct if_options *);
struct dhcp_opt *vivso_find(uint32_t, const void *);

ssize_t dhcp_vendor(char *, size_t);

void dhcp_print_option_encoding(const struct dhcp_opt *opt, int cols);
#define	add_option_mask(var, val) \
	((var)[(val) >> 3] = (uint8_t)((var)[(val) >> 3] | 1 << ((val) & 7)))
#define	del_option_mask(var, val) \
	((var)[(val) >> 3] = (uint8_t)((var)[(val) >> 3] & ~(1 << ((val) & 7))))
#define	has_option_mask(var, val) \
	((var)[(val) >> 3] & (uint8_t)(1 << ((val) & 7)))
int make_option_mask(const struct dhcp_opt *, size_t,
    const struct dhcp_opt *, size_t,
    uint8_t *, const char *, int);

size_t encode_rfc1035(const char *src, uint8_t *dst);
ssize_t decode_rfc1035(char *, size_t, const uint8_t *, size_t);
ssize_t print_string(char *, size_t, int, const uint8_t *, size_t);
int dhcp_set_leasefile(char *, size_t, int, const struct interface *);

void dhcp_envoption(struct dhcpcd_ctx *,
    FILE *, const char *, const char *, struct dhcp_opt *,
    const uint8_t *(*dgetopt)(struct dhcpcd_ctx *,
    size_t *, unsigned int *, size_t *,
    const uint8_t *, size_t, struct dhcp_opt **),
    const uint8_t *od, size_t ol);
void dhcp_zero_index(struct dhcp_opt *);

ssize_t dhcp_readfile(struct dhcpcd_ctx *, const char *, void *, size_t);
ssize_t dhcp_writefile(struct dhcpcd_ctx *, const char *, mode_t,
    const void *, size_t);
int dhcp_filemtime(struct dhcpcd_ctx *, const char *, time_t *);
int dhcp_unlink(struct dhcpcd_ctx *, const char *);
size_t dhcp_read_hwaddr_aton(struct dhcpcd_ctx *, uint8_t **, const char *);
#endif
