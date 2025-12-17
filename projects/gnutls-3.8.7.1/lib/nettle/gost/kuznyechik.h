/* kuznyechik.h

   The GOST R 34.12-2015 (Kuznyechik) cipher function.

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

#ifndef GNUTLS_LIB_NETTLE_KUZNYECHIK_H_INCLUDED
#define GNUTLS_LIB_NETTLE_KUZNYECHIK_H_INCLUDED

#include <nettle/nettle-types.h>

#include "config.h"

#ifndef HAVE_NETTLE_KUZNYECHIK_SET_KEY

#ifdef __cplusplus
extern "C" {
#endif

#define kuznyechik_set_key _gnutls_kuznyechik_set_key
#define kuznyechik_set_param _gnutls_kuznyechik_set_param
#define kuznyechik_encrypt _gnutls_kuznyechik_encrypt
#define kuznyechik_decrypt _gnutls_kuznyechik_decrypt

#define KUZNYECHIK_KEY_SIZE 32
#define KUZNYECHIK_SUBKEYS_SIZE (16 * 10)
#define KUZNYECHIK_BLOCK_SIZE 16

struct kuznyechik_ctx {
	uint8_t key[KUZNYECHIK_SUBKEYS_SIZE];
	uint8_t dekey[KUZNYECHIK_SUBKEYS_SIZE];
};

void kuznyechik_set_key(struct kuznyechik_ctx *ctx, const uint8_t *key);

void kuznyechik_encrypt(const struct kuznyechik_ctx *ctx, size_t length,
			uint8_t *dst, const uint8_t *src);
void kuznyechik_decrypt(const struct kuznyechik_ctx *ctx, size_t length,
			uint8_t *dst, const uint8_t *src);

#ifdef __cplusplus
}
#endif
#endif
#endif /* GNUTLS_LIB_NETTLE_KUZNYECHIK_H_INCLUDED */
