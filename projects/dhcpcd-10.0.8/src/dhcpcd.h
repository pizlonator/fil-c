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

#ifndef DHCPCD_H
#define DHCPCD_H

#include <sys/socket.h>
#include <net/if.h>

#include <stdio.h>

#include "config.h"
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#include "defs.h"
#include "control.h"
#include "if-options.h"

#define HWADDR_LEN	20
#define IF_SSIDLEN	32
#define PROFILE_LEN	64
#define SECRET_LEN	64

#define IF_INACTIVE	0
#define IF_ACTIVE	1
#define IF_ACTIVE_USER	2

#define	LINK_UP		1
#define	LINK_UNKNOWN	0
#define	LINK_DOWN	-1

#define IF_DATA_IPV4	0
#define IF_DATA_ARP	1
#define IF_DATA_IPV4LL	2
#define IF_DATA_DHCP	3
#define IF_DATA_IPV6	4
#define IF_DATA_IPV6ND	5
#define IF_DATA_DHCP6	6
#define IF_DATA_MAX	7

#ifdef __QNX__
/* QNX carries defines for, but does not actually support PF_LINK */
#undef IFLR_ACTIVE
#endif

struct interface {
	struct dhcpcd_ctx *ctx;
	TAILQ_ENTRY(interface) next;
	char name[IF_NAMESIZE];
	unsigned int index;
	unsigned int active;
	unsigned int flags;
	uint16_t hwtype; /* ARPHRD_ETHER for example */
	unsigned char hwaddr[HWADDR_LEN];
	uint8_t hwlen;
	unsigned short vlanid;
	unsigned int metric;
	int carrier;
	bool wireless;
	uint8_t ssid[IF_SSIDLEN];
	unsigned int ssid_len;

	char profile[PROFILE_LEN];
	struct if_options *options;
	void *if_data[IF_DATA_MAX];
};
TAILQ_HEAD(if_head, interface);

#include "privsep.h"

/* dhcpcd requires CMSG_SPACE to evaluate to a compile time constant. */
#if defined(__QNX) || \
	(defined(__NetBSD_Version__) && __NetBSD_Version__ < 600000000)
#undef CMSG_SPACE
#endif

#ifndef ALIGNBYTES
#define ALIGNBYTES (sizeof(int) - 1)
#endif
#ifndef ALIGN
#define	ALIGN(p) (((unsigned int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#endif
#ifndef CMSG_SPACE
#define	CMSG_SPACE(len)	(ALIGN(sizeof(struct cmsghdr)) + ALIGN(len))
#endif

struct passwd;

struct dhcpcd_ctx {
	char pidfile[sizeof(PIDFILE) + IF_NAMESIZE + 1];
	char vendor[256];
	int fork_fd;	/* FD for the fork init signal pipe */
	const char *cffile;
	unsigned long long options;
	char *logfile;
	int argc;
	char **argv;
	int ifac;	/* allowed interfaces */
	char **ifav;	/* allowed interfaces */
	int ifdc;	/* denied interfaces */
	char **ifdv;	/* denied interfaces */
	int ifc;	/* listed interfaces */
	char **ifv;	/* listed interfaces */
	int ifcc;	/* configured interfaces */
	char **ifcv;	/* configured interfaces */
	uint8_t duid_type;
	unsigned char *duid;
	size_t duid_len;
	struct if_head *ifaces;

	char *ctl_buf;
	size_t ctl_buflen;
	size_t ctl_bufpos;
	size_t ctl_extra;

	rb_tree_t routes;	/* our routes */
#ifdef RT_FREE_ROUTE_TABLE
	rb_tree_t froutes;	/* free routes for re-use */
#endif
	size_t rt_order;	/* route order storage */

	int pf_inet_fd;
#ifdef PF_LINK
	int pf_link_fd;
#endif
	void *priv;
	int link_fd;
#ifndef SMALL
	int link_rcvbuf;
#endif
	int seq;	/* route message sequence no */
	int sseq;	/* successful seq no sent */

#ifdef USE_SIGNALS
	sigset_t sigset;
#endif
	struct eloop *eloop;

