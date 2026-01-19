/* mceliece6688128f.h - Classic McEliece for libgcrypt
 * Copyright (C) 2023-2024 Simon Josefsson <simon@josefsson.org>
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
 */

#ifndef MCELIECE6688128F_H
#define MCELIECE6688128F_H

#include <string.h>
#include <stdint.h>

#ifdef _GCRYPT_IN_LIBGCRYPT
/**** Start of the glue code to libgcrypt ****/
#include "g10lib.h"             /* for GCC_ATTR_UNUSED */
#include "gcrypt-int.h"

#define mceliece6688128f_keypair _gcry_mceliece6688128f_keypair
#define mceliece6688128f_enc     _gcry_mceliece6688128f_enc
#define mceliece6688128f_dec     _gcry_mceliece6688128f_dec
/**** End of the glue code ****/
#else
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 5 )
#define GCC_ATTR_UNUSED  __attribute__ ((unused))
#else
#define GCC_ATTR_UNUSED
#endif

#define MCELIECE6688128F_SECRETKEY_SIZE 13932
#define MCELIECE6688128F_PUBLICKEY_SIZE 1044992
#define MCELIECE6688128F_CIPHERTEXT_SIZE 208
#define MCELIECE6688128F_SIZE 32
#endif

typedef void mceliece6688128f_random_func (void *ctx,
					   size_t length,
					   uint8_t *dst);

void
mceliece6688128f_keypair (uint8_t *pk, uint8_t *sk);

void
mceliece6688128f_enc (uint8_t *c, uint8_t *k, const uint8_t *pk);

void
mceliece6688128f_dec (uint8_t *k, const uint8_t *c, const uint8_t *sk);

#endif /* MCELIECE6688128F_H */
