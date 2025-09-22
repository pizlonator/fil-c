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

#include <sys/param.h>
#include <sys/types.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "dhcpcd-embedded.h"
#include "duid.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "logerr.h"
#include "sa.h"

#define	IN_CONFIG_BLOCK(ifo)	((ifo)->options & DHCPCD_FORKED)
#define	SET_CONFIG_BLOCK(ifo)	((ifo)->options |= DHCPCD_FORKED)
#define	CLEAR_CONFIG_BLOCK(ifo)	((ifo)->options &= ~DHCPCD_FORKED)

static unsigned long long default_options;

const struct option cf_options[] = {
	{"background",      no_argument,       NULL, 'b'},
	{"script",          required_argument, NULL, 'c'},
	{"debug",           no_argument,       NULL, 'd'},
	{"env",             required_argument, NULL, 'e'},
	{"config",          required_argument, NULL, 'f'},
	{"reconfigure",     no_argument,       NULL, 'g'},
	{"hostname",        optional_argument, NULL, 'h'},
	{"vendorclassid",   optional_argument, NULL, 'i'},
	{"logfile",         required_argument, NULL, 'j'},
	{"release",         no_argument,       NULL, 'k'},
	{"leasetime",       required_argument, NULL, 'l'},
	{"metric",          required_argument, NULL, 'm'},
	{"rebind",          no_argument,       NULL, 'n'},
	{"option",          required_argument, NULL, 'o'},
	{"persistent",      no_argument,       NULL, 'p'},
	{"quiet",           no_argument,       NULL, 'q'},
	{"request",         optional_argument, NULL, 'r'},
	{"inform",          optional_argument, NULL, 's'},
	{"inform6",         optional_argument, NULL, O_INFORM6},
	{"timeout",         required_argument, NULL, 't'},
	{"userclass",       required_argument, NULL, 'u'},
#ifndef SMALL
	{"msuserclass",     required_argument, NULL, O_MSUSERCLASS},
#endif
	{"vendor",          required_argument, NULL, 'v'},
	{"waitip",          optional_argument, NULL, 'w'},
	{"exit",            no_argument,       NULL, 'x'},
	{"allowinterfaces", required_argument, NULL, 'z'},
	{"reboot",          required_argument, NULL, 'y'},
	{"noarp",           no_argument,       NULL, 'A'},
	{"nobackground",    no_argument,       NULL, 'B'},
	{"nohook",          required_argument, NULL, 'C'},
	{"duid",            optional_argument, NULL, 'D'},
	{"lastlease",       no_argument,       NULL, 'E'},
	{"fqdn",            optional_argument, NULL, 'F'},
	{"nogateway",       no_argument,       NULL, 'G'},
	{"xidhwaddr",       no_argument,       NULL, 'H'},
	{"clientid",        optional_argument, NULL, 'I'},
	{"broadcast",       no_argument,       NULL, 'J'},
	{"nolink",          no_argument,       NULL, 'K'},
	{"noipv4ll",        no_argument,       NULL, 'L'},
	{"manager",         no_argument,       NULL, 'M'},
	{"renew",           no_argument,       NULL, 'N'},
	{"nooption",        required_argument, NULL, 'O'},
	{"printpidfile",    no_argument,       NULL, 'P'},
	{"require",         required_argument, NULL, 'Q'},
	{"static",          required_argument, NULL, 'S'},
	{"test",            no_argument,       NULL, 'T'},
	{"dumplease",       no_argument,       NULL, 'U'},
	{"variables",       no_argument,       NULL, 'V'},
	{"whitelist",       required_argument, NULL, 'W'},
	{"blacklist",       required_argument, NULL, 'X'},
	{"denyinterfaces",  required_argument, NULL, 'Z'},
	{"oneshot",         no_argument,       NULL, '1'},
	{"ipv4only",        no_argument,       NULL, '4'},
	{"ipv6only",        no_argument,       NULL, '6'},
	{"anonymous",       no_argument,       NULL, O_ANONYMOUS},
	{"randomise_hwaddr",no_argument,       NULL, O_RANDOMISE_HWADDR},
	{"arping",          required_argument, NULL, O_ARPING},
	{"destination",     required_argument, NULL, O_DESTINATION},
	{"fallback",        required_argument, NULL, O_FALLBACK},
	{"ipv6rs",          no_argument,       NULL, O_IPV6RS},
	{"noipv6rs",        no_argument,       NULL, O_NOIPV6RS},
	{"ipv6ra_autoconf", no_argument,       NULL, O_IPV6RA_AUTOCONF},
	{"ipv6ra_noautoconf", no_argument,     NULL, O_IPV6RA_NOAUTOCONF},
	{"ipv6ra_fork",     no_argument,       NULL, O_IPV6RA_FORK},
	{"ipv4",            no_argument,       NULL, O_IPV4},
	{"noipv4",          no_argument,       NULL, O_NOIPV4},
	{"ipv6",            no_argument,       NULL, O_IPV6},
	{"noipv6",          no_argument,       NULL, O_NOIPV6},
	{"noalias",         no_argument,       NULL, O_NOALIAS},
	{"iaid",            required_argument, NULL, O_IAID},
	{"ia_na",           optional_argument, NULL, O_IA_NA},
	{"ia_ta",           optional_argument, NULL, O_IA_TA},
	{"ia_pd",           optional_argument, NULL, O_IA_PD},
	{"hostname_short",  no_argument,       NULL, O_HOSTNAME_SHORT},
	{"dev",             required_argument, NULL, O_DEV},
	{"nodev",           no_argument,       NULL, O_NODEV},
	{"define",          required_argument, NULL, O_DEFINE},
	{"definend",        required_argument, NULL, O_DEFINEND},
	{"define6",         required_argument, NULL, O_DEFINE6},
	{"embed",           required_argument, NULL, O_EMBED},
	{"encap",           required_argument, NULL, O_ENCAP},
	{"vendopt",         required_argument, NULL, O_VENDOPT},
	{"vendclass",       required_argument, NULL, O_VENDCLASS},
	{"authprotocol",    required_argument, NULL, O_AUTHPROTOCOL},
	{"authtoken",       required_argument, NULL, O_AUTHTOKEN},
	{"noauthrequired",  no_argument,       NULL, O_AUTHNOTREQUIRED},
	{"dhcp",            no_argument,       NULL, O_DHCP},
	{"nodhcp",          no_argument,       NULL, O_NODHCP},
	{"dhcp6",           no_argument,       NULL, O_DHCP6},
	{"nodhcp6",         no_argument,       NULL, O_NODHCP6},
	{"controlgroup",    required_argument, NULL, O_CONTROLGRP},
	{"slaac",           required_argument, NULL, O_SLAAC},
	{"gateway",         no_argument,       NULL, O_GATEWAY},
	{"reject",          required_argument, NULL, O_REJECT},
	{"bootp",           no_argument,       NULL, O_BOOTP},
	{"nodelay",         no_argument,       NULL, O_NODELAY},
	{"noup",            no_argument,       NULL, O_NOUP},
	{"lastleaseextend", no_argument,       NULL, O_LASTLEASE_EXTEND},
	{"inactive",        no_argument,       NULL, O_INACTIVE},
	{"mudurl",          required_argument, NULL, O_MUDURL},
	{"link_rcvbuf",     required_argument, NULL, O_LINK_RCVBUF},
	{"configure",       no_argument,       NULL, O_CONFIGURE},
	{"noconfigure",     no_argument,       NULL, O_NOCONFIGURE},
	{"arp_persistdefence", no_argument,    NULL, O_ARP_PERSISTDEFENCE},
	{"request_time",    required_argument, NULL, O_REQUEST_TIME},
	{"fallback_time",   required_argument, NULL, O_FALLBACK_TIME},
	{"ipv4ll_time",     required_argument, NULL, O_IPV4LL_TIME},
	{NULL,              0,                 NULL, '\0'}
};

static char *
add_environ(char ***array, const char *value, int uniq)
{
	char **newlist, **list = *array;
	size_t i = 0, l, lv;
	char *match = NULL, *p, *n;

	match = strdup(value);
	if (match == NULL) {
		logerr(__func__);
		return NULL;
	}
	p = strchr(match, '=');
	if (p == NULL) {
		logerrx("%s: no assignment: %s", __func__, value);
		free(match);
		return NULL;
	}
	*p++ = '\0';
	l = strlen(match);

	while (list && list[i]) {
		/* We know that it must contain '=' due to the above test */
		size_t listl = (size_t)(strchr(list[i], '=') - list[i]);

		if (l == listl && strncmp(list[i], match, l) == 0) {
			if (uniq) {
				n = strdup(value);
				if (n == NULL) {
					logerr(__func__);
					free(match);
					return NULL;
				}
				free(list[i]);
				list[i] = n;
			} else {
				/* Append a space and the value to it */
				l = strlen(list[i]);
				lv = strlen(p);
				n = realloc(list[i], l + lv + 2);
				if (n == NULL) {
					logerr(__func__);
					free(match);
					return NULL;
				}
				list[i] = n;
				list[i][l] = ' ';
				memcpy(list[i] + l + 1, p, lv);
				list[i][l + lv + 1] = '\0';
			}
			free(match);
			return list[i];
		}
		i++;
	}

	free(match);
	n = strdup(value);
	if (n == NULL) {
		logerr(__func__);
		return NULL;
	}
	newlist = reallocarray(list, i + 2, sizeof(char *));
	if (newlist == NULL) {
		logerr(__func__);
		free(n);
		return NULL;
	}
	newlist[i] = n;
	newlist[i + 1] = NULL;
	*array = newlist;
	return newlist[i];
}

