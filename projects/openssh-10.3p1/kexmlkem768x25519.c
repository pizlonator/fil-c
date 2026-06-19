/* $OpenBSD: kexmlkem768x25519.c,v 1.2 2024/10/27 02:06:59 djm Exp $ */
/*
 * Copyright (c) 2023 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <endian.h>

#include "sshkey.h"
#include "kex.h"
#include "sshbuf.h"
#include "digest.h"
#include "ssherr.h"
#include "log.h"

#ifdef USE_MLKEM768X25519

#include "libcrux_mlkem768_sha3.h"
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <stdio.h>

#define FIPS_FALLBACK_PROPQ "provider=default,-fips"

static int
mlkem_keypair_gen(const char *algname, unsigned char *pubkeybuf, size_t pubkey_size,
	          unsigned char *privkeybuf, size_t privkey_size)
{
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    int ret = SSH_ERR_INTERNAL_ERROR;
    size_t got_pub_size = pubkey_size, got_priv_size = privkey_size;

    ctx = EVP_PKEY_CTX_new_from_name(NULL, algname, NULL);

    if (ctx == NULL && FIPS_mode()) {
	/* We have filtered x25519 + ML-KEM in FIPS mode earlier
	 * so if we are in FIPS mode and ML-KEM is not available with default propq,
	 * we can fetch it from the default provider */
        ctx = EVP_PKEY_CTX_new_from_name(NULL, algname, FIPS_FALLBACK_PROPQ);
    }

    if (ctx == NULL) {
	ret = SSH_ERR_LIBCRYPTO_ERROR;
	goto err;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0
        || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
	ret = SSH_ERR_LIBCRYPTO_ERROR;
        goto err;
    }

    if (EVP_PKEY_get_raw_public_key(pkey, NULL, &got_pub_size) <= 0
	|| EVP_PKEY_get_raw_private_key(pkey, NULL, &got_priv_size) <= 0) {
	ret = SSH_ERR_LIBCRYPTO_ERROR;
        goto err;
    }

    if (privkey_size != got_priv_size || pubkey_size != got_pub_size) {
	ret = SSH_ERR_LIBCRYPTO_ERROR;
        goto err;
    }

    if (EVP_PKEY_get_raw_public_key(pkey, pubkeybuf, &got_pub_size) <= 0
	|| EVP_PKEY_get_raw_private_key(pkey, privkeybuf, &got_priv_size) <= 0) {
	ret = SSH_ERR_LIBCRYPTO_ERROR;
        goto err;
    }

    ret = 0;

 err:
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    if (ret == SSH_ERR_LIBCRYPTO_ERROR)
	   ERR_print_errors_fp(stderr);
    return ret;
}

static int
mlkem768_keypair_gen(unsigned char *pubkeybuf, unsigned char *privkeybuf)
{
	return mlkem_keypair_gen("mlkem768", pubkeybuf, crypto_kem_mlkem768_PUBLICKEYBYTES,
			privkeybuf, crypto_kem_mlkem768_SECRETKEYBYTES);
}

static int
mlkem1024_keypair_gen(unsigned char *pubkeybuf, unsigned char *privkeybuf)
{
	return mlkem_keypair_gen("mlkem1024", pubkeybuf, crypto_kem_mlkem1024_PUBLICKEYBYTES,
			privkeybuf, crypto_kem_mlkem1024_SECRETKEYBYTES);
}

static int
mlkem_encap_secret(const char *mlkem_alg, const u_char *pubkeybuf, u_char *secret, u_char *out)
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    int r = SSH_ERR_INTERNAL_ERROR;
    size_t outlen, expected_outlen, publen, secretlen = crypto_kem_mlkem768_BYTES;
    int fips_fallback = 0;

    if (strcmp(mlkem_alg, "mlkem768") == 0) {
	    outlen = crypto_kem_mlkem768_CIPHERTEXTBYTES;
	    publen = crypto_kem_mlkem768_PUBLICKEYBYTES;
    } else if (strcmp(mlkem_alg, "mlkem1024") == 0) {
	    outlen = crypto_kem_mlkem1024_CIPHERTEXTBYTES;
	    publen = crypto_kem_mlkem1024_PUBLICKEYBYTES;
    } else
	    return r;

    expected_outlen = outlen;

    pkey = EVP_PKEY_new_raw_public_key_ex(NULL, mlkem_alg, NULL,
		    pubkeybuf, publen);
    if (pkey == NULL && FIPS_mode()) {
        pkey = EVP_PKEY_new_raw_public_key_ex(NULL, mlkem_alg, FIPS_FALLBACK_PROPQ,
		    pubkeybuf, publen);
	fips_fallback = 1;
    }
    if (pkey == NULL) {
	r = SSH_ERR_LIBCRYPTO_ERROR;
	goto err;
    }

    ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, fips_fallback ? FIPS_FALLBACK_PROPQ : NULL);
    if (ctx == NULL
	|| EVP_PKEY_encapsulate_init(ctx, NULL) <= 0
        || EVP_PKEY_encapsulate(ctx, out, &expected_outlen, secret, &secretlen) <= 0
	|| secretlen != crypto_kem_mlkem768_BYTES
	|| outlen != expected_outlen) {
	r = SSH_ERR_LIBCRYPTO_ERROR;
	goto err;
    }
    r = 0;

 err:
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    if (r == SSH_ERR_LIBCRYPTO_ERROR)
	   ERR_print_errors_fp(stderr);

    return r;
}

