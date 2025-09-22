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

#ifndef DHCP_H
#define DHCP_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <netinet/ip.h>
#define __FAVOR_BSD /* Nasty glibc hack so we can use BSD semantics for UDP */
#include <netinet/udp.h>
#undef __FAVOR_BSD

#include <limits.h>
#include <stdint.h>

#include "arp.h"
#include "bpf.h"
#include "auth.h"
#include "dhcp-common.h"

/* UDP port numbers for BOOTP */
#define BOOTPS			67
#define BOOTPC			68

#define MAGIC_COOKIE		0x63825363
#define BROADCAST_FLAG		0x8000

/* BOOTP message OP code */
#define BOOTREQUEST		1
#define BOOTREPLY		2

/* DHCP message type */
#define DHCP_DISCOVER		1
#define DHCP_OFFER		2
#define DHCP_REQUEST		3
#define DHCP_DECLINE		4
#define DHCP_ACK		5
#define DHCP_NAK		6
#define DHCP_RELEASE		7
#define DHCP_INFORM		8
#define DHCP_FORCERENEW		9

/* Constants taken from RFC 2131. */
#define T1			0.5
#define T2			0.875
#define DHCP_BASE		4
#define DHCP_MAX		64
#define DHCP_RAND_MIN		-1
#define DHCP_RAND_MAX		1

#ifdef RFC2131_STRICT
/* Be strictly conformant for section 4.1.1 */
#  define DHCP_MIN_DELAY	1
#  define DHCP_MAX_DELAY	10
#else
/* or mirror the more modern IPv6RS and DHCPv6 delays */
#  define DHCP_MIN_DELAY	0
#  define DHCP_MAX_DELAY	1
#endif

/* DHCP options */
enum DHO {
	DHO_PAD                    = 0,
	DHO_SUBNETMASK             = 1,
	DHO_ROUTER                 = 3,
	DHO_DNSSERVER              = 6,
	DHO_HOSTNAME               = 12,
	DHO_DNSDOMAIN              = 15,
	DHO_MTU                    = 26,
	DHO_BROADCAST              = 28,
	DHO_STATICROUTE            = 33,
	DHO_NISDOMAIN              = 40,
	DHO_NISSERVER              = 41,
	DHO_NTPSERVER              = 42,
	DHO_VENDOR                 = 43,
	DHO_IPADDRESS              = 50,
	DHO_LEASETIME              = 51,
	DHO_OPTSOVERLOADED         = 52,
	DHO_MESSAGETYPE            = 53,
	DHO_SERVERID               = 54,
	DHO_PARAMETERREQUESTLIST   = 55,
	DHO_MESSAGE                = 56,
	DHO_MAXMESSAGESIZE         = 57,
	DHO_RENEWALTIME            = 58,
	DHO_REBINDTIME             = 59,
	DHO_VENDORCLASSID          = 60,
	DHO_CLIENTID               = 61,
	DHO_USERCLASS              = 77,  /* RFC 3004 */
	DHO_RAPIDCOMMIT            = 80,  /* RFC 4039 */
	DHO_FQDN                   = 81,
	DHO_AUTHENTICATION         = 90,  /* RFC 3118 */
	DHO_IPV6_PREFERRED_ONLY    = 108, /* RFC 8925 */
	DHO_AUTOCONFIGURE          = 116, /* RFC 2563 */
	DHO_DNSSEARCH              = 119, /* RFC 3397 */
	DHO_CSR                    = 121, /* RFC 3442 */
	DHO_VIVCO                  = 124, /* RFC 3925 */
	DHO_VIVSO                  = 125, /* RFC 3925 */
	DHO_FORCERENEW_NONCE       = 145, /* RFC 6704 */
	DHO_MUDURL                 = 161, /* draft-ietf-opsawg-mud */
	DHO_SIXRD                  = 212, /* RFC 5969 */
	DHO_MSCSR                  = 249, /* MS code for RFC 3442 */
	DHO_END                    = 255
};

/* FQDN values - lsnybble used in flags
 * hsnybble to create order
 * and to allow 0x00 to mean disable
 */
enum FQDN {
	FQDN_DISABLE    = 0x00,
	FQDN_NONE       = 0x18,
	FQDN_PTR        = 0x20,
	FQDN_BOTH       = 0x31
};

#define	MIN_V6ONLY_WAIT		300 /* seconds, RFC 8925 */

/* Sizes for BOOTP options */
#define	BOOTP_CHADDR_LEN	 16
#define	BOOTP_SNAME_LEN		 64
#define	BOOTP_FILE_LEN		128
#define	BOOTP_VEND_LEN		 64

