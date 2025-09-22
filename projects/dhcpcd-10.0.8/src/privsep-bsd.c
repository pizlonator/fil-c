/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd, BSD driver
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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/sysctl.h>

/* Need these for filtering the ioctls */
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#ifdef __NetBSD__
#include <netinet/if_ether.h>
#include <net/if_vlanvar.h> /* Needs netinet/if_ether.h */
#elif defined(__DragonFly__)
#include <net/vlan/if_vlan_var.h>
#else
#include <net/if_vlan_var.h>
#endif
#ifdef __DragonFly__
#  include <netproto/802_11/ieee80211_ioctl.h>
#else
#  include <net80211/ieee80211.h>
#  include <net80211/ieee80211_ioctl.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpcd.h"
#include "if.h"
#include "logerr.h"
#include "privsep.h"

static ssize_t
ps_root_doioctldom(struct dhcpcd_ctx *ctx, int domain, unsigned long req, void *data, size_t len)
{
#if defined(INET6) || (defined(SIOCALIFADDR) && defined(IFLR_ACTIVE))
	struct priv *priv = (struct priv *)ctx->priv;
#endif
	int s;

	switch(domain) {
#ifdef INET
	case PF_INET:
		s = ctx->pf_inet_fd;
		break;
#endif
#ifdef INET6
	case PF_INET6:
		s = priv->pf_inet6_fd;
		break;
#endif
#if defined(SIOCALIFADDR) && defined(IFLR_ACTIVE) /*NetBSD */
	case PF_LINK:
		s = priv->pf_link_fd;
		break;
#endif
	default:
		errno = EPFNOSUPPORT;
		return -1;
	}

	/* Only allow these ioctls */
	switch(req) {
#ifdef SIOCGIFDATA
	case SIOCGIFDATA:	/* FALLTHROUGH */
#endif
#ifdef SIOCG80211NWID
	case SIOCG80211NWID:	/* FALLTHROUGH */
#endif
#ifdef SIOCGETVLAN
	case SIOCGETVLAN:	/* FALLTHROUGH */
#endif
#ifdef SIOCIFAFATTACH
	case SIOCIFAFATTACH:	/* FALLTHROUGH */
#endif
#ifdef SIOCSIFXFLAGS
	case SIOCSIFXFLAGS:	/* FALLTHROUGH */
#endif
#ifdef SIOCSIFINFO_FLAGS
	case SIOCSIFINFO_FLAGS:	/* FALLTHROUGH */
#endif
#ifdef SIOCSRTRFLUSH_IN6
	case SIOCSRTRFLUSH_IN6:	/* FALLTHROUGH */
	case SIOCSPFXFLUSH_IN6: /* FALLTHROUGH */
#endif
#if defined(SIOCALIFADDR) && defined(IFLR_ACTIVE)
	case SIOCALIFADDR:	/* FALLTHROUGH */
	case SIOCDLIFADDR:	/* FALLTHROUGH */
#else
	case SIOCSIFLLADDR:	/* FALLTHROUGH */
#endif
#ifdef SIOCSIFINFO_IN6
	case SIOCSIFINFO_IN6:	/* FALLTHROUGH */
#endif
	case SIOCAIFADDR_IN6:	/* FALLTHROUGH */
	case SIOCDIFADDR_IN6:
		break;
	default:
		errno = EPERM;
		return -1;
	}

	return ioctl(s, req, data, len);
}

static ssize_t
ps_root_doroute(struct dhcpcd_ctx *ctx, void *data, size_t len)
{

	return write(ctx->link_fd, data, len);
}

#if defined(HAVE_CAPSICUM) || defined(HAVE_PLEDGE)
static ssize_t
ps_root_doindirectioctl(struct dhcpcd_ctx *ctx,
    unsigned long req, void *data, size_t len)
{
	char *p = data;
	struct ifreq ifr = { .ifr_flags = 0 };

	/* ioctl filtering is done in ps_root_doioctldom */

	if (len < IFNAMSIZ + 1) {
		errno = EINVAL;
		return -1;
	}

	strlcpy(ifr.ifr_name, p, IFNAMSIZ);
	len -= IFNAMSIZ;
	memmove(data, p + IFNAMSIZ, len);
	ifr.ifr_data = data;

	return ps_root_doioctldom(ctx, PF_INET, req, &ifr, sizeof(ifr));
}
#endif

