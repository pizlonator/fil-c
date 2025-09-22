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

#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4ll.h"
#include "ipv6nd.h"
#include "logerr.h"
#include "privsep.h"
#include "script.h"

#define DEFAULT_PATH	"/usr/bin:/usr/sbin:/bin:/sbin"

static const char * const if_params[] = {
	"interface",
	"protocol",
	"reason",
	"pid",
	"ifcarrier",
	"ifmetric",
	"ifwireless",
	"ifflags",
	"ssid",
	"profile",
	"interface_order",
	NULL
};

static const char * true_str = "true";
static const char * false_str = "false";

void
if_printoptions(void)
{
	const char * const *p;

	for (p = if_params; *p; p++)
		printf(" -  %s\n", *p);
}

pid_t
script_exec(char *const *argv, char *const *env)
{
	pid_t pid = 0;
	posix_spawnattr_t attr;
	int r;
#ifdef USE_SIGNALS
	size_t i;
	short flags;
	sigset_t defsigs;
#else
	UNUSED(ctx);
#endif

	/* posix_spawn is a safe way of executing another image
	 * and changing signals back to how they should be. */
	if (posix_spawnattr_init(&attr) == -1)
		return -1;
#ifdef USE_SIGNALS
	flags = POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
	posix_spawnattr_setflags(&attr, flags);
	sigemptyset(&defsigs);
	posix_spawnattr_setsigmask(&attr, &defsigs);
	for (i = 0; i < dhcpcd_signals_len; i++)
		sigaddset(&defsigs, dhcpcd_signals[i]);
	for (i = 0; i < dhcpcd_signals_ignore_len; i++)
		sigaddset(&defsigs, dhcpcd_signals_ignore[i]);
	posix_spawnattr_setsigdefault(&attr, &defsigs);
#endif
	errno = 0;
	r = posix_spawn(&pid, argv[0], NULL, &attr, argv, env);
	posix_spawnattr_destroy(&attr);
	if (r) {
		errno = r;
		return -1;
	}
	return pid;
}

#ifdef INET
static int
append_config(FILE *fp, const char *prefix, const char *const *config)
{
	size_t i;

	if (config == NULL)
		return 0;

	/* Do we need to replace existing config rather than append? */
	for (i = 0; config[i] != NULL; i++) {
		if (efprintf(fp, "%s_%s", prefix, config[i]) == -1)
			return -1;
	}
	return 1;
}

#endif

#define	PROTO_LINK	0
#define	PROTO_DHCP	1
#define	PROTO_IPV4LL	2
#define	PROTO_RA	3
#define	PROTO_DHCP6	4
#define	PROTO_STATIC6	5
static const char *protocols[] = {
	"link",
	"dhcp",
	"ipv4ll",
	"ra",
	"dhcp6",
	"static6"
};

int
efprintf(FILE *fp, const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vfprintf(fp, fmt, args);
	va_end(args);
	if (r == -1)
		return -1;
	/* Write a trailing NULL so we can easily create env strings. */
	if (fputc('\0', fp) == EOF)
		return -1;
	return r;
}

char **
script_buftoenv(struct dhcpcd_ctx *ctx, char *buf, size_t len)
{
	char **env, **envp, *bufp, *endp;
	size_t nenv;

	/* Count the terminated env strings.
	 * Assert that the terminations are correct. */
	nenv = 0;
	endp = buf + len;
	for (bufp = buf; bufp < endp; bufp++) {
		if (*bufp == '\0') {
#ifndef NDEBUG
			if (bufp + 1 < endp)
				assert(*(bufp + 1) != '\0');
#endif
			nenv++;
		}
	}
	assert(*(bufp - 1) == '\0');
	if (nenv == 0)
		return NULL;

	if (ctx->script_envlen < nenv) {
		env = reallocarray(ctx->script_env, nenv + 1, sizeof(*env));
		if (env == NULL)
			return NULL;
		ctx->script_env = env;
		ctx->script_envlen = nenv;
	}

	bufp = buf;
	envp = ctx->script_env;
	*envp++ = bufp++;
	endp--; /* Avoid setting the last \0 to an invalid pointer */
	for (; bufp < endp; bufp++) {
		if (*bufp == '\0')
			*envp++ = bufp + 1;
	}
	*envp = NULL;

	return ctx->script_env;
}