#define PARSE_STRING		0
#define PARSE_STRING_NULL	1
#define PARSE_HWADDR		2
#define parse_string(a, b, c) parse_str((a), (b), (c), PARSE_STRING)
#define parse_nstring(a, b, c) parse_str((a), (b), (c), PARSE_STRING_NULL)
#define parse_hwaddr(a, b, c) parse_str((a), (b), (c), PARSE_HWADDR)
static ssize_t
parse_str(char *sbuf, size_t slen, const char *str, int flags)
{
	size_t l;
	const char *p, *end;
	int i;
	char c[4], cmd;

	end = str + strlen(str);
	/* If surrounded by quotes then it's a string */
	if (*str == '"') {
		p = end - 1;
		if (*p == '"') {
			str++;
			end = p;
		}
	} else {
		l = (size_t)hwaddr_aton(NULL, str);
		if (l > 0) {
			if ((ssize_t)l == -1) {
				errno = ENOBUFS;
				return -1;
			}
			if (sbuf == NULL)
				return (ssize_t)l;
			if (l > slen) {
				errno = ENOBUFS;
				return -1;
			}
			hwaddr_aton((uint8_t *)sbuf, str);
			return (ssize_t)l;
		}
	}

	/* Process escapes */
	l = 0;
	/* If processing a string on the clientid, first byte should be
	 * 0 to indicate a non hardware type */
	if (flags == PARSE_HWADDR && *str) {
		if (sbuf)
			*sbuf++ = 0;
		l++;
	}
	c[3] = '\0';
	while (str < end) {
		if (++l > slen && sbuf) {
			errno = ENOBUFS;
			return -1;
		}
		if (*str == '\\') {
			str++;
			switch((cmd = *str++)) {
			case '\0':
				str--;
				break;
			case 'b':
				if (sbuf)
					*sbuf++ = '\b';
				break;
			case 'n':
				if (sbuf)
					*sbuf++ = '\n';
				break;
			case 'r':
				if (sbuf)
					*sbuf++ = '\r';
				break;
			case 't':
				if (sbuf)
					*sbuf++ = '\t';
				break;
			case 'x':
				/* Grab a hex code */
				c[1] = '\0';
				for (i = 0; i < 2; i++) {
					if (isxdigit((unsigned char)*str) == 0)
						break;
					c[i] = *str++;
				}
				if (c[1] != '\0') {
					c[2] = '\0';
					if (sbuf)
						*sbuf++ = (char)strtol(c, NULL, 16);
				} else
					l--;
				break;
			case '0':
				/* Grab an octal code */
				c[2] = '\0';
				for (i = 0; i < 3; i++) {
					if (*str < '0' || *str > '7')
						break;
					c[i] = *str++;
				}
				if (c[2] != '\0') {
					i = (int)strtol(c, NULL, 8);
					if (i > 255)
						i = 255;
					if (sbuf)
						*sbuf++ = (char)i;
				} else
					l--;
				break;
			default:
				if (sbuf)
					*sbuf++ = cmd;
				break;
			}
		} else {
			if (sbuf)
				*sbuf++ = *str;
			str++;
		}
	}
	if (flags == PARSE_STRING_NULL) {
		l++;
		if (sbuf != NULL) {
			if (l > slen) {
				errno = ENOBUFS;
				return -1;
			}
			*sbuf = '\0';
		}
	}
	return (ssize_t)l;
}

static int
parse_iaid1(uint8_t *iaid, const char *arg, size_t len, int n)
{
	int e;
	uint32_t narg;
	ssize_t s;

	narg = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
	if (e == 0) {
		if (n)
			narg = htonl(narg);
		memcpy(iaid, &narg, sizeof(narg));
		return 0;
	}

	if ((s = parse_string((char *)iaid, len, arg)) < 1)
		return -1;
	if (s < 4)
		iaid[3] = '\0';
	if (s < 3)
		iaid[2] = '\0';
	if (s < 2)
		iaid[1] = '\0';
	return 0;
}

static int
parse_iaid(uint8_t *iaid, const char *arg, size_t len)
{

	return parse_iaid1(iaid, arg, len, 1);
}

#ifdef AUTH
static int
parse_uint32(uint32_t *i, const char *arg)
{

	return parse_iaid1((uint8_t *)i, arg, sizeof(uint32_t), 0);
}
#endif

static char **
splitv(int *argc, char **argv, const char *arg)
{
	char **n, **v = argv;
	char *o = strdup(arg), *p, *t, *nt;

	if (o == NULL) {
		logerr(__func__);
		return v;
	}
	p = o;
	while ((t = strsep(&p, ", "))) {
		nt = strdup(t);
		if (nt == NULL) {
			logerr(__func__);
			free(o);
			return v;
		}
		n = reallocarray(v, (size_t)(*argc) + 1, sizeof(char *));
		if (n == NULL) {
			logerr(__func__);
			free(o);
			free(nt);
			return v;
		}
		v = n;
		v[(*argc)++] = nt;
	}
	free(o);
	return v;
}

#ifdef INET
static int
parse_addr(struct in_addr *addr, struct in_addr *net, const char *arg)
{
	char *p;

	if (arg == NULL || *arg == '\0') {
		if (addr != NULL)
			addr->s_addr = 0;
		if (net != NULL)
			net->s_addr = 0;
		return 0;
	}
	if ((p = strchr(arg, '/')) != NULL) {
		int e;
		intmax_t i;

		*p++ = '\0';
		i = strtoi(p, NULL, 10, 0, 32, &e);
		if (e != 0 ||
		    (net != NULL && inet_cidrtoaddr((int)i, net) != 0))
		{
			logerrx("invalid CIDR: %s", p);
			return -1;
		}
	}

	if (addr != NULL && inet_aton(arg, addr) == 0) {
		logerrx("invalid IP address: %s", arg);
		return -1;
	}
	if (p != NULL)
		*--p = '/';
	else if (net != NULL && addr != NULL)
		net->s_addr = ipv4_getnetmask(addr->s_addr);
	return 0;
}
#else
static int
parse_addr(__unused struct in_addr *addr, __unused struct in_addr *net,
    __unused const char *arg)
{

	logerrx("No IPv4 support");
	return -1;
}
#endif

static void
set_option_space(struct dhcpcd_ctx *ctx,
    const char *arg,
    const struct dhcp_opt **d, size_t *dl,
    const struct dhcp_opt **od, size_t *odl,
    struct if_options *ifo,
    uint8_t *request[], uint8_t *require[], uint8_t *no[], uint8_t *reject[])
{

#if !defined(INET) && !defined(INET6)
	UNUSED(ctx);
#endif

#ifdef INET6
	if (strncmp(arg, "nd_", strlen("nd_")) == 0) {
		*d = ctx->nd_opts;
		*dl = ctx->nd_opts_len;
		*od = ifo->nd_override;
		*odl = ifo->nd_override_len;
		*request = ifo->requestmasknd;
		*require = ifo->requiremasknd;
		*no = ifo->nomasknd;
		*reject = ifo->rejectmasknd;
		return;
	}

#ifdef DHCP6
	if (strncmp(arg, "dhcp6_", strlen("dhcp6_")) == 0) {
		*d = ctx->dhcp6_opts;
		*dl = ctx->dhcp6_opts_len;
		*od = ifo->dhcp6_override;
		*odl = ifo->dhcp6_override_len;
		*request = ifo->requestmask6;
		*require = ifo->requiremask6;
		*no = ifo->nomask6;
		*reject = ifo->rejectmask6;
		return;
	}
#endif
#else
	UNUSED(arg);
#endif

#ifdef INET
	*d = ctx->dhcp_opts;
	*dl = ctx->dhcp_opts_len;
	*od = ifo->dhcp_override;
	*odl = ifo->dhcp_override_len;
#else
	*d = NULL;
	*dl = 0;
	*od = NULL;
	*odl = 0;
#endif
	*request = ifo->requestmask;
	*require = ifo->requiremask;
	*no = ifo->nomask;
	*reject = ifo->rejectmask;
}

void
free_dhcp_opt_embenc(struct dhcp_opt *opt)
{
	size_t i;
	struct dhcp_opt *o;

	free(opt->var);

	for (i = 0, o = opt->embopts; i < opt->embopts_len; i++, o++)
		free_dhcp_opt_embenc(o);
	free(opt->embopts);
	opt->embopts_len = 0;
	opt->embopts = NULL;

	for (i = 0, o = opt->encopts; i < opt->encopts_len; i++, o++)
		free_dhcp_opt_embenc(o);
	free(opt->encopts);
	opt->encopts_len = 0;
	opt->encopts = NULL;
}

static char *
strwhite(const char *s)
{

	if (s == NULL)
		return NULL;
	while (*s != ' ' && *s != '\t') {
		if (*s == '\0')
			return NULL;
		s++;
	}
	return UNCONST(s);
}

static char *
strskipwhite(const char *s)
{

	if (s == NULL || *s == '\0')
		return NULL;
	while (*s == ' ' || *s == '\t') {
		s++;
		if (*s == '\0')
			return NULL;
	}
	return UNCONST(s);
}

#ifdef AUTH
/* Find the end pointer of a string. */
static char *
strend(const char *s)
{

	s = strskipwhite(s);
	if (s == NULL)
		return NULL;
	if (*s != '"')
		return strchr(s, ' ');
	s++;
	for (; *s != '"' ; s++) {
		if (*s == '\0')
			return NULL;
		if (*s == '\\') {
			if (*(++s) == '\0')
				return NULL;
		}
	}
	return UNCONST(++s);
}
#endif

