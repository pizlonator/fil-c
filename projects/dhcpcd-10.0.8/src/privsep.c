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

/*
 * The current design is this:
 * Spawn a priv process to carry out privileged actions and
 * spawning unpriv process to initate network connections such as BPF
 * or address specific listener.
 * Spawn an unpriv process to send/receive common network data.
 * Then drop all privs and start running.
 * Every process aside from the privileged proxy is chrooted.
 * All privsep processes ignore signals - only the manager process accepts them.
 *
 * dhcpcd will maintain the config file in the chroot, no need to handle
 * this in a script or something.
 */

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef AF_LINK
#include <net/if_dl.h>
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stddef.h>	/* For offsetof, struct padding debug */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arp.h"
#include "common.h"
#include "control.h"
#include "dev.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "eloop.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"

#ifdef HAVE_CAPSICUM
#include <sys/capsicum.h>
#include <sys/procdesc.h>
#include <capsicum_helpers.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif

/* CMSG_ALIGN is a Linux extension */
#ifndef CMSG_ALIGN
#define CMSG_ALIGN(n)	(CMSG_SPACE((n)) - CMSG_SPACE(0))
#endif

/* Calculate number of padding bytes to achieve 'struct cmsghdr' alignment */
#define CALC_CMSG_PADLEN(has_cmsg, pos) \
    ((has_cmsg) ? (socklen_t)(CMSG_ALIGN((pos)) - (pos)) : 0)

int
ps_init(struct dhcpcd_ctx *ctx)
{
	struct passwd *pw;
	struct stat st;

	errno = 0;
	if ((ctx->ps_user = pw = getpwnam(PRIVSEP_USER)) == NULL) {
		ctx->options &= ~DHCPCD_PRIVSEP;
		if (errno == 0) {
			logerrx("no such user %s", PRIVSEP_USER);
			/* Just incase logerrx caused an error... */
			errno = 0;
		} else
			logerr("getpwnam");
		return -1;
	}

	if (stat(pw->pw_dir, &st) == -1 || !S_ISDIR(st.st_mode)) {
		ctx->options &= ~DHCPCD_PRIVSEP;
		logerrx("refusing chroot: %s: %s",
		    PRIVSEP_USER, pw->pw_dir);
		errno = 0;
		return -1;
	}

	ctx->options |= DHCPCD_PRIVSEP;
	return 0;
}

static int
ps_dropprivs(struct dhcpcd_ctx *ctx)
{
	struct passwd *pw = ctx->ps_user;

	if (ctx->options & DHCPCD_LAUNCHER)
		logdebugx("chrooting as %s to %s", pw->pw_name, pw->pw_dir);
	if (chroot(pw->pw_dir) == -1 &&
	    (errno != EPERM || ctx->options & DHCPCD_FORKED))
		logerr("%s: chroot: %s", __func__, pw->pw_dir);
	if (chdir("/") == -1)
		logerr("%s: chdir: /", __func__);

	if ((setgroups(1, &pw->pw_gid) == -1 ||
	     setgid(pw->pw_gid) == -1 ||
	     setuid(pw->pw_uid) == -1) &&
	     (errno != EPERM || ctx->options & DHCPCD_FORKED))
	{
		logerr("failed to drop privileges");
		return -1;
	}

	struct rlimit rzero = { .rlim_cur = 0, .rlim_max = 0 };

	/* Prohibit new files, sockets, etc */
	/*
	 * If poll(2) is called with nfds>RLIMIT_NOFILE then it returns EINVAL.
	 * We don't know the final value of nfds at this point *easily*.
	 * Sadly, this is a POSIX limitation and most platforms adhere to it.
	 * However, some are not that strict and are whitelisted below.
	 * Also, if we're not using poll then we can be restrictive.
	 *
	 * For the non whitelisted platforms there should be a sandbox to
	 * fallback to where we don't allow new files, etc:
	 *      Linux:seccomp, FreeBSD:capsicum, OpenBSD:pledge
	 * Solaris users are sadly out of luck on both counts.
	 */
#if defined(__NetBSD__) || defined(__DragonFly__) || \
    defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	/* The control proxy *does* need to create new fd's via accept(2). */
	if (ctx->ps_ctl == NULL || ctx->ps_ctl->psp_pid != getpid()) {
		if (setrlimit(RLIMIT_NOFILE, &rzero) == -1)
			logerr("setrlimit RLIMIT_NOFILE");
	}
#endif

#define DHC_NOCHKIO	(DHCPCD_STARTED | DHCPCD_DAEMONISE)
	/* Prohibit writing to files.
	 * Obviously this won't work if we are using a logfile
	 * or redirecting stderr to a file. */
	if ((ctx->options & DHC_NOCHKIO) == DHC_NOCHKIO ||
	    (ctx->logfile == NULL && isatty(STDERR_FILENO) == 1))
	{
		if (setrlimit(RLIMIT_FSIZE, &rzero) == -1)
			logerr("setrlimit RLIMIT_FSIZE");
	}

#ifdef RLIMIT_NPROC
	/* Prohibit forks */
	if (setrlimit(RLIMIT_NPROC, &rzero) == -1)
		logerr("setrlimit RLIMIT_NPROC");
#endif

	return 0;
}

