/*
 * Copyright (C) 2010-2012 Free Software Foundation, Inc.
 * Copyright (C) 2013-2017 Nikos Mavrogiannopoulos
 * Copyright (C) 2016-2017 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GNUTLS.
 *
 * The GNUTLS library is free software; you can redistribute it and/or
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

/* This file contains the functions needed for RSA/DSA public key
 * encryption and signatures.
 */

#include "gnutls_int.h"
#include "mpi.h"
#include "pk.h"
#include "errors.h"
#include "datum.h"
#include "global.h"
#include "tls-sig.h"
#include "num.h"
#include "x509/x509_int.h"
#include "x509/common.h"
#include "random.h"
#include "pk.h"
#include <nettle/dsa.h>
#include "dsa-fips.h"
#include "rsa-fips.h"
#include <nettle/rsa.h>
#ifndef HAVE_NETTLE_RSA_OAEP_SHA256_ENCRYPT
#include "int/rsa-oaep.h"
#endif
#include <gnutls/crypto.h>
#include <nettle/bignum.h>
#include <nettle/ecc.h>
#include <nettle/ecdsa.h>
#include <nettle/ecc-curve.h>
#include <nettle/curve25519.h>
#include <nettle/curve448.h>
#include <nettle/eddsa.h>
#include <nettle/version.h>
#if ENABLE_GOST
#if NEED_INT_ECC
#include "ecc/gostdsa.h"
#include "ecc-gost-curve.h"
#else
#include <nettle/gostdsa.h>
#define gost_point_mul_g ecc_point_mul_g
#define gost_point_set ecc_point_set
#endif
#include "gost/gostdsa2.h"
#endif
#include "int/ecdsa-compute-k.h"
#include "int/dsa-compute-k.h"
#include "gnettle.h"
#include "fips.h"
#include "dh.h"
#ifdef HAVE_LIBOQS
#include "dlwrap/oqs.h"
#include "liboqs/liboqs.h"
#endif

static inline const struct ecc_curve *get_supported_nist_curve(int curve);
static inline const struct ecc_curve *get_supported_gost_curve(int curve);

static inline const char *get_supported_nist_curve_order(int curve);
static inline const char *get_supported_nist_curve_modulus(int curve);

/* When these callbacks are used for a nettle operation, the
 * caller must check the macro HAVE_LIB_ERROR() after the operation
 * is complete. If the macro is true, the operation is to be considered
 * failed (meaning the random generation failed).
 */
static void rnd_key_func(void *_ctx, size_t length, uint8_t *data)
{
	if (gnutls_rnd(GNUTLS_RND_KEY, data, length) < 0) {
		_gnutls_switch_lib_state(LIB_STATE_ERROR);
	}
}

static void rnd_tmpkey_func(void *_ctx, size_t length, uint8_t *data)
{
	if (gnutls_rnd(GNUTLS_RND_RANDOM, data, length) < 0) {
		_gnutls_switch_lib_state(LIB_STATE_ERROR);
	}
}

static void rnd_nonce_func(void *_ctx, size_t length, uint8_t *data)
{
	if (gnutls_rnd(GNUTLS_RND_NONCE, data, length) < 0) {
		_gnutls_switch_lib_state(LIB_STATE_ERROR);
	}
}

static void rnd_datum_func(void *ctx, size_t length, uint8_t *data)
{
	gnutls_datum_t *d = ctx;

	if (length > d->size) {
		memset(data, 0, length - d->size);
		memcpy(data + (length - d->size), d->data, d->size);
	} else {
		memcpy(data, d->data, length);
	}
}

static void rnd_nonce_func_fallback(void *_ctx, size_t length, uint8_t *data)
{
	if (unlikely(_gnutls_get_lib_state() != LIB_STATE_SELFTEST)) {
		_gnutls_switch_lib_state(LIB_STATE_ERROR);
	}

	memset(data, 0xAA, length);
}

static void ecc_scalar_zclear(struct ecc_scalar *s)
{
	zeroize_key(s->p, ecc_size(s->ecc) * sizeof(mp_limb_t));
	ecc_scalar_clear(s);
}

static void ecc_point_zclear(struct ecc_point *p)
{
	zeroize_key(p->p, ecc_size_a(p->ecc) * sizeof(mp_limb_t));
	ecc_point_clear(p);
}

static void _dsa_params_get(const gnutls_pk_params_st *pk_params,
			    struct dsa_params *pub)
{
	memcpy(pub->p, pk_params->params[DSA_P], SIZEOF_MPZT);

	if (pk_params->params[DSA_Q])
		memcpy(&pub->q, pk_params->params[DSA_Q], SIZEOF_MPZT);
	memcpy(pub->g, pk_params->params[DSA_G], SIZEOF_MPZT);
}

static void _rsa_params_to_privkey(const gnutls_pk_params_st *pk_params,
				   struct rsa_private_key *priv)
{
	memcpy(priv->d, pk_params->params[RSA_PRIV], SIZEOF_MPZT);
	memcpy(priv->p, pk_params->params[RSA_PRIME1], SIZEOF_MPZT);
	memcpy(priv->q, pk_params->params[RSA_PRIME2], SIZEOF_MPZT);
	memcpy(priv->c, pk_params->params[RSA_COEF], SIZEOF_MPZT);
	memcpy(priv->a, pk_params->params[RSA_E1], SIZEOF_MPZT);
	memcpy(priv->b, pk_params->params[RSA_E2], SIZEOF_MPZT);
	/* we do not rsa_private_key_prepare() because it involves a multiplication.
	 * we call it once when we import the parameters */
	priv->size = nettle_mpz_sizeinbase_256_u(
		TOMPZ(pk_params->params[RSA_MODULUS]));
}

/* returns a negative value on invalid pubkey */
static int _rsa_params_to_pubkey(const gnutls_pk_params_st *pk_params,
				 struct rsa_public_key *pub)
{
	memcpy(pub->n, pk_params->params[RSA_MODULUS], SIZEOF_MPZT);
	memcpy(pub->e, pk_params->params[RSA_PUB], SIZEOF_MPZT);
	if (rsa_public_key_prepare(pub) == 0)
		return gnutls_assert_val(GNUTLS_E_PK_INVALID_PUBKEY);

	return 0;
}

static int _ecc_params_to_privkey(const gnutls_pk_params_st *pk_params,
				  struct ecc_scalar *priv,
				  const struct ecc_curve *curve)
{
	ecc_scalar_init(priv, curve);
	if (ecc_scalar_set(priv, pk_params->params[ECC_K]) == 0) {
		ecc_scalar_clear(priv);
		return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);
	}

	return 0;
}

static int _ecc_params_to_pubkey(const gnutls_pk_params_st *pk_params,
				 struct ecc_point *pub,
				 const struct ecc_curve *curve)
{
	ecc_point_init(pub, curve);
	if (ecc_point_set(pub, pk_params->params[ECC_X],
			  pk_params->params[ECC_Y]) == 0) {
		ecc_point_clear(pub);
		return gnutls_assert_val(GNUTLS_E_PK_INVALID_PUBKEY);
	}

	return 0;
}

#if ENABLE_GOST
static int _gost_params_to_privkey(const gnutls_pk_params_st *pk_params,
				   struct ecc_scalar *priv,
				   const struct ecc_curve *curve)
{
	ecc_scalar_init(priv, curve);
	if (ecc_scalar_set(priv, pk_params->params[GOST_K]) == 0) {
		ecc_scalar_clear(priv);
		return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);
	}

	return 0;
}

static int _gost_params_to_pubkey(const gnutls_pk_params_st *pk_params,
				  struct ecc_point *pub,
				  const struct ecc_curve *curve)
{
	ecc_point_init(pub, curve);
	if (gost_point_set(pub, pk_params->params[GOST_X],
			   pk_params->params[GOST_Y]) == 0) {
		ecc_point_clear(pub);
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	return 0;
}
#endif

static int ecc_shared_secret(struct ecc_scalar *private_key,
			     struct ecc_point *public_key, void *out,
			     unsigned size)
{
	struct ecc_point r;
	mpz_t x, y;
	int ret = 0;

	mpz_init(x);
	mpz_init(y);
	ecc_point_init(&r, public_key->ecc);

	ecc_point_mul(&r, private_key, public_key);

	ecc_point_get(&r, x, y);

	/* Check if the point is not an identity element.  Note that this cannot
	 * happen in nettle implementation, because it cannot represent an
	 * infinity point. */
	if (mpz_cmp_ui(x, 0) == 0 && mpz_cmp_ui(y, 0) == 0) {
		ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
		goto cleanup;
	}

	nettle_mpz_get_str_256(size, out, x);

cleanup:
	mpz_clear(x);
	mpz_clear(y);
	ecc_point_clear(&r);

	return ret;
}

#define MAX_DH_BITS DEFAULT_MAX_VERIFY_BITS
/* This is used when we have no idea on the structure
 * of p-1 used by the peer. It is still a conservative
 * choice, but small than what we've been using before.
 */
#define DH_EXPONENT_SIZE(p_size) (2 * _gnutls_pk_bits_to_subgroup_bits(p_size))

static inline int edwards_curve_mul(gnutls_pk_algorithm_t algo, uint8_t *q,
				    const uint8_t *n, const uint8_t *p)
{
	switch (algo) {
	case GNUTLS_PK_ECDH_X25519:
		curve25519_mul(q, n, p);
		return 0;
	case GNUTLS_PK_ECDH_X448:
		curve448_mul(q, n, p);
		return 0;
	default:
		return gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
	}
}

/* This is used for DH or ECDH key derivation. In DH for example
 * it is given the peers Y and our x, and calculates Y^x
 */
static int _wrap_nettle_pk_derive(gnutls_pk_algorithm_t algo,
				  gnutls_datum_t *out,
				  const gnutls_pk_params_st *priv,
				  const gnutls_pk_params_st *pub,
				  const gnutls_datum_t *nonce,
				  unsigned int flags)
{
	int ret;
	bool not_approved = false;

	switch (algo) {
	case GNUTLS_PK_DH: {
		bigint_t f, x, q, prime;
		bigint_t k = NULL, primesub1 = NULL, r = NULL;
		unsigned int bits;

		if (nonce != NULL) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		f = pub->params[DH_Y];
		x = priv->params[DH_X];
		q = priv->params[DH_Q];
		prime = priv->params[DH_P];

		ret = _gnutls_mpi_init_multi(&k, &primesub1, &r, NULL);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = _gnutls_mpi_sub_ui(primesub1, prime, 1);
		if (ret < 0) {
			gnutls_assert();
			goto dh_cleanup;
		}

		/* check if f==0,1, or f >= p-1 */
		if ((_gnutls_mpi_cmp_ui(f, 1) == 0) ||
		    (_gnutls_mpi_cmp_ui(f, 0) == 0) ||
		    (_gnutls_mpi_cmp(f, primesub1) >= 0)) {
			gnutls_assert();
			ret = GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
			goto dh_cleanup;
		}

		/* if we have Q check that y ^ q mod p == 1 */
		if (q != NULL) {
			ret = _gnutls_mpi_powm(r, f, q, prime);
			if (ret < 0) {
				gnutls_assert();
				goto dh_cleanup;
			}
			ret = _gnutls_mpi_cmp_ui(r, 1);
			if (ret != 0) {
				gnutls_assert();
				ret = GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
				goto dh_cleanup;
			}
		} else if ((flags & PK_DERIVE_TLS13) &&
			   _gnutls_fips_mode_enabled()) {
			/* Mandatory in FIPS mode for TLS 1.3 */
			ret = gnutls_assert_val(
				GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER);
			goto dh_cleanup;
		}

		/* prevent denial of service */
		bits = _gnutls_mpi_get_nbits(prime);
		if (bits == 0 || bits > MAX_DH_BITS) {
			gnutls_assert();
			ret = GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
			goto dh_cleanup;
		}

		if (bits < 2048) {
			not_approved = true;
		}

		ret = _gnutls_mpi_powm(k, f, x, prime);
		if (ret < 0) {
			gnutls_assert();
			goto dh_cleanup;
		}

		/* check if k==0,1, or k = p-1 */
		if ((_gnutls_mpi_cmp_ui(k, 1) == 0) ||
		    (_gnutls_mpi_cmp_ui(k, 0) == 0) ||
		    (_gnutls_mpi_cmp(k, primesub1) == 0)) {
			ret = gnutls_assert_val(
				GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER);
			goto dh_cleanup;
		}

		if (flags & PK_DERIVE_TLS13) {
			ret = _gnutls_mpi_dprint_size(k, out, (bits + 7) / 8);
		} else {
			ret = _gnutls_mpi_dprint(k, out);
		}

		if (ret < 0) {
			gnutls_assert();
			goto dh_cleanup;
		}

		ret = 0;
	dh_cleanup:
		_gnutls_mpi_release(&r);
		_gnutls_mpi_release(&primesub1);
		zrelease_temp_mpi_key(&k);
		if (ret < 0)
			goto cleanup;

		break;
	}
	case GNUTLS_PK_EC: {
		struct ecc_scalar ecc_priv;
		struct ecc_point ecc_pub;
		const struct ecc_curve *curve;
		struct ecc_scalar n;
		struct ecc_scalar m;
		struct ecc_point r;
		mpz_t x, y, xx, yy, nn, mm;

		out->data = NULL;

		if (nonce != NULL) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		curve = get_supported_nist_curve(priv->curve);
		if (curve == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		/* P-192 is not supported in FIPS 140-3 */
		if (priv->curve == GNUTLS_ECC_CURVE_SECP192R1) {
			not_approved = true;
		}

		mpz_init(x);
		mpz_init(y);
		mpz_init(xx);
		mpz_init(yy);
		mpz_init(nn);
		mpz_init(mm);

		ecc_scalar_init(&n, curve);
		ecc_scalar_init(&m, curve);
		ecc_point_init(&r, curve);

		ret = _ecc_params_to_pubkey(pub, &ecc_pub, curve);
		if (ret < 0) {
			gnutls_assert();
			goto ecc_fail_cleanup;
		}

		ret = _ecc_params_to_privkey(priv, &ecc_priv, curve);
		if (ret < 0) {
			ecc_point_clear(&ecc_pub);
			gnutls_assert();
			goto ecc_fail_cleanup;
		}

		out->size = gnutls_ecc_curve_get_size(priv->curve);
		/*ecc_size(curve)*sizeof(mp_limb_t); */
		out->data = gnutls_malloc(out->size);
		if (out->data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto ecc_cleanup;
		}

		/* Perform ECC Full Public-Key Validation Routine
			 * according to SP800-56A (revision 3), 5.6.2.3.3.
			 */

		/* Step 1: verify that Q is not an identity
			 * element (an infinity point). Note that this
			 * cannot happen in the nettle implementation,
			 * because it cannot represent an infinity point
			 * on curves. */
		ret = ecc_shared_secret(&ecc_priv, &ecc_pub, out->data,
					out->size);
		if (ret < 0) {
			gnutls_free(out->data);
			goto ecc_cleanup;
		}
#ifdef ENABLE_FIPS140
		if (_gnutls_fips_mode_enabled()) {
			const char *order, *modulus;

			ecc_point_mul(&r, &ecc_priv, &ecc_pub);
			ecc_point_get(&r, x, y);

			/* Step 2: verify that both coordinates of Q are
				 * in the range [0, p - 1].
				 *
				 * Step 3: verify that Q lie on the curve
				 *
				 * Both checks are performed in nettle.  */
			if (!ecc_point_set(&r, x, y)) {
				ret = gnutls_assert_val(
					GNUTLS_E_ILLEGAL_PARAMETER);
				goto ecc_cleanup;
			}

			/* Step 4: verify that n * Q, where n is the
				 * curve order, result in an identity element
				 *
				 * Since nettle internally cannot represent an
				 * identity element on curves, we validate this
				 * instead:
				 *
				 *   (n - 1) * Q = -Q
				 *
				 * That effectively means: n * Q = -Q + Q = O
				 */
			order = get_supported_nist_curve_order(priv->curve);
			if (unlikely(order == NULL)) {
				ret = gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
				goto ecc_cleanup;
			}

			ret = mpz_set_str(nn, order, 16);
			if (unlikely(ret < 0)) {
				ret = gnutls_assert_val(
					GNUTLS_E_MPI_SCAN_FAILED);
				goto ecc_cleanup;
			}

			modulus = get_supported_nist_curve_modulus(priv->curve);
			if (unlikely(modulus == NULL)) {
				ret = gnutls_assert_val(
					GNUTLS_E_INTERNAL_ERROR);
				goto ecc_cleanup;
			}

			ret = mpz_set_str(mm, modulus, 16);
			if (unlikely(ret < 0)) {
				ret = gnutls_assert_val(
					GNUTLS_E_MPI_SCAN_FAILED);
				goto ecc_cleanup;
			}

			/* (n - 1) * Q = -Q */
			mpz_sub_ui(nn, nn, 1);
			ecc_scalar_set(&n, nn);
			ecc_point_mul(&r, &n, &r);
			ecc_point_get(&r, xx, yy);
			mpz_sub(mm, mm, y);

			if (mpz_cmp(xx, x) != 0 || mpz_cmp(yy, mm) != 0) {
				ret = gnutls_assert_val(
					GNUTLS_E_ILLEGAL_PARAMETER);
				goto ecc_cleanup;
			}
		} else {
			not_approved = true;
		}
#endif

	ecc_cleanup:
		ecc_point_clear(&ecc_pub);
		ecc_scalar_zclear(&ecc_priv);
	ecc_fail_cleanup:
		mpz_clear(x);
		mpz_clear(y);
		mpz_clear(xx);
		mpz_clear(yy);
		mpz_clear(nn);
		mpz_clear(mm);
		ecc_point_clear(&r);
		ecc_scalar_clear(&n);
		ecc_scalar_clear(&m);
		if (ret < 0)
			goto cleanup;
		break;
	}
	case GNUTLS_PK_ECDH_X25519:
	case GNUTLS_PK_ECDH_X448: {
		unsigned size = gnutls_ecc_curve_get_size(priv->curve);

		/* Edwards curves are not approved */
		not_approved = true;

		if (nonce != NULL) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		/* The point is in pub, while the private part (scalar) in priv. */

		if (size == 0 || priv->raw_priv.size != size) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		out->data = gnutls_malloc(size);
		if (out->data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		out->size = size;

		ret = edwards_curve_mul(algo, out->data, priv->raw_priv.data,
					pub->raw_pub.data);
		if (ret < 0)
			goto cleanup;

		if (_gnutls_mem_is_zero(out->data, out->size)) {
			gnutls_free(out->data);
			gnutls_assert();
			ret = GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
			goto cleanup;
		}
		break;
	}
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512: {
		struct ecc_scalar ecc_priv;
		struct ecc_point ecc_pub;
		const struct ecc_curve *curve;

		/* GOST curves are not approved */
		not_approved = true;

		out->data = NULL;

		curve = get_supported_gost_curve(priv->curve);
		if (curve == NULL) {
			gnutls_assert();
			ret = GNUTLS_E_ECC_UNSUPPORTED_CURVE;
			goto cleanup;
		}

		if (nonce == NULL) {
			gnutls_assert();
			ret = GNUTLS_E_INVALID_REQUEST;
			goto cleanup;
		}

		ret = _gost_params_to_pubkey(pub, &ecc_pub, curve);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = _gost_params_to_privkey(priv, &ecc_priv, curve);
		if (ret < 0) {
			ecc_point_clear(&ecc_pub);
			gnutls_assert();
			goto cleanup;
		}

		out->size = 2 * gnutls_ecc_curve_get_size(priv->curve);
		out->data = gnutls_malloc(out->size);
		if (out->data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto gost_cleanup;
		}

		gostdsa_vko(&ecc_priv, &ecc_pub, nonce->size, nonce->data,
			    out->data);

	gost_cleanup:
		ecc_point_clear(&ecc_pub);
		ecc_scalar_zclear(&ecc_priv);
		if (ret < 0)
			goto cleanup;
		break;
	}
#endif
	default:
		gnutls_assert();
		ret = GNUTLS_E_INTERNAL_ERROR;
		goto cleanup;
	}

	ret = 0;

cleanup:
	if (ret < 0) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);
	} else if (not_approved) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_NOT_APPROVED);
	} else {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_APPROVED);
	}

	return ret;
}