static int
parse_option(struct dhcpcd_ctx *ctx, const char *ifname, struct if_options *ifo,
    int opt, const char *arg, struct dhcp_opt **ldop, struct dhcp_opt **edop)
{
	int e, i, t;
	long l;
	unsigned long u;
	char *p = NULL, *bp, *fp, *np;
	ssize_t s;
	struct in_addr addr, addr2;
	in_addr_t *naddr;
	struct rt *rt;
	const struct dhcp_opt *d, *od;
	uint8_t *request, *require, *no, *reject;
	struct dhcp_opt **dop, *ndop;
	size_t *dop_len, dl, odl;
	struct vivco *vivco;
	struct group *grp;
#ifdef AUTH
	struct token *token;
#endif
#ifdef _REENTRANT
	struct group grpbuf;
#endif
#ifdef DHCP6
	size_t sl;
	struct if_ia *ia;
	uint8_t iaid[4];
#ifndef SMALL
	struct if_sla *sla, *slap;
#endif
#endif

	dop = NULL;
	dop_len = NULL;
#ifdef INET6
	i = 0;
#endif

/* Add a guard for static analysers.
 * This should not be needed really because of the argument_required option
 * in the options declaration above. */
#define ARG_REQUIRED if (arg == NULL) goto arg_required

	switch(opt) {
	case 'f': /* FALLTHROUGH */
	case 'g': /* FALLTHROUGH */
	case 'n': /* FALLTHROUGH */
	case 'q': /* FALLTHROUGH */
	case 'x': /* FALLTHROUGH */
	case 'N': /* FALLTHROUGH */
	case 'P': /* FALLTHROUGH */
	case 'T': /* FALLTHROUGH */
	case 'U': /* FALLTHROUGH */
	case 'V': /* We need to handle non interface options */
		break;
	case 'b':
		ifo->options |= DHCPCD_BACKGROUND;
		break;
	case 'c':
		ARG_REQUIRED;
		if (IN_CONFIG_BLOCK(ifo)) {
			logerrx("%s: per interface scripts"
			    " are no longer supported",
			    ifname);
			return -1;
		}
		if (ctx->script != dhcpcd_default_script)
			free(ctx->script);
		s = parse_nstring(NULL, 0, arg);
		if (s == 0) {
			ctx->script = NULL;
			break;
		}
		dl = (size_t)s;
		if (s == -1 || (ctx->script = malloc(dl)) == NULL) {
			ctx->script = NULL;
			logerr(__func__);
			return -1;
		}
		s = parse_nstring(ctx->script, dl, arg);
		if (s == -1 ||
		    ctx->script[0] == '\0' ||
		    strcmp(ctx->script, "/dev/null") == 0)
		{
			free(ctx->script);
			ctx->script = NULL;
		}
		break;
	case 'd':
		ifo->options |= DHCPCD_DEBUG;
		break;
	case 'e':
		ARG_REQUIRED;
		add_environ(&ifo->environ, arg, 1);
		break;
	case 'h':
		if (!arg) {
			ifo->options |= DHCPCD_HOSTNAME;
			break;
		}
		s = parse_nstring(ifo->hostname, sizeof(ifo->hostname), arg);
		if (s == -1) {
			logerr("%s: hostname", __func__);
			return -1;
		}
		if (s != 0 && ifo->hostname[0] == '.') {
			logerrx("hostname cannot begin with .");
			return -1;
		}
		if (ifo->hostname[0] == '\0')
			ifo->options &= ~DHCPCD_HOSTNAME;
		else
			ifo->options |= DHCPCD_HOSTNAME;
		break;
	case 'i':
		if (arg)
			s = parse_string((char *)ifo->vendorclassid + 1,
			    VENDORCLASSID_MAX_LEN, arg);
		else
			s = 0;
		if (s == -1) {
			logerr("vendorclassid");
			return -1;
		}
		*ifo->vendorclassid = (uint8_t)s;
		break;
	case 'j':
		ARG_REQUIRED;
		/* per interface logging is not supported
		 * don't want to overide the commandline */
		if (!IN_CONFIG_BLOCK(ifo) && ctx->logfile == NULL) {
			logclose();
			ctx->logfile = strdup(arg);
			logopen(ctx->logfile);
		}
		break;
	case 'k':
		ifo->options |= DHCPCD_RELEASE;
		break;
	case 'l':
		ARG_REQUIRED;
		if (strcmp(arg, "-1") == 0) {
			ifo->leasetime = DHCP_INFINITE_LIFETIME;
			break;
		}
		ifo->leasetime = (uint32_t)strtou(arg, NULL,
		    0, 0, UINT32_MAX, &e);
		if (e) {
			logerrx("failed to convert leasetime %s", arg);
			return -1;
		}
		break;
	case 'm':
		ARG_REQUIRED;
		ifo->metric = (int)strtoi(arg, NULL, 0, 0, INT32_MAX, &e);
		if (e) {
			logerrx("failed to convert metric %s", arg);
			return -1;
		}
		break;
	case 'o':
		ARG_REQUIRED;
		if (ctx->options & DHCPCD_PRINT_PIDFILE)
			break;
		set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, request, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, no, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, reject, arg, -1) != 0)
		{
			logerrx("unknown option: %s", arg);
			return -1;
		}
		break;
	case O_REJECT:
		ARG_REQUIRED;
		if (ctx->options & DHCPCD_PRINT_PIDFILE)
			break;
		set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, reject, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, request, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, require, arg, -1) != 0)
		{
			logerrx("unknown option: %s", arg);
			return -1;
		}
		break;
	case 'p':
		ifo->options |= DHCPCD_PERSISTENT;
		break;
	case 'r':
		if (parse_addr(&ifo->req_addr, NULL, arg) != 0)
			return -1;
		ifo->options |= DHCPCD_REQUEST;
		ifo->req_mask.s_addr = 0;
		break;
	case 's':
		if (arg && *arg != '\0') {
			/* Strip out a broadcast address */
			p = strchr(arg, '/');
			if (p != NULL) {
				p = strchr(p + 1, '/');
				if (p != NULL)
					*p = '\0';
			}
			i = parse_addr(&ifo->req_addr, &ifo->req_mask, arg);
			if (p != NULL) {
				/* Ensure the original string is preserved */
				*p++ = '/';
				if (i == 0)
					i = parse_addr(&ifo->req_brd, NULL, p);
			}
			if (i != 0)
				return -1;
		} else {
			ifo->req_addr.s_addr = 0;
			ifo->req_mask.s_addr = 0;
		}
		ifo->options |= DHCPCD_INFORM | DHCPCD_PERSISTENT;
		ifo->options &= ~DHCPCD_STATIC;
		break;
	case O_INFORM6:
		ifo->options |= DHCPCD_INFORM6;
		break;
	case 't':
		ARG_REQUIRED;
		ifo->timeout = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logerrx("failed to convert timeout %s", arg);
			return -1;
		}
		break;
	case 'u':
		dl = sizeof(ifo->userclass) - ifo->userclass[0] - 1;
		s = parse_string((char *)ifo->userclass +
		    ifo->userclass[0] + 2, dl, arg);
		if (s == -1) {
			logerr("userclass");
			return -1;
		}
		if (s != 0) {
			ifo->userclass[ifo->userclass[0] + 1] = (uint8_t)s;
			ifo->userclass[0] = (uint8_t)(ifo->userclass[0] + s +1);
		}
		break;
#ifndef SMALL
	case O_MSUSERCLASS:
		/* Some Microsoft DHCP servers expect userclass to be an
		 * opaque blob. This is not RFC 3004 compliant. */
		s = parse_string((char *)ifo->userclass + 1,
		    sizeof(ifo->userclass) - 1, arg);
		if (s == -1) {
			logerr("msuserclass");
			return -1;
		}
		ifo->userclass[0] = (uint8_t)s;
		break;
#endif
	case 'v':
		ARG_REQUIRED;
		p = strchr(arg, ',');
		if (!p || !p[1]) {
			logerrx("invalid vendor format: %s", arg);
			return -1;
		}

		/* If vendor starts with , then it is not encapsulated */
		if (p == arg) {
			arg++;
			s = parse_string((char *)ifo->vendor + 1,
			    VENDOR_MAX_LEN, arg);
			if (s == -1) {
				logerr("vendor");
				return -1;
			}
			ifo->vendor[0] = (uint8_t)s;
			ifo->options |= DHCPCD_VENDORRAW;
			break;
		}

		/* Encapsulated vendor options */
		if (ifo->options & DHCPCD_VENDORRAW) {
			ifo->options &= ~DHCPCD_VENDORRAW;
			ifo->vendor[0] = 0;
		}

		/* Strip and preserve the comma */
		*p = '\0';
		i = (int)strtoi(arg, NULL, 0, 1, 254, &e);
		*p = ',';
		if (e) {
			logerrx("vendor option should be between"
			    " 1 and 254 inclusive");
			return -1;
		}

		arg = p + 1;
		s = VENDOR_MAX_LEN - ifo->vendor[0] - 2;
		if (inet_aton(arg, &addr) == 1) {
			if (s < 6) {
				s = -1;
				errno = ENOBUFS;
			} else {
				memcpy(ifo->vendor + ifo->vendor[0] + 3,
				    &addr.s_addr, sizeof(addr.s_addr));
				s = sizeof(addr.s_addr);
			}
		} else {
			s = parse_string((char *)ifo->vendor +
			    ifo->vendor[0] + 3, (size_t)s, arg);
		}
		if (s == -1) {
			logerr("vendor");
			return -1;
		}
		if (s != 0) {
			ifo->vendor[ifo->vendor[0] + 1] = (uint8_t)i;
			ifo->vendor[ifo->vendor[0] + 2] = (uint8_t)s;
			ifo->vendor[0] = (uint8_t)(ifo->vendor[0] + s + 2);
		}
		break;
	case 'w':
		ifo->options |= DHCPCD_WAITIP;
		p = UNCONST(arg);
		// Generally it's --waitip=46, but some expect
		// --waitip="4 6" to work as well.
		// It's easier to allow it rather than have confusing docs.
		while (p != NULL && p[0] != '\0') {
			if (p[0] == '4' || p[1] == '4')
				ifo->options |= DHCPCD_WAITIP4;
			if (p[0] == '6' || p[1] == '6')
				ifo->options |= DHCPCD_WAITIP6;
			p = strskipwhite(++p);
		}
		break;
	case 'y':
		ARG_REQUIRED;
		ifo->reboot = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logerr("failed to convert reboot %s", arg);
			return -1;
		}
		break;
	case 'z':
		ARG_REQUIRED;
		if (!IN_CONFIG_BLOCK(ifo))
			ctx->ifav = splitv(&ctx->ifac, ctx->ifav, arg);
		break;
	case 'A':
		ifo->options &= ~DHCPCD_ARP;
		/* IPv4LL requires ARP */
		ifo->options &= ~DHCPCD_IPV4LL;
		break;
	case 'B':
		ifo->options &= ~DHCPCD_DAEMONISE;
		break;
	case 'C':
		ARG_REQUIRED;
		/* Commas to spaces for shell */
		while ((p = strchr(arg, ',')))
			*p = ' ';
		dl = strlen("skip_hooks=") + strlen(arg) + 1;
		p = malloc(sizeof(char) * dl);
		if (p == NULL) {
			logerr(__func__);
			return -1;
		}
		snprintf(p, dl, "skip_hooks=%s", arg);
		add_environ(&ifo->environ, p, 0);
		free(p);
		break;
	case 'D':
		ifo->options |= DHCPCD_CLIENTID | DHCPCD_DUID;
		if (ifname != NULL) /* duid type only a global option */
			break;
		if (arg == NULL)
			ctx->duid_type = DUID_DEFAULT;
		else if (strcmp(arg, "ll") == 0)
			ctx->duid_type = DUID_LL;
		else if (strcmp(arg, "llt") == 0)
			ctx->duid_type = DUID_LLT;
		else if (strcmp(arg, "uuid") == 0)
			ctx->duid_type = DUID_UUID;
		else {
			dl = hwaddr_aton(NULL, arg);
			if (dl != 0) {
				no = realloc(ctx->duid, dl);
				if (no == NULL)
					logerrx(__func__);
				else {
					ctx->duid = no;
					ctx->duid_len = hwaddr_aton(no, arg);
				}
			}
		}
		break;
	case 'E':
		ifo->options |= DHCPCD_LASTLEASE;
		break;
	case 'F':
		if (!arg) {
			ifo->fqdn = FQDN_BOTH;
			break;
		}
		if (strcmp(arg, "none") == 0)
			ifo->fqdn = FQDN_NONE;
		else if (strcmp(arg, "ptr") == 0)
			ifo->fqdn = FQDN_PTR;
		else if (strcmp(arg, "both") == 0)
			ifo->fqdn = FQDN_BOTH;
		else if (strcmp(arg, "disable") == 0)
			ifo->fqdn = FQDN_DISABLE;
		else {
			logerrx("invalid FQDN value: %s", arg);
			return -1;
		}
		break;
	case 'G':
		ifo->options &= ~DHCPCD_GATEWAY;
		break;
	case 'H':
		ifo->options |= DHCPCD_XID_HWADDR;
		break;
	case 'I':
		/* Strings have a type of 0 */;
		ifo->clientid[1] = 0;
		if (arg)
			s = parse_hwaddr((char *)ifo->clientid + 1,
			    CLIENTID_MAX_LEN, arg);
		else
			s = 0;
		if (s == -1) {
			logerr("clientid");
			return -1;
		}
		ifo->options |= DHCPCD_CLIENTID;
		ifo->clientid[0] = (uint8_t)s;
		ifo->options &= ~DHCPCD_DUID;
		break;
	case 'J':
		ifo->options |= DHCPCD_BROADCAST;
		break;
	case 'K':
		ifo->options &= ~DHCPCD_LINK;
		break;
	case 'L':
		ifo->options &= ~DHCPCD_IPV4LL;
		break;
	case 'M':
		ifo->options |= DHCPCD_MANAGER;
		break;
	case 'O':
		ARG_REQUIRED;
		if (ctx->options & DHCPCD_PRINT_PIDFILE)
			break;
		set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, request, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, require, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, no, arg, 1) != 0)
		{
			logerrx("unknown option: %s", arg);
			return -1;
		}
		break;
	case 'Q':
		ARG_REQUIRED;
		if (ctx->options & DHCPCD_PRINT_PIDFILE)
			break;
		set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, require, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, request, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, no, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, reject, arg, -1) != 0)
		{
			logerrx("unknown option: %s", arg);
			return -1;
		}
		break;
	case 'S':
		ARG_REQUIRED;
		p = strchr(arg, '=');
		if (p == NULL) {
			logerrx("static assignment required");
			return -1;
		}
		p = strskipwhite(++p);
		if (strncmp(arg, "ip_address=", strlen("ip_address=")) == 0) {
			if (p == NULL) {
				ifo->options &= ~DHCPCD_STATIC;
				ifo->req_addr.s_addr = INADDR_ANY;
				break;
			}
			if (parse_addr(&ifo->req_addr,
			    ifo->req_mask.s_addr == 0 ? &ifo->req_mask : NULL,
			    p) != 0)
				return -1;

			ifo->options |= DHCPCD_STATIC;
			ifo->options &= ~DHCPCD_INFORM;
		} else if (strncmp(arg, "subnet_mask=",
		    strlen("subnet_mask=")) == 0)
		{
			if (p == NULL) {
				ifo->req_mask.s_addr = INADDR_ANY;
				break;
			}
			if (parse_addr(&ifo->req_mask, NULL, p) != 0)
				return -1;
		} else if (strncmp(arg, "broadcast_address=",
		    strlen("broadcast_address=")) == 0)
		{
			if (p == NULL) {
				ifo->req_brd.s_addr = INADDR_ANY;
				break;
			}
			if (parse_addr(&ifo->req_brd, NULL, p) != 0)
				return -1;
		} else if (strncmp(arg, "routes=", strlen("routes=")) == 0 ||
		    strncmp(arg, "static_routes=",
		        strlen("static_routes=")) == 0 ||
		    strncmp(arg, "classless_static_routes=",
		        strlen("classless_static_routes=")) == 0 ||
		    strncmp(arg, "ms_classless_static_routes=",
		        strlen("ms_classless_static_routes=")) == 0)
		{
			struct in_addr addr3;

			if (p == NULL) {
				rt_headclear(&ifo->routes, AF_INET);
				add_environ(&ifo->config, arg, 1);
				break;
			}

			fp = np = strwhite(p);
			if (np == NULL) {
				logerrx("all routes need a gateway");
				return -1;
			}
			*np++ = '\0';
			np = strskipwhite(np);
			if (parse_addr(&addr, &addr2, p) == -1 ||
			    parse_addr(&addr3, NULL, np) == -1)
			{
				*fp = ' ';
				return -1;
			}
			*fp = ' ';
			if ((rt = rt_new0(ctx)) == NULL)
				return -1;
			sa_in_init(&rt->rt_dest, &addr);
			sa_in_init(&rt->rt_netmask, &addr2);
			sa_in_init(&rt->rt_gateway, &addr3);
			if (rt_proto_add_ctx(&ifo->routes, rt, ctx))
				add_environ(&ifo->config, arg, 0);
		} else if (strncmp(arg, "routers=", strlen("routers=")) == 0) {
			if (p == NULL) {
				rt_headclear(&ifo->routes, AF_INET);
				add_environ(&ifo->config, arg, 1);
				break;
			}
			if (parse_addr(&addr, NULL, p) == -1)
				return -1;
			if ((rt = rt_new0(ctx)) == NULL)
				return -1;
			addr2.s_addr = INADDR_ANY;
			sa_in_init(&rt->rt_dest, &addr2);
			sa_in_init(&rt->rt_netmask, &addr2);
			sa_in_init(&rt->rt_gateway, &addr);
			if (rt_proto_add_ctx(&ifo->routes, rt, ctx))
				add_environ(&ifo->config, arg, 0);
		} else if (strncmp(arg, "interface_mtu=",
		    strlen("interface_mtu=")) == 0 ||
		    strncmp(arg, "mtu=", strlen("mtu=")) == 0)
		{
			if (p == NULL)
				break;
			ifo->mtu = (unsigned int)strtou(p, NULL, 0,
			    MTU_MIN, MTU_MAX, &e);
			if (e) {
				logerrx("invalid MTU %s", p);
				return -1;
			}
		} else if (strncmp(arg, "ip6_address=", strlen("ip6_address=")) == 0) {
			if (p == NULL) {
				memset(&ifo->req_addr6, 0,
				    sizeof(ifo->req_addr6));
				break;
			}

			np = strchr(p, '/');
			if (np)
				*np++ = '\0';
			if ((i = inet_pton(AF_INET6, p, &ifo->req_addr6)) == 1) {
				if (np) {
					ifo->req_prefix_len = (uint8_t)strtou(np,
					    NULL, 0, 0, 128, &e);
					if (e) {
						logerrx("%s: failed to "
						    "convert prefix len",
						    ifname);
						return -1;
					}
				} else
					ifo->req_prefix_len = 128;
			}
			if (np)
				*(--np) = '\0';
			if (i != 1) {
				logerrx("invalid AF_INET6: %s", p);
				memset(&ifo->req_addr6, 0,
				    sizeof(ifo->req_addr6));
				return -1;
			}
		} else
			add_environ(&ifo->config, arg, p == NULL ? 1 : 0);
		break;

	case 'W':
		if (parse_addr(&addr, &addr2, arg) != 0)
			return -1;
		if (strchr(arg, '/') == NULL)
			addr2.s_addr = INADDR_BROADCAST;
		naddr = reallocarray(ifo->whitelist,
		    ifo->whitelist_len + 2, sizeof(in_addr_t));
		if (naddr == NULL) {
			logerr(__func__);
			return -1;
		}
		ifo->whitelist = naddr;
		ifo->whitelist[ifo->whitelist_len++] = addr.s_addr;
		ifo->whitelist[ifo->whitelist_len++] = addr2.s_addr;
		break;
	case 'X':
		if (parse_addr(&addr, &addr2, arg) != 0)
			return -1;
		if (strchr(arg, '/') == NULL)
			addr2.s_addr = INADDR_BROADCAST;
		naddr = reallocarray(ifo->blacklist,
		    ifo->blacklist_len + 2, sizeof(in_addr_t));
		if (naddr == NULL) {
			logerr(__func__);
			return -1;
		}
		ifo->blacklist = naddr;
		ifo->blacklist[ifo->blacklist_len++] = addr.s_addr;
		ifo->blacklist[ifo->blacklist_len++] = addr2.s_addr;
		break;
	case 'Z':
		ARG_REQUIRED;
		if (!IN_CONFIG_BLOCK(ifo))
			ctx->ifdv = splitv(&ctx->ifdc, ctx->ifdv, arg);
		break;
	case '1':
		ifo->options |= DHCPCD_ONESHOT;
		break;
	case '4':
