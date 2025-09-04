/* vi: set sw=4 ts=4: */
/*
 * Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000
 *      The Regents of the University of California.  All rights reserved.
 *
 * Busybox port by Vladimir Oleynik (C) 2005 <dzo@simtreas.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ''This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 *	traceroute6
 *
 *      Modified for NRL 4.4BSD IPv6 release.
 *      07/31/96 bgp
 *
 *	Modified for Linux IPv6 by Pedro Roque <roque@di.fc.ul.pt>
 *	31/07/1996
 *
 *	As ICMP error messages for IPv6 now include more than 8 bytes
 *	UDP datagrams are now sent via an UDP socket instead of magic
 *	RAW socket tricks.
 *
 *	Converted to busybox applet by Leonid Lisovskiy <lly@sf.net>
 *	2009-11-16
 */

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 30 hops & can be changed with the -m flag).  Three
 * probes (change with -q flag) are sent at each ttl setting and a
 * line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 30 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 30 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */
//config:config TRACEROUTE
//config:	bool "traceroute (11 kb)"
//config:	default y
//config:	help
//config:	Utility to trace the route of IP packets.
//config:
//config:config TRACEROUTE6
//config:	bool "traceroute6 (12 kb)"
//config:	default y
//config:	depends on FEATURE_IPV6
//config:	help
//config:	Utility to trace the route of IPv6 packets.
//config:
//config:config FEATURE_TRACEROUTE_VERBOSE
//config:	bool "Enable verbose output"
//config:	default y
//config:	depends on TRACEROUTE || TRACEROUTE6
//config:	help
//config:	Add some verbosity to traceroute. This includes among other things
//config:	hostnames and ICMP response types.
//config:
//config:config FEATURE_TRACEROUTE_USE_ICMP
//config:	bool "Enable -I option (use ICMP instead of UDP)"
//config:	default y
//config:	depends on TRACEROUTE || TRACEROUTE6

/* Needs socket(AF_INET, SOCK_RAW, IPPROTO_ICMP), therefore BB_SUID_MAYBE: */
//applet:IF_TRACEROUTE(APPLET(traceroute, BB_DIR_USR_BIN, BB_SUID_MAYBE))
//applet:IF_TRACEROUTE6(APPLET(traceroute6, BB_DIR_USR_BIN, BB_SUID_MAYBE))

//kbuild:lib-$(CONFIG_TRACEROUTE) += traceroute.o
//kbuild:lib-$(CONFIG_TRACEROUTE6) += traceroute.o

//usage:#define traceroute_trivial_usage
//usage:       "[-"IF_TRACEROUTE6("46")IF_FEATURE_TRACEROUTE_USE_ICMP("I")"Flnrv] [-f 1ST_TTL] [-m MAXTTL] [-q PROBES] [-p PORT]\n"
//usage:       "	[-t TOS] [-w WAIT_SEC] [-s SRC_IP] [-i IFACE]\n"
//usage:       "	[-z PAUSE_MSEC] HOST [BYTES]"
//usage:#define traceroute_full_usage "\n\n"
//usage:       "Trace the route to HOST\n"
//usage:	IF_TRACEROUTE6(
//usage:     "\n	-4,-6	Force IP or IPv6 name resolution"
//usage:	)
//usage:     "\n	-F	Set don't fragment bit"
//usage:	IF_FEATURE_TRACEROUTE_USE_ICMP(
//usage:     "\n	-I	Use ICMP ECHO instead of UDP datagrams"
//usage:	)
//usage:     "\n	-l	Display TTL value of the returned packet"
//Currently disabled (TRACEROUTE_SO_DEBUG==0)
////usage:     "\n	-d	Set SO_DEBUG options to socket"
//usage:     "\n	-n	Print numeric addresses"
//usage:     "\n	-r	Bypass routing tables, send directly to HOST"
//usage:	IF_FEATURE_TRACEROUTE_VERBOSE(
//usage:     "\n	-v	Verbose"
//usage:	)
//usage:     "\n	-f N	First number of hops (default 1)"
//usage:     "\n	-m N	Max number of hops"
//usage:     "\n	-q N	Number of probes per hop (default 3)"
//usage:     "\n	-p N	Base UDP port number used in probes"
//usage:     "\n		(default 33434)"
//usage:     "\n	-s IP	Source address"
//usage:     "\n	-i IFACE Source interface"
//usage:     "\n	-t N	Type-of-service in probe packets (default 0)"
//usage:     "\n	-w SEC	Wait for a response (default 3)"
//usage:     "\n	-z MSEC	Wait before each send"
//usage:
//usage:#define traceroute6_trivial_usage
//usage:       "[-"IF_FEATURE_TRACEROUTE_USE_ICMP("I")"nrv] [-f 1ST_TTL] [-m MAXTTL] [-q PROBES] [-p PORT]\n"
//usage:       "	[-t TOS] [-w WAIT_SEC] [-s SRC_IP] [-i IFACE]\n"
//usage:       "	[-z PAUSE_MSEC] HOST [BYTES]"
//usage:#define traceroute6_full_usage "\n\n"
//usage:       "Trace the route to HOST\n"
////NOP?     "\n	-F	Set don't fragment bit"
//usage:	IF_FEATURE_TRACEROUTE_USE_ICMP(
//usage:     "\n	-I	Use ICMP ECHO instead of UDP datagrams"
//usage:	)
////NOP:     "\n	-l	Display TTL value of the returned packet"
//Currently disabled (TRACEROUTE_SO_DEBUG==0)
////usage:     "\n	-d	Set SO_DEBUG options to socket"
//usage:     "\n	-n	Print numeric addresses"
//usage:     "\n	-r	Bypass routing tables, send directly to HOST"
//usage:	IF_FEATURE_TRACEROUTE_VERBOSE(
//usage:     "\n	-v	Verbose"
//usage:	)
//usage:     "\n	-f N	First number of hops (default 1)"
//usage:     "\n	-m N	Max number of hops"
//usage:     "\n	-q N	Number of probes per hop (default 3)"
//usage:     "\n	-p N	Base UDP port number used in probes"
//usage:     "\n		(default 33434)"
//usage:     "\n	-s IP	Source address"
//usage:     "\n	-i IFACE Source interface"
//usage:     "\n	-t N	Type-of-service in probe packets (default 0)"
//usage:     "\n	-w SEC	Wait for a response (default 3)"
//usage:     "\n	-z MSEC	Wait before each send"