static int _wrap_nettle_pk_encaps(gnutls_pk_algorithm_t algo,
				  gnutls_datum_t *ciphertext,
				  gnutls_datum_t *shared_secret,
				  const gnutls_datum_t *pub)
{
	int ret;

	switch (algo) {
#ifdef HAVE_LIBOQS
	case GNUTLS_PK_EXP_KYBER768: {
		OQS_KEM *kem = NULL;
		OQS_STATUS rc;

		if (_gnutls_liboqs_ensure() < 0 ||
		    !GNUTLS_OQS_FUNC(OQS_KEM_alg_is_enabled)(
			    OQS_KEM_alg_kyber_768))
			return gnutls_assert_val(GNUTLS_E_UNKNOWN_PK_ALGORITHM);

		kem = GNUTLS_OQS_FUNC(OQS_KEM_new)(OQS_KEM_alg_kyber_768);
		if (kem == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		ciphertext->data = gnutls_malloc(kem->length_ciphertext);
		if (ciphertext->data == NULL) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}
		ciphertext->size = kem->length_ciphertext;

		shared_secret->data = gnutls_malloc(kem->length_shared_secret);
		if (shared_secret->data == NULL) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}
		shared_secret->size = kem->length_shared_secret;

		rc = GNUTLS_OQS_FUNC(OQS_KEM_encaps)(
			kem, ciphertext->data, shared_secret->data, pub->data);
		if (rc != OQS_SUCCESS) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
		ret = 0;
	} break;
#endif
	default:
		ret = gnutls_assert_val(GNUTLS_E_UNKNOWN_ALGORITHM);
		goto cleanup;
	}

cleanup:
	if (ret < 0) {
		gnutls_free(ciphertext->data);
		gnutls_free(shared_secret->data);
	}
	return ret;
}

static int _wrap_nettle_pk_decaps(gnutls_pk_algorithm_t algo,
				  gnutls_datum_t *shared_secret,
				  const gnutls_datum_t *ciphertext,
				  const gnutls_datum_t *priv)
{
	int ret;

	switch (algo) {
#ifdef HAVE_LIBOQS
	case GNUTLS_PK_EXP_KYBER768: {
		OQS_KEM *kem = NULL;
		OQS_STATUS rc;

		if (_gnutls_liboqs_ensure() < 0 ||
		    !GNUTLS_OQS_FUNC(OQS_KEM_alg_is_enabled)(
			    OQS_KEM_alg_kyber_768))
			return gnutls_assert_val(GNUTLS_E_UNKNOWN_PK_ALGORITHM);

		kem = GNUTLS_OQS_FUNC(OQS_KEM_new)(OQS_KEM_alg_kyber_768);
		if (kem == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		shared_secret->data = gnutls_malloc(kem->length_shared_secret);
		if (shared_secret->data == NULL) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}
		shared_secret->size = kem->length_shared_secret;

		rc = GNUTLS_OQS_FUNC(OQS_KEM_decaps)(
			kem, shared_secret->data, ciphertext->data, priv->data);
		if (rc != OQS_SUCCESS) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
		ret = 0;
	} break;
#endif
	default:
		ret = gnutls_assert_val(GNUTLS_E_UNKNOWN_ALGORITHM);
		goto cleanup;
	}
cleanup:
	if (ret < 0)
		gnutls_free(shared_secret->data);
	return ret;
}

/* This wraps nettle_rsa_encrypt so it returns ciphertext as a byte
 * array instead of a mpz_t value.  Returns 1 on success; 0 otherwise.
 */
static inline int _rsa_encrypt(const struct rsa_public_key *key, void *rnd_ctx,
			       nettle_random_func *rnd_func, size_t length,
			       const uint8_t *message, uint8_t *ciphertext)
{
	mpz_t p;
	int ret;

	mpz_init(p);

	ret = rsa_encrypt(key, rnd_ctx, rnd_func, length, message, p);

	if (ret == 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (_gnutls_mpi_bprint_size(p, ciphertext, key->size) < 0) {
		gnutls_assert();
		goto cleanup;
	}

cleanup:
	mpz_clear(p);
	return ret;
}

/* This wraps nettle_rsa_oaep_sha*_encrypt to parametrize the function
 * calls with a DIG argument.  Returns 1 on success; 0 otherwise.
 */
static inline int _rsa_oaep_encrypt(gnutls_digest_algorithm_t dig,
				    const struct rsa_public_key *pub,
				    void *rnd_ctx, nettle_random_func *rnd_func,
				    size_t label_length, const uint8_t *label,
				    size_t length, const uint8_t *message,
				    uint8_t *ciphertext)
{
	int (*encrypt_func)(const struct rsa_public_key *, void *,
			    nettle_random_func *, size_t, const uint8_t *,
			    size_t, const uint8_t *, uint8_t *);

	switch (dig) {
	case GNUTLS_DIG_SHA256:
		encrypt_func = rsa_oaep_sha256_encrypt;
		break;
	case GNUTLS_DIG_SHA384:
		encrypt_func = rsa_oaep_sha384_encrypt;
		break;
	case GNUTLS_DIG_SHA512:
		encrypt_func = rsa_oaep_sha512_encrypt;
		break;
	default:
		gnutls_assert();
		return 0;
	}

	return encrypt_func(pub, rnd_ctx, rnd_func, label_length, label, length,
			    message, ciphertext);
}

static int _wrap_nettle_pk_encrypt(gnutls_pk_algorithm_t algo,
				   gnutls_datum_t *ciphertext,
				   const gnutls_datum_t *plaintext,
				   const gnutls_pk_params_st *pk_params)
{
	int ret;
	bool not_approved = false;
	uint8_t *buf = NULL;

	FAIL_IF_LIB_ERROR;

	if (algo == GNUTLS_PK_RSA && pk_params->spki.pk == GNUTLS_PK_RSA_OAEP) {
		algo = GNUTLS_PK_RSA_OAEP;
	}

	switch (algo) {
	case GNUTLS_PK_RSA: {
		struct rsa_public_key pub;
		nettle_random_func *random_func;

		if (!_gnutls_config_is_rsa_pkcs1_encrypt_allowed()) {
			ret = gnutls_assert_val(
				GNUTLS_E_UNSUPPORTED_ENCRYPTION_ALGORITHM);
			goto cleanup;
		}

		/* RSA encryption with PKCS#1 v1.5 padding is not approved */
		not_approved = true;

		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST)
			random_func = rnd_nonce_func_fallback;
		else
			random_func = rnd_nonce_func;

		buf = gnutls_malloc(pub.size);
		if (!buf) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		ret = _rsa_encrypt(&pub, NULL, random_func, plaintext->size,
				   plaintext->data, buf);
		if (ret == 0 || HAVE_LIB_ERROR()) {
			ret = gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);
			goto cleanup;
		}

		ciphertext->data = buf;
		buf = NULL;
		ciphertext->size = pub.size;
		break;
	}
	case GNUTLS_PK_RSA_OAEP: {
		struct rsa_public_key pub;
		nettle_random_func *random_func;

		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST)
			random_func = rnd_nonce_func_fallback;
		else
			random_func = rnd_nonce_func;

		buf = gnutls_malloc(pub.size);
		if (!buf) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		ret = _rsa_oaep_encrypt(pk_params->spki.rsa_oaep_dig, &pub,
					NULL, random_func,
					pk_params->spki.rsa_oaep_label.size,
					pk_params->spki.rsa_oaep_label.data,
					plaintext->size, plaintext->data, buf);
		if (ret == 0 || HAVE_LIB_ERROR()) {
			ret = gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);
			goto cleanup;
		}
		ciphertext->data = buf;
		buf = NULL;
		ciphertext->size = pub.size;
		break;
	}
	default:
		gnutls_assert();
		ret = GNUTLS_E_INVALID_REQUEST;
		goto cleanup;
	}

	ret = 0;

cleanup:
	gnutls_free(buf);
	if (ret < 0) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);
	} else if (not_approved) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_NOT_APPROVED);
	} else {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_APPROVED);
	}

	FAIL_IF_LIB_ERROR;
	return ret;
}

/* This wraps nettle_rsa_decrypt_tr so it takes ciphertext as a byte
 * array instead of a mpz_t value.  Returns 1 on success; 0 otherwise.
 */
static inline int _rsa_decrypt_tr(const struct rsa_public_key *pub,
				  const struct rsa_private_key *key,
				  void *rnd_ctx, nettle_random_func *rnd_func,
				  size_t *length, uint8_t *message,
				  const uint8_t *ciphertext)
{
	bigint_t c;
	int ret;

	if (_gnutls_mpi_init_scan_nz(&c, ciphertext, pub->size) < 0) {
		gnutls_assert();
		return 0;
	}

	ret = rsa_decrypt_tr(pub, key, rnd_ctx, rnd_func, length, message, c);

	_gnutls_mpi_release(&c);

	return ret;
}

/* This wraps nettle_rsa_oaep_sha*_decrypt to parametrize the function
 * calls with a DIG argument.  Returns 1 on success; 0 otherwise.
 */
static inline int _rsa_oaep_decrypt(gnutls_digest_algorithm_t dig,
				    const struct rsa_public_key *pub,
				    const struct rsa_private_key *key,
				    void *rnd_ctx, nettle_random_func *rnd_func,
				    size_t label_length, const uint8_t *label,
				    size_t *length, uint8_t *message,
				    const uint8_t *ciphertext)
{
	int (*decrypt_func)(const struct rsa_public_key *,
			    const struct rsa_private_key *, void *,
			    nettle_random_func *, size_t, const uint8_t *,
			    size_t *, uint8_t *, const uint8_t *);

	switch (dig) {
	case GNUTLS_DIG_SHA256:
		decrypt_func = rsa_oaep_sha256_decrypt;
		break;
	case GNUTLS_DIG_SHA384:
		decrypt_func = rsa_oaep_sha384_decrypt;
		break;
	case GNUTLS_DIG_SHA512:
		decrypt_func = rsa_oaep_sha512_decrypt;
		break;
	default:
		gnutls_assert();
		return GNUTLS_E_UNKNOWN_ALGORITHM;
	}

	return decrypt_func(pub, key, rnd_ctx, rnd_func, label_length, label,
			    length, message, ciphertext);
}

static int _wrap_nettle_pk_decrypt(gnutls_pk_algorithm_t algo,
				   gnutls_datum_t *plaintext,
				   const gnutls_datum_t *ciphertext,
				   const gnutls_pk_params_st *pk_params)
{
	int ret;
	bool not_approved = false;
	uint8_t *buf = NULL;

	FAIL_IF_LIB_ERROR;

	if (algo == GNUTLS_PK_RSA && pk_params->spki.pk == GNUTLS_PK_RSA_OAEP) {
		algo = GNUTLS_PK_RSA_OAEP;
	}

	switch (algo) {
	case GNUTLS_PK_RSA: {
		struct rsa_private_key priv;
		struct rsa_public_key pub;
		size_t length;
		nettle_random_func *random_func;

		if (!_gnutls_config_is_rsa_pkcs1_encrypt_allowed()) {
			ret = gnutls_assert_val(
				GNUTLS_E_UNSUPPORTED_ENCRYPTION_ALGORITHM);
			goto cleanup;
		}

		/* RSA decryption with PKCS#1 v1.5 padding is not approved */
		not_approved = true;

		_rsa_params_to_privkey(pk_params, &priv);
		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		if (ciphertext->size != pub.size) {
			ret = gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);
			goto cleanup;
		}

		length = pub.size;
		buf = gnutls_malloc(length);
		if (!buf) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST)
			random_func = rnd_nonce_func_fallback;
		else
			random_func = rnd_nonce_func;
		ret = _rsa_decrypt_tr(&pub, &priv, NULL, random_func, &length,
				      buf, ciphertext->data);

		if (ret == 0 || HAVE_LIB_ERROR()) {
			ret = gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);
			goto cleanup;
		}

		plaintext->data = buf;
		buf = NULL;
		plaintext->size = length;
		break;
	}
	case GNUTLS_PK_RSA_OAEP: {
		struct rsa_private_key priv;
		struct rsa_public_key pub;
		size_t length;
		nettle_random_func *random_func;

		_rsa_params_to_privkey(pk_params, &priv);
		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		if (ciphertext->size != pub.size) {
			ret = gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);
			goto cleanup;
		}

		length = pub.size;
		buf = gnutls_malloc(length);
		if (!buf) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST)
			random_func = rnd_nonce_func_fallback;
		else
			random_func = rnd_nonce_func;
		ret = _rsa_oaep_decrypt(pk_params->spki.rsa_oaep_dig, &pub,
					&priv, NULL, random_func,
					pk_params->spki.rsa_oaep_label.size,
					pk_params->spki.rsa_oaep_label.data,
					&length, buf, ciphertext->data);

		if (ret == 0 || HAVE_LIB_ERROR()) {
			ret = gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);
			goto cleanup;
		}

		plaintext->data = buf;
		buf = NULL;
		plaintext->size = length;
		break;
	}
	default:
		gnutls_assert();
		ret = GNUTLS_E_INTERNAL_ERROR;
		goto cleanup;
	}

	ret = 0;

