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

static const char dhcpcd_copyright[] = "Copyright (c) 2006-2023 Roy Marples";

#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "arp.h"
#include "common.h"
#include "control.h"
#include "dev.h"
#include "dhcp-common.h"
#include "dhcpcd.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "duid.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "script.h"

#ifdef HAVE_CAPSICUM
#include <sys/capsicum.h>
#endif
#ifdef HAVE_OPENSSL
#include <openssl/crypto.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif

#ifdef USE_SIGNALS
const int dhcpcd_signals[] = {
	SIGTERM,
	SIGINT,
	SIGALRM,
	SIGHUP,
	SIGUSR1,
	SIGUSR2,
	SIGCHLD,
};
const size_t dhcpcd_signals_len = __arraycount(dhcpcd_signals);

const int dhcpcd_signals_ignore[] = {
	SIGPIPE,
};
const size_t dhcpcd_signals_ignore_len = __arraycount(dhcpcd_signals_ignore);
#endif

const char *dhcpcd_default_script = SCRIPT;

static void
usage(void)
{

printf("usage: "PACKAGE"\t[-146ABbDdEGgHJKLMNPpqTV]\n"
	"\t\t[-C, --nohook hook] [-c, --script script]\n"
	"\t\t[-e, --env value] [-F, --fqdn FQDN] [-f, --config file]\n"
	"\t\t[-h, --hostname hostname] [-I, --clientid clientid]\n"
	"\t\t[-i, --vendorclassid vendorclassid] [-j, --logfile logfile]\n"
	"\t\t[-l, --leasetime seconds] [-m, --metric metric]\n"
	"\t\t[-O, --nooption option] [-o, --option option]\n"
	"\t\t[-Q, --require option] [-r, --request address]\n"
	"\t\t[-S, --static value]\n"
	"\t\t[-s, --inform address[/cidr[/broadcast_address]]]\n [--inform6]"
	"\t\t[-t, --timeout seconds] [-u, --userclass class]\n"
	"\t\t[-v, --vendor code, value] [-W, --whitelist address[/cidr]] [-w]\n"
	"\t\t[--waitip [4 | 6]] [-y, --reboot seconds]\n"
	"\t\t[-X, --blacklist address[/cidr]] [-Z, --denyinterfaces pattern]\n"
	"\t\t[-z, --allowinterfaces pattern] [--inactive] [interface] [...]\n"
	"       "PACKAGE"\t-n, --rebind [interface]\n"
	"       "PACKAGE"\t-k, --release [interface]\n"
	"       "PACKAGE"\t-U, --dumplease interface\n"
	"       "PACKAGE"\t--version\n"
	"       "PACKAGE"\t-x, --exit [interface]\n");
}

static void
free_globals(struct dhcpcd_ctx *ctx)
{
	struct dhcp_opt *opt;

	if (ctx->ifac) {
		for (; ctx->ifac > 0; ctx->ifac--)
			free(ctx->ifav[ctx->ifac - 1]);
		free(ctx->ifav);
		ctx->ifav = NULL;
	}
	if (ctx->ifdc) {
		for (; ctx->ifdc > 0; ctx->ifdc--)
			free(ctx->ifdv[ctx->ifdc - 1]);
		free(ctx->ifdv);
		ctx->ifdv = NULL;
	}
	if (ctx->ifcc) {
		for (; ctx->ifcc > 0; ctx->ifcc--)
			free(ctx->ifcv[ctx->ifcc - 1]);
		free(ctx->ifcv);
		ctx->ifcv = NULL;
	}

#ifdef INET
	if (ctx->dhcp_opts) {
		for (opt = ctx->dhcp_opts;
		    ctx->dhcp_opts_len > 0;
		    opt++, ctx->dhcp_opts_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->dhcp_opts);
		ctx->dhcp_opts = NULL;
	}
#endif
#ifdef INET6
	if (ctx->nd_opts) {
		for (opt = ctx->nd_opts;
		    ctx->nd_opts_len > 0;
		    opt++, ctx->nd_opts_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->nd_opts);
		ctx->nd_opts = NULL;
	}
#ifdef DHCP6
	if (ctx->dhcp6_opts) {
		for (opt = ctx->dhcp6_opts;
		    ctx->dhcp6_opts_len > 0;
		    opt++, ctx->dhcp6_opts_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->dhcp6_opts);
		ctx->dhcp6_opts = NULL;
	}
#endif
#endif
	if (ctx->vivso) {
		for (opt = ctx->vivso;
		    ctx->vivso_len > 0;
		    opt++, ctx->vivso_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->vivso);
		ctx->vivso = NULL;
	}
}

static void
handle_exit_timeout(void *arg)
{
	struct dhcpcd_ctx *ctx;

	ctx = arg;
	logerrx("timed out");
	if (!(ctx->options & DHCPCD_MANAGER)) {
		struct interface *ifp;

		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (ifp->active == IF_ACTIVE_USER)
				script_runreason(ifp, "STOPPED");
		}
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}
	ctx->options |= DHCPCD_NOWAITIP;
	dhcpcd_daemonise(ctx);
}

static const char *
dhcpcd_af(int af)
{

	switch (af) {
	case AF_UNSPEC:
		return "IP";
	case AF_INET:
		return "IPv4";
	case AF_INET6:
		return "IPv6";
	default:
		return NULL;
	}
}

int
dhcpcd_ifafwaiting(const struct interface *ifp)
{
	unsigned long long opts;
	bool foundany = false;

	if (ifp->active != IF_ACTIVE_USER)
		return AF_MAX;

#define DHCPCD_WAITALL	(DHCPCD_WAITIP4 | DHCPCD_WAITIP6)
	opts = ifp->options->options;
#ifdef INET
	if (opts & DHCPCD_WAITIP4 ||
	    (opts & DHCPCD_WAITIP && !(opts & DHCPCD_WAITALL)))
	{
		bool foundaddr = ipv4_hasaddr(ifp);

		if (opts & DHCPCD_WAITIP4 && !foundaddr)
			return AF_INET;
		if (foundaddr)
			foundany = true;
	}
#endif
#ifdef INET6
	if (opts & DHCPCD_WAITIP6 ||
	    (opts & DHCPCD_WAITIP && !(opts & DHCPCD_WAITALL)))
	{
		bool foundaddr = ipv6_hasaddr(ifp);

		if (opts & DHCPCD_WAITIP6 && !foundaddr)
			return AF_INET6;
		if (foundaddr)
			foundany = true;
	}
#endif

	if (opts & DHCPCD_WAITIP && !(opts & DHCPCD_WAITALL) && !foundany)
		return AF_UNSPEC;
	return AF_MAX;
}

int
dhcpcd_afwaiting(const struct dhcpcd_ctx *ctx)
{
	unsigned long long opts;
	const struct interface *ifp;
	int af;

	if (!(ctx->options & DHCPCD_WAITOPTS))
		return AF_MAX;

	opts = ctx->options;
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
#ifdef INET
		if (opts & (DHCPCD_WAITIP | DHCPCD_WAITIP4) &&
		    ipv4_hasaddr(ifp))
			opts &= ~(DHCPCD_WAITIP | DHCPCD_WAITIP4);
#endif
#ifdef INET6
		if (opts & (DHCPCD_WAITIP | DHCPCD_WAITIP6) &&
		    ipv6_hasaddr(ifp))
			opts &= ~(DHCPCD_WAITIP | DHCPCD_WAITIP6);
#endif
		if (!(opts & DHCPCD_WAITOPTS))
			break;
	}
	if (opts & DHCPCD_WAITIP)
		af = AF_UNSPEC;
	else if (opts & DHCPCD_WAITIP4)
		af = AF_INET;
	else if (opts & DHCPCD_WAITIP6)
		af = AF_INET6;
	else
		return AF_MAX;
	return af;
}

static int
dhcpcd_ipwaited(struct dhcpcd_ctx *ctx)
{
	struct interface *ifp;
	int af;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if ((af = dhcpcd_ifafwaiting(ifp)) != AF_MAX) {
			logdebugx("%s: waiting for an %s address",
			    ifp->name, dhcpcd_af(af));
			return 0;
		}
	}

	if ((af = dhcpcd_afwaiting(ctx)) != AF_MAX) {
		logdebugx("waiting for an %s address",
		    dhcpcd_af(af));
		return 0;
	}

	return 1;
}

#ifndef THERE_IS_NO_FORK
void
dhcpcd_daemonised(struct dhcpcd_ctx *ctx)
{
	unsigned int logopts = loggetopts();

	/*
	 * Stop writing to stderr.
	 * On the happy path, only the manager process writes to stderr,
	 * so this just stops wasting fprintf calls to nowhere.
	 */
	logopts &= ~LOGERR_ERR;
	logsetopts(logopts);

	/*
	 * We need to do something with stdout/stderr to avoid SIGPIPE.
	 * We know that stdin is already mapped to /dev/null.
	 * TODO: Capture script output and log it to the logfile and/or syslog.
	 */
	dup2(STDIN_FILENO, STDOUT_FILENO);
	dup2(STDIN_FILENO, STDERR_FILENO);

	ctx->options |= DHCPCD_DAEMONISED;
}
#endif

/* Returns the pid of the child, otherwise 0. */
void
dhcpcd_daemonise(struct dhcpcd_ctx *ctx)
{
#ifdef THERE_IS_NO_FORK
	eloop_timeout_delete(ctx->eloop, handle_exit_timeout, ctx);
	errno = ENOSYS;
	return;
#else
	int exit_code;

	if (ctx->options & DHCPCD_DAEMONISE &&
	    !(ctx->options & (DHCPCD_DAEMONISED | DHCPCD_NOWAITIP)))
	{
		if (!dhcpcd_ipwaited(ctx))
			return;
	}

	if (ctx->options & DHCPCD_ONESHOT) {
		loginfox("exiting due to oneshot");
		eloop_exit(ctx->eloop, EXIT_SUCCESS);
		return;
	}

	eloop_timeout_delete(ctx->eloop, handle_exit_timeout, ctx);
	if (ctx->options & DHCPCD_DAEMONISED ||
	    !(ctx->options & DHCPCD_DAEMONISE))
		return;

#ifdef PRIVSEP
	if (IN_PRIVSEP(ctx))
		ps_daemonised(ctx);
	else
#endif
		dhcpcd_daemonised(ctx);

	eloop_event_delete(ctx->eloop, ctx->fork_fd);
	exit_code = EXIT_SUCCESS;
	if (write(ctx->fork_fd, &exit_code, sizeof(exit_code)) == -1)
		logerr(__func__);
	close(ctx->fork_fd);
	ctx->fork_fd = -1;
#endif
}