static int
ps_setbuf0(int fd, int ctl, int minlen)
{
	int len;
	socklen_t slen;

	slen = sizeof(len);
	if (getsockopt(fd, SOL_SOCKET, ctl, &len, &slen) == -1)
		return -1;

#ifdef __linux__
	len /= 2;
#endif
	if (len >= minlen)
		return 0;

	return setsockopt(fd, SOL_SOCKET, ctl, &minlen, sizeof(minlen));
}

static int
ps_setbuf(int fd)
{
	/* Ensure we can receive a fully sized privsep message.
	 * Double the send buffer. */
	int minlen = (int)sizeof(struct ps_msg);

	if (ps_setbuf0(fd, SO_RCVBUF, minlen) == -1 ||
	    ps_setbuf0(fd, SO_SNDBUF, minlen * 2) == -1)
	{
		logerr(__func__);
		return -1;
	}
	return 0;
}

int
ps_setbuf_fdpair(int fd[])
{

	if (ps_setbuf(fd[0]) == -1 || ps_setbuf(fd[1]) == -1)
		return -1;
	return 0;
}

#ifdef PRIVSEP_RIGHTS
int
ps_rights_limit_ioctl(int fd)
{
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_IOCTL);
	if (cap_rights_limit(fd, &rights) == -1 && errno != ENOSYS)
		return -1;
	return 0;
}

int
ps_rights_limit_fd_fctnl(int fd)
{
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_EVENT,
	    CAP_ACCEPT, CAP_FCNTL);
	if (cap_rights_limit(fd, &rights) == -1 && errno != ENOSYS)
		return -1;
	return 0;
}

int
ps_rights_limit_fd(int fd)
{
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_EVENT, CAP_SHUTDOWN);
	if (cap_rights_limit(fd, &rights) == -1 && errno != ENOSYS)
		return -1;
	return 0;
}

int
ps_rights_limit_fd_sockopt(int fd)
{
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_EVENT,
	    CAP_GETSOCKOPT, CAP_SETSOCKOPT);
	if (cap_rights_limit(fd, &rights) == -1 && errno != ENOSYS)
		return -1;
	return 0;
}

int
ps_rights_limit_fd_rdonly(int fd)
{
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_READ, CAP_EVENT);
	if (cap_rights_limit(fd, &rights) == -1 && errno != ENOSYS)
		return -1;
	return 0;
}

int
ps_rights_limit_fdpair(int fd[])
{

	if (ps_rights_limit_fd(fd[0]) == -1 || ps_rights_limit_fd(fd[1]) == -1)
		return -1;
	return 0;
}

static int
ps_rights_limit_stdio()
{
	const int iebadf = CAPH_IGNORE_EBADF;
	int error = 0;

	if (caph_limit_stream(STDIN_FILENO, CAPH_READ | iebadf) == -1)
		error = -1;
	if (caph_limit_stream(STDOUT_FILENO, CAPH_WRITE | iebadf) == -1)
		error = -1;
	if (caph_limit_stream(STDERR_FILENO, CAPH_WRITE | iebadf) == -1)
		error = -1;

	return error;
}
#endif

#ifdef HAVE_CAPSICUM
static void
ps_processhangup(void *arg, unsigned short events)
{
	struct ps_process *psp = arg;
	struct dhcpcd_ctx *ctx = psp->psp_ctx;

	if (!(events & ELE_HANGUP))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	logdebugx("%s%s%s exited from PID %d",
	    psp->psp_ifname, psp->psp_ifname[0] != '\0' ? ": " : "",
	    psp->psp_name, psp->psp_pid);

	ps_freeprocess(psp);

	if (!(ctx->options & DHCPCD_EXITING))
		return;
	if (!(ps_waitforprocs(ctx)))
		eloop_exit(ctx->ps_eloop, EXIT_SUCCESS);
}
#endif

pid_t
ps_startprocess(struct ps_process *psp,
    void (*recv_msg)(void *, unsigned short),
    void (*recv_unpriv_msg)(void *, unsigned short),
    int (*callback)(struct ps_process *), void (*signal_cb)(int, void *),
    unsigned int flags)
{
	struct dhcpcd_ctx *ctx = psp->psp_ctx;
	int fd[2];
	pid_t pid;

	if (xsocketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CXNB, 0, fd) == -1) {
		logerr("%s: socketpair", __func__);
		return -1;
	}
	if (ps_setbuf_fdpair(fd) == -1) {
		logerr("%s: ps_setbuf_fdpair", __func__);
		return -1;
	}