cleanup:
	gnutls_free(buf);
	if (ret < 0) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);
	} else if (not_approved) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_NOT_APPROVED);
	} else {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_APPROVED);
	}

	FAIL_IF_LIB_ERROR;
	return ret;
}

/* This wraps nettle_rsa_sec_decrypt so it takes ciphertext as a byte
 * array instead of a mpz_t value.  Returns 1 on success; 0 otherwise.
 */
static inline int _rsa_sec_decrypt(const struct rsa_public_key *pub,
				   const struct rsa_private_key *key,
				   void *rnd_ctx, nettle_random_func *rnd_func,
				   size_t length, uint8_t *message,
				   const uint8_t *ciphertext)
{
	bigint_t c;
	int ret;

	if (_gnutls_mpi_init_scan_nz(&c, ciphertext, pub->size) < 0) {
		gnutls_assert();
		return 0;
	}

	ret = rsa_sec_decrypt(pub, key, rnd_ctx, rnd_func, length, message, c);

	_gnutls_mpi_release(&c);

	return ret;
}

/* Note: we do not allocate in this function to avoid asymettric
 * unallocation (which creates a side channel) in case of failure
 * */
static int _wrap_nettle_pk_decrypt2(gnutls_pk_algorithm_t algo,
				    const gnutls_datum_t *ciphertext,
				    unsigned char *plaintext,
				    size_t plaintext_size,
				    const gnutls_pk_params_st *pk_params)
{
	struct rsa_private_key priv;
	struct rsa_public_key pub;
	uint32_t is_err;
	int ret;
	nettle_random_func *random_func;
	bool not_approved = false;

	FAIL_IF_LIB_ERROR;

	if (algo != GNUTLS_PK_RSA || plaintext == NULL) {
		ret = gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
		goto fail;
	}

	if (pk_params->spki.pk == GNUTLS_PK_RSA_OAEP) {
		algo = GNUTLS_PK_RSA_OAEP;
	}

	_rsa_params_to_privkey(pk_params, &priv);
	ret = _rsa_params_to_pubkey(pk_params, &pub);
	if (ret < 0) {
		gnutls_assert();
		goto fail;
	}

	if (ciphertext->size != pub.size) {
		ret = gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);
		goto fail;
	}

	if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST)
		random_func = rnd_nonce_func_fallback;
	else
		random_func = rnd_nonce_func;

	switch (algo) {
	case GNUTLS_PK_RSA:
		if (!_gnutls_config_is_rsa_pkcs1_encrypt_allowed()) {
			ret = gnutls_assert_val(
				GNUTLS_E_UNSUPPORTED_ENCRYPTION_ALGORITHM);
			goto fail;
		}

		/* RSA decryption with PKCS#1 v1.5 padding is not approved */
		not_approved = true;

		ret = _rsa_sec_decrypt(&pub, &priv, NULL, random_func,
				       plaintext_size, plaintext,
				       ciphertext->data);
		break;
	case GNUTLS_PK_RSA_OAEP:
		ret = _rsa_oaep_decrypt(pk_params->spki.rsa_oaep_dig, &pub,
					&priv, NULL, random_func,
					pk_params->spki.rsa_oaep_label.size,
					pk_params->spki.rsa_oaep_label.data,
					&plaintext_size, plaintext,
					ciphertext->data);
		break;
	default:
		gnutls_assert();
		ret = GNUTLS_E_INTERNAL_ERROR;
		goto fail;
	}

	/* The decrypt operation is infallible; treat the approved
	 * operation as complete at this point, regardless of any
	 * decryption failure detected below.
	 */
	_gnutls_switch_fips_state(not_approved ?
					  GNUTLS_FIPS140_OP_NOT_APPROVED :
					  GNUTLS_FIPS140_OP_APPROVED);

	/* after this point, any conditional on failure that cause differences
	 * in execution may create a timing or cache access pattern side
	 * channel that can be used as an oracle, so thread very carefully */

	/* Here HAVE_LIB_ERROR() should be fine as it doesn't have
	 * branches in it and returns a bool */
	is_err = HAVE_LIB_ERROR();
	/* if is_err != 0 */
	is_err = CONSTCHECK_NOT_EQUAL(is_err, 0);
	/* or ret == 0 */
	is_err |= CONSTCHECK_EQUAL(ret, 0);
	/* then return GNUTLS_E_DECRYPTION_FAILED */
	return (int)((is_err * UINT_MAX) & GNUTLS_E_DECRYPTION_FAILED);

fail:
	_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);

	return ret;
}

#define CHECK_INVALID_RSA_PSS_PARAMS(dig_size, salt_size, pub_size, err) \
	if (unlikely(dig_size + salt_size + 2 > pub_size))               \
	return gnutls_assert_val(err)

static int _rsa_pss_sign_digest_tr(gnutls_digest_algorithm_t dig,
				   const struct rsa_public_key *pub,
				   const struct rsa_private_key *priv,
				   void *rnd_ctx, nettle_random_func *rnd_func,
				   size_t salt_size, const uint8_t *digest,
				   mpz_t s)
{
	int (*sign_func)(const struct rsa_public_key *,
			 const struct rsa_private_key *, void *,
			 nettle_random_func *, size_t, const uint8_t *,
			 const uint8_t *, mpz_t);
	uint8_t *salt = NULL;
	size_t hash_size;
	int ret;

	switch (dig) {
	case GNUTLS_DIG_SHA256:
		sign_func = rsa_pss_sha256_sign_digest_tr;
		hash_size = 32;
		break;
	case GNUTLS_DIG_SHA384:
		sign_func = rsa_pss_sha384_sign_digest_tr;
		hash_size = 48;
		break;
	case GNUTLS_DIG_SHA512:
		sign_func = rsa_pss_sha512_sign_digest_tr;
		hash_size = 64;
		break;
	default:
		gnutls_assert();
		return GNUTLS_E_UNKNOWN_ALGORITHM;
	}

	/* This is also checked in pss_encode_mgf1, but error out earlier.  */
	CHECK_INVALID_RSA_PSS_PARAMS(hash_size, salt_size, pub->size,
				     GNUTLS_E_PK_INVALID_PUBKEY_PARAMS);

	if (salt_size > 0) {
		salt = gnutls_malloc(salt_size);
		if (salt == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		ret = gnutls_rnd(GNUTLS_RND_NONCE, salt, salt_size);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}

	ret = sign_func(pub, priv, rnd_ctx, rnd_func, salt_size, salt, digest,
			s);
	if (ret == 0) {
		gnutls_assert();
		ret = GNUTLS_E_PK_SIGN_FAILED;
	} else
		ret = 0;

cleanup:
	gnutls_free(salt);
	return ret;
}

static inline gnutls_ecc_curve_t get_eddsa_curve(gnutls_pk_algorithm_t algo)
{
	switch (algo) {
	case GNUTLS_PK_EDDSA_ED25519:
		return GNUTLS_ECC_CURVE_ED25519;
	case GNUTLS_PK_EDDSA_ED448:
		return GNUTLS_ECC_CURVE_ED448;
	default:
		return gnutls_assert_val(GNUTLS_ECC_CURVE_INVALID);
	}
}

static inline gnutls_ecc_curve_t get_ecdh_curve(gnutls_pk_algorithm_t algo)
{
	switch (algo) {
	case GNUTLS_PK_ECDH_X25519:
		return GNUTLS_ECC_CURVE_X25519;
	case GNUTLS_PK_ECDH_X448:
		return GNUTLS_ECC_CURVE_X448;
	default:
		return gnutls_assert_val(GNUTLS_ECC_CURVE_INVALID);
	}
}

static inline int eddsa_sign(gnutls_pk_algorithm_t algo, const uint8_t *pub,
			     const uint8_t *priv, size_t length,
			     const uint8_t *msg, uint8_t *signature)
{
	switch (algo) {
	case GNUTLS_PK_EDDSA_ED25519:
		ed25519_sha512_sign(pub, priv, length, msg, signature);
		return 0;
	case GNUTLS_PK_EDDSA_ED448:
		ed448_shake256_sign(pub, priv, length, msg, signature);
		return 0;
	default:
		return gnutls_assert_val(
			GNUTLS_E_UNSUPPORTED_SIGNATURE_ALGORITHM);
	}
}

/* This is the lower-level part of privkey_sign_raw_data().
 *
 * It accepts data in the appropriate hash form, i.e., DigestInfo
 * for PK_RSA, hash for PK_ECDSA, PK_DSA, PK_RSA_PSS, and raw data
 * for Ed25519 and Ed448.
 *
 * in case of EC/DSA, signed data are encoded into r,s values
 */
static int _wrap_nettle_pk_sign(gnutls_pk_algorithm_t algo,
				gnutls_datum_t *signature,
				const gnutls_datum_t *vdata,
				const gnutls_pk_params_st *pk_params,
				const gnutls_x509_spki_st *sign_params)
{
	int ret;
	unsigned int hash_len;
	const mac_entry_st *me;
	bool not_approved = false;

	FAIL_IF_LIB_ERROR;

	/* check if the curve relates to the algorithm used */
	if (IS_EC(algo) && gnutls_ecc_curve_get_pk(pk_params->curve) != algo) {
		ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
		goto cleanup;
	}

	/* deterministic ECDSA/DSA is prohibited under FIPS except in
	 * the selftests */
	if ((algo == GNUTLS_PK_DSA || algo == GNUTLS_PK_ECDSA) &&
	    (sign_params->flags & GNUTLS_PK_FLAG_REPRODUCIBLE) &&
	    _gnutls_fips_mode_enabled() &&
	    _gnutls_get_lib_state() != LIB_STATE_SELFTEST) {
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		goto cleanup;
	}

	switch (algo) {
	case GNUTLS_PK_EDDSA_ED25519: /* we do EdDSA */
	case GNUTLS_PK_EDDSA_ED448: {
		const gnutls_ecc_curve_entry_st *e;

		/* EdDSA is not approved yet */
		not_approved = true;

		if (unlikely(get_eddsa_curve(algo) != pk_params->curve)) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		e = _gnutls_ecc_curve_get_params(pk_params->curve);
		if (e == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		signature->data = gnutls_malloc(e->sig_size);
		if (signature->data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		signature->size = e->sig_size;

		if (pk_params->raw_pub.size != e->size ||
		    pk_params->raw_priv.size != e->size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIGN_FAILED);
			goto cleanup;
		}

		ret = eddsa_sign(algo, pk_params->raw_pub.data,
				 pk_params->raw_priv.data, vdata->size,
				 vdata->data, signature->data);
		if (ret < 0)
			goto cleanup;

		break;
	}
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512: {
		struct ecc_scalar priv;
		struct dsa_signature sig;
		const struct ecc_curve *curve;

		/* GOSTDSA is not approved */
		not_approved = true;

		curve = get_supported_gost_curve(pk_params->curve);
		if (curve == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		ret = _ecc_params_to_privkey(pk_params, &priv, curve);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* This call will return a valid MAC entry and
			 * getters will check that is not null anyway. */
		me = hash_to_entry(_gnutls_gost_digest(pk_params->algo));
		if (_gnutls_mac_get_algo_len(me) != vdata->size) {
			_gnutls_debug_log(
				"Security level of algorithm requires hash %s(%zd)\n",
				_gnutls_mac_get_name(me),
				_gnutls_mac_get_algo_len(me));
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		dsa_signature_init(&sig);

		gostdsa_sign(&priv, NULL, rnd_tmpkey_func, vdata->size,
			     vdata->data, &sig);

		ret = _gnutls_encode_gost_rs(signature, &sig.r, &sig.s,
					     (ecc_bit_size(curve) + 7) / 8);

		dsa_signature_clear(&sig);
		ecc_scalar_zclear(&priv);

		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		break;
	}
#endif
	case GNUTLS_PK_ECDSA: /* we do ECDSA */
	{
		struct ecc_scalar priv;
		struct dsa_signature sig;
		int curve_id = pk_params->curve;
		const struct ecc_curve *curve;
		mpz_t q;
		/* 521-bit elliptic curve generator at maximum */
		uint8_t buf[(521 + 7) / 8];
		gnutls_datum_t k = { NULL, 0 };
		void *random_ctx;
		nettle_random_func *random_func;

		curve = get_supported_nist_curve(curve_id);
		if (curve == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		/* P-192 is not supported in FIPS 140-3 */
		if (curve_id == GNUTLS_ECC_CURVE_SECP192R1) {
			not_approved = true;
		}

		ret = _ecc_params_to_privkey(pk_params, &priv, curve);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		dsa_signature_init(&sig);

		me = _gnutls_dsa_q_to_hash(pk_params, &hash_len);

		if (hash_len > vdata->size) {
			gnutls_assert();
			_gnutls_debug_log(
				"Security level of algorithm requires hash %s(%d) or better\n",
				_gnutls_mac_get_name(me), hash_len);
			hash_len = vdata->size;
		}

		/* Only SHA-2 is allowed in FIPS 140-3 */
		switch (DIG_TO_MAC(sign_params->dsa_dig)) {
		case GNUTLS_MAC_SHA256:
		case GNUTLS_MAC_SHA384:
		case GNUTLS_MAC_SHA512:
		case GNUTLS_MAC_SHA224:
			break;
		default:
			not_approved = true;
		}

		mpz_init(q);

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST ||
		    (sign_params->flags & GNUTLS_PK_FLAG_REPRODUCIBLE)) {
			mp_limb_t h[DSA_COMPUTE_K_ITCH];

			ret = _gnutls_ecc_curve_to_dsa_q(q, curve_id);
			if (ret < 0)
				goto ecdsa_cleanup;

			ret = _gnutls_dsa_compute_k(
				h, mpz_limbs_read(q), priv.p,
				ecc_size(priv.ecc), ecc_bit_size(priv.ecc),
				DIG_TO_MAC(sign_params->dsa_dig), vdata->data,
				vdata->size);
			if (ret < 0)
				goto ecdsa_cleanup;

			k.data = buf;
			k.size = (ecc_bit_size(priv.ecc) + 7) / 8;

			_gnutls_ecdsa_compute_k_finish(k.data, k.size, h,
						       ecc_size(priv.ecc));

			random_ctx = &k;
			random_func = rnd_datum_func;
		} else {
			random_ctx = NULL;
			random_func = rnd_nonce_func;
		}
		ecdsa_sign(&priv, random_ctx, random_func, hash_len,
			   vdata->data, &sig);

		/* prevent memory leaks */
		if (HAVE_LIB_ERROR()) {
			ret = GNUTLS_E_LIB_IN_ERROR_STATE;
			goto ecdsa_cleanup;
		}

		ret = _gnutls_encode_ber_rs(signature, &sig.r, &sig.s);

	ecdsa_cleanup:
		dsa_signature_clear(&sig);
		ecc_scalar_zclear(&priv);
		mpz_clear(q);

		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		break;
	}
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA: {
		struct dsa_params pub;
		bigint_t priv;
		struct dsa_signature sig;
		/* 512-bit DSA subgroup at maximum */
		uint8_t buf[(512 + 7) / 8];
		gnutls_datum_t k = { NULL, 0 };
		void *random_ctx;
		nettle_random_func *random_func;

		/* DSA is currently being defined as sunset with the
			 * current draft of FIPS 186-5 */
		not_approved = true;

		memset(&priv, 0, sizeof(priv));
		memset(&pub, 0, sizeof(pub));
		_dsa_params_get(pk_params, &pub);

		priv = pk_params->params[DSA_X];

		dsa_signature_init(&sig);

		me = _gnutls_dsa_q_to_hash(pk_params, &hash_len);

		if (hash_len > vdata->size) {
			gnutls_assert();
			_gnutls_debug_log(
				"Security level of algorithm requires hash %s(%d) or better (have: %d)\n",
				_gnutls_mac_get_name(me), hash_len,
				(int)vdata->size);
			hash_len = vdata->size;
		}

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST ||
		    (sign_params->flags & GNUTLS_PK_FLAG_REPRODUCIBLE)) {
			mp_limb_t h[DSA_COMPUTE_K_ITCH];

			ret = _gnutls_dsa_compute_k(
				h, mpz_limbs_read(pub.q),
				mpz_limbs_read(TOMPZ(priv)), mpz_size(pub.q),
				mpz_sizeinbase(pub.q, 2),
				DIG_TO_MAC(sign_params->dsa_dig), vdata->data,
				vdata->size);
			if (ret < 0)
				goto dsa_fail;

			k.data = buf;
			k.size = (mpz_sizeinbase(pub.q, 2) + 7) / 8;

			_gnutls_dsa_compute_k_finish(k.data, k.size, h,
						     mpz_size(pub.q));

			random_ctx = &k;
			random_func = rnd_datum_func;
		} else {
			random_ctx = NULL;
			random_func = rnd_nonce_func;
		}
		ret = dsa_sign(&pub, TOMPZ(priv), random_ctx, random_func,
			       hash_len, vdata->data, &sig);
		if (ret == 0 || HAVE_LIB_ERROR()) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIGN_FAILED;
			goto dsa_fail;
		}

		ret = _gnutls_encode_ber_rs(signature, &sig.r, &sig.s);

	dsa_fail:
		dsa_signature_clear(&sig);

		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		break;
	}
#endif
	case GNUTLS_PK_RSA: {
		struct rsa_private_key priv;
		struct rsa_public_key pub;
		nettle_random_func *random_func;
		mpz_t s;

		_rsa_params_to_privkey(pk_params, &priv);

		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* RSA modulus size should be 2048-bit or larger in FIPS
			 * 140-3.  In addition to this, only SHA-2 is allowed
			 * for SigGen; it is checked in pk_prepare_hash lib/pk.c
			 */
		if (unlikely(mpz_sizeinbase(pub.n, 2) < 2048)) {
			not_approved = true;
		}

		mpz_init(s);

		if (_gnutls_get_lib_state() == LIB_STATE_SELFTEST)
			random_func = rnd_nonce_func_fallback;
		else
			random_func = rnd_nonce_func;
		ret = rsa_pkcs1_sign_tr(&pub, &priv, NULL, random_func,
					vdata->size, vdata->data, s);
		if (ret == 0 || HAVE_LIB_ERROR()) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIGN_FAILED;
			goto rsa_fail;
		}

		ret = _gnutls_mpi_dprint_size(s, signature, pub.size);

	rsa_fail:
		mpz_clear(s);

		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		break;
	}
	case GNUTLS_PK_RSA_PSS: {
		struct rsa_private_key priv;
		struct rsa_public_key pub;
		mpz_t s;

		_rsa_params_to_privkey(pk_params, &priv);

		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* RSA modulus size should be 2048-bit or larger in FIPS
			 * 140-3.  In addition to this, only SHA-2 is allowed
			 * for SigGen; however, Nettle only support SHA256,
			 * SHA384, and SHA512 for RSA-PSS (see
			 * _rsa_pss_sign_digest_tr in this file for details).
			 */
		if (unlikely(mpz_sizeinbase(pub.n, 2) < 2048)) {
			not_approved = true;
		}

		mpz_init(s);

		me = hash_to_entry(sign_params->rsa_pss_dig);

		/* According to FIPS 186-5 5.4, the salt length must be
			 * in the range between 0 and the hash length inclusive.
			 */
		if (sign_params->salt_size > _gnutls_mac_get_algo_len(me)) {
			not_approved = true;
		}

		ret = _rsa_pss_sign_digest_tr(sign_params->rsa_pss_dig, &pub,
					      &priv, NULL, rnd_nonce_func,
					      sign_params->salt_size,
					      vdata->data, s);
		if (ret < 0) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIGN_FAILED;
			goto rsa_pss_fail;
		}

		ret = _gnutls_mpi_dprint_size(s, signature, pub.size);

	rsa_pss_fail:
		mpz_clear(s);

		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		break;
	}
	default:
		gnutls_assert();
		ret = GNUTLS_E_INTERNAL_ERROR;
		goto cleanup;
	}

	ret = 0;

cleanup:
	if (ret < 0) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);
	} else if (not_approved) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_NOT_APPROVED);
	} else {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_APPROVED);
	}

	FAIL_IF_LIB_ERROR;
	return ret;
}