#define TRACEROUTE_SO_DEBUG 0

#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#if ENABLE_FEATURE_IPV6
# include <netinet/ip6.h>
# include <netinet/icmp6.h>
# ifndef SOL_IPV6
#  define SOL_IPV6 IPPROTO_IPV6
# endif
# if defined(IPV6_PKTINFO) && !defined(IPV6_RECVPKTINFO)
#  define IPV6_RECVPKTINFO IPV6_PKTINFO
# endif
#endif

#include "libbb.h"
#include "inet_common.h"

#ifndef IPPROTO_ICMP
# define IPPROTO_ICMP 1
#endif
#ifndef IPPROTO_IP
# define IPPROTO_IP 0
#endif
/* Some operating systems, like GNU/Hurd, don't define SOL_RAW, but do have
 * IPPROTO_RAW. Since the IPPROTO definitions are also valid to use for
 * setsockopt (and take the same value as their corresponding SOL definitions,
 * if they exist), we can just fall back on IPPROTO_RAW. */
#ifndef SOL_RAW
# define SOL_RAW IPPROTO_RAW
#endif


#define OPT_STRING \
	"FIlnrdvt:i:m:p:q:s:w:z:f:" \
	"4" IF_TRACEROUTE6("6")
enum {
	OPT_DONT_FRAGMNT = (1 << 0),    /* F */
	OPT_USE_ICMP     = (1 << 1) * ENABLE_FEATURE_TRACEROUTE_USE_ICMP, /* I */
	OPT_TTL_FLAG     = (1 << 2),    /* l */
	OPT_ADDR_NUM     = (1 << 3),    /* n */
	OPT_BYPASS_ROUTE = (1 << 4),    /* r */
	OPT_DEBUG        = (1 << 5),    /* d */
	OPT_VERBOSE      = (1 << 6) * ENABLE_FEATURE_TRACEROUTE_VERBOSE, /* v */
	OPT_TOS          = (1 << 7),    /* t */
	OPT_DEVICE       = (1 << 8),    /* i */
	OPT_MAX_TTL      = (1 << 9),    /* m */
	OPT_PORT         = (1 << 10),   /* p */
	OPT_NPROBES      = (1 << 11),   /* q */
	OPT_SOURCE       = (1 << 12),   /* s */
	OPT_WAITTIME     = (1 << 13),   /* w */
	OPT_PAUSE_MS     = (1 << 14),   /* z */
	OPT_FIRST_TTL    = (1 << 15),   /* f */
	OPT_IPV4         = (1 << 16),   /* 4 */
	OPT_IPV6         = (1 << 17) * ENABLE_TRACEROUTE6, /* 6 */
};
#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
# define verbose (option_mask32 & OPT_VERBOSE)
#else
# define verbose 0
#endif

enum {
	SIZEOF_ICMP_HDR = 8,
	rcvsock = 3, /* receive (icmp) socket file descriptor */
	sndsock = 4, /* send (udp/icmp) socket file descriptor */
};

/* Data section of the probe packet */
struct outdata_t {
	unsigned char seq;             /* sequence number of this packet */
	unsigned char ttl;             /* ttl packet left with */
// UNUSED. Retaining to have the same packet size.
	struct timeval tv_UNUSED PACKED; /* time packet left */
};

#if ENABLE_TRACEROUTE6
struct outdata6_t {
	uint32_t ident6;
	uint32_t seq6;
	struct timeval tv_UNUSED PACKED; /* time packet left */
};
#endif

struct globals {
	/* Pointer to entire malloced IP packet, "packlen" bytes long: */
	struct ip *outip;
	/* Pointer to ICMP or UDP payload (not header): */
	struct outdata_t *outdata;
	len_and_sockaddr *dest_lsa;
	len_and_sockaddr *from_lsa;	/* response came from this address */
#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
	struct sockaddr *to; 		/* response came to this (local) address */
#endif
	uint32_t ident;
	uint16_t port;                  /* start udp dest port # for probe packets */
#if ENABLE_TRACEROUTE6
	smallint ipv6;
# define G_ipv6 G.ipv6
#else
# define G_ipv6 0
#endif
	int packlen;                    /* total length of packet */
	int pmtu;                       /* Path MTU Discovery (RFC1191) */
	int waittime;                   /* time to wait for response (in seconds) */
	int first_ttl;
	int nprobes;
	int max_ttl;
	unsigned pausemsecs;
	unsigned char recv_pkt[512];    /* last inbound (icmp) packet */
};