#ifdef PRIVSEP_RIGHTS
	if (ps_rights_limit_fdpair(fd) == -1) {
		logerr("%s: ps_rights_limit_fdpair", __func__);
		return -1;
	}
#endif

#ifdef HAVE_CAPSICUM
	pid = pdfork(&psp->psp_pfd, PD_CLOEXEC);
#else
	pid = fork();
#endif
	switch (pid) {
	case -1:
#ifdef HAVE_CAPSICUM
		logerr("pdfork");
#else
		logerr("fork");
#endif
		return -1;
	case 0:
		psp->psp_pid = getpid();
		psp->psp_fd = fd[1];
		close(fd[0]);
		break;
	default:
		psp->psp_pid = pid;
		psp->psp_fd = fd[0];
		close(fd[1]);
		if (recv_unpriv_msg == NULL)
			;
		else if (eloop_event_add(ctx->eloop, psp->psp_fd, ELE_READ,
		    recv_unpriv_msg, psp) == -1)
		{
			logerr("%s: eloop_event_add fd %d",
			    __func__, psp->psp_fd);
			return -1;
		}
#ifdef HAVE_CAPSICUM
		if (eloop_event_add(ctx->eloop, psp->psp_pfd, ELE_HANGUP,
		    ps_processhangup, psp) == -1)
		{
			logerr("%s: eloop_event_add pfd %d",
			    __func__, psp->psp_pfd);
			return -1;
		}
#endif
		psp->psp_started = true;
		return pid;
	}

	/* If we are not the root process, close un-needed stuff. */
	if (ctx->ps_root != psp) {
		ps_root_close(ctx);
#ifdef PLUGIN_DEV
		dev_stop(ctx);
#endif
	}

	ctx->options |= DHCPCD_FORKED;
	if (ctx->ps_log_fd != -1)
		logsetfd(ctx->ps_log_fd);

#ifdef DEBUG_FD
	logerrx("pid %d log_fd=%d data_fd=%d psp_fd=%d",
	    getpid(), ctx->ps_log_fd, ctx->ps_data_fd, psp->psp_fd);
#endif

	eloop_clear(ctx->eloop, -1);
	eloop_forked(ctx->eloop);
	eloop_signal_set_cb(ctx->eloop,
	    dhcpcd_signals, dhcpcd_signals_len, signal_cb, ctx);
	/* ctx->sigset aready has the initial sigmask set in main() */
	if (eloop_signal_mask(ctx->eloop, NULL) == -1) {
		logerr("%s: eloop_signal_mask", __func__);
		goto errexit;
	}

	if (ctx->fork_fd != -1) {
		/* Already removed from eloop thanks to above clear. */
		close(ctx->fork_fd);
		ctx->fork_fd = -1;
	}

	/* This process has no need of the blocking inner eloop. */
	if (!(flags & PSF_ELOOP)) {
		eloop_free(ctx->ps_eloop);
		ctx->ps_eloop = NULL;
	} else
		eloop_forked(ctx->ps_eloop);

	pidfile_clean();
	ps_freeprocesses(ctx, psp);

	if (ctx->ps_root != psp) {
		ctx->options &= ~DHCPCD_PRIVSEPROOT;
		ctx->ps_root = NULL;
		if (ctx->ps_log_root_fd != -1) {
			/* Already removed from eloop thanks to above clear. */
			close(ctx->ps_log_root_fd);
			ctx->ps_log_root_fd = -1;
		}
#ifdef PRIVSEP_RIGHTS
		if (ps_rights_limit_stdio() == -1) {
			logerr("ps_rights_limit_stdio");
			goto errexit;
		}
#endif
	}

	if (eloop_event_add(ctx->eloop, psp->psp_fd, ELE_READ,
	    recv_msg, psp) == -1)
	{
		logerr("%d %s: eloop_event_add XX fd %d", getpid(), __func__, psp->psp_fd);
		goto errexit;
	}

	if (callback(psp) == -1)
		goto errexit;

	if (flags & PSF_DROPPRIVS)
		ps_dropprivs(ctx);

	psp->psp_started = true;
	return 0;

errexit:
	if (psp->psp_fd != -1) {
		close(psp->psp_fd);
		psp->psp_fd = -1;
	}
	eloop_exit(ctx->eloop, EXIT_FAILURE);
	return -1;
}

void
ps_process_timeout(void *arg)
{
	struct dhcpcd_ctx *ctx = arg;

	logerrx("%s: timed out", __func__);
	eloop_exit(ctx->eloop, EXIT_FAILURE);
}