static int _rsa_pss_verify_digest(gnutls_digest_algorithm_t dig,
				  const struct rsa_public_key *pub,
				  size_t salt_size, const uint8_t *digest,
				  size_t digest_size, const mpz_t s)
{
	int (*verify_func)(const struct rsa_public_key *, size_t,
			   const uint8_t *, const mpz_t);
	size_t hash_size;

	switch (dig) {
	case GNUTLS_DIG_SHA256:
		verify_func = rsa_pss_sha256_verify_digest;
		hash_size = 32;
		break;
	case GNUTLS_DIG_SHA384:
		verify_func = rsa_pss_sha384_verify_digest;
		hash_size = 48;
		break;
	case GNUTLS_DIG_SHA512:
		verify_func = rsa_pss_sha512_verify_digest;
		hash_size = 64;
		break;
	default:
		gnutls_assert();
		return 0;
	}

	if (digest_size != hash_size)
		return gnutls_assert_val(0);

	CHECK_INVALID_RSA_PSS_PARAMS(hash_size, salt_size, pub->size, 0);

	return verify_func(pub, salt_size, digest, s);
}

static inline int eddsa_verify(gnutls_pk_algorithm_t algo, const uint8_t *pub,
			       size_t length, const uint8_t *msg,
			       const uint8_t *signature)
{
	int ret;

	switch (algo) {
	case GNUTLS_PK_EDDSA_ED25519:
		ret = ed25519_sha512_verify(pub, length, msg, signature);
		if (ret == 0)
			return gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
		return 0;
	case GNUTLS_PK_EDDSA_ED448:
		ret = ed448_shake256_verify(pub, length, msg, signature);
		if (ret == 0)
			return gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
		return 0;
	default:
		return gnutls_assert_val(
			GNUTLS_E_UNSUPPORTED_SIGNATURE_ALGORITHM);
	}
}

static int _wrap_nettle_pk_verify(gnutls_pk_algorithm_t algo,
				  const gnutls_datum_t *vdata,
				  const gnutls_datum_t *signature,
				  const gnutls_pk_params_st *pk_params,
				  const gnutls_x509_spki_st *sign_params)
{
	int ret;
	unsigned int hash_len;
	bigint_t tmp[2] = { NULL, NULL };
	bool not_approved = false;

	FAIL_IF_LIB_ERROR;

	/* check if the curve relates to the algorithm used */
	if (IS_EC(algo) && gnutls_ecc_curve_get_pk(pk_params->curve) != algo) {
		ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
		goto cleanup;
	}

	switch (algo) {
	case GNUTLS_PK_EDDSA_ED25519: /* we do EdDSA */
	case GNUTLS_PK_EDDSA_ED448: {
		const gnutls_ecc_curve_entry_st *e;

		/* EdDSA is not approved yet */
		not_approved = true;

		if (unlikely(get_eddsa_curve(algo) != pk_params->curve)) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		e = _gnutls_ecc_curve_get_params(pk_params->curve);
		if (e == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		if (signature->size != e->sig_size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
			goto cleanup;
		}

		if (pk_params->raw_pub.size != e->size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIGN_FAILED);
			goto cleanup;
		}

		ret = eddsa_verify(algo, pk_params->raw_pub.data, vdata->size,
				   vdata->data, signature->data);
		break;
	}
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512: {
		struct ecc_point pub;
		struct dsa_signature sig;
		const struct ecc_curve *curve;
		const mac_entry_st *me;

		/* GOSTDSA is not approved */
		not_approved = true;

		curve = get_supported_gost_curve(pk_params->curve);
		if (curve == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		/* This call will return a valid MAC entry and
			 * getters will check that is not null anyway. */
		me = hash_to_entry(_gnutls_gost_digest(pk_params->algo));
		if (_gnutls_mac_get_algo_len(me) != vdata->size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
			goto cleanup;
		}

		ret = _gnutls_decode_gost_rs(signature, &tmp[0], &tmp[1]);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = _gost_params_to_pubkey(pk_params, &pub, curve);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		memcpy(sig.r, tmp[0], SIZEOF_MPZT);
		memcpy(sig.s, tmp[1], SIZEOF_MPZT);

		ret = gostdsa_verify(&pub, vdata->size, vdata->data, &sig);
		if (ret == 0) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIG_VERIFY_FAILED;
		} else
			ret = 0;

		ecc_point_clear(&pub);
		break;
	}
#endif
	case GNUTLS_PK_ECDSA: /* ECDSA */
	{
		struct ecc_point pub;
		struct dsa_signature sig;
		int curve_id = pk_params->curve;
		const struct ecc_curve *curve;

		curve = get_supported_nist_curve(curve_id);
		if (curve == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		ret = _gnutls_decode_ber_rs(signature, &tmp[0], &tmp[1]);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = _ecc_params_to_pubkey(pk_params, &pub, curve);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		memcpy(sig.r, tmp[0], SIZEOF_MPZT);
		memcpy(sig.s, tmp[1], SIZEOF_MPZT);

		(void)_gnutls_dsa_q_to_hash(pk_params, &hash_len);

		if (hash_len > vdata->size)
			hash_len = vdata->size;

		/* SHA-1 is allowed for SigVer in FIPS 140-3 in legacy
			 * mode */
		switch (DIG_TO_MAC(sign_params->dsa_dig)) {
		case GNUTLS_MAC_SHA1:
		case GNUTLS_MAC_SHA256:
		case GNUTLS_MAC_SHA384:
		case GNUTLS_MAC_SHA512:
		case GNUTLS_MAC_SHA224:
			break;
		default:
			not_approved = true;
		}

		ret = ecdsa_verify(&pub, hash_len, vdata->data, &sig);
		if (ret == 0) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIG_VERIFY_FAILED;
		} else
			ret = 0;

		ecc_point_clear(&pub);
		break;
	}
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA: {
		struct dsa_params pub;
		struct dsa_signature sig;
		bigint_t y;

		/* DSA is currently being defined as sunset with the
			 * current draft of FIPS 186-5 */
		not_approved = true;

		ret = _gnutls_decode_ber_rs(signature, &tmp[0], &tmp[1]);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		memset(&pub, 0, sizeof(pub));
		_dsa_params_get(pk_params, &pub);
		y = pk_params->params[DSA_Y];

		memcpy(sig.r, tmp[0], SIZEOF_MPZT);
		memcpy(sig.s, tmp[1], SIZEOF_MPZT);

		_gnutls_dsa_q_to_hash(pk_params, &hash_len);

		if (hash_len > vdata->size)
			hash_len = vdata->size;

		ret = dsa_verify(&pub, TOMPZ(y), hash_len, vdata->data, &sig);
		if (ret == 0) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIG_VERIFY_FAILED;
		} else
			ret = 0;

		break;
	}
#endif
	case GNUTLS_PK_RSA: {
		struct rsa_public_key pub;
		size_t bits;

		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		bits = mpz_sizeinbase(pub.n, 2);

		/* In FIPS 140-3, RSA key size should be larger than
			 * 2048-bit or one of the known lengths (1024, 1280,
			 * 1536, 1792; i.e., multiple of 256-bits).
			 *
			 * In addition to this, only SHA-1 and SHA-2 are allowed
			 * for SigVer; it is checked in _pkcs1_rsa_verify_sig in
			 * lib/pubkey.c.
			 */
		if (unlikely(bits < 2048 && bits != 1024 && bits != 1280 &&
			     bits != 1536 && bits != 1792)) {
			not_approved = true;
		}

		if (signature->size != pub.size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
			goto cleanup;
		}

		ret = _gnutls_mpi_init_scan_nz(&tmp[0], signature->data,
					       signature->size);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = rsa_pkcs1_verify(&pub, vdata->size, vdata->data,
				       TOMPZ(tmp[0]));
		if (ret == 0)
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
		else
			ret = 0;

		break;
	}
	case GNUTLS_PK_RSA_PSS: {
		struct rsa_public_key pub;

		if ((sign_params->flags &
		     GNUTLS_PK_FLAG_RSA_PSS_FIXED_SALT_LENGTH) &&
		    sign_params->salt_size != vdata->size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
			goto cleanup;
		}

		ret = _rsa_params_to_pubkey(pk_params, &pub);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* RSA modulus size should be 2048-bit or larger in FIPS
			 * 140-3.  In addition to this, only SHA-1 and SHA-2 are
			 * allowed for SigVer, while Nettle only supports
			 * SHA256, SHA384, and SHA512 for RSA-PSS (see
			 * _rsa_pss_verify_digest in this file for the details).
			 */
		if (unlikely(mpz_sizeinbase(pub.n, 2) < 2048)) {
			not_approved = true;
		}

		if (signature->size != pub.size) {
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
			goto cleanup;
		}

		ret = _gnutls_mpi_init_scan_nz(&tmp[0], signature->data,
					       signature->size);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = _rsa_pss_verify_digest(sign_params->rsa_pss_dig, &pub,
					     sign_params->salt_size,
					     vdata->data, vdata->size,
					     TOMPZ(tmp[0]));
		if (ret == 0)
			ret = gnutls_assert_val(GNUTLS_E_PK_SIG_VERIFY_FAILED);
		else
			ret = 0;

		break;
	}
	default:
		gnutls_assert();
		ret = GNUTLS_E_INTERNAL_ERROR;
		goto cleanup;
	}

cleanup:
	if (ret < 0) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);
	} else if (not_approved) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_NOT_APPROVED);
	} else {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_APPROVED);
	}

	_gnutls_mpi_release(&tmp[0]);
	_gnutls_mpi_release(&tmp[1]);
	FAIL_IF_LIB_ERROR;
	return ret;
}

static inline const struct ecc_curve *get_supported_nist_curve(int curve)
{
	switch (curve) {
#ifdef ENABLE_NON_SUITEB_CURVES
	case GNUTLS_ECC_CURVE_SECP192R1:
		return nettle_get_secp_192r1();
	case GNUTLS_ECC_CURVE_SECP224R1:
		return nettle_get_secp_224r1();
#endif
	case GNUTLS_ECC_CURVE_SECP256R1:
		return nettle_get_secp_256r1();
	case GNUTLS_ECC_CURVE_SECP384R1:
		return nettle_get_secp_384r1();
	case GNUTLS_ECC_CURVE_SECP521R1:
		return nettle_get_secp_521r1();
	default:
		return NULL;
	}
}

static inline const char *get_supported_nist_curve_order(int curve)
{
	static const struct {
		int curve;
		const char *order;
	} orders[] = {
#ifdef ENABLE_NON_SUITEB_CURVES
		{ GNUTLS_ECC_CURVE_SECP192R1, "ffffffffffffffffffffffff99def836"
					      "146bc9b1b4d22831" },
		{ GNUTLS_ECC_CURVE_SECP224R1, "ffffffffffffffffffffffffffff16a2"
					      "e0b8f03e13dd29455c5c2a3d" },
#endif
		{ GNUTLS_ECC_CURVE_SECP256R1,
		  "ffffffff00000000ffffffffffffffff"
		  "bce6faada7179e84f3b9cac2fc632551" },
		{ GNUTLS_ECC_CURVE_SECP384R1,
		  "ffffffffffffffffffffffffffffffff"
		  "ffffffffffffffffc7634d81f4372ddf"
		  "581a0db248b0a77aecec196accc52973" },
		{ GNUTLS_ECC_CURVE_SECP521R1, "1fffffffffffffffffffffffffffffff"
					      "ffffffffffffffffffffffffffffffff"
					      "ffa51868783bf2f966b7fcc0148f709a"
					      "5d03bb5c9b8899c47aebb6fb71e91386"
					      "409" },
	};
	size_t i;

	for (i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
		if (orders[i].curve == curve)
			return orders[i].order;
	}
	return NULL;
}

static inline const char *get_supported_nist_curve_modulus(int curve)
{
	static const struct {
		int curve;
		const char *order;
	} orders[] = {
#ifdef ENABLE_NON_SUITEB_CURVES
		{ GNUTLS_ECC_CURVE_SECP192R1, "fffffffffffffffffffffffffffffffe"
					      "ffffffffffffffff" },
		{ GNUTLS_ECC_CURVE_SECP224R1, "ffffffffffffffffffffffffffffffff"
					      "000000000000000000000001" },
#endif
		{ GNUTLS_ECC_CURVE_SECP256R1,
		  "ffffffff000000010000000000000000"
		  "00000000ffffffffffffffffffffffff" },
		{ GNUTLS_ECC_CURVE_SECP384R1,
		  "ffffffffffffffffffffffffffffffff"
		  "fffffffffffffffffffffffffffffffe"
		  "ffffffff0000000000000000ffffffff" },
		{ GNUTLS_ECC_CURVE_SECP521R1,
		  "1ff"
		  "ffffffffffffffffffffffffffffffff"
		  "ffffffffffffffffffffffffffffffff"
		  "ffffffffffffffffffffffffffffffff"
		  "ffffffffffffffffffffffffffffffff" },
	};
	size_t i;

	for (i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
		if (orders[i].curve == curve)
			return orders[i].order;
	}
	return NULL;
}

