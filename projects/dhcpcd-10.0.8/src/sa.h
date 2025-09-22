/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Socket Address handling for dhcpcd
 * Copyright (c) 2015-2023 Roy Marples <roy@marples.name>
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

#ifndef SA_H
#define SA_H

#include <sys/socket.h>
#include <netinet/in.h>

union sa_ss {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};

#ifdef BSD
#define HAVE_SA_LEN
#endif

/* Allow for a sockaddr_dl being printed too. */
#define INET_MAX_ADDRSTRLEN	(20 * 3)

#ifdef INET
#define satosin(sa) ((struct sockaddr_in *)(void *)(sa))
#define satocsin(sa) ((const struct sockaddr_in *)(const void *)(sa))
#endif
#ifdef INET6
#define satosin6(sa) ((struct sockaddr_in6 *)(void *)(sa))
#define satocsin6(sa) ((const struct sockaddr_in6 *)(const void *)(sa))
#endif

socklen_t sa_addroffset(const struct sockaddr *sa);
socklen_t sa_addrlen(const struct sockaddr *sa);
#ifdef HAVE_SA_LEN
#define	sa_len(sa)	((sa)->sa_len)
#else
socklen_t sa_len(const struct sockaddr *sa);
#endif
bool sa_is_unspecified(const struct sockaddr *);
bool sa_is_allones(const struct sockaddr *);
bool sa_is_loopback(const struct sockaddr *);
void *sa_toaddr(struct sockaddr *);
int sa_toprefix(const struct sockaddr *);
int sa_fromprefix(struct sockaddr *, int);
void in6_addr_fromprefix(struct in6_addr *, int);
const char *sa_addrtop(const struct sockaddr *, char *, socklen_t);
int sa_cmp(const struct sockaddr *, const struct sockaddr *);
void sa_in_init(struct sockaddr *, const struct in_addr *);
void sa_in6_init(struct sockaddr *, const struct in6_addr *);

#endif
