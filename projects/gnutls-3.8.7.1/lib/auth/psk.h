/*
 * Copyright (C) 2005-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
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

#ifndef GNUTLS_LIB_AUTH_PSK_H
#define GNUTLS_LIB_AUTH_PSK_H

#include "auth.h"
#include "auth/dh_common.h"

#define _gnutls_copy_psk_username(info, datum)                            \
	_gnutls_copy_psk_string(&(info)->username, &(info)->username_len, \
				(datum))

#define _gnutls_copy_psk_hint(info, datum) \
	_gnutls_copy_psk_string(&(info)->hint, &(info)->hint_len, (datum))

typedef struct gnutls_psk_client_credentials_st {
	gnutls_datum_t username;
	gnutls_datum_t key;
	gnutls_psk_client_credentials_function3 *get_function;
	gnutls_psk_client_credentials_function2 *get_function2;
	gnutls_psk_client_credentials_function *get_function1;
	/* TLS 1.3 - The HMAC algorithm to use to compute the binder values */
	const mac_entry_st *binder_algo;
} psk_client_credentials_st;

typedef struct gnutls_psk_server_credentials_st {
	char *password_file;
	/* callback functions, instead of reading the password files.
	 */
	gnutls_psk_server_credentials_function3 *pwd_callback;
	gnutls_psk_server_credentials_function2 *pwd_callback2;
	gnutls_psk_server_credentials_function *pwd_callback1;

	/* For DHE_PSK */
	gnutls_dh_params_t dh_params;
	unsigned int deinit_dh_params;
	gnutls_sec_param_t dh_sec_param;
	/* this callback is used to retrieve the DH or RSA
	 * parameters.
	 */
	gnutls_params_function *params_func;

	/* Identity hint. */
	char *hint;
	/* TLS 1.3 - HMAC algorithm for the binder values */
	const mac_entry_st *binder_algo;
} psk_server_cred_st;

typedef struct psk_auth_info_st {
	char *username;
	uint16_t username_len;
	dh_info_st dh;
	char *hint;
	uint16_t hint_len;
} *psk_auth_info_t;

typedef struct psk_auth_info_st psk_auth_info_st;

inline static int _gnutls_copy_psk_string(char **dest, uint16_t *dest_len,
					  const gnutls_datum_t str)
{
	char *_tmp;

	assert(MAX_USERNAME_SIZE >= str.size);

	_tmp = gnutls_malloc(str.size + 1);
	if (_tmp == NULL)
		return GNUTLS_E_MEMORY_ERROR;
	memcpy(_tmp, str.data, str.size);
	_tmp[str.size] = '\0';

	gnutls_free(*dest);
	*dest = _tmp;
	*dest_len = str.size;

	return GNUTLS_E_SUCCESS;
}

#ifdef ENABLE_PSK

int _gnutls_set_psk_session_key(gnutls_session_t session, gnutls_datum_t *key,
				gnutls_datum_t *psk2);
int _gnutls_gen_psk_server_kx(gnutls_session_t session, gnutls_buffer_st *data);
int _gnutls_gen_psk_client_kx(gnutls_session_t, gnutls_buffer_st *);

#else
#define _gnutls_set_psk_session_key(x, y, z) GNUTLS_E_UNIMPLEMENTED_FEATURE
#endif /* ENABLE_PSK */

#endif /* GNUTLS_LIB_AUTH_PSK_H */
