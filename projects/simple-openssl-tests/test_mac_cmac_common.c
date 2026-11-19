/*
 * test_mac_cmac_common.c -- shared implementation of the CMAC family tests
 * (API in test_mac_cmac_common.h; one binary per cipher, one struct
 * cmac_family row and one KAT row set per binary:
 * test_mac_cmac_aes_128_cbc.c, test_mac_cmac_aes_192_cbc.c,
 * test_mac_cmac_aes_256_cbc.c and test_mac_cmac_des_ede3_cbc.c).
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf '<hex msg>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -cipher aes-128-cbc -macopt hexkey:2b7e1516... CMAC
 *
 * (the per-cipher vector provenance travels with the vectors, into the
 * per-cipher binaries).
 *
 * Each per-cipher binary runs every section for its own cipher: the
 * one-shot KATs; the partial-update chunked pass == the KAT (the monolith
 * ran it on AES-128-CBC only, splitting the 40-byte example 3 message as
 * {17, 23}; the DES-EDE3-CBC binary splits its 16-byte message as {7, 9});
 * the EVP_MAC_CTX_get_mac_size read-back (the monolith checked it for
 * DES-EDE3-CBC only; the split checks it per cipher -- 16 for the AES-CBC
 * ciphers, 8 for DES-EDE3-CBC -- as the monolith's coverage note already
 * claimed); and the wrong-cipher negative, parameterized per cipher (the
 * monolith ran it with the AES-128-CBC key and an AES-256-CBC cipher only).
 *
 * Wrong-cipher note: a wrong cipher is rejected at init when its key
 * length differs from the key given (an AES-256-CBC cipher with a 16-byte
 * key fails the key-length check on every OpenSSL 3.x); non-CBC modes are
 * only rejected at set_params time in newer releases, so they are not
 * used for the negative test.
 */

#include "test_mac_cmac_common.h"

#include <openssl/core_names.h>

/* The SP 800-38A message (also RFC 4493's M), shared by every row of the
 * family.  The constant is an array (not a pointer) so that it is an
 * address constant usable where the per-cipher binaries need it. */
const char cmac_msg64_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

/* One-shot CMAC.  The cipher is selected through the "cipher" parameter
 * and the key through EVP_MAC_init.  Returns the tag length (0 on
 * failure). */
static size_t cmac_oneshot(const char *ciphername, const unsigned char *key,
                           size_t keylen, const unsigned char *data, size_t datalen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)ciphername, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && datalen > 0)
        ok = EVP_MAC_update(ctx, data, datalen);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* CMAC with the message fed as updates of exactly {a, b, rest} bytes.
 * Must agree with cmac_oneshot. */
static size_t cmac_chunked(const char *ciphername, const unsigned char *key,
                           size_t keylen, const unsigned char *data, size_t datalen,
                           size_t a, size_t b, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t off = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)ciphername, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && a > 0 && datalen > 0) {
        size_t n = a < datalen ? a : datalen;
        ok = EVP_MAC_update(ctx, data, n);
        off = n;
    }
    if (ok && b > 0 && off < datalen) {
        size_t n = b < datalen - off ? b : datalen - off;
        ok = EVP_MAC_update(ctx, data + off, n);
        off += n;
    }
    if (ok && off < datalen)
        ok = EVP_MAC_update(ctx, data + off, datalen - off);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* (1) One-shot KATs of nkats rows.  An empty-message row (example 1 of
 * each AES set) doubles as the subkey-generation sanity check. */
void cmac_test_kats(const struct cmac_family *fam,
                    const struct cmac_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(fam->keylen);
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OTHER(fam->macsize);
    unsigned char *got = ALLOCATE_OUTPUT(fam->macsize);
    size_t k, keylen, wantlen, gotlen;
    char msgbuf[128];

    from_hex(cmac_msg64_hex, msg, 64);
    for (k = 0; k < nkats; k++) {
        const struct cmac_kat *kat = &kats[k];

        keylen = from_hex(kat->keyhex, key, fam->keylen);
        wantlen = from_hex(kat->taghex, want, fam->macsize);
        gotlen = cmac_oneshot(fam->ciphername, key, keylen, msg, kat->msglen,
                              got, fam->macsize);
        snprintf(msgbuf, sizeof(msgbuf), "%s: one-shot tag does not match the KAT",
                 kat->label);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }

    free_poison_allocations();
}

/* (2) Partial updates: kat's message fed as updates of exactly {a, b, rest}
 * bytes must produce exactly kat's one-shot tag. */
void cmac_test_partial_updates(const struct cmac_family *fam,
                               const struct cmac_kat *kat, size_t a, size_t b)
{
    unsigned char *key = ALLOCATE_KEY(fam->keylen);
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OTHER(fam->macsize);
    unsigned char *got = ALLOCATE_OUTPUT(fam->macsize);
    size_t keylen, wantlen, gotlen;
    char msgbuf[128];

    keylen = from_hex(kat->keyhex, key, fam->keylen);
    from_hex(cmac_msg64_hex, msg, 64);
    wantlen = from_hex(kat->taghex, want, fam->macsize);
    gotlen = cmac_chunked(fam->ciphername, key, keylen, msg, kat->msglen, a, b,
                          got, fam->macsize);
    snprintf(msgbuf, sizeof(msgbuf),
             "partial updates {%zu,%zu} do not match the %s KAT", a, b,
             kat->label);
    expect_equal(msgbuf, got, gotlen, want, wantlen);

    free_poison_allocations();
}

/* (3) EVP_MAC_CTX_get_mac_size: fam's CMAC block size (16 for the AES-CBC
 * ciphers, 8 for DES-EDE3-CBC), read back after a plain init with the key
 * hex keyhex. */
void cmac_test_mac_size(const struct cmac_family *fam, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(fam->keylen);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t keylen, macsize;
    char msgbuf[128];

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    keylen = from_hex(keyhex, key, fam->keylen);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)fam->ciphername, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    /* EVP_MAC_CTX_get_mac_size is only meaningful once the context has
     * been initialized. */
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1, "CMAC init failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    snprintf(msgbuf, sizeof(msgbuf),
             "%s CMAC size is not %zu (the cipher block size)",
             fam->ciphername, fam->macsize);
    CHECK(macsize == fam->macsize, msgbuf);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);

    free_poison_allocations();
}

/* (4) A wrong cipher must fail at init: a cipher whose key length differs
 * from the key given (see the wrong-cipher note at the top). */
void cmac_test_wrong_cipher(const struct cmac_family *fam, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(fam->keylen);
    unsigned char *msg = ALLOCATE_INPUT(64);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t keylen;
    char msgbuf[128];
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    keylen = from_hex(keyhex, key, fam->keylen);
    from_hex(cmac_msg64_hex, msg, 64);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)fam->wrongcipher, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    snprintf(msgbuf, sizeof(msgbuf), "CMAC accepted %s with a %zu-byte key",
             fam->wrongcipher, fam->keylen);
    CHECK(!ok, msgbuf);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);

    free_poison_allocations();
}