static inline const struct ecc_curve *get_supported_gost_curve(int curve)
{
	switch (curve) {
#if ENABLE_GOST
	case GNUTLS_ECC_CURVE_GOST256CPA:
	case GNUTLS_ECC_CURVE_GOST256CPXA:
	case GNUTLS_ECC_CURVE_GOST256B:
		return nettle_get_gost_gc256b();
	case GNUTLS_ECC_CURVE_GOST512A:
		return nettle_get_gost_gc512a();
#endif
	default:
		return NULL;
	}
}

static int _wrap_nettle_pk_curve_exists(gnutls_ecc_curve_t curve)
{
	switch (curve) {
	case GNUTLS_ECC_CURVE_ED25519:
	case GNUTLS_ECC_CURVE_X25519:
	case GNUTLS_ECC_CURVE_ED448:
	case GNUTLS_ECC_CURVE_X448:
		return 1;
	default:
		return ((get_supported_nist_curve(curve) != NULL ||
			 get_supported_gost_curve(curve) != NULL) ?
				1 :
				0);
	}
}

static int _wrap_nettle_pk_exists(gnutls_pk_algorithm_t pk)
{
	switch (pk) {
	case GNUTLS_PK_RSA:
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA:
#endif
	case GNUTLS_PK_DH:
	case GNUTLS_PK_ECDSA:
	case GNUTLS_PK_ECDH_X25519:
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_RSA_OAEP:
	case GNUTLS_PK_EDDSA_ED25519:
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512:
#endif
	case GNUTLS_PK_ECDH_X448:
	case GNUTLS_PK_EDDSA_ED448:
		return 1;
#ifdef HAVE_LIBOQS
	case GNUTLS_PK_EXP_KYBER768:
		return _gnutls_liboqs_ensure() == 0 &&
		       GNUTLS_OQS_FUNC(OQS_KEM_alg_is_enabled)(
			       OQS_KEM_alg_kyber_768);
#endif
	default:
		return 0;
	}
}

static int _wrap_nettle_pk_sign_exists(gnutls_sign_algorithm_t sign)
{
	switch (sign) {
	case GNUTLS_SIGN_RSA_SHA1:
#ifdef ENABLE_DSA
	case GNUTLS_SIGN_DSA_SHA1:
#endif
	case GNUTLS_SIGN_RSA_MD5:
	case GNUTLS_SIGN_RSA_MD2:
	case GNUTLS_SIGN_RSA_RMD160:
	case GNUTLS_SIGN_RSA_SHA256:
	case GNUTLS_SIGN_RSA_SHA384:
	case GNUTLS_SIGN_RSA_SHA512:
	case GNUTLS_SIGN_RSA_SHA224:
#ifdef ENABLE_DSA
	case GNUTLS_SIGN_DSA_SHA224:
	case GNUTLS_SIGN_DSA_SHA256:
#endif
	case GNUTLS_SIGN_ECDSA_SHA1:
	case GNUTLS_SIGN_ECDSA_SHA224:
	case GNUTLS_SIGN_ECDSA_SHA256:
	case GNUTLS_SIGN_ECDSA_SHA384:
	case GNUTLS_SIGN_ECDSA_SHA512:
#ifdef ENABLE_DSA
	case GNUTLS_SIGN_DSA_SHA384:
	case GNUTLS_SIGN_DSA_SHA512:
#endif
	case GNUTLS_SIGN_ECDSA_SHA3_224:
	case GNUTLS_SIGN_ECDSA_SHA3_256:
	case GNUTLS_SIGN_ECDSA_SHA3_384:
	case GNUTLS_SIGN_ECDSA_SHA3_512:

#ifdef ENABLE_DSA
	case GNUTLS_SIGN_DSA_SHA3_224:
	case GNUTLS_SIGN_DSA_SHA3_256:
	case GNUTLS_SIGN_DSA_SHA3_384:
	case GNUTLS_SIGN_DSA_SHA3_512:
#endif
	case GNUTLS_SIGN_RSA_SHA3_224:
	case GNUTLS_SIGN_RSA_SHA3_256:
	case GNUTLS_SIGN_RSA_SHA3_384:
	case GNUTLS_SIGN_RSA_SHA3_512:

	case GNUTLS_SIGN_RSA_PSS_SHA256:
	case GNUTLS_SIGN_RSA_PSS_SHA384:
	case GNUTLS_SIGN_RSA_PSS_SHA512:
	case GNUTLS_SIGN_EDDSA_ED25519:
	case GNUTLS_SIGN_RSA_RAW:

	case GNUTLS_SIGN_ECDSA_SECP256R1_SHA256:
	case GNUTLS_SIGN_ECDSA_SECP384R1_SHA384:
	case GNUTLS_SIGN_ECDSA_SECP521R1_SHA512:

	case GNUTLS_SIGN_RSA_PSS_RSAE_SHA256:
	case GNUTLS_SIGN_RSA_PSS_RSAE_SHA384:
	case GNUTLS_SIGN_RSA_PSS_RSAE_SHA512:

#if ENABLE_GOST
	case GNUTLS_SIGN_GOST_94:
	case GNUTLS_SIGN_GOST_256:
	case GNUTLS_SIGN_GOST_512:
#endif
	case GNUTLS_SIGN_EDDSA_ED448:
		return 1;
	default:
		return 0;
	}
}

/* Generates algorithm's parameters. That is:
 *  For DSA: p, q, and g are generated.
 *  For RSA: nothing
 *  For ECDSA/EDDSA: nothing
 */
static int wrap_nettle_pk_generate_params(gnutls_pk_algorithm_t algo,
					  unsigned int level /*bits or curve */,
					  gnutls_pk_params_st *params)
{
	int ret;
	unsigned int i, q_bits;

	FAIL_IF_LIB_ERROR;

	params->algo = algo;

	switch (algo) {
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA:
#endif
	case GNUTLS_PK_DH: {
		struct dsa_params pub;
		struct dss_params_validation_seeds cert;
		unsigned index;

		dsa_params_init(&pub);

		if (GNUTLS_BITS_HAVE_SUBGROUP(level)) {
			q_bits = GNUTLS_BITS_TO_SUBGROUP(level);
			level = GNUTLS_BITS_TO_GROUP(level);
		} else {
			q_bits = _gnutls_pk_bits_to_subgroup_bits(level);
		}

		if (q_bits == 0)
			return gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);

		if (_gnutls_fips_mode_enabled() != 0 ||
		    params->pkflags & GNUTLS_PK_FLAG_PROVABLE) {
			if (algo == GNUTLS_PK_DSA)
				index = 1;
			else
				index = 2;

			if (params->palgo != 0 &&
			    params->palgo != GNUTLS_DIG_SHA384) {
				ret = GNUTLS_E_INVALID_REQUEST;
				goto dsa_fail;
			}

			params->palgo = GNUTLS_DIG_SHA384;

			if (params->seed_size) {
				ret = _dsa_generate_dss_pqg(&pub, &cert, index,
							    params->seed_size,
							    params->seed, NULL,
							    NULL, level,
							    q_bits);
			} else {
				ret = dsa_generate_dss_pqg(&pub, &cert, index,
							   NULL,
							   rnd_tmpkey_func,
							   NULL, NULL, level,
							   q_bits);
			}
			if (ret != 1 || HAVE_LIB_ERROR()) {
				gnutls_assert();
				ret = GNUTLS_E_PK_GENERATION_ERROR;
				goto dsa_fail;
			}

			if (cert.seed_length &&
			    cert.seed_length < sizeof(params->seed)) {
				params->seed_size = cert.seed_length;
				memcpy(params->seed, cert.seed,
				       cert.seed_length);
			}

			/* verify the generated parameters */
			ret = dsa_validate_dss_pqg(&pub, &cert, index);
			if (ret != 1) {
				gnutls_assert();
				ret = GNUTLS_E_PK_GENERATION_ERROR;
				goto dsa_fail;
			}
		} else {
			if (q_bits < 160)
				q_bits = 160;

			ret = dsa_generate_params(&pub, NULL, rnd_tmpkey_func,
						  NULL, NULL, level, q_bits);
			if (ret != 1 || HAVE_LIB_ERROR()) {
				gnutls_assert();
				ret = GNUTLS_E_PK_GENERATION_ERROR;
				goto dsa_fail;
			}
		}

		params->params_nr = 0;

		ret = _gnutls_mpi_init_multi(&params->params[DSA_P],
					     &params->params[DSA_Q],
					     &params->params[DSA_G], NULL);
		if (ret < 0) {
			gnutls_assert();
			goto dsa_fail;
		}
		params->params_nr = 3;

		mpz_set(TOMPZ(params->params[DSA_P]), pub.p);
		mpz_set(TOMPZ(params->params[DSA_Q]), pub.q);
		mpz_set(TOMPZ(params->params[DSA_G]), pub.g);

		ret = 0;

	dsa_fail:
		dsa_params_clear(&pub);

		if (ret < 0)
			goto fail;

		break;
	}
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_RSA_OAEP:
	case GNUTLS_PK_RSA:
	case GNUTLS_PK_ECDSA:
	case GNUTLS_PK_EDDSA_ED25519:
	case GNUTLS_PK_EDDSA_ED448:
	case GNUTLS_PK_ECDH_X25519:
	case GNUTLS_PK_ECDH_X448:
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512:
#endif
		break;
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	FAIL_IF_LIB_ERROR;
	return 0;

fail:

	for (i = 0; i < params->params_nr; i++) {
		_gnutls_mpi_release(&params->params[i]);
	}
	params->params_nr = 0;

	FAIL_IF_LIB_ERROR;
	return ret;
}

#ifdef ENABLE_FIPS140
int _gnutls_dh_generate_key(gnutls_dh_params_t dh_params,
			    gnutls_datum_t *priv_key, gnutls_datum_t *pub_key);

int _gnutls_dh_compute_key(gnutls_dh_params_t dh_params,
			   const gnutls_datum_t *priv_key,
			   const gnutls_datum_t *pub_key,
			   const gnutls_datum_t *peer_key, gnutls_datum_t *Z);

int _gnutls_ecdh_compute_key(gnutls_ecc_curve_t curve, const gnutls_datum_t *x,
			     const gnutls_datum_t *y, const gnutls_datum_t *k,
			     const gnutls_datum_t *peer_x,
			     const gnutls_datum_t *peer_y, gnutls_datum_t *Z);

int _gnutls_ecdh_generate_key(gnutls_ecc_curve_t curve, gnutls_datum_t *x,
			      gnutls_datum_t *y, gnutls_datum_t *k);

int _gnutls_dh_generate_key(gnutls_dh_params_t dh_params,
			    gnutls_datum_t *priv_key, gnutls_datum_t *pub_key)
{
	gnutls_pk_params_st params;
	int ret;

	gnutls_pk_params_init(&params);
	params.params[DH_P] = _gnutls_mpi_copy(dh_params->params[0]);
	params.params[DH_G] = _gnutls_mpi_copy(dh_params->params[1]);

	params.params_nr = 5;
	params.algo = GNUTLS_PK_DH;

	priv_key->data = NULL;
	pub_key->data = NULL;

	ret = _gnutls_pk_generate_keys(GNUTLS_PK_DH, dh_params->q_bits, &params,
				       0);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}

	ret = _gnutls_mpi_dprint_lz(params.params[DH_X], priv_key);
	if (ret < 0) {
		gnutls_assert();
		goto fail;
	}

	ret = _gnutls_mpi_dprint_lz(params.params[DH_Y], pub_key);
	if (ret < 0) {
		gnutls_assert();
		goto fail;
	}

	ret = 0;
	goto cleanup;
fail:
	gnutls_free(pub_key->data);
	gnutls_free(priv_key->data);
cleanup:
	gnutls_pk_params_clear(&params);
	gnutls_pk_params_release(&params);
	return ret;
}

/* Note that the value of Z will have the leading bytes stripped if they are zero -
 * which follows the TLS approach. */
