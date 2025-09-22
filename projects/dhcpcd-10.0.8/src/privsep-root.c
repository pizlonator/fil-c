/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd, privileged proxy
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
#include "common.h"
#include "dev.h"
#include "dhcpcd.h"
#include "dhcp6.h"
#include "eloop.h"
#include "if.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "sa.h"
#include "script.h"

__CTASSERT(sizeof(ioctl_request_t) <= sizeof(unsigned long));

struct psr_error
{
	ssize_t psr_result;
	int psr_errno;
	char psr_pad[sizeof(ssize_t) - sizeof(int)];
	size_t psr_datalen;
};

struct psr_ctx {
	struct dhcpcd_ctx *psr_ctx;
	struct psr_error psr_error;
	size_t psr_datalen;
	void *psr_data;
};

static void
ps_root_readerrorcb(void *arg, unsigned short events)
{
	struct psr_ctx *psr_ctx = arg;
	struct dhcpcd_ctx *ctx = psr_ctx->psr_ctx;
	struct psr_error *psr_error = &psr_ctx->psr_error;
	struct iovec iov[] = {
		{ .iov_base = psr_error, .iov_len = sizeof(*psr_error) },
		{ .iov_base = psr_ctx->psr_data,
		  .iov_len = psr_ctx->psr_datalen },
	};
	ssize_t len;
	int exit_code = EXIT_FAILURE;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

#define PSR_ERROR(e)				\
	do {					\
		psr_error->psr_result = -1;	\
		psr_error->psr_errno = (e);	\
		goto out;			\
	} while (0 /* CONSTCOND */)

	len = readv(PS_ROOT_FD(ctx), iov, __arraycount(iov));
	if (len == -1)
		PSR_ERROR(errno);
	else if ((size_t)len < sizeof(*psr_error))
		PSR_ERROR(EINVAL);
	exit_code = EXIT_SUCCESS;

out:
	eloop_exit(ctx->ps_eloop, exit_code);
}

ssize_t
ps_root_readerror(struct dhcpcd_ctx *ctx, void *data, size_t len)
{
	struct psr_ctx psr_ctx = {
	    .psr_ctx = ctx,
	    .psr_data = data, .psr_datalen = len,
	};
	int fd = PS_ROOT_FD(ctx);

	if (eloop_event_add(ctx->ps_eloop, fd, ELE_READ,
	    ps_root_readerrorcb, &psr_ctx) == -1)
		return -1;

	eloop_enter(ctx->ps_eloop);
	eloop_start(ctx->ps_eloop, &ctx->sigset);
	eloop_event_delete(ctx->ps_eloop, fd);

	errno = psr_ctx.psr_error.psr_errno;
	return psr_ctx.psr_error.psr_result;
}

#ifdef PRIVSEP_GETIFADDRS
static void
ps_root_mreaderrorcb(void *arg, unsigned short events)
{
	struct psr_ctx *psr_ctx = arg;
	struct dhcpcd_ctx *ctx = psr_ctx->psr_ctx;
	struct psr_error *psr_error = &psr_ctx->psr_error;
	struct iovec iov[] = {
		{ .iov_base = psr_error, .iov_len = sizeof(*psr_error) },
		{ .iov_base = NULL, .iov_len = 0 },
	};
	ssize_t len;
	int exit_code = EXIT_FAILURE;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = recv(PS_ROOT_FD(ctx), psr_error, sizeof(*psr_error), MSG_PEEK);
	if (len == -1)
		PSR_ERROR(errno);
	else if ((size_t)len < sizeof(*psr_error))
		PSR_ERROR(EINVAL);

	if (psr_error->psr_datalen > SSIZE_MAX)
		PSR_ERROR(ENOBUFS);
	else if (psr_error->psr_datalen != 0) {
		psr_ctx->psr_data = malloc(psr_error->psr_datalen);
		if (psr_ctx->psr_data == NULL)
			PSR_ERROR(errno);
		psr_ctx->psr_datalen = psr_error->psr_datalen;
		iov[1].iov_base = psr_ctx->psr_data;
		iov[1].iov_len = psr_ctx->psr_datalen;
	}

	len = readv(PS_ROOT_FD(ctx), iov, __arraycount(iov));
	if (len == -1)
		PSR_ERROR(errno);
	else if ((size_t)len != sizeof(*psr_error) + psr_ctx->psr_datalen)
		PSR_ERROR(EINVAL);
	exit_code = EXIT_SUCCESS;

out:
	eloop_exit(ctx->ps_eloop, exit_code);
}

