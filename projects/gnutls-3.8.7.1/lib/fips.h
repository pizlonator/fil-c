/*
 * Copyright (C) 2013 Red Hat
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

#ifndef GNUTLS_LIB_FIPS_H
#define GNUTLS_LIB_FIPS_H

#include "gnutls_int.h"
#include <gnutls/gnutls.h>

#define FIPS140_RND_KEY_SIZE 32

typedef enum {
	LIB_STATE_POWERON,
	LIB_STATE_INIT,
	LIB_STATE_SELFTEST,
	LIB_STATE_OPERATIONAL,
	LIB_STATE_ERROR,
	LIB_STATE_SHUTDOWN
} gnutls_lib_state_t;

/* do not access directly */
extern unsigned int _gnutls_lib_state;
extern gnutls_crypto_rnd_st _gnutls_fips_rnd_ops;

void _gnutls_switch_fips_state(gnutls_fips140_operation_state_t state);

inline static void _gnutls_switch_lib_state(gnutls_lib_state_t state)
{
	/* Once into zombie state no errors can change us */
	_gnutls_lib_state = state;
}

inline static gnutls_lib_state_t _gnutls_get_lib_state(void)
{
	return _gnutls_lib_state;
}

int _gnutls_fips_perform_self_checks1(void);
int _gnutls_fips_perform_self_checks2(void);
void _gnutls_fips_mode_reset_zombie(void);

#ifdef ENABLE_FIPS140
unsigned _gnutls_fips_mode_enabled(void);
#else
#define _gnutls_fips_mode_enabled() 0
#endif

#define HAVE_LIB_ERROR()                                             \
	unlikely(_gnutls_get_lib_state() != LIB_STATE_OPERATIONAL && \
		 _gnutls_get_lib_state() != LIB_STATE_SELFTEST)

#define FAIL_IF_LIB_ERROR     \
	if (HAVE_LIB_ERROR()) \
	return GNUTLS_E_LIB_IN_ERROR_STATE

void _gnutls_switch_lib_state(gnutls_lib_state_t state);

void _gnutls_lib_simulate_error(void);
void _gnutls_lib_force_operational(void);

inline static bool
is_mac_algo_hmac_approved_in_fips(gnutls_mac_algorithm_t algo)
{
	switch (algo) {
	case GNUTLS_MAC_SHA1:
	case GNUTLS_MAC_SHA256:
	case GNUTLS_MAC_SHA384:
	case GNUTLS_MAC_SHA512:
	case GNUTLS_MAC_SHA224:
	case GNUTLS_MAC_SHA3_224:
	case GNUTLS_MAC_SHA3_256:
	case GNUTLS_MAC_SHA3_384:
	case GNUTLS_MAC_SHA3_512:
		return true;
	default:
		return false;
	}
}

inline static bool is_mac_algo_approved_in_fips(gnutls_mac_algorithm_t algo)
{
	if (is_mac_algo_hmac_approved_in_fips(algo)) {
		return true;
	}

	switch (algo) {
	case GNUTLS_MAC_AES_CMAC_128:
	case GNUTLS_MAC_AES_CMAC_256:
	case GNUTLS_MAC_AES_GMAC_128:
	case GNUTLS_MAC_AES_GMAC_192:
	case GNUTLS_MAC_AES_GMAC_256:
	/* They are not a MAC algorithm, but go through the same check */
	case GNUTLS_MAC_SHAKE_128:
	case GNUTLS_MAC_SHAKE_256:
		return true;
	default:
		return false;
	}
}

inline static bool is_mac_algo_allowed_in_fips(gnutls_mac_algorithm_t algo)
{
	return is_mac_algo_approved_in_fips(algo);
}

