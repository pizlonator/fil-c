/* magma.h

   The GOST R 34.12-2015 (MAGMA) cipher function.

   Copyright (C) 2017 Dmitry Eremin-Solenikov

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
   not, see https://www.gnu.org/licenses/.
*/

#ifndef GNUTLS_LIB_NETTLE_MAGMA_H_INCLUDED
#define GNUTLS_LIB_NETTLE_MAGMA_H_INCLUDED

#include "config.h"

#ifndef HAVE_NETTLE_MAGMA_SET_KEY

#include <nettle/nettle-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define magma_set_key _gnutls_magma_set_key
#define magma_set_param _gnutls_magma_set_param
#define magma_encrypt _gnutls_magma_encrypt
#define magma_decrypt _gnutls_magma_decrypt

#define MAGMA_KEY_SIZE 32
#define MAGMA_BLOCK_SIZE 8

struct magma_ctx {
	uint32_t key[MAGMA_KEY_SIZE / 4];
};

void magma_set_key(struct magma_ctx *ctx, const uint8_t *key);

void magma_encrypt(const struct magma_ctx *ctx, size_t length, uint8_t *dst,
		   const uint8_t *src);
void magma_decrypt(const struct magma_ctx *ctx, size_t length, uint8_t *dst,
		   const uint8_t *src);

#ifdef __cplusplus
}
#endif
#endif
#endif /* GNUTLS_LIB_NETTLE_MAGMA_H_INCLUDED */