ssize_t
ps_root_mreaderror(struct dhcpcd_ctx *ctx, void **data, size_t *len)
{
	struct psr_ctx psr_ctx = {
	    .psr_ctx = ctx,
	};
	int fd = PS_ROOT_FD(ctx);

	if (eloop_event_add(ctx->ps_eloop, fd, ELE_READ,
	    ps_root_mreaderrorcb, &psr_ctx) == -1)
		return -1;

	eloop_enter(ctx->ps_eloop);
	eloop_start(ctx->ps_eloop, &ctx->sigset);
	eloop_event_delete(ctx->ps_eloop, fd);

	errno = psr_ctx.psr_error.psr_errno;
	*data = psr_ctx.psr_data;
	*len = psr_ctx.psr_datalen;
	return psr_ctx.psr_error.psr_result;
}
#endif

static ssize_t
ps_root_writeerror(struct dhcpcd_ctx *ctx, ssize_t result,
    void *data, size_t len)
{
	struct psr_error psr = {
		.psr_result = result,
		.psr_errno = errno,
		.psr_datalen = len,
	};
	struct iovec iov[] = {
		{ .iov_base = &psr, .iov_len = sizeof(psr) },
		{ .iov_base = data, .iov_len = len },
	};
	ssize_t err;
	int fd = PS_ROOT_FD(ctx);

#ifdef PRIVSEP_DEBUG
	logdebugx("%s: result %zd errno %d", __func__, result, errno);
#endif

	err = writev(fd, iov, __arraycount(iov));

	/* Error sending the message? Try sending the error of sending. */
	if (err == -1) {
		logerr("%s: result=%zd, data=%p, len=%zu",
		    __func__, result, data, len);
		psr.psr_result = err;
		psr.psr_errno = errno;
		iov[1].iov_base = NULL;
		iov[1].iov_len = 0;
		err = writev(fd, iov, __arraycount(iov));
	}

	return err;
}

static ssize_t
ps_root_doioctl(unsigned long req, void *data, size_t len)
{
	int s, err;

	/* Only allow these ioctls */
	switch(req) {
#ifdef SIOCAIFADDR
	case SIOCAIFADDR:	/* FALLTHROUGH */
	case SIOCDIFADDR:	/* FALLTHROUGH */
#endif
#ifdef SIOCSIFHWADDR
	case SIOCSIFHWADDR:	/* FALLTHROUGH */
#endif
#ifdef SIOCGIFPRIORITY
	case SIOCGIFPRIORITY:	/* FALLTHROUGH */
#endif
	case SIOCSIFFLAGS:	/* FALLTHROUGH */
	case SIOCGIFMTU:	/* FALLTHROUGH */
	case SIOCSIFMTU:
		break;
	default:
		errno = EPERM;
		return -1;
	}

	s = xsocket(PF_INET, SOCK_DGRAM, 0);
	if (s != -1)
#ifdef IOCTL_REQUEST_TYPE
	{
		ioctl_request_t reqt;

		memcpy(&reqt, &req, sizeof(reqt));
		err = ioctl(s, reqt, data, len);
	}
#else
		err = ioctl(s, req, data, len);
#endif
	else
		err = -1;
	if (s != -1)
		close(s);
	return err;
}

static ssize_t
ps_root_run_script(struct dhcpcd_ctx *ctx, const void *data, size_t len)
{
	const char *envbuf = data;
	char * const argv[] = { ctx->script, NULL };
	pid_t pid;
	int status;

	if (len == 0)
		return 0;

	if (script_buftoenv(ctx, UNCONST(envbuf), len) == NULL)
		return -1;

	pid = script_exec(argv, ctx->script_env);
	if (pid == -1)
		return -1;

	/* Wait for the script to finish */
	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR) {
			logerr(__func__);
			status = 0;
			break;
		}
	}
	return status;
}

