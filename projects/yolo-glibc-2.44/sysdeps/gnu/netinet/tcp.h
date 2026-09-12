/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_TCP_H
#define _NETINET_TCP_H	1

#include <features.h>

/*
 * User-settable options (used with setsockopt).
 */
#define	TCP_NODELAY		 1  /* Don't delay send to coalesce packets  */
#define	TCP_MAXSEG		 2  /* Set maximum segment size  */
#define TCP_CORK		 3  /* Control sending of partial frames  */
#define TCP_KEEPIDLE		 4  /* Start keeplives after this period */
#define TCP_KEEPINTVL		 5  /* Interval between keepalives */
#define TCP_KEEPCNT		 6  /* Number of keepalives before death */
#define TCP_SYNCNT		 7  /* Number of SYN retransmits */
#define TCP_LINGER2		 8  /* Life time of orphaned FIN-WAIT-2 state */
#define TCP_DEFER_ACCEPT	 9  /* Wake up listener only when data arrive */
#define TCP_WINDOW_CLAMP	 10 /* Bound advertised window */
#define TCP_INFO		 11 /* Information about this connection. */
#define	TCP_QUICKACK		 12 /* Bock/re-enable quick ACKs.  */
#define TCP_CONGESTION		 13 /* Congestion control algorithm.  */
#define TCP_MD5SIG		 14 /* TCP MD5 Signature (RFC2385) */
#define TCP_COOKIE_TRANSACTIONS	 15 /* TCP Cookie Transactions */
#define TCP_THIN_LINEAR_TIMEOUTS 16 /* Use linear timeouts for thin streams*/
#define TCP_THIN_DUPACK		 17 /* Fast retrans. after 1 dupack */
#define TCP_USER_TIMEOUT	 18 /* How long for loss retry before timeout */
#define TCP_REPAIR		 19 /* TCP sock is under repair right now */
#define TCP_REPAIR_QUEUE	 20 /* Set TCP queue to repair */
#define TCP_QUEUE_SEQ		 21 /* Set sequence number of repaired queue. */
#define TCP_REPAIR_OPTIONS	 22 /* Repair TCP connection options */
#define TCP_FASTOPEN		 23 /* Enable FastOpen on listeners */
#define TCP_TIMESTAMP		 24 /* TCP time stamp */
#define TCP_NOTSENT_LOWAT	 25 /* Limit number of unsent bytes in
				       write queue.  */
#define TCP_CC_INFO		 26 /* Get Congestion Control
				       (optional) info.  */
#define TCP_SAVE_SYN		 27 /* Record SYN headers for new
				       connections.  */
#define TCP_SAVED_SYN		 28 /* Get SYN headers recorded for
				       connection.  */
#define TCP_REPAIR_WINDOW	 29 /* Get/set window parameters.  */
#define TCP_FASTOPEN_CONNECT	 30 /* Attempt FastOpen with connect.  */
#define TCP_ULP			 31 /* Attach a ULP to a TCP connection.  */
#define TCP_MD5SIG_EXT		 32 /* TCP MD5 Signature with extensions.  */
#define TCP_FASTOPEN_KEY	 33 /* Set the key for Fast Open (cookie).  */
#define TCP_FASTOPEN_NO_COOKIE	 34 /* Enable TFO without a TFO cookie.  */
#define TCP_ZEROCOPY_RECEIVE	 35
#define TCP_INQ			 36 /* Notify bytes available to read
				       as a cmsg on read.  */
#define TCP_CM_INQ		 TCP_INQ
#define TCP_TX_DELAY		 37 /* Delay outgoing packets by XX usec.  */
#define TCP_RTO_MAX_MS		 44 /* Max time to retransmit (msec).  */
#define TCP_RTO_MIN_US		 45 /* Min time to retransmit (usec).  */
#define TCP_DELACK_MAX_US	 46 /* Max delayed ack time (usec).  */

#define TCP_REPAIR_ON		 1
#define TCP_REPAIR_OFF		 0
#define TCP_REPAIR_OFF_NO_WP	 -1

#ifdef __USE_MISC
# include <sys/types.h>
# include <sys/socket.h>
# include <stdint.h>