static void
dhcpcd_drop_af(struct interface *ifp, int stop, int af)
{

	if (af == AF_UNSPEC || af == AF_INET6) {
#ifdef DHCP6
		dhcp6_drop(ifp, stop ? NULL : "EXPIRE6");
#endif
#ifdef INET6
		ipv6nd_drop(ifp);
		ipv6_drop(ifp);
#endif
	}

	if (af == AF_UNSPEC || af == AF_INET) {
#ifdef IPV4LL
		ipv4ll_drop(ifp);
#endif
#ifdef INET
		dhcp_drop(ifp, stop ? "STOP" : "EXPIRE");
#endif
#ifdef ARP
		arp_drop(ifp);
#endif
	}

#if !defined(DHCP6) && !defined(DHCP)
	UNUSED(stop);
#endif
}

static void
dhcpcd_drop(struct interface *ifp, int stop)
{

	dhcpcd_drop_af(ifp, stop, AF_UNSPEC);
}

static void
stop_interface(struct interface *ifp, const char *reason)
{
	struct dhcpcd_ctx *ctx;

	ctx = ifp->ctx;
	loginfox("%s: removing interface", ifp->name);
	ifp->options->options |= DHCPCD_STOPPING;

	dhcpcd_drop(ifp, 1);
	script_runreason(ifp, reason == NULL ? "STOPPED" : reason);

	/* Delete all timeouts for the interfaces */
	eloop_q_timeout_delete(ctx->eloop, ELOOP_QUEUE_ALL, NULL, ifp);

	/* De-activate the interface */
	ifp->active = IF_INACTIVE;
	ifp->options->options &= ~DHCPCD_STOPPING;

	if (!(ctx->options & (DHCPCD_MANAGER | DHCPCD_TEST)))
		eloop_exit(ctx->eloop, EXIT_FAILURE);
}

static void
configure_interface1(struct interface *ifp)
{
	struct if_options *ifo = ifp->options;

	/* Do any platform specific configuration */
	if_conf(ifp);

	/* If we want to release a lease, we can't really persist the
	 * address either. */
	if (ifo->options & DHCPCD_RELEASE)
		ifo->options &= ~DHCPCD_PERSISTENT;

	if (ifp->flags & (IFF_POINTOPOINT | IFF_LOOPBACK)) {
		ifo->options &= ~DHCPCD_ARP;
		if (!(ifp->flags & IFF_MULTICAST))
			ifo->options &= ~DHCPCD_IPV6RS;
		if (!(ifo->options & (DHCPCD_INFORM | DHCPCD_WANTDHCP)))
			ifo->options |= DHCPCD_STATIC;
	}

	if (ifo->metric != -1)
		ifp->metric = (unsigned int)ifo->metric;

#ifdef INET6
	/* We want to setup INET6 on the interface as soon as possible. */
	if (ifp->active == IF_ACTIVE_USER &&
	    ifo->options & DHCPCD_IPV6 &&
	    !(ifp->ctx->options & (DHCPCD_DUMPLEASE | DHCPCD_TEST)))
	{
		/* If not doing any DHCP, disable the RDNSS requirement. */
		if (!(ifo->options & (DHCPCD_DHCP | DHCPCD_DHCP6)))
			ifo->options &= ~DHCPCD_IPV6RA_REQRDNSS;
		if_setup_inet6(ifp);
	}
#endif

	if (!(ifo->options & DHCPCD_IAID)) {
		/*
		 * An IAID is for identifying a unqiue interface within
		 * the client. It is 4 bytes long. Working out a default
		 * value is problematic.
		 *
		 * Interface name and number are not stable
		 * between different OS's. Some OS's also cannot make
		 * up their mind what the interface should be called
		 * (yes, udev, I'm looking at you).
		 * Also, the name could be longer than 4 bytes.
		 * Also, with pluggable interfaces the name and index
		 * could easily get swapped per actual interface.
		 *
		 * The MAC address is 6 bytes long, the final 3
		 * being unique to the manufacturer and the initial 3
		 * being unique to the organisation which makes it.
		 * We could use the last 4 bytes of the MAC address
		 * as the IAID as it's the most stable part given the
		 * above, but equally it's not guaranteed to be
		 * unique.
		 *
		 * Given the above, and our need to reliably work
		 * between reboots without persitent storage,
		 * generating the IAID from the MAC address is the only
		 * logical default.
		 * Saying that, if a VLANID has been specified then we
		 * can use that. It's possible that different interfaces
		 * can have the same VLANID, but this is no worse than
		 * generating the IAID from the duplicate MAC address.
		 *
		 * dhclient uses the last 4 bytes of the MAC address.
		 * dibbler uses an increamenting counter.
		 * wide-dhcpv6 uses 0 or a configured value.
		 * odhcp6c uses 1.
		 * Windows 7 uses the first 3 bytes of the MAC address
		 * and an unknown byte.
		 * dhcpcd-6.1.0 and earlier used the interface name,
		 * falling back to interface index if name > 4.
		 */
		if (ifp->vlanid != 0) {
			uint32_t vlanid;

			/* Maximal VLANID is 4095, so prefix with 0xff
			 * so we don't conflict with an interface index. */
			vlanid = htonl(ifp->vlanid | 0xff000000);
			memcpy(ifo->iaid, &vlanid, sizeof(vlanid));
		} else if (ifo->options & DHCPCD_ANONYMOUS)
			memset(ifo->iaid, 0, sizeof(ifo->iaid));
		else if (ifp->hwlen >= sizeof(ifo->iaid)) {
			memcpy(ifo->iaid,
			    ifp->hwaddr + ifp->hwlen - sizeof(ifo->iaid),
			    sizeof(ifo->iaid));
		} else {
			uint32_t len;

			len = (uint32_t)strlen(ifp->name);
			if (len <= sizeof(ifo->iaid)) {
				memcpy(ifo->iaid, ifp->name, len);
				if (len < sizeof(ifo->iaid))
					memset(ifo->iaid + len, 0,
					    sizeof(ifo->iaid) - len);
			} else {
				/* IAID is the same size as a uint32_t */
				len = htonl(ifp->index);
				memcpy(ifo->iaid, &len, sizeof(ifo->iaid));
			}
		}
		ifo->options |= DHCPCD_IAID;
	}

#ifdef DHCP6
	if (ifo->ia_len == 0 && ifo->options & DHCPCD_IPV6 &&
	    ifp->name[0] != '\0')
	{
		ifo->ia = malloc(sizeof(*ifo->ia));
		if (ifo->ia == NULL)
			logerr(__func__);
		else {
			ifo->ia_len = 1;
			ifo->ia->ia_type = D6_OPTION_IA_NA;
			memcpy(ifo->ia->iaid, ifo->iaid, sizeof(ifo->iaid));
			memset(&ifo->ia->addr, 0, sizeof(ifo->ia->addr));
#ifndef SMALL
			ifo->ia->sla = NULL;
			ifo->ia->sla_len = 0;
#endif
		}
	} else {
		size_t i;

		for (i = 0; i < ifo->ia_len; i++) {
			if (!ifo->ia[i].iaid_set) {
				memcpy(&ifo->ia[i].iaid, ifo->iaid,
				    sizeof(ifo->ia[i].iaid));
				ifo->ia[i].iaid_set = 1;
			}
		}
	}
#endif

	/* If root is network mounted, we don't want to kill the connection
	 * if the DHCP server goes the way of the dodo OR dhcpcd is rebooting
	 * and the lease file has expired. */
	if (is_root_local() == 0)
		ifo->options |= DHCPCD_LASTLEASE_EXTEND;
}

int
dhcpcd_selectprofile(struct interface *ifp, const char *profile)
{
	struct if_options *ifo;
	char pssid[PROFILE_LEN];

	if (ifp->ssid_len) {
		ssize_t r;

		r = print_string(pssid, sizeof(pssid), OT_ESCSTRING,
		    ifp->ssid, ifp->ssid_len);
		if (r == -1) {
			logerr(__func__);
			pssid[0] = '\0';
		}
	} else
		pssid[0] = '\0';
	ifo = read_config(ifp->ctx, ifp->name, pssid, profile);
	if (ifo == NULL) {
		logdebugx("%s: no profile %s", ifp->name, profile);
		return -1;
	}
	if (profile != NULL) {
		strlcpy(ifp->profile, profile, sizeof(ifp->profile));
		loginfox("%s: selected profile %s", ifp->name, profile);
	} else
		*ifp->profile = '\0';

	free_options(ifp->ctx, ifp->options);
	ifp->options = ifo;
	if (profile) {
		add_options(ifp->ctx, ifp->name, ifp->options,
		    ifp->ctx->argc, ifp->ctx->argv);
		configure_interface1(ifp);
	}
	return 1;
}

static void
configure_interface(struct interface *ifp, int argc, char **argv,
    unsigned long long options)
{
	time_t old;

	old = ifp->options ? ifp->options->mtime : 0;
	dhcpcd_selectprofile(ifp, NULL);
	if (ifp->options == NULL) {
		/* dhcpcd cannot continue with this interface. */
		ifp->active = IF_INACTIVE;
		return;
	}
	add_options(ifp->ctx, ifp->name, ifp->options, argc, argv);
	ifp->options->options |= options;
	configure_interface1(ifp);

	/* If the mtime has changed drop any old lease */
	if (old != 0 && ifp->options->mtime != old) {
		logwarnx("%s: config file changed, expiring leases",
		    ifp->name);
		dhcpcd_drop(ifp, 0);
	}
}

static void
dhcpcd_initstate1(struct interface *ifp, int argc, char **argv,
    unsigned long long options)
{
	struct if_options *ifo;

	configure_interface(ifp, argc, argv, options);
	if (!ifp->active)
		return;

	ifo = ifp->options;
	ifo->options |= options;

#ifdef INET6
	if (ifo->options & DHCPCD_IPV6 && ipv6_init(ifp->ctx) == -1) {
		logerr(__func__);
		ifo->options &= ~DHCPCD_IPV6;
	}
#endif
}

static void
dhcpcd_initstate(struct interface *ifp, unsigned long long options)
{

	dhcpcd_initstate1(ifp, ifp->ctx->argc, ifp->ctx->argv, options);
}

