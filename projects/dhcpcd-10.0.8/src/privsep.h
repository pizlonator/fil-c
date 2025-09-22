/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd
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

#ifndef PRIVSEP_H
#define PRIVSEP_H

//#define PRIVSEP_DEBUG

/* Start flags */
#define	PSF_DROPPRIVS		0x01
#define	PSF_ELOOP		0x02

/* Protocols */
#define	PS_BOOTP		0x0001
#define	PS_ND			0x0002
#define	PS_DHCP6		0x0003
#define	PS_BPF_BOOTP		0x0004
#define	PS_BPF_ARP		0x0005

/* Generic commands */
#define	PS_IOCTL		0x0010
#define	PS_ROUTE		0x0011	/* Also used for NETLINK */
#define	PS_SCRIPT		0x0012
#define	PS_UNLINK		0x0013
#define	PS_READFILE		0x0014
#define	PS_WRITEFILE		0x0015
#define	PS_FILEMTIME		0x0016
#define	PS_AUTH_MONORDM		0x0017
#define	PS_CTL			0x0018
#define	PS_CTL_EOF		0x0019
#define	PS_LOGREOPEN		0x0020
#define	PS_STOPPROCS		0x0021
#define	PS_DAEMONISED		0x0022

/* Domains */
#define	PS_ROOT			0x0101
#define	PS_INET			0x0102
#define	PS_CONTROL		0x0103

/* BSD Commands */
#define	PS_IOCTLLINK		0x0201
#define	PS_IOCTL6		0x0202
#define	PS_IOCTLINDIRECT	0x0203
#define	PS_IP6FORWARDING	0x0204
#define	PS_GETIFADDRS		0x0205
#define	PS_IFIGNOREGRP		0x0206
#define	PS_SYSCTL		0x0207

/* Dev Commands */
#define	PS_DEV_LISTENING	0x1001
#define	PS_DEV_INITTED		0x1002
#define	PS_DEV_IFCMD		0x1003

/* Dev Interface Commands (via flags) */
#define	PS_DEV_IFADDED		0x0001
#define	PS_DEV_IFREMOVED	0x0002
#define	PS_DEV_IFUPDATED	0x0003

/* Control Type (via flags) */
#define	PS_CTL_PRIV		0x0004
#define	PS_CTL_UNPRIV		0x0005

/* Sysctl Needs (via flags) */
#define	PS_SYSCTL_OLEN		0x0001
#define	PS_SYSCTL_ODATA		0x0002

/* Process commands */
#define	PS_START		0x4000
#define	PS_STOP			0x8000

/* Max INET message size + meta data for IPC */
#define	PS_BUFLEN		((64 * 1024) +			\
				 sizeof(struct ps_msghdr) +	\
				 sizeof(struct msghdr) +	\
				 CMSG_SPACE(sizeof(struct in6_pktinfo) + \
					    sizeof(int)))

#define	PSP_NAMESIZE		16 + INET_MAX_ADDRSTRLEN

/* Handy macro to work out if in the privsep engine or not. */
#define	IN_PRIVSEP(ctx)	\
	((ctx)->options & DHCPCD_PRIVSEP)
#define	IN_PRIVSEP_SE(ctx)	\
	(((ctx)->options & (DHCPCD_PRIVSEP | DHCPCD_FORKED)) == DHCPCD_PRIVSEP)

#define	PS_PROCESS_TIMEOUT	5	/* seconds to stop all processes */

#if defined(PRIVSEP) && defined(HAVE_CAPSICUM)
#define PRIVSEP_RIGHTS
#endif

#define PS_ROOT_FD(ctx) ((ctx)->ps_root ? (ctx)->ps_root->psp_fd : -1)

#if !defined(DISABLE_SECCOMP) && defined(__linux__)
# include <linux/version.h>
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
#  define HAVE_SECCOMP
# endif
#endif

#include "config.h"
#include "arp.h"
#include "dhcp.h"
#include "dhcpcd.h"

struct ps_addr {
	sa_family_t psa_family;
	uint8_t psa_pad[4 - sizeof(sa_family_t)];
	union {
		struct in_addr psau_in_addr;
		struct in6_addr psau_in6_addr;
	} psa_u;
#define	psa_in_addr	psa_u.psau_in_addr
#define	psa_in6_addr	psa_u.psau_in6_addr
};