/* DHCP is basically an extension to BOOTP */
struct bootp {
	uint8_t op;		/* message type */
	uint8_t htype;		/* hardware address type */
	uint8_t hlen;		/* hardware address length */
	uint8_t hops;		/* should be zero in client message */
	uint32_t xid;		/* transaction id */
	uint16_t secs;		/* elapsed time in sec. from boot */
	uint16_t flags;		/* such as broadcast flag */
	uint32_t ciaddr;	/* (previously allocated) client IP */
	uint32_t yiaddr;	/* 'your' client IP address */
	uint32_t siaddr;	/* should be zero in client's messages */
	uint32_t giaddr;	/* should be zero in client's messages */
	uint8_t chaddr[BOOTP_CHADDR_LEN];	/* client's hardware address */
	uint8_t sname[BOOTP_SNAME_LEN];		/* server host name */
	uint8_t file[BOOTP_FILE_LEN];		/* boot file name */
	uint8_t vend[BOOTP_VEND_LEN];		/* vendor specific area */
	/* DHCP allows a variable length vendor area */
};

#define	DHCP_MIN_LEN		(offsetof(struct bootp, vend) + 4)

struct bootp_pkt
{
	struct ip ip;
	struct udphdr udp;
	struct bootp bootp;
};

struct dhcp_lease {
	struct in_addr addr;
	struct in_addr mask;
	struct in_addr brd;
	uint32_t leasetime;
	uint32_t renewaltime;
	uint32_t rebindtime;
	struct in_addr server;
	uint8_t frominfo;
	uint32_t cookie;
};

#ifndef DHCP_INFINITE_LIFETIME
#  define DHCP_INFINITE_LIFETIME	(~0U)
#endif

enum DHS {
	DHS_NONE,
	DHS_INIT,
	DHS_DISCOVER,
	DHS_REQUEST,
	DHS_PROBE,
	DHS_BOUND,
	DHS_RENEW,
	DHS_REBIND,
	DHS_REBOOT,
	DHS_INFORM,
	DHS_RENEW_REQUESTED,
	DHS_RELEASE
};

struct dhcp_state {
	enum DHS state;
	struct bootp *sent;
	size_t sent_len;
	struct bootp *offer;
	size_t offer_len;
	struct bootp *new;
	size_t new_len;
	struct bootp *old;
	size_t old_len;
	struct dhcp_lease lease;
	const char *reason;
	unsigned int interval;
	unsigned int nakoff;
	uint32_t xid;
	int socket;

	struct bpf *bpf;
	int udp_rfd;
	struct ipv4_addr *addr;
	uint8_t added;

	char leasefile[sizeof(LEASEFILE) + IF_NAMESIZE + (IF_SSIDLEN * 4)];
	struct timespec started;
	unsigned char *clientid;
	struct authstate auth;
#ifdef ARPING
	ssize_t arping_index;
#endif
};

#ifdef INET
#define D_STATE(ifp)							       \
	((struct dhcp_state *)(ifp)->if_data[IF_DATA_DHCP])
#define D_CSTATE(ifp)							       \
	((const struct dhcp_state *)(ifp)->if_data[IF_DATA_DHCP])
#define D_STATE_RUNNING(ifp)						       \
	(D_CSTATE((ifp)) && D_CSTATE((ifp))->new && D_CSTATE((ifp))->reason)

#define IS_DHCP(b)	((b)->vend[0] == 0x63 &&	\
			 (b)->vend[1] == 0x82 &&	\
			 (b)->vend[2] == 0x53 &&	\
			 (b)->vend[3] == 0x63)

#include "dhcpcd.h"
#include "if-options.h"

ssize_t print_rfc3361(FILE *, const uint8_t *, size_t);
ssize_t print_rfc3442(FILE *, const uint8_t *, size_t);

int dhcp_openudp(struct in_addr *);
void dhcp_packet(struct interface *, uint8_t *, size_t, unsigned int);
void dhcp_recvmsg(struct dhcpcd_ctx *, struct msghdr *);
void dhcp_printoptions(const struct dhcpcd_ctx *,
    const struct dhcp_opt *, size_t);
uint16_t dhcp_get_mtu(const struct interface *);
int dhcp_get_routes(rb_tree_t *, struct interface *);
ssize_t dhcp_env(FILE *, const char *, const struct interface *,
    const struct bootp *, size_t);

struct ipv4_addr *dhcp_handleifa(int, struct ipv4_addr *, pid_t pid);
void dhcp_drop(struct interface *, const char *);
void dhcp_start(struct interface *);
void dhcp_abort(struct interface *);
void dhcp_discover(void *);
void dhcp_inform(struct interface *);
void dhcp_renew(struct interface *);
void dhcp_bind(struct interface *);
void dhcp_reboot_newopts(struct interface *, unsigned long long);
void dhcp_close(struct interface *);
void dhcp_free(struct interface *);
int dhcp_dump(struct interface *);
#endif /* INET */

#endif /* DHCP_H */