int _gnutls_dh_compute_key(gnutls_dh_params_t dh_params,
			   const gnutls_datum_t *priv_key,
			   const gnutls_datum_t *pub_key,
			   const gnutls_datum_t *peer_key, gnutls_datum_t *Z)
{
	gnutls_pk_params_st pub, priv;
	int ret;

	gnutls_pk_params_init(&pub);
	pub.params_nr = 5;
	pub.algo = GNUTLS_PK_DH;

	gnutls_pk_params_init(&priv);
	priv.params_nr = 5;
	priv.algo = GNUTLS_PK_DH;

	if (_gnutls_mpi_init_scan_nz(&pub.params[DH_Y], peer_key->data,
				     peer_key->size) != 0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	priv.params[DH_P] = _gnutls_mpi_copy(dh_params->params[0]);
	priv.params[DH_G] = _gnutls_mpi_copy(dh_params->params[1]);
	if (dh_params->params[2])
		priv.params[DH_Q] = _gnutls_mpi_copy(dh_params->params[2]);

	if (_gnutls_mpi_init_scan_nz(&priv.params[DH_X], priv_key->data,
				     priv_key->size) != 0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	Z->data = NULL;

	ret = _gnutls_pk_derive(GNUTLS_PK_DH, Z, &priv, &pub);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;
cleanup:
	gnutls_pk_params_clear(&pub);
	gnutls_pk_params_release(&pub);
	gnutls_pk_params_clear(&priv);
	gnutls_pk_params_release(&priv);
	return ret;
}

int _gnutls_ecdh_generate_key(gnutls_ecc_curve_t curve, gnutls_datum_t *x,
			      gnutls_datum_t *y, gnutls_datum_t *k)
{
	gnutls_pk_params_st params;
	int ret;

	gnutls_pk_params_init(&params);
	params.params_nr = 3;
	params.curve = curve;
	params.algo = GNUTLS_PK_ECDSA;

	x->data = NULL;
	y->data = NULL;
	k->data = NULL;

	ret = _gnutls_pk_generate_keys(GNUTLS_PK_ECDSA, curve, &params, 0);
	if (ret < 0) {
		return gnutls_assert_val(ret);
	}

	ret = _gnutls_mpi_dprint_lz(params.params[ECC_X], x);
	if (ret < 0) {
		gnutls_assert();
		goto fail;
	}

	ret = _gnutls_mpi_dprint_lz(params.params[ECC_Y], y);
	if (ret < 0) {
		gnutls_assert();
		goto fail;
	}

	ret = _gnutls_mpi_dprint_lz(params.params[ECC_K], k);
	if (ret < 0) {
		gnutls_assert();
		goto fail;
	}

	ret = 0;
	goto cleanup;
fail:
	gnutls_free(y->data);
	gnutls_free(x->data);
	gnutls_free(k->data);
cleanup:
	gnutls_pk_params_clear(&params);
	gnutls_pk_params_release(&params);
	return ret;
}

int _gnutls_ecdh_compute_key(gnutls_ecc_curve_t curve, const gnutls_datum_t *x,
			     const gnutls_datum_t *y, const gnutls_datum_t *k,
			     const gnutls_datum_t *peer_x,
			     const gnutls_datum_t *peer_y, gnutls_datum_t *Z)
{
	gnutls_pk_params_st pub, priv;
	int ret;

	gnutls_pk_params_init(&pub);
	pub.params_nr = 3;
	pub.algo = GNUTLS_PK_ECDSA;
	pub.curve = curve;

	gnutls_pk_params_init(&priv);
	priv.params_nr = 3;
	priv.algo = GNUTLS_PK_ECDSA;
	priv.curve = curve;

	if (_gnutls_mpi_init_scan_nz(&pub.params[ECC_Y], peer_y->data,
				     peer_y->size) != 0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	if (_gnutls_mpi_init_scan_nz(&pub.params[ECC_X], peer_x->data,
				     peer_x->size) != 0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	if (_gnutls_mpi_init_scan_nz(&priv.params[ECC_Y], y->data, y->size) !=
	    0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	if (_gnutls_mpi_init_scan_nz(&priv.params[ECC_X], x->data, x->size) !=
	    0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	if (_gnutls_mpi_init_scan_nz(&priv.params[ECC_K], k->data, k->size) !=
	    0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	Z->data = NULL;

	ret = _gnutls_pk_derive(GNUTLS_PK_ECDSA, Z, &priv, &pub);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;
cleanup:
	gnutls_pk_params_clear(&pub);
	gnutls_pk_params_release(&pub);
	gnutls_pk_params_clear(&priv);
	gnutls_pk_params_release(&priv);
	return ret;
}

static int pct_test(gnutls_pk_algorithm_t algo,
		    const gnutls_pk_params_st *params)
{
	int ret;
	gnutls_datum_t sig = { NULL, 0 };
	const char const_data[20] = "onetwothreefourfive";
	const char const_data_sha256[32] = "onetwothreefourfivesixseveneight";
	const char const_data_sha384[48] =
		"onetwothreefourfivesixseveneightnineteneleventwe";
	const char const_data_sha512[64] =
		"onetwothreefourfivesixseveneightnineteneleventwelvethirteenfourt";
	gnutls_datum_t ddata, tmp = { NULL, 0 };
	char *gen_data = NULL;
	gnutls_x509_spki_st spki;
	gnutls_fips140_context_t context;

	ret = _gnutls_x509_spki_copy(&spki, &params->spki);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (algo == GNUTLS_PK_DSA || algo == GNUTLS_PK_EC) {
		unsigned hash_len;
		const mac_entry_st *me;

		me = _gnutls_dsa_q_to_hash(params, &hash_len);
		spki.dsa_dig = MAC_TO_DIG(me->id);
		gen_data = gnutls_malloc(hash_len);
		gnutls_rnd(GNUTLS_RND_NONCE, gen_data, hash_len);

		ddata.data = (void *)gen_data;
		ddata.size = hash_len;
	} else if (algo == GNUTLS_PK_GOST_01 || algo == GNUTLS_PK_GOST_12_256) {
		ddata.data = (void *)const_data_sha256;
		ddata.size = sizeof(const_data_sha256);
	} else if (algo == GNUTLS_PK_GOST_12_512) {
		ddata.data = (void *)const_data_sha512;
		ddata.size = sizeof(const_data_sha512);
	} else if (algo == GNUTLS_PK_RSA_PSS) {
		if (spki.rsa_pss_dig == GNUTLS_DIG_UNKNOWN)
			spki.rsa_pss_dig = GNUTLS_DIG_SHA256;

		switch (spki.rsa_pss_dig) {
		case GNUTLS_DIG_SHA256:
			ddata.data = (void *)const_data_sha256;
			ddata.size = sizeof(const_data_sha256);
			break;
		case GNUTLS_DIG_SHA384:
			ddata.data = (void *)const_data_sha384;
			ddata.size = sizeof(const_data_sha384);
			break;
		case GNUTLS_DIG_SHA512:
			ddata.data = (void *)const_data_sha512;
			ddata.size = sizeof(const_data_sha512);
			break;
		default:
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
			goto cleanup;
		}
	} else {
		ddata.data = (void *)const_data;
		ddata.size = sizeof(const_data);
	}

	switch (algo) {
	case GNUTLS_PK_RSA:
	case GNUTLS_PK_RSA_OAEP:
		if (algo == GNUTLS_PK_RSA) {
			/* Push a temporary FIPS context because _gnutls_pk_encrypt and
			 * _gnutls_pk_decrypt below will mark RSAES-PKCS1-v1_5 operation
			 * non-approved */
			if (gnutls_fips140_context_init(&context) < 0) {
				ret = gnutls_assert_val(
					GNUTLS_E_PK_GENERATION_ERROR);
				goto cleanup;
			}
			if (gnutls_fips140_push_context(context) < 0) {
				ret = gnutls_assert_val(
					GNUTLS_E_PK_GENERATION_ERROR);
				gnutls_fips140_context_deinit(context);
				goto cleanup;
			}
		}

		ret = _gnutls_pk_encrypt(algo, &sig, &ddata, params);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
		}
		if (ret == 0 && ddata.size == sig.size &&
		    memcmp(ddata.data, sig.data, sig.size) == 0) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
		}
		if (ret == 0 &&
		    _gnutls_pk_decrypt(algo, &tmp, &sig, params) < 0) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
		}
		if (ret == 0 &&
		    !(tmp.size == ddata.size &&
		      memcmp(tmp.data, ddata.data, tmp.size) == 0)) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
		}

		if (algo == GNUTLS_PK_RSA) {
			if (unlikely(gnutls_fips140_pop_context() < 0)) {
				ret = gnutls_assert_val(
					GNUTLS_E_PK_GENERATION_ERROR);
			}
			gnutls_fips140_context_deinit(context);
		}

		if (ret < 0) {
			goto cleanup;
		}

		free(sig.data);
		sig.data = NULL;

		/* RSA-OAEP can't be used for signing */
		if (algo == GNUTLS_PK_RSA_OAEP) {
			break;
		}

		FALLTHROUGH;
	case GNUTLS_PK_EC: /* we only do keys for ECDSA */
	case GNUTLS_PK_EDDSA_ED25519:
	case GNUTLS_PK_EDDSA_ED448:
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA:
#endif
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512:
		ret = _gnutls_pk_sign(algo, &sig, &ddata, params, &spki);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
			goto cleanup;
		}

		ret = _gnutls_pk_verify(algo, &ddata, &sig, params, &spki);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
			gnutls_assert();
			goto cleanup;
		}
		break;
	case GNUTLS_PK_DH: {
		mpz_t y;

		/* Perform SP800 56A (rev 3) 5.6.2.1.4 Owner Assurance
			 * of Pair-wise Consistency check, even if we only
			 * support ephemeral DH, as it is required by FIPS
			 * 140-3 IG 10.3.A.
			 *
			 * Use the private key, x, along with the generator g
			 * and prime modulus p included in the domain
			 * parameters associated with the key pair to compute
			 * g^x mod p. Compare the result to the public key, y.
			 */
		mpz_init(y);
		mpz_powm(y, TOMPZ(params->params[DSA_G]),
			 TOMPZ(params->params[DSA_X]),
			 TOMPZ(params->params[DSA_P]));
		if (unlikely(mpz_cmp(y, TOMPZ(params->params[DSA_Y])) != 0)) {
			ret = gnutls_assert_val(GNUTLS_E_PK_GENERATION_ERROR);
			mpz_clear(y);
			goto cleanup;
		}
		mpz_clear(y);
		break;
	}
	case GNUTLS_PK_ECDH_X25519:
	case GNUTLS_PK_ECDH_X448:
		break;
#ifdef HAVE_LIBOQS
	case GNUTLS_PK_EXP_KYBER768:
		if (_gnutls_liboqs_ensure() < 0 ||
		    !GNUTLS_OQS_FUNC(OQS_KEM_alg_is_enabled)(
			    OQS_KEM_alg_kyber_768)) {
			ret = gnutls_assert_val(GNUTLS_E_UNKNOWN_PK_ALGORITHM);
			goto cleanup;
		}
#endif
		break;
	default:
		ret = gnutls_assert_val(GNUTLS_E_UNKNOWN_PK_ALGORITHM);
		goto cleanup;
	}

	ret = 0;
cleanup:
	if (ret == GNUTLS_E_PK_GENERATION_ERROR) {
		_gnutls_switch_lib_state(LIB_STATE_ERROR);
	}
	_gnutls_x509_spki_clear(&spki);
	gnutls_free(gen_data);
	gnutls_free(sig.data);
	gnutls_free(tmp.data);
	return ret;
}
#endif

static inline int eddsa_public_key(gnutls_pk_algorithm_t algo, uint8_t *pub,
				   const uint8_t *priv)
{
	switch (algo) {
	case GNUTLS_PK_EDDSA_ED25519:
		ed25519_sha512_public_key(pub, priv);
		return 0;
	case GNUTLS_PK_EDDSA_ED448:
		ed448_shake256_public_key(pub, priv);
		return 0;
	default:
		return gnutls_assert_val(
			GNUTLS_E_UNSUPPORTED_SIGNATURE_ALGORITHM);
	}
}

static inline int edwards_curve_mul_g(gnutls_pk_algorithm_t algo, uint8_t *q,
				      const uint8_t *n)
{
	switch (algo) {
	case GNUTLS_PK_ECDH_X25519:
		curve25519_mul_g(q, n);
		return 0;
	case GNUTLS_PK_ECDH_X448:
		curve448_mul_g(q, n);
		return 0;
	default:
		return gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
	}
}

static inline int dh_find_q(const gnutls_pk_params_st *pk_params, mpz_t q)
{
	gnutls_datum_t prime = { NULL, 0 };
	gnutls_datum_t generator = { NULL, 0 };
	uint8_t *data_q;
	size_t n_q;
	bigint_t _q;
	int ret = 0;

	ret = _gnutls_mpi_dprint(pk_params->params[DSA_P], &prime);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = _gnutls_mpi_dprint(pk_params->params[DSA_G], &generator);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (!_gnutls_dh_prime_match_fips_approved(
		    prime.data, prime.size, generator.data, generator.size,
		    &data_q, &n_q)) {
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		goto cleanup;
	}

	if (_gnutls_mpi_init_scan_nz(&_q, data_q, n_q) != 0) {
		ret = gnutls_assert_val(GNUTLS_E_MPI_SCAN_FAILED);
		goto cleanup;
	}

	mpz_set(q, TOMPZ(_q));
	_gnutls_mpi_release(&_q);

cleanup:
	gnutls_free(prime.data);
	gnutls_free(generator.data);

	return ret;
}

/* To generate a DH key either q must be set in the params or
 * level should be set to the number of required bits.
 */
static int
wrap_nettle_pk_generate_keys(gnutls_pk_algorithm_t algo,
			     unsigned int level /*bits or curve */,
			     gnutls_pk_params_st *params, unsigned ephemeral
			     /*non-zero if they are ephemeral keys */)
{
	int ret;
	unsigned int i;
	unsigned rnd_level;
	nettle_random_func *rnd_func;
	bool not_approved = false;

	FAIL_IF_LIB_ERROR;

	/* check if the curve relates to the algorithm used */
	if (IS_EC(algo) && gnutls_ecc_curve_get_pk(level) != algo) {
		ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
		goto cleanup;
	}

	if (ephemeral) {
		rnd_level = GNUTLS_RND_RANDOM;
		rnd_func = rnd_tmpkey_func;
	} else {
		rnd_func = rnd_key_func;
		rnd_level = GNUTLS_RND_KEY;
	}

	switch (algo) {
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA:
#ifdef ENABLE_FIPS140
		if (_gnutls_fips_mode_enabled() != 0) {
			struct dsa_params pub;
			mpz_t x, y;

			/* DSA is currently being defined as sunset with the
			 * current draft of FIPS 186-5 */
			not_approved = true;

			if (params->params[DSA_Q] == NULL) {
				ret = gnutls_assert_val(
					GNUTLS_E_INVALID_REQUEST);
				goto cleanup;
			}

			_dsa_params_get(params, &pub);

			mpz_init(x);
			mpz_init(y);

			ret = dsa_generate_dss_keypair(&pub, y, x, NULL,
						       rnd_func, NULL, NULL);
			if (ret != 1 || HAVE_LIB_ERROR()) {
				gnutls_assert();
				ret = GNUTLS_E_PK_GENERATION_ERROR;
				goto dsa_fail;
			}

			ret = _gnutls_mpi_init_multi(&params->params[DSA_Y],
						     &params->params[DSA_X],
						     NULL);
			if (ret < 0) {
				gnutls_assert();
				goto dsa_fail;
			}

			mpz_set(TOMPZ(params->params[DSA_Y]), y);
			mpz_set(TOMPZ(params->params[DSA_X]), x);
			params->params_nr += 2;

		dsa_fail:
			mpz_clear(x);
			mpz_clear(y);

			if (ret < 0)
				goto cleanup;

			break;
		}
#endif
		FALLTHROUGH;
#endif
	case GNUTLS_PK_DH: {
		struct dsa_params pub;
		mpz_t r;
		mpz_t x, y;
		int max_tries;
		unsigned have_q = 0;
		mpz_t q;
		mpz_t primesub1;
		mpz_t ypowq;

		if (algo != params->algo) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		_dsa_params_get(params, &pub);

		if (params->params[DSA_Q] != NULL)
			have_q = 1;

		/* This check is for the case !ENABLE_FIPS140 */
		if (algo == GNUTLS_PK_DSA && have_q == 0) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		mpz_init(r);
		mpz_init(x);
		mpz_init(y);

		mpz_init(q);
		mpz_init(primesub1);
		mpz_init(ypowq);

		max_tries = 3;
		do {
			if (have_q) {
				mpz_set(r, pub.q);
				mpz_sub_ui(r, r, 2);
				nettle_mpz_random(x, NULL, rnd_func, r);
				mpz_add_ui(x, x, 1);
			} else {
				unsigned size = mpz_sizeinbase(pub.p, 2);
				if (level == 0)
					level = MIN(size,
						    DH_EXPONENT_SIZE(size));
				nettle_mpz_random_size(x, NULL, rnd_func,
						       level);

				if (level >= size)
					mpz_mod(x, x, pub.p);
			}

			mpz_powm(y, pub.g, x, pub.p);

			max_tries--;
			if (max_tries <= 0) {
				gnutls_assert();
				ret = GNUTLS_E_RANDOM_FAILED;
				goto dh_fail;
			}

			if (HAVE_LIB_ERROR()) {
				gnutls_assert();
				ret = GNUTLS_E_LIB_IN_ERROR_STATE;
				goto dh_fail;
			}

		} while (mpz_cmp_ui(y, 1) == 0);

#ifdef ENABLE_FIPS140
		if (_gnutls_fips_mode_enabled()) {
			/* Perform FFC full public key validation checks
				 * according to SP800-56A (revision 3), 5.6.2.3.1.
				 */

			/* Step 1: 2 <= y <= p - 2 */
			mpz_sub_ui(primesub1, pub.p, 1);

			if (mpz_cmp_ui(y, 2) < 0 ||
			    mpz_cmp(y, primesub1) >= 0) {
				ret = gnutls_assert_val(GNUTLS_E_RANDOM_FAILED);
				goto dh_fail;
			}

			/* Step 2: 1 = y^q mod p */
			if (have_q)
				mpz_set(q, pub.q);
			else {
				ret = dh_find_q(params, q);
				if (ret < 0)
					goto dh_fail;
			}

			mpz_powm(ypowq, y, q, pub.p);
			if (mpz_cmp_ui(ypowq, 1) != 0) {
				ret = gnutls_assert_val(GNUTLS_E_RANDOM_FAILED);
				goto dh_fail;
			}
		}
#endif

		ret = _gnutls_mpi_init_multi(&params->params[DSA_Y],
					     &params->params[DSA_X], NULL);
		if (ret < 0) {
			gnutls_assert();
			goto dh_fail;
		}

		mpz_set(TOMPZ(params->params[DSA_Y]), y);
		mpz_set(TOMPZ(params->params[DSA_X]), x);
		params->params_nr += 2;

		ret = 0;

	dh_fail:
		mpz_clear(r);
		mpz_clear(x);
		mpz_clear(y);
		mpz_clear(q);
		mpz_clear(primesub1);
		mpz_clear(ypowq);

		if (ret < 0)
			goto cleanup;

		break;
	}
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_RSA_OAEP:
	case GNUTLS_PK_RSA: {
		struct rsa_public_key pub;
		struct rsa_private_key priv;

		rsa_public_key_init(&pub);
		rsa_private_key_init(&priv);

		mpz_set_ui(pub.e, 65537);

		if ((params->pkflags & GNUTLS_PK_FLAG_PROVABLE) ||
		    _gnutls_fips_mode_enabled() != 0) {
			params->pkflags |= GNUTLS_PK_FLAG_PROVABLE;
			if (params->palgo != 0 &&
			    params->palgo != GNUTLS_DIG_SHA384) {
				ret = GNUTLS_E_INVALID_REQUEST;
				goto rsa_fail;
			}

			params->palgo = GNUTLS_DIG_SHA384;

			if (params->seed_size) {
				ret = _rsa_generate_fips186_4_keypair(
					&pub, &priv, params->seed_size,
					params->seed, NULL, NULL, level);
			} else {
				unsigned retries = 0;
				/* The provable RSA key generation process is deterministic
					 * but has an internal maximum iteration counter and when
					 * exceed will fail for certain random seeds. This is a very
					 * rare condition, but it nevertheless happens and even CI builds fail
					 * occasionally. When we generate the random seed internally, remediate
					 * by retrying a different seed on failure. */
				do {
					params->seed_size =
						sizeof(params->seed);
					ret = rsa_generate_fips186_4_keypair(
						&pub, &priv, NULL, rnd_func,
						NULL, NULL, &params->seed_size,
						params->seed, level);
				} while (ret != 1 && ++retries < 3);
			}
		} else {
			not_approved = true;

			ret = rsa_generate_keypair(&pub, &priv, NULL, rnd_func,
						   NULL, NULL, level, 0);
		}
		if (ret != 1 || HAVE_LIB_ERROR()) {
			gnutls_assert();
			ret = GNUTLS_E_PK_GENERATION_ERROR;
			goto rsa_fail;
		}

		params->params_nr = 0;
		for (i = 0; i < RSA_PRIVATE_PARAMS; i++) {
			ret = _gnutls_mpi_init(&params->params[i]);
			if (ret < 0) {
				gnutls_assert();
				goto rsa_fail;
			}
			params->params_nr++;
		}

		/* In FIPS 140-3, pub.n should be 2048-bit or larger; it
			 * is assured in rsa_generate_fips186_4_keypair in
			 * lib/nettle/int/rsa-keygen-fips186.c. */

		mpz_set(TOMPZ(params->params[RSA_MODULUS]), pub.n);
		mpz_set(TOMPZ(params->params[RSA_PUB]), pub.e);
		mpz_set(TOMPZ(params->params[RSA_PRIV]), priv.d);
		mpz_set(TOMPZ(params->params[RSA_PRIME1]), priv.p);
		mpz_set(TOMPZ(params->params[RSA_PRIME2]), priv.q);
		mpz_set(TOMPZ(params->params[RSA_COEF]), priv.c);
		mpz_set(TOMPZ(params->params[RSA_E1]), priv.a);
		mpz_set(TOMPZ(params->params[RSA_E2]), priv.b);

		ret = 0;

	rsa_fail:
		rsa_private_key_clear(&priv);
		rsa_public_key_clear(&pub);

		if (ret < 0)
			goto cleanup;

		break;
	}
	case GNUTLS_PK_EDDSA_ED25519:
	case GNUTLS_PK_EDDSA_ED448: {
		unsigned size = gnutls_ecc_curve_get_size(level);

		/* EdDSA is not approved yet */
		not_approved = true;

		if (params->pkflags & GNUTLS_PK_FLAG_PROVABLE) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		if (unlikely(get_eddsa_curve(algo) != level)) {
			ret = gnutls_assert_val(GNUTLS_E_ECC_UNSUPPORTED_CURVE);
			goto cleanup;
		}

		if (size == 0) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		params->curve = level;

		params->raw_priv.data = gnutls_malloc(size);
		if (params->raw_priv.data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		params->raw_pub.data = gnutls_malloc(size);
		if (params->raw_pub.data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		ret = gnutls_rnd(rnd_level, params->raw_priv.data, size);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}
		params->raw_pub.size = size;
		params->raw_priv.size = size;

		ret = eddsa_public_key(algo, params->raw_pub.data,
				       params->raw_priv.data);
		if (ret < 0)
			goto cleanup;

		break;
	}
	case GNUTLS_PK_ECDSA:
		if (params->pkflags & GNUTLS_PK_FLAG_PROVABLE)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		{
			struct ecc_scalar key;
			struct ecc_point pub;
			const struct ecc_curve *curve;
			struct ecc_scalar n;
			struct ecc_scalar m;
			struct ecc_point r;
			mpz_t x, y, xx, yy, nn, mm;

			curve = get_supported_nist_curve(level);
			if (curve == NULL) {
				ret = gnutls_assert_val(
					GNUTLS_E_ECC_UNSUPPORTED_CURVE);
				goto cleanup;
			}

			/* P-192 is not supported in FIPS 140-3 */
			if (level == GNUTLS_ECC_CURVE_SECP192R1) {
				not_approved = true;
			}

			mpz_init(x);
			mpz_init(y);
			mpz_init(xx);
			mpz_init(yy);
			mpz_init(nn);
			mpz_init(mm);

			ecc_scalar_init(&key, curve);
			ecc_point_init(&pub, curve);
			ecc_scalar_init(&n, curve);
			ecc_scalar_init(&m, curve);
			ecc_point_init(&r, curve);

			ecdsa_generate_keypair(&pub, &key, NULL, rnd_func);
			if (HAVE_LIB_ERROR()) {
				ret = gnutls_assert_val(
					GNUTLS_E_LIB_IN_ERROR_STATE);
				goto ecc_fail;
			}

			ret = _gnutls_mpi_init_multi(&params->params[ECC_X],
						     &params->params[ECC_Y],
						     &params->params[ECC_K],
						     NULL);
			if (ret < 0) {
				gnutls_assert();
				goto ecc_fail;
			}

			params->curve = level;
			params->params_nr = ECC_PRIVATE_PARAMS;

			ecc_point_get(&pub, x, y);

#ifdef ENABLE_FIPS140
			if (_gnutls_fips_mode_enabled()) {
				/* Perform ECC full public key validation checks
				 * according to SP800-56A (revision 3), 5.6.2.3.3.
				 */

				const char *order, *modulus;

				/* Step 1: verify that Q is not an identity
				 * element (an infinity point).  Note that this
				 * cannot happen in the nettle implementation,
				 * because it cannot represent an infinity point
				 * on curves. */
				if (mpz_cmp_ui(x, 0) == 0 &&
				    mpz_cmp_ui(y, 0) == 0) {
					ret = gnutls_assert_val(
						GNUTLS_E_ILLEGAL_PARAMETER);
					goto ecc_fail;
				}

				/* Step 2: verify that both coordinates of Q are
				 * in the range [0, p - 1].
				 *
				 * Step 3: verify that Q lie on the curve
				 *
				 * Both checks are performed in nettle.  */
				if (!ecc_point_set(&r, x, y)) {
					ret = gnutls_assert_val(
						GNUTLS_E_ILLEGAL_PARAMETER);
					goto ecc_fail;
				}

				/* Step 4: verify that n * Q, where n is the
				 * curve order, result in an identity element
				 *
				 * Since nettle internally cannot represent an
				 * identity element on curves, we validate this
				 * instead:
				 *
				 *   (n - 1) * Q = -Q
				 *
				 * That effectively means: n * Q = -Q + Q = O
				 */
				order = get_supported_nist_curve_order(level);
				if (unlikely(order == NULL)) {
					ret = gnutls_assert_val(
						GNUTLS_E_INTERNAL_ERROR);
					goto ecc_fail;
				}

				ret = mpz_set_str(nn, order, 16);
				if (unlikely(ret < 0)) {
					ret = gnutls_assert_val(
						GNUTLS_E_MPI_SCAN_FAILED);
					goto ecc_fail;
				}

				modulus =
					get_supported_nist_curve_modulus(level);
				if (unlikely(modulus == NULL)) {
					ret = gnutls_assert_val(
						GNUTLS_E_INTERNAL_ERROR);
					goto ecc_fail;
				}

				ret = mpz_set_str(mm, modulus, 16);
				if (unlikely(ret < 0)) {
					ret = gnutls_assert_val(
						GNUTLS_E_MPI_SCAN_FAILED);
					goto ecc_fail;
				}

				/* (n - 1) * Q = -Q */
				mpz_sub_ui(nn, nn, 1);
				ecc_scalar_set(&n, nn);
				ecc_point_mul(&r, &n, &r);
				ecc_point_get(&r, xx, yy);
				mpz_sub(mm, mm, y);

				if (mpz_cmp(xx, x) != 0 ||
				    mpz_cmp(yy, mm) != 0) {
					ret = gnutls_assert_val(
						GNUTLS_E_ILLEGAL_PARAMETER);
					goto ecc_fail;
				}
			} else {
				not_approved = true;
			}
#endif

			mpz_set(TOMPZ(params->params[ECC_X]), x);
			mpz_set(TOMPZ(params->params[ECC_Y]), y);

			ecc_scalar_get(&key, TOMPZ(params->params[ECC_K]));

			ret = 0;

		ecc_fail:
			mpz_clear(x);
			mpz_clear(y);
			mpz_clear(xx);
			mpz_clear(yy);
			mpz_clear(nn);
			mpz_clear(mm);
			ecc_point_clear(&pub);
			ecc_scalar_clear(&key);
			ecc_point_clear(&r);
			ecc_scalar_clear(&n);
			ecc_scalar_clear(&m);

			if (ret < 0)
				goto cleanup;

			break;
		}
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512:
		if (params->pkflags & GNUTLS_PK_FLAG_PROVABLE)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		{
			struct ecc_scalar key;
			struct ecc_point pub;
			const struct ecc_curve *curve;
			const mac_entry_st *me;

			/* GOST curves are not approved */
			not_approved = true;

			curve = get_supported_gost_curve(level);
			if (curve == NULL) {
				ret = gnutls_assert_val(
					GNUTLS_E_ECC_UNSUPPORTED_CURVE);
				goto cleanup;
			}

			me = hash_to_entry(_gnutls_gost_digest(algo));
			if (!me || me->output_size * 8 != ecc_bit_size(curve)) {
				ret = gnutls_assert_val(
					GNUTLS_E_INVALID_REQUEST);
				goto cleanup;
			}

			ecc_scalar_init(&key, curve);
			ecc_point_init(&pub, curve);

			gostdsa_generate_keypair(&pub, &key, NULL,
						 rnd_key_func);
			if (HAVE_LIB_ERROR()) {
				ret = gnutls_assert_val(
					GNUTLS_E_LIB_IN_ERROR_STATE);
				goto ecc_fail;
			}

			ret = _gnutls_mpi_init_multi(&params->params[GOST_X],
						     &params->params[GOST_Y],
						     &params->params[GOST_K],
						     NULL);
			if (ret < 0) {
				gnutls_assert();
				goto gost_fail;
			}

			params->curve = level;
			params->params_nr = GOST_PRIVATE_PARAMS;

			ecc_point_get(&pub, TOMPZ(params->params[GOST_X]),
				      TOMPZ(params->params[GOST_Y]));
			ecc_scalar_get(&key, TOMPZ(params->params[GOST_K]));

			ret = 0;

		gost_fail:
			ecc_point_clear(&pub);
			ecc_scalar_clear(&key);

			if (ret < 0)
				goto cleanup;

			break;
		}
#endif
	case GNUTLS_PK_ECDH_X25519:
	case GNUTLS_PK_ECDH_X448: {
		unsigned size = gnutls_ecc_curve_get_size(level);

		not_approved = true;

		if (size == 0) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
			goto cleanup;
		}

		params->curve = level;

		params->raw_priv.data = gnutls_malloc(size);
		if (params->raw_priv.data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		params->raw_pub.data = gnutls_malloc(size);
		if (params->raw_pub.data == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		ret = gnutls_rnd(rnd_level, params->raw_priv.data, size);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}
		params->raw_pub.size = size;
		params->raw_priv.size = size;

		ret = edwards_curve_mul_g(algo, params->raw_pub.data,
					  params->raw_priv.data);
		if (ret < 0)
			goto cleanup;
		break;
	}
#ifdef HAVE_LIBOQS
	case GNUTLS_PK_EXP_KYBER768: {
		OQS_KEM *kem = NULL;
		OQS_STATUS rc;

		if (_gnutls_liboqs_ensure() < 0 ||
		    !GNUTLS_OQS_FUNC(OQS_KEM_alg_is_enabled)(
			    OQS_KEM_alg_kyber_768)) {
			ret = gnutls_assert_val(GNUTLS_E_UNKNOWN_PK_ALGORITHM);
			goto cleanup;
		}

		not_approved = true;

		kem = GNUTLS_OQS_FUNC(OQS_KEM_new)(OQS_KEM_alg_kyber_768);
		if (kem == NULL) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		params->raw_priv.size = kem->length_secret_key;
		params->raw_priv.data = gnutls_malloc(params->raw_priv.size);
		if (params->raw_priv.data == NULL) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		params->raw_pub.size = kem->length_public_key;
		params->raw_pub.data = gnutls_malloc(params->raw_pub.size);
		if (params->raw_pub.data == NULL) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto cleanup;
		}

		rc = GNUTLS_OQS_FUNC(OQS_KEM_keypair)(kem, params->raw_pub.data,
						      params->raw_priv.data);
		if (rc != OQS_SUCCESS) {
			GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto cleanup;
		}

		GNUTLS_OQS_FUNC(OQS_KEM_free)(kem);

		ret = 0;
		break;
	}
#endif
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	params->algo = algo;

#ifdef ENABLE_FIPS140
	if (_gnutls_fips_mode_enabled()) {
		ret = pct_test(algo, params);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}
#endif

cleanup:
	if (ret < 0) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_ERROR);
		for (i = 0; i < params->params_nr; i++) {
			_gnutls_mpi_release(&params->params[i]);
		}
		params->params_nr = 0;
		gnutls_free(params->raw_priv.data);
		gnutls_free(params->raw_pub.data);
	} else if (not_approved) {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_NOT_APPROVED);
	} else {
		_gnutls_switch_fips_state(GNUTLS_FIPS140_OP_APPROVED);
	}

	FAIL_IF_LIB_ERROR;
	return ret;
}

