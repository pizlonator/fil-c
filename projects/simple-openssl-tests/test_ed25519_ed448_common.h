/* test_ed25519_ed448_common.h -- shared machinery for the Ed25519/Ed448
 * (PureEdDSA) family tests.
 *
 * One binary per algorithm (test_ed25519.c, which also carries the
 * Ed25519ctx and Ed25519ph instances, and test_ed448.c); each binary
 * defines its own RFC 8032 KAT constants and runs its sections through the
 * shared helpers below.  The big explanatory comment (the vector
 * provenance and the OpenSSL 3.2 instance/context gate) lives in
 * test_ed25519_ed448_common.c.
 */

#ifndef TEST_ED25519_ED448_COMMON_H
#define TEST_ED25519_ED448_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

/* Imports an EdDSA key pair (32-byte Ed25519 / 57-byte Ed448 seed plus the
 * matching public key). */
EVP_PKEY *ed25519_ed448_make_key(const char *alg, const unsigned char *seed,
                                 size_t seedlen, const unsigned char *pub,
                                 size_t publen);

/* One-shot sign with optional OSSL params (instance/context-string);
 * returns 1 on success and stores the signature length. */
int ed25519_ed448_sign(EVP_PKEY *key, const OSSL_PARAM *params,
                       const unsigned char *msg, size_t msglen,
                       unsigned char *sig, size_t *siglen);

/* One-shot verify with optional OSSL params; returns 1 on success. */
int ed25519_ed448_verify(EVP_PKEY *key, const OSSL_PARAM *params,
                         const unsigned char *msg, size_t msglen,
                         const unsigned char *sig, size_t siglen);

/* Returns 1 if the EdDSA signature context accepts `param` (the gate for
 * the OpenSSL 3.2+ instance/context-string subtests). */
int ed25519_ed448_ctx_accepts(EVP_PKEY *key, const char *param);

#endif /* TEST_ED25519_ED448_COMMON_H */
