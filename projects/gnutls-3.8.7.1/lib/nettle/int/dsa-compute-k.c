/*
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Author: Daiki Ueno
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "dsa-compute-k.h"

#include "gnutls_int.h"
#include "mem.h"
#include "mpn-base256.h"
#include <string.h>

/* For mini-gmp */
#ifndef GMP_LIMB_BITS
#define GMP_LIMB_BITS GMP_NUMB_BITS
#endif

static inline int is_zero_limb(mp_limb_t x)
{
	x |= (x << 1);
	return ((x >> 1) - 1) >> (GMP_LIMB_BITS - 1);
}

static int sec_zero_p(const mp_limb_t *ap, mp_size_t n)
{
	volatile mp_limb_t w;
	mp_size_t i;

	for (i = 0, w = 0; i < n; i++)
		w |= ap[i];

	return is_zero_limb(w);
}

int _gnutls_dsa_compute_k(mp_limb_t *h, const mp_limb_t *q, const mp_limb_t *x,
			  mp_size_t qn, mp_bitcnt_t q_bits,
			  gnutls_mac_algorithm_t mac, const uint8_t *digest,
			  size_t length)
{
	uint8_t V[MAX_HASH_SIZE];
	uint8_t K[MAX_HASH_SIZE];
	uint8_t xp[MAX_Q_SIZE];
	uint8_t tp[MAX_Q_SIZE];
	mp_bitcnt_t h_bits = length * 8;
	mp_size_t hn = BITS_TO_LIMBS(h_bits);
	size_t nbytes = (q_bits + 7) / 8;
	const uint8_t c0 = 0x00;
	const uint8_t c1 = 0x01;
	mp_limb_t cy;
	gnutls_hmac_hd_t hd;
	int ret = 0;
	mp_limb_t scratch[MAX_Q_LIMBS];

	if (unlikely(q_bits > MAX_Q_BITS))
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	if (unlikely(length > MAX_HASH_SIZE))
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	/* int2octets(x) */
	mpn_get_base256(xp, nbytes, x, qn);

	/* bits2octets(h) */
	mpn_set_base256(h, hn, digest, length);

	if (hn < qn)
		/* qlen > blen: add zero bits to the left */
		mpn_zero(&h[hn], qn - hn);
	else if (h_bits > q_bits) {
		/* qlen < blen: keep the leftmost qlen bits.  We do this in 2
		 * steps because mpn_rshift only accepts shift count in the
		 * range 1 to mp_bits_per_limb-1.
		 */
		mp_bitcnt_t shift = h_bits - q_bits;

		if (shift / GMP_NUMB_BITS > 0) {
			mpn_copyi(h, &h[shift / GMP_NUMB_BITS], qn);
			hn -= shift / GMP_NUMB_BITS;
		}

		if (shift % GMP_NUMB_BITS > 0)
			mpn_rshift(h, h, hn, shift % GMP_NUMB_BITS);
	}

	cy = mpn_sub_n(h, h, q, qn);
	/* Fall back to addmul_1, if nettle is linked with mini-gmp. */
#if defined(mpn_cnd_add_n) && !NETTLE_USE_MINI_GMP
	mpn_cnd_add_n(cy, h, h, q, qn);
#else
	mpn_addmul_1(h, q, qn, cy != 0);
#endif
	mpn_get_base256(tp, nbytes, h, qn);

	/* Step b */
	memset(V, c1, length);

	/* Step c */
	memset(K, c0, length);

	/* Step d */
	ret = gnutls_hmac_init(&hd, mac, K, length);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, V, length);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, &c0, 1);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, xp, nbytes);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, tp, nbytes);
	if (ret < 0)
		goto out;
	gnutls_hmac_deinit(hd, K);

	/* Step e */
	ret = gnutls_hmac_fast(mac, K, length, V, length, V);
	if (ret < 0)
		goto out;

	/* Step f */
	ret = gnutls_hmac_init(&hd, mac, K, length);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, V, length);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, &c1, 1);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, xp, nbytes);
	if (ret < 0)
		goto out;
	ret = gnutls_hmac(hd, tp, nbytes);
	if (ret < 0)
		goto out;
	gnutls_hmac_deinit(hd, K);

	/* Step g */
	ret = gnutls_hmac_fast(mac, K, length, V, length, V);
	if (ret < 0)
		goto out;

	/* Step h */
	for (;;) {
		/* Step 1 */
		size_t tlen = 0;

		/* Step 2 */
		while (tlen < nbytes) {
			size_t remaining = MIN(nbytes - tlen, length);
			ret = gnutls_hmac_fast(mac, K, length, V, length, V);
			if (ret < 0)
				goto out;
			memcpy(&tp[tlen], V, remaining);
			tlen += remaining;
		}

		/* Step 3 */
		mpn_set_base256(h, qn, tp, tlen);
		if (tlen * 8 > q_bits)
			mpn_rshift(h, h, qn, tlen * 8 - q_bits);
		/* Check if k is in [1,q-1] */
		if (!sec_zero_p(h, qn) && mpn_sub_n(scratch, h, q, qn))
			break;

		ret = gnutls_hmac_init(&hd, mac, K, length);
		if (ret < 0)
			goto out;
		ret = gnutls_hmac(hd, V, length);
		if (ret < 0)
			goto out;
		ret = gnutls_hmac(hd, &c0, 1);
		if (ret < 0)
			goto out;
		gnutls_hmac_deinit(hd, K);

		ret = gnutls_hmac_fast(mac, K, length, V, length, V);
		if (ret < 0)
			goto out;
	}

out:
	zeroize_key(xp, sizeof(xp));
	zeroize_key(tp, sizeof(tp));

	return ret;
}

/* cancel-out dsa_sign's addition of 1 to random data */
void _gnutls_dsa_compute_k_finish(uint8_t *k, size_t nbytes, mp_limb_t *h,
				  mp_size_t n)
{
	/* Fall back to sub_1, if nettle is linked with mini-gmp. */
#ifdef mpn_sec_sub_1
	mp_limb_t t[MAX_Q_LIMBS];

	mpn_sec_sub_1(h, h, n, 1, t);
#else
	mpn_sub_1(h, h, n, 1);
#endif
	mpn_get_base256(k, nbytes, h, n);
}

void _gnutls_ecdsa_compute_k_finish(uint8_t *k, size_t nbytes, mp_limb_t *h,
				    mp_size_t n)
{
	mpn_get_base256(k, nbytes, h, n);
}