#ifdef HAVE_PLEDGE
static ssize_t
ps_root_doifignoregroup(struct dhcpcd_ctx *ctx, void *data, size_t len)
{

	if (len == 0 || ((const char *)data)[len - 1] != '\0') {
		errno = EINVAL;
		return -1;
	}

	return if_ignoregroup(ctx->pf_inet_fd, data);
}
#endif

#ifdef HAVE_CAPSICUM
static ssize_t
ps_root_dosysctl(unsigned long flags,
    void *data, size_t len, void **rdata, size_t *rlen)
{
	char *p = data, *e = p + len;
	int name[10];
	unsigned int namelen;
	void *oldp;
	size_t *oldlenp, oldlen, nlen;
	void *newp;
	size_t newlen;
	int err;

	if (sizeof(namelen) >= len) {
		errno = EINVAL;
		return -1;
	}
	memcpy(&namelen, p, sizeof(namelen));
	p += sizeof(namelen);
	nlen = sizeof(*name) * namelen;
	if (namelen > __arraycount(name)) {
		errno = ENOBUFS;
		return -1;
	}
	if (p + nlen > e) {
		errno = EINVAL;
		return -1;
	}
	memcpy(name, p, nlen);
	p += nlen;
	if (p + sizeof(oldlen) > e) {
		errno = EINVAL;
		return -1;
	}
	memcpy(&oldlen, p, sizeof(oldlen));
	p += sizeof(oldlen);
	if (p + sizeof(newlen) > e) {
		errno = EINVAL;
		return -1;
	}
	memcpy(&newlen, p, sizeof(newlen));
	p += sizeof(newlen);
	if (p + newlen > e) {
		errno = EINVAL;
		return -1;
	}
	newp = newlen ? p : NULL;

	if (flags & PS_SYSCTL_OLEN) {
		*rlen = sizeof(oldlen) + oldlen;
		*rdata = malloc(*rlen);
		if (*rdata == NULL)
			return -1;
		oldlenp = (size_t *)*rdata;
		*oldlenp = oldlen;
		if (flags & PS_SYSCTL_ODATA)
			oldp = (char *)*rdata + sizeof(oldlen);
		else
			oldp = NULL;
	} else {
		oldlenp = NULL;
		oldp = NULL;
	}

	err = sysctl(name, namelen, oldp, oldlenp, newp, newlen);
	return err;
}
#endif

ssize_t
ps_root_os(struct dhcpcd_ctx *ctx, struct ps_msghdr *psm, struct msghdr *msg,
    void **rdata, size_t *rlen, bool *free_rdata)
{
	struct iovec *iov = msg->msg_iov;
	void *data = iov->iov_base;
	size_t len = iov->iov_len;
	ssize_t err;

	switch (psm->ps_cmd) {
	case PS_IOCTLLINK:
		err = ps_root_doioctldom(ctx, PF_LINK, psm->ps_flags, data, len);
		break;
	case PS_IOCTL6:
		err = ps_root_doioctldom(ctx, PF_INET6, psm->ps_flags, data, len);
		break;
	case PS_ROUTE:
		return ps_root_doroute(ctx, data, len);
#if defined(HAVE_CAPSICUM) || defined(HAVE_PLEDGE)
	case PS_IOCTLINDIRECT:
		err = ps_root_doindirectioctl(ctx, psm->ps_flags, data, len);
		break;
#endif
#ifdef HAVE_PLEDGE
	case PS_IFIGNOREGRP:
		return ps_root_doifignoregroup(ctx, data, len);
#endif
#ifdef HAVE_CAPSICUM
	case PS_SYSCTL:
		*free_rdata = true;
		return ps_root_dosysctl(psm->ps_flags, data, len, rdata, rlen);
#else
	UNUSED(free_rdata);
#endif
	default:
		errno = ENOTSUP;
		return -1;
	}

	if (err != -1) {
		*rdata = data;
		*rlen = len;
	}
	return err;
}