inline static bool
is_cipher_algo_approved_in_fips(gnutls_cipher_algorithm_t algo)
{
	switch (algo) {
	case GNUTLS_CIPHER_AES_128_CBC:
	case GNUTLS_CIPHER_AES_256_CBC:
	case GNUTLS_CIPHER_AES_192_CBC:
	case GNUTLS_CIPHER_AES_128_CCM:
	case GNUTLS_CIPHER_AES_256_CCM:
	case GNUTLS_CIPHER_AES_128_CCM_8:
	case GNUTLS_CIPHER_AES_256_CCM_8:
	case GNUTLS_CIPHER_AES_128_CFB8:
	case GNUTLS_CIPHER_AES_192_CFB8:
	case GNUTLS_CIPHER_AES_256_CFB8:
	case GNUTLS_CIPHER_AES_128_XTS:
	case GNUTLS_CIPHER_AES_256_XTS:
		return true;
	default:
		return false;
	}
}

inline static bool
is_cipher_algo_allowed_in_fips(gnutls_cipher_algorithm_t algo)
{
	if (is_cipher_algo_approved_in_fips(algo)) {
		return true;
	}

	/* GCM is only approved in TLS */
	switch (algo) {
	case GNUTLS_CIPHER_AES_128_GCM:
	case GNUTLS_CIPHER_AES_192_GCM:
	case GNUTLS_CIPHER_AES_256_GCM:
		return true;
	default:
		return false;
	}
}

#ifdef ENABLE_FIPS140
/* This will test the condition when in FIPS140-2 mode
 * and return an error if necessary or ignore */
#define FIPS_RULE(condition, ret_error, ...)                                            \
	{                                                                               \
		gnutls_fips_mode_t _mode = _gnutls_fips_mode_enabled();                 \
		if (_mode != GNUTLS_FIPS140_DISABLED) {                                 \
			if (condition) {                                                \
				if (_mode == GNUTLS_FIPS140_LOG) {                      \
					_gnutls_audit_log(                              \
						NULL,                                   \
						"fips140-2: allowing " __VA_ARGS__);    \
				} else if (_mode != GNUTLS_FIPS140_LAX) {               \
					_gnutls_debug_log(                              \
						"fips140-2: disallowing " __VA_ARGS__); \
					return ret_error;                               \
				}                                                       \
			}                                                               \
		}                                                                       \
	}

inline static bool is_mac_algo_allowed(gnutls_mac_algorithm_t algo)
{
	gnutls_fips_mode_t mode = _gnutls_fips_mode_enabled();
	if (_gnutls_get_lib_state() != LIB_STATE_SELFTEST &&
	    !is_mac_algo_allowed_in_fips(algo)) {
		switch (mode) {
		case GNUTLS_FIPS140_LOG:
			_gnutls_audit_log(NULL,
					  "fips140-2: allowing access to %s\n",
					  gnutls_mac_get_name(algo));
			FALLTHROUGH;
		case GNUTLS_FIPS140_DISABLED:
		case GNUTLS_FIPS140_LAX:
			return true;
		default:
			return false;
		}
	}

	return true;
}

inline static bool is_cipher_algo_allowed(gnutls_cipher_algorithm_t algo)
{
	gnutls_fips_mode_t mode = _gnutls_fips_mode_enabled();
	if (_gnutls_get_lib_state() != LIB_STATE_SELFTEST &&
	    !is_cipher_algo_allowed_in_fips(algo)) {
		switch (mode) {
		case GNUTLS_FIPS140_LOG:
			_gnutls_audit_log(NULL,
					  "fips140-2: allowing access to %s\n",
					  gnutls_cipher_get_name(algo));
			FALLTHROUGH;
		case GNUTLS_FIPS140_DISABLED:
		case GNUTLS_FIPS140_LAX:
			return true;
		default:
			return false;
		}
	}

	return true;
}
#else
#define is_mac_algo_allowed(x) true
#define is_cipher_algo_allowed(x) true
#define FIPS_RULE(condition, ret_error, ...)
#endif

#endif /* GNUTLS_LIB_FIPS_H */
