/*
 * test_x25519_x448_common.c -- shared implementation of the X25519/X448
 * (RFC 7748) family tests (API in test_x25519_x448_common.h; one binary per
 * function, one struct x25519_x448_fam row per binary).
 *
 * Known-answer vectors, all taken verbatim from RFC 7748 and verified with
 * filcc-compiled scratch probes before being embedded in the per-binary
 * rows (the per-binary comments say which sections they carry):
 *
 *   - Section 6.1: Alice (a -> X25519(a,9)), Bob (b -> X25519(b,9)) and the
 *     shared secret K, in both derive directions (the X25519 binary).
 *   - Section 6.2: the X448 counterparts (56-byte keys, shared secret K;
 *     the X448 binary).
 *   - Section 5.2: the iterated vectors -- starting from k = u = 0x09 (for
 *     X25519) / 0x05 (for X448), each iteration computes k = f(k, u) and
 *     u = old k; the result after 1 and 1000 iterations must match the RFC.
 *     The iterated shape is identical for the two functions, so it lives
 *     here and every binary runs it for its own function.
 *   - Section 6.2/6: an all-zero peer point (low-order) must yield an
 *     all-zero shared secret, and implementations must abort: OpenSSL's
 *     EVP_PKEY_derive fails outright, which is what we assert (likewise
 *     parameterized and run by both binaries).
 *
 * X25519-as-KEM (EVP_PKEY_encapsulate/decapsulate) is a 3.5+ feature, is
 * tested only by the X25519 binary, and gates mid-run there with a SKIP
 * note on older builds (the only SKIP-able feature in this family; the raw
 * DH KATs exist in all OpenSSL 3.x).
 */

#include "test_x25519_x448_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* Imports an X25519/X448 key from the given private and/or public key
 * bytes (either may be NULL).  Buffers are static: the key references them
 * for its lifetime. */
EVP_PKEY *x25519_x448_make_key(const char *alg, const unsigned char *priv,
                               size_t privlen, const unsigned char *pub,
                               size_t publen)
{
    static unsigned char *pbuf;
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    if (priv != NULL) {
        pbuf = ALLOCATE_KEY(privlen);
        memcpy(pbuf, priv, privlen);
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                                        pbuf, privlen);
    }
    if (pub != NULL) {
        pbuf = ALLOCATE_KEY(publen);
        memcpy(pbuf, pub, publen);
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                        pbuf, publen);
    }
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, alg, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed");
    EVP_PKEY_CTX_free(ctx);
    return key;
}

/* derive(our_key, peer); returns 1 and stores the shared secret on
 * success, 0 on failure. */
int x25519_x448_derive(EVP_PKEY *key, EVP_PKEY *peer, unsigned char *secret,
                       size_t cap, size_t *secretlen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    size_t len = cap;
    int ok;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_derive_init(ctx) == 1, "EVP_PKEY_derive_init failed");
    CHECK(EVP_PKEY_derive_set_peer(ctx, peer) == 1,
          "EVP_PKEY_derive_set_peer failed");
    ok = EVP_PKEY_derive(ctx, secret, &len) == 1;
    *secretlen = ok ? len : 0;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

/* (1) fam's section 6.1/6.2 KAT, run in both derive directions, plus the
 * public-key-derivation checks (the RFC public key is X(sk, base)). */
void x25519_x448_test_kats(const struct x25519_x448_fam *fam)
{
    size_t keylen = fam->keylen;
    unsigned char *ask = ALLOCATE_KEY(keylen), *apk = ALLOCATE_KEY(keylen);
    unsigned char *bsk = ALLOCATE_KEY(keylen), *bpk = ALLOCATE_KEY(keylen);
    unsigned char *z = ALLOCATE_OUTPUT(keylen);
    unsigned char *want = ALLOCATE_OUTPUT(keylen);
    size_t zlen = 0, wantlen;
    char mbuf[128];
    EVP_PKEY *A, *B;

    from_hex(fam->askhex, ask, keylen);
    from_hex(fam->apkhex, apk, keylen);
    from_hex(fam->bskhex, bsk, keylen);
    from_hex(fam->bpkhex, bpk, keylen);
    wantlen = from_hex(fam->khex, want, keylen);
    CHECK(wantlen == keylen, "shared-secret constant has the wrong length");

    A = x25519_x448_make_key(fam->alg, ask, keylen, apk, keylen);
    B = x25519_x448_make_key(fam->alg, bsk, keylen, bpk, keylen);

    zlen = 0;
    snprintf(mbuf, sizeof(mbuf), "%s derive failed", fam->alg);
    CHECK(x25519_x448_derive(A, B, z, keylen, &zlen) == 1, mbuf);
    snprintf(mbuf, sizeof(mbuf), "%s: derive(a, B_pub) != RFC K", fam->alg);
    unpoison_all();
    CHECK(zlen == keylen && memcmp(z, want, keylen) == 0, mbuf);
    repoison_all();

    snprintf(mbuf, sizeof(mbuf), "%s derive failed", fam->alg);
    CHECK(x25519_x448_derive(B, A, z, keylen, &zlen) == 1, mbuf);
    snprintf(mbuf, sizeof(mbuf), "%s: derive(b, A_pub) != RFC K", fam->alg);
    unpoison_all();
    CHECK(zlen == keylen && memcmp(z, want, keylen) == 0, mbuf);
    repoison_all();

    EVP_PKEY_free(A);
    EVP_PKEY_free(B);

    free_poison_allocations();
}

