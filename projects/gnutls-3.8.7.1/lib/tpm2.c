/*
 * Copyright © 2018-2021 David Woodhouse.
 * Copyright © 2019,2021 Red Hat, Inc.
 *
 * Author: David Woodhouse <dwmw2@infradead.org>, Nikos Mavrogiannopoulos,
 *	Daiki Ueno
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

#include "gnutls_int.h"
#include "global.h"
#include "tpm2.h"
#include "pin.h"
#include "abstract_int.h"

#include <string.h>
#include <libtasn1.h>

static const char OID_loadable_key[] = "2.23.133.10.1.3";

static int rsa_key_info(gnutls_privkey_t key, unsigned int flags, void *_info)
{
	if (flags & GNUTLS_PRIVKEY_INFO_PK_ALGO) {
		return GNUTLS_PK_RSA;
	}

	if (flags & GNUTLS_PRIVKEY_INFO_PK_ALGO_BITS) {
		struct tpm2_info_st *info = _info;

		return tpm2_rsa_key_bits(info);
	}

	if (flags & GNUTLS_PRIVKEY_INFO_HAVE_SIGN_ALGO) {
		gnutls_sign_algorithm_t algo = GNUTLS_FLAGS_TO_SIGN_ALGO(flags);
		switch (algo) {
		case GNUTLS_SIGN_RSA_RAW:
		case GNUTLS_SIGN_RSA_SHA1:
		case GNUTLS_SIGN_RSA_SHA256:
		case GNUTLS_SIGN_RSA_SHA384:
		case GNUTLS_SIGN_RSA_SHA512:
			return 1;

		case GNUTLS_SIGN_RSA_PSS_SHA256:
		case GNUTLS_SIGN_RSA_PSS_RSAE_SHA256:
		case GNUTLS_SIGN_RSA_PSS_SHA384:
		case GNUTLS_SIGN_RSA_PSS_RSAE_SHA384:
		case GNUTLS_SIGN_RSA_PSS_SHA512:
		case GNUTLS_SIGN_RSA_PSS_RSAE_SHA512:
			return 1;

		default:
			_gnutls_debug_log(
				"tpm2: unsupported RSA sign algo %s\n",
				gnutls_sign_get_name(algo));
			return 0;
		}
	}

	if (flags & GNUTLS_PRIVKEY_INFO_SIGN_ALGO) {
		return GNUTLS_SIGN_RSA_RAW;
	}

	return -1;
}

static int ec_key_info(gnutls_privkey_t key, unsigned int flags, void *_info)
{
	if (flags & GNUTLS_PRIVKEY_INFO_PK_ALGO) {
		return GNUTLS_PK_EC;
	}

	if (flags & GNUTLS_PRIVKEY_INFO_HAVE_SIGN_ALGO) {
		gnutls_sign_algorithm_t algo = GNUTLS_FLAGS_TO_SIGN_ALGO(flags);
		struct tpm2_info_st *info = _info;
		uint16_t tpm2_curve = tpm2_key_curve(info);

		switch (algo) {
		case GNUTLS_SIGN_ECDSA_SHA1:
		case GNUTLS_SIGN_ECDSA_SHA256:
			return 1;

		case GNUTLS_SIGN_ECDSA_SECP256R1_SHA256:
			return tpm2_curve == 0x0003; /* TPM2_ECC_NIST_P256 */

		case GNUTLS_SIGN_ECDSA_SECP384R1_SHA384:
			return tpm2_curve == 0x0004; /* TPM2_ECC_NIST_P384 */

		case GNUTLS_SIGN_ECDSA_SECP521R1_SHA512:
			return tpm2_curve == 0x0005; /* TPM2_ECC_NIST_P521 */

		default:
			_gnutls_debug_log("tpm2: unsupported EC sign algo %s\n",
					  gnutls_sign_get_name(algo));
			return 0;
		}
	}

	if (flags & GNUTLS_PRIVKEY_INFO_SIGN_ALGO) {
		return GNUTLS_SIGN_ECDSA_SHA256;
	}

	return -1;
}

static int decode_data(asn1_node n, gnutls_datum_t *r)
{
	asn1_data_node_st d;
	int lenlen;
	int result;

	if (!n) {
		return GNUTLS_E_INVALID_REQUEST;
	}

	result = asn1_read_node_value(n, &d);
	if (result != ASN1_SUCCESS) {
		return _gnutls_asn2err(result);
	}

	result = asn1_get_length_der(d.value, d.value_len, &lenlen);
	if (result < 0) {
		return _gnutls_asn2err(result);
	}

	r->data = (unsigned char *)d.value + lenlen;
	r->size = d.value_len - lenlen;

	return 0;
}

