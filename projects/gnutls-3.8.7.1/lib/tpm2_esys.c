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

/* Portions taken from tpm2-tss-engine, copyright as below: */

/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of tpm2-tss-engine nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include "config.h"

#include "gnutls_int.h"
#include "abstract_int.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define RTLD_NOW 0
#define RTLD_GLOBAL 0
#else
#include <dlfcn.h>
#endif

#ifndef TSS2_ESYS_LIBRARY_SONAME
#define TSS2_ESYS_LIBRARY_SONAME "none"
#endif

#ifndef TSS2_MU_LIBRARY_SONAME
#define TSS2_MU_LIBRARY_SONAME "none"
#endif

#ifndef TSS2_TCTILDR_LIBRARY_SONAME
#define TSS2_TCTILDR_LIBRARY_SONAME "none"
#endif

#include "tpm2.h"
#include "locks.h"

#include "dlwrap/tss2_mu.h"
#include "dlwrap/tss2_esys.h"
#include "dlwrap/tss2_tctildr.h"

struct tpm2_info_st {
	TPM2B_PUBLIC pub;
	TPM2B_PRIVATE priv;
	TPM2B_DIGEST userauth;
	TPM2B_DIGEST ownerauth;
	unsigned bits;
	bool need_userauth;
	bool need_ownerauth;
	bool did_ownerauth;
	unsigned int parent;
	struct pin_info_st *pin_info;
};

static TSS2_TCTI_CONTEXT *tcti_ctx;

#define PRIMARY_HASH_ALGORITHM TPM2_ALG_SHA256
#define PRIMARY_OBJECT_ATTRIBUTES                                        \
	(TPMA_OBJECT_USERWITHAUTH | TPMA_OBJECT_RESTRICTED |             \
	 TPMA_OBJECT_DECRYPT | TPMA_OBJECT_NODA | TPMA_OBJECT_FIXEDTPM | \
	 TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_SENSITIVEDATAORIGIN)

static const TPM2B_PUBLIC primary_template_rsa = {
	.publicArea = {
		       .type = TPM2_ALG_RSA,
		       .nameAlg = PRIMARY_HASH_ALGORITHM,
		       .objectAttributes = PRIMARY_OBJECT_ATTRIBUTES,
		       .authPolicy = {
				      .size = 0,
				      },
		       .parameters.rsaDetail = {
						.symmetric = {
							      .algorithm =
							      TPM2_ALG_AES,
							      .keyBits.aes =
							      128,
							      .mode.aes =
							      TPM2_ALG_CFB,
							      },
						.scheme = {
							   .scheme =
							   TPM2_ALG_NULL,
							   .details = {}
							   },
						.keyBits = 2048,
						.exponent = 0,
						},
		       .unique.rsa = {
				      .size = 0,
				      }
		       }
};

static const TPM2B_PUBLIC primary_template_ecc = {
	.publicArea = {
		       .type = TPM2_ALG_ECC,
		       .nameAlg = PRIMARY_HASH_ALGORITHM,
		       .objectAttributes = PRIMARY_OBJECT_ATTRIBUTES,
		       .authPolicy = {
				      .size = 0,
				      },
		       .parameters.eccDetail = {
						.symmetric = {
							      .algorithm =
							      TPM2_ALG_AES,
							      .keyBits.aes =
							      128,
							      .mode.aes =
							      TPM2_ALG_CFB,
							      },
						.scheme = {
							   .scheme =
							   TPM2_ALG_NULL,
							   .details = {}
							   },
						.curveID = TPM2_ECC_NIST_P256,
						.kdf = {
							.scheme = TPM2_ALG_NULL,
							.details = {}
							},
						},
		       .unique.ecc = {
				      .x.size = 0,
				      .y.size = 0}
		       }
};

static const TPM2B_SENSITIVE_CREATE primary_sensitive = {
	.sensitive = {
		      .userAuth = {
				   .size = 0,
				   },
		      .data = {
			       .size = 0,
			       }
		      }
};

static const TPM2B_DATA all_outside_info = {
	.size = 0,
};

static const TPML_PCR_SELECTION all_creation_pcr = {
	.count = 0,
};