static long
make_env(struct dhcpcd_ctx *ctx, const struct interface *ifp,
    const char *reason)
{
	FILE *fp;
	long buf_pos, i;
	char *path;
	int protocol = PROTO_LINK;
	const struct if_options *ifo;
	const struct interface *ifp2;
	int af;
	bool is_stdin = ifp->name[0] == '\0';
	const char *if_up, *if_down;
	rb_tree_t ifaces;
	struct rt *rt;
#ifdef INET
	const struct dhcp_state *state;
#ifdef IPV4LL
	const struct ipv4ll_state *istate;
#endif
#endif
#ifdef DHCP6
	const struct dhcp6_state *d6_state;
#endif

#ifdef HAVE_OPEN_MEMSTREAM
	if (ctx->script_fp == NULL) {
		fp = open_memstream(&ctx->script_buf, &ctx->script_buflen);
		if (fp == NULL)
			goto eexit;
		ctx->script_fp = fp;
	} else {
		fp = ctx->script_fp;
		rewind(fp);
	}
#else
	char tmpfile[] = "/tmp/dhcpcd-script-env-XXXXXX";
	int tmpfd;

	fp = NULL;
	tmpfd = mkstemp(tmpfile);
	if (tmpfd == -1) {
		logerr("%s: mkstemp", __func__);
		return -1;
	}
	unlink(tmpfile);
	fp = fdopen(tmpfd, "w+");
	if (fp == NULL) {
		close(tmpfd);
		goto eexit;
	}
#endif

	if (!(ifp->ctx->options & DHCPCD_DUMPLEASE)) {
		/* Needed for scripts */
		path = getenv("PATH");
		if (efprintf(fp, "PATH=%s",
		    path == NULL ? DEFAULT_PATH : path) == -1)
			goto eexit;
		if (efprintf(fp, "pid=%d", getpid()) == -1)
			goto eexit;
	}

	if (!is_stdin) {
		if (efprintf(fp, "reason=%s", reason) == -1)
			goto eexit;
	}

	ifo = ifp->options;
#ifdef INET
	state = D_STATE(ifp);
#ifdef IPV4LL
	istate = IPV4LL_CSTATE(ifp);
#endif
#endif
#ifdef DHCP6
	d6_state = D6_CSTATE(ifp);
#endif
	if (strcmp(reason, "TEST") == 0) {
		if (1 == 2) {
			/* This space left intentionally blank
			 * as all the below statements are optional. */
		}
#ifdef INET6
#ifdef DHCP6
		else if (d6_state && d6_state->new)
			protocol = PROTO_DHCP6;
#endif
		else if (ipv6nd_hasra(ifp))
			protocol = PROTO_RA;
#endif
#ifdef INET
#ifdef IPV4LL
		else if (istate && istate->addr != NULL)
			protocol = PROTO_IPV4LL;
#endif
		else
			protocol = PROTO_DHCP;
#endif
	}
#ifdef INET6
	else if (strcmp(reason, "STATIC6") == 0)
		protocol = PROTO_STATIC6;
#ifdef DHCP6
	else if (reason[strlen(reason) - 1] == '6')
		protocol = PROTO_DHCP6;
#endif
	else if (strcmp(reason, "ROUTERADVERT") == 0)
		protocol = PROTO_RA;
#endif
	else if (strcmp(reason, "PREINIT") == 0 ||
	    strcmp(reason, "CARRIER") == 0 ||
	    strcmp(reason, "NOCARRIER") == 0 ||
	    strcmp(reason, "NOCARRIER_ROAMING") == 0 ||
	    strcmp(reason, "UNKNOWN") == 0 ||
	    strcmp(reason, "DEPARTED") == 0 ||
	    strcmp(reason, "STOPPED") == 0)
		protocol = PROTO_LINK;
#ifdef INET
#ifdef IPV4LL
	else if (strcmp(reason, "IPV4LL") == 0)
		protocol = PROTO_IPV4LL;
#endif
	else
		protocol = PROTO_DHCP;
#endif

	if (!is_stdin) {
		if (efprintf(fp, "interface=%s", ifp->name) == -1)
			goto eexit;
		if (protocols[protocol] != NULL) {
			if (efprintf(fp, "protocol=%s",
			    protocols[protocol]) == -1)
				goto eexit;
		}
	}
	if (ifp->ctx->options & DHCPCD_DUMPLEASE && protocol != PROTO_LINK)
		goto dumplease;
	if (efprintf(fp, "if_configured=%s",
	    ifo->options & DHCPCD_CONFIGURE ? "true" : "false") == -1)
		goto eexit;
	if (efprintf(fp, "ifcarrier=%s",
	    ifp->carrier == LINK_UNKNOWN ? "unknown" :
	    ifp->carrier == LINK_UP ? "up" : "down") == -1)
		goto eexit;
	if (efprintf(fp, "ifmetric=%d", ifp->metric) == -1)
		goto eexit;
	if (efprintf(fp, "ifwireless=%d", ifp->wireless) == -1)
		goto eexit;
	if (efprintf(fp, "ifflags=%u", ifp->flags) == -1)
		goto eexit;
	if (efprintf(fp, "ifmtu=%d", if_getmtu(ifp)) == -1)
		goto eexit;
	if (ifp->wireless) {
		char pssid[IF_SSIDLEN * 4];

		if (print_string(pssid, sizeof(pssid), OT_ESCSTRING,
		    ifp->ssid, ifp->ssid_len) != -1)
		{
			if (efprintf(fp, "ifssid=%s", pssid) == -1)
				goto eexit;
		}
	}
	if (*ifp->profile != '\0') {
		if (efprintf(fp, "profile=%s", ifp->profile) == -1)
			goto eexit;
	}
	if (ifp->ctx->options & DHCPCD_DUMPLEASE)
		goto dumplease;

	ifp->ctx->rt_order = 0;
	rb_tree_init(&ifaces, &rt_compare_proto_ops);
	TAILQ_FOREACH(ifp2, ifp->ctx->ifaces, next) {
		if (!ifp2->active)
			continue;
		rt = rt_new(UNCONST(ifp2));
		if (rt == NULL)
			goto eexit;
		if (rt_proto_add(&ifaces, rt) != rt)
			goto eexit;
	}
	if (fprintf(fp, "interface_order=") == -1)
		goto eexit;
	RB_TREE_FOREACH(rt, &ifaces) {
		if (rt != RB_TREE_MIN(&ifaces) &&
		    fprintf(fp, "%s", " ") == -1)
			goto eexit;
		if (fprintf(fp, "%s", rt->rt_ifp->name) == -1)
			goto eexit;
	}
	rt_headclear(&ifaces, AF_UNSPEC);
	if (fputc('\0', fp) == EOF)
		goto eexit;

	if (strcmp(reason, "STOPPED") == 0) {
		if_up = false_str;
		if_down = ifo->options & DHCPCD_RELEASE ? true_str : false_str;
	} else if (strcmp(reason, "TEST") == 0 ||
	    strcmp(reason, "PREINIT") == 0 ||
	    strcmp(reason, "CARRIER") == 0 ||
	    strcmp(reason, "UNKNOWN") == 0)
	{
		if_up = false_str;
		if_down = false_str;
	} else if (strcmp(reason, "NOCARRIER") == 0) {
		if_up = false_str;
		if_down = true_str;
	} else if (strcmp(reason, "NOCARRIER_ROAMING") == 0) {
		if_up = true_str;
		if_down = false_str;
	} else if (1 == 2 /* appease ifdefs */
#ifdef INET
	    || (protocol == PROTO_DHCP && state && state->new)
#ifdef IPV4LL
	    || (protocol == PROTO_IPV4LL && IPV4LL_STATE_RUNNING(ifp))
#endif
#endif
#ifdef INET6
	    || (protocol == PROTO_STATIC6 && IPV6_STATE_RUNNING(ifp))
#ifdef DHCP6
	    || (protocol == PROTO_DHCP6 && d6_state && d6_state->new)
#endif
	    || (protocol == PROTO_RA && ipv6nd_hasra(ifp))
#endif
	    )
	{
		if_up = true_str;
		if_down = false_str;
	} else {
		if_up = false_str;
		if_down = true_str;
	}
	if (efprintf(fp, "if_up=%s", if_up) == -1)
		goto eexit;
	if (efprintf(fp, "if_down=%s", if_down) == -1)
		goto eexit;

	if ((af = dhcpcd_ifafwaiting(ifp)) != AF_MAX) {
		if (efprintf(fp, "if_afwaiting=%d", af) == -1)
			goto eexit;
	}
	if ((af = dhcpcd_afwaiting(ifp->ctx)) != AF_MAX) {
		TAILQ_FOREACH(ifp2, ifp->ctx->ifaces, next) {
			if ((af = dhcpcd_ifafwaiting(ifp2)) != AF_MAX)
				break;
		}
	}
	if (af != AF_MAX) {
		if (efprintf(fp, "af_waiting=%d", af) == -1)
			goto eexit;
	}
	if (ifo->options & DHCPCD_DEBUG) {
		if (efprintf(fp, "syslog_debug=true") == -1)
			goto eexit;
	}
#ifdef INET
	if (protocol == PROTO_DHCP && state && state->old) {
		if (dhcp_env(fp, "old", ifp,
		    state->old, state->old_len) == -1)
			goto eexit;
		if (append_config(fp, "old",
		    (const char *const *)ifo->config) == -1)
			goto eexit;
	}
#endif
#ifdef DHCP6
	if (protocol == PROTO_DHCP6 && d6_state && d6_state->old) {
		if (dhcp6_env(fp, "old", ifp,
		    d6_state->old, d6_state->old_len) == -1)
			goto eexit;
	}
#endif

dumplease:
#ifdef INET
#ifdef IPV4LL
	if (protocol == PROTO_IPV4LL && istate) {
		if (ipv4ll_env(fp, istate->down ? "old" : "new", ifp) == -1)
			goto eexit;
	}
#endif
	if (protocol == PROTO_DHCP && state && state->new) {
		if (dhcp_env(fp, "new", ifp,
		    state->new, state->new_len) == -1)
			goto eexit;
		if (append_config(fp, "new",
		    (const char *const *)ifo->config) == -1)
			goto eexit;
	}
#endif
#ifdef INET6
	if (protocol == PROTO_STATIC6) {
		if (ipv6_env(fp, "new", ifp) == -1)
			goto eexit;
	}
#ifdef DHCP6
	if (protocol == PROTO_DHCP6 && D6_STATE_RUNNING(ifp)) {
		if (dhcp6_env(fp, "new", ifp,
		    d6_state->new, d6_state->new_len) == -1)
			goto eexit;
	}
#endif
	if (protocol == PROTO_RA) {
		if (ipv6nd_env(fp, ifp) == -1)
			goto eexit;
	}
#endif

	/* Add our base environment */
	if (ifo->environ) {
		for (i = 0; ifo->environ[i] != NULL; i++)
			if (efprintf(fp, "%s", ifo->environ[i]) == -1)
				goto eexit;
	}

	/* Convert buffer to argv */
	fflush(fp);

	buf_pos = ftell(fp);
	if (buf_pos == -1) {
		logerr(__func__);
		goto eexit;
	}

#ifndef HAVE_OPEN_MEMSTREAM
	size_t buf_len = (size_t)buf_pos;
	if (ctx->script_buflen < buf_len) {
		char *buf = realloc(ctx->script_buf, buf_len);
		if (buf == NULL)
			goto eexit;
		ctx->script_buf = buf;
		ctx->script_buflen = buf_len;
	}
	rewind(fp);
	if (fread(ctx->script_buf, sizeof(char), buf_len, fp) != buf_len)
		goto eexit;
	fclose(fp);
	fp = NULL;
#endif

	if (is_stdin)
		return buf_pos;

	if (script_buftoenv(ctx, ctx->script_buf, (size_t)buf_pos) == NULL)
		goto eexit;

	return buf_pos;

eexit:
	logerr(__func__);
#ifndef HAVE_OPEN_MEMSTREAM
	if (fp != NULL)
		fclose(fp);
#endif
	return -1;
}

