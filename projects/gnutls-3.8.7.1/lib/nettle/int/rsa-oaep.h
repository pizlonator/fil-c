/* rsa.h

   The RSA publickey algorithm.

   Copyright (C) 2001, 2002 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#ifndef GNUTLS_LIB_NETTLE_INT_RSA_OAEP_H_INCLUDED
#define GNUTLS_LIB_NETTLE_INT_RSA_OAEP_H_INCLUDED

#include <nettle/nettle-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Name mangling */
#define rsa_oaep_sha256_encrypt gnutls_nettle_backport_rsa_oaep_sha256_encrypt
#define rsa_oaep_sha256_decrypt gnutls_nettle_backport_rsa_oaep_sha256_decrypt
#define rsa_oaep_sha384_encrypt gnutls_nettle_backport_rsa_oaep_sha384_encrypt
#define rsa_oaep_sha384_decrypt gnutls_nettle_backport_rsa_oaep_sha384_decrypt
#define rsa_oaep_sha512_encrypt gnutls_nettle_backport_rsa_oaep_sha512_encrypt
#define rsa_oaep_sha512_decrypt gnutls_nettle_backport_rsa_oaep_sha512_decrypt

/* RSA encryption, using OAEP */

int rsa_oaep_sha256_encrypt(const struct rsa_public_key *key, void *random_ctx,
			    nettle_random_func *random, size_t label_length,
			    const uint8_t *label, size_t length,
			    const uint8_t *message, uint8_t *ciphertext);

int rsa_oaep_sha256_decrypt(const struct rsa_public_key *pub,
			    const struct rsa_private_key *key, void *random_ctx,
			    nettle_random_func *random, size_t label_length,
			    const uint8_t *label, size_t *length,
			    uint8_t *message, const uint8_t *ciphertext);

int rsa_oaep_sha384_encrypt(const struct rsa_public_key *key, void *random_ctx,
			    nettle_random_func *random, size_t label_length,
			    const uint8_t *label, size_t length,
			    const uint8_t *message, uint8_t *ciphertext);

int rsa_oaep_sha384_decrypt(const struct rsa_public_key *pub,
			    const struct rsa_private_key *key, void *random_ctx,
			    nettle_random_func *random, size_t label_length,
			    const uint8_t *label, size_t *length,
			    uint8_t *message, const uint8_t *ciphertext);

int rsa_oaep_sha512_encrypt(const struct rsa_public_key *key, void *random_ctx,
			    nettle_random_func *random, size_t label_length,
			    const uint8_t *label, size_t length,
			    const uint8_t *message, uint8_t *ciphertext);

int rsa_oaep_sha512_decrypt(const struct rsa_public_key *pub,
			    const struct rsa_private_key *key, void *random_ctx,
			    nettle_random_func *random, size_t label_length,
			    const uint8_t *label, size_t *length,
			    uint8_t *message, const uint8_t *ciphertext);

#ifdef __cplusplus
}
#endif

#endif /* GNUTLS_LIB_NETTLE_INT_RSA_OAEP_H_INCLUDED */
