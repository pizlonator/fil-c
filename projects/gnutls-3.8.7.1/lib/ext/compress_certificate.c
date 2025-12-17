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

#include "compress.h"
#include "errors.h"
#include "gnutls_int.h"
#include "hello_ext_lib.h"
#include "num.h"
#include "ext/compress_certificate.h"

/* Converts compression algorithm number established in RFC8879 to internal compression method type
 */
gnutls_compression_method_t
_gnutls_compress_certificate_num2method(uint16_t num)
{
	switch (num) {
	case 1:
		return GNUTLS_COMP_ZLIB;
	case 2:
		return GNUTLS_COMP_BROTLI;
	case 3:
		return GNUTLS_COMP_ZSTD;
	default:
		return GNUTLS_COMP_UNKNOWN;
	}
}

/* Converts compression method type to compression algorithm number established in RFC8879
 */
int _gnutls_compress_certificate_method2num(gnutls_compression_method_t method)
{
	switch (method) {
	case GNUTLS_COMP_ZLIB:
		return 1;
	case GNUTLS_COMP_BROTLI:
		return 2;
	case GNUTLS_COMP_ZSTD:
		return 3;
	default:
		return GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
	}
}

/* Returns 1 if the method is set as supported compression method for the session,
 * returns 0 otherwise
 */
bool _gnutls_compress_certificate_is_method_enabled(
	gnutls_session_t session, gnutls_compression_method_t method)
{
	int ret;
	unsigned i;
	compress_certificate_ext_st *priv;
	gnutls_ext_priv_data_t epriv;

	ret = _gnutls_hello_ext_get_priv(
		session, GNUTLS_EXTENSION_COMPRESS_CERTIFICATE, &epriv);
	if (ret < 0)
		return false;
	priv = epriv;

	for (i = 0; i < priv->methods_len; ++i)
		if (priv->methods[i] == method)
			return true;

	return false;
}

/**
 * gnutls_compress_certificate_get_selected_method:
 * @session: is a #gnutls_session_t type.
 *
 * This function returns the certificate compression method that has been
 * selected to compress the certificate before sending it to the peer.
 * The selection is done based on the local list of supported compression
 * methods and the peer's requested compression methods.
 *
 * Returns: selected certificate compression method.
 *
 * Since 3.7.4
 **/
gnutls_compression_method_t
gnutls_compress_certificate_get_selected_method(gnutls_session_t session)
{
	return session->internals.compress_certificate_method;
}

/**
 * gnutls_compress_certificate_set_methods:
 * @session: is a #gnutls_session_t type.
 * @methods: is a list of supported compression methods.
 * @methods_len: number of compression methods in @methods
 *
 * This function sets the supported compression methods for certificate compression
 * for the given session. The list of supported compression methods will be used
 * for a) requesting the compression of peer's certificate and b) selecting the
 * method to compress the local certificate before sending it to the peer.
 * The order of compression methods inside the list does matter as the method
 * that appears earlier in the list will be preferred before the later ones.
 * Note that even if you set the list of supported compression methods, the
 * compression might not be used if the peer does not support any of your chosen
 * compression methods.
 *
 * The list of supported compression methods must meet the following criteria:
 * Argument @methods must be an array of valid compression methods of type
 * #gnutls_compression_method_t. Argument @methods_len must contain the number of
 * compression methods stored in the @methods array and must be within range <1, 127>.
 * The length constraints are defined by %MIN_COMPRESS_CERTIFICATE_METHODS
 * and %MAX_COMPRESS_CERTIFICATE_METHODS macros located in the header file
 * compress_certificate.h.
 *
 * If either @methods or @methods_len is equal to 0, current list of supported
 * compression methods will be unset.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since 3.7.4
 **/
