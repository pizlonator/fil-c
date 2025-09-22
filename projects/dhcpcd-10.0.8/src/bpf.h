/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd: BPF arp and bootp filtering
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

#ifndef BPF_HEADER
#define BPF_HEADER

#define	BPF_EOF			0x01U
#define	BPF_PARTIALCSUM		0x02U
#define	BPF_BCAST		0x04U

/*
 * Even though we program the BPF filter should we trust it?
 * On Linux at least there is a window between opening the socket,
 * binding the interface and setting the filter where we receive data.
 * This data is NOT checked OR flushed and IS returned when reading.
 * We have no way of flushing it other than reading these packets!
 * But we don't know if they passed the filter or not ..... so we need
 * to validate each and every packet that comes through ourselves as well.
 * Even if Linux does fix this sorry state, who is to say other kernels
 * don't have bugs causing a similar effect?
 *
 * As such, let's strive to keep the filters just for pattern matching
 * to avoid waking dhcpcd up.
 *
 * If you want to be notified of any packet failing the BPF filter,
 * define BPF_DEBUG below.
 */
//#define	BPF_DEBUG

#include "dhcpcd.h"

struct bpf {
	const struct interface *bpf_ifp;
	int bpf_fd;
	unsigned int bpf_flags;
	void *bpf_buffer;
	size_t bpf_size;
	size_t bpf_len;
	size_t bpf_pos;
};

extern const char *bpf_name;
size_t bpf_frame_header_len(const struct interface *);
void *bpf_frame_header_src(const struct interface *, void *, size_t *);
void *bpf_frame_header_dst(const struct interface *, void *, size_t *);
int bpf_frame_bcast(const struct interface *, const void *);
struct bpf * bpf_open(const struct interface *,
    int (*)(const struct bpf *, const struct in_addr *),
    const struct in_addr *);
void bpf_close(struct bpf *);
int bpf_attach(int, void *, unsigned int);
ssize_t bpf_send(const struct bpf *, uint16_t, const void *, size_t);
ssize_t bpf_read(struct bpf *, void *, size_t);
int bpf_arp(const struct bpf *, const struct in_addr *);
int bpf_bootp(const struct bpf *, const struct in_addr *);
#endif