int _gnutls_load_tpm2_key(gnutls_privkey_t pkey, const gnutls_datum_t *fdata)
{
	gnutls_datum_t asn1, pubdata, privdata;
	asn1_node tpmkey = NULL;
	char value_buf[16];
	int value_buflen;
	bool emptyauth = false;
	unsigned int parent;
	int err, ret;
	struct tpm2_info_st *info = NULL;

	ret = gnutls_pem_base64_decode2("TSS2 PRIVATE KEY", fdata, &asn1);
	if (ret < 0) {
		/* Report the first error */
		_gnutls_debug_log("tpm2: error decoding TSS2 key blob: %s\n",
				  gnutls_strerror(ret));
		return ret;
	}

	err = asn1_create_element(_gnutls_get_gnutls_asn(), "GNUTLS.TPMKey",
				  &tpmkey);
	if (err != ASN1_SUCCESS) {
		_gnutls_debug_log("tpm2: failed to create ASN.1 type: %s\n",
				  asn1_strerror(err));
		ret = _gnutls_asn2err(err);
		goto out_asn1;
	}

	err = asn1_der_decoding(&tpmkey, asn1.data, asn1.size, NULL);
	if (err != ASN1_SUCCESS) {
		_gnutls_debug_log("tpm2: failed to decode key from ASN.1: %s\n",
				  asn1_strerror(err));
		ret = _gnutls_asn2err(err);
		goto out_tpmkey;
	}

	value_buflen = sizeof(value_buf);
	err = asn1_read_value(tpmkey, "type", value_buf, &value_buflen);
	if (err != ASN1_SUCCESS) {
		_gnutls_debug_log("tpm2: failed to parse key type OID: %s\n",
				  asn1_strerror(err));
		ret = _gnutls_asn2err(err);
		goto out_tpmkey;
	}
	if (strncmp(value_buf, OID_loadable_key, value_buflen)) {
		_gnutls_debug_log("tpm2: key has unknown type OID %s not %s\n",
				  value_buf, OID_loadable_key);
		ret = GNUTLS_E_TPM_ERROR;
		goto out_tpmkey;
	}

	value_buflen = sizeof(value_buf);
	if (!asn1_read_value(tpmkey, "emptyAuth", value_buf, &value_buflen) &&
	    !strcmp(value_buf, "TRUE")) {
		emptyauth = 1;
	}

	memset(value_buf, 0, 5);
	value_buflen = 5;
	err = asn1_read_value(tpmkey, "parent", value_buf, &value_buflen);
	if (err == ASN1_ELEMENT_NOT_FOUND) {
		parent = 0x40000001; /* RH_OWNER */
	} else if (err != ASN1_SUCCESS) {
		_gnutls_debug_log("tpm2: failed to parse TPM2 key parent: %s\n",
				  asn1_strerror(err));
		ret = GNUTLS_E_TPM_ERROR;
		goto out_tpmkey;
	} else {
		int i = 0;
		parent = 0;

		if (value_buflen == 5) {
			if (value_buf[0]) {
				gnutls_assert();
				_gnutls_debug_log(
					"tpm2: failed to parse parent key\n");
				ret = GNUTLS_E_TPM_ERROR;
				goto out_tpmkey;
			}
			/* Skip the leading zero */
			i++;
		}
		for (; i < value_buflen; i++) {
			parent <<= 8;
			parent |= value_buf[i];
		}
	}

	ret = decode_data(asn1_find_node(tpmkey, "pubkey"), &pubdata);
	if (ret < 0) {
		_gnutls_debug_log("tpm2: failed to parse pubkey element: %s\n",
				  gnutls_strerror(ret));
		ret = GNUTLS_E_TPM_ERROR;
		goto out_tpmkey;
	}
	ret = decode_data(asn1_find_node(tpmkey, "privkey"), &privdata);
	if (ret < 0) {
		_gnutls_debug_log("tpm2: failed to parse privkey element: %s\n",
				  gnutls_strerror(ret));
		ret = GNUTLS_E_TPM_ERROR;
		goto out_tpmkey;
	}

	_gnutls_debug_log("tpm2: parsed key with parent %x, emptyauth %d\n",
			  parent, emptyauth);

	info = tpm2_info_init(&pkey->pin);
	if (info == NULL) {
		_gnutls_debug_log("tpm2: failed to allocate context\n");
		ret = GNUTLS_E_MEMORY_ERROR;
		goto out_tpmkey;
	}

	/* Now we've extracted what we need from the ASN.1, invoke the
	 * actual TPM2 code (whichever implementation we end up with */
	ret = install_tpm2_key(info, pkey, parent, emptyauth, &privdata,
			       &pubdata);
	if (ret < 0) {
		goto out_tpmkey;
	}

	switch (ret) {
	case GNUTLS_PK_RSA:
		gnutls_privkey_import_ext4(pkey, info, NULL,
					   tpm2_rsa_sign_hash_fn, NULL,
					   tpm2_deinit_fn, rsa_key_info, 0);
		pkey->key.ext.pk_params_func = tpm2_convert_public;
		break;

	case GNUTLS_PK_ECDSA:
		gnutls_privkey_import_ext4(pkey, info, NULL,
					   tpm2_ec_sign_hash_fn, NULL,
					   tpm2_deinit_fn, ec_key_info, 0);
		pkey->key.ext.pk_params_func = tpm2_convert_public;
		break;

	default:
		ret = GNUTLS_E_TPM_ERROR;
		goto out_tpmkey;
	}

	ret = 0;
	info = NULL; /* part of pkey now */

out_tpmkey:
	asn1_delete_structure(&tpmkey);
	release_tpm2_ctx(info);
out_asn1:
	gnutls_free(asn1.data);
	return ret;
}

void _gnutls_tpm2_deinit(void)
{
	tpm2_esys_deinit();
}
