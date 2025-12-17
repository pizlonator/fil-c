/*
 * Copyright (C) 2021 Free Software Foundation, Inc.
 *
 * Author: Fratnišek Krenželok
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

#include "config.h"
#include "system/ktls.h"

#ifdef ENABLE_KTLS

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>
#include "record.h"

#if defined(__FreeBSD__)
#include <sys/ktls.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <crypto/cryptodev.h>

#else
#include <linux/tls.h>
#include "ext/session_ticket.h"
#include <sys/sendfile.h>
#include <sys/utsname.h>
#endif

/**
 * gnutls_transport_is_ktls_enabled:
 * @session: is a #gnutls_session_t type.
 *
 * Checks if KTLS is now enabled and was properly inicialized.
 *
 * Returns: %GNUTLS_KTLS_RECV, %GNUTLS_KTLS_SEND, %GNUTLS_KTLS_DUPLEX, otherwise 0
 *
 * Since: 3.7.3
 **/
gnutls_transport_ktls_enable_flags_t
gnutls_transport_is_ktls_enabled(gnutls_session_t session)
{
	if (unlikely(!session->internals.initial_negotiation_completed)) {
		_gnutls_debug_log("Initial negotiation is not yet complete\n");
		return 0;
	}

	return session->internals.ktls_enabled;
}

void _gnutls_ktls_enable(gnutls_session_t session)
{
#if defined(__FreeBSD__)
	session->internals.ktls_enabled |= GNUTLS_KTLS_RECV;
	session->internals.ktls_enabled |= GNUTLS_KTLS_SEND;
#else
	int sockin, sockout;

	gnutls_transport_get_int2(session, &sockin, &sockout);

	if (setsockopt(sockin, SOL_TCP, TCP_ULP, "tls", sizeof("tls")) == 0) {
		session->internals.ktls_enabled |= GNUTLS_KTLS_RECV;
		if (sockin == sockout) {
			session->internals.ktls_enabled |= GNUTLS_KTLS_SEND;
		}
	} else {
		_gnutls_record_log(
			"Unable to set TCP_ULP for read socket: %d\n", errno);
	}

	if (sockin != sockout) {
		if (setsockopt(sockout, SOL_TCP, TCP_ULP, "tls",
			       sizeof("tls")) == 0) {
			session->internals.ktls_enabled |= GNUTLS_KTLS_SEND;
		} else {
			_gnutls_record_log(
				"Unable to set TCP_ULP for write socket: %d\n",
				errno);
		}
	}
#endif
}

#if defined(__FreeBSD__)

