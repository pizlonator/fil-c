/*
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * This file is part of GNUTLS.
 *
 * The GNUTLS library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#include "config.h"

#include "liboqs/sha3.h"

#include "dlwrap/oqs.h"
#include <assert.h>
#include <gnutls/crypto.h>
#include <string.h>

/* SHA3-256 */

static void SHA3_sha3_256(uint8_t *output, const uint8_t *input, size_t inplen)
{
	gnutls_hash_fast(GNUTLS_DIG_SHA3_256, input, inplen, output);
}

/* SHA3-256 incremental */

static void SHA3_sha3_256_inc_init(OQS_SHA3_sha3_256_inc_ctx *state)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHA3_256);
	assert(ret == 0);
	state->ctx = hd;
}

static void SHA3_sha3_256_inc_absorb(OQS_SHA3_sha3_256_inc_ctx *state,
				     const uint8_t *input, size_t inplen)
{
	int ret;

	ret = gnutls_hash((gnutls_hash_hd_t)state->ctx, input, inplen);
	assert(ret == 0);
}

static void SHA3_sha3_256_inc_finalize(uint8_t *output,
				       OQS_SHA3_sha3_256_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, output);
}

static void SHA3_sha3_256_inc_ctx_release(OQS_SHA3_sha3_256_inc_ctx *state)
{
	gnutls_hash_deinit((gnutls_hash_hd_t)state->ctx, NULL);
}

static void SHA3_sha3_256_inc_ctx_clone(OQS_SHA3_sha3_256_inc_ctx *dest,
					const OQS_SHA3_sha3_256_inc_ctx *src)
{
	dest->ctx = gnutls_hash_copy((gnutls_hash_hd_t)src->ctx);
}

static void SHA3_sha3_256_inc_ctx_reset(OQS_SHA3_sha3_256_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
}

/* SHA3-384 */

static void SHA3_sha3_384(uint8_t *output, const uint8_t *input, size_t inplen)
{
	gnutls_hash_fast(GNUTLS_DIG_SHA3_384, input, inplen, output);
}

/* SHA3-384 incremental */
static void SHA3_sha3_384_inc_init(OQS_SHA3_sha3_384_inc_ctx *state)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHA3_384);
	assert(ret == 0);
	state->ctx = hd;
}

static void SHA3_sha3_384_inc_absorb(OQS_SHA3_sha3_384_inc_ctx *state,
				     const uint8_t *input, size_t inplen)
{
	int ret;

	ret = gnutls_hash((gnutls_hash_hd_t)state->ctx, input, inplen);
	assert(ret == 0);
}

static void SHA3_sha3_384_inc_finalize(uint8_t *output,
				       OQS_SHA3_sha3_384_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, output);
}

static void SHA3_sha3_384_inc_ctx_release(OQS_SHA3_sha3_384_inc_ctx *state)
{
	gnutls_hash_deinit((gnutls_hash_hd_t)state->ctx, NULL);
}

static void SHA3_sha3_384_inc_ctx_clone(OQS_SHA3_sha3_384_inc_ctx *dest,
					const OQS_SHA3_sha3_384_inc_ctx *src)
{
	dest->ctx = gnutls_hash_copy((gnutls_hash_hd_t)src->ctx);
}

static void SHA3_sha3_384_inc_ctx_reset(OQS_SHA3_sha3_384_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
}

/* SHA3-512 */

static void SHA3_sha3_512(uint8_t *output, const uint8_t *input, size_t inplen)
{
	gnutls_hash_fast(GNUTLS_DIG_SHA3_512, input, inplen, output);
}

/* SHA3-512 incremental */

static void SHA3_sha3_512_inc_init(OQS_SHA3_sha3_512_inc_ctx *state)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHA3_512);
	assert(ret == 0);
	state->ctx = hd;
}

static void SHA3_sha3_512_inc_absorb(OQS_SHA3_sha3_512_inc_ctx *state,
				     const uint8_t *input, size_t inplen)
{
	int ret;

	ret = gnutls_hash((gnutls_hash_hd_t)state->ctx, input, inplen);
	assert(ret == 0);
}

static void SHA3_sha3_512_inc_finalize(uint8_t *output,
				       OQS_SHA3_sha3_512_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, output);
}

static void SHA3_sha3_512_inc_ctx_release(OQS_SHA3_sha3_512_inc_ctx *state)
{
	gnutls_hash_deinit((gnutls_hash_hd_t)state->ctx, NULL);
}

static void SHA3_sha3_512_inc_ctx_clone(OQS_SHA3_sha3_512_inc_ctx *dest,
					const OQS_SHA3_sha3_512_inc_ctx *src)
{
	dest->ctx = gnutls_hash_copy((gnutls_hash_hd_t)src->ctx);
}

static void SHA3_sha3_512_inc_ctx_reset(OQS_SHA3_sha3_512_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
}

/* SHAKE-128 */

static void SHA3_shake128(uint8_t *output, size_t outlen, const uint8_t *input,
			  size_t inplen)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHAKE_128);
	assert(ret == 0);

	ret = gnutls_hash(hd, input, inplen);
	assert(ret == 0);

	ret = gnutls_hash_squeeze(hd, output, outlen);
	assert(ret == 0);

	gnutls_hash_deinit(hd, NULL);
}

/* SHAKE-128 incremental
 */

static void SHA3_shake128_inc_init(OQS_SHA3_shake128_inc_ctx *state)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHAKE_128);
	assert(ret == 0);

	state->ctx = hd;
}