#define rc_is_key_auth_failed(rc) (((rc) & 0xff) == TPM2_RC_BAD_AUTH)
#define rc_is_parent_auth_failed(rc) (((rc) & 0xff) == TPM2_RC_AUTH_FAIL)

struct tpm2_info_st *tpm2_info_init(struct pin_info_st *pin)
{
	struct tpm2_info_st *t = gnutls_calloc(1, sizeof(struct tpm2_info_st));

	if (t == NULL) {
		return NULL;
	}

	t->pin_info = pin;

	return t;
}

static int tpm2_pin(struct pin_info_st *pin_info, const char *url,
		    const char *label, char *pin, unsigned int pin_size)
{
	int ret;

	if (!label) {
		label = "unknown";
	}

	ret = _gnutls_retrieve_pin(pin_info, url, label, 0, pin, pin_size);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}
	return ret;
}

static void install_tpm_passphrase(TPM2B_DIGEST *auth, char *pass)
{
	if (strlen(pass) > sizeof(auth->buffer) - 1) {
		_gnutls_debug_log("tpm2: password too long; truncating\n");
	}
	auth->size = strlen(pass);
	snprintf((char *)auth->buffer, sizeof(auth->buffer), "%s", pass);
	zeroize_key(pass, auth->size);
}

/* Figure out usable primary template according to the capabilities of
 * the TPM chip; ECC is preferred over RSA for performance reasons.
 */
static const TPM2B_PUBLIC *get_primary_template(ESYS_CONTEXT *ctx)
{
	TPMS_CAPABILITY_DATA *capability_data;
	UINT32 i;
	TSS2_RC rc;

	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_GetCapability)(
		ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, TPM2_CAP_ALGS, 0,
		TPM2_MAX_CAP_ALGS, NULL, &capability_data);
	if (rc) {
		_gnutls_debug_log("tpm2: Esys_GetCapability failed: 0x%x\n",
				  rc);
		return NULL;
	}

	for (i = 0; i < capability_data->data.algorithms.count; i++) {
		if (capability_data->data.algorithms.algProperties[i].alg ==
		    TPM2_ALG_ECC) {
			GNUTLS_TSS2_ESYS_FUNC(Esys_Free)(capability_data);
			return &primary_template_ecc;
		}
	}

	for (i = 0; i < capability_data->data.algorithms.count; i++) {
		if (capability_data->data.algorithms.algProperties[i].alg ==
		    TPM2_ALG_RSA) {
			GNUTLS_TSS2_ESYS_FUNC(Esys_Free)(capability_data);
			return &primary_template_rsa;
		}
	}

	GNUTLS_TSS2_ESYS_FUNC(Esys_Free)(capability_data);
	_gnutls_debug_log("tpm2: unable to find primary template\n");
	return NULL;
}

static const char *tpm2_hierarchy_name(TPM2_RH hierarchy)
{
	switch (hierarchy) {
	case TPM2_RH_OWNER:
		return "owner";
	case TPM2_RH_NULL:
		return "null";
	case TPM2_RH_ENDORSEMENT:
		return "endorsement";
	case TPM2_RH_PLATFORM:
		return "platform";
	default:
		gnutls_assert();
		return NULL;
	}
}

static ESYS_TR tpm2_hierarchy_to_esys_handle(TPM2_RH hierarchy)
{
	switch (hierarchy) {
	case TPM2_RH_OWNER:
		return ESYS_TR_RH_OWNER;
	case TPM2_RH_NULL:
		return ESYS_TR_RH_NULL;
	case TPM2_RH_ENDORSEMENT:
		return ESYS_TR_RH_ENDORSEMENT;
	case TPM2_RH_PLATFORM:
		return ESYS_TR_RH_PLATFORM;
	default:
		gnutls_assert();
		return ESYS_TR_NONE;
	}
}