int gnutls_compress_certificate_set_methods(
	gnutls_session_t session, const gnutls_compression_method_t *methods,
	size_t methods_len)
{
	int ret;
	unsigned i;
	compress_certificate_ext_st *priv;

	if (methods == NULL || methods_len == 0) {
		_gnutls_hello_ext_unset_priv(
			session, GNUTLS_EXTENSION_COMPRESS_CERTIFICATE);
		return 0;
	}

	if (methods_len > MAX_COMPRESS_CERTIFICATE_METHODS)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	for (i = 0; i < methods_len; ++i)
		if ((ret = _gnutls_compression_init_method(methods[i])) < 0)
			return gnutls_assert_val(ret);

	priv = gnutls_malloc(sizeof(*priv));
	if (priv == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	priv->methods_len = methods_len;
	memcpy(priv->methods, methods, methods_len * sizeof(*methods));
	_gnutls_hello_ext_set_priv(session,
				   GNUTLS_EXTENSION_COMPRESS_CERTIFICATE, priv);

	return 0;
}

int _gnutls_compress_certificate_recv_params(gnutls_session_t session,
					     const uint8_t *data,
					     size_t data_size)
{
	int ret;
	unsigned i, j;
	uint16_t num;
	uint8_t bytes_len;
	size_t methods_len;
	gnutls_compression_method_t methods[MAX_COMPRESS_CERTIFICATE_METHODS];
	gnutls_compression_method_t method;
	compress_certificate_ext_st *priv;
	gnutls_ext_priv_data_t epriv;

	ret = _gnutls_hello_ext_get_priv(
		session, GNUTLS_EXTENSION_COMPRESS_CERTIFICATE, &epriv);
	if (ret < 0)
		return 0;
	priv = epriv;

	DECR_LEN(data_size, 1);
	bytes_len = *data;

	if (bytes_len < 2 || bytes_len > 254 || bytes_len % 2 == 1)
		return gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);

	DECR_LEN(data_size, bytes_len);

	methods_len = 0;
	for (i = 0; i < bytes_len / 2; ++i) {
		num = _gnutls_read_uint16(data + i + i + 1);
		method = _gnutls_compress_certificate_num2method(num);
		if (method != GNUTLS_COMP_UNKNOWN)
			methods[methods_len++] = method;
	}

	method = GNUTLS_COMP_UNKNOWN;
	for (i = 0; i < methods_len; ++i)
		for (j = 0; j < priv->methods_len; ++j)
			if (methods[i] == priv->methods[j]) {
				method = methods[i];
				goto endloop;
			}
endloop:
	session->internals.compress_certificate_method = method;

	return 0;
}

int _gnutls_compress_certificate_send_params(gnutls_session_t session,
					     gnutls_buffer_st *data)
{
	int ret, num;
	unsigned i;
	uint8_t bytes_len;
	uint8_t bytes[2 * MAX_COMPRESS_CERTIFICATE_METHODS];
	compress_certificate_ext_st *priv;
	gnutls_ext_priv_data_t epriv;

	ret = _gnutls_hello_ext_get_priv(
		session, GNUTLS_EXTENSION_COMPRESS_CERTIFICATE, &epriv);
	if (ret < 0)
		return 0;
	priv = epriv;

	bytes_len = 2 * priv->methods_len;
	for (i = 0; i < priv->methods_len; ++i) {
		num = _gnutls_compress_certificate_method2num(priv->methods[i]);
		_gnutls_write_uint16(num, bytes + i + i);
	}

	ret = _gnutls_buffer_append_data_prefix(data, 8, bytes, bytes_len);
	if (ret < 0)
		return gnutls_assert_val(ret);

	session->internals.hsk_flags |= HSK_COMP_CRT_REQ_SENT;

	return bytes_len + 1;
}

const hello_ext_entry_st ext_mod_compress_certificate = {
	.name = "Compress Certificate",
	.tls_id = 27,
	.gid = GNUTLS_EXTENSION_COMPRESS_CERTIFICATE,
	.client_parse_point = GNUTLS_EXT_TLS,
	.server_parse_point = GNUTLS_EXT_TLS,
	.validity = GNUTLS_EXT_FLAG_TLS | GNUTLS_EXT_FLAG_DTLS |
		    GNUTLS_EXT_FLAG_CLIENT_HELLO,
	.recv_func = _gnutls_compress_certificate_recv_params,
	.send_func = _gnutls_compress_certificate_send_params,
	.deinit_func = _gnutls_hello_ext_default_deinit
};