static int
send_interface1(struct fd_list *fd, const struct interface *ifp,
    const char *reason)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;
	long len;

	len = make_env(ifp->ctx, ifp, reason);
	if (len == -1)
		return -1;
	return control_queue(fd, ctx->script_buf, (size_t)len);
}

int
send_interface(struct fd_list *fd, const struct interface *ifp, int af)
{
	int retval = 0;
#ifdef INET
	const struct dhcp_state *d;
#endif
#ifdef DHCP6
	const struct dhcp6_state *d6;
#endif

#ifndef AF_LINK
#define	AF_LINK	AF_PACKET
#endif

	if (af == AF_UNSPEC || af == AF_LINK) {
		const char *reason;

		switch (ifp->carrier) {
		case LINK_UP:
			reason = "CARRIER";
			break;
		case LINK_DOWN:
			reason = "NOCARRIER";
			break;
		default:
			reason = "UNKNOWN";
			break;
		}
		if (fd != NULL) {
			if (send_interface1(fd, ifp, reason) == -1)
				retval = -1;
		} else
			retval++;
	}

#ifdef INET
	if (af == AF_UNSPEC || af == AF_INET) {
		if (D_STATE_RUNNING(ifp)) {
			d = D_CSTATE(ifp);
			if (fd != NULL) {
				if (send_interface1(fd, ifp, d->reason) == -1)
					retval = -1;
			} else
				retval++;
		}
#ifdef IPV4LL
		if (IPV4LL_STATE_RUNNING(ifp)) {
			if (fd != NULL) {
				if (send_interface1(fd, ifp, "IPV4LL") == -1)
					retval = -1;
			} else
				retval++;
		}
#endif
	}
#endif

#ifdef INET6
	if (af == AF_UNSPEC || af == AF_INET6) {
		if (IPV6_STATE_RUNNING(ifp)) {
			if (fd != NULL) {
				if (send_interface1(fd, ifp, "STATIC6") == -1)
					retval = -1;
			} else
				retval++;
		}
		if (RS_STATE_RUNNING(ifp)) {
			if (fd != NULL) {
				if (send_interface1(fd, ifp,
				    "ROUTERADVERT") == -1)
					retval = -1;
			} else
				retval++;
		}
#ifdef DHCP6
		if (D6_STATE_RUNNING(ifp)) {
			d6 = D6_CSTATE(ifp);
			if (fd != NULL) {
				if (send_interface1(fd, ifp, d6->reason) == -1)
					retval = -1;
			} else
				retval++;
		}
#endif
	}
#endif

	return retval;
}