typedef	uint32_t tcp_seq;
/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
struct tcphdr
  {
    __extension__ union
    {
      struct
      {
	uint16_t th_sport;	/* source port */
	uint16_t th_dport;	/* destination port */
	tcp_seq th_seq;		/* sequence number */
	tcp_seq th_ack;		/* acknowledgement number */
# if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t th_x2:4;	/* (unused) */
	uint8_t th_off:4;	/* data offset */
# endif
# if __BYTE_ORDER == __BIG_ENDIAN
	uint8_t th_off:4;	/* data offset */
	uint8_t th_x2:4;	/* (unused) */
# endif
	uint8_t th_flags;
# define TH_FIN	0x01
# define TH_SYN	0x02
# define TH_RST	0x04
# define TH_PUSH	0x08
# define TH_ACK	0x10
# define TH_URG	0x20
	uint16_t th_win;	/* window */
	uint16_t th_sum;	/* checksum */
	uint16_t th_urp;	/* urgent pointer */
      };
      struct
      {
	uint16_t source;
	uint16_t dest;
	uint32_t seq;
	uint32_t ack_seq;
# if __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t res1:4;
	uint16_t doff:4;
	uint16_t fin:1;
	uint16_t syn:1;
	uint16_t rst:1;
	uint16_t psh:1;
	uint16_t ack:1;
	uint16_t urg:1;
	uint16_t res2:2;
# elif __BYTE_ORDER == __BIG_ENDIAN
	uint16_t doff:4;
	uint16_t res1:4;
	uint16_t res2:2;
	uint16_t urg:1;
	uint16_t ack:1;
	uint16_t psh:1;
	uint16_t rst:1;
	uint16_t syn:1;
	uint16_t fin:1;
# else
#  error "Adjust your <bits/endian.h> defines"
# endif
	uint16_t window;
	uint16_t check;
	uint16_t urg_ptr;
      };
    };
};

enum
{
  TCP_ESTABLISHED = 1,
  TCP_SYN_SENT,
  TCP_SYN_RECV,
  TCP_FIN_WAIT1,
  TCP_FIN_WAIT2,
  TCP_TIME_WAIT,
  TCP_CLOSE,
  TCP_CLOSE_WAIT,
  TCP_LAST_ACK,
  TCP_LISTEN,
  TCP_CLOSING   /* now a valid state */
};

# define TCPOPT_EOL		0
# define TCPOPT_NOP		1
# define TCPOPT_MAXSEG		2
# define TCPOLEN_MAXSEG		4
# define TCPOPT_WINDOW		3
# define TCPOLEN_WINDOW		3
# define TCPOPT_SACK_PERMITTED	4		/* Experimental */
# define TCPOLEN_SACK_PERMITTED	2
# define TCPOPT_SACK		5		/* Experimental */
# define TCPOPT_TIMESTAMP	8
# define TCPOLEN_TIMESTAMP	10
# define TCPOLEN_TSTAMP_APPA	(TCPOLEN_TIMESTAMP+2) /* appendix A */

# define TCPOPT_TSTAMP_HDR	\
    (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)

/*
 * Default maximum segment size for TCP.
 * With an IP MSS of 576, this is 536,
 * but 512 is probably more convenient.
 * This should be defined as MIN(512, IP_MSS - sizeof (struct tcpiphdr)).
 */
# define TCP_MSS	512

# define TCP_MAXWIN	65535	/* largest value for (unscaled) window */

# define TCP_MAX_WINSHIFT	14	/* maximum window shift */

# define SOL_TCP		6	/* TCP level */


# define TCPI_OPT_TIMESTAMPS	1
# define TCPI_OPT_SACK		2
# define TCPI_OPT_WSCALE	4
# define TCPI_OPT_ECN		8  /* ECN was negotiated at TCP session init */
# define TCPI_OPT_ECN_SEEN	16 /* we received at least one packet with ECT */
# define TCPI_OPT_SYN_DATA	32 /* SYN-ACK acked data in SYN sent or rcvd */
# define TCPI_OPT_USEC_TS	64 /* usec timestamps */
# define TCPI_OPT_TFO_CHILD	128 /* child from a Fast Open option on SYN */


/* Values for tcpi_state.  */
enum tcp_ca_state
{
  TCP_CA_Open = 0,
  TCP_CA_Disorder = 1,
  TCP_CA_CWR = 2,
  TCP_CA_Recovery = 3,
  TCP_CA_Loss = 4
};

/* Values for tcpi_ecn_mode after negotiation.  */
#define TCPI_ECN_MODE_DISABLED	0x0
#define TCPI_ECN_MODE_RFC3168	0x1
#define TCPI_ECN_MODE_ACCECN	0x2
#define TCPI_ECN_MODE_PENDING	0x3

