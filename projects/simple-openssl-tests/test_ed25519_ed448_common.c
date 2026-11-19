/*
 * test_ed25519_ed448_common.c -- shared implementation of the Ed25519/Ed448
 * (PureEdDSA) family tests (API in test_ed25519_ed448_common.h; one binary
 * per algorithm: test_ed25519.c, which also carries the Ed25519ctx and
 * Ed25519ph instances, and test_ed448.c).
 *
 * Known-answer vectors, taken verbatim from RFC 8032 and verified with
 * filcc-compiled scratch probes (and the pizfix CLI for TEST 1) before
 * being embedded in the per-binary KAT rows (the per-binary comments say
 * which sections they carry).  PureEdDSA is deterministic, so signing the
 * same message with the same key must reproduce the RFC signature
 * byte-for-byte; the tests check both directions (our sign == RFC sig, and
 * the RFC sig verifies).
 *
 * The instance/context parameters ("instance", "context-string") only
 * exist from OpenSSL 3.2 on; on 3.0 the corresponding subtests are skipped
 * with a note (the plain Ed25519 / Ed448 KATs are version-independent).
 * The gate is a scan of EVP_PKEY_CTX_settable_params for the parameter
 * name -- ed25519_ed448_ctx_accepts() below -- exactly as in the monolith.
 *
 * The Ed25519 keys/signatures were also cross-checked against the CLI
 * during development (PEM files under /tmp + `openssl pkeyutl -verify
 * -pubin -inkey pub.pem -sigfile sig.bin -rawin -in msg.bin`); that is a
 * development-time check only -- the suite's programs do no file I/O.
 */

#include "test_ed25519_ed448_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* Imports an EdDSA key pair (32-byte Ed25519 / 57-byte Ed448 seed plus the
 * matching public key).  Buffers are static: the key references them. */
EVP_PKEY *ed25519_ed448_make_key(const char *alg, const unsigned char *seed,
                                 size_t seedlen, const unsigned char *pub,
                                 size_t publen)
{
    static unsigned char *sbuf, *pbuf;
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    sbuf = ALLOCATE_KEY(seedlen);
    memcpy(sbuf, seed, seedlen);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                                    sbuf, seedlen);
    pbuf = ALLOCATE_KEY(publen);
    memcpy(pbuf, pub, publen);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                    pbuf, publen);
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, alg, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed for the EdDSA key");
    EVP_PKEY_CTX_free(ctx);
    return key;
}

/* One-shot sign with optional OSSL params (instance/context-string).  The
 * initial *siglen is the 256-byte signature-buffer bound shared by both
 * algorithms (Ed25519 signs 64 bytes, Ed448 114). */
int ed25519_ed448_sign(EVP_PKEY *key, const OSSL_PARAM *params,
                       const unsigned char *msg, size_t msglen,
                       unsigned char *sig, size_t *siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    int ok;

    *siglen = 256;
    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit_ex(mctx, NULL, NULL, NULL, NULL, key,
                                params) == 1,
          "EVP_DigestSignInit_ex failed");
    ok = EVP_DigestSign(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}

/* One-shot verify with optional OSSL params; returns 1 on success. */
int ed25519_ed448_verify(EVP_PKEY *key, const OSSL_PARAM *params,
                         const unsigned char *msg, size_t msglen,
                         const unsigned char *sig, size_t siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    int ok;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestVerifyInit_ex(mctx, NULL, NULL, NULL, NULL, key,
                                  params) == 1,
          "EVP_DigestVerifyInit_ex failed");
    ok = EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}

/* Returns 1 if the EdDSA signature context accepts `param` (used to gate
 * the OpenSSL 3.2+ instance/context-string subtests). */
int ed25519_ed448_ctx_accepts(EVP_PKEY *key, const char *param)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = NULL;
    const OSSL_PARAM *settable;
    int found = 0;

    CHECK(EVP_DigestSignInit_ex(mctx, &pctx, NULL, NULL, NULL, key, NULL) == 1,
          "EVP_DigestSignInit_ex failed in ctx_accepts");
    settable = EVP_PKEY_CTX_settable_params(pctx);
    for (; settable != NULL && settable->key != NULL; settable++) {
        if (strcmp(settable->key, param) == 0) {
            found = 1;
            break;
        }
    }
    EVP_MD_CTX_free(mctx);
    return found;
}