/* Uniquely identify a process */
struct ps_id {
	struct ps_addr psi_addr;
	unsigned int psi_ifindex;
	uint16_t psi_cmd;
	uint8_t psi_pad[2];
};

struct ps_msghdr {
	uint16_t ps_cmd;
	uint8_t ps_pad[sizeof(unsigned long) - sizeof(uint16_t)];
	unsigned long ps_flags;
	struct ps_id ps_id;
	socklen_t ps_namelen;
	socklen_t ps_controllen;
	uint8_t ps_pad2[sizeof(size_t) - sizeof(socklen_t)];
	size_t ps_datalen;
};

struct ps_msg {
	struct ps_msghdr psm_hdr;
	uint8_t psm_data[PS_BUFLEN];
};

struct bpf;

struct ps_process {
	TAILQ_ENTRY(ps_process) next;
	struct dhcpcd_ctx *psp_ctx;
	struct ps_id psp_id;
	pid_t psp_pid;
	int psp_fd;
	int psp_work_fd;
	unsigned int psp_ifindex;
	char psp_ifname[IF_NAMESIZE];
	char psp_name[PSP_NAMESIZE];
	uint16_t psp_proto;
	const char *psp_protostr;
	bool psp_started;

#ifdef INET
	int (*psp_filter)(const struct bpf *, const struct in_addr *);
	struct interface psp_ifp; /* Move BPF gubbins elsewhere */
	struct bpf *psp_bpf;
#endif

#ifdef HAVE_CAPSICUM
	int psp_pfd;
#endif
};
TAILQ_HEAD(ps_process_head, ps_process);

#include "privsep-control.h"
#include "privsep-inet.h"
#include "privsep-root.h"
#ifdef INET
#include "privsep-bpf.h"
#endif

int ps_init(struct dhcpcd_ctx *);
int ps_start(struct dhcpcd_ctx *);
int ps_stop(struct dhcpcd_ctx *);
int ps_stopwait(struct dhcpcd_ctx *);
int ps_entersandbox(const char *, const char **);
int ps_managersandbox(struct dhcpcd_ctx *, const char *);
ssize_t ps_daemonised(struct dhcpcd_ctx *);

int ps_unrollmsg(struct msghdr *, struct ps_msghdr *, const void *, size_t);
ssize_t ps_sendpsmmsg(struct dhcpcd_ctx *, int,
    struct ps_msghdr *, const struct msghdr *);
ssize_t ps_sendpsmdata(struct dhcpcd_ctx *, int,
    struct ps_msghdr *, const void *, size_t);
ssize_t ps_sendmsg(struct dhcpcd_ctx *, int, uint16_t, unsigned long,
    const struct msghdr *);
ssize_t ps_sendcmd(struct dhcpcd_ctx *, int, uint16_t, unsigned long,
    const void *data, size_t len);
ssize_t ps_recvmsg(int, unsigned short, uint16_t, int);
ssize_t ps_recvpsmsg(struct dhcpcd_ctx *, int, unsigned short,
    ssize_t (*callback)(void *, struct ps_msghdr *, struct msghdr *), void *);

/* Internal privsep functions. */
int ps_setbuf_fdpair(int []);

#ifdef PRIVSEP_RIGHTS
int ps_rights_limit_ioctl(int);
int ps_rights_limit_fd_fctnl(int);
int ps_rights_limit_fd_rdonly(int);
int ps_rights_limit_fd_sockopt(int);
int ps_rights_limit_fd(int);
int ps_rights_limit_fdpair(int []);
#endif

#ifdef HAVE_SECCOMP
int ps_seccomp_enter(void);
#endif

pid_t ps_startprocess(struct ps_process *,
    void (*recv_msg)(void *, unsigned short),
    void (*recv_unpriv_msg)(void *, unsigned short),
    int (*callback)(struct ps_process *), void (*)(int, void *),
    unsigned int);
int ps_stopprocess(struct ps_process *);
struct ps_process *ps_findprocess(struct dhcpcd_ctx *, struct ps_id *);
struct ps_process *ps_findprocesspid(struct dhcpcd_ctx *, pid_t);
struct ps_process *ps_newprocess(struct dhcpcd_ctx *, struct ps_id *);
bool ps_waitforprocs(struct dhcpcd_ctx *ctx);
void ps_process_timeout(void *);
void ps_freeprocess(struct ps_process *);
void ps_freeprocesses(struct dhcpcd_ctx *, struct ps_process *);
#endif
