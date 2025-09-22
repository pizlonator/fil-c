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

#ifndef AUTH_H
#define AUTH_H

#include "config.h"

#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#define DHCPCD_AUTH_SEND	(1 << 0)
#define DHCPCD_AUTH_REQUIRE	(1 << 1)
#define DHCPCD_AUTH_RDM_COUNTER	(1 << 2)

#define DHCPCD_AUTH_SENDREQUIRE	(DHCPCD_AUTH_SEND | DHCPCD_AUTH_REQUIRE)

#define AUTH_PROTO_TOKEN	0
#define AUTH_PROTO_DELAYED	1
#define AUTH_PROTO_DELAYEDREALM	2
#define AUTH_PROTO_RECONFKEY	3

#define AUTH_ALG_NONE		0
#define AUTH_ALG_HMAC_MD5	1

#define AUTH_RDM_MONOTONIC	0

struct token {
	TAILQ_ENTRY(token) next;
	uint32_t secretid;
	size_t realm_len;
	unsigned char *realm;
	size_t key_len;
	unsigned char *key;
	time_t expire;
};

TAILQ_HEAD(token_head, token);

struct auth {
	int options;
#ifdef AUTH
	uint8_t protocol;
	uint8_t algorithm;
	uint8_t rdm;
	uint64_t last_replay;
	uint8_t last_replay_set;
	struct token_head tokens;
	uint32_t token_snd_secretid;
	uint32_t token_rcv_secretid;
#endif
};

struct authstate {
	uint64_t replay;
	struct token *token;
	struct token *reconf;
};

void dhcp_auth_reset(struct authstate *);

const struct token * dhcp_auth_validate(struct authstate *,
    const struct auth *,
    const void *, size_t, int, int,
    const void *, size_t);

struct dhcpcd_ctx;
ssize_t dhcp_auth_encode(struct dhcpcd_ctx *, struct auth *,
    const struct token *,
    void *, size_t, int, int,
    void *, size_t);

int auth_get_rdm_monotonic(uint64_t *rdm);
#endif