static bool
ps_root_validpath(const struct dhcpcd_ctx *ctx, uint16_t cmd, const char *path)
{

	/* Avoid a previous directory attack to avoid /proc/../
	 * dhcpcd should never use a path with double dots. */
	if (strstr(path, "..") != NULL)
		return false;

	if (cmd == PS_READFILE) {
#ifdef EMBEDDED_CONFIG
		if (strcmp(ctx->cffile, EMBEDDED_CONFIG) == 0)
			return true;
#endif
		if (strcmp(ctx->cffile, path) == 0)
			return true;
	}
	if (strncmp(DBDIR, path, strlen(DBDIR)) == 0)
		return true;
	if (strncmp(RUNDIR, path, strlen(RUNDIR)) == 0)
		return true;

#ifdef __linux__
	if (strncmp("/proc/net/", path, strlen("/proc/net/")) == 0 ||
	    strncmp("/proc/sys/net/", path, strlen("/proc/sys/net/")) == 0 ||
	    strncmp("/sys/class/net/", path, strlen("/sys/class/net/")) == 0)
		return true;
#endif

	errno = EPERM;
	return false;
}

static ssize_t
ps_root_dowritefile(const struct dhcpcd_ctx *ctx,
    mode_t mode, void *data, size_t len)
{
	char *file = data, *nc;

	nc = memchr(file, '\0', len);
	if (nc == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (!ps_root_validpath(ctx, PS_WRITEFILE, file))
		return -1;
	nc++;
	return writefile(file, mode, nc, len - (size_t)(nc - file));
}

#ifdef AUTH
static ssize_t
ps_root_monordm(uint64_t *rdm, size_t len)
{

	if (len != sizeof(*rdm)) {
		errno = EINVAL;
		return -1;
	}
	return auth_get_rdm_monotonic(rdm);
}
#endif

#ifdef PRIVSEP_GETIFADDRS
#define	IFA_NADDRS	4
static ssize_t
ps_root_dogetifaddrs(void **rdata, size_t *rlen)
{
	struct ifaddrs *ifaddrs, *ifa;
	size_t len;
	uint8_t *buf, *sap;
	socklen_t salen;

	if (getifaddrs(&ifaddrs) == -1)
		return -1;
	if (ifaddrs == NULL) {
		*rdata = NULL;
		*rlen = 0;
		return 0;
	}

	/* Work out the buffer length required.
	 * Ensure everything is aligned correctly, which does
	 * create a larger buffer than what is needed to send,
	 * but makes creating the same structure in the client
	 * much easier. */
	len = 0;
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		len += ALIGN(sizeof(*ifa));
		len += ALIGN(IFNAMSIZ);
		len += ALIGN(sizeof(salen) * IFA_NADDRS);
		if (ifa->ifa_addr != NULL)
			len += ALIGN(sa_len(ifa->ifa_addr));
		if (ifa->ifa_netmask != NULL)
			len += ALIGN(sa_len(ifa->ifa_netmask));
		if (ifa->ifa_broadaddr != NULL)
			len += ALIGN(sa_len(ifa->ifa_broadaddr));
#ifdef BSD
		/*
		 * On BSD we need to carry ifa_data so we can access
		 * if_data->ifi_link_state
		 */
		if (ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_LINK)
			len += ALIGN(sizeof(struct if_data));
#endif
	}

	/* Use calloc to set everything to zero.
	 * This satisfies memory sanitizers because don't write
	 * where we don't need to. */
	buf = calloc(1, len);
	if (buf == NULL) {
		freeifaddrs(ifaddrs);
		return -1;
	}
	*rdata = buf;
	*rlen = len;

	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		memcpy(buf, ifa, sizeof(*ifa));
		buf += ALIGN(sizeof(*ifa));

		strlcpy((char *)buf, ifa->ifa_name, IFNAMSIZ);
		buf += ALIGN(IFNAMSIZ);
		sap = buf;
		buf += ALIGN(sizeof(salen) * IFA_NADDRS);

#define	COPYINSA(addr)						\
	do {							\
		if ((addr) != NULL)				\
			salen = sa_len((addr));			\
		else						\
			salen = 0;				\
		if (salen != 0) {				\
			memcpy(sap, &salen, sizeof(salen));	\
			memcpy(buf, (addr), salen);		\
			buf += ALIGN(salen);			\
		}						\
		sap += sizeof(salen);				\
	} while (0 /*CONSTCOND */)

		COPYINSA(ifa->ifa_addr);
		COPYINSA(ifa->ifa_netmask);
		COPYINSA(ifa->ifa_broadaddr);

#ifdef BSD
		if (ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_LINK)
		{
			salen = (socklen_t)sizeof(struct if_data);
			memcpy(buf, ifa->ifa_data, salen);
			buf += ALIGN(salen);
		} else
#endif
			salen = 0;
		memcpy(sap, &salen, sizeof(salen));
	}

	freeifaddrs(ifaddrs);
	return 0;
}
#endif