static void SHA3_shake128_inc_absorb(OQS_SHA3_shake128_inc_ctx *state,
				     const uint8_t *input, size_t inplen)
{
	int ret;

	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
	ret = gnutls_hash((gnutls_hash_hd_t)state->ctx, input, inplen);
	assert(ret == 0);
}

static void SHA3_shake128_inc_finalize(OQS_SHA3_shake128_inc_ctx *state)
{
	(void)state;
}

static void SHA3_shake128_inc_squeeze(uint8_t *output, size_t outlen,
				      OQS_SHA3_shake128_inc_ctx *state)
{
	int ret;

	ret = gnutls_hash_squeeze((gnutls_hash_hd_t)state->ctx, output, outlen);
	assert(ret == 0);
}

static void SHA3_shake128_inc_ctx_release(OQS_SHA3_shake128_inc_ctx *state)
{
	gnutls_hash_deinit((gnutls_hash_hd_t)state->ctx, NULL);
}

static void SHA3_shake128_inc_ctx_clone(OQS_SHA3_shake128_inc_ctx *dest,
					const OQS_SHA3_shake128_inc_ctx *src)
{
	dest->ctx = gnutls_hash_copy((gnutls_hash_hd_t)src->ctx);
}

static void SHA3_shake128_inc_ctx_reset(OQS_SHA3_shake128_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
}

/* SHAKE-256 */

static void SHA3_shake256(uint8_t *output, size_t outlen, const uint8_t *input,
			  size_t inplen)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHAKE_256);
	assert(ret == 0);

	ret = gnutls_hash(hd, input, inplen);
	assert(ret == 0);

	ret = gnutls_hash_squeeze(hd, output, outlen);
	assert(ret == 0);

	gnutls_hash_deinit(hd, NULL);
}

/* SHAKE-256 incremental */

static void SHA3_shake256_inc_init(OQS_SHA3_shake256_inc_ctx *state)
{
	gnutls_hash_hd_t hd;
	int ret;

	ret = gnutls_hash_init(&hd, GNUTLS_DIG_SHAKE_256);
	assert(ret == 0);

	state->ctx = hd;
}

static void SHA3_shake256_inc_absorb(OQS_SHA3_shake256_inc_ctx *state,
				     const uint8_t *input, size_t inplen)
{
	int ret;

	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
	ret = gnutls_hash((gnutls_hash_hd_t)state->ctx, input, inplen);
	assert(ret == 0);
}

static void SHA3_shake256_inc_finalize(OQS_SHA3_shake256_inc_ctx *state)
{
	(void)state;
}

static void SHA3_shake256_inc_squeeze(uint8_t *output, size_t outlen,
				      OQS_SHA3_shake256_inc_ctx *state)
{
	int ret;

	ret = gnutls_hash_squeeze((gnutls_hash_hd_t)state->ctx, output, outlen);
	assert(ret == 0);
}

static void SHA3_shake256_inc_ctx_release(OQS_SHA3_shake256_inc_ctx *state)
{
	gnutls_hash_deinit((gnutls_hash_hd_t)state->ctx, NULL);
}

static void SHA3_shake256_inc_ctx_clone(OQS_SHA3_shake256_inc_ctx *dest,
					const OQS_SHA3_shake256_inc_ctx *src)
{
	dest->ctx = gnutls_hash_copy((gnutls_hash_hd_t)src->ctx);
}

static void SHA3_shake256_inc_ctx_reset(OQS_SHA3_shake256_inc_ctx *state)
{
	gnutls_hash_output((gnutls_hash_hd_t)state->ctx, NULL);
}

static struct OQS_SHA3_callbacks sha3_callbacks = {
	SHA3_sha3_256,
	SHA3_sha3_256_inc_init,
	SHA3_sha3_256_inc_absorb,
	SHA3_sha3_256_inc_finalize,
	SHA3_sha3_256_inc_ctx_release,
	SHA3_sha3_256_inc_ctx_reset,
	SHA3_sha3_256_inc_ctx_clone,
	SHA3_sha3_384,
	SHA3_sha3_384_inc_init,
	SHA3_sha3_384_inc_absorb,
	SHA3_sha3_384_inc_finalize,
	SHA3_sha3_384_inc_ctx_release,
	SHA3_sha3_384_inc_ctx_reset,
	SHA3_sha3_384_inc_ctx_clone,
	SHA3_sha3_512,
	SHA3_sha3_512_inc_init,
	SHA3_sha3_512_inc_absorb,
	SHA3_sha3_512_inc_finalize,
	SHA3_sha3_512_inc_ctx_release,
	SHA3_sha3_512_inc_ctx_reset,
	SHA3_sha3_512_inc_ctx_clone,
	SHA3_shake128,
	SHA3_shake128_inc_init,
	SHA3_shake128_inc_absorb,
	SHA3_shake128_inc_finalize,
	SHA3_shake128_inc_squeeze,
	SHA3_shake128_inc_ctx_release,
	SHA3_shake128_inc_ctx_clone,
	SHA3_shake128_inc_ctx_reset,
	SHA3_shake256,
	SHA3_shake256_inc_init,
	SHA3_shake256_inc_absorb,
	SHA3_shake256_inc_finalize,
	SHA3_shake256_inc_squeeze,
	SHA3_shake256_inc_ctx_release,
	SHA3_shake256_inc_ctx_clone,
	SHA3_shake256_inc_ctx_reset,
};

void _gnutls_liboqs_sha3_init(void)
{
	GNUTLS_OQS_FUNC(OQS_SHA3_set_callbacks)(&sha3_callbacks);
}

void _gnutls_liboqs_sha3_deinit(void)
{
}