/* Values for tcpi_accecn_opt_seen.  */
#define TCP_ACCECN_OPT_NOT_SEEN		0x0
#define TCP_ACCECN_OPT_EMPTY_SEEN	0x1
#define TCP_ACCECN_OPT_COUNTER_SEEN	0x2
#define TCP_ACCECN_OPT_FAIL_SEEN	0x3

/* Values for tcpi_accecn_fail_mode.  */
#define TCP_ACCECN_ACE_FAIL_SEND	0x1
#define TCP_ACCECN_ACE_FAIL_RECV	0x2
#define TCP_ACCECN_OPT_FAIL_SEND	0x4
#define TCP_ACCECN_OPT_FAIL_RECV	0x8

struct tcp_info
{
  uint8_t	tcpi_state;
  uint8_t	tcpi_ca_state;
  uint8_t	tcpi_retransmits;
  uint8_t	tcpi_probes;
  uint8_t	tcpi_backoff;
  uint8_t	tcpi_options;
  uint8_t	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;

  uint32_t	tcpi_rto;
  uint32_t	tcpi_ato;
  uint32_t	tcpi_snd_mss;
  uint32_t	tcpi_rcv_mss;

  uint32_t	tcpi_unacked;
  uint32_t	tcpi_sacked;
  uint32_t	tcpi_lost;
  uint32_t	tcpi_retrans;
  uint32_t	tcpi_fackets;

  /* Times. */
  uint32_t	tcpi_last_data_sent;
  uint32_t	tcpi_last_ack_sent;	/* Not remembered, sorry.  */
  uint32_t	tcpi_last_data_recv;
  uint32_t	tcpi_last_ack_recv;

  /* Metrics. */
  uint32_t	tcpi_pmtu;
  uint32_t	tcpi_rcv_ssthresh;
  uint32_t	tcpi_rtt;
  uint32_t	tcpi_rttvar;
  uint32_t	tcpi_snd_ssthresh;
  uint32_t	tcpi_snd_cwnd;
  uint32_t	tcpi_advmss;
  uint32_t	tcpi_reordering;

  uint32_t	tcpi_rcv_rtt;
  uint32_t	tcpi_rcv_space;

  uint32_t	tcpi_total_retrans;

  uint64_t	tcpi_pacing_rate;
  uint64_t	tcpi_max_pacing_rate;
  uint64_t	tcpi_bytes_acked;    /* RFC4898 tcpEStatsAppHCThruOctetsAcked */
  uint64_t	tcpi_bytes_received; /* RFC4898 tcpEStatsAppHCThruOctetsReceived */
  uint32_t	tcpi_segs_out;       /* RFC4898 tcpEStatsPerfSegsOut */
  uint32_t	tcpi_segs_in;        /* RFC4898 tcpEStatsPerfSegsIn */

  uint32_t	tcpi_notsent_bytes;
  uint32_t	tcpi_min_rtt;
  uint32_t	tcpi_data_segs_in;    /* RFC4898 tcpEStatsDataSegsIn */
  uint32_t	tcpi_data_segs_out;   /* RFC4898 tcpEStatsDataSegsOut */

  uint64_t	tcpi_delivery_rate;

  uint64_t	tcpi_busy_time;       /* Time (usec) busy sending data */
  uint64_t	tcpi_rwnd_limited;    /* Time (usec) limited by receive window */
  uint64_t	tcpi_sndbuf_limited;  /* Time (usec) limited by send buffer */

  uint32_t	tcpi_delivered;
  uint32_t	tcpi_delivered_ce;

  uint64_t	tcpi_bytes_sent;    /* RFC4898 tcpEStatsPerfHCDataOctetsOut */
  uint64_t	tcpi_bytes_retrans; /* RFC4898 tcpEStatsPerfOctetsRetrans */
  uint32_t	tcpi_dsack_dups;    /* RFC4898 tcpEStatsStackDSACKDups */
  uint32_t	tcpi_reord_seen;    /* reordering events seen */


  uint32_t	tcpi_rcv_ooopack; /* Out-of-order packets received */
  /* Peer's advertised receive window after scaling (bytes) */
  uint32_t	tcpi_snd_wnd;
  /* Local advertised receive window after scaling (bytes) */
  uint32_t	tcpi_rcv_wnd;