static int init_tpm2_primary(struct tpm2_info_st *info, ESYS_CONTEXT *ctx,
			     ESYS_TR *primary_handle)
{
	TSS2_RC rc;
	const char *hierarchy_name;
	ESYS_TR hierarchy;
	const TPM2B_PUBLIC *primary_template;

	hierarchy_name = tpm2_hierarchy_name(info->parent);
	hierarchy = tpm2_hierarchy_to_esys_handle(info->parent);

	if (!hierarchy_name || hierarchy == ESYS_TR_NONE) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	_gnutls_debug_log("tpm2: creating primary key under %s hierarchy\n",
			  hierarchy_name);
reauth:
	if (info->need_ownerauth) {
		char pass[GNUTLS_PKCS11_MAX_PIN_LEN];
		if (tpm2_pin(info->pin_info, "tpm2:", hierarchy_name, pass,
			     sizeof(pass))) {
			return gnutls_assert_val(
				GNUTLS_E_TPM_KEY_PASSWORD_ERROR);
		}
		install_tpm_passphrase(&info->ownerauth, pass);
		info->need_ownerauth = false;
	}
	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_TR_SetAuth)(ctx, hierarchy,
						    &info->ownerauth);
	if (rc) {
		_gnutls_debug_log("tpm2: Esys_TR_SetAuth failed: 0x%x\n", rc);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}
	primary_template = get_primary_template(ctx);
	if (!primary_template) {
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}
	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_CreatePrimary)(
		ctx, hierarchy, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE,
		&primary_sensitive, primary_template, &all_outside_info,
		&all_creation_pcr, primary_handle, NULL, NULL, NULL, NULL);
	if (rc_is_key_auth_failed(rc)) {
		_gnutls_debug_log(
			"tpm2: Esys_CreatePrimary owner auth failed\n");
		info->need_ownerauth = true;
		goto reauth;
	} else if (rc) {
		_gnutls_debug_log("tpm2: Esys_CreatePrimary failed: 0x%x\n",
				  rc);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}
	return 0;
}

#define parent_is_generated(parent) \
	((parent) >> TPM2_HR_SHIFT == TPM2_HT_PERMANENT)
#define parent_is_persistent(parent) \
	((parent) >> TPM2_HR_SHIFT == TPM2_HT_PERSISTENT)

static int init_tpm2_key(ESYS_CONTEXT **ctx, ESYS_TR *key_handle,
			 struct tpm2_info_st *info)
{
	ESYS_TR parent_handle = ESYS_TR_NONE;
	TSS2_RC rc;

	*key_handle = ESYS_TR_NONE;

