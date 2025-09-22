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

#ifndef ELOOP_H
#define ELOOP_H

#include <time.h>

/* Handy macros to create subsecond timeouts */
#define	CSEC_PER_SEC		100
#define	MSEC_PER_SEC		1000
#define	NSEC_PER_CSEC		10000000
#define	NSEC_PER_MSEC		1000000
#define	NSEC_PER_SEC		1000000000

/* eloop queues are really only for deleting timeouts registered
 * for a function or object.
 * The idea being that one interface has different timeouts for
 * say DHCP and DHCPv6. */
#ifndef ELOOP_QUEUE
  #define ELOOP_QUEUE 1
#endif

/* Used for deleting a timeout for all queues. */
#define	ELOOP_QUEUE_ALL	0

/* Forward declare eloop - the content should be invisible to the outside */
struct eloop;

#define	ELE_READ	0x0001
#define	ELE_WRITE	0x0002
#define	ELE_ERROR	0x0100
#define	ELE_HANGUP	0x0200
#define	ELE_NVAL	0x0400

size_t eloop_event_count(const struct eloop  *);
int eloop_event_add(struct eloop *, int, unsigned short,
    void (*)(void *, unsigned short), void *);
int eloop_event_delete(struct eloop *, int);

unsigned long long eloop_timespec_diff(const struct timespec *tsp,
    const struct timespec *usp, unsigned int *nsp);
#define eloop_timeout_add_tv(eloop, tv, cb, ctx) \
    eloop_q_timeout_add_tv((eloop), ELOOP_QUEUE, (tv), (cb), (ctx))
#define eloop_timeout_add_sec(eloop, tv, cb, ctx) \
    eloop_q_timeout_add_sec((eloop), ELOOP_QUEUE, (tv), (cb), (ctx))
#define eloop_timeout_add_msec(eloop, ms, cb, ctx) \
    eloop_q_timeout_add_msec((eloop), ELOOP_QUEUE, (ms), (cb), (ctx))
#define eloop_timeout_delete(eloop, cb, ctx) \
    eloop_q_timeout_delete((eloop), ELOOP_QUEUE, (cb), (ctx))
int eloop_q_timeout_add_tv(struct eloop *, int,
    const struct timespec *, void (*)(void *), void *);
int eloop_q_timeout_add_sec(struct eloop *, int,
    unsigned int, void (*)(void *), void *);
int eloop_q_timeout_add_msec(struct eloop *, int,
    unsigned long, void (*)(void *), void *);
int eloop_q_timeout_delete(struct eloop *, int, void (*)(void *), void *);

int eloop_signal_set_cb(struct eloop *, const int *, size_t,
    void (*)(int, void *), void *);
int eloop_signal_mask(struct eloop *, sigset_t *oldset);

struct eloop * eloop_new(void);
void eloop_clear(struct eloop *, ...);
void eloop_free(struct eloop *);
void eloop_exit(struct eloop *, int);
void eloop_enter(struct eloop *);
int eloop_forked(struct eloop *);
int eloop_open(struct eloop *);
int eloop_start(struct eloop *, sigset_t *);

#endif