#define G (*ptr_to_globals)
#define outip     (G.outip    )
#define outdata   (G.outdata  )
#define dest_lsa  (G.dest_lsa )
#define packlen   (G.packlen  )
#define pmtu      (G.pmtu     )
#define ident     (G.ident    )
#define port      (G.port     )
#define waittime  (G.waittime )
#define recv_pkt  (G.recv_pkt )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

#define outudp   ((struct udphdr *)(outip + 1))
#define outudp6  ((struct udphdr *)(((struct ip6_hdr*)outip) + 1))
#define outicmp  ((struct icmp *)(outip + 1))
#define outicmp6 ((struct icmp *)(((struct ip6_hdr*)outip) + 1))
/* NB: for icmp echo, IPv4 and IPv6 fields are the same size and offset:
 * struct icmp:
 *	uint8_t  icmp_type;
 *	uint8_t  icmp_code;
 *	uint16_t icmp_cksum;
 *	uint16_t icmp_id;
 *	uint16_t icmp_seq;
 * struct icmp6_hdr:
 *	uint8_t  icmp6_type;
 *	uint8_t  icmp6_code;
 *	uint16_t icmp6_cksum;
 *	uint16_t icmp6_id;
 *	uint16_t icmp6_seq;
 * therefore both outicmp and outicmp6 are pointers to *IPv4* icmp struct.
 * SIZEOF_ICMP_HDR == 8 is the same for both, as well.
 * However, values of these pointers are not the same (since IPv6 IP header is larger),
 * and icmp_type constants are not the same:
 * #define ICMP_ECHO               8
 * #define ICMP_ECHOREPLY          0
 * #define ICMP6_ECHO_REQUEST    128
 * #define ICMP6_ECHO_REPLY      129
 */

static int
wait_for_reply(unsigned *timestamp_us, int *left_ms)
{
	struct pollfd pfd[1];
	int read_len = 0;

	pfd[0].fd = rcvsock;
	pfd[0].events = POLLIN;
	if (*left_ms >= 0 && safe_poll(pfd, 1, *left_ms) > 0) {
		unsigned t;

#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
		read_len = recv_from_to(rcvsock,
				recv_pkt, sizeof(recv_pkt),
				/*flags:*/ MSG_DONTWAIT,
				&G.from_lsa->u.sa, G.to, G.from_lsa->len);
#else
		read_len = recvfrom(rcvsock,
				recv_pkt, sizeof(recv_pkt),
				/*flags:*/ MSG_DONTWAIT,
				&G.from_lsa->u.sa, &G.from_lsa->len);
#endif
		if (read_len < 0)
			bb_simple_perror_msg_and_die("recv");
		t = monotonic_us();
		*left_ms -= (t - *timestamp_us) / 1000;
		*timestamp_us = t;
	}

	return read_len;
}

static void
send_probe(int seq, int ttl)
{
	int len, res;
	void *out;
	struct icmp *icp;

	/* Payload */
#if ENABLE_TRACEROUTE6
	if (G_ipv6) {
		struct outdata6_t *pkt = (void *) outdata;
		pkt->ident6 = ident;
		pkt->seq6   = htonl(seq);
		/*xgettimeofday(&pkt->tv);*/
		icp = outicmp6;
	} else
#endif
	{
		outdata->seq = seq;
		outdata->ttl = ttl;
// UNUSED: was storing gettimeofday's result there, but never ever checked it
		/*memcpy(&outdata->tv, tp, sizeof(outdata->tv));*/
		icp = outicmp;
	}
	out = outdata;
	if (option_mask32 & OPT_USE_ICMP) {
		out = icp;
		/*icp->icmp_type = ICMP[6]_ECHO; - already set */
		/*icp->icmp_code = 0; - already set */
		/*icp->icmp_id = ident; - already set */
		icp->icmp_seq = htons(seq);
		/* Always calculate checksum for icmp packets */
		icp->icmp_cksum = 0;
		icp->icmp_cksum = inet_cksum(
				icp,
				((char*)outip + packlen) - (char*)icp
		);
		if (icp->icmp_cksum == 0)
			icp->icmp_cksum = 0xffff;
	}

//BUG! verbose is (x & OPT_VERBOSE), not a counter!
#if 0 //ENABLE_FEATURE_TRACEROUTE_VERBOSE
	/* XXX undocumented debugging hack */
	if (verbose > 1) {
		const uint16_t *sp;
		int nshorts, i;

		sp = (uint16_t *)outip;
		nshorts = (unsigned)packlen / sizeof(uint16_t);
		i = 0;
		printf("[ %d bytes", packlen);
		while (--nshorts >= 0) {
			if ((i++ % 8) == 0)
				printf("\n\t");
			printf(" %04x", ntohs(*sp));
			sp++;
		}
		if (packlen & 1) {
			if ((i % 8) == 0)
				printf("\n\t");
			printf(" %02x", *(unsigned char *)sp);
		}
		printf("]\n");
	}
#endif

#if ENABLE_TRACEROUTE6
	if (G_ipv6) {
		res = setsockopt_int(sndsock, SOL_IPV6, IPV6_UNICAST_HOPS, ttl);
		if (res != 0)
			bb_perror_msg_and_die("setsockopt(%s) %d", "UNICAST_HOPS", ttl);
	} else
#endif
	{
#if defined IP_TTL
		res = setsockopt_int(sndsock, IPPROTO_IP, IP_TTL, ttl);
		if (res != 0)
			bb_perror_msg_and_die("setsockopt(%s) %d", "TTL", ttl);
#endif
	}

	if (!(option_mask32 & OPT_USE_ICMP)) {
		set_nport(&dest_lsa->u.sa, htons(port + seq));
	}
	len = ((char*)outip + packlen) - (char*)out;
	res = xsendto(sndsock, out, len, &dest_lsa->u.sa, dest_lsa->len);
	if (res != len)
		bb_error_msg("sent %d octets, ret=%d", len, res);
}

