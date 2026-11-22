/*
 * test_mac_blake2_common.c -- shared implementation of the keyed BLAKE2
 * MAC tests (API in test_mac_blake2_common.h; one binary per MAC, one
 * struct b2mac_family row and one KAT row set per binary:
 * test_mac_blake2b.c and test_mac_blake2s.c).
 *
 * Every constant below was produced and verified with pizfix/bin/openssl,
 * e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl mac -macopt hexkey:000102...3f \
 *       -macopt size:20 BLAKE2BMAC
 *
 * with key 000102...3f (64 bytes) for BLAKE2BMAC, 000102...1f (32 bytes)
 * for BLAKE2SMAC, 000102...0f (16 bytes) for the key-length variants, and
 * the message "abc".
 *
 * Each per-MAC binary carries its own MAC's rows of the size-sweep and
 * key-length-variant table and runs the chunked-updates and negative
 * sections for its own MAC.
 *
 * Coverage: output size sweeps via the "size" parameter ({64,32,20,1} for
 * BLAKE2BMAC, {32,16,1} for BLAKE2SMAC) with an EVP_MAC_CTX_get_mac_size
 * cross-check; key lengths {16,64} and {16,32} accepted; a zero-length
 * key and an over-long key (65 for b, 33 for s) rejected; an unsupported
 * output size (65 for b, 33 for s) rejected; and chunked updates
 * (one-shot == chunked over a 200-byte deterministic message).
 */

#include "test_mac_blake2_common.h"

#include <openssl/core_names.h>

/* The 000102... key patterns and the "abc" message.  The constants are
 * arrays (not pointers) so that the per-MAC binaries can use them in
 * static struct initializers. */
const char b2mac_key64_hex[] =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "303132333435363738393a3b3c3d3e3f";
const char b2mac_key32_hex[] =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f";
const char b2mac_key16_hex[] = "000102030405060708090a0b0c0d0e0f";
const char b2mac_msg_abc[] = "abc";

/* One keyed BLAKE2 computation.  size == 0 leaves the provider default
 * (64 for BLAKE2BMAC, 32 for BLAKE2SMAC).  Returns the tag length (0 on
 * failure). */
static size_t b2_oneshot(const char *macname, const unsigned char *key,
                         size_t keylen, size_t size,
                         const unsigned char *data, size_t datalen,
                         unsigned char *out, size_t outcap)
{
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[64];
    char msgbuf[128];
    size_t outlen = 0;
    int ok;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed", macname);
    mac = EVP_MAC_fetch(NULL, macname, NULL);
    CHECK(mac != NULL, msgbuf);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0) {
        params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
              "EVP_MAC_CTX_set_params(size) failed");
    }
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

/* Keyed BLAKE2 with the message fed in updates cycling through `chunks`
 * (each truncated to what remains).  Must agree with b2_oneshot. */
static size_t b2_chunked(const char *macname, const unsigned char *key,
                         size_t keylen, size_t size,
                         const unsigned char *data, size_t datalen,
                         const size_t *chunks, size_t nchunks,
                         unsigned char *out, size_t outcap)
{
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[64];
    char msgbuf[128];
    size_t off = 0, ci = 0, outlen = 0;
    int ok;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed", macname);
    mac = EVP_MAC_fetch(NULL, macname, NULL);
    CHECK(mac != NULL, msgbuf);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0) {
        params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
              "EVP_MAC_CTX_set_params(size) failed");
    }
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    while (ok && off < datalen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > datalen - off)
            n = datalen - off;
        ok = EVP_MAC_update(ctx, data + off, n);
        off += n;
    }
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* (1) The one-shot KATs of nkats rows, plus the get_mac_size cross-check
 * at sizetrack. */