static int wrap_nettle_pk_verify_priv_params(gnutls_pk_algorithm_t algo,
					     const gnutls_pk_params_st *params)
{
	int ret;

	switch (algo) {
	case GNUTLS_PK_RSA:
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_RSA_OAEP: {
		bigint_t t1 = NULL, t2 = NULL;

		if (params->params_nr != RSA_PRIVATE_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		ret = _gnutls_mpi_init_multi(&t1, &t2, NULL);
		if (ret < 0)
			return gnutls_assert_val(ret);

		_gnutls_mpi_mulm(t1, params->params[RSA_PRIME1],
				 params->params[RSA_PRIME2],
				 params->params[RSA_MODULUS]);
		if (_gnutls_mpi_cmp_ui(t1, 0) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto rsa_cleanup;
		}

		mpz_invert(TOMPZ(t1), TOMPZ(params->params[RSA_PRIME2]),
			   TOMPZ(params->params[RSA_PRIME1]));
		if (_gnutls_mpi_cmp(t1, params->params[RSA_COEF]) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto rsa_cleanup;
		}

		/* [RSA_PRIME1] = d % p-1, [RSA_PRIME2] = d % q-1 */
		_gnutls_mpi_sub_ui(t1, params->params[RSA_PRIME1], 1);
		ret = _gnutls_mpi_modm(t2, params->params[RSA_PRIV], t1);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto rsa_cleanup;
		}

		if (_gnutls_mpi_cmp(params->params[RSA_E1], t2) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto rsa_cleanup;
		}

		_gnutls_mpi_sub_ui(t1, params->params[RSA_PRIME2], 1);

		ret = _gnutls_mpi_modm(t2, params->params[RSA_PRIV], t1);
		if (ret < 0) {
			ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
			goto rsa_cleanup;
		}

		if (_gnutls_mpi_cmp(params->params[RSA_E2], t2) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto rsa_cleanup;
		}

		ret = 0;

	rsa_cleanup:
		zrelease_mpi_key(&t1);
		zrelease_mpi_key(&t2);
	}

	break;
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA:
#endif
	case GNUTLS_PK_DH: {
		bigint_t t1 = NULL;

		if (params->params_nr != DSA_PRIVATE_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		ret = _gnutls_mpi_init(&t1);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret = _gnutls_mpi_powm(t1, params->params[DSA_G],
				       params->params[DSA_X],
				       params->params[DSA_P]);
		if (ret < 0) {
			gnutls_assert();
			goto dsa_cleanup;
		}

		if (_gnutls_mpi_cmp(t1, params->params[DSA_Y]) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto dsa_cleanup;
		}

		ret = 0;

	dsa_cleanup:
		zrelease_mpi_key(&t1);
	}

	break;
	case GNUTLS_PK_ECDSA: {
		struct ecc_point r, pub;
		struct ecc_scalar priv;
		mpz_t x1, y1, x2, y2;
		const struct ecc_curve *curve;

		if (params->params_nr != ECC_PRIVATE_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		curve = get_supported_nist_curve(params->curve);
		if (curve == NULL)
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		ret = _ecc_params_to_pubkey(params, &pub, curve);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret = _ecc_params_to_privkey(params, &priv, curve);
		if (ret < 0) {
			ecc_point_clear(&pub);
			return gnutls_assert_val(ret);
		}

		ecc_point_init(&r, curve);
		/* verify that x,y lie on the curve */
		ret = ecc_point_set(&r, TOMPZ(params->params[ECC_X]),
				    TOMPZ(params->params[ECC_Y]));
		if (ret == 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto ecc_cleanup;
		}
		ecc_point_clear(&r);

		ecc_point_init(&r, curve);
		ecc_point_mul_g(&r, &priv);

		mpz_init(x1);
		mpz_init(y1);
		ecc_point_get(&r, x1, y1);
		ecc_point_zclear(&r);

		mpz_init(x2);
		mpz_init(y2);
		ecc_point_get(&pub, x2, y2);

		/* verify that k*(Gx,Gy)=(x,y) */
		if (mpz_cmp(x1, x2) != 0 || mpz_cmp(y1, y2) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto ecc_cleanup;
		}

		ret = 0;

	ecc_cleanup:
		ecc_scalar_zclear(&priv);
		ecc_point_clear(&pub);

		mpz_clear(x1);
		mpz_clear(y1);
		mpz_clear(x2);
		mpz_clear(y2);
	} break;
	case GNUTLS_PK_EDDSA_ED25519:
	case GNUTLS_PK_EDDSA_ED448: {
		gnutls_ecc_curve_t curve;
		const gnutls_ecc_curve_entry_st *e;
		uint8_t pub[57]; /* can accommodate both curves */

		curve = get_eddsa_curve(algo);
		e = _gnutls_ecc_curve_get_params(curve);
		if (e == NULL)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		if (params->raw_pub.data == NULL) {
			return 0; /* nothing to verify */
		}

		if (params->raw_pub.size != e->size)
			return gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);

		ret = eddsa_public_key(algo, pub, params->raw_priv.data);
		if (ret < 0)
			return ret;

		if (memcmp(params->raw_pub.data, pub, e->size) != 0)
			return gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);

		ret = 0;
		break;
	}
	case GNUTLS_PK_ECDH_X25519:
	case GNUTLS_PK_ECDH_X448: {
		gnutls_ecc_curve_t curve;
		const gnutls_ecc_curve_entry_st *e;
		uint8_t pub[57]; /* can accommodate both curves */

		curve = get_ecdh_curve(algo);
		e = _gnutls_ecc_curve_get_params(curve);
		if (e == NULL)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		if (params->raw_pub.data == NULL) {
			return 0; /* nothing to verify */
		}

		if (params->raw_pub.size != e->size)
			return gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);

		ret = edwards_curve_mul_g(algo, pub, params->raw_priv.data);
		if (ret < 0)
			return ret;

		if (memcmp(params->raw_pub.data, pub, e->size) != 0)
			return gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);

		ret = 0;
		break;
	}