static int
mlkem768_encap_secret(const u_char *pubkeybuf, u_char *secret, u_char *out)
{
	return mlkem_encap_secret("mlkem768", pubkeybuf, secret, out);
}

static int
mlkem1024_encap_secret(const u_char *pubkeybuf, u_char *secret, u_char *out)
{
	return mlkem_encap_secret("mlkem1024", pubkeybuf, secret, out);
}

static int
mlkem_decap_secret(const char *algname,
    const u_char *privkeybuf, size_t privkey_len,
    const u_char *wrapped, size_t wrapped_len,
    u_char *secret)
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    int r = SSH_ERR_INTERNAL_ERROR;
    size_t secretlen = crypto_kem_mlkem768_BYTES;
    int fips_fallback = 0;

    pkey = EVP_PKEY_new_raw_private_key_ex(NULL, algname,
		    NULL, privkeybuf, privkey_len);
    if (pkey == NULL && FIPS_mode()) {
        pkey = EVP_PKEY_new_raw_private_key_ex(NULL, algname,
		    FIPS_FALLBACK_PROPQ, privkeybuf, privkey_len);
	fips_fallback = 1;
    }
    if (pkey == NULL) {
	r = SSH_ERR_LIBCRYPTO_ERROR;
	goto err;
    }

    ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, fips_fallback ? FIPS_FALLBACK_PROPQ : NULL);
    if (ctx == NULL
	|| EVP_PKEY_decapsulate_init(ctx, NULL) <= 0
        || EVP_PKEY_decapsulate(ctx, secret, &secretlen, wrapped, wrapped_len) <= 0
	|| secretlen != crypto_kem_mlkem768_BYTES) {
	r = SSH_ERR_LIBCRYPTO_ERROR;
	goto err;
    }
    r = 0;

 err:
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);

    if (r == SSH_ERR_LIBCRYPTO_ERROR)
	   ERR_print_errors_fp(stderr);

    return r;
}

static int
mlkem768_decap_secret(const u_char *privkeybuf, const u_char *wrapped, u_char *secret)
{
	return mlkem_decap_secret("mlkem768", privkeybuf, crypto_kem_mlkem768_SECRETKEYBYTES,
		wrapped, crypto_kem_mlkem768_CIPHERTEXTBYTES, secret);
}

static int
mlkem1024_decap_secret(const u_char *privkeybuf, const u_char *wrapped, u_char *secret)
{
	return mlkem_decap_secret("mlkem1024", privkeybuf, crypto_kem_mlkem1024_SECRETKEYBYTES,
		wrapped, crypto_kem_mlkem1024_CIPHERTEXTBYTES, secret);
}