#ifdef INET
		ifo->options &= ~DHCPCD_IPV6;
		ifo->options |= DHCPCD_IPV4;
		break;
#else
		logerrx("INET has been compiled out");
		return -1;
#endif
	case '6':
#ifdef INET6
		ifo->options &= ~DHCPCD_IPV4;
		ifo->options |= DHCPCD_IPV6;
		break;
#else
		logerrx("INET6 has been compiled out");
		return -1;
#endif
	case O_IPV4:
		ifo->options |= DHCPCD_IPV4;
		break;
	case O_NOIPV4:
		ifo->options &= ~DHCPCD_IPV4;
		break;
	case O_IPV6:
		ifo->options |= DHCPCD_IPV6;
		break;
	case O_NOIPV6:
		ifo->options &= ~DHCPCD_IPV6;
		break;
	case O_ANONYMOUS:
		ifo->options |= DHCPCD_ANONYMOUS;
		ifo->options &= ~DHCPCD_HOSTNAME;
		ifo->fqdn = FQDN_DISABLE;

		/* Block everything */
		memset(ifo->nomask, 0xff, sizeof(ifo->nomask));
		memset(ifo->nomask6, 0xff, sizeof(ifo->nomask6));

		/* Allow the bare minimum through */
#ifdef INET
		del_option_mask(ifo->nomask, DHO_SUBNETMASK);
		del_option_mask(ifo->nomask, DHO_CSR);
		del_option_mask(ifo->nomask, DHO_ROUTER);
		del_option_mask(ifo->nomask, DHO_DNSSERVER);
		del_option_mask(ifo->nomask, DHO_DNSDOMAIN);
		del_option_mask(ifo->nomask, DHO_BROADCAST);
		del_option_mask(ifo->nomask, DHO_STATICROUTE);
		del_option_mask(ifo->nomask, DHO_SERVERID);
		del_option_mask(ifo->nomask, DHO_RENEWALTIME);
		del_option_mask(ifo->nomask, DHO_REBINDTIME);
		del_option_mask(ifo->nomask, DHO_DNSSEARCH);
#endif

#ifdef DHCP6
		del_option_mask(ifo->nomask6, D6_OPTION_DNS_SERVERS);
		del_option_mask(ifo->nomask6, D6_OPTION_DOMAIN_LIST);
		del_option_mask(ifo->nomask6, D6_OPTION_SOL_MAX_RT);
		del_option_mask(ifo->nomask6, D6_OPTION_INF_MAX_RT);
#endif

		break;
	case O_RANDOMISE_HWADDR:
		ifo->randomise_hwaddr = true;
		break;
#ifdef INET
	case O_ARPING:
		while (arg != NULL) {
			fp = strwhite(arg);
			if (fp)
				*fp++ = '\0';
			if (parse_addr(&addr, NULL, arg) != 0)
				return -1;
			naddr = reallocarray(ifo->arping,
			    (size_t)ifo->arping_len + 1, sizeof(in_addr_t));
			if (naddr == NULL) {
				logerr(__func__);
				return -1;
			}
			ifo->arping = naddr;
			ifo->arping[ifo->arping_len++] = addr.s_addr;
			arg = strskipwhite(fp);
		}
		break;
	case O_DESTINATION:
		ARG_REQUIRED;
		if (ctx->options & DHCPCD_PRINT_PIDFILE)
			break;
		set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl,
		    ifo->dstmask, arg, 2) != 0)
		{
			if (errno == EINVAL)
				logerrx("option does not take"
				    " an IPv4 address: %s", arg);
			else
				logerrx("unknown option: %s", arg);
			return -1;
		}
		break;
	case O_FALLBACK:
		ARG_REQUIRED;
		free(ifo->fallback);
		ifo->fallback = strdup(arg);
		if (ifo->fallback == NULL) {
			logerrx(__func__);
			return -1;
		}
		break;
#endif
	case O_IAID:
		ARG_REQUIRED;
		if (ctx->options & DHCPCD_MANAGER && !IN_CONFIG_BLOCK(ifo)) {
			logerrx("IAID must belong in an interface block");
			return -1;
		}
		if (parse_iaid(ifo->iaid, arg, sizeof(ifo->iaid)) == -1) {
			logerrx("invalid IAID %s", arg);
			return -1;
		}
		ifo->options |= DHCPCD_IAID;
		break;
	case O_IPV6RS:
		ifo->options |= DHCPCD_IPV6RS;
		break;
	case O_NOIPV6RS:
		ifo->options &= ~DHCPCD_IPV6RS;
		break;
	case O_IPV6RA_FORK:
		ifo->options &= ~DHCPCD_IPV6RA_REQRDNSS;
		break;
	case O_IPV6RA_AUTOCONF:
		ifo->options |= DHCPCD_IPV6RA_AUTOCONF;
		break;
	case O_IPV6RA_NOAUTOCONF:
		ifo->options &= ~DHCPCD_IPV6RA_AUTOCONF;
		break;
	case O_NOALIAS:
		ifo->options |= DHCPCD_NOALIAS;
		break;