#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
/* Convert an ICMP "type" field to a printable string */
static const char *
pr_type(unsigned char t)
{
	static const char *const ttab[] ALIGN_PTR = {
	"Echo Reply",   "ICMP 1",       "ICMP 2",       "Dest Unreachable",
	"Source Quench", "Redirect",    "ICMP 6",       "ICMP 7",
	"Echo",         "Router Advert", "Router Solicit", "Time Exceeded",
	"Param Problem", "Timestamp",   "Timestamp Reply", "Info Request",
	"Info Reply",   "Mask Request", "Mask Reply"
	};
# if ENABLE_TRACEROUTE6
	static const char *const ttab6[] ALIGN_PTR = {
[0]	= "Error", "Dest Unreachable", "Packet Too Big", "Time Exceeded",
[4]	= "Param Problem",
[8]	= "Echo Request", "Echo Reply", "Membership Query", "Membership Report",
[12]	= "Membership Reduction", "Router Solicit", "Router Advert", "Neighbor Solicit",
[16]	= "Neighbor Advert", "Redirect",
	};

	if (G_ipv6) {
		if (t < 5)
			return ttab6[t];
		if (t < 128 || t > ND_REDIRECT)
			return "OUT-OF-RANGE";
		return ttab6[(t & 63) + 8];
	}
# endif
	if (t >= ARRAY_SIZE(ttab))
		return "OUT-OF-RANGE";

	return ttab[t];
}
static int
hexdump_if_verbose(const struct icmp *icp, int len)
{
	const unsigned char *p;
	int i;

	if (!verbose)
		return 0;

	printf("\n%d bytes from %s to %s: icmp type %u (%s) code %u\n",
		len,
		auto_string(xmalloc_sockaddr2dotted_noport(&G.from_lsa->u.sa)),
		auto_string(xmalloc_sockaddr2dotted_noport(G.to)),
		icp->icmp_type, pr_type(icp->icmp_type),
		icp->icmp_code
	);
	p = (const void *)icp;
	for (i = 0; i < len; i++) {
		if (!(i & 0xf))
			printf("\n%04x:" + (i==0), i);
		printf(" %02x", p[i]);
	}
	bb_putchar('\n');
	return 0;
}
#else
# define hexdump_if_verbose(...) 0
#endif

static int
packet4_ok(int read_len, int seq)
{
	const struct icmp *icp;
	unsigned char type, code;
	int hlen;
	const struct ip *ip;

	/* NB: reads from (AF_INET, SOCK_RAW, IPPROTO_ICMP) socket
	 * return the entire IP packet (IOW: they do not strip IP header).
	 * This differs from (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6) sockets!?
	 */
	ip = (struct ip *) recv_pkt;

	hlen = ip->ip_hl << 2;
	if (read_len < hlen + ICMP_MINLEN) {
		if (verbose)
			printf("packet too short (%d bytes) from %s\n", read_len,
				inet_ntoa(G.from_lsa->u.sin.sin_addr));
		return 0;
	}
	read_len -= hlen;
	icp = (struct icmp *)(recv_pkt + hlen);
	type = icp->icmp_type;
	code = icp->icmp_code;
	/* Path MTU Discovery (RFC1191) */
	pmtu = 0;
	if (code == ICMP_UNREACH_NEEDFRAG)
		pmtu = ntohs(icp->icmp_nextmtu);

	if ((option_mask32 & OPT_USE_ICMP)
	 && type == ICMP_ECHOREPLY
	 && icp->icmp_seq == htons(seq)
	) {
		if (icp->icmp_id != ident)
			/* reply to another ping/traceroute from this box? */
			return 0; /* ignore, silently */
		/* In UDP mode, when we reach the machine, we (usually)
		 * would get "port unreachable" - in ICMP we got "echo reply".
		 * Simulate "port unreachable" for caller:
		 */
		return ICMP_UNREACH_PORT+1;
	}

	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS)
	 || type == ICMP_UNREACH
	) {
		const struct ip *hip;
		const struct udphdr *up;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;
		if (option_mask32 & OPT_USE_ICMP) {
			struct icmp *hicmp;

			hicmp = (struct icmp *)((unsigned char *)hip + hlen);
			if (hlen + SIZEOF_ICMP_HDR <= read_len
			 && hip->ip_p == IPPROTO_ICMP
			 && hicmp->icmp_id == ident
			 && hicmp->icmp_seq == htons(seq)
			) {
				return (type == ICMP_TIMXCEED ? -1 : code + 1);
			}
		} else {
			up = (struct udphdr *)((char *)hip + hlen);
			if (hlen + 12 <= read_len
			 && hip->ip_p == IPPROTO_UDP
#if !defined(__FreeBSD__)
// Disabled source check: since we do not form the entire IP packet,
// but defer it to kernel, we can't set source port,
// and thus can't check it here in the reply
			/* && up->source == ident */
			 && up->dest == htons(port + seq)
#else
			/* && up->uh_sport == ident */
			 && up->uh_dport == htons(port + seq)
#endif
			) {
				return (type == ICMP_TIMXCEED ? -1 : code + 1);
			}
		}
	}
	/* testcase: traceroute -vI 127.0.0.1 (sees its own echo requests) */
	return hexdump_if_verbose(icp, read_len);
}