static ssize_t
ps_root_recvmsgcb(void *arg, struct ps_msghdr *psm, struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = arg;
	uint16_t cmd;
	struct ps_process *psp;
	struct iovec *iov = msg->msg_iov;
	void *data = iov->iov_base, *rdata = NULL;
	size_t len = iov->iov_len, rlen = 0;
	uint8_t buf[PS_BUFLEN];
	time_t mtime;
	ssize_t err;
	bool free_rdata = false;

	cmd = (uint16_t)(psm->ps_cmd & ~(PS_START | PS_STOP));
	psp = ps_findprocess(ctx, &psm->ps_id);

#ifdef PRIVSEP_DEBUG
	logerrx("%s: IN cmd %x, psp %p", __func__, psm->ps_cmd, psp);
#endif

	if (psp != NULL) {
		if (psm->ps_cmd & PS_STOP) {
			return ps_stopprocess(psp);
		} else if (psm->ps_cmd & PS_START) {
			/* Process has already started .... */
			logdebugx("%s%sprocess %s already started on pid %d",
			    psp->psp_ifname,
			    psp->psp_ifname[0] != '\0' ? ": " : "",
			    psp->psp_name, psp->psp_pid);
			return 0;
		}

		err = ps_sendpsmmsg(ctx, psp->psp_fd, psm, msg);
		if (err == -1) {
			logerr("%s: failed to send message to pid %d",
			    __func__, psp->psp_pid);
			ps_freeprocess(psp);
		}
		return 0;
	}

	if (psm->ps_cmd & PS_STOP && psp == NULL)
		return 0;

	switch (cmd) {
#ifdef INET
#ifdef ARP
	case PS_BPF_ARP:	/* FALLTHROUGH */
#endif
	case PS_BPF_BOOTP:
		return ps_bpf_cmd(ctx, psm, msg);
#endif
#ifdef INET
	case PS_BOOTP:
		return ps_inet_cmd(ctx, psm, msg);
#endif
#ifdef INET6
#ifdef DHCP6
	case PS_DHCP6:	/* FALLTHROUGH */
#endif
	case PS_ND:
		return ps_inet_cmd(ctx, psm, msg);
#endif
	default:
		break;
	}

	assert(msg->msg_iovlen == 0 || msg->msg_iovlen == 1);

	/* Reset errno */
	errno = 0;

	switch (psm->ps_cmd) {
	case PS_IOCTL:
		err = ps_root_doioctl(psm->ps_flags, data, len);
		if (err != -1) {
			rdata = data;
			rlen = len;
		}
		break;
	case PS_SCRIPT:
		err = ps_root_run_script(ctx, data, len);
		break;
	case PS_STOPPROCS:
		ctx->options |= DHCPCD_EXITING;
		TAILQ_FOREACH(psp, &ctx->ps_processes, next) {
			if (psp != ctx->ps_root)
				ps_stopprocess(psp);
		}
		err = ps_stopwait(ctx);
		break;
	case PS_UNLINK:
		if (!ps_root_validpath(ctx, psm->ps_cmd, data)) {
			err = -1;
			break;
		}
		err = unlink(data);
		break;
	case PS_READFILE:
		if (!ps_root_validpath(ctx, psm->ps_cmd, data)) {
			err = -1;
			break;
		}
		err = readfile(data, buf, sizeof(buf));
		if (err != -1) {
			rdata = buf;
			rlen = (size_t)err;
		}
		break;
	case PS_WRITEFILE:
		err = ps_root_dowritefile(ctx, (mode_t)psm->ps_flags,
		    data, len);
		break;
	case PS_FILEMTIME:
		err = filemtime(data, &mtime);
		if (err != -1) {
			rdata = &mtime;
			rlen = sizeof(mtime);
		}
		break;
	case PS_LOGREOPEN:
		err = logopen(ctx->logfile);
		break;
#ifdef AUTH
	case PS_AUTH_MONORDM:
		err = ps_root_monordm(data, len);
		if (err != -1) {
			rdata = data;
			rlen = len;
		}
		break;
#endif
#ifdef PRIVSEP_GETIFADDRS
	case PS_GETIFADDRS:
		err = ps_root_dogetifaddrs(&rdata, &rlen);
		free_rdata = true;
		break;
#endif
#if defined(INET6) && (defined(__linux__) || defined(HAVE_PLEDGE))
	case PS_IP6FORWARDING:
		 err = ip6_forwarding(data);
		 break;
#endif
#ifdef PLUGIN_DEV
	case PS_DEV_INITTED:
		err = dev_initialised(ctx, data);
		break;
	case PS_DEV_LISTENING:
		err = dev_listening(ctx);
		break;
#endif
	default:
		err = ps_root_os(ctx, psm, msg, &rdata, &rlen, &free_rdata);
		break;
	}

	err = ps_root_writeerror(ctx, err, rlen != 0 ? rdata : 0, rlen);
	if (free_rdata)
		free(rdata);
	return err;
}