static void
dhcpcd_reportssid(struct interface *ifp)
{
	char pssid[IF_SSIDLEN * 4];

	if (print_string(pssid, sizeof(pssid), OT_ESCSTRING,
	    ifp->ssid, ifp->ssid_len) == -1)
	{
		logerr(__func__);
		return;
	}

	loginfox("%s: connected to Access Point: %s", ifp->name, pssid);
}

static void
dhcpcd_nocarrier_roaming(struct interface *ifp)
{

	loginfox("%s: carrier lost - roaming", ifp->name);

#ifdef ARP
	arp_drop(ifp);
#endif
#ifdef INET
	dhcp_abort(ifp);
#endif
#ifdef DHCP6
	dhcp6_abort(ifp);
#endif

	rt_build(ifp->ctx, AF_UNSPEC);
	script_runreason(ifp, "NOCARRIER_ROAMING");
}

void
dhcpcd_handlecarrier(struct interface *ifp, int carrier, unsigned int flags)
{
	bool was_link_up = if_is_link_up(ifp);
	bool was_roaming = if_roaming(ifp);

	ifp->carrier = carrier;
	ifp->flags = flags;

	if (!if_is_link_up(ifp)) {
		if (!ifp->active || (!was_link_up && !was_roaming))
			return;

		/*
		 * If the interface is roaming (generally on wireless)
		 * then while we are not up, we are not down either.
		 * Preserve the network state until we either disconnect
		 * or re-connect.
		 */
		if (!ifp->options->randomise_hwaddr && if_roaming(ifp)) {
			dhcpcd_nocarrier_roaming(ifp);
			return;
		}

		loginfox("%s: carrier lost", ifp->name);
		script_runreason(ifp, "NOCARRIER");
		dhcpcd_drop(ifp, 0);

		if (ifp->options->randomise_hwaddr) {
			bool is_up = ifp->flags & IFF_UP;

			if (is_up)
				if_down(ifp);
			if (if_randomisemac(ifp) == -1 && errno != ENXIO)
				logerr(__func__);
			if (is_up)
				if_up(ifp);
		}

		return;
	}

	/*
	 * At this point carrier is NOT DOWN and we have IFF_UP.
	 * We should treat LINK_UNKNOWN as up as the driver may not support
	 * link state changes.
	 * The consideration of any other information about carrier should
	 * be handled in the OS specific if_carrier() function.
	 */
	if (was_link_up)
		return;

	if (ifp->active) {
		if (carrier == LINK_UNKNOWN)
			loginfox("%s: carrier unknown, assuming up", ifp->name);
		else
			loginfox("%s: carrier acquired", ifp->name);
	}

#if !defined(__linux__) && !defined(__NetBSD__)
	/* BSD does not emit RTM_NEWADDR or RTM_CHGADDR when the
	 * hardware address changes so we have to go
	 * through the disovery process to work it out. */
	dhcpcd_handleinterface(ifp->ctx, 0, ifp->name);
#endif

	if (ifp->wireless) {
		uint8_t ossid[IF_SSIDLEN];
		size_t olen;

		olen = ifp->ssid_len;
		memcpy(ossid, ifp->ssid, ifp->ssid_len);
		if_getssid(ifp);

		/* If we changed SSID network, drop leases */
		if ((ifp->ssid_len != olen ||
		    memcmp(ifp->ssid, ossid, ifp->ssid_len)) && ifp->active)
		{
			dhcpcd_reportssid(ifp);
			dhcpcd_drop(ifp, 0);
#ifdef IPV4LL
			ipv4ll_reset(ifp);
#endif
		}
	}

	if (!ifp->active)
		return;

	dhcpcd_initstate(ifp, 0);
	script_runreason(ifp, "CARRIER");

#ifdef INET6
	/* Set any IPv6 Routers we remembered to expire faster than they
	 * would normally as we maybe on a new network. */
	ipv6nd_startexpire(ifp);
#ifdef IPV6_MANAGETEMPADDR
	/* RFC4941 Section 3.5 */
	ipv6_regentempaddrs(ifp);
#endif
#endif

	dhcpcd_startinterface(ifp);
}

static void
warn_iaid_conflict(struct interface *ifp, uint16_t ia_type, uint8_t *iaid)
{
	struct interface *ifn;
#ifdef INET6
	size_t i;
	struct if_ia *ia;
#endif

	TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
		if (ifn == ifp || !ifn->active)
			continue;
		if (ifn->options->options & DHCPCD_ANONYMOUS)
			continue;
		if (ia_type == 0 &&
		    memcmp(ifn->options->iaid, iaid,
		    sizeof(ifn->options->iaid)) == 0)
			break;
#ifdef INET6
		for (i = 0; i < ifn->options->ia_len; i++) {
			ia = &ifn->options->ia[i];
			if (ia->ia_type == ia_type &&
			    memcmp(ia->iaid, iaid, sizeof(ia->iaid)) == 0)
				break;
		}
#endif
	}

	/* This is only a problem if the interfaces are on the same network. */
	if (ifn)
		logerrx("%s: IAID conflicts with one assigned to %s",
		    ifp->name, ifn->name);
}

static void
dhcpcd_initduid(struct dhcpcd_ctx *ctx, struct interface *ifp)
{
	char buf[DUID_LEN * 3];

	if (ctx->duid != NULL) {
		if (ifp == NULL)
			goto log;
		return;
	}

	duid_init(ctx, ifp);
	if (ctx->duid == NULL)
		return;

log:
	loginfox("DUID %s",
	    hwaddr_ntoa(ctx->duid, ctx->duid_len, buf, sizeof(buf)));
}

void
dhcpcd_startinterface(void *arg)
{
	struct interface *ifp = arg;
	struct if_options *ifo = ifp->options;

	if (ifo->options & DHCPCD_LINK && !if_is_link_up(ifp)) {
		loginfox("%s: waiting for carrier", ifp->name);
		return;
	}

	if (ifo->options & (DHCPCD_DUID | DHCPCD_IPV6) &&
	    !(ifo->options & DHCPCD_ANONYMOUS))
	{
		char buf[sizeof(ifo->iaid) * 3];
#ifdef INET6
		size_t i;
		struct if_ia *ia;
#endif

		/* Try and init DUID from the interface hardware address */
		dhcpcd_initduid(ifp->ctx, ifp);

		/* Report IAIDs */
		loginfox("%s: IAID %s", ifp->name,
		    hwaddr_ntoa(ifo->iaid, sizeof(ifo->iaid),
		    buf, sizeof(buf)));
		warn_iaid_conflict(ifp, 0, ifo->iaid);

#ifdef INET6
		for (i = 0; i < ifo->ia_len; i++) {
			ia = &ifo->ia[i];
			if (memcmp(ifo->iaid, ia->iaid, sizeof(ifo->iaid))) {
				loginfox("%s: IA type %u IAID %s",
				    ifp->name, ia->ia_type,
				    hwaddr_ntoa(ia->iaid, sizeof(ia->iaid),
				    buf, sizeof(buf)));
				warn_iaid_conflict(ifp, ia->ia_type, ia->iaid);
			}
		}
#endif
	}

#ifdef INET6
	if (ifo->options & DHCPCD_IPV6 && ipv6_start(ifp) == -1) {
		logerr("%s: ipv6_start", ifp->name);
		ifo->options &= ~DHCPCD_IPV6;
	}

	if (ifo->options & DHCPCD_IPV6) {
		if (ifp->active == IF_ACTIVE_USER) {
			ipv6_startstatic(ifp);

			if (ifo->options & DHCPCD_IPV6RS)
				ipv6nd_startrs(ifp);
		}

#ifdef DHCP6
		/* DHCPv6 could be turned off, but the interface
		 * is still delegated to. */
		if (ifp->active)
			dhcp6_find_delegates(ifp);

		if (ifo->options & DHCPCD_DHCP6) {
			if (ifp->active == IF_ACTIVE_USER) {
				enum DH6S d6_state;

				if (ifo->options & DHCPCD_IA_FORCED)
					d6_state = DH6S_INIT;
				else if (ifo->options & DHCPCD_INFORM6)
					d6_state = DH6S_INFORM;
				else
					d6_state = DH6S_CONFIRM;
				if (dhcp6_start(ifp, d6_state) == -1)
					logerr("%s: dhcp6_start", ifp->name);
			}
		}
#endif
	}
#endif

#ifdef INET
	if (ifo->options & DHCPCD_IPV4 && ifp->active == IF_ACTIVE_USER) {
		/* Ensure we have an IPv4 state before starting DHCP */
		if (ipv4_getstate(ifp) != NULL)
			dhcp_start(ifp);
	}
#endif
}

static void
dhcpcd_prestartinterface(void *arg)
{
	struct interface *ifp = arg;
	struct dhcpcd_ctx *ctx = ifp->ctx;
	bool randmac_down;

	if (ifp->carrier <= LINK_DOWN &&
	    ifp->options->randomise_hwaddr &&
	    ifp->flags & IFF_UP)
	{
		if_down(ifp);
		randmac_down = true;
	} else
		randmac_down = false;

	if ((!(ctx->options & DHCPCD_MANAGER) ||
	    ifp->options->options & DHCPCD_IF_UP || randmac_down) &&
	    !(ifp->flags & IFF_UP))
	{
		if (ifp->options->randomise_hwaddr &&
		    if_randomisemac(ifp) == -1)
			logerr(__func__);
		if (if_up(ifp) == -1)
			logerr(__func__);
	}

	dhcpcd_startinterface(ifp);
}

static void
run_preinit(struct interface *ifp)
{

	if (ifp->ctx->options & DHCPCD_TEST)
		return;

	script_runreason(ifp, "PREINIT");
	if (ifp->wireless && if_is_link_up(ifp))
		dhcpcd_reportssid(ifp);
	if (ifp->options->options & DHCPCD_LINK && ifp->carrier != LINK_UNKNOWN)
		script_runreason(ifp,
		    ifp->carrier == LINK_UP ? "CARRIER" : "NOCARRIER");
}

void
dhcpcd_activateinterface(struct interface *ifp, unsigned long long options)
{

	if (ifp->active)
		return;

	/* IF_ACTIVE_USER will start protocols when the interface is started.
	 * IF_ACTIVE will ask the protocols for setup,
	 * such as any delegated prefixes. */
	ifp->active = IF_ACTIVE;
	dhcpcd_initstate(ifp, options);

	/* It's possible we might not have been able to load
	 * a config. */
	if (!ifp->active)
		return;

	run_preinit(ifp);
	dhcpcd_prestartinterface(ifp);
}