#if ENABLE_TRACEROUTE6

static int
packet6_ok(int read_len, int seq)
{
	const struct icmp6_hdr *icp;
	unsigned char type, code;

	/* NB: reads from (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6) socket
	 * return only ICMP packet (IOW: they strip IPv6 header).
	 * This differs from (AF_INET, SOCK_RAW, IPPROTO_ICMP) sockets!?
	 */
	if (read_len < ICMP_MINLEN) {
		if (verbose)
			printf("packet too short (%d bytes) from %s\n", read_len,
				auto_string(xmalloc_sockaddr2dotted_noport(&G.from_lsa->u.sa)));
		return 0;
	}
	icp = (struct icmp6_hdr *) recv_pkt;

	type = icp->icmp6_type;
	code = icp->icmp6_code;

	if ((option_mask32 & OPT_USE_ICMP)
	 && type == ICMP6_ECHO_REPLY
	 && icp->icmp6_seq == htons(seq)
	) {
		if (icp->icmp6_id != ident)
			/* reply to another ping/traceroute from this box? */
			return 0; /* ignore, silently */
		/* In UDP mode, when we reach the machine, we (usually)
		 * would get "port unreachable" - in ICMP we got "echo reply".
		 * Simulate "port unreachable" for caller:
		 */
		return (ICMP6_DST_UNREACH_NOPORT << 8) + 1;
	}

	if ((type == ICMP6_TIME_EXCEEDED && code == ICMP6_TIME_EXCEED_TRANSIT)
	 || type == ICMP6_DST_UNREACH
	) {
		struct ip6_hdr *hip;
		struct udphdr *up;
		int nexthdr;

		hip = (struct ip6_hdr *)(icp + 1);
		up  = (struct udphdr *) (hip + 1);
		nexthdr = hip->ip6_nxt;

		if (nexthdr == IPPROTO_FRAGMENT) {
			nexthdr = *(unsigned char*)up;
			up++;
		}
		if (nexthdr == IPPROTO_UDP) {
			struct outdata6_t *pkt;

			pkt = (struct outdata6_t *) (up + 1);

			if (pkt->ident6 == ident
			 && ntohl(pkt->seq6) == seq
			) {
				return (type == ICMP6_TIME_EXCEEDED ? -1 : (code<<8)+1);
			}
		}
	}
	/* cast is safe since the beginning of icmp4 and icmp6 layouts match */
	return hexdump_if_verbose((const struct icmp *)icp, read_len);
}

static int
packet_ok(int read_len, int seq)
{
	if (!G_ipv6)
		return packet4_ok(read_len, seq);
	return packet6_ok(read_len, seq);
}

#else /* !ENABLE_TRACEROUTE6 */

# define packet_ok(read_len, seq) packet4_ok(read_len, seq)

#endif

static void
#if !ENABLE_FEATURE_TRACEROUTE_VERBOSE
print(void)
# define print(len) print()
#else
print(int read_len)
#endif
{
	char *ina = auto_string(xmalloc_sockaddr2dotted_noport(&G.from_lsa->u.sa));

	if (option_mask32 & OPT_ADDR_NUM) {
		printf("  %s", ina);
	} else {
		char *n = NULL;
		if (G_ipv6
		 || G.from_lsa->u.sin.sin_addr.s_addr != INADDR_ANY
		) {
			/* Reverse resolve if IPV6 or not 0.0.0.0 */
			n = auto_string(xmalloc_sockaddr2host_noport(&G.from_lsa->u.sa));
		}
		printf("  %s (%s)", (n ? n : ina), ina);
	}

#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
	if (verbose) {
# if ENABLE_TRACEROUTE6
		/* NB: reads from (AF_INET, SOCK_RAW, IPPROTO_ICMP) socket
		 * return the entire IP packet (IOW: they do not strip IP header).
		 * Reads from (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6) do strip IPv6
		 * header and return only ICMP6 packet. Weird.
		 */
		if (G_ipv6) {
			/* read_len -= sizeof(struct ip6_hdr); - WRONG! */
		} else
# endif
		{
			struct ip *ip4packet = (struct ip*)recv_pkt;
			read_len -= ip4packet->ip_hl << 2;
		}
		printf(" %d bytes to %s", read_len,
			auto_string(xmalloc_sockaddr2dotted_noport(G.to))
		);
	}
#endif
}