	_gnutls_debug_log("tpm2: establishing connection with TPM\n");

	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_Initialize)(ctx, tcti_ctx, NULL);
	if (rc) {
		gnutls_assert();
		_gnutls_debug_log("tpm2: Esys_Initialize failed: 0x%x\n", rc);
		goto error;
	}

	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_Startup)(*ctx, TPM2_SU_CLEAR);
	if (rc == TPM2_RC_INITIALIZE) {
		_gnutls_debug_log(
			"tpm2: was already started up thus false positive failing in tpm2tss log\n");
	} else if (rc) {
		gnutls_assert();
		_gnutls_debug_log("tpm2: Esys_Startup failed: 0x%x\n", rc);
		goto error;
	}

	if (parent_is_generated(info->parent)) {
		if (init_tpm2_primary(info, *ctx, &parent_handle)) {
			gnutls_assert();
			goto error;
		}
	} else {
		rc = GNUTLS_TSS2_ESYS_FUNC(Esys_TR_FromTPMPublic)(
			*ctx, info->parent, ESYS_TR_NONE, ESYS_TR_NONE,
			ESYS_TR_NONE, &parent_handle);
		if (rc) {
			gnutls_assert();
			_gnutls_debug_log(
				"tpm2: Esys_TR_FromTPMPublic failed for parent 0x%x: 0x%x\n",
				info->parent, rc);
			goto error;
		}
		/* If we don't already have a password (and haven't already authenticated
		 * successfully), check the NODA flag on the parent and demand one if DA
		 * protection is enabled (since that strongly implies there is a non-empty
		 * password). */
		if (!info->did_ownerauth && !info->ownerauth.size) {
			TPM2B_PUBLIC *pub = NULL;

			rc = GNUTLS_TSS2_ESYS_FUNC(Esys_ReadPublic)(
				*ctx, parent_handle, ESYS_TR_NONE, ESYS_TR_NONE,
				ESYS_TR_NONE, &pub, NULL, NULL);
			if (!rc && !(pub->publicArea.objectAttributes &
				     TPMA_OBJECT_NODA)) {
				info->need_ownerauth = true;
			}
			GNUTLS_TSS2_ESYS_FUNC(Esys_Free)(pub);
		}
	reauth:
		if (info->need_ownerauth) {
			char pass[GNUTLS_PKCS11_MAX_PIN_LEN];
			if (tpm2_pin(info->pin_info, "tpm2:", "parent", pass,
				     sizeof(pass))) {
				return gnutls_assert_val(
					GNUTLS_E_TPM_KEY_PASSWORD_ERROR);
			}
			install_tpm_passphrase(&info->ownerauth, pass);
			info->need_ownerauth = false;
		}
		rc = GNUTLS_TSS2_ESYS_FUNC(Esys_TR_SetAuth)(*ctx, parent_handle,
							    &info->ownerauth);
		if (rc) {
			gnutls_assert();
			_gnutls_debug_log(
				"tpm2: Esys_TR_SetAuth failed: 0x%x\n", rc);
			goto error;
		}
	}

	_gnutls_debug_log("tpm2: loading TPM2 key blob, parent handle 0x%x\n",
			  parent_handle);

	rc = GNUTLS_TSS2_ESYS_FUNC(
		Esys_Load)(*ctx, parent_handle, ESYS_TR_PASSWORD, ESYS_TR_NONE,
			   ESYS_TR_NONE, &info->priv, &info->pub, key_handle);
	if (rc_is_parent_auth_failed(rc)) {
		gnutls_assert();
		_gnutls_debug_log("tpm2: Esys_Load auth failed\n");
		info->need_ownerauth = true;
		goto reauth;
	}
	if (rc) {
		gnutls_assert();
		_gnutls_debug_log("tpm2: Esys_Load failed: 0x%x\n", rc);
		goto error;
	}
	info->did_ownerauth = true;

	if (parent_is_generated(info->parent)) {
		rc = GNUTLS_TSS2_ESYS_FUNC(Esys_FlushContext)(*ctx,
							      parent_handle);
		if (rc) {
			_gnutls_debug_log(
				"tpm2: Esys_FlushContext for generated primary failed: 0x%x\n",
				rc);
		}
		/* But it's non-fatal. */
	}

	return 0;
error:
	if (parent_is_generated(info->parent) &&
	    parent_handle != ESYS_TR_NONE) {
		GNUTLS_TSS2_ESYS_FUNC(Esys_FlushContext)(*ctx, parent_handle);
	}
	if (*key_handle != ESYS_TR_NONE) {
		GNUTLS_TSS2_ESYS_FUNC(Esys_FlushContext)(*ctx, *key_handle);
	}
	*key_handle = ESYS_TR_NONE;

	GNUTLS_TSS2_ESYS_FUNC(Esys_Finalize)(ctx);
	return GNUTLS_E_TPM_ERROR;
}

static int auth_tpm2_key(struct tpm2_info_st *info, ESYS_CONTEXT *ctx,
			 ESYS_TR key_handle)
{
	TSS2_RC rc;

	if (info->need_userauth) {
		char pass[GNUTLS_PKCS11_MAX_PIN_LEN];
		if (tpm2_pin(info->pin_info, "tpm2:", "key", pass,
			     sizeof(pass))) {
			return gnutls_assert_val(
				GNUTLS_E_TPM_KEY_PASSWORD_ERROR);
		}

		install_tpm_passphrase(&info->userauth, pass);
		info->need_userauth = false;
	}

	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_TR_SetAuth)(ctx, key_handle,
						    &info->userauth);
	if (rc) {
		_gnutls_debug_log("tpm2: Esys_TR_SetAuth failed: 0x%x\n", rc);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}
	return 0;
}

