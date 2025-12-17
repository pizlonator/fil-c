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

#ifndef GNUTLS_LIB_EXT_COMPRESS_CERTIFICATE_H
#define GNUTLS_LIB_EXT_COMPRESS_CERTIFICATE_H

#include "hello_ext.h"

#define MIN_COMPRESS_CERTIFICATE_METHODS 1
#define MAX_COMPRESS_CERTIFICATE_METHODS 127

typedef struct {
	gnutls_compression_method_t methods[MAX_COMPRESS_CERTIFICATE_METHODS];
	size_t methods_len;
} compress_certificate_ext_st;

extern const hello_ext_entry_st ext_mod_compress_certificate;

gnutls_compression_method_t
_gnutls_compress_certificate_num2method(uint16_t num);

int _gnutls_compress_certificate_method2num(gnutls_compression_method_t method);

bool _gnutls_compress_certificate_is_method_enabled(
	gnutls_session_t session, gnutls_compression_method_t method);

int _gnutls_compress_certificate_recv_params(gnutls_session_t session,
					     const uint8_t *data,
					     size_t data_size);

int _gnutls_compress_certificate_send_params(gnutls_session_t session,
					     gnutls_buffer_st *data);

#endif /* GNUTLS_LIB_EXT_COMPRESS_CERTIFICATE_H */