#ifdef DHCP6
	case O_IA_NA:
		i = D6_OPTION_IA_NA;
		/* FALLTHROUGH */
	case O_IA_TA:
		if (i == 0)
			i = D6_OPTION_IA_TA;
		/* FALLTHROUGH */
	case O_IA_PD:
		if (i == 0) {
#ifdef SMALL
			logwarnx("%s: IA_PD not compiled in", ifname);
			return -1;
#else
			if (ctx->options & DHCPCD_MANAGER &&
			    !IN_CONFIG_BLOCK(ifo))
			{
				logerrx("IA PD must belong in an "
				    "interface block");
				return -1;
			}
			i = D6_OPTION_IA_PD;
#endif
		}
		if (ctx->options & DHCPCD_MANAGER &&
		    !IN_CONFIG_BLOCK(ifo) && arg)
		{
			logerrx("IA with IAID must belong in an "
			    "interface block");
			return -1;
		}
		ifo->options |= DHCPCD_IA_FORCED;
		fp = strwhite(arg);
		if (fp) {
			*fp++ = '\0';
			fp = strskipwhite(fp);
		}
		if (arg) {
			p = strchr(arg, '/');
			if (p)
				*p++ = '\0';
			if (parse_iaid(iaid, arg, sizeof(iaid)) == -1) {
				logerr("invalid IAID: %s", arg);
				return -1;
			}
		}
		ia = NULL;
		for (sl = 0; sl < ifo->ia_len; sl++) {
			if ((arg == NULL && !ifo->ia[sl].iaid_set) ||
			    (arg != NULL && ifo->ia[sl].iaid_set &&
			    ifo->ia[sl].ia_type == (uint16_t)i &&
			    ifo->ia[sl].iaid[0] == iaid[0] &&
			    ifo->ia[sl].iaid[1] == iaid[1] &&
			    ifo->ia[sl].iaid[2] == iaid[2] &&
			    ifo->ia[sl].iaid[3] == iaid[3]))
			{
			        ia = &ifo->ia[sl];
				break;
			}
		}
		if (ia == NULL) {
			ia = reallocarray(ifo->ia,
			    ifo->ia_len + 1, sizeof(*ifo->ia));
			if (ia == NULL) {
				logerr(__func__);
				return -1;
			}
			ifo->ia = ia;
			ia = &ifo->ia[ifo->ia_len++];
			ia->ia_type = (uint16_t)i;
			if (arg) {
				ia->iaid[0] = iaid[0];
				ia->iaid[1] = iaid[1];
				ia->iaid[2] = iaid[2];
				ia->iaid[3] = iaid[3];
				ia->iaid_set = 1;
			} else
				ia->iaid_set = 0;
			if (!ia->iaid_set ||
			    p == NULL ||
			    ia->ia_type == D6_OPTION_IA_TA)
			{
				memset(&ia->addr, 0, sizeof(ia->addr));
				ia->prefix_len = 0;
			} else {
				arg = p;
				p = strchr(arg, '/');
				if (p)
					*p++ = '\0';
				if (inet_pton(AF_INET6, arg, &ia->addr) != 1) {
					logerrx("invalid AF_INET6: %s", arg);
					memset(&ia->addr, 0, sizeof(ia->addr));
				}
				if (p && ia->ia_type == D6_OPTION_IA_PD) {
					ia->prefix_len = (uint8_t)strtou(p,
					    NULL, 0, 8, 120, &e);
					if (e) {
						logerrx("%s: failed to convert"
						    " prefix len",
						    p);
						ia->prefix_len = 0;
					}
				}
			}
#ifndef SMALL
			ia->sla_max = 0;
			ia->sla_len = 0;
			ia->sla = NULL;
#endif
		}

#ifdef SMALL
		break;
#else
		if (ia->ia_type != D6_OPTION_IA_PD)
			break;

		for (p = fp; p; p = fp) {
			fp = strwhite(p);
			if (fp) {
				*fp++ = '\0';
				fp = strskipwhite(fp);
			}
			sla = reallocarray(ia->sla,
			    ia->sla_len + 1, sizeof(*ia->sla));
			if (sla == NULL) {
				logerr(__func__);
				return -1;
			}
			ia->sla = sla;
			sla = &ia->sla[ia->sla_len++];
			np = strchr(p, '/');
			if (np)
				*np++ = '\0';
			if (strlcpy(sla->ifname, p,
			    sizeof(sla->ifname)) >= sizeof(sla->ifname))
			{
				logerrx("%s: interface name too long", arg);
				goto err_sla;
			}
			sla->sla_set = false;
			sla->prefix_len = 0;
			sla->suffix = 1;
			p = np;
			if (p) {
				np = strchr(p, '/');
				if (np)
					*np++ = '\0';
				if (*p != '\0') {
					sla->sla = (uint32_t)strtou(p, NULL,
					    0, 0, UINT32_MAX, &e);
					sla->sla_set = true;
					if (e) {
						logerrx("%s: failed to convert "
						    "sla",
						    ifname);
						goto err_sla;
					}
				}
				p = np;
			}
			if (p) {
				np = strchr(p, '/');
				if (np)
					*np++ = '\0';
				if (*p != '\0') {
					sla->prefix_len = (uint8_t)strtou(p,
				    NULL, 0, 0, 120, &e);
					if (e) {
						logerrx("%s: failed to "
						    "convert prefix len",
						    ifname);
						goto err_sla;
					}
				}
				p = np;
			}
			if (p) {
				np = strchr(p, '/');
				if (np)
					*np = '\0';
				if (*p != '\0') {
					sla->suffix = (uint64_t)strtou(p, NULL,
					    0, 0, UINT64_MAX, &e);
					if (e) {
						logerrx("%s: failed to "
						    "convert suffix",
						    ifname);
						goto err_sla;
					}
				}
			}
			/* Sanity check */
			for (sl = 0; sl < ia->sla_len - 1; sl++) {
				slap = &ia->sla[sl];
				if (slap->sla_set != sla->sla_set) {
					logerrx("%s: cannot mix automatic "
					    "and fixed SLA",
					    sla->ifname);
					goto err_sla;
				}
				if (ia->prefix_len &&
				    (sla->prefix_len == ia->prefix_len ||
				    slap->prefix_len == ia->prefix_len))
				{
					logerrx("%s: cannot delegte the same"
					    "prefix length more than once",
					    sla->ifname);
					goto err_sla;
				}
				if (!sla->sla_set &&
				    strcmp(slap->ifname, sla->ifname) == 0)
				{
					logwarnx("%s: cannot specify the "
					    "same interface twice with "
					    "an automatic SLA",
					    sla->ifname);
					goto err_sla;
				}
				if (slap->sla_set && sla->sla_set &&
				    slap->sla == sla->sla)
				{
					logerrx("%s: cannot"
					    " assign the same SLA %u"
					    " more than once",
					    sla->ifname, sla->sla);
					goto err_sla;
				}
			}
			if (sla->sla_set && sla->sla > ia->sla_max)
				ia->sla_max = sla->sla;
		}
		break;
err_sla:
		ia->sla_len--;
		return -1;
#endif
#endif
	case O_HOSTNAME_SHORT:
		ifo->options |= DHCPCD_HOSTNAME | DHCPCD_HOSTNAME_SHORT;
		break;
	case O_DEV:
		ARG_REQUIRED;
#ifdef PLUGIN_DEV
		if (ctx->dev_load)
			free(ctx->dev_load);
		ctx->dev_load = strdup(arg);
