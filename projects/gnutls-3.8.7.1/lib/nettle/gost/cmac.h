/* cmac.h

   CMAC mode, as specified in RFC4493

   Copyright (C) 2017 Red Hat, Inc.

   Contributed by Nikos Mavrogiannopoulos

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

#ifndef GOST_CMAC_H_INCLUDED
#define GOST_CMAC_H_INCLUDED

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_NETTLE_CMAC_MAGMA_UPDATE
#include "magma.h"

#ifdef __cplusplus
extern "C" {
#endif

#define cmac_magma_set_key _gnutls_cmac_magma_set_key
#define cmac_magma_update _gnutls_cmac_magma_update
#define cmac_magma_digest _gnutls_cmac_magma_digest

struct cmac_magma_ctx CMAC64_CTX(struct magma_ctx);

void cmac_magma_set_key(struct cmac_magma_ctx *ctx, const uint8_t *key);

void cmac_magma_update(struct cmac_magma_ctx *ctx, size_t length,
		       const uint8_t *data);

void cmac_magma_digest(struct cmac_magma_ctx *ctx, size_t length,
		       uint8_t *digest);

#ifdef __cplusplus
}
#endif
#endif /* HAVE_NETTLE_CMAC_MAGMA_UPDATE */
#ifndef HAVE_NETTLE_CMAC_KUZNYECHIK_UPDATE
#include "kuznyechik.h"
#ifdef __cplusplus
extern "C" {
#endif

#define cmac_kuznyechik_set_key _gnutls_cmac_kuznyechik_set_key
#define cmac_kuznyechik_update _gnutls_cmac_kuznyechik_update
#define cmac_kuznyechik_digest _gnutls_cmac_kuznyechik_digest

struct cmac_kuznyechik_ctx CMAC128_CTX(struct kuznyechik_ctx);

void cmac_kuznyechik_set_key(struct cmac_kuznyechik_ctx *ctx,
			     const uint8_t *key);

void cmac_kuznyechik_update(struct cmac_kuznyechik_ctx *ctx, size_t length,
			    const uint8_t *data);

void cmac_kuznyechik_digest(struct cmac_kuznyechik_ctx *ctx, size_t length,
			    uint8_t *digest);

#ifdef __cplusplus
}
#endif
#endif
#endif /* CMAC_H_INCLUDED */