static void
print_delta_ms(unsigned t1p, unsigned t2p)
{
	unsigned tt = t2p - t1p;
	printf("  %u.%03u ms", tt / 1000, tt % 1000);
}

/* Keeping init code in a separate (not inlined!) function
 * for stack use reduction and better register allocation in main loop.
 */
static NOINLINE void
traceroute_init(int op, char **argv)
{
	char *source;
	char *device;
	char *tos_str;
	char *max_ttl_str;
	char *port_str;
	char *nprobes_str;
	char *waittime_str;
	char *pausemsecs_str;
	char *first_ttl_str;
	char *dest_str;
#if ENABLE_TRACEROUTE6
	sa_family_t af;
#else
	enum { af = AF_INET };
#endif

	/* Ensure the socket fds won't be 0, 1 or 2 */
	bb_sanitize_stdio();

	INIT_G();
	port = 33434;
	waittime = 5;
	G.first_ttl = 1;
	G.nprobes = 3;
	G.max_ttl = 30;

	op |= getopt32(argv, "^"
		OPT_STRING
		"\0" "-1" /* minimum 1 arg */
		, &tos_str, &device, &max_ttl_str, &port_str, &nprobes_str
		, &source, &waittime_str, &pausemsecs_str, &first_ttl_str
	);
	argv += optind;

	if (op & OPT_MAX_TTL)
		G.max_ttl = xatou_range(max_ttl_str, 1, 255);
	if (op & OPT_PORT)
		port = xatou16(port_str);
	if (op & OPT_NPROBES)
		G.nprobes = xatou_range(nprobes_str, 1, INT_MAX);
	if (op & OPT_WAITTIME)
		waittime = xatou_range(waittime_str, 1, 24 * 60 * 60);
	if (op & OPT_PAUSE_MS)
		G.pausemsecs = xatou_range(pausemsecs_str, 0, 60 * 60 * 1000);
	if (op & OPT_FIRST_TTL)
		G.first_ttl = xatou_range(first_ttl_str, 1, G.max_ttl);

	/* Process destination and optional packet size */
	packlen = sizeof(struct ip)
			+ sizeof(struct udphdr)
			+ sizeof(struct outdata_t);
	if (op & OPT_USE_ICMP) {
		packlen = sizeof(struct ip)
			+ SIZEOF_ICMP_HDR
			+ sizeof(struct outdata_t);
		port = 0; /* on ICMP6 sockets, sendto(ipv6.nonzero_port) throws EINVAL! */
	}
#if ENABLE_TRACEROUTE6
	af = AF_UNSPEC;
	if (op & OPT_IPV4)
		af = AF_INET;
	if (op & OPT_IPV6)
		af = AF_INET6;
	dest_lsa = xhost_and_af2sockaddr(argv[0], port, af);
	af = dest_lsa->u.sa.sa_family;
//TODO: make sure af == AF_INET[6]? (FEATURE_UNIX_LOCAL=y allows "local:/PATH" to be translated to AF_UNIX)
	if (af == AF_INET6) {
		G_ipv6 = 1;
		packlen = sizeof(struct ip6_hdr)
				+ sizeof(struct udphdr)
				+ sizeof(struct outdata6_t);
		if (op & OPT_USE_ICMP)
			packlen = sizeof(struct ip6_hdr)
				+ SIZEOF_ICMP_HDR
				+ sizeof(struct outdata6_t);
	}
#else
	/* accept only IPv4 addresses */
	dest_lsa = xhost_and_af2sockaddr(argv[0], port, AF_INET);
#endif
	G.from_lsa = xmemdup(dest_lsa, LSA_LEN_SIZE + dest_lsa->len);
#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
	G.to = xzalloc(dest_lsa->len);
#endif
	if (argv[1])
		packlen = xatoul_range(argv[1], packlen, 32 * 1024);

	if (af == AF_INET) {
		xmove_fd(xsocket(AF_INET, SOCK_RAW, IPPROTO_ICMP), rcvsock);
#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
		/* want recvmsg to report target local address (for -v) */
		setsockopt_1(rcvsock, IPPROTO_IP, IP_PKTINFO);
#endif
	}
#if ENABLE_TRACEROUTE6
	else {
		xmove_fd(xsocket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6), rcvsock);
# if ENABLE_FEATURE_TRACEROUTE_VERBOSE
		/* want recvmsg to report target local address (for -v) */
		setsockopt_1(rcvsock, SOL_IPV6, IPV6_RECVPKTINFO);
# endif
	}
#endif
#if TRACEROUTE_SO_DEBUG
	if (op & OPT_DEBUG)
		setsockopt_SOL_SOCKET_1(rcvsock, SO_DEBUG);