	char *script;
#ifdef HAVE_OPEN_MEMSTREAM
	FILE *script_fp;
#endif
	char *script_buf;
	size_t script_buflen;
	char **script_env;
	size_t script_envlen;

	int control_fd;
	int control_unpriv_fd;
	struct fd_list_head control_fds;
	char control_sock[sizeof(CONTROLSOCKET) + IF_NAMESIZE];
	char control_sock_unpriv[sizeof(CONTROLSOCKET) + IF_NAMESIZE + 7];
	gid_t control_group;

	/* DHCP Enterprise options, RFC3925 */
	struct dhcp_opt *vivso;
	size_t vivso_len;

	char *randomstate; /* original state */

	/* For filtering RTM_MISS messages per router */
#ifdef BSD
	uint8_t *rt_missfilter;
	size_t rt_missfilterlen;
	size_t rt_missfiltersize;
#endif

#ifdef PRIVSEP
	struct passwd *ps_user;	/* struct passwd for privsep user */
	struct ps_process_head ps_processes;	/* List of spawned processes */
	struct ps_process *ps_root;
	struct ps_process *ps_inet;
	struct ps_process *ps_ctl;
	int ps_data_fd;		/* data returned from processes */
	int ps_log_fd;		/* chroot logging */
	int ps_log_root_fd;	/* outside chroot log reader */
	struct eloop *ps_eloop;	/* eloop for polling root data */
	struct fd_list *ps_control;		/* Queue for the above */
	struct fd_list *ps_control_client;	/* Queue for the above */
#endif

#ifdef INET
	struct dhcp_opt *dhcp_opts;
	size_t dhcp_opts_len;

	int udp_rfd;
	int udp_wfd;

	/* Our aggregate option buffer.
	 * We ONLY use this when options are split, which for most purposes is
	 * practically never. See RFC3396 for details. */
	uint8_t *opt_buffer;
	size_t opt_buffer_len;
#endif
#ifdef INET6
	uint8_t *secret;
	size_t secret_len;

#ifndef __sun
	int nd_fd;
#endif
	struct ra_head *ra_routers;

	struct dhcp_opt *nd_opts;
	size_t nd_opts_len;
#ifdef DHCP6
	int dhcp6_rfd;
	int dhcp6_wfd;
	struct dhcp_opt *dhcp6_opts;
	size_t dhcp6_opts_len;
#endif

#ifndef __linux__
	int ra_global;
#endif
#endif /* INET6 */

#ifdef PLUGIN_DEV
	char *dev_load;
	int dev_fd;
	struct dev *dev;
	void *dev_handle;
#endif
};

#ifdef USE_SIGNALS
extern const int dhcpcd_signals[];
extern const size_t dhcpcd_signals_len;
extern const int dhcpcd_signals_ignore[];
extern const size_t dhcpcd_signals_ignore_len;
#endif

extern const char *dhcpcd_default_script;

int dhcpcd_ifafwaiting(const struct interface *);
int dhcpcd_afwaiting(const struct dhcpcd_ctx *);
void dhcpcd_daemonised(struct dhcpcd_ctx *);
void dhcpcd_daemonise(struct dhcpcd_ctx *);

void dhcpcd_signal_cb(int, void *);

void dhcpcd_linkoverflow(struct dhcpcd_ctx *);
int dhcpcd_handleargs(struct dhcpcd_ctx *, struct fd_list *, int, char **);
void dhcpcd_handlecarrier(struct interface *, int, unsigned int);
int dhcpcd_handleinterface(void *, int, const char *);
void dhcpcd_handlehwaddr(struct interface *, uint16_t, const void *, uint8_t);
void dhcpcd_dropinterface(struct interface *, const char *);
int dhcpcd_selectprofile(struct interface *, const char *);

void dhcpcd_startinterface(void *);
void dhcpcd_activateinterface(struct interface *, unsigned long long);

#endif