int
ps_stopprocess(struct ps_process *psp)
{
	int err = 0;

	if (psp == NULL)
		return 0;

	psp->psp_started = false;

#ifdef PRIVSEP_DEBUG
	logdebugx("%s: me=%d pid=%d fd=%d %s", __func__,
	    getpid(), psp->psp_pid, psp->psp_fd, psp->psp_name);
#endif

	if (psp->psp_fd != -1) {
		eloop_event_delete(psp->psp_ctx->eloop, psp->psp_fd);
#if 0
		if (ps_sendcmd(psp->psp_ctx, psp->psp_fd, PS_STOP, 0,
		    NULL, 0) == -1)
		{
			logerr("%d %d %s %s", getpid(), psp->psp_pid, psp->psp_name, __func__);
			err = -1;
		}
		shutdown(psp->psp_fd, SHUT_WR);
#else
		if (shutdown(psp->psp_fd, SHUT_WR) == -1) {
			logerr(__func__);
			err = -1;
		}
#endif
	}

	/* Don't wait for the process as it may not respond to the shutdown
	 * request. We'll reap the process on receipt of SIGCHLD where we
	 * also close the fd. */
	return err;
}

int
ps_start(struct dhcpcd_ctx *ctx)
{
	pid_t pid;

	TAILQ_INIT(&ctx->ps_processes);

	/* We need an inner eloop to block with. */
	if ((ctx->ps_eloop = eloop_new()) == NULL)
		return -1;
	eloop_signal_set_cb(ctx->ps_eloop,
	    dhcpcd_signals, dhcpcd_signals_len,
	    dhcpcd_signal_cb, ctx);

	switch (pid = ps_root_start(ctx)) {
	case -1:
		logerr("ps_root_start");
		return -1;
	case 0:
		return 0;
	default:
		logdebugx("spawned privileged proxy on PID %d", pid);
	}

	/* No point in spawning the generic network listener if we're
	 * not going to use it. */
	if (!ps_inet_canstart(ctx))
		goto started_net;

	switch (pid = ps_inet_start(ctx)) {
	case -1:
		return -1;
	case 0:
		return 0;
	default:
		logdebugx("spawned network proxy on PID %d", pid);
	}

started_net:
	if (!(ctx->options & DHCPCD_TEST)) {
		switch (pid = ps_ctl_start(ctx)) {
		case -1:
			return -1;
		case 0:
			return 0;
		default:
			logdebugx("spawned controller proxy on PID %d", pid);
		}
	}

#ifdef ARC4RANDOM_H
	/* Seed the random number generator early incase it needs /dev/urandom
	 * which won't be available in the chroot. */
	arc4random();
#endif

	return 1;
}

int
ps_entersandbox(const char *_pledge, const char **sandbox)
{

#if !defined(HAVE_PLEDGE)
	UNUSED(_pledge);
#endif

#if defined(HAVE_CAPSICUM)
	if (sandbox != NULL)
		*sandbox = "capsicum";
	return cap_enter();
#elif defined(HAVE_PLEDGE)
	if (sandbox != NULL)
		*sandbox = "pledge";
	// There is no need to use unveil(2) because we are in an empty chroot
	// This is encouraged by Theo de Raadt himself:
	// https://www.mail-archive.com/misc@openbsd.org/msg171655.html
	return pledge(_pledge, NULL);
#elif defined(HAVE_SECCOMP)
	if (sandbox != NULL)
		*sandbox = "seccomp";
	return ps_seccomp_enter();
#else
	if (sandbox != NULL)
		*sandbox = "posix resource limited";
	return 0;
#endif
}

int
ps_managersandbox(struct dhcpcd_ctx *ctx, const char *_pledge)
{
	const char *sandbox = NULL;
	bool forked;
	int dropped;

	forked = ctx->options & DHCPCD_FORKED;
	ctx->options &= ~DHCPCD_FORKED;
	dropped = ps_dropprivs(ctx);
	if (forked)
		ctx->options |= DHCPCD_FORKED;

	/*
	 * If we don't have a root process, we cannot use syslog.
	 * If it cannot be opened before chrooting then syslog(3) will fail.
	 * openlog(3) does not return an error which doubly sucks.
	 */
	if (ctx->ps_root == NULL) {
		unsigned int logopts = loggetopts();

		logopts &= ~LOGERR_LOG;
		logsetopts(logopts);
	}

	if (dropped == -1) {
		logerr("%s: ps_dropprivs", __func__);
		return -1;
	}

#ifdef PRIVSEP_RIGHTS
	if ((ctx->pf_inet_fd != -1 &&
	    ps_rights_limit_ioctl(ctx->pf_inet_fd) == -1) ||
	     ps_rights_limit_stdio() == -1)
	{
		logerr("%s: cap_rights_limit", __func__);
		return -1;
	}
#endif

	if (_pledge == NULL)
		_pledge = "stdio";
	if (ps_entersandbox(_pledge, &sandbox) == -1) {
		if (errno == ENOSYS) {
			if (sandbox != NULL)
				logwarnx("sandbox unavailable: %s", sandbox);
			return 0;
		}
		logerr("%s: %s", __func__, sandbox);
		return -1;
	} else if (ctx->options & DHCPCD_LAUNCHER ||
		  ((!(ctx->options & DHCPCD_DAEMONISE)) &&
		   ctx->options & DHCPCD_MANAGER))
		logdebugx("sandbox: %s", sandbox);
	return 0;
}