#endif

	{
		int snd;
		if (af == AF_INET) {
			if (op & OPT_USE_ICMP)
				snd = xsocket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
			else
				snd = xsocket(AF_INET, SOCK_DGRAM, 0);
		}
#if ENABLE_TRACEROUTE6
# if defined(__FreeBSD__)
#  define SOL_V6_OPTION SOL_IPV6
# else
#  define SOL_V6_OPTION SOL_RAW
# endif
		else {
			if (setsockopt_int(rcvsock, SOL_V6_OPTION, IPV6_CHECKSUM, 2) != 0)
				bb_perror_msg_and_die("setsockopt(%s)", "IPV6_CHECKSUM");
			if (op & OPT_USE_ICMP)
				snd = xsocket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
			else
				snd = xsocket(AF_INET6, SOCK_DGRAM, 0);
		}
#endif
		xmove_fd(snd, sndsock);
	}

#ifdef SO_SNDBUF
	if (setsockopt_SOL_SOCKET_int(sndsock, SO_SNDBUF, packlen) != 0) {
		bb_perror_msg_and_die("setsockopt(%s)", "SO_SNDBUF");
	}
#endif
#ifdef IP_TOS
	if (op & OPT_TOS) {
		int tos = xatou_range(tos_str, 0, 255);
		if (setsockopt_int(sndsock, IPPROTO_IP, IP_TOS, tos) != 0)
			bb_perror_msg_and_die("setsockopt(%s,%d)", "TOS", tos);
	}
#endif
#ifdef IP_DONTFRAG
	if (op & OPT_DONT_FRAGMNT)
		setsockopt_1(sndsock, IPPROTO_IP, IP_DONTFRAG);
#endif
#if TRACEROUTE_SO_DEBUG
	if (op & OPT_DEBUG)
		setsockopt_SOL_SOCKET_1(sndsock, SO_DEBUG);
#endif
	if (op & OPT_BYPASS_ROUTE)
		setsockopt_SOL_SOCKET_1(sndsock, SO_DONTROUTE);

	outip = xzalloc(packlen);

	ident = getpid();
	/* we can use native-endian ident, but other Unix ping/traceroute
	 * utils use *big-endian pid*, and e.g. ping on our machine may be
	 * *not* from busybox, idents may collide. Follow the convention:
	 */
	ident = htons(ident);

	outdata = (void*)(outudp + 1);
	if (af == AF_INET) {
		if (op & OPT_USE_ICMP) {
			outicmp->icmp_type = ICMP_ECHO;
			/*outicmp->icmp_code = 0; - set by xzalloc */
			outicmp->icmp_id = ident;
			outdata = (void*)((char *)outicmp + SIZEOF_ICMP_HDR);
		}
	}
#if ENABLE_TRACEROUTE6
	else {
		outdata = (void*)(outudp6 + 1);
		if (op & OPT_USE_ICMP) {
			outicmp6->icmp_type = ICMP6_ECHO_REQUEST;
			/*outicmp->icmp_code = 0; - set by xzalloc */
			outicmp6->icmp_id = ident;
			outdata = (void*)((char *)outicmp6 + SIZEOF_ICMP_HDR);
		}
	}
#endif

	if (op & OPT_DEVICE) /* hmm, do we need error check? */
		setsockopt_bindtodevice(sndsock, device);

	if (op & OPT_SOURCE) {
#if ENABLE_TRACEROUTE6
// TODO: need xdotted_and_af2sockaddr?
		len_and_sockaddr *source_lsa = xhost_and_af2sockaddr(source, 0, af);
#else
		len_and_sockaddr *source_lsa = xdotted2sockaddr(source, 0);
#endif
		if (getuid() != 0)
			bb_simple_error_msg_and_die(bb_msg_you_must_be_root);
		/* Ping4 does this (why?) */
		if (af == AF_INET)
			if (setsockopt(sndsock, IPPROTO_IP, IP_MULTICAST_IF,
					&source_lsa->u.sa, source_lsa->len))
				bb_simple_error_msg_and_die("can't set multicast source interface");
//TODO: we can query source port we bound to,
// and check it in replies... if we care enough
		xbind(sndsock, &source_lsa->u.sa, source_lsa->len);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(source_lsa);
	} else {
		len_and_sockaddr *source_lsa;

		set_nport(&dest_lsa->u.sa, htons(port));
		/* Connect makes kernel pick source IP (and port if UDP) */
		xconnect(sndsock, &dest_lsa->u.sa, dest_lsa->len);
		/* Read IP and port */
		source_lsa = get_sock_lsa(sndsock);
		if (source_lsa == NULL)
			bb_simple_perror_msg_and_die("getsockname");
		/* bind our recv ICMP socket to this IP (but not port, ICMP has no ports) */
		//set_nport(&source_lsa->u.sa, 0); - paranoia, seems to work without this for both ipv4 and ipv6
		xbind(rcvsock, &source_lsa->u.sa, source_lsa->len);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(source_lsa);
	}

	/* Revert to non-privileged user after opening sockets */
	xsetgid(getgid());
	xsetuid(getuid());

	dest_str = xmalloc_sockaddr2dotted_noport(&dest_lsa->u.sa);
	printf("traceroute to %s (%s)", argv[0], dest_str);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(dest_str);

	if (op & OPT_SOURCE)
		printf(" from %s", source);
	printf(", %d hops max, %d byte packets\n", G.max_ttl, packlen);
}