int
kex_kem_mlkem768x25519_keypair(struct kex *kex)
{
#if 0
	struct sshbuf *buf = NULL;
	u_char rnd[LIBCRUX_ML_KEM_KEY_PAIR_PRNG_LEN], *cp = NULL;
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct libcrux_mlkem768_keypair keypair;

	if ((buf = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	need = crypto_kem_mlkem768_PUBLICKEYBYTES + CURVE25519_SIZE;
	if ((r = sshbuf_reserve(buf, need, &cp)) != 0)
		goto out;
	arc4random_buf(rnd, sizeof(rnd));
	keypair = libcrux_ml_kem_mlkem768_portable_generate_key_pair(rnd);
	memcpy(cp, keypair.pk.value, crypto_kem_mlkem768_PUBLICKEYBYTES);
	memcpy(kex->mlkem768_client_key, keypair.sk.value,
	    sizeof(kex->mlkem768_client_key));
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem768:", cp,
	    crypto_kem_mlkem768_PUBLICKEYBYTES);
#endif
	cp += crypto_kem_mlkem768_PUBLICKEYBYTES;
	kexc25519_keygen(kex->c25519_client_key, cp);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key c25519:", cp, CURVE25519_SIZE);
#endif
	/* success */
	r = 0;
	kex->client_pub = buf;
	buf = NULL;
 out:
	explicit_bzero(&keypair, sizeof(keypair));
	explicit_bzero(rnd, sizeof(rnd));
	sshbuf_free(buf);
	return r;
#else
	struct sshbuf *buf = NULL;
	u_char *cp = NULL;
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;

	if ((buf = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	need = crypto_kem_mlkem768_PUBLICKEYBYTES + CURVE25519_SIZE;
	if ((r = sshbuf_reserve(buf, need, &cp)) != 0)
		goto out;
	if ((r = mlkem768_keypair_gen(cp, kex->mlkem768_client_key)) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem768:", cp,
	    crypto_kem_mlkem768_PUBLICKEYBYTES);
#endif
	cp += crypto_kem_mlkem768_PUBLICKEYBYTES;
	kexc25519_keygen(kex->c25519_client_key, cp);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key c25519:", cp, CURVE25519_SIZE);
#endif
	/* success */
	r = 0;
	kex->client_pub = buf;
	buf = NULL;
 out:
	sshbuf_free(buf);
	return r;
#endif
}

int
kex_kem_mlkem768x25519_enc(struct kex *kex,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
#if 0
	struct sshbuf *server_blob = NULL;
	struct sshbuf *buf = NULL;
	const u_char *client_pub;
	u_char rnd[LIBCRUX_ML_KEM_ENC_PRNG_LEN];
	u_char server_pub[CURVE25519_SIZE], server_key[CURVE25519_SIZE];
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct libcrux_mlkem768_enc_result enc;
	struct libcrux_mlkem768_pk mlkem_pub;

	*server_blobp = NULL;
	*shared_secretp = NULL;
	memset(&mlkem_pub, 0, sizeof(mlkem_pub));

	/* client_blob contains both KEM and ECDH client pubkeys */
	need = crypto_kem_mlkem768_PUBLICKEYBYTES + CURVE25519_SIZE;
	if (sshbuf_len(client_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	client_pub = sshbuf_ptr(client_blob);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem768:", client_pub,
	    crypto_kem_mlkem768_PUBLICKEYBYTES);
	dump_digest("client public key 25519:",
	    client_pub + crypto_kem_mlkem768_PUBLICKEYBYTES,
	    CURVE25519_SIZE);
#endif
	/* check public key validity */
	memcpy(mlkem_pub.value, client_pub, crypto_kem_mlkem768_PUBLICKEYBYTES);
	if (!libcrux_ml_kem_mlkem768_portable_validate_public_key(&mlkem_pub)) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}

	/* allocate buffer for concatenation of KEM key and ECDH shared key */
	/* the buffer will be hashed and the result is the shared secret */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* allocate space for encrypted KEM key and ECDH pub key */
	if ((server_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* generate and encrypt KEM key with client key */
	arc4random_buf(rnd, sizeof(rnd));
	enc = libcrux_ml_kem_mlkem768_portable_encapsulate(&mlkem_pub, rnd);
	/* generate ECDH key pair, store server pubkey after ciphertext */
	kexc25519_keygen(server_key, server_pub);
	if ((r = sshbuf_put(buf, enc.snd, sizeof(enc.snd))) != 0 ||
	    (r = sshbuf_put(server_blob, enc.fst.value, sizeof(enc.fst.value))) != 0 ||
	    (r = sshbuf_put(server_blob, server_pub, sizeof(server_pub))) != 0)
		goto out;
	/* append ECDH shared key */
	client_pub += crypto_kem_mlkem768_PUBLICKEYBYTES;
	if ((r = kexc25519_shared_key_ext(server_key, client_pub, buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf, hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("server public key 25519:", server_pub, CURVE25519_SIZE);
	dump_digest("server cipher text:",
	    enc.fst.value, sizeof(enc.fst.value));
	dump_digest("server kem key:", enc.snd, sizeof(enc.snd));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* string-encoded hash is resulting shared secret */
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*server_blobp = server_blob;
	*shared_secretp = buf;
	server_blob = NULL;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(server_key, sizeof(server_key));
	explicit_bzero(rnd, sizeof(rnd));
	explicit_bzero(&enc, sizeof(enc));
	sshbuf_free(server_blob);
	sshbuf_free(buf);
	return r;
#else
	struct sshbuf *server_blob = NULL;
	struct sshbuf *buf = NULL;
	const u_char *client_pub;
	u_char server_pub[CURVE25519_SIZE], server_key[CURVE25519_SIZE];
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct libcrux_mlkem768_enc_result enc;

	*server_blobp = NULL;
	*shared_secretp = NULL;

	/* client_blob contains both KEM and ECDH client pubkeys */
	need = crypto_kem_mlkem768_PUBLICKEYBYTES + CURVE25519_SIZE;
	if (sshbuf_len(client_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	client_pub = sshbuf_ptr(client_blob);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem768:", client_pub,
	    crypto_kem_mlkem768_PUBLICKEYBYTES);
	dump_digest("client public key 25519:",
	    client_pub + crypto_kem_mlkem768_PUBLICKEYBYTES,
	    CURVE25519_SIZE);
#endif

	/* allocate buffer for concatenation of KEM key and ECDH shared key */
	/* the buffer will be hashed and the result is the shared secret */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* allocate space for encrypted KEM key and ECDH pub key */
	if ((server_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (mlkem768_encap_secret(client_pub, enc.snd, enc.fst.value) != 0)
		goto out;

	/* generate ECDH key pair, store server pubkey after ciphertext */
	kexc25519_keygen(server_key, server_pub);
	if ((r = sshbuf_put(buf, enc.snd, sizeof(enc.snd))) != 0 ||
	    (r = sshbuf_put(server_blob, enc.fst.value, sizeof(enc.fst.value))) != 0 ||
	    (r = sshbuf_put(server_blob, server_pub, sizeof(server_pub))) != 0)
		goto out;
	/* append ECDH shared key */
	client_pub += crypto_kem_mlkem768_PUBLICKEYBYTES;
	if ((r = kexc25519_shared_key_ext(server_key, client_pub, buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf, hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("server public key 25519:", server_pub, CURVE25519_SIZE);
	dump_digest("server cipher text:",
	    enc.fst.value, sizeof(enc.fst.value));
	dump_digest("server kem key:", enc.snd, sizeof(enc.snd));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* string-encoded hash is resulting shared secret */
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*server_blobp = server_blob;
	*shared_secretp = buf;
	server_blob = NULL;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(server_key, sizeof(server_key));
	explicit_bzero(&enc, sizeof(enc));
	sshbuf_free(server_blob);
	sshbuf_free(buf);
	return r;
#endif
}

int
kex_kem_mlkem768x25519_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp)
{
#if 0
	struct sshbuf *buf = NULL;
	u_char mlkem_key[crypto_kem_mlkem768_BYTES];
	const u_char *ciphertext, *server_pub;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r;
	struct libcrux_mlkem768_sk mlkem_priv;
	struct libcrux_mlkem768_ciphertext mlkem_ciphertext;

	*shared_secretp = NULL;
	memset(&mlkem_priv, 0, sizeof(mlkem_priv));
	memset(&mlkem_ciphertext, 0, sizeof(mlkem_ciphertext));

	need = crypto_kem_mlkem768_CIPHERTEXTBYTES + CURVE25519_SIZE;
	if (sshbuf_len(server_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	ciphertext = sshbuf_ptr(server_blob);
	server_pub = ciphertext + crypto_kem_mlkem768_CIPHERTEXTBYTES;
	/* hash concatenation of KEM key and ECDH shared key */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	memcpy(mlkem_priv.value, kex->mlkem768_client_key,
	    sizeof(kex->mlkem768_client_key));
	memcpy(mlkem_ciphertext.value, ciphertext,
	    sizeof(mlkem_ciphertext.value));
#ifdef DEBUG_KEXECDH
	dump_digest("server cipher text:", mlkem_ciphertext.value,
	    sizeof(mlkem_ciphertext.value));
	dump_digest("server public key c25519:", server_pub, CURVE25519_SIZE);
#endif
	libcrux_ml_kem_mlkem768_portable_decapsulate(&mlkem_priv,
	    &mlkem_ciphertext, mlkem_key);
	if ((r = sshbuf_put(buf, mlkem_key, sizeof(mlkem_key))) != 0)
		goto out;
	if ((r = kexc25519_shared_key_ext(kex->c25519_client_key, server_pub,
	    buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf,
	    hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client kem key:", mlkem_key, sizeof(mlkem_key));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*shared_secretp = buf;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(&mlkem_priv, sizeof(mlkem_priv));
	explicit_bzero(&mlkem_ciphertext, sizeof(mlkem_ciphertext));
	explicit_bzero(mlkem_key, sizeof(mlkem_key));
	sshbuf_free(buf);
	return r;
#else
	struct sshbuf *buf = NULL;
	const u_char *ciphertext, *server_pub;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	u_char decap[crypto_kem_mlkem768_BYTES];
	size_t need;
	int r;

	*shared_secretp = NULL;

	need = crypto_kem_mlkem768_CIPHERTEXTBYTES + CURVE25519_SIZE;
	if (sshbuf_len(server_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	ciphertext = sshbuf_ptr(server_blob);
	server_pub = ciphertext + crypto_kem_mlkem768_CIPHERTEXTBYTES;
	/* hash concatenation of KEM key and ECDH shared key */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
#ifdef DEBUG_KEXECDH
	dump_digest("server cipher text:", ciphertext, crypto_kem_mlkem768_CIPHERTEXTBYTES);
	dump_digest("server public key c25519:", server_pub, CURVE25519_SIZE);
#endif
	if ((r = mlkem768_decap_secret(kex->mlkem768_client_key, ciphertext, decap)) != 0)
		goto out;
	if ((r = sshbuf_put(buf, decap, sizeof(decap))) != 0)
		goto out;
	if ((r = kexc25519_shared_key_ext(kex->c25519_client_key, server_pub,
	    buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf,
	    hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client kem key:", decap, sizeof(decap));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*shared_secretp = buf;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(decap, sizeof(decap));
	sshbuf_free(buf);
	return r;
#endif
}

#define NIST_P256_COMPRESSED_LEN   33
#define NIST_P256_UNCOMPRESSED_LEN 65
#define NIST_P384_COMPRESSED_LEN   49
#define NIST_P384_UNCOMPRESSED_LEN 97
#define NIST_BUF_MAX_SIZE NIST_P384_UNCOMPRESSED_LEN

static const char ec256[] = "P-256";
static const char ec384[] = "P-384";
static const char *len2curve_name(size_t len)
{
	switch (len) {
		case NIST_P256_COMPRESSED_LEN:
		case NIST_P256_UNCOMPRESSED_LEN:
			return ec256;
			break;
		case NIST_P384_COMPRESSED_LEN:
		case NIST_P384_UNCOMPRESSED_LEN:
			return ec384;
			break;
	}
	return NULL;
}

static EVP_PKEY *
buf2nist_key(const unsigned char *pub_key_buf, size_t pub_key_len)
{
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	OSSL_PARAM params[3];
	const char *curve_name = len2curve_name(pub_key_len);

	if (curve_name == NULL)
		return NULL;

	ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!ctx)
		goto err;

	if (EVP_PKEY_fromdata_init(ctx) <= 0)
		goto err;

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, curve_name, 0);
	params[1] = OSSL_PARAM_construct_octet_string(
			OSSL_PKEY_PARAM_PUB_KEY, (void *)pub_key_buf, pub_key_len);
	params[2] = OSSL_PARAM_construct_end();

	if (EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
		goto err;

	EVP_PKEY_CTX_free(ctx);
	return pkey;

err:
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	return NULL;
}

static int
kex_nist_shared_key_ext(EVP_PKEY *priv_key,
    const u_char *pub_key_buf, size_t pub_key_len, struct sshbuf *out)
{
	EVP_PKEY_CTX *ctx = NULL;
	unsigned char *shared_secret = NULL;
	size_t shared_secret_len = 0;
	EVP_PKEY *peer_key = buf2nist_key(pub_key_buf, pub_key_len);
	int r = SSH_ERR_INTERNAL_ERROR;

	if (peer_key == NULL)
		return SSH_ERR_KEY_LENGTH;

	ctx = EVP_PKEY_CTX_new_from_pkey(NULL, priv_key, NULL);
	if (!ctx)
		goto end;

	if ((EVP_PKEY_derive_init(ctx) <= 0)
		|| EVP_PKEY_derive_set_peer(ctx, peer_key) <= 0
		|| EVP_PKEY_derive(ctx, NULL, &shared_secret_len) <= 0)
		goto end;

	shared_secret = OPENSSL_malloc(shared_secret_len);
	if (shared_secret == NULL)
		goto end;

	if (EVP_PKEY_derive(ctx, shared_secret, &shared_secret_len) <= 0)
		goto end;

	if ((r = sshbuf_put(out, shared_secret, shared_secret_len)) != 0)
		goto end;

	r = 0;

end:
	EVP_PKEY_free(peer_key);
	if (shared_secret)
		OPENSSL_clear_free(shared_secret, shared_secret_len);
	EVP_PKEY_CTX_free(ctx);

	return r;
}

static EVP_PKEY *
nist_pkey_keygen(size_t pub_key_len)
{
	const char *curve_name = len2curve_name(pub_key_len);
	EVP_PKEY_CTX *pctx = NULL;
	EVP_PKEY *pkey = NULL;
	OSSL_PARAM params[2];

	if (curve_name == NULL)
		return NULL;

	pctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!pctx)
		return NULL;

	if (EVP_PKEY_keygen_init(pctx) <= 0) {
		EVP_PKEY_CTX_free(pctx);
		return NULL;
	}

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, curve_name, 0);
	params[1] = OSSL_PARAM_construct_end();

	if (EVP_PKEY_CTX_set_params(pctx, params) <= 0
	    || EVP_PKEY_keygen(pctx, &pkey) <= 0) {
		EVP_PKEY_CTX_free(pctx);
		return NULL;
	}

	EVP_PKEY_CTX_free(pctx);
	return pkey;
}

static size_t decompress_pub_key(void *pub, size_t compressed_len, size_t decompressed_len)
{
    EC_GROUP *group = NULL;
    EC_POINT *point = NULL;
    BN_CTX *ctx = NULL;
    size_t len = 0;
    int group_nid = NID_undef;

    switch (compressed_len) {
    case NIST_P256_COMPRESSED_LEN:
         group_nid = NID_X9_62_prime256v1;
       break;
    case NIST_P384_COMPRESSED_LEN:
         group_nid = NID_secp384r1;
       break;
    default:
       return 0;
       break;
    }

    ctx = BN_CTX_new();
    group = EC_GROUP_new_by_curve_name(group_nid);
    if (ctx == NULL || group == NULL)
        goto err;

    point = EC_POINT_new(group);
    if (point == NULL)
        goto err;

    if (!EC_POINT_oct2point(group, point, pub, compressed_len, ctx))
        goto err;

    len = EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED, pub, decompressed_len, ctx);

err:
    EC_POINT_free(point);
    EC_GROUP_free(group);
    BN_CTX_free(ctx);

    return len;
}

static int
get_uncompressed_ec_pubkey(EVP_PKEY *pkey, unsigned char *buf, size_t buf_len)
{
    OSSL_PARAM params[2];
    size_t required_len = 0, out_len = 0;

    params[0] = OSSL_PARAM_construct_utf8_string(
        OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT,
        "uncompressed", 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_set_params(pkey, params) <= 0
	    || EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                          buf, buf_len, &required_len) <= 0) {
        return SSH_ERR_LIBCRYPTO_ERROR;
    }

    if (required_len != buf_len) {
        /* Red Hat certified FIPS provider ignores OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT
	 * We may have to perform the conversion manually */
        if (len2curve_name(required_len) == len2curve_name(buf_len)) {
	    out_len = decompress_pub_key(buf, required_len, buf_len);
	    if (out_len != buf_len) {
	        debug_f("Error decompressing the compressed public key");
	        return SSH_ERR_LIBCRYPTO_ERROR;
	    } else {
		return 0;
	    }
	} else {
	    debug_f("Unexpected length of uncompressed public key: expected %d, got %d", buf_len, required_len);
	    return SSH_ERR_LIBCRYPTO_ERROR;
	}
    }

    return 0;
}
/* nist_bytes_len should always be uncompressed */
static int
kex_kem_mlkem_nist_keypair(struct kex *kex, size_t mlkem_bytes_len, size_t nist_bytes_len)
{
	struct sshbuf *buf = NULL;
	u_char *cp = NULL;
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;
	u_char *client_key = NULL;

	if ((buf = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	need = mlkem_bytes_len + nist_bytes_len;
	if ((r = sshbuf_reserve(buf, need, &cp)) != 0)
		goto out;

	if (mlkem_bytes_len == crypto_kem_mlkem768_PUBLICKEYBYTES) {
		client_key = kex->mlkem768_client_key;
		r = mlkem768_keypair_gen(cp, client_key);
	}

	if (mlkem_bytes_len == crypto_kem_mlkem1024_PUBLICKEYBYTES) {
		client_key = kex->mlkem1024_client_key;
		r = mlkem1024_keypair_gen(cp, client_key);
	}

	if (client_key == NULL)
		goto out;

	if (r != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkemXXX:", cp, mlkem_bytes_len);
#endif
	cp += mlkem_bytes_len;
	if ((kex->ec_hybrid_client_key = nist_pkey_keygen(nist_bytes_len)) == NULL)
		goto out;

	if ((r = get_uncompressed_ec_pubkey(kex->ec_hybrid_client_key, cp, nist_bytes_len)) != 0)
		goto out;

#ifdef DEBUG_KEXECDH
	dump_digest("client public key NIST:", cp, nist_bytes_len);
#endif
	/* success */
	r = 0;
	kex->client_pub = buf;
	buf = NULL;
 out:
	sshbuf_free(buf);
        if (r == SSH_ERR_LIBCRYPTO_ERROR)
	   ERR_print_errors_fp(stderr);

	return r;
}

static int
kex_kem_mlkem_nist_enc(struct kex *kex, const char *nist_curve,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
	struct sshbuf *server_blob = NULL;
	struct sshbuf *buf = NULL;
	const u_char *client_pub;
	u_char server_pub[NIST_BUF_MAX_SIZE];
	u_char enc_out[crypto_kem_mlkem1024_CIPHERTEXTBYTES];
	u_char secret[crypto_kem_mlkem768_BYTES];
	EVP_PKEY *server_key = NULL;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t client_buf_len, mlkem_buf_len, ecdh_buf_len, server_key_len, enc_out_len;
	int r = SSH_ERR_INTERNAL_ERROR;

	*server_blobp = NULL;
	*shared_secretp = NULL;

	client_buf_len = sshbuf_len(client_blob);
	/* client_blob contains both KEM and ECDH client pubkeys */
	if (strcmp(nist_curve, "P-256") == 0) {
		if (crypto_kem_mlkem768_PUBLICKEYBYTES > client_buf_len)
			return r;

		ecdh_buf_len = client_buf_len - crypto_kem_mlkem768_PUBLICKEYBYTES;
		if (ecdh_buf_len != NIST_P256_COMPRESSED_LEN &&
			ecdh_buf_len != NIST_P256_UNCOMPRESSED_LEN)
			return r;
		mlkem_buf_len = crypto_kem_mlkem768_PUBLICKEYBYTES;
		enc_out_len = crypto_kem_mlkem768_CIPHERTEXTBYTES;
		server_key_len = NIST_P256_UNCOMPRESSED_LEN;
	} else if (strcmp(nist_curve, "P-384") == 0) {
		if (crypto_kem_mlkem1024_PUBLICKEYBYTES > client_buf_len)
			return r;

		ecdh_buf_len = client_buf_len - crypto_kem_mlkem1024_PUBLICKEYBYTES;
		if (ecdh_buf_len != NIST_P384_COMPRESSED_LEN &&
			ecdh_buf_len != NIST_P384_UNCOMPRESSED_LEN)
			return r;
		mlkem_buf_len = crypto_kem_mlkem1024_PUBLICKEYBYTES;
		enc_out_len = crypto_kem_mlkem1024_CIPHERTEXTBYTES;
		server_key_len = NIST_P384_UNCOMPRESSED_LEN;
	} else
		return r;

	client_pub = sshbuf_ptr(client_blob);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem:", client_pub, mlkem_buf_len);
	dump_digest("client public key NIST:", client_pub + mlkem_buf_len, ecdh_buf_len);
#endif

	/* allocate buffer for concatenation of KEM key and ECDH shared key */
	/* the buffer will be hashed and the result is the shared secret */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* allocate space for encrypted KEM key and ECDH pub key */
	if ((server_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	r = (mlkem_buf_len == crypto_kem_mlkem768_PUBLICKEYBYTES) ?
		mlkem768_encap_secret(client_pub, secret, enc_out) :
		mlkem1024_encap_secret(client_pub, secret, enc_out);

	if (r != 0)
		goto out;

	/* generate ECDH key pair, store server pubkey after ciphertext */
	server_key = nist_pkey_keygen(server_key_len);

	if ((r = get_uncompressed_ec_pubkey(server_key, server_pub, server_key_len) != 0) ||
	    (r = sshbuf_put(buf, secret, sizeof(secret))) != 0 ||
	    (r = sshbuf_put(server_blob, enc_out, enc_out_len) != 0)||
	    (r = sshbuf_put(server_blob, server_pub, server_key_len)) != 0)
		goto out;

	/* append ECDH shared key */
	client_pub += mlkem_buf_len;
	if ((r = kex_nist_shared_key_ext(server_key, client_pub, ecdh_buf_len, buf)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf, hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("server public NIST:", server_pub, server_key_len);
	dump_digest("server cipher text:", enc_out, enc_out_len);
	dump_digest("server kem key:", secret, sizeof(secret));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* string-encoded hash is resulting shared secret */
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*server_blobp = server_blob;
	*shared_secretp = buf;
	server_blob = NULL;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	EVP_PKEY_free(server_key);
	explicit_bzero(enc_out, sizeof(enc_out));
	explicit_bzero(secret, sizeof(secret));
	sshbuf_free(server_blob);
	sshbuf_free(buf);
	return r;
}

static int
kex_kem_mlkem_nist_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp,
    size_t mlkem_len)
{
	struct sshbuf *buf = NULL;
	const u_char *ciphertext, *server_pub;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	u_char decap[crypto_kem_mlkem768_BYTES];
	int r;
	size_t nist_len;

	*shared_secretp = NULL;

	if (sshbuf_len(server_blob) < mlkem_len) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}

	nist_len = sshbuf_len(server_blob) - mlkem_len;

	switch (mlkem_len) {
		case crypto_kem_mlkem768_CIPHERTEXTBYTES:
			if (nist_len != NIST_P256_COMPRESSED_LEN
				&& nist_len != NIST_P256_UNCOMPRESSED_LEN) {
				r = SSH_ERR_SIGNATURE_INVALID;
				goto out;
			}
		break;
		case crypto_kem_mlkem1024_CIPHERTEXTBYTES:
			if (nist_len != NIST_P384_COMPRESSED_LEN
				&& nist_len != NIST_P384_UNCOMPRESSED_LEN) {
				r = SSH_ERR_SIGNATURE_INVALID;
				goto out;
			}
		break;
	}

	ciphertext = sshbuf_ptr(server_blob);
	server_pub = ciphertext + mlkem_len;
	/* hash concatenation of KEM key and ECDH shared key */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
#ifdef DEBUG_KEXECDH
	dump_digest("server cipher text:", ciphertext, mlkem_len);
	dump_digest("server public key NIST:", server_pub, nist_len);
#endif
	r = (mlkem_len == crypto_kem_mlkem768_CIPHERTEXTBYTES) ?
		mlkem768_decap_secret(kex->mlkem768_client_key, ciphertext, decap) :
		mlkem1024_decap_secret(kex->mlkem1024_client_key, ciphertext, decap);

	if (r != 0)
		goto out;
	if ((r = sshbuf_put(buf, decap, sizeof(decap))) != 0)
		goto out;
	if ((r = kex_nist_shared_key_ext(kex->ec_hybrid_client_key, server_pub,
		nist_len, buf)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf,
	    hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client kem key:", decap, sizeof(decap));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*shared_secretp = buf;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(decap, sizeof(decap));
	sshbuf_free(buf);
	return r;
}

int
kex_kem_mlkem768nistp256_keypair(struct kex *kex)
{
	return kex_kem_mlkem_nist_keypair(kex, crypto_kem_mlkem768_PUBLICKEYBYTES, NIST_P256_UNCOMPRESSED_LEN);
}

int
kex_kem_mlkem768nistp256_enc(struct kex *kex, const struct sshbuf *client_blob,
    struct sshbuf **server_blobp, struct sshbuf **shared_secretp)
{
	return kex_kem_mlkem_nist_enc(kex, "P-256", client_blob, server_blobp, shared_secretp);
}

int
kex_kem_mlkem768nistp256_dec(struct kex *kex, const struct sshbuf *server_blob,
    struct sshbuf **shared_secretp)
{
	return kex_kem_mlkem_nist_dec(kex, server_blob, shared_secretp,
		crypto_kem_mlkem768_CIPHERTEXTBYTES);
}

int
kex_kem_mlkem1024nistp384_keypair(struct kex *kex)
{
	return kex_kem_mlkem_nist_keypair(kex, crypto_kem_mlkem1024_PUBLICKEYBYTES, NIST_P384_UNCOMPRESSED_LEN);
}

int
kex_kem_mlkem1024nistp384_enc(struct kex *kex, const struct sshbuf *client_blob,
    struct sshbuf **server_blobp, struct sshbuf **shared_secretp)
{
	return kex_kem_mlkem_nist_enc(kex, "P-384", client_blob, server_blobp, shared_secretp);
}

int
kex_kem_mlkem1024nistp384_dec(struct kex *kex, const struct sshbuf *server_blob,
    struct sshbuf **shared_secretp)
{
	return kex_kem_mlkem_nist_dec(kex, server_blob, shared_secretp,
		crypto_kem_mlkem1024_CIPHERTEXTBYTES);
}

#else /* USE_MLKEM768X25519 */
int
kex_kem_mlkem768x25519_keypair(struct kex *kex)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int
kex_kem_mlkem768x25519_enc(struct kex *kex,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int
kex_kem_mlkem768x25519_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int	 kex_kem_mlkem768nistp256_keypair(struct kex *)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int	 kex_kem_mlkem768nistp256_enc(struct kex *, const struct sshbuf *,
    struct sshbuf **, struct sshbuf **)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int	 kex_kem_mlkem768nistp256_dec(struct kex *, const struct sshbuf *,
    struct sshbuf **)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int	 kex_kem_mlkem1024nistp384_keypair(struct kex *)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int	 kex_kem_mlkem1024nistp384_enc(struct kex *, const struct sshbuf *,
    struct sshbuf **, struct sshbuf **)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int	 kex_kem_mlkem1024nistp384_dec(struct kex *, const struct sshbuf *,
    struct sshbuf **)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

#endif /* USE_MLKEM768X25519 */