/* Receive from state engine, do an action. */
static void
ps_root_recvmsg(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;

	if (ps_recvpsmsg(psp->psp_ctx, psp->psp_fd, events,
	    ps_root_recvmsgcb, psp->psp_ctx) == -1)
		logerr(__func__);
}

#ifdef PLUGIN_DEV
static int
ps_root_handleinterface(void *arg, int action, const char *ifname)
{
	struct dhcpcd_ctx *ctx = arg;
	unsigned long flag;

	if (action == 1)
		flag = PS_DEV_IFADDED;
	else if (action == -1)
		flag = PS_DEV_IFREMOVED;
	else if (action == 0)
		flag = PS_DEV_IFUPDATED;
	else {
		errno = EINVAL;
		return -1;
	}

	return (int)ps_sendcmd(ctx, ctx->ps_data_fd, PS_DEV_IFCMD,
	    flag, ifname, strlen(ifname) + 1);
}
#endif

static int
ps_root_startcb(struct ps_process *psp)
{
	struct dhcpcd_ctx *ctx = psp->psp_ctx;

	if (ctx->options & DHCPCD_MANAGER)
		setproctitle("[privileged proxy]");
	else
		setproctitle("[privileged proxy] %s%s%s",
		    ctx->ifv[0],
		    ctx->options & DHCPCD_IPV4 ? " [ip4]" : "",
		    ctx->options & DHCPCD_IPV6 ? " [ip6]" : "");
	ctx->options |= DHCPCD_PRIVSEPROOT;

	if (if_opensockets(ctx) == -1)
		logerr("%s: if_opensockets", __func__);

	/* Open network sockets for sending.
	 * This is a small bit wasteful for non sandboxed OS's
	 * but makes life very easy for unicasting DHCPv6 in non manager
	 * mode as we no longer care about address selection.
	 * We can't call shutdown SHUT_RD on the socket because it's
	 * not connected. All we can do is try and set a zero sized
	 * receive buffer and just let it overflow.
	 * Reading from it just to drain it is a waste of CPU time. */
#ifdef INET
	if (ctx->options & DHCPCD_IPV4) {
		int buflen = 1;

		ctx->udp_wfd = xsocket(PF_INET,
		    SOCK_RAW | SOCK_CXNB, IPPROTO_UDP);
		if (ctx->udp_wfd == -1)
			logerr("%s: dhcp_openraw", __func__);
		else if (setsockopt(ctx->udp_wfd, SOL_SOCKET, SO_RCVBUF,
		    &buflen, sizeof(buflen)) == -1)
			logerr("%s: setsockopt SO_RCVBUF DHCP", __func__);
	}
#endif
#ifdef INET6
	if (ctx->options & DHCPCD_IPV6) {
		int buflen = 1;

		ctx->nd_fd = ipv6nd_open(false);
		if (ctx->nd_fd == -1)
			logerr("%s: ipv6nd_open", __func__);
		else if (setsockopt(ctx->nd_fd, SOL_SOCKET, SO_RCVBUF,
		    &buflen, sizeof(buflen)) == -1)
			logerr("%s: setsockopt SO_RCVBUF ND", __func__);
	}
#endif
#ifdef DHCP6
	if (ctx->options & DHCPCD_IPV6) {
		int buflen = 1;

		ctx->dhcp6_wfd = dhcp6_openraw();
		if (ctx->dhcp6_wfd == -1)
			logerr("%s: dhcp6_openraw", __func__);
		else if (setsockopt(ctx->dhcp6_wfd, SOL_SOCKET, SO_RCVBUF,
		    &buflen, sizeof(buflen)) == -1)
			logerr("%s: setsockopt SO_RCVBUF DHCP6", __func__);
	}
#endif

#ifdef PLUGIN_DEV
	/* Start any dev listening plugin which may want to
	 * change the interface name provided by the kernel */
	if ((ctx->options & (DHCPCD_MANAGER | DHCPCD_DEV)) ==
	    (DHCPCD_MANAGER | DHCPCD_DEV))
		dev_start(ctx, ps_root_handleinterface);
#endif

	return 0;
}

