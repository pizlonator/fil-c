/* sha3.h

   The sha3 hash function (aka Keccak).

   Copyright (C) 2012 Niels Möller

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

#ifndef GNUTLS_LIB_NETTLE_INT_SHA3_SHAKE_H_INCLUDED
#define GNUTLS_LIB_NETTLE_INT_SHA3_SHAKE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* This function has already been public in older Nettle releases */
#undef sha3_256_shake

/* Name mangling */
#define sha3_128_init gnutls_nettle_backport_sha3_128_init
#define sha3_128_update gnutls_nettle_backport_sha3_128_update
#define sha3_128_shake gnutls_nettle_backport_sha3_128_shake
#define sha3_128_shake_output gnutls_nettle_backport_sha3_128_shake_output
#define sha3_256_shake gnutls_nettle_backport_sha3_256_shake
#define sha3_256_shake_output gnutls_nettle_backport_sha3_256_shake_output

#define SHA3_128_DIGEST_SIZE 16
#define SHA3_128_BLOCK_SIZE 168

struct sha3_128_ctx {
	struct sha3_state state;
	unsigned index;
	uint8_t block[SHA3_128_BLOCK_SIZE];
};

void sha3_128_init(struct sha3_128_ctx *ctx);

void sha3_128_update(struct sha3_128_ctx *ctx, size_t length,
		     const uint8_t *data);

void sha3_128_shake(struct sha3_128_ctx *ctx, size_t length, uint8_t *digest);

void sha3_128_shake_output(struct sha3_128_ctx *ctx, size_t length,
			   uint8_t *digest);

/* Alternative digest function implementing shake256, with arbitrary
   digest size */
void sha3_256_shake(struct sha3_256_ctx *ctx, size_t length, uint8_t *digest);

/* Unlike sha3_256_shake, this function can be called multiple times
   to retrieve output from shake256 in an incremental manner */
void sha3_256_shake_output(struct sha3_256_ctx *ctx, size_t length,
			   uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* GNUTLS_LIB_NETTLE_INT_SHA3_SHAKE_H_INCLUDED */