int
dhcpcd_handleinterface(void *arg, int action, const char *ifname)
{
	struct dhcpcd_ctx *ctx = arg;
	struct ifaddrs *ifaddrs;
	struct if_head *ifs;
	struct interface *ifp, *iff;
	const char * const argv[] = { ifname };
	int e;

	if (action == -1) {
		ifp = if_find(ctx->ifaces, ifname);
		if (ifp == NULL) {
			errno = ESRCH;
			return -1;
		}
		if (ifp->active) {
			logdebugx("%s: interface departed", ifp->name);
			stop_interface(ifp, "DEPARTED");
		}
		TAILQ_REMOVE(ctx->ifaces, ifp, next);
		if_free(ifp);
		return 0;
	}

	ifs = if_discover(ctx, &ifaddrs, -1, UNCONST(argv));
	if (ifs == NULL) {
		logerr(__func__);
		return -1;
	}

	ifp = if_find(ifs, ifname);
	if (ifp == NULL) {
		/* This can happen if an interface is quickly added
		 * and then removed. */
		errno = ENOENT;
		e = -1;
		goto out;
	}
	e = 1;

	/* Check if we already have the interface */
	iff = if_find(ctx->ifaces, ifp->name);

	if (iff != NULL) {
		if (iff->active)
			logdebugx("%s: interface updated", iff->name);
		/* The flags and hwaddr could have changed */
		iff->flags = ifp->flags;
		iff->hwlen = ifp->hwlen;
		if (ifp->hwlen != 0)
			memcpy(iff->hwaddr, ifp->hwaddr, iff->hwlen);
	} else {
		TAILQ_REMOVE(ifs, ifp, next);
		TAILQ_INSERT_TAIL(ctx->ifaces, ifp, next);
		if (ifp->active) {
			logdebugx("%s: interface added", ifp->name);
			dhcpcd_initstate(ifp, 0);
			run_preinit(ifp);
		}
		iff = ifp;
	}

	if (action > 0) {
		if_learnaddrs(ctx, ifs, &ifaddrs);
		if (iff->active)
			dhcpcd_prestartinterface(iff);
	}

out:
	/* Free our discovered list */
	while ((ifp = TAILQ_FIRST(ifs))) {
		TAILQ_REMOVE(ifs, ifp, next);
		if_free(ifp);
	}
	free(ifs);
	if_freeifaddrs(ctx, &ifaddrs);

	return e;
}

static void
dhcpcd_handlelink(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	if (if_handlelink(ctx) == -1) {
		if (errno == ENOBUFS || errno == ENOMEM) {
			dhcpcd_linkoverflow(ctx);
			return;
		}
		if (errno != ENOTSUP)
			logerr(__func__);
	}
}

static void
dhcpcd_checkcarrier(void *arg)
{
	struct interface *ifp0 = arg, *ifp;

	ifp = if_find(ifp0->ctx->ifaces, ifp0->name);
	if (ifp == NULL || ifp->carrier == ifp0->carrier)
		return;

	dhcpcd_handlecarrier(ifp, ifp0->carrier, ifp0->flags);
	if_free(ifp0);
}

#ifndef SMALL
static void
dhcpcd_setlinkrcvbuf(struct dhcpcd_ctx *ctx)
{
	socklen_t socklen;

	if (ctx->link_rcvbuf == 0)
		return;

	logdebugx("setting route socket receive buffer size to %d bytes",
	    ctx->link_rcvbuf);

	socklen = sizeof(ctx->link_rcvbuf);
	if (setsockopt(ctx->link_fd, SOL_SOCKET,
	    SO_RCVBUF, &ctx->link_rcvbuf, socklen) == -1)
		logerr(__func__);
}
#endif

static void
dhcpcd_runprestartinterface(void *arg)
{
	struct interface *ifp = arg;

	run_preinit(ifp);
	dhcpcd_prestartinterface(ifp);
}

void
dhcpcd_linkoverflow(struct dhcpcd_ctx *ctx)
{
	socklen_t socklen;
	int rcvbuflen;
	char buf[2048];
	ssize_t rlen;
	size_t rcnt;
	struct if_head *ifaces;
	struct ifaddrs *ifaddrs;
	struct interface *ifp, *ifn, *ifp1;

	socklen = sizeof(rcvbuflen);
	if (getsockopt(ctx->link_fd, SOL_SOCKET,
	    SO_RCVBUF, &rcvbuflen, &socklen) == -1) {
		logerr("%s: getsockopt", __func__);
		rcvbuflen = 0;
	}
#ifdef __linux__
	else
		rcvbuflen /= 2;
#endif

	logerrx("route socket overflowed (rcvbuflen %d)"
	    " - learning interface state", rcvbuflen);

	/* Drain the socket.
	 * We cannot open a new one due to privsep. */
	rcnt = 0;
	do {
		rlen = read(ctx->link_fd, buf, sizeof(buf));
		if (++rcnt % 1000 == 0)
			logwarnx("drained %zu messages", rcnt);
	} while (rlen != -1 || errno == ENOBUFS || errno == ENOMEM);
	if (rcnt % 1000 != 0)
		logwarnx("drained %zu messages", rcnt);

	/* Work out the current interfaces. */
	ifaces = if_discover(ctx, &ifaddrs, ctx->ifc, ctx->ifv);
	if (ifaces == NULL) {
		logerr(__func__);
		return;
	}

	/* Punt departed interfaces */
	TAILQ_FOREACH_SAFE(ifp, ctx->ifaces, next, ifn) {
		if (if_find(ifaces, ifp->name) != NULL)
			continue;
		dhcpcd_handleinterface(ctx, -1, ifp->name);
	}

	/* Add new interfaces */
	while ((ifp = TAILQ_FIRST(ifaces)) != NULL ) {
		TAILQ_REMOVE(ifaces, ifp, next);
		ifp1 = if_find(ctx->ifaces, ifp->name);
		if (ifp1 != NULL) {
			/* If the interface already exists,
			 * check carrier state.
			 * dhcpcd_checkcarrier will free ifp. */
			eloop_timeout_add_sec(ctx->eloop, 0,
			    dhcpcd_checkcarrier, ifp);
			continue;
		}
		TAILQ_INSERT_TAIL(ctx->ifaces, ifp, next);
		if (ifp->active) {
			dhcpcd_initstate(ifp, 0);
			eloop_timeout_add_sec(ctx->eloop, 0,
			    dhcpcd_runprestartinterface, ifp);
		}
	}
	free(ifaces);

	/* Update address state. */
	if_markaddrsstale(ctx->ifaces);
	if_learnaddrs(ctx, ctx->ifaces, &ifaddrs);
	if_deletestaleaddrs(ctx->ifaces);
	if_freeifaddrs(ctx, &ifaddrs);
}

void
dhcpcd_handlehwaddr(struct interface *ifp,
    uint16_t hwtype, const void *hwaddr, uint8_t hwlen)
{
	char buf[sizeof(ifp->hwaddr) * 3];

	if (hwaddr == NULL || !if_valid_hwaddr(hwaddr, hwlen))
		hwlen = 0;

	if (hwlen > sizeof(ifp->hwaddr)) {
		errno = ENOBUFS;
		logerr("%s: %s", __func__, ifp->name);
		return;
	}

	if (ifp->hwtype != hwtype) {
		if (ifp->active)
			loginfox("%s: hardware address type changed"
			    " from %d to %d", ifp->name, ifp->hwtype, hwtype);
		ifp->hwtype = hwtype;
	}

	if (ifp->hwlen == hwlen &&
	    (hwlen == 0 || memcmp(ifp->hwaddr, hwaddr, hwlen) == 0))
		return;

	if (ifp->active) {
		loginfox("%s: old hardware address: %s", ifp->name,
		    hwaddr_ntoa(ifp->hwaddr, ifp->hwlen, buf, sizeof(buf)));
		loginfox("%s: new hardware address: %s", ifp->name,
		    hwaddr_ntoa(hwaddr, hwlen, buf, sizeof(buf)));
	}
	ifp->hwlen = hwlen;
	if (hwaddr != NULL)
		memcpy(ifp->hwaddr, hwaddr, hwlen);
}

static void
if_reboot(struct interface *ifp, int argc, char **argv)
{
#ifdef INET
	unsigned long long oldopts;

	oldopts = ifp->options->options;
#endif
	script_runreason(ifp, "RECONFIGURE");
	dhcpcd_initstate1(ifp, argc, argv, 0);
#ifdef INET
	dhcp_reboot_newopts(ifp, oldopts);
#endif
#ifdef DHCP6
	dhcp6_reboot(ifp);
#endif
	dhcpcd_prestartinterface(ifp);
}

static void
reload_config(struct dhcpcd_ctx *ctx)
{
	struct if_options *ifo;

	free_globals(ctx);
	if ((ifo = read_config(ctx, NULL, NULL, NULL)) == NULL)
		return;
	add_options(ctx, NULL, ifo, ctx->argc, ctx->argv);
	/* We need to preserve these options. */
	if (ctx->options & DHCPCD_STARTED)
		ifo->options |= DHCPCD_STARTED;
	if (ctx->options & DHCPCD_MANAGER)
		ifo->options |= DHCPCD_MANAGER;
	if (ctx->options & DHCPCD_DAEMONISED)
		ifo->options |= DHCPCD_DAEMONISED;
	if (ctx->options & DHCPCD_PRIVSEP)
		ifo->options |= DHCPCD_PRIVSEP;
	ctx->options = ifo->options;
	free_options(ctx, ifo);
}

static void
reconf_reboot(struct dhcpcd_ctx *ctx, int action, int argc, char **argv, int oi)
{
	int i;
	struct interface *ifp;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		for (i = oi; i < argc; i++) {
			if (strcmp(ifp->name, argv[i]) == 0)
				break;
		}
		if (oi != argc && i == argc)
			continue;
		if (ifp->active == IF_ACTIVE_USER) {
			if (action)
				if_reboot(ifp, argc, argv);
#ifdef INET
			else
				ipv4_applyaddr(ifp);
#endif
		} else if (i != argc) {
			ifp->active = IF_ACTIVE_USER;
			dhcpcd_initstate1(ifp, argc, argv, 0);
			run_preinit(ifp);
			dhcpcd_prestartinterface(ifp);
		}
	}
}