void
ps_root_signalcb(int sig, void *arg)
{
	struct dhcpcd_ctx *ctx = arg;
	int status;
	pid_t pid;
	const char *ifname, *name;
	struct ps_process *psp;

	if (sig != SIGCHLD)
		return;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		psp = ps_findprocesspid(ctx, pid);
		if (psp != NULL) {
			ifname = psp->psp_ifname;
			name = psp->psp_name;
		} else {
			/* Ignore logging the double fork */
			if (ctx->options & DHCPCD_LAUNCHER)
				continue;
			ifname = "";
			name = "unknown process";
		}

		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			logerrx("%s%s%s exited unexpectedly from PID %d,"
			    " code=%d",
			    ifname, ifname[0] != '\0' ? ": " : "",
			    name, pid, WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			logerrx("%s%s%s exited unexpectedly from PID %d,"
			    " signal=%s",
			    ifname, ifname[0] != '\0' ? ": " : "",
			    name, pid, strsignal(WTERMSIG(status)));
		else
			logdebugx("%s%s%s exited from PID %d",
			    ifname, ifname[0] != '\0' ? ": " : "",
			    name, pid);

		if (psp != NULL)
			ps_freeprocess(psp);
	}

	if (!(ctx->options & DHCPCD_EXITING))
		return;
	if (!(ps_waitforprocs(ctx)))
		eloop_exit(ctx->ps_eloop, EXIT_SUCCESS);
}

int (*handle_interface)(void *, int, const char *);

#ifdef PLUGIN_DEV
static ssize_t
ps_root_devcb(struct dhcpcd_ctx *ctx, struct ps_msghdr *psm, struct msghdr *msg)
{
	int action;
	struct iovec *iov = msg->msg_iov;

	if (msg->msg_iovlen != 1) {
		errno = EINVAL;
		return -1;
	}

	switch(psm->ps_flags) {
	case PS_DEV_IFADDED:
		action = 1;
		break;
	case PS_DEV_IFREMOVED:
		action = -1;
		break;
	case PS_DEV_IFUPDATED:
		action = 0;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	return dhcpcd_handleinterface(ctx, action, iov->iov_base);
}
#endif

static ssize_t
ps_root_dispatchcb(void *arg, struct ps_msghdr *psm, struct msghdr *msg)
{
	struct dhcpcd_ctx *ctx = arg;
	ssize_t err;

	switch(psm->ps_cmd) {
#ifdef PLUGIN_DEV
	case PS_DEV_IFCMD:
		err = ps_root_devcb(ctx, psm, msg);
		break;
#endif
	default:
#ifdef INET
		err = ps_bpf_dispatch(ctx, psm, msg);
		if (err == -1 && errno == ENOTSUP)
#endif
			err = ps_inet_dispatch(ctx, psm, msg);
	}
	return err;
}

static void
ps_root_dispatch(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	if (ps_recvpsmsg(ctx, ctx->ps_data_fd, events,
	    ps_root_dispatchcb, ctx) == -1)
		logerr(__func__);
}

static void
ps_root_log(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx = arg;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	if (logreadfd(ctx->ps_log_root_fd) == -1)
		logerr(__func__);
}

pid_t
ps_root_start(struct dhcpcd_ctx *ctx)
{
	struct ps_id id = {
		.psi_ifindex = 0,
		.psi_cmd = PS_ROOT,
	};
	struct ps_process *psp;
	int logfd[2], datafd[2];
	pid_t pid;

	if (xsocketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CXNB, 0, logfd) == -1)
		return -1;
#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fdpair(logfd) == -1)
		return -1;
#endif

	if (xsocketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CXNB, 0, datafd) == -1)
		return -1;
	if (ps_setbuf_fdpair(datafd) == -1)
		return -1;
#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fdpair(datafd) == -1)
		return -1;