int tpm2_rsa_sign_hash_fn(gnutls_privkey_t key, gnutls_sign_algorithm_t algo,
			  void *_info, unsigned int flags,
			  const gnutls_datum_t *data, gnutls_datum_t *sig)
{
	struct tpm2_info_st *info = _info;
	int ret;
	ESYS_CONTEXT *ectx = NULL;
	TPM2B_PUBLIC_KEY_RSA digest, *tsig = NULL;
	TPM2B_DATA label = { .size = 0 };
	TPMT_RSA_DECRYPT in_scheme = { .scheme = TPM2_ALG_NULL };
	ESYS_TR key_handle = ESYS_TR_NONE;
	const gnutls_sign_entry_st *se;
	gnutls_x509_spki_st params;
	TSS2_RC rc;

	_gnutls_debug_log("tpm2: RSA (%s) sign function called for %d bytes\n",
			  gnutls_sign_get_name(algo), data->size);

	se = _gnutls_sign_to_entry(algo);
	if (unlikely(se == NULL)) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	switch (se->pk) {
	case GNUTLS_PK_RSA_PSS:
		/* This code is a copy from privkey_sign_* functions and
		 * exercised twice because gnutls_privkey_sign_hash_func
		 * currently does not provide access to SPKI params
		 * calculated. */
		ret = _gnutls_privkey_get_spki_params(key, &params);
		if (ret < 0) {
			return gnutls_assert_val(ret);
		}

		flags |= GNUTLS_PRIVKEY_SIGN_FLAG_RSA_PSS;
		ret = _gnutls_privkey_update_spki_params(
			key, key->pk_algorithm, se->hash, flags, &params);
		if (ret < 0) {
			return gnutls_assert_val(ret);
		}

		FIX_SIGN_PARAMS(params, flags, se->hash);

		digest.size = info->pub.publicArea.unique.rsa.size;
		ret = _gnutls_rsa_pss_sign_pad(&params, tpm2_rsa_key_bits(info),
					       data, digest.buffer,
					       digest.size);
		if (ret < 0) {
			return gnutls_assert_val(GNUTLS_E_PK_SIGN_FAILED);
		}
		break;
	case GNUTLS_PK_RSA:
		digest.size = info->pub.publicArea.unique.rsa.size;
		ret = _gnutls_rsa_pkcs1_sign_pad(tpm2_rsa_key_bits(info), data,
						 digest.buffer, digest.size);
		if (ret < 0) {
			return gnutls_assert_val(GNUTLS_E_PK_SIGN_FAILED);
		}
		break;
	default:
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	}

	ret = init_tpm2_key(&ectx, &key_handle, info);
	if (ret < 0) {
		gnutls_assert();
		goto out;
	}
reauth:
	ret = auth_tpm2_key(info, ectx, key_handle);
	if (ret < 0) {
		gnutls_assert();
		goto out;
	}

	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_RSA_Decrypt)(
		ectx, key_handle, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE,
		&digest, &in_scheme, &label, &tsig);
	if (rc_is_key_auth_failed(rc)) {
		gnutls_assert();
		_gnutls_debug_log("tpm2: Esys_RSA_Decrypt auth failed\n");
		info->need_userauth = true;
		goto reauth;
	}
	if (rc) {
		gnutls_assert();
		_gnutls_debug_log(
			"tpm2: failed to generate RSA signature: 0x%x\n", rc);
		goto out;
	}

	ret = _gnutls_set_datum(sig, tsig->buffer, tsig->size);
out:
	GNUTLS_TSS2_ESYS_FUNC(Esys_Free)(tsig);

	if (key_handle != ESYS_TR_NONE) {
		GNUTLS_TSS2_ESYS_FUNC(Esys_FlushContext)(ectx, key_handle);
	}

	if (ectx) {
		GNUTLS_TSS2_ESYS_FUNC(Esys_Finalize)(&ectx);
	}

	return ret;
}

