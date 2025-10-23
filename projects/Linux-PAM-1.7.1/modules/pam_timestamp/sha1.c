/* Yet another SHA-1 implementation.
 *
 * Copyright (c) 2003 Red Hat, Inc.
 * Written by Nalin Dahyabhai <nalin@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/* See http://www.itl.nist.gov/fipspubs/fip180-1.htm for descriptions. */

#include "pam_inline.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <unistd.h>
#include "sha1.h"

static const unsigned char
padding[SHA1_BLOCK_SIZE] = {
	0x80, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
};

static uint32_t
F(uint32_t b, uint32_t c, uint32_t d)
{
	return (b & c) | ((~b) & d);
}

static uint32_t
G(uint32_t b, uint32_t c, uint32_t d)
{
	return b ^ c ^ d;
}

static uint32_t
H(uint32_t b, uint32_t c, uint32_t d)
{
	return (b & c) | (b & d) | (c & d);
}

static uint32_t
RL(uint32_t n, uint32_t s)
{
	return (n << s) | (n >> (32 - s));
}

static uint32_t
sha1_round(uint32_t (*FUNC)(uint32_t, uint32_t, uint32_t),
      uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e,
      uint32_t i, uint32_t n)
{
	return RL(a, 5) + FUNC(b, c, d) + e + i + n;
}

void
sha1_init(struct sha1_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->a = 0x67452301;
	ctx->b = 0xefcdab89;
	ctx->c = 0x98badcfe;
	ctx->d = 0x10325476;
	ctx->e = 0xc3d2e1f0;
}

static void
sha1_process(struct sha1_context *ctx, uint32_t buffer[SHA1_BLOCK_SIZE / 4])
{
	uint32_t a, b, c, d, e, temp;
	uint32_t data[80];
	int i;

	for (i = 0; i < 16; i++) {
		data[i] = htonl(buffer[i]);
	}
	for (i = 16; i < 80; i++) {
		data[i] = RL(data[i - 3] ^ data[i - 8] ^ data[i - 14] ^ data[i - 16], 1);
	}

	a = ctx->a;
	b = ctx->b;
	c = ctx->c;
	d = ctx->d;
	e = ctx->e;

	for (i =  0; i < 20; i++) {
		temp = sha1_round(F, a, b, c, d, e, data[i], 0x5a827999);
		e = d; d = c; c = RL(b, 30); b = a; a = temp;
	}
	for (i = 20; i < 40; i++) {
		temp = sha1_round(G, a, b, c, d, e, data[i], 0x6ed9eba1);
		e = d; d = c; c = RL(b, 30); b = a; a = temp;
	}
	for (i = 40; i < 60; i++) {
		temp = sha1_round(H, a, b, c, d, e, data[i], 0x8f1bbcdc);
		e = d; d = c; c = RL(b, 30); b = a; a = temp;
	}
	for (i = 60; i < 80; i++) {
		temp = sha1_round(G, a, b, c, d, e, data[i], 0xca62c1d6);
		e = d; d = c; c = RL(b, 30); b = a; a = temp;
	}

	ctx->a += a;
	ctx->b += b;
	ctx->c += c;
	ctx->d += d;
	ctx->e += e;

	pam_overwrite_n(buffer, sizeof(buffer[0]) * SHA1_BLOCK_SIZE / 4);
	pam_overwrite_array(data);
}

void
sha1_update(struct sha1_context *ctx, const unsigned char *data, size_t length)
{
	size_t i = 0, l = length, c, t;
	uint32_t count = 0;

	/* Process any pending + data blocks. */
	while (l + ctx->pending_count >= SHA1_BLOCK_SIZE) {
		c = ctx->pending_count;
		t = SHA1_BLOCK_SIZE - c;
		memcpy(ctx->pending.c + c, &data[i], t);
		sha1_process(ctx, ctx->pending.i);
		i += t;
		l -= t;
		ctx->pending_count = 0;
	}

	/* Save what's left of the data block as a pending data block. */
	c = ctx->pending_count;
	memcpy(ctx->pending.c + c, &data[i], l);
	ctx->pending_count += l;

	/* Update the message length. */
	ctx->count += length;

	/* Update our internal counts. */
	if (length != 0) {
		count = ctx->counts[0];
		ctx->counts[0] += length;
		if (count >= ctx->counts[0]) {
			ctx->counts[1]++;
		}
	}
}

size_t
sha1_output(struct sha1_context *ctx, unsigned char *out)
{
	struct sha1_context ctx2;

	/* Output the sum. */
	if (out != NULL) {
		uint32_t c;
		memcpy(&ctx2, ctx, sizeof(ctx2));

		/* Pad this block. */
		c = ctx2.pending_count;
		memcpy(ctx2.pending.c + c,
		       padding, SHA1_BLOCK_SIZE - c);

		/* Do we need to process two blocks now? */
		if (c >= (SHA1_BLOCK_SIZE - (sizeof(uint32_t) * 2))) {
			/* Process this block. */
			sha1_process(&ctx2, ctx2.pending.i);
			/* Set up another block. */
			ctx2.pending_count = 0;
			memset(ctx2.pending.c, 0, SHA1_BLOCK_SIZE);
                        ctx2.pending.c[0] =
				(c == SHA1_BLOCK_SIZE) ? 0x80 : 0;
		}

		/* Process the final block. */
		ctx2.counts[1] <<= 3;
		if (ctx2.counts[0] >> 29) {
			ctx2.counts[1] |=
			(ctx2.counts[0] >> 29);
		}
		ctx2.counts[0] <<= 3;
		ctx2.counts[0] = htonl(ctx2.counts[0]);
		ctx2.counts[1] = htonl(ctx2.counts[1]);
		memcpy(ctx2.pending.c + 56,
		       &ctx2.counts[1], sizeof(uint32_t));
		memcpy(ctx2.pending.c + 60,
		       &ctx2.counts[0], sizeof(uint32_t));
		sha1_process(&ctx2, ctx2.pending.i);

		/* Output the data. */
		out[ 3] = (ctx2.a >>  0) & 0xff;
		out[ 2] = (ctx2.a >>  8) & 0xff;
		out[ 1] = (ctx2.a >> 16) & 0xff;
		out[ 0] = (ctx2.a >> 24) & 0xff;

		out[ 7] = (ctx2.b >>  0) & 0xff;
		out[ 6] = (ctx2.b >>  8) & 0xff;
		out[ 5] = (ctx2.b >> 16) & 0xff;
		out[ 4] = (ctx2.b >> 24) & 0xff;

		out[11] = (ctx2.c >>  0) & 0xff;
		out[10] = (ctx2.c >>  8) & 0xff;
		out[ 9] = (ctx2.c >> 16) & 0xff;
		out[ 8] = (ctx2.c >> 24) & 0xff;

		out[15] = (ctx2.d >>  0) & 0xff;
		out[14] = (ctx2.d >>  8) & 0xff;
		out[13] = (ctx2.d >> 16) & 0xff;
		out[12] = (ctx2.d >> 24) & 0xff;

		out[19] = (ctx2.e >>  0) & 0xff;
		out[18] = (ctx2.e >>  8) & 0xff;
		out[17] = (ctx2.e >> 16) & 0xff;
		out[16] = (ctx2.e >> 24) & 0xff;
	}

	return SHA1_OUTPUT_SIZE;
}
