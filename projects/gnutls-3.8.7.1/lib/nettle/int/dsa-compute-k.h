/*
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Author: Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
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

#ifndef GNUTLS_LIB_NETTLE_INT_DSA_COMPUTE_K_H
#define GNUTLS_LIB_NETTLE_INT_DSA_COMPUTE_K_H

#include <gnutls/gnutls.h>
#include <nettle/bignum.h> /* includes gmp.h */

#define BITS_TO_LIMBS(bits) (((bits) + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS)

/* The maximum size of q, chosen from the fact that we support
 * 521-bit elliptic curve generator and 512-bit DSA subgroup at
 * maximum. */
#define MAX_Q_BITS 521
#define MAX_Q_SIZE ((MAX_Q_BITS + 7) / 8)
#define MAX_Q_LIMBS BITS_TO_LIMBS(MAX_Q_BITS)

#define MAX_HASH_BITS (MAX_HASH_SIZE * 8)
#define MAX_HASH_LIMBS BITS_TO_LIMBS(MAX_HASH_BITS)

#define DSA_COMPUTE_K_ITCH MAX(MAX_Q_LIMBS, MAX_HASH_LIMBS)

int _gnutls_dsa_compute_k(mp_limb_t *h, const mp_limb_t *q, const mp_limb_t *x,
			  mp_size_t qn, mp_bitcnt_t q_bits,
			  gnutls_mac_algorithm_t mac, const uint8_t *digest,
			  size_t length);

void _gnutls_dsa_compute_k_finish(uint8_t *k, size_t nbytes, mp_limb_t *h,
				  mp_size_t n);

void _gnutls_ecdsa_compute_k_finish(uint8_t *k, size_t nbytes, mp_limb_t *h,
				    mp_size_t n);

#endif /* GNUTLS_LIB_NETTLE_INT_DSA_COMPUTE_K_H */