int tpm2_ec_sign_hash_fn(gnutls_privkey_t key, gnutls_sign_algorithm_t algo,
			 void *_info, unsigned int flags,
			 const gnutls_datum_t *data, gnutls_datum_t *sig)
{
	struct tpm2_info_st *info = _info;
	int ret;
	ESYS_CONTEXT *ectx = NULL;
	TPM2B_DIGEST digest;
	TPMT_SIGNATURE *tsig = NULL;
	ESYS_TR key_handle = ESYS_TR_NONE;
	TSS2_RC rc;
	TPMT_TK_HASHCHECK validation = { .tag = TPM2_ST_HASHCHECK,
					 .hierarchy = TPM2_RH_NULL,
					 .digest.size = 0 };
	TPMT_SIG_SCHEME in_scheme = { .scheme = TPM2_ALG_ECDSA };
	gnutls_datum_t sig_r, sig_s;

	_gnutls_debug_log("tpm2: EC sign function called for %d bytes\n",
			  data->size);

	switch (algo) {
	case GNUTLS_SIGN_ECDSA_SHA1:
		in_scheme.details.ecdsa.hashAlg = TPM2_ALG_SHA1;
		break;
	case GNUTLS_SIGN_ECDSA_SHA256:
	case GNUTLS_SIGN_ECDSA_SECP256R1_SHA256:
		in_scheme.details.ecdsa.hashAlg = TPM2_ALG_SHA256;
		break;
	case GNUTLS_SIGN_ECDSA_SHA384:
	case GNUTLS_SIGN_ECDSA_SECP384R1_SHA384:
		in_scheme.details.ecdsa.hashAlg = TPM2_ALG_SHA384;
		break;
	case GNUTLS_SIGN_ECDSA_SHA512:
	case GNUTLS_SIGN_ECDSA_SECP521R1_SHA512:
		in_scheme.details.ecdsa.hashAlg = TPM2_ALG_SHA512;
		break;
	default:
		_gnutls_debug_log("tpm2: Unknown TPM2 EC digest size %d\n",
				  data->size);
		return GNUTLS_E_PK_SIGN_FAILED;
	}

	memcpy(digest.buffer, data->data, data->size);
	digest.size = data->size;

	ret = init_tpm2_key(&ectx, &key_handle, info);
	if (ret < 0) {
		gnutls_assert();
		goto out;
	}
reauth:
	ret = auth_tpm2_key(info, ectx, key_handle);
	if (ret < 0) {
		gnutls_assert();
		goto out;
	}

	rc = GNUTLS_TSS2_ESYS_FUNC(Esys_Sign)(ectx, key_handle,
					      ESYS_TR_PASSWORD, ESYS_TR_NONE,
					      ESYS_TR_NONE, &digest, &in_scheme,
					      &validation, &tsig);
	if (rc_is_key_auth_failed(rc)) {
		_gnutls_debug_log("tpm2: Esys_Sign auth failed\n");
		info->need_userauth = true;
		goto reauth;
	}
	if (rc) {
		_gnutls_debug_log(
			"tpm2: failed to generate EC signature: 0x%x\n", rc);
		goto out;
	}

	sig_r.data = tsig->signature.ecdsa.signatureR.buffer;
	sig_r.size = tsig->signature.ecdsa.signatureR.size;
	sig_s.data = tsig->signature.ecdsa.signatureS.buffer;
	sig_s.size = tsig->signature.ecdsa.signatureS.size;

	ret = gnutls_encode_rs_value(sig, &sig_r, &sig_s);
out:
	GNUTLS_TSS2_ESYS_FUNC(Esys_Free)(tsig);

	if (key_handle != ESYS_TR_NONE) {
		GNUTLS_TSS2_ESYS_FUNC(Esys_FlushContext)(ectx, key_handle);
	}

	if (ectx) {
		GNUTLS_TSS2_ESYS_FUNC(Esys_Finalize)(&ectx);
	}

	return ret;
}

/* We can't use GNUTLS_ONCE here, as it wouldn't allow manual unloading */
GNUTLS_STATIC_MUTEX(tpm2_esys_init_mutex);
static int _tpm2_esys_init = 0;

static int init_tcti(void)
{
	const char *tcti;
	const char *const tcti_vars[] = { "GNUTLS_TPM2_TCTI", "TPM2TOOLS_TCTI",
					  "TCTI", "TEST_TCTI" };
	size_t i;

	for (i = 0; i < sizeof(tcti_vars) / sizeof(tcti_vars[0]); i++) {
		tcti = secure_getenv(tcti_vars[i]);
		if (tcti && *tcti != '\0') {
			_gnutls_debug_log(
				"tpm2: TCTI configuration found in %s\n",
				tcti_vars[i]);
			break;
		}
	}

	if (tcti && *tcti != '\0') {
		TSS2_RC rc;

		rc = GNUTLS_TSS2_TCTILDR_FUNC(
			Tss2_TctiLdr_Initialize)(tcti, &tcti_ctx);
		if (rc) {
			_gnutls_debug_log(
				"tpm2: unable to initialize TCTI \"%s\": 0x%x\n",
				tcti, rc);
			return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
		}
	}

	return GNUTLS_E_SUCCESS;
}