static void
stop_all_interfaces(struct dhcpcd_ctx *ctx, unsigned long long opts)
{
	struct interface *ifp;

	ctx->options |= DHCPCD_EXITING;
	if (ctx->ifaces == NULL)
		return;

	/* Drop the last interface first */
	TAILQ_FOREACH_REVERSE(ifp, ctx->ifaces, if_head, next) {
		if (!ifp->active)
			continue;
		ifp->options->options |= opts;
		if (ifp->options->options & DHCPCD_RELEASE)
			ifp->options->options &= ~DHCPCD_PERSISTENT;
		ifp->options->options |= DHCPCD_EXITING;
		stop_interface(ifp, NULL);
	}
}

static void
dhcpcd_ifrenew(struct interface *ifp)
{

	if (!ifp->active)
		return;

	if (ifp->options->options & DHCPCD_LINK && !if_is_link_up(ifp))
		return;

#ifdef INET
	dhcp_renew(ifp);
#endif
#ifdef INET6
#define DHCPCD_RARENEW (DHCPCD_IPV6 | DHCPCD_IPV6RS)
	if ((ifp->options->options & DHCPCD_RARENEW) == DHCPCD_RARENEW)
		ipv6nd_startrs(ifp);
#endif
#ifdef DHCP6
	dhcp6_renew(ifp);
#endif
}

static void
dhcpcd_renew(struct dhcpcd_ctx *ctx)
{
	struct interface *ifp;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		dhcpcd_ifrenew(ifp);
	}
}

#ifdef USE_SIGNALS
#define sigmsg "received %s, %s"
static volatile bool dhcpcd_exiting = false;
void
dhcpcd_signal_cb(int sig, void *arg)
{
	struct dhcpcd_ctx *ctx = arg;
	unsigned long long opts;
	int exit_code;

	if (ctx->options & DHCPCD_DUMPLEASE) {
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}

	if (sig != SIGCHLD && ctx->options & DHCPCD_FORKED) {
		if (sig != SIGHUP &&
		    write(ctx->fork_fd, &sig, sizeof(sig)) == -1)
			logerr("%s: write", __func__);
		return;
	}

	opts = 0;
	exit_code = EXIT_FAILURE;
	switch (sig) {
	case SIGINT:
		loginfox(sigmsg, "SIGINT", "stopping");
		break;
	case SIGTERM:
		loginfox(sigmsg, "SIGTERM", "stopping");
		exit_code = EXIT_SUCCESS;
		break;
	case SIGALRM:
		loginfox(sigmsg, "SIGALRM", "releasing");
		opts |= DHCPCD_RELEASE;
		exit_code = EXIT_SUCCESS;
		break;
	case SIGHUP:
		loginfox(sigmsg, "SIGHUP", "rebinding");
		reload_config(ctx);
		/* Preserve any options passed on the commandline
		 * when we were started. */
		reconf_reboot(ctx, 1, ctx->argc, ctx->argv,
		    ctx->argc - ctx->ifc);
		return;
	case SIGUSR1:
		loginfox(sigmsg, "SIGUSR1", "renewing");
		dhcpcd_renew(ctx);
		return;
	case SIGUSR2:
		loginfox(sigmsg, "SIGUSR2", "reopening log");
#ifdef PRIVSEP
		if (IN_PRIVSEP(ctx)) {
			if (ps_root_logreopen(ctx) == -1)
				logerr("ps_root_logreopen");
			return;
		}
#endif
		if (logopen(ctx->logfile) == -1)
			logerr("logopen");
		return;
	case SIGCHLD:
#ifdef PRIVSEP
		ps_root_signalcb(sig, ctx);
#else
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
#endif
		return;
	default:
		logerrx("received signal %d but don't know what to do with it",
		    sig);
		return;
	}

	/*
	 * Privsep has a mini-eloop for reading data from other processes.
	 * This mini-eloop processes signals as well so we can reap children.
	 * During teardown we don't want to process SIGTERM or SIGINT again,
	 * as that could trigger memory issues.
	 */
	if (dhcpcd_exiting)
		return;

	dhcpcd_exiting = true;
	if (!(ctx->options & DHCPCD_TEST))
		stop_all_interfaces(ctx, opts);
	eloop_exit(ctx->eloop, exit_code);
	dhcpcd_exiting = false;
}
#endif

int
dhcpcd_handleargs(struct dhcpcd_ctx *ctx, struct fd_list *fd,
    int argc, char **argv)
{
	struct interface *ifp;
	struct if_options *ifo;
	unsigned long long opts, orig_opts;
	int opt, oi, oifind, do_reboot, do_renew, af = AF_UNSPEC;
	size_t len, l, nifaces;
	char *tmp, *p;

	/* Special commands for our control socket
	 * as the other end should be blocking until it gets the
	 * expected reply we should be safely able just to change the
	 * write callback on the fd */
	/* Make any change here in privsep-control.c as well. */
	if (strcmp(*argv, "--version") == 0) {
		return control_queue(fd, UNCONST(VERSION),
		    strlen(VERSION) + 1);
	} else if (strcmp(*argv, "--getconfigfile") == 0) {
		return control_queue(fd, UNCONST(fd->ctx->cffile),
		    strlen(fd->ctx->cffile) + 1);
	} else if (strcmp(*argv, "--getinterfaces") == 0) {
		oifind = argc = 0;
		goto dumplease;
	} else if (strcmp(*argv, "--listen") == 0) {
		fd->flags |= FD_LISTEN;
		return 0;
	}

	/* Log the command */
	len = 1;
	for (opt = 0; opt < argc; opt++)
		len += strlen(argv[opt]) + 1;
	tmp = malloc(len);
	if (tmp == NULL)
		return -1;
	p = tmp;
	for (opt = 0; opt < argc; opt++) {
		l = strlen(argv[opt]);
		strlcpy(p, argv[opt], len);
		len -= l + 1;
		p += l;
		*p++ = ' ';
	}
	*--p = '\0';
	loginfox("control command: %s", tmp);
	free(tmp);

	optind = 0;
	oi = 0;
	opts = 0;
	do_reboot = do_renew = 0;
	while ((opt = getopt_long(argc, argv, IF_OPTS, cf_options, &oi)) != -1)
	{
		switch (opt) {
		case 'g':
			/* Assumed if below not set */
			break;
		case 'k':
			opts |= DHCPCD_RELEASE;
			break;
		case 'n':
			do_reboot = 1;
			break;
		case 'p':
			opts |= DHCPCD_PERSISTENT;
			break;
		case 'x':
			opts |= DHCPCD_EXITING;
			break;
		case 'N':
			do_renew = 1;
			break;
		case 'U':
			opts |= DHCPCD_DUMPLEASE;
			break;
		case '4':
			af = AF_INET;
			break;
		case '6':
			af = AF_INET6;
			break;
		}
	}

	/* store the index; the optind will change when a getopt get called */
	oifind = optind;

	if (opts & DHCPCD_DUMPLEASE) {
		ctx->options |= DHCPCD_DUMPLEASE;
dumplease:
		nifaces = 0;
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (!ifp->active)
				continue;
			for (oi = oifind; oi < argc; oi++) {
				if (strcmp(ifp->name, argv[oi]) == 0)
					break;
			}
			if (oifind == argc || oi < argc) {
				opt = send_interface(NULL, ifp, af);
				if (opt == -1)
					goto dumperr;
				nifaces += (size_t)opt;
			}
		}
		if (write(fd->fd, &nifaces, sizeof(nifaces)) != sizeof(nifaces))
			goto dumperr;
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (!ifp->active)
				continue;
			for (oi = oifind; oi < argc; oi++) {
				if (strcmp(ifp->name, argv[oi]) == 0)
					break;
			}
			if (oifind == argc || oi < argc) {
				if (send_interface(fd, ifp, af) == -1)
					goto dumperr;
			}
		}
		ctx->options &= ~DHCPCD_DUMPLEASE;
		return 0;
dumperr:
		ctx->options &= ~DHCPCD_DUMPLEASE;
		return -1;
	}

	/* Only privileged users can control dhcpcd via the socket. */
	if (fd->flags & FD_UNPRIV) {
		errno = EPERM;
		return -1;
	}

	if (opts & (DHCPCD_EXITING | DHCPCD_RELEASE)) {
		if (oifind == argc && af == AF_UNSPEC) {
			stop_all_interfaces(ctx, opts);
			eloop_exit(ctx->eloop, EXIT_SUCCESS);
			return 0;
		}

		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (!ifp->active)
				continue;
			for (oi = oifind; oi < argc; oi++) {
				if (strcmp(ifp->name, argv[oi]) == 0)
					break;
			}
			if (oi == argc)
				continue;

			ifo = ifp->options;
			orig_opts = ifo->options;
			ifo->options |= opts;
			if (opts & DHCPCD_RELEASE)
				ifo->options &= ~DHCPCD_PERSISTENT;
			switch (af) {
			case AF_INET:
				ifo->options &= ~DHCPCD_IPV4;
				break;
			case AF_INET6:
				ifo->options &= ~DHCPCD_IPV6;
				break;
			}
			if (af != AF_UNSPEC)
				dhcpcd_drop_af(ifp, 1, af);
			else
				stop_interface(ifp, NULL);
			ifo->options = orig_opts;
		}
		return 0;
	}

	if (do_renew) {
		if (oifind == argc) {
			dhcpcd_renew(ctx);
			return 0;
		}
		for (oi = oifind; oi < argc; oi++) {
			if ((ifp = if_find(ctx->ifaces, argv[oi])) == NULL)
				continue;
			dhcpcd_ifrenew(ifp);
		}
		return 0;
	}

	reload_config(ctx);
	/* XXX: Respect initial commandline options? */
	reconf_reboot(ctx, do_reboot, argc, argv, oifind);
	return 0;
}

static void dhcpcd_readdump1(void *, unsigned short);

static void
dhcpcd_readdump2(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;
	ssize_t len;
	int exit_code = EXIT_FAILURE;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = read(ctx->control_fd, ctx->ctl_buf + ctx->ctl_bufpos,
	    ctx->ctl_buflen - ctx->ctl_bufpos);
	if (len == -1) {
		logerr(__func__);
		goto finished;
	} else if (len == 0)
		goto finished;
	if ((size_t)len + ctx->ctl_bufpos != ctx->ctl_buflen) {
		ctx->ctl_bufpos += (size_t)len;
		return;
	}

	if (ctx->ctl_buf[ctx->ctl_buflen - 1] != '\0') /* unlikely */
		ctx->ctl_buf[ctx->ctl_buflen - 1] = '\0';
	script_dump(ctx->ctl_buf, ctx->ctl_buflen);
	fflush(stdout);
	if (--ctx->ctl_extra != 0) {
		putchar('\n');
		if (eloop_event_add(ctx->eloop, ctx->control_fd, ELE_READ,
		    dhcpcd_readdump1, ctx) == -1)
			logerr("%s: eloop_event_add", __func__);
		return;
	}
	exit_code = EXIT_SUCCESS;

finished:
	shutdown(ctx->control_fd, SHUT_RDWR);
	eloop_exit(ctx->eloop, exit_code);
}