int
ps_stop(struct dhcpcd_ctx *ctx)
{
	int r, ret = 0;

	if (!(ctx->options & DHCPCD_PRIVSEP) ||
	    ctx->options & DHCPCD_FORKED ||
	    ctx->eloop == NULL)
		return 0;

	if (ctx->ps_ctl != NULL) {
		r = ps_ctl_stop(ctx);
		if (r != 0)
			ret = r;
	}

	if (ctx->ps_inet != NULL) {
		r = ps_inet_stop(ctx);
		if (r != 0)
			ret = r;
	}

	if (ctx->ps_root != NULL) {
		if (ps_root_stopprocesses(ctx) == -1)
			ret = -1;
	}

	return ret;
}

bool
ps_waitforprocs(struct dhcpcd_ctx *ctx)
{
	struct ps_process *psp = TAILQ_FIRST(&ctx->ps_processes);

	if (psp == NULL)
		return false;

	/* Different processes */
	if (psp != TAILQ_LAST(&ctx->ps_processes, ps_process_head))
		return true;

	return !psp->psp_started;
}

int
ps_stopwait(struct dhcpcd_ctx *ctx)
{
	int error = EXIT_SUCCESS;

	if (ctx->ps_eloop == NULL || !ps_waitforprocs(ctx))
		return 0;

	ctx->options |= DHCPCD_EXITING;
	if (eloop_timeout_add_sec(ctx->ps_eloop, PS_PROCESS_TIMEOUT,
	    ps_process_timeout, ctx) == -1)
		logerr("%s: eloop_timeout_add_sec", __func__);
	eloop_enter(ctx->ps_eloop);

#ifdef HAVE_CAPSICUM
	struct ps_process *psp;

	TAILQ_FOREACH(psp, &ctx->ps_processes, next) {
		if (psp->psp_pfd == -1)
			continue;
		if (eloop_event_add(ctx->ps_eloop, psp->psp_pfd,
		    ELE_HANGUP, ps_processhangup, psp) == -1)
			logerr("%s: eloop_event_add pfd %d",
			    __func__, psp->psp_pfd);
	}
#endif

	error = eloop_start(ctx->ps_eloop, &ctx->sigset);
	if (error != EXIT_SUCCESS)
		logerr("%s: eloop_start", __func__);

	eloop_timeout_delete(ctx->ps_eloop, ps_process_timeout, ctx);

	return error;
}

void
ps_freeprocess(struct ps_process *psp)
{
	struct dhcpcd_ctx *ctx = psp->psp_ctx;

	TAILQ_REMOVE(&ctx->ps_processes, psp, next);

	if (psp->psp_fd != -1) {
		eloop_event_delete(ctx->eloop, psp->psp_fd);
		close(psp->psp_fd);
	}
	if (psp->psp_work_fd != -1) {
		eloop_event_delete(ctx->eloop, psp->psp_work_fd);
		close(psp->psp_work_fd);
	}
#ifdef HAVE_CAPSICUM
	if (psp->psp_pfd != -1) {
		eloop_event_delete(ctx->eloop, psp->psp_pfd);
		if (ctx->ps_eloop != NULL)
			eloop_event_delete(ctx->ps_eloop, psp->psp_pfd);
		close(psp->psp_pfd);
	}
#endif
	if (ctx->ps_root == psp)
		ctx->ps_root = NULL;
	if (ctx->ps_inet == psp)
		ctx->ps_inet = NULL;
	if (ctx->ps_ctl == psp)
		ctx->ps_ctl = NULL;
#ifdef INET
	if (psp->psp_bpf != NULL)
		bpf_close(psp->psp_bpf);
#endif
	free(psp);
}

static void
ps_free(struct dhcpcd_ctx *ctx)
{
	struct ps_process *ppsp, *psp;
	bool stop;

	if (ctx->ps_root != NULL)
		ppsp = ctx->ps_root;
	else if (ctx->ps_ctl != NULL)
		ppsp = ctx->ps_ctl;
	else
		ppsp = NULL;
	if (ppsp != NULL)
		stop = ppsp->psp_pid == getpid();
	else
		stop = false;

	while ((psp = TAILQ_FIRST(&ctx->ps_processes)) != NULL) {
		if (stop && psp != ppsp)
			ps_stopprocess(psp);
		ps_freeprocess(psp);
	}
}