static int tpm2_esys_ensure(void)
{
	int ret;

	if (_tpm2_esys_init)
		return GNUTLS_E_SUCCESS;

	ret = gnutls_static_mutex_lock(&tpm2_esys_init_mutex);
	if (unlikely(ret < 0))
		return gnutls_assert_val(ret);

	if (gnutls_tss2_esys_ensure_library(TSS2_ESYS_LIBRARY_SONAME,
					    RTLD_NOW | RTLD_GLOBAL) < 0) {
		_gnutls_debug_log(
			"tpm2: unable to initialize tss2-esys functions\n");
		ret = gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
		goto out;
	}

	if (gnutls_tss2_mu_ensure_library(TSS2_MU_LIBRARY_SONAME,
					  RTLD_NOW | RTLD_GLOBAL) < 0) {
		_gnutls_debug_log(
			"tpm2: unable to initialize tss2-mu functions\n");
		ret = gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
		goto out;
	}

	if (gnutls_tss2_tctildr_ensure_library(TSS2_TCTILDR_LIBRARY_SONAME,
					       RTLD_NOW | RTLD_GLOBAL) < 0) {
		_gnutls_debug_log(
			"tpm2: unable to initialize tss2-tctildr functions\n");
		ret = gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
		goto out;
	}

	ret = init_tcti();
	if (ret < 0)
		goto out;

	_tpm2_esys_init = 1;
	ret = GNUTLS_E_SUCCESS;

out:
	(void)gnutls_static_mutex_unlock(&tpm2_esys_init_mutex);

	return ret;
}

/* called by the global destructor through _gnutls_tpm2_deinit */
void tpm2_esys_deinit(void)
{
	if (_tpm2_esys_init) {
		if (tcti_ctx) {
			GNUTLS_TSS2_TCTILDR_FUNC(Tss2_TctiLdr_Finalize)
			(&tcti_ctx);
			tcti_ctx = NULL;
		}
		gnutls_tss2_esys_unload_library();
		gnutls_tss2_mu_unload_library();
		gnutls_tss2_tctildr_unload_library();
	}
	_tpm2_esys_init = 0;
}

int install_tpm2_key(struct tpm2_info_st *info, gnutls_privkey_t pkey,
		     unsigned int parent, bool emptyauth,
		     gnutls_datum_t *privdata, gnutls_datum_t *pubdata)
{
	TSS2_RC rc;
	int ret;

	ret = tpm2_esys_ensure();
	if (ret < 0)
		return gnutls_assert_val(ret);

	if (!parent_is_persistent(parent) && parent != TPM2_RH_OWNER &&
	    parent != TPM2_RH_NULL && parent != TPM2_RH_ENDORSEMENT &&
	    parent != TPM2_RH_PLATFORM) {
		_gnutls_debug_log("tpm2: Invalid TPM2 parent handle 0x%08x\n",
				  parent);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}

	info->parent = parent;

	rc = GNUTLS_TSS2_MU_FUNC(Tss2_MU_TPM2B_PRIVATE_Unmarshal)(
		privdata->data, privdata->size, NULL, &info->priv);
	if (rc) {
		_gnutls_debug_log(
			"tpm2: failed to import private key data: 0x%x\n", rc);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}

	rc = GNUTLS_TSS2_MU_FUNC(Tss2_MU_TPM2B_PUBLIC_Unmarshal)(
		pubdata->data, pubdata->size, NULL, &info->pub);
	if (rc) {
		_gnutls_debug_log(
			"tpm2: failed to import public key data: 0x%x\n", rc);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}

	info->need_userauth = !emptyauth;

	switch (info->pub.publicArea.type) {
	case TPM2_ALG_RSA:
		return GNUTLS_PK_RSA;
	case TPM2_ALG_ECC:
		return GNUTLS_PK_ECDSA;
	default:
		_gnutls_debug_log("tpm2: unsupported key type %d\n",
				  info->pub.publicArea.type);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}
}