#endif

	psp = ctx->ps_root = ps_newprocess(ctx, &id);
	strlcpy(psp->psp_name, "privileged proxy", sizeof(psp->psp_name));
	pid = ps_startprocess(psp, ps_root_recvmsg, NULL,
	    ps_root_startcb, ps_root_signalcb, PSF_ELOOP);

	if (pid == 0) {
		ctx->ps_log_fd = logfd[0]; /* Keep open to pass to processes */
		ctx->ps_log_root_fd = logfd[1];
		if (eloop_event_add(ctx->eloop, ctx->ps_log_root_fd, ELE_READ,
		    ps_root_log, ctx) == -1)
			return -1;
		ctx->ps_data_fd = datafd[1];
		close(datafd[0]);
		return 0;
	} else if (pid == -1)
		return -1;

	logsetfd(logfd[0]);
	close(logfd[1]);

	ctx->ps_data_fd = datafd[0];
	close(datafd[1]);
	if (eloop_event_add(ctx->eloop, ctx->ps_data_fd, ELE_READ,
	    ps_root_dispatch, ctx) == -1)
		return 1;

	return pid;
}

void
ps_root_close(struct dhcpcd_ctx *ctx)
{

	if_closesockets(ctx);

#ifdef INET
	if (ctx->udp_wfd != -1) {
		close(ctx->udp_wfd);
		ctx->udp_wfd = -1;
	}
#endif
#ifdef INET6
	if (ctx->nd_fd != -1) {
		close(ctx->nd_fd);
		ctx->nd_fd = -1;
	}
#endif
#ifdef DHCP6
	if (ctx->dhcp6_wfd != -1) {
		close(ctx->dhcp6_wfd);
		ctx->dhcp6_wfd = -1;
	}
#endif
}

int
ps_root_stop(struct dhcpcd_ctx *ctx)
{
	struct ps_process *psp = ctx->ps_root;

	if (!(ctx->options & DHCPCD_PRIVSEP) ||
	    ctx->eloop == NULL)
		return 0;

	/* If we are the root process then remove the pidfile */
	if (ctx->options & DHCPCD_PRIVSEPROOT &&
	    !(ctx->options & DHCPCD_TEST))
	{
		if (unlink(ctx->pidfile) == -1)
			logerr("%s: unlink: %s", __func__, ctx->pidfile);
	}

	/* Only the manager process gets past this point. */
	if (ctx->options & DHCPCD_FORKED)
		return 0;

	/* We cannot log the root process exited before we
	 * log dhcpcd exits because the latter requires the former.
	 * So we just log the intent to exit.
	 * Even sending this will be a race to exit. */
	if (psp) {
		logdebugx("%s%s%s will exit from PID %d",
		    psp->psp_ifname,
		    psp->psp_ifname[0] != '\0' ? ": " : "",
		    psp->psp_name, psp->psp_pid);

		if (ps_stopprocess(psp) == -1)
			return -1;
	} /* else the root process has already exited :( */

	return ps_stopwait(ctx);
}

ssize_t
ps_root_stopprocesses(struct dhcpcd_ctx *ctx)
{

	if (!(IN_PRIVSEP_SE(ctx)))
		return 0;

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_STOPPROCS, 0,
	    NULL, 0) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}

ssize_t
ps_root_script(struct dhcpcd_ctx *ctx, const void *data, size_t len)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_SCRIPT,
	    0, data, len) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}

ssize_t
ps_root_ioctl(struct dhcpcd_ctx *ctx, ioctl_request_t req, void *data,
    size_t len)
{
	int fd = PS_ROOT_FD(ctx);
#ifdef IOCTL_REQUEST_TYPE
	unsigned long ulreq = 0;

	memcpy(&ulreq, &req, sizeof(req));
	if (ps_sendcmd(ctx, fd, PS_IOCTL, ulreq, data, len) == -1)
		return -1;
#else
	if (ps_sendcmd(ctx, fd, PS_IOCTL, req, data, len) == -1)
		return -1;
#endif
	return ps_root_readerror(ctx, data, len);
}

ssize_t
ps_root_unlink(struct dhcpcd_ctx *ctx, const char *file)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_UNLINK, 0,
	    file, strlen(file) + 1) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}

ssize_t
ps_root_readfile(struct dhcpcd_ctx *ctx, const char *file,
    void *data, size_t len)
{
	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_READFILE, 0,
	    file, strlen(file) + 1) == -1)
		return -1;
	return ps_root_readerror(ctx, data, len);
}

ssize_t
ps_root_writefile(struct dhcpcd_ctx *ctx, const char *file, mode_t mode,
    const void *data, size_t len)
{
	char buf[PS_BUFLEN];
	size_t flen;

	flen = strlcpy(buf, file, sizeof(buf));
	flen += 1;
	if (flen > sizeof(buf) || flen + len > sizeof(buf)) {
		errno = ENOBUFS;
		return -1;
	}
	memcpy(buf + flen, data, len);

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_WRITEFILE, mode,
	    buf, flen + len) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}