int
ps_unrollmsg(struct msghdr *msg, struct ps_msghdr *psm,
    const void *data, size_t len)
{
	uint8_t *datap, *namep, *controlp;
	socklen_t cmsg_padlen =
	    CALC_CMSG_PADLEN(psm->ps_controllen, psm->ps_namelen);

	namep = UNCONST(data);
	controlp = namep + psm->ps_namelen + cmsg_padlen;
	datap = controlp + psm->ps_controllen;

	if (psm->ps_namelen != 0) {
		if (psm->ps_namelen > len) {
			errno = EINVAL;
			return -1;
		}
		msg->msg_name = namep;
		len -= psm->ps_namelen;
	} else
		msg->msg_name = NULL;
	msg->msg_namelen = psm->ps_namelen;

	if (psm->ps_controllen != 0) {
		if (psm->ps_controllen > len) {
			errno = EINVAL;
			return -1;
		}
		msg->msg_control = controlp;
		len -= psm->ps_controllen + cmsg_padlen;
	} else
		msg->msg_control = NULL;
	msg->msg_controllen = psm->ps_controllen;

	if (len != 0) {
		msg->msg_iovlen = 1;
		msg->msg_iov[0].iov_base = datap;
		msg->msg_iov[0].iov_len = len;
	} else {
		msg->msg_iovlen = 0;
		msg->msg_iov[0].iov_base = NULL;
		msg->msg_iov[0].iov_len = 0;
	}
	return 0;
}

ssize_t
ps_sendpsmmsg(struct dhcpcd_ctx *ctx, int fd,
    struct ps_msghdr *psm, const struct msghdr *msg)
{
	long padding[1] = { 0 };
	struct iovec iov[] = {
		{ .iov_base = UNCONST(psm), .iov_len = sizeof(*psm) },
		{ .iov_base = NULL, },	/* name */
		{ .iov_base = NULL, },	/* control padding */
		{ .iov_base = NULL, },	/* control */
		{ .iov_base = NULL, },	/* payload 1 */
		{ .iov_base = NULL, },	/* payload 2 */
		{ .iov_base = NULL, },	/* payload 3 */
	};
	int iovlen;
	ssize_t len;

	if (msg != NULL) {
		struct iovec *iovp = &iov[1];
		int i;
		socklen_t cmsg_padlen;

		psm->ps_namelen = msg->msg_namelen;
		psm->ps_controllen = (socklen_t)msg->msg_controllen;

		iovp->iov_base = msg->msg_name;
		iovp->iov_len = msg->msg_namelen;
		iovp++;

		cmsg_padlen =
		    CALC_CMSG_PADLEN(msg->msg_controllen, msg->msg_namelen);
		assert(cmsg_padlen <= sizeof(padding));
		iovp->iov_len = cmsg_padlen;
		iovp->iov_base = cmsg_padlen != 0 ? padding : NULL;
		iovp++;

		iovp->iov_base = msg->msg_control;
		iovp->iov_len = msg->msg_controllen;
		iovlen = 4;

		for (i = 0; i < (int)msg->msg_iovlen; i++) {
			if ((size_t)(iovlen + i) > __arraycount(iov)) {
				errno =	ENOBUFS;
				return -1;
			}
			iovp++;
			iovp->iov_base = msg->msg_iov[i].iov_base;
			iovp->iov_len = msg->msg_iov[i].iov_len;
		}
		iovlen += i;
	} else
		iovlen = 1;

	len = writev(fd, iov, iovlen);
	if (len == -1) {
		if (ctx->options & DHCPCD_FORKED &&
		    !(ctx->options & DHCPCD_PRIVSEPROOT))
			eloop_exit(ctx->eloop, EXIT_FAILURE);
	}
	return len;
}

ssize_t
ps_sendpsmdata(struct dhcpcd_ctx *ctx, int fd,
    struct ps_msghdr *psm, const void *data, size_t len)
{
	struct iovec iov[] = {
		{ .iov_base = UNCONST(data), .iov_len = len },
	};
	struct msghdr msg = {
		.msg_iov = iov, .msg_iovlen = 1,
	};

	return ps_sendpsmmsg(ctx, fd, psm, &msg);
}