static ssize_t
ps_root_ioctldom(struct dhcpcd_ctx *ctx, uint16_t domain, unsigned long request,
    void *data, size_t len)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), domain,
	    request, data, len) == -1)
		return -1;
	return ps_root_readerror(ctx, data, len);
}

ssize_t
ps_root_ioctllink(struct dhcpcd_ctx *ctx, unsigned long request,
    void *data, size_t len)
{

	return ps_root_ioctldom(ctx, PS_IOCTLLINK, request, data, len);
}

ssize_t
ps_root_ioctl6(struct dhcpcd_ctx *ctx, unsigned long request,
    void *data, size_t len)
{

	return ps_root_ioctldom(ctx, PS_IOCTL6, request, data, len);
}

ssize_t
ps_root_route(struct dhcpcd_ctx *ctx, void *data, size_t len)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_ROUTE, 0, data, len) == -1)
		return -1;
	return ps_root_readerror(ctx, data, len);
}

#if defined(HAVE_CAPSICUM) || defined(HAVE_PLEDGE)
ssize_t
ps_root_indirectioctl(struct dhcpcd_ctx *ctx, unsigned long request,
    const char *ifname, void *data, size_t len)
{
	char buf[PS_BUFLEN];

	if (IFNAMSIZ + len > sizeof(buf)) {
		errno = ENOBUFS;
		return -1;
	}

	strlcpy(buf, ifname, IFNAMSIZ);
	memcpy(buf + IFNAMSIZ, data, len);
	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_IOCTLINDIRECT,
	    request, buf, IFNAMSIZ + len) == -1)
		return -1;
	return ps_root_readerror(ctx, data, len);
}
#endif

#ifdef HAVE_PLEDGE
ssize_t
ps_root_ifignoregroup(struct dhcpcd_ctx *ctx, const char *ifname)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_IFIGNOREGRP, 0,
	    ifname, strlen(ifname) + 1) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}
#endif

#ifdef HAVE_CAPSICUM
ssize_t
ps_root_sysctl(struct dhcpcd_ctx *ctx,
    const int *name, unsigned int namelen,
    void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{
	char buf[PS_BUFLEN], *p = buf;
	unsigned long flags = 0;
	size_t olen = (oldp && oldlenp) ? *oldlenp : 0, nolen;

	if (sizeof(namelen) + (sizeof(*name) * namelen) +
	    sizeof(oldlenp) +
	    sizeof(newlen) + newlen > sizeof(buf))
	{
		errno = ENOBUFS;
		return -1;
	}

	if (oldlenp)
		flags |= PS_SYSCTL_OLEN;
	if (oldp)
		flags |= PS_SYSCTL_ODATA;
	memcpy(p, &namelen, sizeof(namelen));
	p += sizeof(namelen);
	memcpy(p, name, sizeof(*name) * namelen);
	p += sizeof(*name) * namelen;
	memcpy(p, &olen, sizeof(olen));
	p += sizeof(olen);
	memcpy(p, &newlen, sizeof(newlen));
	p += sizeof(newlen);
	if (newlen) {
		memcpy(p, newp, newlen);
		p += newlen;
	}

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_SYSCTL,
	    flags, buf, (size_t)(p - buf)) == -1)
		return -1;

	if (ps_root_readerror(ctx, buf, sizeof(buf)) == -1)
		return -1;

	p = buf;
	memcpy(&nolen, p, sizeof(nolen));
	p += sizeof(nolen);
	if (oldlenp) {
		*oldlenp = nolen;
		if (oldp && nolen <= olen)
			memcpy(oldp, p, nolen);
	}

	return 0;
}
#endif
