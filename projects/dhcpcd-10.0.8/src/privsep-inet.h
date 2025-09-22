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

#ifndef PRIVSEP_INET_H
#define PRIVSEP_INET_H

bool ps_inet_canstart(const struct dhcpcd_ctx *);
pid_t ps_inet_start(struct dhcpcd_ctx *);
int ps_inet_stop(struct dhcpcd_ctx *);
ssize_t ps_inet_cmd(struct dhcpcd_ctx *, struct ps_msghdr *, struct msghdr *);
ssize_t ps_inet_dispatch(void *, struct ps_msghdr *, struct msghdr *);

#ifdef INET
struct ipv4_addr;
ssize_t ps_inet_openbootp(struct ipv4_addr *);
ssize_t ps_inet_closebootp(struct ipv4_addr *);
ssize_t ps_inet_sendbootp(struct interface *, const struct msghdr *);
#endif

#ifdef INET6
struct ipv6_addr;
#ifdef __sun
ssize_t ps_inet_opennd(struct interface *);
ssize_t ps_inet_closend(struct interface *);
#endif
ssize_t ps_inet_sendnd(struct interface *, const struct msghdr *);
#ifdef DHCP6
ssize_t ps_inet_opendhcp6(struct ipv6_addr *);
ssize_t ps_inet_closedhcp6(struct ipv6_addr *);
ssize_t ps_inet_senddhcp6(struct interface *, const struct msghdr *);
#endif /* DHCP6 */
#endif /* INET6 */
#endif