/* (2) RFC 7748 section 5.2 iterated vectors for fam: k = X(k, u), u = old
 * k, starting from the base-point encoding.  Checks 1 and 1000
 * iterations. */
void x25519_x448_test_iterated(const struct x25519_x448_fam *fam)
{
    size_t keylen = fam->keylen;
    unsigned char *k = ALLOCATE_KEY(keylen);
    unsigned char *u = ALLOCATE_KEY(keylen);
    unsigned char *t = ALLOCATE_OUTPUT(keylen);
    unsigned char *want = ALLOCATE_OUTPUT(keylen);
    unsigned char *skbuf, *pubbuf;
    int it;
    size_t tl = 0;
    char mbuf[128];

    from_hex(fam->starthex, k, keylen);
    memcpy(u, k, keylen);

    for (it = 0; it < 1000; it++) {
        EVP_PKEY *ka, *kb;
        skbuf = ALLOCATE_KEY(keylen);
        memcpy(skbuf, k, keylen);
        pubbuf = ALLOCATE_KEY(keylen);
        memcpy(pubbuf, u, keylen);
        ka = x25519_x448_make_key(fam->alg, skbuf, keylen, NULL, 0);
        kb = x25519_x448_make_key(fam->alg, NULL, 0, pubbuf, keylen);
        snprintf(mbuf, sizeof(mbuf), "iterated %s derive failed", fam->alg);
        CHECK(x25519_x448_derive(ka, kb, t, keylen, &tl) == 1 && tl == keylen,
              mbuf);
        EVP_PKEY_free(ka);
        EVP_PKEY_free(kb);
        memcpy(u, k, keylen);
        memcpy(k, t, keylen);
        if (it == 0) {
            from_hex(fam->iter1hex, want, keylen);
            snprintf(mbuf, sizeof(mbuf),
                     "iterated %s: 1-iteration value != RFC", fam->alg);
            unpoison_all();
            CHECK(memcmp(k, want, keylen) == 0, mbuf);
            repoison_all();
        }
    }
    from_hex(fam->iter1000hex, want, keylen);
    snprintf(mbuf, sizeof(mbuf),
             "iterated %s: 1000-iteration value != RFC", fam->alg);
    unpoison_all();
    CHECK(memcmp(k, want, keylen) == 0, mbuf);
    repoison_all();

    free_poison_allocations();
}

/* (3) RFC 7748 section 6 for fam: an all-zero peer point must produce an
 * all-zero shared secret, which implementations must reject; OpenSSL's
 * derive fails outright.  The secret key is the section 6.1/6.2 KAT's
 * Alice key (the same constant the monolith selected per function). */
void x25519_x448_test_low_order_peer(const struct x25519_x448_fam *fam)
{
    size_t keylen = fam->keylen;
    unsigned char *ask = ALLOCATE_KEY(keylen);
    unsigned char *zero = ALLOCATE_KEY(keylen);
    unsigned char *z = ALLOCATE_OUTPUT(keylen);
    size_t zlen = 0;
    EVP_PKEY *A, *Z;

    from_hex(fam->askhex, ask, keylen);
    memset(zero, 0, keylen);
    A = x25519_x448_make_key(fam->alg, ask, keylen, NULL, 0);
    Z = x25519_x448_make_key(fam->alg, NULL, 0, zero, keylen);
    CHECK(x25519_x448_derive(A, Z, z, keylen, &zlen) == 0,
          "low-order (all-zero) peer point was accepted");
    EVP_PKEY_free(A);
    EVP_PKEY_free(Z);

    free_poison_allocations();
}