int _gnutls_ktls_set_keys(gnutls_session_t session,
			  gnutls_transport_ktls_enable_flags_t in)
{
	gnutls_cipher_algorithm_t cipher = gnutls_cipher_get(session);
	gnutls_datum_t mac_key;
	gnutls_datum_t iv;
	gnutls_datum_t cipher_key;
	unsigned char seq_number[12];
	int sockin, sockout;
	int ret;

	gnutls_transport_get_int2(session, &sockin, &sockout);
	/* check whether or not cipher suite supports ktls
	 */
	int version = gnutls_protocol_get_version(session);
	if ((version != GNUTLS_TLS1_3 && version != GNUTLS_TLS1_2) ||
	    (cipher != GNUTLS_CIPHER_AES_128_GCM &&
	     cipher != GNUTLS_CIPHER_AES_256_GCM &&
	     cipher != GNUTLS_CIPHER_CHACHA20_POLY1305)) {
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;
	}

	ret = gnutls_record_get_state(session, 1, &mac_key, &iv, &cipher_key,
				      seq_number);
	if (ret < 0) {
		return ret;
	}

	in &= session->internals.ktls_enabled;

	if (in & GNUTLS_KTLS_RECV) {
		struct tls_enable crypto_info;
		memset(&crypto_info, 0, sizeof(crypto_info));
		switch (cipher) {
		case GNUTLS_CIPHER_AES_128_GCM: {
			crypto_info.cipher_algorithm = CRYPTO_AES_NIST_GCM_16;
			assert(cipher_key.size == AES_128_GMAC_KEY_LEN);

			if (version == GNUTLS_TLS1_2) {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_TWO;
				crypto_info.iv =
					gnutls_malloc(TLS_AEAD_GCM_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_AEAD_GCM_LEN;
				memcpy(crypto_info.iv, seq_number,
				       TLS_AEAD_GCM_LEN);
			} else {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_THREE;
				assert(iv.size == TLS_1_3_GCM_IV_LEN);

				crypto_info.iv =
					gnutls_malloc(TLS_1_3_GCM_IV_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_1_3_GCM_IV_LEN;
				memcpy(crypto_info.iv, iv.data,
				       TLS_1_3_GCM_IV_LEN);
			}

			memcpy(crypto_info.rec_seq, seq_number, 8);

			crypto_info.cipher_key_len = AES_128_GMAC_KEY_LEN;
			crypto_info.cipher_key =
				gnutls_malloc(AES_128_GMAC_KEY_LEN);
			if (!crypto_info.cipher_key) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			memcpy(crypto_info.cipher_key, cipher_key.data,
			       AES_128_GMAC_KEY_LEN);

			if (setsockopt(sockin, IPPROTO_TCP, TCP_RXTLS_ENABLE,
				       &crypto_info, sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_AES_256_GCM: {
			crypto_info.cipher_algorithm = CRYPTO_AES_NIST_GCM_16;
			assert(cipher_key.size == AES_256_GMAC_KEY_LEN);

			if (version == GNUTLS_TLS1_2) {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_TWO;
				crypto_info.iv =
					gnutls_malloc(TLS_AEAD_GCM_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_AEAD_GCM_LEN;
				memcpy(crypto_info.iv, seq_number,
				       TLS_AEAD_GCM_LEN);
			} else {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_THREE;
				assert(iv.size == TLS_1_3_GCM_IV_LEN);

				crypto_info.iv =
					gnutls_malloc(TLS_1_3_GCM_IV_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_1_3_GCM_IV_LEN;
				memcpy(crypto_info.iv, iv.data,
				       TLS_1_3_GCM_IV_LEN);
			}

			memcpy(crypto_info.rec_seq, seq_number, 8);

			crypto_info.cipher_key_len = AES_256_GMAC_KEY_LEN;
			crypto_info.cipher_key =
				gnutls_malloc(AES_256_GMAC_KEY_LEN);
			if (!crypto_info.cipher_key) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			memcpy(crypto_info.cipher_key, cipher_key.data,
			       AES_256_GMAC_KEY_LEN);

			if (setsockopt(sockin, IPPROTO_TCP, TCP_RXTLS_ENABLE,
				       &crypto_info, sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}

		} break;
		case GNUTLS_CIPHER_CHACHA20_POLY1305: {
			crypto_info.cipher_algorithm = CRYPTO_CHACHA20_POLY1305;
			assert(cipher_key.size == POLY1305_KEY_LEN);

			if (version == GNUTLS_TLS1_2) {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_TWO;
			} else {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_THREE;
			}

			assert(iv.size == CHACHA20_POLY1305_IV_LEN);
			crypto_info.iv =
				gnutls_malloc(CHACHA20_POLY1305_IV_LEN);
			if (!crypto_info.iv) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			crypto_info.iv_len = TLS_CHACHA20_IV_LEN;
			memcpy(crypto_info.iv, iv.data, TLS_CHACHA20_IV_LEN);

			memcpy(crypto_info.rec_seq, seq_number, 8);

			crypto_info.cipher_key_len = POLY1305_KEY_LEN;
			crypto_info.cipher_key =
				gnutls_malloc(POLY1305_KEY_LEN);
			if (!crypto_info.cipher_key) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			memcpy(crypto_info.cipher_key, cipher_key.data,
			       POLY1305_KEY_LEN);

			if (setsockopt(sockin, IPPROTO_TCP, TCP_RXTLS_ENABLE,
				       &crypto_info, sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}

		} break;
		default:
			assert(0);
		}
	}

	ret = gnutls_record_get_state(session, 0, &mac_key, &iv, &cipher_key,
				      seq_number);
	if (ret < 0) {
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	}

	if (in & GNUTLS_KTLS_SEND) {
		struct tls_enable crypto_info;
		memset(&crypto_info, 0, sizeof(crypto_info));
		switch (cipher) {
		case GNUTLS_CIPHER_AES_128_GCM: {
			crypto_info.cipher_algorithm = CRYPTO_AES_NIST_GCM_16;
			assert(cipher_key.size == AES_128_GMAC_KEY_LEN);

			if (version == GNUTLS_TLS1_2) {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_TWO;
				crypto_info.iv =
					gnutls_malloc(TLS_AEAD_GCM_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_AEAD_GCM_LEN;
				memcpy(crypto_info.iv, seq_number,
				       TLS_AEAD_GCM_LEN);
			} else {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_THREE;
				assert(iv.size == TLS_1_3_GCM_IV_LEN);

				crypto_info.iv =
					gnutls_malloc(TLS_1_3_GCM_IV_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_1_3_GCM_IV_LEN;
				memcpy(crypto_info.iv, iv.data,
				       TLS_1_3_GCM_IV_LEN);
			}

			memcpy(crypto_info.rec_seq, seq_number, 8);

			crypto_info.cipher_key_len = AES_128_GMAC_KEY_LEN;
			crypto_info.cipher_key =
				gnutls_malloc(AES_128_GMAC_KEY_LEN);
			if (!crypto_info.cipher_key) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			memcpy(crypto_info.cipher_key, cipher_key.data,
			       AES_128_GMAC_KEY_LEN);

			if (setsockopt(sockin, IPPROTO_TCP, TCP_TXTLS_ENABLE,
				       &crypto_info, sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_AES_256_GCM: {
			crypto_info.cipher_algorithm = CRYPTO_AES_NIST_GCM_16;
			assert(cipher_key.size == AES_256_GMAC_KEY_LEN);

			if (version == GNUTLS_TLS1_2) {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_TWO;
				crypto_info.iv =
					gnutls_malloc(TLS_AEAD_GCM_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_AEAD_GCM_LEN;
				memcpy(crypto_info.iv, seq_number,
				       TLS_AEAD_GCM_LEN);
			} else {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_THREE;
				assert(iv.size == TLS_1_3_GCM_IV_LEN);

				crypto_info.iv =
					gnutls_malloc(TLS_1_3_GCM_IV_LEN);
				if (!crypto_info.iv) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
				crypto_info.iv_len = TLS_1_3_GCM_IV_LEN;
				memcpy(crypto_info.iv, iv.data,
				       TLS_1_3_GCM_IV_LEN);
			}

			memcpy(crypto_info.rec_seq, seq_number, 8);

			crypto_info.cipher_key_len = AES_256_GMAC_KEY_LEN;
			crypto_info.cipher_key =
				gnutls_malloc(AES_256_GMAC_KEY_LEN);
			if (!crypto_info.cipher_key) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			memcpy(crypto_info.cipher_key, cipher_key.data,
			       AES_256_GMAC_KEY_LEN);

			if (setsockopt(sockin, IPPROTO_TCP, TCP_TXTLS_ENABLE,
				       &crypto_info, sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}

		} break;
		case GNUTLS_CIPHER_CHACHA20_POLY1305: {
			crypto_info.cipher_algorithm = CRYPTO_CHACHA20_POLY1305;
			assert(cipher_key.size == POLY1305_KEY_LEN);

			if (version == GNUTLS_TLS1_2) {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_TWO;
			} else {
				crypto_info.tls_vmajor = TLS_MAJOR_VER_ONE;
				crypto_info.tls_vminor = TLS_MINOR_VER_THREE;
			}

			assert(iv.size == CHACHA20_POLY1305_IV_LEN);
			crypto_info.iv =
				gnutls_malloc(CHACHA20_POLY1305_IV_LEN);
			if (!crypto_info.iv) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			crypto_info.iv_len = TLS_CHACHA20_IV_LEN;
			memcpy(crypto_info.iv, iv.data, TLS_CHACHA20_IV_LEN);

			memcpy(crypto_info.rec_seq, seq_number, 8);

			crypto_info.cipher_key_len = POLY1305_KEY_LEN;
			crypto_info.cipher_key =
				gnutls_malloc(POLY1305_KEY_LEN);
			if (!crypto_info.cipher_key) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}
			memcpy(crypto_info.cipher_key, cipher_key.data,
			       POLY1305_KEY_LEN);

			if (setsockopt(sockin, IPPROTO_TCP, TCP_TXTLS_ENABLE,
				       &crypto_info, sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}

		} break;
		default:
			assert(0);
		}

		// set callback for sending handshake messages
		gnutls_handshake_set_read_function(
			session, _gnutls_ktls_send_handshake_msg);

		// set callback for sending alert messages
		gnutls_alert_set_read_function(session,
					       _gnutls_ktls_send_alert_msg);
	}

	return in;
}
#else

int _gnutls_ktls_set_keys(gnutls_session_t session,
			  gnutls_transport_ktls_enable_flags_t in)
{
	gnutls_cipher_algorithm_t cipher = gnutls_cipher_get(session);
	int version = gnutls_protocol_get_version(session);
	struct utsname utsname;
	char *endptr;
	long major, minor;
	gnutls_datum_t mac_key;
	gnutls_datum_t iv;
	gnutls_datum_t cipher_key;
	unsigned char seq_number[12];
	int sockin, sockout;
	int ret;

	ret = uname(&utsname);
	if (unlikely(ret < 0)) {
		return GNUTLS_E_INTERNAL_ERROR;
	}

	if (strcmp(utsname.sysname, "Linux") != 0) {
		return GNUTLS_E_INTERNAL_ERROR;
	}

	major = strtol(utsname.release, &endptr, 10);
	if (major < 0 || major == LONG_MAX || *endptr != '.') {
		return GNUTLS_E_INTERNAL_ERROR;
	}
	minor = strtol(endptr + 1, &endptr, 10);
	if (minor < 0 || minor == LONG_MAX || *endptr != '.') {
		return GNUTLS_E_INTERNAL_ERROR;
	}

	_gnutls_debug_log("Linux kernel version %lu.%lu has been detected\n",
			  major, minor);

	/* setsockopt(SOL_TLS, TLS_RX) support added in 5.10 */
	if (major < 5 || (major == 5 && minor < 10)) {
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;
	}

	/* check whether or not cipher suite supports ktls
	 */
	switch (cipher) {
	case GNUTLS_CIPHER_AES_128_GCM:
	case GNUTLS_CIPHER_AES_256_GCM:
		break;
	case GNUTLS_CIPHER_AES_128_CCM:
	case GNUTLS_CIPHER_CHACHA20_POLY1305:
		/* AES-128-CCM and CHACHA20-POLY1305 can only be set
		 * using setsockopt since 5.11.
		 */
		if (major < 5 || (major == 5 && minor < 11)) {
			return GNUTLS_E_UNIMPLEMENTED_FEATURE;
		}
		break;
	default:
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;
	}

	if (version < GNUTLS_TLS1_2) {
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;
	}

	gnutls_transport_get_int2(session, &sockin, &sockout);

	ret = gnutls_record_get_state(session, 1, &mac_key, &iv, &cipher_key,
				      seq_number);
	if (ret < 0) {
		return ret;
	}

	in &= session->internals.ktls_enabled;

	if (in & GNUTLS_KTLS_RECV) {
		switch (cipher) {
		case GNUTLS_CIPHER_AES_128_GCM: {
			struct tls12_crypto_info_aes_gcm_128 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_128;
			assert(cipher_key.size ==
			       TLS_CIPHER_AES_GCM_128_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_AES_GCM_128_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_AES_GCM_128_SALT_SIZE +
					       TLS_CIPHER_AES_GCM_128_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_AES_GCM_128_SALT_SIZE,
				       TLS_CIPHER_AES_GCM_128_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_AES_GCM_128_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_AES_GCM_128_KEY_SIZE);

			if (setsockopt(sockin, SOL_TLS, TLS_RX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_AES_256_GCM: {
			struct tls12_crypto_info_aes_gcm_256 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_256;
			assert(cipher_key.size ==
			       TLS_CIPHER_AES_GCM_256_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_AES_GCM_256_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_AES_GCM_256_SALT_SIZE +
					       TLS_CIPHER_AES_GCM_256_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_AES_GCM_256_SALT_SIZE,
				       TLS_CIPHER_AES_GCM_256_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_AES_GCM_256_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_AES_GCM_256_KEY_SIZE);

			if (setsockopt(sockin, SOL_TLS, TLS_RX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_AES_128_CCM: {
			struct tls12_crypto_info_aes_ccm_128 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type = TLS_CIPHER_AES_CCM_128;
			assert(cipher_key.size ==
			       TLS_CIPHER_AES_CCM_128_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_AES_CCM_128_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_AES_CCM_128_SALT_SIZE +
					       TLS_CIPHER_AES_CCM_128_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_AES_CCM_128_SALT_SIZE,
				       TLS_CIPHER_AES_CCM_128_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_AES_CCM_128_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_AES_CCM_128_KEY_SIZE);

			if (setsockopt(sockin, SOL_TLS, TLS_RX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_CHACHA20_POLY1305: {
			struct tls12_crypto_info_chacha20_poly1305 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type =
				TLS_CIPHER_CHACHA20_POLY1305;
			assert(cipher_key.size ==
			       TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE +
					       TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE,
				       TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE);

			if (setsockopt(sockin, SOL_TLS, TLS_RX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_RECV;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		default:
			assert(0);
		}
	}

	ret = gnutls_record_get_state(session, 0, &mac_key, &iv, &cipher_key,
				      seq_number);
	if (ret < 0) {
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	}

	if (in & GNUTLS_KTLS_SEND) {
		switch (cipher) {
		case GNUTLS_CIPHER_AES_128_GCM: {
			struct tls12_crypto_info_aes_gcm_128 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_128;

			assert(cipher_key.size ==
			       TLS_CIPHER_AES_GCM_128_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_AES_GCM_128_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_AES_GCM_128_SALT_SIZE +
					       TLS_CIPHER_AES_GCM_128_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_AES_GCM_128_SALT_SIZE,
				       TLS_CIPHER_AES_GCM_128_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_AES_GCM_128_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_AES_GCM_128_KEY_SIZE);

			if (setsockopt(sockout, SOL_TLS, TLS_TX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_AES_256_GCM: {
			struct tls12_crypto_info_aes_gcm_256 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_256;
			assert(cipher_key.size ==
			       TLS_CIPHER_AES_GCM_256_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_AES_GCM_256_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_AES_GCM_256_SALT_SIZE +
					       TLS_CIPHER_AES_GCM_256_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_AES_GCM_256_SALT_SIZE,
				       TLS_CIPHER_AES_GCM_256_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_AES_GCM_256_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_AES_GCM_256_KEY_SIZE);

			if (setsockopt(sockout, SOL_TLS, TLS_TX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_AES_128_CCM: {
			struct tls12_crypto_info_aes_ccm_128 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type = TLS_CIPHER_AES_CCM_128;
			assert(cipher_key.size ==
			       TLS_CIPHER_AES_CCM_128_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_AES_CCM_128_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_AES_CCM_128_SALT_SIZE +
					       TLS_CIPHER_AES_CCM_128_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_AES_CCM_128_SALT_SIZE,
				       TLS_CIPHER_AES_CCM_128_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_AES_CCM_128_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_AES_CCM_128_KEY_SIZE);

			if (setsockopt(sockout, SOL_TLS, TLS_TX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		case GNUTLS_CIPHER_CHACHA20_POLY1305: {
			struct tls12_crypto_info_chacha20_poly1305 crypto_info;
			memset(&crypto_info, 0, sizeof(crypto_info));

			crypto_info.info.cipher_type =
				TLS_CIPHER_CHACHA20_POLY1305;
			assert(cipher_key.size ==
			       TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE);

			/* for TLS 1.2 IV is generated in kernel */
			if (version == GNUTLS_TLS1_2) {
				crypto_info.info.version = TLS_1_2_VERSION;
				memcpy(crypto_info.iv, seq_number,
				       TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);
			} else {
				crypto_info.info.version = TLS_1_3_VERSION;
				assert(iv.size ==
				       TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE +
					       TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);

				memcpy(crypto_info.iv,
				       iv.data +
					       TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE,
				       TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);
			}

			memcpy(crypto_info.salt, iv.data,
			       TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE);
			memcpy(crypto_info.rec_seq, seq_number,
			       TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE);
			memcpy(crypto_info.key, cipher_key.data,
			       TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE);

			if (setsockopt(sockout, SOL_TLS, TLS_TX, &crypto_info,
				       sizeof(crypto_info))) {
				session->internals.ktls_enabled &=
					~GNUTLS_KTLS_SEND;
				return gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
			}
		} break;
		default:
			assert(0);
		}

		// set callback for sending handshake messages
		gnutls_handshake_set_read_function(
			session, _gnutls_ktls_send_handshake_msg);

		// set callback for sending alert messages
		gnutls_alert_set_read_function(session,
					       _gnutls_ktls_send_alert_msg);
	}

	return in;
}
#endif

ssize_t _gnutls_ktls_send_file(gnutls_session_t session, int fd, off_t *offset,
			       size_t count)
{
	ssize_t ret;
	int sockin, sockout;

	assert(session != NULL);
#if defined(__FreeBSD__)
	off_t sbytes = 0;
	assert(offset != NULL);
#endif

	gnutls_transport_get_int2(session, &sockin, &sockout);
#if defined(__FreeBSD__)
	ret = sendfile(fd, sockout, *offset, count, NULL, &sbytes, 0);
#else
	ret = sendfile(sockout, fd, offset, count);
#endif
	if (ret == -1) {
		switch (errno) {
		case EINTR:
			return GNUTLS_E_INTERRUPTED;
		case EAGAIN:
			return GNUTLS_E_AGAIN;
		default:
			return GNUTLS_E_PUSH_ERROR;
		}
	}
#if defined(__FreeBSD__)
	*offset += sbytes; /* follow linux sendfile behavior */
	return sbytes;
#else
	return ret;
#endif
}

int _gnutls_ktls_send_control_msg(gnutls_session_t session,
				  unsigned char record_type, const void *data,
				  size_t data_size)
{
	const char *buf = data;
	ssize_t ret;
	int sockin, sockout;
	size_t data_to_send = data_size;

	assert(session != NULL);

	gnutls_transport_get_int2(session, &sockin, &sockout);

	while (data_to_send > 0) {
		char cmsg[CMSG_SPACE(sizeof(unsigned char))];
		struct msghdr msg = { 0 };
		struct iovec msg_iov; /* Vector of data to send/receive into. */
		struct cmsghdr *hdr;

		msg.msg_control = cmsg;
		msg.msg_controllen = sizeof cmsg;

		hdr = CMSG_FIRSTHDR(&msg);
#if defined(__FreeBSD__)
		hdr->cmsg_level = IPPROTO_TCP;
#else
		hdr->cmsg_level = SOL_TLS;
#endif
		hdr->cmsg_type = TLS_SET_RECORD_TYPE;
		hdr->cmsg_len = CMSG_LEN(sizeof(unsigned char));

		// construct record header
		*CMSG_DATA(hdr) = record_type;
		msg.msg_controllen = hdr->cmsg_len;

		msg_iov.iov_base = (void *)buf;
		msg_iov.iov_len = data_to_send;

		msg.msg_iov = &msg_iov;
		msg.msg_iovlen = 1;

		ret = sendmsg(sockout, &msg, MSG_DONTWAIT);

		if (ret == -1) {
			switch (errno) {
			case EINTR:
				if (data_to_send < data_size) {
					return data_size - data_to_send;
				} else {
					return GNUTLS_E_INTERRUPTED;
				}
			case EAGAIN:
				if (data_to_send < data_size) {
					return data_size - data_to_send;
				} else {
					return GNUTLS_E_AGAIN;
				}
			default:
				return GNUTLS_E_PUSH_ERROR;
			}
		}

		buf += ret;
		data_to_send -= ret;
	}

	return data_size;
}

int _gnutls_ktls_send_handshake_msg(gnutls_session_t session,
				    gnutls_record_encryption_level_t level,
				    gnutls_handshake_description_t htype,
				    const void *data, size_t data_size)
{
	return _gnutls_ktls_send_control_msg(session, GNUTLS_HANDSHAKE, data,
					     data_size);
}

int _gnutls_ktls_send_alert_msg(gnutls_session_t session,
				gnutls_record_encryption_level_t level,
				gnutls_alert_level_t alert_level,
				gnutls_alert_description_t alert_desc)
{
	uint8_t data[2];
	data[0] = (uint8_t)alert_level;
	data[1] = (uint8_t)alert_desc;
	return _gnutls_ktls_send_control_msg(session, GNUTLS_ALERT, data, 2);
}

int _gnutls_ktls_recv_control_msg(gnutls_session_t session,
				  unsigned char *record_type, void *data,
				  size_t data_size)
{
	char *buf = data;
	ssize_t ret;
	int sockin, sockout;

	char cmsg[CMSG_SPACE(sizeof(unsigned char))];
	struct msghdr msg = { 0 };
	struct iovec msg_iov;
	struct cmsghdr *hdr;

	assert(session != NULL);

	gnutls_transport_get_int2(session, &sockin, &sockout);

	if (session->internals.read_eof != 0) {
		return 0;
	} else if (session->internals.invalid_connection != 0 ||
		   session->internals.may_not_read != 0)
		return GNUTLS_E_INVALID_SESSION;

	/* receive message */
	msg.msg_control = cmsg;
	msg.msg_controllen = sizeof cmsg;

	msg_iov.iov_base = buf;
	msg_iov.iov_len = data_size;

	msg.msg_iov = &msg_iov;
	msg.msg_iovlen = 1;

	ret = recvmsg(sockin, &msg, MSG_DONTWAIT);

	if (ret == -1) {
		switch (errno) {
		case EAGAIN:
			return GNUTLS_E_AGAIN;
		case EINVAL:
			return GNUTLS_E_UNSUPPORTED_VERSION_PACKET;
		case EMSGSIZE:
			return GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
		case EBADMSG:
			return GNUTLS_E_DECRYPTION_FAILED;
		default:
			return GNUTLS_E_PULL_ERROR;
		}
	}

	/* connection closed */
	if (ret == 0)
		return 0;

	/* get record type from header */
	hdr = CMSG_FIRSTHDR(&msg);
	if (hdr == NULL) {
		return GNUTLS_E_PULL_ERROR;
	}
#if defined(__FreeBSD__)
	if (hdr->cmsg_level == IPPROTO_TCP && hdr->cmsg_type == TLS_GET_RECORD)
#else
	if (hdr->cmsg_level == SOL_TLS && hdr->cmsg_type == TLS_GET_RECORD_TYPE)
#endif
		*record_type = *(unsigned char *)CMSG_DATA(hdr);
	else
		*record_type = GNUTLS_APPLICATION_DATA;

	return ret;
}

int _gnutls_ktls_recv_int(gnutls_session_t session, content_type_t type,
			  void *data, size_t data_size)
{
	unsigned char record_type;
	int ret;

	ret = _gnutls_ktls_recv_control_msg(session, &record_type, data,
					    data_size);

	if (ret > 0) {
		switch (record_type) {
		case GNUTLS_CHANGE_CIPHER_SPEC:
			return gnutls_assert_val(
				GNUTLS_E_UNIMPLEMENTED_FEATURE);
			break;
		case GNUTLS_ALERT:
			session_invalidate(session);
			ret = 0;
			break;
		case GNUTLS_HANDSHAKE:
			ret = gnutls_handshake_write(
				session, GNUTLS_ENCRYPTION_LEVEL_APPLICATION,
				data, ret);

			if (ret < 0)
				return gnutls_assert_val(ret);

			if (type != record_type)
				return GNUTLS_E_AGAIN;
			break;
		case GNUTLS_APPLICATION_DATA:
			if (type != record_type)
				ret = GNUTLS_E_GOT_APPLICATION_DATA;
			break;
		case GNUTLS_HEARTBEAT:
			break;
		default:
			gnutls_assert();
			return GNUTLS_E_UNEXPECTED_PACKET;
		}
	}
	return ret;
}
#else //ENABLE_KTLS
gnutls_transport_ktls_enable_flags_t
gnutls_transport_is_ktls_enabled(gnutls_session_t session)
{
	return 0;
}

void _gnutls_ktls_enable(gnutls_session_t session)
{
}

int _gnutls_ktls_set_keys(gnutls_session_t sessioni,
			  gnutls_transport_ktls_enable_flags_t in)
{
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

ssize_t _gnutls_ktls_send_file(gnutls_session_t session, int fd, off_t *offset,
			       size_t count)
{
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

int _gnutls_ktls_send_control_msg(gnutls_session_t session,
				  unsigned char record_type, const void *data,
				  size_t data_size)
{
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

int _gnutls_ktls_send_handshake_msg(gnutls_session_t session,
				    gnutls_record_encryption_level_t level,
				    gnutls_handshake_description_t htype,
				    const void *data, size_t data_size)
{
	(void)level;
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

int _gnutls_ktls_recv_int(gnutls_session_t session, content_type_t type,
			  void *data, size_t data_size)
{
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

#endif //ENABLE_KTLS