  uint32_t	tcpi_rehash; /* PLB or timeout triggered rehash attempts */
  /* Total number of RTO timeouts, including
   * SYN/SYN-ACK and recurring timeouts
   */
  uint16_t	tcpi_total_rto;
  /* Total number of RTO recoveries, including any unfinished recovery. */
  uint16_t	tcpi_total_rto_recoveries;
  /* Total time spent in RTO recoveries in milliseconds, including any
   * unfinished recovery.
   */
  uint32_t	tcpi_total_rto_time;
  uint32_t	tcpi_received_ce;        /* # of CE marks received */
  uint32_t	tcpi_delivered_e1_bytes; /* Accurate ECN byte counters */
  uint32_t	tcpi_delivered_e0_bytes;
  uint32_t	tcpi_delivered_ce_bytes;
  uint32_t	tcpi_received_e1_bytes;
  uint32_t	tcpi_received_e0_bytes;
  uint32_t	tcpi_received_ce_bytes;
  uint32_t	tcpi_ecn_mode:2,
		tcpi_accecn_opt_seen:2,
		tcpi_accecn_fail_mode:4,
		tcpi_options2:24;
};

/* Netlink attributes types for SCM_TIMESTAMPING_OPT_STATS */
enum {
  TCP_NLA_PAD,
  TCP_NLA_BUSY,                   /* Time (usec) busy sending data */
  TCP_NLA_RWND_LIMITED,           /* Time (usec) limited by receive window */
  TCP_NLA_SNDBUF_LIMITED,         /* Time (usec) limited by send buffer */
  TCP_NLA_DATA_SEGS_OUT,          /* Data pkts sent including retransmission */
  TCP_NLA_TOTAL_RETRANS,          /* Data pkts retransmitted */
  TCP_NLA_PACING_RATE,            /* Pacing rate in bytes per second */
  TCP_NLA_DELIVERY_RATE,          /* Delivery rate in bytes per second */
  TCP_NLA_SND_CWND,               /* Sending congestion window */
  TCP_NLA_REORDERING,             /* Reordering metric */
  TCP_NLA_MIN_RTT,                /* minimum RTT */
  TCP_NLA_RECUR_RETRANS,          /* Recurring retransmits for the current pkt */
  TCP_NLA_DELIVERY_RATE_APP_LMT,  /* delivery rate application limited ? */
  TCP_NLA_SNDQ_SIZE,              /* Data (bytes) pending in send queue */
  TCP_NLA_CA_STATE,               /* ca_state of socket */
  TCP_NLA_SND_SSTHRESH,           /* Slow start size threshold */
  TCP_NLA_DELIVERED,              /* Data pkts delivered incl. out-of-order */
  TCP_NLA_DELIVERED_CE,           /* Like above but only ones w/ CE marks */
  TCP_NLA_BYTES_SENT,             /* Data bytes sent including retransmission */
  TCP_NLA_BYTES_RETRANS,          /* Data bytes retransmitted */
  TCP_NLA_DSACK_DUPS,             /* DSACK blocks received */
  TCP_NLA_REORD_SEEN,             /* reordering events seen */
  TCP_NLA_SRTT,                   /* smoothed RTT in usecs */
  TCP_NLA_TIMEOUT_REHASH,         /* Timeout-triggered rehash attempts */
  TCP_NLA_BYTES_NOTSENT,          /* Bytes in write queue not yet sent */
  TCP_NLA_EDT,                    /* Earliest departure time (CLOCK_MONOTONIC) */
  TCP_NLA_TTL,                    /* TTL or hop limit of a packet received */
  TCP_NLA_REHASH,                 /* PLB and timeout triggered rehash attempts */
};

/* For TCP_MD5SIG socket option.  */
#define TCP_MD5SIG_MAXKEYLEN	80

/* tcp_md5sig extension flags for TCP_MD5SIG_EXT.  */
#define TCP_MD5SIG_FLAG_PREFIX	1 /* Address prefix length.  */
#define TCP_MD5SIG_FLAG_IFINDEX	2 /* Ifindex set.  */

struct tcp_md5sig
{
  struct sockaddr_storage tcpm_addr;		/* Address associated.  */
  uint8_t	tcpm_flags;			/* Extension flags.  */
  uint8_t	tcpm_prefixlen;			/* Address prefix.  */
  uint16_t	tcpm_keylen;			/* Key length.  */
  int		tcpm_ifindex;			/* Device index for scope.  */
  uint8_t	tcpm_key[TCP_MD5SIG_MAXKEYLEN];	/* Key (binary).  */
};