static void
dhcpcd_readdump1(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;
	ssize_t len;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = read(ctx->control_fd, &ctx->ctl_buflen, sizeof(ctx->ctl_buflen));
	if (len != sizeof(ctx->ctl_buflen)) {
		if (len != -1)
			errno = EINVAL;
		goto err;
	}
	if (ctx->ctl_buflen > SSIZE_MAX) {
		errno = ENOBUFS;
		goto err;
	}

	free(ctx->ctl_buf);
	ctx->ctl_buf = malloc(ctx->ctl_buflen);
	if (ctx->ctl_buf == NULL)
		goto err;

	ctx->ctl_bufpos = 0;
	if (eloop_event_add(ctx->eloop, ctx->control_fd, ELE_READ,
	    dhcpcd_readdump2, ctx) == -1)
		logerr("%s: eloop_event_add", __func__);
	return;

err:
	logerr(__func__);
	eloop_exit(ctx->eloop, EXIT_FAILURE);
}

static void
dhcpcd_readdump0(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;
	ssize_t len;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = read(ctx->control_fd, &ctx->ctl_extra, sizeof(ctx->ctl_extra));
	if (len != sizeof(ctx->ctl_extra)) {
		if (len != -1)
			errno = EINVAL;
		logerr(__func__);
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}

	if (ctx->ctl_extra == 0) {
		eloop_exit(ctx->eloop, EXIT_SUCCESS);
		return;
	}

	if (eloop_event_add(ctx->eloop, ctx->control_fd, ELE_READ,
	    dhcpcd_readdump1, ctx) == -1)
		logerr("%s: eloop_event_add", __func__);
}

static void
dhcpcd_readdumptimeout(void *arg)
{
	struct dhcpcd_ctx *ctx = arg;

	logerrx(__func__);
	eloop_exit(ctx->eloop, EXIT_FAILURE);
}

static int
dhcpcd_readdump(struct dhcpcd_ctx *ctx)
{

	ctx->options |=	DHCPCD_FORKED;
	if (eloop_timeout_add_sec(ctx->eloop, 5,
	    dhcpcd_readdumptimeout, ctx) == -1)
		return -1;
	return eloop_event_add(ctx->eloop, ctx->control_fd, ELE_READ,
	    dhcpcd_readdump0, ctx);
}

static void
dhcpcd_fork_cb(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;
	int exit_code;
	ssize_t len;

	if (!(events & ELE_READ))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = read(ctx->fork_fd, &exit_code, sizeof(exit_code));
	if (len == -1) {
		logerr(__func__);
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}
	if (len == 0) {
		if (ctx->options & DHCPCD_FORKED) {
			logerrx("%s: dhcpcd manager hungup", __func__);
			eloop_exit(ctx->eloop, EXIT_FAILURE);
		} else {
			// Launcher exited
			eloop_event_delete(ctx->eloop, ctx->fork_fd);
			close(ctx->fork_fd);
			ctx->fork_fd = -1;
		}
		return;
	}
	if ((size_t)len < sizeof(exit_code)) {
		logerrx("%s: truncated read %zd (expected %zu)",
		    __func__, len, sizeof(exit_code));
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}

	if (ctx->options & DHCPCD_FORKED) {
		if (exit_code == EXIT_SUCCESS)
			logdebugx("forked to background");
		eloop_exit(ctx->eloop, exit_code);
	} else
		dhcpcd_signal_cb(exit_code, ctx);
}

static void
dhcpcd_pidfile_timeout(void *arg)
{
	struct dhcpcd_ctx *ctx = arg;
	pid_t pid;

	pid = pidfile_read(ctx->pidfile);

	if(pid == -1)
		eloop_exit(ctx->eloop, EXIT_SUCCESS);
	else if (++ctx->duid_len >= 100) { /* overload duid_len */
		logerrx("pid %d failed to exit", pid);
		eloop_exit(ctx->eloop, EXIT_FAILURE);
	} else
		eloop_timeout_add_msec(ctx->eloop, 100,
		    dhcpcd_pidfile_timeout, ctx);
}

static int dup_null(int fd)
{
	int fd_null = open(_PATH_DEVNULL, O_WRONLY);
	int err;

	if (fd_null == -1) {
		logwarn("open %s", _PATH_DEVNULL);
		return -1;
	}

	if ((err = dup2(fd_null, fd)) == -1)
		logwarn("dup2 %d", fd);
	close(fd_null);
	return err;
}

int
main(int argc, char **argv, char **envp)
{
	struct dhcpcd_ctx ctx;
	struct ifaddrs *ifaddrs = NULL;
	struct if_options *ifo;
	struct interface *ifp;
	sa_family_t family = AF_UNSPEC;
	int opt, oi = 0, i;
	unsigned int logopts, t;
	ssize_t len;
#if defined(USE_SIGNALS) || !defined(THERE_IS_NO_FORK)
	pid_t pid;
	int fork_fd[2];
#endif
#ifdef USE_SIGNALS
	int sig = 0;
	const char *siga = NULL;
	size_t si;
#endif

#ifdef SETPROCTITLE_H
	setproctitle_init(argc, argv, envp);
#else
	UNUSED(envp);
#endif

	/* Test for --help and --version */
	if (argc > 1) {
		if (strcmp(argv[1], "--help") == 0) {
			usage();
			return EXIT_SUCCESS;
		} else if (strcmp(argv[1], "--version") == 0) {
			printf(""PACKAGE" "VERSION"\n%s\n", dhcpcd_copyright);
			printf("Compiled in features:"
#ifdef INET
			" INET"
#endif
#ifdef ARP
			" ARP"
#endif
#ifdef ARPING
			" ARPing"
#endif
#ifdef IPV4LL
			" IPv4LL"
#endif
#ifdef INET6
			" INET6"
#endif
#ifdef DHCP6
			" DHCPv6"
#endif
#ifdef AUTH
			" AUTH"
#endif
#ifdef PRIVSEP
			" PRIVSEP"
#endif
			"\n");
			return EXIT_SUCCESS;
		}
	}

	memset(&ctx, 0, sizeof(ctx));
	closefrom(STDERR_FILENO + 1);

	ifo = NULL;
	ctx.cffile = CONFIG;
	ctx.script = UNCONST(dhcpcd_default_script);
	ctx.control_fd = ctx.control_unpriv_fd = ctx.link_fd = -1;
	ctx.pf_inet_fd = -1;
#ifdef PF_LINK
	ctx.pf_link_fd = -1;
#endif

	TAILQ_INIT(&ctx.control_fds);
#ifdef USE_SIGNALS
	ctx.fork_fd = -1;
#endif
#ifdef PLUGIN_DEV
	ctx.dev_fd = -1;
#endif
#ifdef INET
	ctx.udp_rfd = -1;
	ctx.udp_wfd = -1;
#endif
#if defined(INET6) && !defined(__sun)
	ctx.nd_fd = -1;
#endif
#ifdef DHCP6
	ctx.dhcp6_rfd = -1;
	ctx.dhcp6_wfd = -1;
#endif
#ifdef PRIVSEP
	ctx.ps_log_fd = ctx.ps_log_root_fd = -1;
	TAILQ_INIT(&ctx.ps_processes);
#endif

	logopts = LOGERR_LOG | LOGERR_LOG_DATE | LOGERR_LOG_PID;

	/* Ensure we have stdin, stdout and stderr file descriptors.
	 * This is important as we do run scripts which expect these. */
	if (fcntl(STDIN_FILENO,  F_GETFD) == -1)
		dup_null(STDIN_FILENO);
	if (fcntl(STDOUT_FILENO,  F_GETFD) == -1)
		dup_null(STDOUT_FILENO);
	if (fcntl(STDERR_FILENO,  F_GETFD) == -1)
		dup_null(STDERR_FILENO);
	else
		logopts |= LOGERR_ERR;

	i = 0;

	while ((opt = getopt_long(argc, argv,
	    ctx.options & DHCPCD_PRINT_PIDFILE ? NOERR_IF_OPTS : IF_OPTS,
	    cf_options, &oi)) != -1)
	{
		switch (opt) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'f':
			ctx.cffile = optarg;
			break;
		case 'j':
			free(ctx.logfile);
			ctx.logfile = strdup(optarg);
			break;
#ifdef USE_SIGNALS
		case 'k':
			sig = SIGALRM;
			siga = "ALRM";
			break;
		case 'n':
			sig = SIGHUP;
			siga = "HUP";
			break;
		case 'q':
			/* -qq disables console output entirely.
			 * This is important for systemd because it logs
			 * both console AND syslog to the same log
			 * resulting in untold confusion. */
			if (logopts & LOGERR_QUIET)
				logopts &= ~LOGERR_ERR;
			else
				logopts |= LOGERR_QUIET;
			break;
		case 'x':
			sig = SIGTERM;
			siga = "TERM";
			break;
		case 'N':
			sig = SIGUSR1;
			siga = "USR1";
			break;
#endif
		case 'P':
			ctx.options |= DHCPCD_PRINT_PIDFILE;
			logopts &= ~(LOGERR_LOG | LOGERR_ERR);
			break;
		case 'T':
			i = 1;
			logopts &= ~LOGERR_LOG;
			break;
		case 'U':
			i = 3;
			break;
		case 'V':
			i = 2;
			break;
		case '?':
			if (ctx.options & DHCPCD_PRINT_PIDFILE)
				continue;
			usage();
			goto exit_failure;
		}
	}

	if (optind != argc - 1)
		ctx.options |= DHCPCD_MANAGER;

	logsetopts(logopts);
	logopen(ctx.logfile);

	ctx.argv = argv;
	ctx.argc = argc;
	ctx.ifc = argc - optind;
	ctx.ifv = argv + optind;

	rt_init(&ctx);

	ifo = read_config(&ctx, NULL, NULL, NULL);
	if (ifo == NULL) {
		if (ctx.options & DHCPCD_PRINT_PIDFILE)
			goto printpidfile;
		goto exit_failure;
	}

	opt = add_options(&ctx, NULL, ifo, argc, argv);
	if (opt != 1) {
		if (ctx.options & DHCPCD_PRINT_PIDFILE)
			goto printpidfile;
		if (opt == 0)
			usage();
		goto exit_failure;
	}
	if (i == 2) {
		printf("Interface options:\n");
		if (optind == argc - 1) {
			free_options(&ctx, ifo);
			ifo = read_config(&ctx, argv[optind], NULL, NULL);
			if (ifo == NULL)
				goto exit_failure;
			add_options(&ctx, NULL, ifo, argc, argv);
		}
		if_printoptions();
#ifdef INET
		if (family == 0 || family == AF_INET) {
			printf("\nDHCPv4 options:\n");
			dhcp_printoptions(&ctx,
			    ifo->dhcp_override, ifo->dhcp_override_len);
		}
#endif
#ifdef INET6
		if (family == 0 || family == AF_INET6) {
			printf("\nND options:\n");
			ipv6nd_printoptions(&ctx,
			    ifo->nd_override, ifo->nd_override_len);
#ifdef DHCP6
			printf("\nDHCPv6 options:\n");
			dhcp6_printoptions(&ctx,
			    ifo->dhcp6_override, ifo->dhcp6_override_len);
#endif
		}
#endif
		goto exit_success;
	}
	ctx.options |= ifo->options;

	if (i == 1 || i == 3) {
		if (i == 1)
			ctx.options |= DHCPCD_TEST;
		else
			ctx.options |= DHCPCD_DUMPLEASE;
		ctx.options |= DHCPCD_PERSISTENT;
		ctx.options &= ~DHCPCD_DAEMONISE;
	}

