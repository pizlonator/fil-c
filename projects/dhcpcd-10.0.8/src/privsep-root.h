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

#ifndef PRIVSEP_ROOT_H
#define PRIVSEP_ROOT_H

#include "if.h"

#if defined(PRIVSEP) && (defined(HAVE_CAPSICUM) || defined(__linux__))
#define PRIVSEP_GETIFADDRS
#endif

pid_t ps_root_start(struct dhcpcd_ctx *ctx);
void ps_root_close(struct dhcpcd_ctx *ctx);
int ps_root_stop(struct dhcpcd_ctx *ctx);
void ps_root_signalcb(int, void *);

ssize_t ps_root_readerror(struct dhcpcd_ctx *, void *, size_t);
ssize_t ps_root_mreaderror(struct dhcpcd_ctx *, void **, size_t *);
ssize_t ps_root_ioctl(struct dhcpcd_ctx *, ioctl_request_t, void *, size_t);
ssize_t ps_root_ip6forwarding(struct dhcpcd_ctx *, const char *);
ssize_t ps_root_unlink(struct dhcpcd_ctx *, const char *);
ssize_t ps_root_filemtime(struct dhcpcd_ctx *, const char *, time_t *);
ssize_t ps_root_readfile(struct dhcpcd_ctx *, const char *, void *, size_t);
ssize_t ps_root_writefile(struct dhcpcd_ctx *, const char *, mode_t,
    const void *, size_t);
ssize_t ps_root_logreopen(struct dhcpcd_ctx *);
ssize_t ps_root_script(struct dhcpcd_ctx *, const void *, size_t);
ssize_t ps_root_stopprocesses(struct dhcpcd_ctx *);
int ps_root_getauthrdm(struct dhcpcd_ctx *, uint64_t *);
#ifdef PRIVSEP_GETIFADDRS
int ps_root_getifaddrs(struct dhcpcd_ctx *, struct ifaddrs **);
#endif

ssize_t ps_root_os(struct dhcpcd_ctx *, struct ps_msghdr *, struct msghdr *,
    void **, size_t *, bool *);
#if defined(BSD) || defined(__sun)
ssize_t ps_root_route(struct dhcpcd_ctx *, void *, size_t);
ssize_t ps_root_ioctllink(struct dhcpcd_ctx *, unsigned long, void *, size_t);
ssize_t ps_root_ioctl6(struct dhcpcd_ctx *, unsigned long, void *, size_t);
ssize_t ps_root_indirectioctl(struct dhcpcd_ctx *, unsigned long, const char *,
    void *, size_t);
ssize_t ps_root_ifignoregroup(struct dhcpcd_ctx *, const char *);
ssize_t ps_root_sysctl(struct dhcpcd_ctx *, const int *, unsigned int,
    void *, size_t *, const void *, size_t);
#endif
#ifdef __linux__
ssize_t ps_root_sendnetlink(struct dhcpcd_ctx *, int, struct msghdr *);
#endif

#ifdef PLUGIN_DEV
int ps_root_dev_initialised(struct dhcpcd_ctx *, const char *);
int ps_root_dev_listening(struct dhcpcd_ctx *);
#endif

#endif