#endif
		break;
	case O_NODEV:
		ifo->options &= ~DHCPCD_DEV;
		break;
	case O_DEFINE:
		dop = &ifo->dhcp_override;
		dop_len = &ifo->dhcp_override_len;
		/* FALLTHROUGH */
	case O_DEFINEND:
		if (dop == NULL) {
			dop = &ifo->nd_override;
			dop_len = &ifo->nd_override_len;
		}
		/* FALLTHROUGH */
	case O_DEFINE6:
		if (dop == NULL) {
			dop = &ifo->dhcp6_override;
			dop_len = &ifo->dhcp6_override_len;
		}
		/* FALLTHROUGH */
	case O_VENDOPT:
		if (dop == NULL) {
			dop = &ifo->vivso_override;
			dop_len = &ifo->vivso_override_len;
		}
		*edop = *ldop = NULL;
		/* FALLTHROUGH */
	case O_EMBED:
		if (dop == NULL) {
			if (*edop) {
				dop = &(*edop)->embopts;
				dop_len = &(*edop)->embopts_len;
			} else if (ldop) {
				dop = &(*ldop)->embopts;
				dop_len = &(*ldop)->embopts_len;
			} else {
				logerrx("embed must be after a define "
				    "or encap");
				return -1;
			}
		}
		/* FALLTHROUGH */
	case O_ENCAP:
		ARG_REQUIRED;
		if (dop == NULL) {
			if (*ldop == NULL) {
				logerrx("encap must be after a define");
				return -1;
			}
			dop = &(*ldop)->encopts;
			dop_len = &(*ldop)->encopts_len;
		}

		/* Shared code for define, define6, embed and encap */

		/* code */
		if (opt == O_EMBED) /* Embedded options don't have codes */
			u = 0;
		else {
			fp = strwhite(arg);
			if (fp == NULL) {
				logerrx("invalid syntax: %s", arg);
				return -1;
			}
			*fp++ = '\0';
			u = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
			if (e) {
				logerrx("invalid code: %s", arg);
				return -1;
			}
			arg = strskipwhite(fp);
			if (arg == NULL) {
				logerrx("invalid syntax");
				return -1;
			}
		}
		/* type */
		fp = strwhite(arg);
		if (fp)
			*fp++ = '\0';
		np = strchr(arg, ':');
		/* length */
		if (np) {
			*np++ = '\0';
			bp = NULL; /* No bitflag */
			l = (long)strtou(np, NULL, 0, 0, LONG_MAX, &e);
			if (e) {
				logerrx("failed to convert length");
				return -1;
			}
		} else {
			l = 0;
			bp = strchr(arg, '='); /* bitflag assignment */
			if (bp)
				*bp++ = '\0';
		}
		t = 0;
		if (strcasecmp(arg, "request") == 0) {
			t |= OT_REQUEST;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logerrx("incomplete request type");
				return -1;
			}
			*fp++ = '\0';
		} else if (strcasecmp(arg, "norequest") == 0) {
			t |= OT_NOREQ;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logerrx("incomplete request type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "optional") == 0) {
			t |= OT_OPTIONAL;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logerrx("incomplete optional type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "index") == 0) {
			t |= OT_INDEX;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logerrx("incomplete index type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "array") == 0) {
			t |= OT_ARRAY;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logerrx("incomplete array type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "ipaddress") == 0)
			t |= OT_ADDRIPV4;
		else if (strcasecmp(arg, "ip6address") == 0)
			t |= OT_ADDRIPV6;
		else if (strcasecmp(arg, "string") == 0)
			t |= OT_STRING;
		else if (strcasecmp(arg, "uri") == 0)
			t |= OT_URI;
		else if (strcasecmp(arg, "byte") == 0)
			t |= OT_UINT8;
		else if (strcasecmp(arg, "bitflags") == 0)
			t |= OT_BITFLAG;
		else if (strcasecmp(arg, "uint8") == 0)
			t |= OT_UINT8;
		else if (strcasecmp(arg, "int8") == 0)
			t |= OT_INT8;
		else if (strcasecmp(arg, "uint16") == 0)
			t |= OT_UINT16;
		else if (strcasecmp(arg, "int16") == 0)
			t |= OT_INT16;
		else if (strcasecmp(arg, "uint32") == 0)
			t |= OT_UINT32;
		else if (strcasecmp(arg, "int32") == 0)
			t |= OT_INT32;
		else if (strcasecmp(arg, "flag") == 0)
			t |= OT_FLAG;
		else if (strcasecmp(arg, "raw") == 0)
			t |= OT_STRING | OT_RAW;
		else if (strcasecmp(arg, "ascii") == 0)
			t |= OT_STRING | OT_ASCII;
		else if (strcasecmp(arg, "domain") == 0)
			t |= OT_STRING | OT_DOMAIN | OT_RFC1035;
		else if (strcasecmp(arg, "dname") == 0)
			t |= OT_STRING | OT_DOMAIN;
		else if (strcasecmp(arg, "binhex") == 0)
			t |= OT_STRING | OT_BINHEX;
		else if (strcasecmp(arg, "embed") == 0)
			t |= OT_EMBED;
		else if (strcasecmp(arg, "encap") == 0)
			t |= OT_ENCAP;
		else if (strcasecmp(arg, "rfc3361") ==0)
			t |= OT_STRING | OT_RFC3361;
		else if (strcasecmp(arg, "rfc3442") ==0)
			t |= OT_STRING | OT_RFC3442;
		else if (strcasecmp(arg, "option") == 0)
			t |= OT_OPTION;
		else {
			logerrx("unknown type: %s", arg);
			return -1;
		}
		if (l && !(t & (OT_STRING | OT_BINHEX))) {
			logwarnx("ignoring length for type: %s", arg);
			l = 0;
		}
		if (t & OT_ARRAY && t & (OT_STRING | OT_BINHEX) &&
		    !(t & (OT_RFC1035 | OT_DOMAIN)))
		{
			logwarnx("ignoring array for strings");
			t &= ~OT_ARRAY;
		}
		if (t & OT_BITFLAG) {
			if (bp == NULL)
				logwarnx("missing bitflag assignment");
		}
		/* variable */
		if (!fp) {
			if (!(t & OT_OPTION)) {
			        logerrx("type %s requires a variable name",
				    arg);
				return -1;
			}
			np = NULL;
		} else {
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp)
				*fp++ = '\0';
			if (strcasecmp(arg, "reserved")) {
				np = strdup(arg);
				if (np == NULL) {
					logerr(__func__);
					return -1;
				}
			} else {
				np = NULL;
				t |= OT_RESERVED;
			}
		}
		if (opt != O_EMBED) {
			for (dl = 0, ndop = *dop; dl < *dop_len; dl++, ndop++)
			{
				/* type 0 seems freshly malloced struct
				 * for us to use */
				if (ndop->option == u || ndop->type == 0)
					break;
			}
			if (dl == *dop_len)
				ndop = NULL;
		} else
			ndop = NULL;
		if (ndop == NULL) {
			ndop = reallocarray(*dop, *dop_len + 1, sizeof(**dop));
			if (ndop == NULL) {
				logerr(__func__);
				free(np);
				return -1;
			}
			*dop = ndop;
			ndop = &(*dop)[(*dop_len)++];
			ndop->embopts = NULL;
			ndop->embopts_len = 0;
			ndop->encopts = NULL;
			ndop->encopts_len = 0;
		} else
			free_dhcp_opt_embenc(ndop);
		ndop->option = (uint32_t)u; /* could have been 0 */
		ndop->type = t;
		ndop->len = (size_t)l;
		ndop->var = np;
		if (bp) {
			dl = strlen(bp);
			memcpy(ndop->bitflags, bp, dl);
			memset(ndop->bitflags + dl, 0,
			    sizeof(ndop->bitflags) - dl);
		} else
			memset(ndop->bitflags, 0, sizeof(ndop->bitflags));
		/* Save the define for embed and encap options */
		switch (opt) {
		case O_DEFINE:
		case O_DEFINEND:
		case O_DEFINE6:
		case O_VENDOPT:
			*ldop = ndop;
			break;
		case O_ENCAP:
			*edop = ndop;
			break;
		}
		break;
	case O_VENDCLASS:
		ARG_REQUIRED;
		fp = strwhite(arg);
		if (fp)
			*fp++ = '\0';
		u = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logerrx("invalid code: %s", arg);
			return -1;
		}
		fp = strskipwhite(fp);
		if (fp) {
			s = parse_string(NULL, 0, fp);
			if (s == -1) {
				logerr(__func__);
				return -1;
			}
			dl = (size_t)s;
			if (dl + (sizeof(uint16_t) * 2) > UINT16_MAX) {
				logerrx("vendor class is too big");
				return -1;
			}
			np = malloc(dl);
			if (np == NULL) {
				logerr(__func__);
				return -1;
			}
			parse_string(np, dl, fp);
		} else {
			dl = 0;
			np = NULL;
		}
		vivco = reallocarray(ifo->vivco,
		    ifo->vivco_len + 1, sizeof(*ifo->vivco));
		if (vivco == NULL) {
			logerr( __func__);
			free(np);
			return -1;
		}
		ifo->vivco = vivco;
		ifo->vivco_en = (uint32_t)u;
		vivco = &ifo->vivco[ifo->vivco_len++];
		vivco->len = dl;
		vivco->data = (uint8_t *)np;
		break;
	case O_AUTHPROTOCOL:
		ARG_REQUIRED;
#ifdef AUTH
		fp = strwhite(arg);
		if (fp)
			*fp++ = '\0';
		if (strcasecmp(arg, "token") == 0)
			ifo->auth.protocol = AUTH_PROTO_TOKEN;
		else if (strcasecmp(arg, "delayed") == 0)
			ifo->auth.protocol = AUTH_PROTO_DELAYED;
		else if (strcasecmp(arg, "delayedrealm") == 0)
			ifo->auth.protocol = AUTH_PROTO_DELAYEDREALM;
		else {
			logerrx("%s: unsupported protocol", arg);
			return -1;
		}
		arg = strskipwhite(fp);
		fp = strwhite(arg);
		if (arg == NULL) {
			ifo->auth.options |= DHCPCD_AUTH_SEND;
			if (ifo->auth.protocol == AUTH_PROTO_TOKEN)
				ifo->auth.protocol = AUTH_ALG_NONE;
			else
				ifo->auth.algorithm = AUTH_ALG_HMAC_MD5;
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
			break;
		}
		if (fp)
			*fp++ = '\0';
		if (ifo->auth.protocol == AUTH_PROTO_TOKEN) {
			np = strchr(arg, '/');
			if (np) {
				if (fp == NULL || np < fp)
					*np++ = '\0';
				else
					np = NULL;
			}
			if (parse_uint32(&ifo->auth.token_snd_secretid,
			    arg) == -1)
				logerrx("%s: not a number", arg);
			else
				ifo->auth.token_rcv_secretid =
				    ifo->auth.token_snd_secretid;
			if (np &&
			    parse_uint32(&ifo->auth.token_rcv_secretid,
			    np) == -1)
				logerrx("%s: not a number", arg);
		} else {
			if (strcasecmp(arg, "hmacmd5") == 0 ||
			    strcasecmp(arg, "hmac-md5") == 0)
				ifo->auth.algorithm = AUTH_ALG_HMAC_MD5;
			else {
				logerrx("%s: unsupported algorithm", arg);
				return 1;
			}
		}
		arg = fp;
		if (arg == NULL) {
			ifo->auth.options |= DHCPCD_AUTH_SEND;
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
			break;
		}
		if (strcasecmp(arg, "monocounter") == 0) {
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
			ifo->auth.options |= DHCPCD_AUTH_RDM_COUNTER;
		} else if (strcasecmp(arg, "monotonic") ==0 ||
		    strcasecmp(arg, "monotime") == 0)
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
		else {
			logerrx("%s: unsupported RDM", arg);
			return -1;
		}
		ifo->auth.options |= DHCPCD_AUTH_SEND;
		break;
#else
		logerrx("no authentication support");
		return -1;
#endif
	case O_AUTHTOKEN:
		ARG_REQUIRED;
#ifdef AUTH
		fp = strwhite(arg);
		if (fp == NULL) {
			logerrx("authtoken requires a realm");
			return -1;
		}
		*fp++ = '\0';
		token = calloc(1, sizeof(*token));
		if (token == NULL) {
			logerr(__func__);
			return -1;
		}
		if (parse_uint32(&token->secretid, arg) == -1) {
			logerrx("%s: not a number", arg);
			goto invalid_token;
		}
		arg = fp;
		fp = strend(arg);
		if (fp == NULL) {
			logerrx("authtoken requires a realm");
			goto invalid_token;
		}
		*fp++ = '\0';
		s = parse_string(NULL, 0, arg);
		if (s == -1) {
			logerr("realm_len");
			goto invalid_token;
		}
		if (s != 0) {
			token->realm_len = (size_t)s;
			token->realm = malloc(token->realm_len);
			if (token->realm == NULL) {
				logerr(__func__);
				goto invalid_token;
			}
			parse_string((char *)token->realm, token->realm_len,
			    arg);
		}
		arg = fp;
		fp = strend(arg);
		if (fp == NULL) {
			logerrx("authtoken requies an expiry date");
			goto invalid_token;
		}
		*fp++ = '\0';
		if (*arg == '"') {
			arg++;
			np = strchr(arg, '"');
			if (np)
				*np = '\0';
		}
		if (strcmp(arg, "0") == 0 || strcasecmp(arg, "forever") == 0)
			token->expire =0;
		else {
			struct tm tm;

			memset(&tm, 0, sizeof(tm));
			if (strptime(arg, "%Y-%m-%d %H:%M", &tm) == NULL) {
				logerrx("%s: invalid date time", arg);
				goto invalid_token;
			}
			if ((token->expire = mktime(&tm)) == (time_t)-1) {
				logerr("%s: mktime", __func__);
				goto invalid_token;
			}
		}
		arg = fp;
		s = parse_string(NULL, 0, arg);
		if (s == -1 || s == 0) {
			if (s == -1)
				logerr("token_len");
			else
				logerrx("authtoken requires a key");
			goto invalid_token;
		}
		token->key_len = (size_t)s;
		token->key = malloc(token->key_len);
		if (token->key == NULL) {
			logerr(__func__);
			goto invalid_token;
		}
		parse_string((char *)token->key, token->key_len, arg);
		TAILQ_INSERT_TAIL(&ifo->auth.tokens, token, next);
		break;

invalid_token:
		free(token->realm);
		free(token);
#else
		logerrx("no authentication support");
#endif
		return -1;
	case O_AUTHNOTREQUIRED:
		ifo->auth.options &= ~DHCPCD_AUTH_REQUIRE;
		break;
	case O_DHCP:
		ifo->options |= DHCPCD_DHCP | DHCPCD_WANTDHCP | DHCPCD_IPV4;
		break;
	case O_NODHCP:
		ifo->options &= ~DHCPCD_DHCP;
		break;
	case O_DHCP6:
		ifo->options |= DHCPCD_DHCP6 | DHCPCD_IPV6;
		break;
	case O_NODHCP6:
		ifo->options &= ~DHCPCD_DHCP6;
		break;
	case O_CONTROLGRP:
		ARG_REQUIRED;
#ifdef PRIVSEP
		/* Control group is already set by this point.
		 * We don't need to pledge getpw either with this. */
		if (IN_PRIVSEP(ctx))
			break;
#endif
#ifdef _REENTRANT
		l = sysconf(_SC_GETGR_R_SIZE_MAX);
		if (l == -1)
			dl = 1024;
		else
			dl = (size_t)l;
		p = malloc(dl);
		if (p == NULL) {
			logerr(__func__);
			return -1;
		}
		while ((i = getgrnam_r(arg, &grpbuf, p, dl, &grp)) ==
		    ERANGE)
		{
			size_t nl = dl * 2;
			if (nl < dl) {
				logerrx("control_group: out of buffer");
				free(p);
				return -1;
			}
			dl = nl;
			np = realloc(p, dl);
			if (np == NULL) {
				logerr(__func__);
				free(p);
				return -1;
			}
			p = np;
		}
		if (i != 0) {
			errno = i;
			logerr("getgrnam_r");
			free(p);
			return -1;
		}
		if (grp == NULL) {
			if (!ctx->control_group)
				logerrx("controlgroup: %s: not found", arg);
			free(p);
			return -1;
		}
		ctx->control_group = grp->gr_gid;
		free(p);
#else
		grp = getgrnam(arg);
		if (grp == NULL) {
			if (!ctx->control_group)
				logerrx("controlgroup: %s: not found", arg);
			return -1;
		}
		ctx->control_group = grp->gr_gid;
#endif
		break;
	case O_GATEWAY:
		ifo->options |= DHCPCD_GATEWAY;
		break;
	case O_NOUP:
		ifo->options &= ~DHCPCD_IF_UP;
		break;
	case O_SLAAC:
		ARG_REQUIRED;
		np = strwhite(arg);
		if (np != NULL) {
			*np++ = '\0';
			np = strskipwhite(np);
		}
		if (strcmp(arg, "private") == 0 ||
		    strcmp(arg, "stableprivate") == 0 ||
		    strcmp(arg, "stable") == 0)
			ifo->options |= DHCPCD_SLAACPRIVATE;
		else
			ifo->options &= ~DHCPCD_SLAACPRIVATE;
#ifdef INET6
		if (strcmp(arg, "token") == 0) {
			if (np == NULL) {
				logerrx("slaac token: no token specified");
				return -1;
			}
			arg = np;
			np = strwhite(np);
			if (np != NULL) {
				*np++ = '\0';
				np = strskipwhite(np);
			}
			if (inet_pton(AF_INET6, arg, &ifo->token) != 1) {
				logerrx("slaac token: invalid token");
				return -1;
			}
		}
#endif
		if (np != NULL &&
		    (strcmp(np, "temp") == 0 || strcmp(np, "temporary") == 0))
			ifo->options |= DHCPCD_SLAACTEMP;
		break;
	case O_BOOTP:
		ifo->options |= DHCPCD_BOOTP;
		break;
	case O_NODELAY:
		ifo->options &= ~DHCPCD_INITIAL_DELAY;
		break;
	case O_LASTLEASE_EXTEND:
		ifo->options |= DHCPCD_LASTLEASE | DHCPCD_LASTLEASE_EXTEND;
		break;
	case O_INACTIVE:
		ifo->options |= DHCPCD_INACTIVE;
		break;
	case O_MUDURL:
		ARG_REQUIRED;
		s = parse_string((char *)ifo->mudurl + 1, MUDURL_MAX_LEN, arg);
		if (s == -1) {
			logerr("mudurl");
			return -1;
		}
		*ifo->mudurl = (uint8_t)s;
		break;
	case O_LINK_RCVBUF:
#ifndef SMALL
		ARG_REQUIRED;
		ctx->link_rcvbuf = (int)strtoi(arg, NULL, 0, 0, INT32_MAX, &e);
		if (e) {
			logerrx("failed to convert link_rcvbuf %s", arg);
			return -1;
		}
#endif
		break;
	case O_CONFIGURE:
		ifo->options |= DHCPCD_CONFIGURE;
		break;
	case O_NOCONFIGURE:
		ifo->options &= ~DHCPCD_CONFIGURE;
		break;
	case O_ARP_PERSISTDEFENCE:
		ifo->options |= DHCPCD_ARP_PERSISTDEFENCE;
		break;
	case O_REQUEST_TIME:
		ARG_REQUIRED;
		ifo->request_time =
		    (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logerrx("invalid request time: %s", arg);
			return -1;
		}
		break;
#ifdef INET
	case O_FALLBACK_TIME:
		ARG_REQUIRED;
		ifo->request_time =
		    (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logerrx("invalid fallback time: %s", arg);
			return -1;
		}
		break;
	case O_IPV4LL_TIME:
		ARG_REQUIRED;
		ifo->ipv4ll_time =
		    (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logerrx("invalid ipv4ll time: %s", arg);
			return -1;
		}
		break;
#endif
	default:
		return 0;
	}

	return 1;

#ifdef ARG_REQUIRED
arg_required:
	logerrx("option %d requires an argument", opt);
	return -1;
#undef ARG_REQUIRED
#endif
}

static int
parse_config_line(struct dhcpcd_ctx *ctx, const char *ifname,
    struct if_options *ifo, const char *opt, char *line,
    struct dhcp_opt **ldop, struct dhcp_opt **edop)
{
	unsigned int i;

	for (i = 0; i < sizeof(cf_options) / sizeof(cf_options[0]); i++) {
		if (!cf_options[i].name ||
		    strcmp(cf_options[i].name, opt) != 0)
			continue;

		if (cf_options[i].has_arg == required_argument && !line) {
			logerrx("option requires an argument -- %s", opt);
			return -1;
		}

		return parse_option(ctx, ifname, ifo, cf_options[i].val, line,
		    ldop, edop);
	}

	if (!(ctx->options & DHCPCD_PRINT_PIDFILE))
		logerrx("unknown option: %s", opt);
	return -1;
}

static void
finish_config(struct if_options *ifo)
{

	/* Terminate the encapsulated options */
	if (ifo->vendor[0] && !(ifo->options & DHCPCD_VENDORRAW)) {
		ifo->vendor[0]++;
		ifo->vendor[ifo->vendor[0]] = DHO_END;
		/* We are called twice.
		 * This should be fixed, but in the meantime, this
		 * guard should suffice */
		ifo->options |= DHCPCD_VENDORRAW;
	}

	if (!(ifo->options & DHCPCD_ARP) ||
	    ifo->options & (DHCPCD_INFORM | DHCPCD_STATIC))
		ifo->options &= ~DHCPCD_IPV4LL;

	if (!(ifo->options & DHCPCD_IPV4))
		ifo->options &= ~(DHCPCD_DHCP | DHCPCD_IPV4LL | DHCPCD_WAITIP4);

	if (!(ifo->options & DHCPCD_IPV6))
		ifo->options &=
		    ~(DHCPCD_IPV6RS | DHCPCD_DHCP6 | DHCPCD_WAITIP6);

	if (!(ifo->options & DHCPCD_IPV6RS))
		ifo->options &=
		    ~(DHCPCD_IPV6RA_AUTOCONF | DHCPCD_IPV6RA_REQRDNSS);
}

static struct if_options *
default_config(struct dhcpcd_ctx *ctx)
{
	struct if_options *ifo;

	/* Seed our default options */
	if ((ifo = calloc(1, sizeof(*ifo))) == NULL) {
		logerr(__func__);
		return NULL;
	}
	ifo->options |= DHCPCD_IF_UP | DHCPCD_LINK | DHCPCD_INITIAL_DELAY;
	ifo->timeout = DEFAULT_TIMEOUT;
	ifo->reboot = DEFAULT_REBOOT;
	ifo->request_time = DEFAULT_REQUEST;
#ifdef INET
	ifo->fallback_time = DEFAULT_FALLBACK;
	ifo->ipv4ll_time = DEFAULT_IPV4LL;
#endif
	ifo->metric = -1;
	ifo->auth.options |= DHCPCD_AUTH_REQUIRE;
	rb_tree_init(&ifo->routes, &rt_compare_list_ops);
#ifdef AUTH
	TAILQ_INIT(&ifo->auth.tokens);
#endif

	/* Inherit some global defaults */
	if (ctx->options & DHCPCD_CONFIGURE)
		ifo->options |= DHCPCD_CONFIGURE;
	if (ctx->options & DHCPCD_PERSISTENT)
		ifo->options |= DHCPCD_PERSISTENT;
	if (ctx->options & DHCPCD_SLAACPRIVATE)
		ifo->options |= DHCPCD_SLAACPRIVATE;

	return ifo;
}

struct if_options *
read_config(struct dhcpcd_ctx *ctx,
    const char *ifname, const char *ssid, const char *profile)
{
	struct if_options *ifo;
	char buf[UDPLEN_MAX], *bp; /* 64k max config file size */
	char *line, *option, *p;
	ssize_t buflen;
	size_t vlen;
	int skip, have_profile, new_block, had_block;
#if !defined(INET) || !defined(INET6)
	size_t i;
	struct dhcp_opt *opt;
#endif
	struct dhcp_opt *ldop, *edop;

	/* Seed our default options */
	if ((ifo = default_config(ctx)) == NULL)
		return NULL;
	if (default_options == 0) {
		default_options |= DHCPCD_CONFIGURE | DHCPCD_DAEMONISE |
		    DHCPCD_GATEWAY;
#ifdef INET
		skip = xsocket(PF_INET, SOCK_DGRAM, 0);
		if (skip != -1) {
			close(skip);
			default_options |= DHCPCD_IPV4 | DHCPCD_ARP |
			    DHCPCD_DHCP | DHCPCD_IPV4LL;
		}
#endif
#ifdef INET6
		skip = xsocket(PF_INET6, SOCK_DGRAM, 0);
		if (skip != -1) {
			close(skip);
			default_options |= DHCPCD_IPV6 | DHCPCD_IPV6RS |
			    DHCPCD_IPV6RA_AUTOCONF | DHCPCD_IPV6RA_REQRDNSS |
			    DHCPCD_DHCP6;
		}
#endif
#ifdef PLUGIN_DEV
		default_options |= DHCPCD_DEV;
#endif
	}
	ifo->options |= default_options;

	CLEAR_CONFIG_BLOCK(ifo);

	vlen = strlcpy((char *)ifo->vendorclassid + 1, ctx->vendor,
	    sizeof(ifo->vendorclassid) - 1);
	ifo->vendorclassid[0] = (uint8_t)(vlen > 255 ? 0 : vlen);

	/* Reset route order */
	ctx->rt_order = 0;

	/* Parse our embedded options file */
	if (ifname == NULL && !(ctx->options & DHCPCD_PRINT_PIDFILE)) {
		/* Space for initial estimates */
#if defined(INET) && defined(INITDEFINES)
		ifo->dhcp_override =
		    calloc(INITDEFINES, sizeof(*ifo->dhcp_override));
		if (ifo->dhcp_override == NULL)
			logerr(__func__);
		else
			ifo->dhcp_override_len = INITDEFINES;
#endif

#if defined(INET6) && defined(INITDEFINENDS)
		ifo->nd_override =
		    calloc(INITDEFINENDS, sizeof(*ifo->nd_override));
		if (ifo->nd_override == NULL)
			logerr(__func__);
		else
			ifo->nd_override_len = INITDEFINENDS;
#endif
#if defined(INET6) && defined(INITDEFINE6S)
		ifo->dhcp6_override =
		    calloc(INITDEFINE6S, sizeof(*ifo->dhcp6_override));
		if (ifo->dhcp6_override == NULL)
			logerr(__func__);
		else
			ifo->dhcp6_override_len = INITDEFINE6S;
#endif

		/* Now load our embedded config */
#ifdef EMBEDDED_CONFIG
		buflen = dhcp_readfile(ctx, EMBEDDED_CONFIG, buf, sizeof(buf));
		if (buflen == -1) {
			logerr("%s: %s", __func__, EMBEDDED_CONFIG);
			return ifo;
		}
		if (buf[buflen - 1] != '\0') {
			if ((size_t)buflen < sizeof(buf) - 1)
				buflen++;
			buf[buflen - 1] = '\0';
		}
#else
		buflen = (ssize_t)strlcpy(buf, dhcpcd_embedded_conf,
		    sizeof(buf));
		if ((size_t)buflen >= sizeof(buf)) {
			logerrx("%s: embedded config too big", __func__);
			return ifo;
		}
		/* Our embedded config is NULL terminated */
#endif
		bp = buf;
		while ((line = get_line(&bp, &buflen)) != NULL) {
			option = strsep(&line, " \t");
			if (line)
				line = strskipwhite(line);
			/* Trim trailing whitespace */
			if (line) {
				p = line + strlen(line) - 1;
				while (p != line &&
				    (*p == ' ' || *p == '\t') &&
				    *(p - 1) != '\\')
					*p-- = '\0';
			}
			parse_config_line(ctx, NULL, ifo, option, line,
			    &ldop, &edop);
		}

#ifdef INET
		ctx->dhcp_opts = ifo->dhcp_override;
		ctx->dhcp_opts_len = ifo->dhcp_override_len;
#else
		for (i = 0, opt = ifo->dhcp_override;
		    i < ifo->dhcp_override_len;
		    i++, opt++)
			free_dhcp_opt_embenc(opt);
		free(ifo->dhcp_override);
#endif
		ifo->dhcp_override = NULL;
		ifo->dhcp_override_len = 0;

#ifdef INET6
		ctx->nd_opts = ifo->nd_override;
		ctx->nd_opts_len = ifo->nd_override_len;
#ifdef DHCP6
		ctx->dhcp6_opts = ifo->dhcp6_override;
		ctx->dhcp6_opts_len = ifo->dhcp6_override_len;
#endif
#else
		for (i = 0, opt = ifo->nd_override;
		    i < ifo->nd_override_len;
		    i++, opt++)
			free_dhcp_opt_embenc(opt);
		free(ifo->nd_override);
		for (i = 0, opt = ifo->dhcp6_override;
		    i < ifo->dhcp6_override_len;
		    i++, opt++)
			free_dhcp_opt_embenc(opt);
		free(ifo->dhcp6_override);
#endif
		ifo->nd_override = NULL;
		ifo->nd_override_len = 0;
		ifo->dhcp6_override = NULL;
		ifo->dhcp6_override_len = 0;

		ctx->vivso = ifo->vivso_override;
		ctx->vivso_len = ifo->vivso_override_len;
		ifo->vivso_override = NULL;
		ifo->vivso_override_len = 0;
	}

	/* Parse our options file */
	buflen = dhcp_readfile(ctx, ctx->cffile, buf, sizeof(buf));
	if (buflen == -1) {
		/* dhcpcd can continue without it, but no DNS options
		 * would be requested ... */
		logerr("%s: %s", __func__, ctx->cffile);
		return ifo;
	}
	if (buf[buflen - 1] != '\0') {
		if ((size_t)buflen < sizeof(buf) - 1)
			buflen++;
		buf[buflen - 1] = '\0';
	}
	dhcp_filemtime(ctx, ctx->cffile, &ifo->mtime);

	ldop = edop = NULL;
	skip = have_profile = new_block = 0;
	had_block = ifname == NULL ? 1 : 0;
	bp = buf;
	while ((line = get_line(&bp, &buflen)) != NULL) {
		option = strsep(&line, " \t");
		if (line)
			line = strskipwhite(line);
		/* Trim trailing whitespace */
		if (line) {
			p = line + strlen(line) - 1;
			while (p != line &&
			    (*p == ' ' || *p == '\t') &&
			    *(p - 1) != '\\')
				*p-- = '\0';
		}
		if (skip == 0 && new_block) {
			had_block = 1;
			new_block = 0;
			ifo->options &= ~DHCPCD_WAITOPTS;
			SET_CONFIG_BLOCK(ifo);
		}

		/* Start of an interface block, skip if not ours */
		if (strcmp(option, "interface") == 0) {
			char **n;

			new_block = 1;
			if (line == NULL) {
				/* No interface given */
				skip = 1;
				continue;
			}
			if (ifname && strcmp(line, ifname) == 0)
				skip = 0;
			else
				skip = 1;
			if (ifname)
				continue;

			n = reallocarray(ctx->ifcv,
			    (size_t)ctx->ifcc + 1, sizeof(char *));
			if (n == NULL) {
				logerr(__func__);
				continue;
			}
			ctx->ifcv = n;
			ctx->ifcv[ctx->ifcc] = strdup(line);
			if (ctx->ifcv[ctx->ifcc] == NULL) {
				logerr(__func__);
				continue;
			}
			ctx->ifcc++;
			continue;
		}
		/* Start of an ssid block, skip if not ours */
		if (strcmp(option, "ssid") == 0) {
			new_block = 1;
			if (ssid && line && strcmp(line, ssid) == 0)
				skip = 0;
			else
				skip = 1;
			continue;
		}
		/* Start of a profile block, skip if not ours */
		if (strcmp(option, "profile") == 0) {
			new_block = 1;
			if (profile && line && strcmp(line, profile) == 0) {
				skip = 0;
				have_profile = 1;
			} else
				skip = 1;
			continue;
		}
		/* Skip arping if we have selected a profile but not parsing
		 * one. */
		if (profile && !have_profile && strcmp(option, "arping") == 0)
			continue;
		if (skip)
			continue;

		parse_config_line(ctx, ifname, ifo, option, line, &ldop, &edop);
	}

	if (profile && !have_profile) {
		free_options(ctx, ifo);
		errno = ENOENT;
		return NULL;
	}

	if (!had_block)
		ifo->options &= ~DHCPCD_WAITOPTS;
	CLEAR_CONFIG_BLOCK(ifo);
	finish_config(ifo);
	return ifo;
}

int
add_options(struct dhcpcd_ctx *ctx, const char *ifname,
    struct if_options *ifo, int argc, char **argv)
{
	int oi, opt, r;
	unsigned long long wait_opts;

	if (argc == 0)
		return 1;

	optind = 0;
	r = 1;
	/* Don't apply the command line wait options to each interface,
	 * only use the dhcpcd.conf entry for that. */
	if (ifname != NULL)
		wait_opts = ifo->options & DHCPCD_WAITOPTS;
	while ((opt = getopt_long(argc, argv,
	    ctx->options & DHCPCD_PRINT_PIDFILE ? NOERR_IF_OPTS : IF_OPTS,
	    cf_options, &oi)) != -1)
	{
		r = parse_option(ctx, ifname, ifo, opt, optarg, NULL, NULL);
		if (r != 1)
			break;
	}
	if (ifname != NULL) {
		ifo->options &= ~DHCPCD_WAITOPTS;
		ifo->options |= wait_opts;
	}

	finish_config(ifo);
	return r;
}

void
free_options(struct dhcpcd_ctx *ctx, struct if_options *ifo)
{
	size_t i;
#ifdef RT_FREE_ROUTE_TABLE
	struct interface *ifp;
	struct rt *rt;
#endif
	struct dhcp_opt *opt;
	struct vivco *vo;
#ifdef AUTH
	struct token *token;
#endif

	if (ifo == NULL)
		return;

	if (ifo->environ) {
		i = 0;
		while (ifo->environ[i])
			free(ifo->environ[i++]);
		free(ifo->environ);
	}
	if (ifo->config) {
		i = 0;
		while (ifo->config[i])
			free(ifo->config[i++]);
		free(ifo->config);
	}

#ifdef RT_FREE_ROUTE_TABLE
	/* Stupidly, we don't know the interface when creating the options.
	 * As such, make sure each route has one so they can goto the
	 * free list. */
	ifp = ctx->ifaces != NULL ? TAILQ_FIRST(ctx->ifaces) : NULL;
	if (ifp != NULL) {
		RB_TREE_FOREACH(rt, &ifo->routes) {
			if (rt->rt_ifp == NULL)
				rt->rt_ifp = ifp;
		}
	}
#endif
	rt_headclear0(ctx, &ifo->routes, AF_UNSPEC);

	free(ifo->arping);
	free(ifo->blacklist);
	free(ifo->fallback);

	for (opt = ifo->dhcp_override;
	    ifo->dhcp_override_len > 0;
	    opt++, ifo->dhcp_override_len--)
		free_dhcp_opt_embenc(opt);
	free(ifo->dhcp_override);
	for (opt = ifo->nd_override;
	    ifo->nd_override_len > 0;
	    opt++, ifo->nd_override_len--)
		free_dhcp_opt_embenc(opt);
	free(ifo->nd_override);
	for (opt = ifo->dhcp6_override;
	    ifo->dhcp6_override_len > 0;
	    opt++, ifo->dhcp6_override_len--)
		free_dhcp_opt_embenc(opt);
	free(ifo->dhcp6_override);
	for (vo = ifo->vivco;
	    ifo->vivco_len > 0;
	    vo++, ifo->vivco_len--)
		free(vo->data);
	free(ifo->vivco);
	for (opt = ifo->vivso_override;
	    ifo->vivso_override_len > 0;
	    opt++, ifo->vivso_override_len--)
		free_dhcp_opt_embenc(opt);
	free(ifo->vivso_override);

#if defined(INET6) && !defined(SMALL)
	for (; ifo->ia_len > 0; ifo->ia_len--)
		free(ifo->ia[ifo->ia_len - 1].sla);
#endif
	free(ifo->ia);

#ifdef AUTH
	while ((token = TAILQ_FIRST(&ifo->auth.tokens))) {
		TAILQ_REMOVE(&ifo->auth.tokens, token, next);
		if (token->realm_len)
			free(token->realm);
		free(token->key);
		free(token);
	}
#endif
	free(ifo);
}
