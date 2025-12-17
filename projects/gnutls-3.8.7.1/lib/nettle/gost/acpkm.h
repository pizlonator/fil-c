/* acpkm.h

   The R 1323565.1.017-2018 cipher function. See draft-irtf-cfrg-re-keying.

   Copyright (C) 2018 Dmitry Eremin-Solenikov

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

#ifndef NETTLE_ACPKM_H_INCLUDED
#define NETTLE_ACPKM_H_INCLUDED

#include <nettle/nettle-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define acpkm_crypt _gnutls_acpkm_crypt

struct acpkm_ctx {
	size_t N;
	size_t pos;
};

#define ACPKM_CTX(type)               \
	{                             \
		struct acpkm_ctx ctx; \
		type cipher;          \
	}

#define ACPKM_KEY_SIZE 32

void acpkm_crypt(struct acpkm_ctx *ctx, void *cipher,
		 nettle_cipher_func *encrypt, nettle_set_key_func *set_key,
		 size_t length, uint8_t *dst, const uint8_t *src);

#ifdef __cplusplus
}
#endif
#endif /* NETTLE_ACPKM_H_INCLUDED */