#ifdef THERE_IS_NO_FORK
	ctx.options &= ~DHCPCD_DAEMONISE;
#endif

	if (ctx.options & DHCPCD_DEBUG)
		logsetopts(logopts | LOGERR_DEBUG);

	if (!(ctx.options & (DHCPCD_TEST | DHCPCD_DUMPLEASE))) {
printpidfile:
		/* If we have any other args, we should run as a single dhcpcd
		 *  instance for that interface. */
		if (optind == argc - 1 && !(ctx.options & DHCPCD_MANAGER)) {
			const char *per;
			const char *ifname;

			ifname = *ctx.ifv;
			if (ifname == NULL || strlen(ifname) > IF_NAMESIZE) {
				errno = ifname == NULL ? EINVAL : E2BIG;
				logerr("%s: ", ifname);
				goto exit_failure;
			}
			/* Allow a dhcpcd interface per address family */
			switch(family) {
			case AF_INET:
				per = "-4";
				break;
			case AF_INET6:
				per = "-6";
				break;
			default:
				per = "";
			}
			snprintf(ctx.pidfile, sizeof(ctx.pidfile),
			    PIDFILE, ifname, per, ".");
		} else {
			snprintf(ctx.pidfile, sizeof(ctx.pidfile),
			    PIDFILE, "", "", "");
			ctx.options |= DHCPCD_MANAGER;

			/*
			 * If we are given any interfaces or a family, we
			 * cannot send a signal as that would impact
			 * other interfaces.
			 */
			if (optind != argc || family != AF_UNSPEC)
				sig = 0;
		}
		if (ctx.options & DHCPCD_PRINT_PIDFILE) {
			printf("%s\n", ctx.pidfile);
			goto exit_success;
		}
	}

	if (chdir("/") == -1)
		logerr("%s: chdir: /", __func__);

	/* Freeing allocated addresses from dumping leases can trigger
	 * eloop removals as well, so init here. */
	if ((ctx.eloop = eloop_new()) == NULL) {
		logerr("%s: eloop_init", __func__);
		goto exit_failure;
	}

#ifdef USE_SIGNALS
	for (si = 0; si < dhcpcd_signals_ignore_len; si++)
		signal(dhcpcd_signals_ignore[si], SIG_IGN);

	/* Save signal mask, block and redirect signals to our handler */
	eloop_signal_set_cb(ctx.eloop,
	    dhcpcd_signals, dhcpcd_signals_len,
	    dhcpcd_signal_cb, &ctx);
	if (eloop_signal_mask(ctx.eloop, &ctx.sigset) == -1) {
		logerr("%s: eloop_signal_mask", __func__);
		goto exit_failure;
	}

	if (sig != 0) {
		pid = pidfile_read(ctx.pidfile);
		if (pid != 0 && pid != -1)
			loginfox("sending signal %s to pid %d", siga, pid);
		if (pid == 0 || pid == -1 || kill(pid, sig) != 0) {
			if (pid != 0 && pid != -1 && errno != ESRCH) {
				logerr("kill");
				goto exit_failure;
			}
			unlink(ctx.pidfile);
			/* We can still continue and send the command
			 * via the control socket. */
		} else {
			if (sig == SIGHUP || sig == SIGUSR1)
				goto exit_success;
			/* Spin until it exits */
			loginfox("waiting for pid %d to exit", pid);
			dhcpcd_pidfile_timeout(&ctx);
			goto run_loop;
		}
	}
#endif

#ifdef HAVE_OPENSSL
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS |
	    OPENSSL_INIT_ADD_ALL_DIGESTS | OPENSSL_INIT_LOAD_CONFIG, NULL);
#endif

#ifdef PRIVSEP
	ps_init(&ctx);
#endif

#ifndef SMALL
	if (ctx.options & DHCPCD_DUMPLEASE &&
	    ioctl(fileno(stdin), FIONREAD, &i, sizeof(i)) == 0 &&
	    i > 0)
	{
		ctx.options |= DHCPCD_FORKED; /* pretend child process */
#ifdef PRIVSEP
		if (IN_PRIVSEP(&ctx) && ps_managersandbox(&ctx, NULL) == -1)
			goto exit_failure;
#endif
		ifp = calloc(1, sizeof(*ifp));
		if (ifp == NULL) {
			logerr(__func__);
			goto exit_failure;
		}
		ifp->ctx = &ctx;
		ifp->options = ifo;
		switch (family) {
		case AF_INET:
#ifdef INET
			if (dhcp_dump(ifp) == -1)
				goto exit_failure;
			break;
#else
			logerrx("No DHCP support");
			goto exit_failure;
#endif
		case AF_INET6:
#ifdef DHCP6
			if (dhcp6_dump(ifp) == -1)
				goto exit_failure;
			break;
#else
			logerrx("No DHCP6 support");
			goto exit_failure;
#endif
		default:
			logerrx("Family not specified. Please use -4 or -6.");
			goto exit_failure;
		}
		goto exit_success;
	}
#endif

	/* Try and contact the manager process to send the instruction. */
	if (!(ctx.options & DHCPCD_TEST)) {
		ctx.options |= DHCPCD_FORKED; /* avoid socket unlink */
		if (!(ctx.options & DHCPCD_MANAGER))
			ctx.control_fd = control_open(argv[optind], family,
			    ctx.options & DHCPCD_DUMPLEASE);
		if (!(ctx.options & DHCPCD_MANAGER) && ctx.control_fd == -1)
			ctx.control_fd = control_open(argv[optind], AF_UNSPEC,
			    ctx.options & DHCPCD_DUMPLEASE);
		if (ctx.control_fd == -1)
			ctx.control_fd = control_open(NULL, AF_UNSPEC,
			    ctx.options & DHCPCD_DUMPLEASE);
		if (ctx.control_fd != -1) {
#ifdef PRIVSEP
			if (IN_PRIVSEP(&ctx) &&
			    ps_managersandbox(&ctx, NULL) == -1)
				goto exit_failure;
#endif
			if (!(ctx.options & DHCPCD_DUMPLEASE))
				loginfox("sending commands to dhcpcd process");
			len = control_send(&ctx, argc, argv);
			if (len > 0)
				logdebugx("send OK");
			else {
				logerr("%s: control_send", __func__);
				goto exit_failure;
			}
			if (ctx.options & DHCPCD_DUMPLEASE) {
				if (dhcpcd_readdump(&ctx) == -1) {
					logerr("%s: dhcpcd_readdump", __func__);
					goto exit_failure;
				}
				goto run_loop;
			}
			goto exit_success;
		} else {
			if (errno != ENOENT)
				logerr("%s: control_open", __func__);
			/* If asking dhcpcd to exit and we failed to
			 * send a signal or a message then we
			 * don't proceed past here. */
			if (ctx.options & DHCPCD_DUMPLEASE ||
			    sig == SIGTERM || sig == SIGALRM)
			{
				if (errno == ENOENT)
					logerrx(PACKAGE" is not running");
				goto exit_failure;
			}
			if (errno == EPERM || errno == EACCES)
				goto exit_failure;
		}
		ctx.options &= ~DHCPCD_FORKED;
	}

	if (!(ctx.options & DHCPCD_TEST)) {
		/* Ensure we have the needed directories */
		if (mkdir(DBDIR, 0750) == -1 && errno != EEXIST)
			logerr("%s: mkdir: %s", __func__, DBDIR);
		if (mkdir(RUNDIR, 0755) == -1 && errno != EEXIST)
			logerr("%s: mkdir: %s", __func__, RUNDIR);
		if ((pid = pidfile_lock(ctx.pidfile)) != 0) {
			if (pid == -1)
				logerr("%s: pidfile_lock: %s",
				    __func__, ctx.pidfile);
			else
				logerrx(PACKAGE
				    " already running on pid %d (%s)",
				    pid, ctx.pidfile);
			goto exit_failure;
		}
	}

	loginfox(PACKAGE "-" VERSION " starting");

	// We don't need stdin past this point
	dup_null(STDIN_FILENO);