ssize_t
ps_sendmsg(struct dhcpcd_ctx *ctx, int fd, uint16_t cmd, unsigned long flags,
    const struct msghdr *msg)
{
	struct ps_msghdr psm = {
		.ps_cmd = cmd,
		.ps_flags = flags,
		.ps_namelen = msg->msg_namelen,
		.ps_controllen = (socklen_t)msg->msg_controllen,
	};
	size_t i;

	for (i = 0; i < (size_t)msg->msg_iovlen; i++)
		psm.ps_datalen += msg->msg_iov[i].iov_len;

#if 0	/* For debugging structure padding. */
	logerrx("psa.family %lu %zu", offsetof(struct ps_addr, psa_family), sizeof(psm.ps_id.psi_addr.psa_family));
	logerrx("psa.pad %lu %zu", offsetof(struct ps_addr, psa_pad), sizeof(psm.ps_id.psi_addr.psa_pad));
	logerrx("psa.psa_u %lu %zu", offsetof(struct ps_addr, psa_u), sizeof(psm.ps_id.psi_addr.psa_u));
	logerrx("psa %zu", sizeof(psm.ps_id.psi_addr));

	logerrx("psi.addr %lu %zu", offsetof(struct ps_id, psi_addr), sizeof(psm.ps_id.psi_addr));
	logerrx("psi.index %lu %zu", offsetof(struct ps_id, psi_ifindex), sizeof(psm.ps_id.psi_ifindex));
	logerrx("psi.cmd %lu %zu", offsetof(struct ps_id, psi_cmd), sizeof(psm.ps_id.psi_cmd));
	logerrx("psi.pad %lu %zu", offsetof(struct ps_id, psi_pad), sizeof(psm.ps_id.psi_pad));
	logerrx("psi %zu", sizeof(struct ps_id));

	logerrx("ps_cmd %lu", offsetof(struct ps_msghdr, ps_cmd));
	logerrx("ps_pad %lu %zu", offsetof(struct ps_msghdr, ps_pad), sizeof(psm.ps_pad));
	logerrx("ps_flags %lu %zu", offsetof(struct ps_msghdr, ps_flags), sizeof(psm.ps_flags));

	logerrx("ps_id %lu %zu", offsetof(struct ps_msghdr, ps_id), sizeof(psm.ps_id));

	logerrx("ps_namelen %lu %zu", offsetof(struct ps_msghdr, ps_namelen), sizeof(psm.ps_namelen));
	logerrx("ps_controllen %lu %zu", offsetof(struct ps_msghdr, ps_controllen), sizeof(psm.ps_controllen));
	logerrx("ps_pad2 %lu %zu", offsetof(struct ps_msghdr, ps_pad2), sizeof(psm.ps_pad2));
	logerrx("ps_datalen %lu %zu", offsetof(struct ps_msghdr, ps_datalen), sizeof(psm.ps_datalen));
	logerrx("psm %zu", sizeof(psm));
#endif

	return ps_sendpsmmsg(ctx, fd, &psm, msg);
}

ssize_t
ps_sendcmd(struct dhcpcd_ctx *ctx, int fd, uint16_t cmd, unsigned long flags,
    const void *data, size_t len)
{
	struct ps_msghdr psm = {
		.ps_cmd = cmd,
		.ps_flags = flags,
	};
	struct iovec iov[] = {
		{ .iov_base = UNCONST(data), .iov_len = len }
	};
	struct msghdr msg = {
		.msg_iov = iov, .msg_iovlen = 1,
	};

	return ps_sendpsmmsg(ctx, fd, &psm, &msg);
}

static ssize_t
ps_sendcmdmsg(int fd, uint16_t cmd, const struct msghdr *msg)
{
	struct ps_msghdr psm = { .ps_cmd = cmd };
	uint8_t data[PS_BUFLEN], *p = data;
	struct iovec iov[] = {
		{ .iov_base = &psm, .iov_len = sizeof(psm) },
		{ .iov_base = data, .iov_len = 0 },
	};
	size_t dl = sizeof(data);
	socklen_t cmsg_padlen =
	    CALC_CMSG_PADLEN(msg->msg_controllen, msg->msg_namelen);

	if (msg->msg_namelen != 0) {
		if (msg->msg_namelen > dl)
			goto nobufs;
		psm.ps_namelen = msg->msg_namelen;
		memcpy(p, msg->msg_name, msg->msg_namelen);
		p += msg->msg_namelen;
		dl -= msg->msg_namelen;
	}

	if (msg->msg_controllen != 0) {
		if (msg->msg_controllen + cmsg_padlen > dl)
			goto nobufs;
		if (cmsg_padlen != 0) {
			memset(p, 0, cmsg_padlen);
			p += cmsg_padlen;
			dl -= cmsg_padlen;
		}
		psm.ps_controllen = (socklen_t)msg->msg_controllen;
		memcpy(p, msg->msg_control, msg->msg_controllen);
		p += msg->msg_controllen;
		dl -= msg->msg_controllen;
	}

	psm.ps_datalen = msg->msg_iov[0].iov_len;
	if (psm.ps_datalen > dl)
		goto nobufs;

	iov[1].iov_len =
	    psm.ps_namelen + psm.ps_controllen + psm.ps_datalen + cmsg_padlen;
	if (psm.ps_datalen != 0)
		memcpy(p, msg->msg_iov[0].iov_base, psm.ps_datalen);
	return writev(fd, iov, __arraycount(iov));

nobufs:
	errno = ENOBUFS;
	return -1;
}