/* INET_DIAG_MD5SIG */
struct tcp_diag_md5sig {
  uint8_t   tcpm_family;
  uint8_t   tcpm_prefixlen;
  uint16_t  tcpm_keylen;
  uint32_t  tcpm_addr[4];
  uint8_t   tcpm_key[TCP_MD5SIG_MAXKEYLEN];
};

#define TCP_AO_MAXKEYLEN  80

#define TCP_AO_KEYF_IFINDEX  (1 << 0)     /* L3 ifindex for VRF */
#define TCP_AO_KEYF_EXCLUDE_OPT  (1 << 1) /* Indicates whether TCP options
                                           * other than TCP-AO are included
                                           * in the MAC calculation
                                           */

struct tcp_ao_add { /* setsockopt(TCP_AO_ADD_KEY) */
  struct sockaddr_storage addr;   /* Peer's address for the key */
  int8_t    alg_name[64];         /* Crypto hash algorithm to use */
  int32_t   ifindex;              /* L3 dev index for VRF */
  uint32_t  set_current  :1,      /* Set key as Current_key at once */
            set_rnext    :1,      /* Request it from peer with RNext_key */
            reserved     :30;     /* Must be 0 */
  uint16_t  reserved2;            /* Padding, must be 0 */
  uint8_t   prefix;               /* Peer's address prefix */
  uint8_t   sndid;                /* SendID for outgoing segments */
  uint8_t   rcvid;                /* RecvID to match for incoming seg */
  uint8_t   maclen;               /* length of authentication code (hash) */
  uint8_t   keyflags;             /* See TCP_AO_KEYF_ */
  uint8_t   keylen;               /* Length of ::key */
  uint8_t   key[TCP_AO_MAXKEYLEN];
} __attribute__((aligned(8)));

struct tcp_ao_del { /* setsockopt(TCP_AO_DEL_KEY) */
  struct sockaddr_storage addr;   /* Peer's address for the key */
  int32_t  ifindex;               /* L3 dev index for VRF */
  uint32_t set_current  :1,       /* Corresponding ::current_key */
           set_rnext    :1,       /* Corresponding ::rnext */
           del_async    :1,       /* Only valid for listen sockets */
           reserved     :29;      /* Must be 0 */
  uint16_t reserved2;             /* Padding, must be 0 */
  uint8_t  prefix;                /* Peer's address prefix */
  uint8_t  sndid;                 /* SendID for outgoing segments */
  uint8_t  rcvid;                 /* RecvID to match for incoming seg */
  uint8_t  current_key;           /* KeyID to set as Current_key */
  uint8_t  rnext;                 /* KeyID to set as Rnext_key */
  uint8_t  keyflags;              /* See TCP_AO_KEYF_ */
} __attribute__((aligned(8)));

struct tcp_ao_info_opt { /* setsockopt(TCP_AO_INFO), getsockopt(TCP_AO_INFO)
                          */
  /* Here 'in' is for setsockopt(), 'out' is for getsockopt() */
  uint32_t  set_current   :1,   /* In/out: corresponding ::current_key */
            set_rnext     :1,   /* In/out: corresponding ::rnext */
            ao_required   :1,   /* In/out: don't accept non-AO connects */
            set_counters  :1,   /* In: set/clear ::pkt_* counters */
            accept_icmps  :1,   /* In/out: accept incoming ICMPs */
            reserved      :27;  /* must be 0 */
  uint16_t  reserved2;          /* Padding, must be 0 */
  uint8_t   current_key;        /* In/out: KeyID of Current_key */
  uint8_t   rnext;              /* In/out: keyid of RNext_key */
  uint64_t  pkt_good;           /* In/out: verified segments */
  uint64_t  pkt_bad;            /* In/out: failed verification */
  uint64_t  pkt_key_not_found;  /* In/out: could not find a key to verify */
  uint64_t  pkt_ao_required;    /* In/out: segments missing TCP-AO sign */
  uint64_t  pkt_dropped_icmp;   /* In/out: ICMPs that were ignored */
} __attribute__((aligned(8)));