ssize_t
ps_root_filemtime(struct dhcpcd_ctx *ctx, const char *file, time_t *time)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_FILEMTIME, 0,
	    file, strlen(file) + 1) == -1)
		return -1;
	return ps_root_readerror(ctx, time, sizeof(*time));
}

ssize_t
ps_root_logreopen(struct dhcpcd_ctx *ctx)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_LOGREOPEN, 0,
	    NULL, 0) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}

#ifdef PRIVSEP_GETIFADDRS
int
ps_root_getifaddrs(struct dhcpcd_ctx *ctx, struct ifaddrs **ifahead)
{
	struct ifaddrs *ifa;
	void *buf = NULL;
	char *bp, *sap;
	socklen_t salen;
	size_t len;
	ssize_t err;

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx),
	    PS_GETIFADDRS, 0, NULL, 0) == -1)
		return -1;
	err = ps_root_mreaderror(ctx, &buf, &len);

	if (err == -1)
		return -1;

	/* Should be impossible - lo0 will always exist. */
	if (len == 0) {
		*ifahead = NULL;
		return 0;
	}

	bp = buf;
	*ifahead = (struct ifaddrs *)(void *)bp;
	for (ifa = *ifahead; ifa != NULL; ifa = ifa->ifa_next) {
		if (len < ALIGN(sizeof(*ifa)) +
		    ALIGN(IFNAMSIZ) + ALIGN(sizeof(salen) * IFA_NADDRS))
			goto err;
		bp += ALIGN(sizeof(*ifa));
		ifa->ifa_name = bp;
		bp += ALIGN(IFNAMSIZ);
		sap = bp;
		bp += ALIGN(sizeof(salen) * IFA_NADDRS);
		len -= ALIGN(sizeof(*ifa)) +
		    ALIGN(IFNAMSIZ) + ALIGN(sizeof(salen) * IFA_NADDRS);

#define	COPYOUTSA(addr)							\
	do {								\
		memcpy(&salen, sap, sizeof(salen));			\
		if (len < salen)					\
			goto err;					\
		if (salen != 0) {					\
			(addr) = (struct sockaddr *)(void *)bp;		\
			bp += ALIGN(salen);				\
			len -= ALIGN(salen);				\
		}							\
		sap += sizeof(salen);					\
	} while (0 /* CONSTCOND */)

		COPYOUTSA(ifa->ifa_addr);
		COPYOUTSA(ifa->ifa_netmask);
		COPYOUTSA(ifa->ifa_broadaddr);

		memcpy(&salen, sap, sizeof(salen));
		if (len < salen)
			goto err;
		if (salen != 0) {
			ifa->ifa_data = bp;
			bp += ALIGN(salen);
			len -= ALIGN(salen);
		} else
			ifa->ifa_data = NULL;

		if (len != 0)
			ifa->ifa_next = (struct ifaddrs *)(void *)bp;
		else
			ifa->ifa_next = NULL;
	}
	return 0;

err:
	free(buf);
	*ifahead = NULL;
	errno = EINVAL;
	return -1;
}
#endif

#if defined(__linux__) || defined(HAVE_PLEDGE)
ssize_t
ps_root_ip6forwarding(struct dhcpcd_ctx *ctx, const char *ifname)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_IP6FORWARDING, 0,
	    ifname, ifname != NULL ? strlen(ifname) + 1 : 0) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}
#endif

#ifdef AUTH
int
ps_root_getauthrdm(struct dhcpcd_ctx *ctx, uint64_t *rdm)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_AUTH_MONORDM, 0,
	    rdm, sizeof(*rdm))== -1)
		return -1;
	return (int)ps_root_readerror(ctx, rdm, sizeof(*rdm));
}
#endif

#ifdef PLUGIN_DEV
int
ps_root_dev_initialised(struct dhcpcd_ctx *ctx, const char *ifname)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_DEV_INITTED, 0,
	    ifname, strlen(ifname) + 1)== -1)
		return -1;
	return (int)ps_root_readerror(ctx, NULL, 0);
}

int
ps_root_dev_listening(struct dhcpcd_ctx * ctx)
{

	if (ps_sendcmd(ctx, PS_ROOT_FD(ctx), PS_DEV_LISTENING,
	    0, NULL, 0) == -1)
		return -1;
	return (int)ps_root_readerror(ctx, NULL, 0);
}
#endif