uint16_t tpm2_key_curve(struct tpm2_info_st *info)
{
	return info->pub.publicArea.parameters.eccDetail.curveID;
}

int tpm2_rsa_key_bits(struct tpm2_info_st *info)
{
	return info->pub.publicArea.parameters.rsaDetail.keyBits;
}

void release_tpm2_ctx(struct tpm2_info_st *info)
{
	if (info) {
		zeroize_key(info->ownerauth.buffer,
			    sizeof(info->ownerauth.buffer));
		zeroize_key(info->userauth.buffer,
			    sizeof(info->userauth.buffer));
		gnutls_free(info);
	}
}

void tpm2_deinit_fn(gnutls_privkey_t key, void *priv)
{
	release_tpm2_ctx(priv);
}

static gnutls_ecc_curve_t tpm2_curve_to_gnutls_curve(TPMI_ECC_CURVE curve)
{
	switch (curve) {
	case TPM2_ECC_NIST_P192:
		return GNUTLS_ECC_CURVE_SECP192R1;
	case TPM2_ECC_NIST_P224:
		return GNUTLS_ECC_CURVE_SECP224R1;
	case TPM2_ECC_NIST_P256:
		return GNUTLS_ECC_CURVE_SECP256R1;
	case TPM2_ECC_NIST_P384:
		return GNUTLS_ECC_CURVE_SECP384R1;
	case TPM2_ECC_NIST_P521:
		return GNUTLS_ECC_CURVE_SECP521R1;
	default:
		return GNUTLS_ECC_CURVE_INVALID;
	}
}

static int convert_public_rsa(struct tpm2_info_st *info,
			      gnutls_pk_params_st *params)
{
	int ret;
	UINT32 exponent;

	memset(params, 0, sizeof(gnutls_pk_params_st));

	params->algo = GNUTLS_PK_RSA;
	params->params_nr = 2;

	ret = _gnutls_mpi_init_scan_nz(&params->params[RSA_MODULUS],
				       info->pub.publicArea.unique.rsa.buffer,
				       info->pub.publicArea.unique.rsa.size);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}

	exponent = info->pub.publicArea.parameters.rsaDetail.exponent;
	if (exponent == 0) {
		exponent = 0x10001;
	}
	ret = _gnutls_mpi_init(&params->params[RSA_PUB]);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}
	_gnutls_mpi_set_ui(params->params[RSA_PUB], exponent);

	return 0;
}

static int convert_public_ecc(struct tpm2_info_st *info,
			      gnutls_pk_params_st *params)
{
	int ret;

	TPMS_ECC_PARMS *detail = &info->pub.publicArea.parameters.eccDetail;
	TPMS_ECC_POINT *point = &info->pub.publicArea.unique.ecc;

	memset(params, 0, sizeof(gnutls_pk_params_st));

	params->algo = GNUTLS_PK_ECDSA;
	params->params_nr = 2;

	ret = _gnutls_mpi_init_scan_nz(&params->params[ECC_X], point->x.buffer,
				       point->x.size);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}
	ret = _gnutls_mpi_init_scan_nz(&params->params[ECC_Y], point->y.buffer,
				       point->y.size);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}

	params->curve = tpm2_curve_to_gnutls_curve(detail->curveID);
	if (params->curve == GNUTLS_ECC_CURVE_INVALID) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	return 0;
}

int tpm2_convert_public(gnutls_privkey_t key, void *_info,
			gnutls_pk_params_st *params)
{
	struct tpm2_info_st *info = _info;

	switch (info->pub.publicArea.type) {
	case TPM2_ALG_RSA:
		return convert_public_rsa(info, params);
	case TPM2_ALG_ECC:
		return convert_public_ecc(info, params);
	default:
		_gnutls_debug_log("tpm2: unsupported TPM2 key type %d\n",
				  info->pub.publicArea.type);
		return gnutls_assert_val(GNUTLS_E_TPM_ERROR);
	}

	return 0;
}