struct tcp_ao_getsockopt { /* getsockopt(TCP_AO_GET_KEYS) */
  struct sockaddr_storage addr;   /* In/out: dump keys for peer
                                   * with this address/prefix
                                   */
  uint8_t   alg_name[64];         /* out: crypto hash algorithm */
  uint8_t   key[TCP_AO_MAXKEYLEN];
  uint32_t  nkeys;                /* In: size of the userspace buffer
                                   * @optval, measured in @optlen - the
                                   * sizeof(struct tcp_ao_getsockopt)
                                   * Out: number of keys that matched
                                   */
  uint16_t  is_current  :1,       /* In: match and dump Current_key,
                                   * Out: the dumped key is Current_key
                                   */
            is_rnext    :1,       /* In: match and dump RNext_key,
                                   * Out: the dumped key is RNext_key
                                   */
            get_all     :1,       /* In: dump all keys */
            reserved    :13;      /* Padding, must be 0 */
  uint8_t  sndid;                 /* In/out: dump keys with SendID */
  uint8_t  rcvid;                 /* In/out: dump keys with RecvID */
  uint8_t  prefix;                /* In/out: dump keys with address/prefix */
  uint8_t  maclen;                /* Out: key's length of authentication
                                   * code (hash)
                                  */
  uint8_t  keyflags;              /* In/out: see TCP_AO_KEYF_ */
  uint8_t  keylen;                /* Out: length of ::key */
  int32_t  ifindex;               /* In/out: L3 dev index for VRF */
  uint64_t  pkt_good;             /* Out: verified segments */
  uint64_t  pkt_bad;              /* Out: segments that failed verification */
} __attribute__((aligned(8)));

struct tcp_ao_repair { /* {s,g}etsockopt(TCP_AO_REPAIR) */
  uint32_t      snt_isn;
  uint32_t      rcv_isn;
  uint32_t      snd_sne;
  uint32_t      rcv_sne;
} __attribute__((aligned(8)));

/* For socket repair options.  */
struct tcp_repair_opt
{
  uint32_t	opt_code;
  uint32_t	opt_val;
};

/* Queue to repair, for TCP_REPAIR_QUEUE.  */
enum
{
  TCP_NO_QUEUE,
  TCP_RECV_QUEUE,
  TCP_SEND_QUEUE,
  TCP_QUEUES_NR,
};

/* For cookie transactions socket options.  */
#define TCP_COOKIE_MIN		8		/*  64-bits */
#define TCP_COOKIE_MAX		16		/* 128-bits */
#define TCP_COOKIE_PAIR_SIZE	(2*TCP_COOKIE_MAX)

/* Flags for both getsockopt and setsockopt */
#define TCP_COOKIE_IN_ALWAYS	(1 << 0)	/* Discard SYN without cookie */
#define TCP_COOKIE_OUT_NEVER	(1 << 1)	/* Prohibit outgoing cookies,
						 * supersedes everything. */

/* Flags for getsockopt */
#define TCP_S_DATA_IN		(1 << 2)	/* Was data received? */
#define TCP_S_DATA_OUT		(1 << 3)	/* Was data sent? */

#define TCP_MSS_DEFAULT		 536U	/* IPv4 (RFC1122, RFC2581) */
#define TCP_MSS_DESIRED		1220U	/* IPv6 (tunneled), EDNS0 (RFC3226) */

struct tcp_cookie_transactions
{
  uint16_t	tcpct_flags;
  uint8_t	__tcpct_pad1;
  uint8_t	tcpct_cookie_desired;
  uint16_t	tcpct_s_data_desired;
  uint16_t	tcpct_used;
  uint8_t	tcpct_value[TCP_MSS_DEFAULT];
};

/* For use with TCP_REPAIR_WINDOW.  */
struct tcp_repair_window
{
  uint32_t snd_wl1;
  uint32_t snd_wnd;
  uint32_t max_window;
  uint32_t rcv_wnd;
  uint32_t rcv_wup;
};

/* For use with TCP_ZEROCOPY_RECEIVE.  */
struct tcp_zerocopy_receive
{
  uint64_t address; /* In: address of mapping.  */
  uint32_t length; /* In/out: number of bytes to map/mapped.  */
  uint32_t recv_skip_hint; /* Out: amount of bytes to skip.  */
  uint32_t inq; /* Out: amount of bytes in read queue.  */
  int32_t err; /* Out: socket error.  */
  uint64_t copybuf_address; /* On: copybuf address (small reads).  */
  int32_t copybuf_len; /* In/Out: copybuf bytes avail/used or error.  */
  uint32_t flags; /* In: flags.  */
  uint64_t msg_control; /* Ancillary data.  */
  uint64_t msg_controllen;
  uint32_t msg_flags;
  uint32_t reserved; /* Set to 0 for now.  */
};

#endif /* Misc.  */

#endif /* netinet/tcp.h */