static int
script_status(const char *script, int status)
{

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status))
			logerrx("%s: %s: WEXITSTATUS %d",
			    __func__, script, WEXITSTATUS(status));
	} else if (WIFSIGNALED(status))
		logerrx("%s: %s: %s",
		    __func__, script, strsignal(WTERMSIG(status)));

	return WEXITSTATUS(status);
}

static int
script_run(struct dhcpcd_ctx *ctx, char **argv)
{
	pid_t pid;
	int status;

	pid = script_exec(argv, ctx->script_env);
	if (pid == -1) {
		logerr("%s: %s", __func__, argv[0]);
		return -1;
	} else if (pid == 0)
		return 0;

	/* Wait for the script to finish */
	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR) {
			logerr("%s: waitpid", __func__);
			status = 0;
			break;
		}
	}
	return script_status(argv[0], status);
}

int
script_dump(const char *env, size_t len)
{
	const char *ep = env + len;

	if (len == 0)
		return 0;

	if (*(ep - 1) != '\0') {
		errno = EINVAL;
		return -1;
	}

	for (; env < ep; env += strlen(env) + 1) {
		if (strncmp(env, "new_", 4) == 0)
			env += 4;
		printf("%s\n", env);
	}
	return 0;
}

int
script_runreason(const struct interface *ifp, const char *reason)
{
	struct dhcpcd_ctx *ctx = ifp->ctx;
	char *argv[2];
	int status = 0;
	struct fd_list *fd;
	long buflen;

	if (ctx->script == NULL &&
	    TAILQ_FIRST(&ifp->ctx->control_fds) == NULL)
		return 0;

	/* Make our env */
	if ((buflen = make_env(ifp->ctx, ifp, reason)) == -1) {
		logerr(__func__);
		return -1;
	}

	if (strncmp(reason, "DUMP", 4) == 0)
		return script_dump(ctx->script_buf, (size_t)buflen);

	if (ctx->script == NULL)
		goto send_listeners;

	argv[0] = ctx->script;
	argv[1] = NULL;
	logdebugx("%s: executing: %s %s", ifp->name, argv[0], reason);

#ifdef PRIVSEP
	if (IN_PRIVSEP(ctx)) {
		ssize_t err;

		err = ps_root_script(ctx, ctx->script_buf, (size_t)buflen);
		if (err == -1)
			logerr(__func__);
		else
			script_status(ctx->script, (int)err);
		goto send_listeners;
	}
#endif

	script_run(ctx, argv);

send_listeners:
	/* Send to our listeners */
	status = 0;
	TAILQ_FOREACH(fd, &ctx->control_fds, next) {
		if (!(fd->flags & FD_LISTEN))
			continue;
		if (control_queue(fd, ctx->script_buf, ctx->script_buflen)== -1)
			logerr("%s: control_queue", __func__);
		else
			status = 1;
	}

	return status;
}