#if defined(USE_SIGNALS) && !defined(THERE_IS_NO_FORK)
	if (!(ctx.options & DHCPCD_DAEMONISE))
		goto start_manager;

	if (xsocketpair(AF_UNIX, SOCK_SEQPACKET|SOCK_CXNB, 0, fork_fd) == -1) {
		logerr("socketpair");
		goto exit_failure;
	}
	switch (pid = fork()) {
	case -1:
		logerr("fork");
		goto exit_failure;
	case 0:
		ctx.fork_fd = fork_fd[1];
		close(fork_fd[0]);
#ifdef PRIVSEP_RIGHTS
		if (ps_rights_limit_fd(ctx.fork_fd) == -1) {
			logerr("ps_rights_limit_fdpair");
			goto exit_failure;
		}
#endif
		if (eloop_event_add(ctx.eloop, ctx.fork_fd, ELE_READ,
		    dhcpcd_fork_cb, &ctx) == -1)
			logerr("%s: eloop_event_add", __func__);

		if (setsid() == -1) {
			logerr("%s: setsid", __func__);
			goto exit_failure;
		}
		/* Ensure we can never get a controlling terminal */
		switch (pid = fork()) {
		case -1:
			logerr("fork");
			goto exit_failure;
		case 0:
			eloop_forked(ctx.eloop);
			break;
		default:
			ctx.options |= DHCPCD_FORKED; /* A lie */
			i = EXIT_SUCCESS;
			goto exit1;
		}
		break;
	default:
		setproctitle("[launcher]");
		ctx.options |= DHCPCD_FORKED | DHCPCD_LAUNCHER;
		ctx.fork_fd = fork_fd[0];
		close(fork_fd[1]);
#ifdef PRIVSEP_RIGHTS
		if (ps_rights_limit_fd(ctx.fork_fd) == -1) {
			logerr("ps_rights_limit_fd");
			goto exit_failure;
		}
#endif
		if (eloop_event_add(ctx.eloop, ctx.fork_fd, ELE_READ,
		    dhcpcd_fork_cb, &ctx) == -1)
			logerr("%s: eloop_event_add", __func__);

#ifdef PRIVSEP
		if (IN_PRIVSEP(&ctx) && ps_managersandbox(&ctx, NULL) == -1)
			goto exit_failure;
#endif
		goto run_loop;
	}

#ifdef DEBUG_FD
	loginfox("forkfd %d", ctx.fork_fd);
#endif

	/* We have now forked, setsid, forked once more.
	 * From this point on, we are the controlling daemon. */
	logdebugx("spawned manager process on PID %d", getpid());

start_manager:
	ctx.options |= DHCPCD_STARTED;
	if ((pid = pidfile_lock(ctx.pidfile)) != 0) {
		logerr("%s: pidfile_lock %d", __func__, pid);
#ifdef PRIVSEP
		/* privsep has not started ... */
		ctx.options &= ~DHCPCD_PRIVSEP;
#endif
		goto exit_failure;
	}
#endif

	os_init();

#if defined(BSD) && defined(INET6)
	/* Disable the kernel RTADV sysctl as early as possible. */
	if (ctx.options & DHCPCD_IPV6 && ctx.options & DHCPCD_IPV6RS)
		if_disable_rtadv();
#endif

#ifdef PRIVSEP
	if (IN_PRIVSEP(&ctx) && ps_start(&ctx) == -1) {
		logerr("ps_start");
		goto exit_failure;
	}
	if (ctx.options & DHCPCD_FORKED)
		goto run_loop;
#endif

	if (!(ctx.options & DHCPCD_TEST)) {
		if (control_start(&ctx,
		    ctx.options & DHCPCD_MANAGER ?
		    NULL : argv[optind], family) == -1)
		{
			logerr("%s: control_start", __func__);
			goto exit_failure;
		}
	}

#ifdef PLUGIN_DEV
	/* Start any dev listening plugin which may want to
	 * change the interface name provided by the kernel */
	if (!IN_PRIVSEP(&ctx) &&
	    (ctx.options & (DHCPCD_MANAGER | DHCPCD_DEV)) ==
	    (DHCPCD_MANAGER | DHCPCD_DEV))
		dev_start(&ctx, dhcpcd_handleinterface);
#endif

	setproctitle("%s%s%s",
	    ctx.options & DHCPCD_MANAGER ? "[manager]" : argv[optind],
	    ctx.options & DHCPCD_IPV4 ? " [ip4]" : "",
	    ctx.options & DHCPCD_IPV6 ? " [ip6]" : "");

	if (if_opensockets(&ctx) == -1) {
		logerr("%s: if_opensockets", __func__);
		goto exit_failure;
	}
#ifndef SMALL
	dhcpcd_setlinkrcvbuf(&ctx);
#endif

	/* Try and create DUID from the machine UUID. */
	dhcpcd_initduid(&ctx, NULL);

	/* Cache the default vendor option. */
	if (dhcp_vendor(ctx.vendor, sizeof(ctx.vendor)) == -1)
		logerr("dhcp_vendor");

	/* Start handling kernel messages for interfaces, addresses and
	 * routes. */
	if (eloop_event_add(ctx.eloop, ctx.link_fd, ELE_READ,
	    dhcpcd_handlelink, &ctx) == -1)
		logerr("%s: eloop_event_add", __func__);

#ifdef PRIVSEP
	if (IN_PRIVSEP(&ctx) && ps_managersandbox(&ctx, "stdio route") == -1)
		goto exit_failure;
#endif

	/* When running dhcpcd against a single interface, we need to retain
	 * the old behaviour of waiting for an IP address */
	if (ctx.ifc == 1 && !(ctx.options & DHCPCD_BACKGROUND))
		ctx.options |= DHCPCD_WAITIP;

	ctx.ifaces = if_discover(&ctx, &ifaddrs, ctx.ifc, ctx.ifv);
	if (ctx.ifaces == NULL) {
		logerr("%s: if_discover", __func__);
		goto exit_failure;
	}
	for (i = 0; i < ctx.ifc; i++) {
		if ((ifp = if_find(ctx.ifaces, ctx.ifv[i])) == NULL)
			logerrx("%s: interface not found",
			    ctx.ifv[i]);
		else if (!ifp->active)
			logerrx("%s: interface has an invalid configuration",
			    ctx.ifv[i]);
	}
	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		if (ifp->active == IF_ACTIVE_USER)
			break;
	}

	if (ifp == NULL) {
		if (ctx.ifc == 0) {
			int loglevel;

			loglevel = ctx.options & DHCPCD_INACTIVE ?
			    LOG_DEBUG : LOG_ERR;
			logmessage(loglevel, "no valid interfaces found");
			dhcpcd_daemonise(&ctx);
		} else
			goto exit_failure;
		if (!(ctx.options & DHCPCD_LINK)) {
			logerrx("aborting as link detection is disabled");
			goto exit_failure;
		}
	}

	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		if (ifp->active)
			dhcpcd_initstate1(ifp, argc, argv, 0);
	}
	if_learnaddrs(&ctx, ctx.ifaces, &ifaddrs);
	if_freeifaddrs(&ctx, &ifaddrs);
	ifaddrs = NULL;

	if (ctx.options & DHCPCD_BACKGROUND)
		dhcpcd_daemonise(&ctx);

	opt = 0;
	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		if (ifp->active) {
			run_preinit(ifp);
			if (if_is_link_up(ifp))
				opt = 1;
		}
	}

	if (!(ctx.options & DHCPCD_BACKGROUND)) {
		if (ctx.options & DHCPCD_MANAGER)
			t = ifo->timeout;
		else {
			t = 0;
			TAILQ_FOREACH(ifp, ctx.ifaces, next) {
				if (ifp->active) {
					t = ifp->options->timeout;
					break;
				}
			}
		}
		if (opt == 0 &&
		    ctx.options & DHCPCD_LINK &&
		    !(ctx.options & DHCPCD_WAITIP))
		{
			int loglevel;

			loglevel = ctx.options & DHCPCD_INACTIVE ?
			    LOG_DEBUG : LOG_WARNING;
			logmessage(loglevel, "no interfaces have a carrier");
			dhcpcd_daemonise(&ctx);
		} else if (t > 0 &&
		    /* Test mode removes the daemonise bit, so check for both */
		    ctx.options & (DHCPCD_DAEMONISE | DHCPCD_TEST))
		{
			eloop_timeout_add_sec(ctx.eloop, t,
			    handle_exit_timeout, &ctx);
		}
	}
	free_options(&ctx, ifo);
	ifo = NULL;

	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		if (ifp->active)
			eloop_timeout_add_sec(ctx.eloop, 0,
			    dhcpcd_prestartinterface, ifp);
	}

run_loop:
	i = eloop_start(ctx.eloop, &ctx.sigset);
	if (i < 0) {
		logerr("%s: eloop_start", __func__);
		goto exit_failure;
	}
	goto exit1;

exit_success:
	i = EXIT_SUCCESS;
	goto exit1;

exit_failure:
	i = EXIT_FAILURE;

exit1:
	if (!(ctx.options & DHCPCD_TEST) && control_stop(&ctx) == -1)
		logerr("%s: control_stop", __func__);
	if_freeifaddrs(&ctx, &ifaddrs);
#ifdef PRIVSEP
	ps_stop(&ctx);
#endif
	/* Free memory and close fd's */
	if (ctx.ifaces) {
		while ((ifp = TAILQ_FIRST(ctx.ifaces))) {
			TAILQ_REMOVE(ctx.ifaces, ifp, next);
			if_free(ifp);
		}
		free(ctx.ifaces);
		ctx.ifaces = NULL;
	}
	free_options(&ctx, ifo);
#ifdef HAVE_OPEN_MEMSTREAM
	if (ctx.script_fp)
		fclose(ctx.script_fp);
#endif
	free(ctx.script_buf);
	free(ctx.script_env);
	rt_dispose(&ctx);
	free(ctx.duid);
	if_closesockets(&ctx);
	free_globals(&ctx);
#ifdef INET6
	ipv6_ctxfree(&ctx);
#endif
#ifdef PLUGIN_DEV
	dev_stop(&ctx);
#endif
	if (ctx.script != dhcpcd_default_script)
		free(ctx.script);
#ifdef PRIVSEP
	if (ps_stopwait(&ctx) != EXIT_SUCCESS)
		i = EXIT_FAILURE;
#endif
	if (ctx.options & DHCPCD_STARTED && !(ctx.options & DHCPCD_FORKED))
		loginfox(PACKAGE " exited");
#ifdef PRIVSEP
	if (ps_root_stop(&ctx) == -1)
		i = EXIT_FAILURE;
	eloop_free(ctx.ps_eloop);
#endif

#ifdef USE_SIGNALS
	/* If still attached, detach from the launcher */
	if (ctx.options & DHCPCD_STARTED && ctx.fork_fd != -1) {
		if (write(ctx.fork_fd, &i, sizeof(i)) == -1)
			logerr("%s: write", __func__);
	}
#endif

	eloop_free(ctx.eloop);
	logclose();
	free(ctx.logfile);
	free(ctx.ctl_buf);
#ifdef SETPROCTITLE_H
	setproctitle_fini();
#endif

#ifdef USE_SIGNALS
	if (ctx.options & (DHCPCD_FORKED | DHCPCD_PRIVSEP))
		_exit(i); /* so atexit won't remove our pidfile */
#endif
	return i;
}
