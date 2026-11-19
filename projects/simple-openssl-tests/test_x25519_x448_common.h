/* test_x25519_x448_common.h -- shared machinery for the X25519/X448
 * (RFC 7748) family tests.
 *
 * One binary per function (test_x25519.c and test_x448.c); each binary
 * defines one struct x25519_x448_fam row with its own RFC 7748 constants
 * and calls the shared test routines below.  The big explanatory comment
 * (the vector provenance, the iterated-vector shape, the low-order
 * requirement, and the KEM note) lives in test_x25519_x448_common.c.
 */

#ifndef TEST_X25519_X448_COMMON_H
#define TEST_X25519_X448_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per function: the EVP_PKEY fetch name ("X25519" or "X448"), the
 * key length in bytes (32 or 56), the RFC 7748 section 6.1/6.2 KAT (Alice
 * and Bob key pairs and the shared secret K), and the section 5.2
 * iterated-vector start and checkpoint values. */
struct x25519_x448_fam {
    const char *alg;         /* fetch name: "X25519" or "X448" */
    size_t keylen;           /* 32 or 56 */
    const char *askhex;      /* Alice secret (also the low-order test's key) */
    const char *apkhex;      /* Alice public = X(a, 9) */
    const char *bskhex;      /* Bob secret */
    const char *bpkhex;      /* Bob public = X(b, 9) */
    const char *khex;        /* the shared secret K */
    const char *starthex;    /* 5.2 start: k = u = base-point encoding */
    const char *iter1hex;    /* value after 1 iteration */
    const char *iter1000hex; /* value after 1000 iterations */
};

/* Imports an X25519/X448 key from the given private and/or public key
 * bytes (either may be NULL). */
EVP_PKEY *x25519_x448_make_key(const char *alg, const unsigned char *priv,
                               size_t privlen, const unsigned char *pub,
                               size_t publen);

/* derive(our_key, peer); returns 1 and stores the shared secret on
 * success, 0 on failure. */
int x25519_x448_derive(EVP_PKEY *key, EVP_PKEY *peer, unsigned char *secret,
                       size_t cap, size_t *secretlen);

/* (1) fam's section 6.1/6.2 KAT, run in both derive directions. */
void x25519_x448_test_kats(const struct x25519_x448_fam *fam);

/* (2) fam's RFC 7748 section 5.2 iterated vectors: k = X(k, u), u = old k,
 * starting from the base-point encoding; checks the values after 1 and
 * 1000 iterations. */
void x25519_x448_test_iterated(const struct x25519_x448_fam *fam);

/* (3) The RFC 7748 section 6 low-order-point rejection for fam: an
 * all-zero peer point must make EVP_PKEY_derive fail outright. */
void x25519_x448_test_low_order_peer(const struct x25519_x448_fam *fam);

#endif /* TEST_X25519_X448_COMMON_H */
