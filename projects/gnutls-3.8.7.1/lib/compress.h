/*
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * Author: Zoltan Fridrich
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

#ifndef GNUTLS_LIB_COMPRESS_H
#define GNUTLS_LIB_COMPRESS_H

#include "gnutls_int.h"

int _gnutls_compression_init_method(gnutls_compression_method_t method);
void _gnutls_compression_deinit(void);
size_t _gnutls_compress_bound(gnutls_compression_method_t alg, size_t src_len);
int _gnutls_compress(gnutls_compression_method_t alg, uint8_t *dst,
		     size_t dst_len, const uint8_t *src, size_t src_len);
int _gnutls_decompress(gnutls_compression_method_t alg, uint8_t *dst,
		       size_t dst_len, const uint8_t *src, size_t src_len);

#endif /* GNUTLS_LIB_COMPRESS_H */
