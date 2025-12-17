/* acpkm.c

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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "acpkm.h"

static uint8_t acpkm_mesh_data[ACPKM_KEY_SIZE] = {
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a,
	0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
	0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
};

void acpkm_crypt(struct acpkm_ctx *ctx, void *cipher,
		 nettle_cipher_func *encrypt, nettle_set_key_func *set_key,
		 size_t length, uint8_t *dst, const uint8_t *src)
{
	size_t N = ctx->N;
	size_t part;
	uint8_t new_key[ACPKM_KEY_SIZE];

	/* Less than a block, no rekeying */
	if (ctx->pos + length < N) {
		encrypt(cipher, length, dst, src);
		ctx->pos += length;
		return;
	}

	for (part = N - ctx->pos; length >= part; part = N) {
		encrypt(cipher, part, dst, src);
		src += part;
		dst += part;
		length -= part;

		/* Rekey */
		encrypt(cipher, ACPKM_KEY_SIZE, new_key, acpkm_mesh_data);
		set_key(cipher, new_key);
	}

	if (length != 0)
		encrypt(cipher, length, dst, src);

	ctx->pos = length;
}