#ifdef HAVE_LIBOQS
	case GNUTLS_PK_EXP_KYBER768:
		if (_gnutls_liboqs_ensure() < 0 ||
		    !GNUTLS_OQS_FUNC(OQS_KEM_alg_is_enabled)(
			    OQS_KEM_alg_kyber_768))
			return gnutls_assert_val(GNUTLS_E_UNKNOWN_PK_ALGORITHM);

		ret = 0;
		break;
#endif
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512: {
		struct ecc_point r, pub;
		struct ecc_scalar priv;
		mpz_t x1, y1, x2, y2;
		const struct ecc_curve *curve;

		if (params->params_nr != GOST_PRIVATE_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		curve = get_supported_gost_curve(params->curve);
		if (curve == NULL)
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		ret = _gost_params_to_pubkey(params, &pub, curve);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret = _gost_params_to_privkey(params, &priv, curve);
		if (ret < 0) {
			ecc_point_clear(&pub);
			return gnutls_assert_val(ret);
		}

		ecc_point_init(&r, curve);
		/* verify that x,y lie on the curve */
		ret = gost_point_set(&r, TOMPZ(params->params[GOST_X]),
				     TOMPZ(params->params[GOST_Y]));
		if (ret == 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto gost_cleanup;
		}
		ecc_point_clear(&r);

		ecc_point_init(&r, curve);
		gost_point_mul_g(&r, &priv);

		mpz_init(x1);
		mpz_init(y1);
		ecc_point_get(&r, x1, y1);
		ecc_point_zclear(&r);

		mpz_init(x2);
		mpz_init(y2);
		ecc_point_get(&pub, x2, y2);

		/* verify that k*(Gx,Gy)=(x,y) */
		if (mpz_cmp(x1, x2) != 0 || mpz_cmp(y1, y2) != 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto gost_cleanup;
		}

		ret = 0;

	gost_cleanup:
		ecc_scalar_zclear(&priv);
		ecc_point_clear(&pub);

		mpz_clear(x1);
		mpz_clear(y1);
		mpz_clear(x2);
		mpz_clear(y2);
	} break;
#endif
	default:
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	return ret;
}

static int wrap_nettle_pk_verify_pub_params(gnutls_pk_algorithm_t algo,
					    const gnutls_pk_params_st *params)
{
	int ret;

	switch (algo) {
	case GNUTLS_PK_RSA:
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_RSA_OAEP:
#ifdef ENABLE_DSA
	case GNUTLS_PK_DSA:
#endif
	case GNUTLS_PK_EDDSA_ED25519:
	case GNUTLS_PK_EDDSA_ED448:
		return 0;
	case GNUTLS_PK_ECDSA: {
		/* just verify that x and y lie on the curve */
		struct ecc_point r, pub;
		const struct ecc_curve *curve;

		if (params->params_nr != ECC_PUBLIC_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		curve = get_supported_nist_curve(params->curve);
		if (curve == NULL)
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		ret = _ecc_params_to_pubkey(params, &pub, curve);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ecc_point_init(&r, curve);
		/* verify that x,y lie on the curve */
		ret = ecc_point_set(&r, TOMPZ(params->params[ECC_X]),
				    TOMPZ(params->params[ECC_Y]));
		if (ret == 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto ecc_cleanup;
		}
		ecc_point_clear(&r);

		ret = 0;

	ecc_cleanup:
		ecc_point_clear(&pub);
	} break;
#if ENABLE_GOST
	case GNUTLS_PK_GOST_01:
	case GNUTLS_PK_GOST_12_256:
	case GNUTLS_PK_GOST_12_512: {
		/* just verify that x and y lie on the curve */
		struct ecc_point r, pub;
		const struct ecc_curve *curve;

		if (params->params_nr != GOST_PUBLIC_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		curve = get_supported_gost_curve(params->curve);
		if (curve == NULL)
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		ret = _gost_params_to_pubkey(params, &pub, curve);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ecc_point_init(&r, curve);
		/* verify that x,y lie on the curve */
		ret = ecc_point_set(&r, TOMPZ(params->params[GOST_X]),
				    TOMPZ(params->params[GOST_Y]));
		if (ret == 0) {
			ret = gnutls_assert_val(GNUTLS_E_ILLEGAL_PARAMETER);
			goto gost_cleanup;
		}
		ecc_point_clear(&r);

		ret = 0;

	gost_cleanup:
		ecc_point_clear(&pub);
	} break;
#endif
	default:
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	return ret;
}

static int calc_rsa_exp(gnutls_pk_params_st *params)
{
	bigint_t tmp;
	int ret;

	if (params->params_nr < RSA_PRIVATE_PARAMS - 2) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	params->params[RSA_E1] = params->params[RSA_E2] = NULL;

	ret = _gnutls_mpi_init_multi(&tmp, &params->params[RSA_E1],
				     &params->params[RSA_E2], NULL);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* [6] = d % p-1, [7] = d % q-1 */
	_gnutls_mpi_sub_ui(tmp, params->params[RSA_PRIME1], 1);
	ret = _gnutls_mpi_modm(params->params[RSA_E1],
			       params->params[RSA_PRIV] /*d */, tmp);
	if (ret < 0)
		goto fail;

	_gnutls_mpi_sub_ui(tmp, params->params[RSA_PRIME2], 1);
	ret = _gnutls_mpi_modm(params->params[RSA_E2],
			       params->params[RSA_PRIV] /*d */, tmp);
	if (ret < 0)
		goto fail;

	zrelease_mpi_key(&tmp);

	return 0;

fail:
	zrelease_mpi_key(&tmp);
	zrelease_mpi_key(&params->params[RSA_E1]);
	zrelease_mpi_key(&params->params[RSA_E2]);

	return ret;
}

static int calc_rsa_priv(gnutls_pk_params_st *params)
{
	bigint_t lcm, p1, q1;
	int ret;

	params->params[RSA_PRIV] = NULL;

	ret = _gnutls_mpi_init_multi(&params->params[RSA_PRIV], &lcm, &p1, &q1,
				     NULL);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* lcm(p - 1, q - 1) */
	mpz_sub_ui(p1, params->params[RSA_PRIME1], 1);
	mpz_sub_ui(q1, params->params[RSA_PRIME2], 1);
	mpz_lcm(lcm, p1, q1);

	zrelease_mpi_key(&p1);
	zrelease_mpi_key(&q1);

	/* d = e^{-1} (mod lcm) */
	ret = mpz_invert(params->params[RSA_PRIV], params->params[RSA_PUB],
			 lcm);

	zrelease_mpi_key(&lcm);

	if (ret == 0) {
		zrelease_mpi_key(&params->params[RSA_PRIV]);
		return GNUTLS_E_INVALID_REQUEST;
	}

	return 0;
}

#ifdef ENABLE_DSA
static int calc_dsa_pub(gnutls_pk_params_st *params)
{
	int ret;

	params->params[DSA_Y] = NULL;

	ret = _gnutls_mpi_init(&params->params[DSA_Y]);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* y = g^x mod p */
	ret = _gnutls_mpi_powm(params->params[DSA_Y], params->params[DSA_G],
			       params->params[DSA_X], params->params[DSA_P]);
	if (ret < 0) {
		zrelease_mpi_key(&params->params[DSA_Y]);
		return gnutls_assert_val(ret);
	}

	return 0;
}
#endif

static int wrap_nettle_pk_fixup(gnutls_pk_algorithm_t algo,
				gnutls_direction_t direction,
				gnutls_pk_params_st *params)
{
	int ret;

	if (direction != GNUTLS_IMPORT)
		return 0;

	if (algo == GNUTLS_PK_RSA) {
		struct rsa_private_key priv;

		if (params->params[RSA_PRIV] == NULL) {
			ret = calc_rsa_priv(params);
			if (ret < 0)
				return gnutls_assert_val(ret);
			params->params_nr++;
		}

		/* do not trust the generated values. Some old private keys
		 * generated by us have mess on the values. Those were very
		 * old but it seemed some of the shipped example private
		 * keys were as old.
		 */
		if (params->params_nr < RSA_PRIVATE_PARAMS - 3)
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);

		if (params->params[RSA_COEF] == NULL) {
			ret = _gnutls_mpi_init(&params->params[RSA_COEF]);
			if (ret < 0)
				return gnutls_assert_val(ret);
		}

		if (mpz_cmp_ui(TOMPZ(params->params[RSA_PRIME1]), 0) == 0)
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);

		if (mpz_invert(TOMPZ(params->params[RSA_COEF]),
			       TOMPZ(params->params[RSA_PRIME2]),
			       TOMPZ(params->params[RSA_PRIME1])) == 0)
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);

		/* calculate exp1 [6] and exp2 [7] */
		zrelease_mpi_key(&params->params[RSA_E1]);
		zrelease_mpi_key(&params->params[RSA_E2]);

		/* marks RSA_COEF as present */
		params->params_nr = RSA_PRIVATE_PARAMS - 2;
		ret = calc_rsa_exp(params);
		if (ret < 0)
			return gnutls_assert_val(ret);

		params->params_nr = RSA_PRIVATE_PARAMS;

		/* perform nettle's internal checks */
		_rsa_params_to_privkey(params, &priv);
		ret = rsa_private_key_prepare(&priv);
		if (ret == 0) {
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);
		}
	} else if (algo == GNUTLS_PK_EDDSA_ED25519 ||
		   algo == GNUTLS_PK_EDDSA_ED448) {
		if (unlikely(get_eddsa_curve(algo) != params->curve))
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		if (params->raw_priv.data == NULL)
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);

		if (params->raw_pub.data == NULL) {
			params->raw_pub.data =
				gnutls_malloc(params->raw_priv.size);
		}

		if (params->raw_pub.data == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		ret = eddsa_public_key(algo, params->raw_pub.data,
				       params->raw_priv.data);
		if (ret < 0) {
			gnutls_free(params->raw_pub.data);
			return ret;
		}

		params->raw_pub.size = params->raw_priv.size;
	} else if (algo == GNUTLS_PK_ECDH_X25519 ||
		   algo == GNUTLS_PK_ECDH_X448) {
		if (unlikely(get_ecdh_curve(algo) != params->curve))
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		if (params->raw_priv.data == NULL)
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);

		if (params->raw_pub.data == NULL) {
			params->raw_pub.data =
				gnutls_malloc(params->raw_priv.size);
		}

		if (params->raw_pub.data == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		ret = edwards_curve_mul_g(algo, params->raw_pub.data,
					  params->raw_priv.data);
		if (ret < 0) {
			gnutls_free(params->raw_pub.data);
			return ret;
		}

		params->raw_pub.size = params->raw_priv.size;
	} else if (algo == GNUTLS_PK_RSA_PSS) {
		if (params->params_nr < RSA_PRIVATE_PARAMS - 3)
			return gnutls_assert_val(GNUTLS_E_PK_INVALID_PRIVKEY);

		if (params->spki.rsa_pss_dig != 0) {
			unsigned pub_size = nettle_mpz_sizeinbase_256_u(
				TOMPZ(params->params[RSA_MODULUS]));
			/* sanity check for private key */
			CHECK_INVALID_RSA_PSS_PARAMS(
				gnutls_hash_get_len(params->spki.rsa_pss_dig),
				params->spki.salt_size, pub_size,
				GNUTLS_E_PK_INVALID_PUBKEY_PARAMS);
		}
	}
#ifdef ENABLE_DSA
	else if (algo == GNUTLS_PK_DSA) {
		if (params->params[DSA_Y] == NULL) {
			ret = calc_dsa_pub(params);
			if (ret < 0)
				return gnutls_assert_val(ret);
			params->params_nr++;
		}
	}
#endif
#if ENABLE_GOST
	else if (algo == GNUTLS_PK_GOST_01 || algo == GNUTLS_PK_GOST_12_256 ||
		 algo == GNUTLS_PK_GOST_12_512) {
		struct ecc_point r;
		struct ecc_scalar priv;
		const struct ecc_curve *curve;

		if (params->params_nr != GOST_PRIVATE_PARAMS)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		curve = get_supported_gost_curve(params->curve);
		if (curve == NULL)
			return gnutls_assert_val(
				GNUTLS_E_ECC_UNSUPPORTED_CURVE);

		if (ecc_bit_size(curve) <
		    _gnutls_mpi_get_nbits(params->params[GOST_K]))
			gostdsa_unmask_key(curve,
					   TOMPZ(params->params[GOST_K]));

		ret = _gost_params_to_privkey(params, &priv, curve);
		if (ret < 0) {
			return gnutls_assert_val(ret);
		}

		ecc_point_init(&r, curve);
		gost_point_mul_g(&r, &priv);

		ecc_point_get(&r, params->params[GOST_X],
			      params->params[GOST_Y]);

		ecc_point_clear(&r);
		ecc_scalar_clear(&priv);
	}
#endif

	return 0;
}

int crypto_pk_prio = INT_MAX;

gnutls_crypto_pk_st _gnutls_pk_ops = {
	.encrypt = _wrap_nettle_pk_encrypt,
	.decrypt = _wrap_nettle_pk_decrypt,
	.decrypt2 = _wrap_nettle_pk_decrypt2,
	.sign = _wrap_nettle_pk_sign,
	.verify = _wrap_nettle_pk_verify,
	.verify_priv_params = wrap_nettle_pk_verify_priv_params,
	.verify_pub_params = wrap_nettle_pk_verify_pub_params,
	.generate_params = wrap_nettle_pk_generate_params,
	.generate_keys = wrap_nettle_pk_generate_keys,
	.pk_fixup_private_params = wrap_nettle_pk_fixup,
	.derive = _wrap_nettle_pk_derive,
	.encaps = _wrap_nettle_pk_encaps,
	.decaps = _wrap_nettle_pk_decaps,
	.curve_exists = _wrap_nettle_pk_curve_exists,
	.pk_exists = _wrap_nettle_pk_exists,
	.sign_exists = _wrap_nettle_pk_sign_exists
};