ssize_t
ps_recvmsg(int rfd, unsigned short events, uint16_t cmd, int wfd)
{
	struct sockaddr_storage ss = { .ss_family = AF_UNSPEC };
	uint8_t controlbuf[sizeof(struct sockaddr_storage)] = { 0 };
	uint8_t databuf[64 * 1024];
	struct iovec iov[] = {
	    { .iov_base = databuf, .iov_len = sizeof(databuf) }
	};
	struct msghdr msg = {
		.msg_name = &ss, .msg_namelen = sizeof(ss),
		.msg_control = controlbuf, .msg_controllen = sizeof(controlbuf),
		.msg_iov = iov, .msg_iovlen = 1,
	};
	ssize_t len;

	if (!(events & ELE_READ))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = recvmsg(rfd, &msg, 0);
	if (len == -1) {
		logerr("%s: recvmsg", __func__);
		return len;
	}

	iov[0].iov_len = (size_t)len;
	len = ps_sendcmdmsg(wfd, cmd, &msg);
	if (len == -1)
		logerr("%s: ps_sendcmdmsg", __func__);
	return len;
}

ssize_t
ps_daemonised(struct dhcpcd_ctx *ctx)
{
	struct ps_process *psp;
	ssize_t err = 0;

	dhcpcd_daemonised(ctx);

	/* Echo the message to all processes */
	TAILQ_FOREACH(psp, &ctx->ps_processes, next) {
		if (psp->psp_pid == getpid())
			continue;
		if (ps_sendcmd(psp->psp_ctx, psp->psp_fd, PS_DAEMONISED,
		    0, NULL, 0) == -1)
			err = -1;
	}

	return err;
}

ssize_t
ps_recvpsmsg(struct dhcpcd_ctx *ctx, int fd, unsigned short events,
    ssize_t (*callback)(void *, struct ps_msghdr *, struct msghdr *),
    void *cbctx)
{
	struct ps_msg psm;
	ssize_t len;
	size_t dlen;
	struct iovec iov[1];
	struct msghdr msg = { .msg_iov = iov, .msg_iovlen = 1 };
	bool stop = false;

	if (!(events & ELE_READ))
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	len = read(fd, &psm, sizeof(psm));
#ifdef PRIVSEP_DEBUG
	logdebugx("%s: %zd", __func__, len);
#endif

	if (len == -1 || len == 0)
		stop = true;
	else {
		dlen = (size_t)len;
		if (dlen < sizeof(psm.psm_hdr)) {
			errno = EINVAL;
			return -1;
		}

		if (psm.psm_hdr.ps_cmd == PS_STOP) {
			stop = true;
			len = 0;
		} else if (psm.psm_hdr.ps_cmd == PS_DAEMONISED) {
			ps_daemonised(ctx);
			return 0;
		}
	}

	if (stop) {
		ctx->options |= DHCPCD_EXITING;
#ifdef PRIVSEP_DEBUG
		logdebugx("process %d stopping", getpid());
#endif
		ps_free(ctx);
		eloop_exit(ctx->eloop, len != -1 ? EXIT_SUCCESS : EXIT_FAILURE);
		return len;
	}
	dlen -= sizeof(psm.psm_hdr);

	if (ps_unrollmsg(&msg, &psm.psm_hdr, psm.psm_data, dlen) == -1)
		return -1;

	if (callback == NULL)
		return 0;

	errno = 0;
	return callback(cbctx, &psm.psm_hdr, &msg);
}

struct ps_process *
ps_findprocess(struct dhcpcd_ctx *ctx, struct ps_id *psid)
{
	struct ps_process *psp;

	TAILQ_FOREACH(psp, &ctx->ps_processes, next) {
		if (!(psp->psp_started))
			continue;
		if (memcmp(&psp->psp_id, psid, sizeof(psp->psp_id)) == 0)
			return psp;
	}
	errno = ESRCH;
	return NULL;
}

struct ps_process *
ps_findprocesspid(struct dhcpcd_ctx *ctx, pid_t pid)
{
	struct ps_process *psp;

	TAILQ_FOREACH(psp, &ctx->ps_processes, next) {
		if (psp->psp_pid == pid)
			return psp;
	}
	errno = ESRCH;
	return NULL;
}

struct ps_process *
ps_newprocess(struct dhcpcd_ctx *ctx, struct ps_id *psid)
{
	struct ps_process *psp;

	psp = calloc(1, sizeof(*psp));
	if (psp == NULL)
		return NULL;
	psp->psp_ctx = ctx;
	memcpy(&psp->psp_id, psid, sizeof(psp->psp_id));
	psp->psp_fd = -1;
	psp->psp_work_fd = -1;
#ifdef HAVE_CAPSICUM
	psp->psp_pfd = -1;
#endif

	if (!(ctx->options & DHCPCD_MANAGER))
		strlcpy(psp->psp_ifname, ctx->ifv[0], sizeof(psp->psp_ifname));
	TAILQ_INSERT_TAIL(&ctx->ps_processes, psp, next);
	return psp;
}

void
ps_freeprocesses(struct dhcpcd_ctx *ctx, struct ps_process *notthis)
{
	struct ps_process *psp, *psn;

	TAILQ_FOREACH_SAFE(psp, &ctx->ps_processes, next, psn) {
		if (psp == notthis)
			continue;
		ps_freeprocess(psp);
	}
}