static int
common_traceroute_main(int op, char **argv)
{
	int ttl;
	int seq;
	struct sockaddr *lastaddr;

	traceroute_init(op, argv);

	lastaddr = xzalloc(dest_lsa->len);
	seq = 0;
	for (ttl = G.first_ttl; ttl <= G.max_ttl; ++ttl) {
		int probe;
		int unreachable = 0; /* counter */
		int got_there = 0;

		printf("%2d", ttl);
		for (probe = 0; probe < G.nprobes; ++probe) {
			unsigned t1;
			unsigned t2;
			int left_ms;
			int read_len;
			int icmp_code;

			fflush_all();
			if (probe != 0)
				msleep(G.pausemsecs);

			send_probe(++seq, ttl);

			t2 = t1 = monotonic_us();
			left_ms = waittime * 1000;
			for (;;) {
				/* NB: wait_for_reply() fills "G.from_lsa" and "G.to" with
				 * "where it came from" and "what local address it arrived to"
				 * addresses. Sets t2 = monotonic_us(), updates left_ms.
				 */
				read_len = wait_for_reply(&t2, &left_ms);

				if (read_len == 0) { /* there was no packet at all? */
					printf("  *");
					goto next_probe;
				}
				icmp_code = packet_ok(read_len, seq);
				if (icmp_code != 0)
					break; /* got a good response */
				/* unrecognized type/code or too short, back to recv */
			}

			if (probe == 0
			 || (memcmp(lastaddr, &G.from_lsa->u.sa, G.from_lsa->len) != 0)
			) {
				print(read_len);
				memcpy(lastaddr, &G.from_lsa->u.sa, G.from_lsa->len);
			}
			print_delta_ms(t1, t2);
			if (!G_ipv6) {
				if (op & OPT_TTL_FLAG) {
					struct ip *ip = (struct ip *)recv_pkt;
					printf(" (%d)", ip->ip_ttl);
				}
			}

			/* Got a "time exceeded in transit" icmp message? */
			if (icmp_code == -1)
				continue;

			icmp_code--;
			switch (icmp_code) {
#if ENABLE_TRACEROUTE6
			case ICMP6_DST_UNREACH_NOPORT << 8:
				got_there = 1;
				break;
#endif
			case ICMP_UNREACH_PORT: {
				struct ip *ip = (struct ip *)recv_pkt;
				if (ip->ip_ttl <= 1)
					printf(" !");
				got_there = 1;
				break;
			}
			case ICMP_UNREACH_NET:
#if ENABLE_TRACEROUTE6 && (ICMP6_DST_UNREACH_NOROUTE != ICMP_UNREACH_NET)
			case ICMP6_DST_UNREACH_NOROUTE << 8:
#endif
				printf(" !N");
				++unreachable;
				break;
			case ICMP_UNREACH_HOST:
#if ENABLE_TRACEROUTE6
			case ICMP6_DST_UNREACH_ADDR << 8:
#endif
				printf(" !H");
				++unreachable;
				break;
			case ICMP_UNREACH_PROTOCOL:
				printf(" !P");
				got_there = 1;
				break;
			case ICMP_UNREACH_NEEDFRAG:
				printf(" !F-%d", pmtu);
				++unreachable;
				break;
			case ICMP_UNREACH_SRCFAIL:
#if ENABLE_TRACEROUTE6
			case ICMP6_DST_UNREACH_ADMIN << 8:
#endif
				printf(" !S");
				++unreachable;
				break;
			case ICMP_UNREACH_FILTER_PROHIB:
			case ICMP_UNREACH_NET_PROHIB:   /* misuse */
				printf(" !A");
				++unreachable;
				break;
			case ICMP_UNREACH_HOST_PROHIB:
				printf(" !C");
				++unreachable;
				break;
			case ICMP_UNREACH_HOST_PRECEDENCE:
				printf(" !V");
				++unreachable;
				break;
			case ICMP_UNREACH_PRECEDENCE_CUTOFF:
				printf(" !C");
				++unreachable;
				break;
			case ICMP_UNREACH_NET_UNKNOWN:
			case ICMP_UNREACH_HOST_UNKNOWN:
				printf(" !U");
				++unreachable;
				break;
			case ICMP_UNREACH_ISOLATED:
				printf(" !I");
				++unreachable;
				break;
			case ICMP_UNREACH_TOSNET:
			case ICMP_UNREACH_TOSHOST:
				printf(" !T");
				++unreachable;
				break;
			default:
				printf(" !<%d>", icmp_code);
				++unreachable;
				break;
			}
 next_probe: ;
		} /* for (nprobes) */

		bb_putchar('\n');
		if (got_there
		 || (unreachable > 0 && unreachable >= G.nprobes - 1)
		) {
			break;
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
#if ENABLE_FEATURE_TRACEROUTE_VERBOSE
		free(G.to);
#endif
		free(lastaddr);
		free(G.from_lsa);
	}

	return 0;
}

#if ENABLE_TRACEROUTE
int traceroute_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int traceroute_main(int argc UNUSED_PARAM, char **argv)
{
	return common_traceroute_main(0, argv);
}
#endif

#if ENABLE_TRACEROUTE6
int traceroute6_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int traceroute6_main(int argc UNUSED_PARAM, char **argv)
{
	return common_traceroute_main(OPT_IPV6, argv);
}
#endif
