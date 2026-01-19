/* sntrup761.h  -  Streamlined NTRU Prime sntrup761 key-encapsulation method
 * Copyright (C) 2023 Simon Josefsson <simon@josefsson.org>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * For a description of the algorithm, see:
 *   https://ntruprime.cr.yp.to/
 */

/*
 * Derived from public domain source, written by (in alphabetical order):
 * - Daniel J. Bernstein
 * - Chitchanok Chuengsatiansup
 * - Tanja Lange
 * - Christine van Vredendaal
 */

#ifndef SNTRUP761_H
#define SNTRUP761_H

#include <string.h>
#include <stdint.h>

#ifdef _GCRYPT_IN_LIBGCRYPT
/**** Start of the glue code to libgcrypt ****/
#include "gcrypt-int.h"

static inline void
crypto_hash_sha512 (unsigned char *out,
		    const unsigned char *in, size_t inlen)
{
  _gcry_md_hash_buffer (GCRY_MD_SHA512, out, in, inlen);
}

#define sntrup761_keypair _gcry_sntrup761_keypair
#define sntrup761_enc     _gcry_sntrup761_enc
#define sntrup761_dec     _gcry_sntrup761_dec
/**** End of the glue code ****/
#else
#define SNTRUP761_SECRETKEY_SIZE 1763
#define SNTRUP761_PUBLICKEY_SIZE 1158
#define SNTRUP761_CIPHERTEXT_SIZE 1039
#define SNTRUP761_SIZE 32
#endif

typedef void sntrup761_random_func (void *ctx, size_t length, uint8_t *dst);

void
sntrup761_keypair (uint8_t *pk, uint8_t *sk,
		   void *random_ctx, sntrup761_random_func *random);

void
sntrup761_enc (uint8_t *c, uint8_t *k, const uint8_t *pk,
	       void *random_ctx, sntrup761_random_func *random);

void
sntrup761_dec (uint8_t *k, const uint8_t *c, const uint8_t *sk);

#endif /* SNTRUP761_H */