void b2mac_test_kats(const struct b2mac_family *fam,
                     const struct b2mac_kat *kats, size_t nkats,
                     size_t sizetrack)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *msg = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[64];
    char msgbuf[128];
    size_t k, keylen, wantlen, gotlen, macsize;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed",
             fam->macname);
    mac = EVP_MAC_fetch(NULL, fam->macname, NULL);
    CHECK(mac != NULL, msgbuf);
    memcpy(msg, b2mac_msg_abc, 3);
    from_hex(fam->keyhex, key, 64);

    for (k = 0; k < nkats; k++) {
        const struct b2mac_kat *kat = &kats[k];

        keylen = from_hex(kat->keyhex, key, 64);
        wantlen = from_hex(kat->taghex, want, 64);
        gotlen = b2_oneshot(fam->macname, key, keylen, kat->size, msg, 3,
                            got, 64);
        snprintf(msgbuf, sizeof(msgbuf),
                 "%s (size=%zu, default %zu) one-shot does not match the KAT",
                 fam->macname, kat->size, kat->defsize);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }

    /* get_mac_size tracks the requested output size (queried after
     * init, where it is meaningful). */
    keylen = from_hex(fam->keyhex, key, 64);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    {
        size_t sz = sizetrack;
        params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &sz);
        params[1] = OSSL_PARAM_construct_end();
        snprintf(msgbuf, sizeof(msgbuf),
                 "EVP_MAC_CTX_set_params(size=%zu) failed", sizetrack);
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1, msgbuf);
    }
    memcpy(kcopy, key, keylen);
    snprintf(msgbuf, sizeof(msgbuf), "%s init failed", fam->macname);
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1, msgbuf);
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    snprintf(msgbuf, sizeof(msgbuf), "%s mac size does not track the size param",
             fam->macname);
    CHECK(macsize == sizetrack, msgbuf);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);

    free_poison_allocations();
}

/* (2) Chunked updates over a 200-byte deterministic message must equal
 * the one-shot tag at the default size. */
void b2mac_test_chunked(const struct b2mac_family *fam,
                        const size_t *chunks, size_t nchunks)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);
    char msgbuf[128];
    size_t i, one_len, many_len;

    from_hex(fam->keyhex, key, 64);
    for (i = 0; i < 200; i++)
        msg[i] = (unsigned char)(i * 3 + 11);

    one_len = b2_oneshot(fam->macname, key, fam->maxkeylen, 0, msg, 200,
                         one, 64);
    many_len = b2_chunked(fam->macname, key, fam->maxkeylen, 0, msg, 200,
                          chunks, nchunks, many, 64);
    snprintf(msgbuf, sizeof(msgbuf), "chunked %s differs from one-shot",
             fam->macname);
    expect_equal(msgbuf, many, many_len, one, one_len);

    free_poison_allocations();
}

/* (3) Negative tests: a zero-length key, the over-long key and the
 * unsupported output size must all fail cleanly. */
void b2mac_test_negative(const struct b2mac_family *fam)
{
    unsigned char *key = ALLOCATE_KEY(65);
    unsigned char *msg = ALLOCATE_INPUT(3);
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char empty[1] = { 0 };
    char msgbuf[128], badsize_msg[128], overlong_msg[128], zerolen_msg[128];
    size_t size_bad = fam->badsize, keylen;
    int okset, okinit;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed",
             fam->macname);
    mac = EVP_MAC_fetch(NULL, fam->macname, NULL);
    CHECK(mac != NULL, msgbuf);
    memcpy(msg, b2mac_msg_abc, 3);
    keylen = from_hex(fam->keyhex, key, fam->maxkeylen);
    key[fam->maxkeylen] = 0x40; /* the extra key byte for the over-long case */
    snprintf(zerolen_msg, sizeof(zerolen_msg), "%s accepted a zero-length key",
             fam->macname);
    snprintf(overlong_msg, sizeof(overlong_msg), "%s accepted a %zu-byte key",
             fam->macname, fam->overlong_len);
    snprintf(badsize_msg, sizeof(badsize_msg), "%s accepted an output size of %zu",
             fam->macname, fam->badsize);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    okinit = EVP_MAC_init(ctx, empty, 0, NULL);
    CHECK(!okinit, zerolen_msg);
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    okinit = EVP_MAC_init(ctx, key, fam->overlong_len, NULL);
    CHECK(!okinit, overlong_msg);
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size_bad);
    params[1] = OSSL_PARAM_construct_end();
    okset = EVP_MAC_CTX_set_params(ctx, params);
    okinit = okset ? EVP_MAC_init(ctx, key, keylen, NULL) : 0;
    CHECK(!(okset && okinit), badsize_msg);
    EVP_MAC_CTX_free(ctx);

    EVP_MAC_free(mac);

    free_poison_allocations();
}
