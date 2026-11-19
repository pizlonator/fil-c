/*
 * test_mac_hmac_common.c -- shared implementation of the HMAC family tests
 * (API in test_mac_hmac_common.h; one binary per digest, one struct
 * hmac_family row and one KAT row set per binary: test_mac_hmac_md5.c,
 * test_mac_hmac_sha1.c, test_mac_hmac_sha256.c, test_mac_hmac_sha384.c and
 * test_mac_hmac_sha512.c).
 *
 * Known-answer vectors, all verified two ways before embedding: with
 * pizfix/bin/openssl, e.g.
 *
 *   printf '<hex data>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -digest sha256 -macopt hexkey:<key> HMAC
 *
 * and independently with Python's hmac module.  The RFC 4231 binaries
 * (SHA256/384/512) carry that RFC's test cases 1-4, 6 and 7 for their
 * digest (test case 5 is a truncation case, which EVP cannot express and
 * is therefore omitted); the RFC 2202 binaries (SHA1/MD5) carry that RFC's
 * test cases 1-4, 6 and 7.  TC6/TC7 use 131-byte (RFC 4231) and 80-byte
 * (RFC 2202) keys, both larger than the respective block sizes, and TC7
 * uses data longer than one block.
 *
 * Each per-digest binary runs every section for its own digest: the
 * mac-size probe (the monolith ran it for the RFC 4231 digests only); the
 * one-shot KATs; the chunked {1, 17} updates == one-shot (the monolith ran
 * chunked updates for the RFC 4231 digests only); the key-size section --
 * empty key == the 32-zero-byte and 64-zero-byte keys -- which the
 * monolith ran for SHA256 only, anchored to its empty-key KAT (the other
 * digests have no published empty-key vector, so their equivalence anchors
 * on the empty-key result itself); and the nonexistent-digest negative.
 */

#include "test_mac_hmac_common.h"

#include <openssl/core_names.h>

/* The RFC 4231/2202 "larger than block-size" ASCII data (the TC6 and TC7
 * test strings), shared by the RFC 4231 and RFC 2202 sections (the
 * monolith carried two identical copies; the split keeps one, with the
 * length-drift checks where the monolith had them -- in the RFC 4231
 * material builder). */
static const char tc6[] =
    "Test Using Larger Than Block-Size Key - Hash Key First";
static const char tc7[] =
    "This is a test using a larger than block-size key and a larger "
    "than block-size data. The key needs to be hashed before being "
    "used by the HMAC algorithm.";

/* One-shot HMAC via EVP_MAC: the digest comes through the "digest"
 * parameter, the key through EVP_MAC_init.  Returns the tag length, or 0
 * on failure (the negative tests look at the intermediate return codes
 * themselves). */
static size_t hmac_oneshot(const char *digestname, const unsigned char *key,
                           size_t keylen, const unsigned char *data, size_t datalen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[256];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"HMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)digestname, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(digest) failed");
    /* EVP_MAC_init's key pointer is not const; work on a labeled copy. */
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, key != NULL ? kcopy : NULL, keylen, NULL);
    if (ok && datalen > 0)
        ok = EVP_MAC_update(ctx, data, datalen);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* HMAC with the data fed in updates of {a, b, rest} bytes.  Must agree
 * with hmac_oneshot for every split. */
static size_t hmac_chunked(const char *digestname, const unsigned char *key,
                           size_t keylen, const unsigned char *data, size_t datalen,
                           size_t a, size_t b, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[256];
    size_t off = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"HMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)digestname, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(digest) failed");
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

/* The RFC 4231 key/data material, built from patterns and ASCII so the
 * vectors stay readable. */
static void rfc4231_material(unsigned char *key_tc1, unsigned char *key_tc4,
                             unsigned char *key_tc6, unsigned char *data_tc1,
                             unsigned char *data_tc2, unsigned char *data_tc3,
                             unsigned char *data_tc4, unsigned char *data_tc6,
                             unsigned char *data_tc7, size_t *len_tc7)
{
    size_t i;

    CHECK(sizeof(tc6) - 1 == 54, "RFC 4231 TC6 data length drifted");
    CHECK(sizeof(tc7) - 1 == 152, "RFC 4231 TC7 data length drifted");
    for (i = 0; i < 20; i++)
        key_tc1[i] = 0x0b;
    for (i = 0; i < 25; i++)
        key_tc4[i] = (unsigned char)(i + 1); /* 01..19, 25 bytes */
    for (i = 0; i < 131; i++)
        key_tc6[i] = 0xaa;
    memcpy(data_tc1, "Hi There", 8);
    memcpy(data_tc2, "what do ya want for nothing?", 28);
    for (i = 0; i < 50; i++) {
        data_tc3[i] = 0xdd;
        data_tc4[i] = 0xcd;
    }
    memcpy(data_tc6, tc6, sizeof(tc6) - 1);
    memcpy(data_tc7, tc7, sizeof(tc7) - 1);
    *len_tc7 = sizeof(tc7) - 1;
}

/* (0) EVP_MAC_CTX_get_mac_size == the digest size, probed after a plain
 * init with a 20-byte 0x0b key. */
void hmac_test_mac_size(const struct hmac_family *fam)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char probekey[20];
    char msgbuf[128];
    size_t macsize, i;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"HMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    for (i = 0; i < sizeof(probekey); i++)
        probekey[i] = 0x0b;
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)fam->digestname, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(digest) failed");
    /* EVP_MAC_CTX_get_mac_size is only meaningful once the context has
     * been initialized. */
    CHECK(EVP_MAC_init(ctx, probekey, sizeof(probekey), NULL) == 1,
          "EVP_MAC_init with a probe key failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    snprintf(msgbuf, sizeof(msgbuf),
             "%s: EVP_MAC_CTX_get_mac_size is not the digest size",
             fam->digestname);
    CHECK(macsize == fam->digestsize, msgbuf);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

/* (1) RFC 4231 test cases 1-4, 6 and 7 for fam's digest: one-shot == KAT
 * and a chunked {1, 17} pass == one-shot, for each of nkats rows. */
void hmac_test_rfc4231(const struct hmac_family *fam,
                       const struct hmac_kat *kats, size_t nkats)
{
    unsigned char *key_tc1 = ALLOCATE_KEY(20);
    unsigned char *key_tc4 = ALLOCATE_KEY(25);
    unsigned char *key_tc6 = ALLOCATE_KEY(131);
    unsigned char *data1 = ALLOCATE_INPUT(8);
    unsigned char *data2 = ALLOCATE_INPUT(32);
    unsigned char *data3 = ALLOCATE_INPUT(50);
    unsigned char *data4 = ALLOCATE_INPUT(50);
    unsigned char *data6 = ALLOCATE_INPUT(54);
    unsigned char *data7 = ALLOCATE_INPUT(152);
    unsigned char *want = ALLOCATE_OTHER(fam->digestsize);
    unsigned char *got = ALLOCATE_OUTPUT(fam->digestsize);
    unsigned char *got2 = ALLOCATE_OUTPUT(fam->digestsize);
    size_t len_tc7 = 0, t;
    char msg[128];

    rfc4231_material(key_tc1, key_tc4, key_tc6, data1, data2, data3, data4,
                     data6, data7, &len_tc7);

    for (t = 0; t < nkats; t++) {
        const unsigned char *key;
        size_t keylen;
        const unsigned char *data;
        size_t datalen;
        size_t gotlen, got2len, wantlen;

        switch (t) {
        case 0: key = key_tc1; keylen = 20; data = data1; datalen = 8; break;
        case 1: key = (const unsigned char *)"Jefe"; keylen = 4;
                data = data2; datalen = 28; break;
        case 2: key = key_tc6; keylen = 20; data = data3; datalen = 50; break;
        case 3: key = key_tc4; keylen = 25; data = data4; datalen = 50; break;
        case 4: key = key_tc6; keylen = 131; data = data6; datalen = 54; break;
        default: key = key_tc6; keylen = 131; data = data7;
                 datalen = len_tc7; break;
        }
        wantlen = from_hex(kats[t].taghex, want, fam->digestsize);
        gotlen = hmac_oneshot(fam->digestname, key, keylen, data, datalen,
                              got, fam->digestsize);
        snprintf(msg, sizeof(msg),
                 "RFC 4231 %s TC%zu one-shot does not match the KAT",
                 fam->digestname, kats[t].tc);
        expect_equal(msg, got, gotlen, want, wantlen);

        got2len = hmac_chunked(fam->digestname, key, keylen, data, datalen, 1, 17,
                               got2, fam->digestsize);
        snprintf(msg, sizeof(msg),
                 "RFC 4231 %s TC%zu chunked pass differs from one-shot",
                 fam->digestname, kats[t].tc);
        expect_equal(msg, got2, got2len, got, gotlen);
    }

    free_poison_allocations();
}

/* (2) RFC 2202 test cases 1-4, 6 and 7 for fam's digest: one-shot == KAT,
 * plus the chunked {1, 17} pass == one-shot (added by the split; the
 * monolith ran chunked updates for the RFC 4231 digests only).  The
 * TC1/TC3/TC4 keys are basekeylen bytes (20 for SHA1, 16 for MD5); TC6/TC7
 * use 80-byte keys.  The monolith built the TC6/TC7 data inline in its RFC
 * 2202 section, without the RFC 4231 drift checks; kept as-is. */
void hmac_test_rfc2202(const struct hmac_family *fam,
                       const struct hmac_kat *kats, size_t nkats,
                       size_t basekeylen)
{
    unsigned char *key = ALLOCATE_KEY(80);
    unsigned char *data1 = ALLOCATE_INPUT(8);
    unsigned char *data2 = ALLOCATE_INPUT(32);
    unsigned char *data3 = ALLOCATE_INPUT(50);
    unsigned char *data4 = ALLOCATE_INPUT(50);
    unsigned char *data6 = ALLOCATE_INPUT(54);
    unsigned char *data7 = ALLOCATE_INPUT(152);
    unsigned char *want = ALLOCATE_OTHER(fam->digestsize);
    unsigned char *got = ALLOCATE_OUTPUT(fam->digestsize);
    unsigned char *got2 = ALLOCATE_OUTPUT(fam->digestsize);
    size_t len_tc7 = 0, t, i;
    char msg[128];

    memcpy(data1, "Hi There", 8);
    memcpy(data2, "what do ya want for nothing?", 28);
    for (i = 0; i < 50; i++) {
        data3[i] = 0xdd;
        data4[i] = 0xcd;
    }
    memcpy(data6, tc6, sizeof(tc6) - 1);
    memcpy(data7, tc7, sizeof(tc7) - 1);
    len_tc7 = sizeof(tc7) - 1;

    for (t = 0; t < nkats; t++) {
        size_t keylen, datalen, gotlen, got2len, wantlen;
        const unsigned char *data;

        switch (t) {
        case 0: keylen = basekeylen; data = data1; datalen = 8; break;
        case 1: keylen = 4; data = data2; datalen = 28; break;
        case 2: keylen = basekeylen; data = data3; datalen = 50; break;
        case 3: keylen = basekeylen; data = data4; datalen = 50; break;
        case 4: keylen = 80; data = data6; datalen = 54; break;
        default: keylen = 80; data = data7; datalen = len_tc7; break;
        }
        for (i = 0; i < keylen; i++)
            key[i] = (t == 0) ? 0x0b : (t == 1 || t == 3) ? (unsigned char)(i + 1)
                                                          : 0xaa;
        if (t == 1)
            memcpy(key, "Jefe", 4);
        wantlen = from_hex(kats[t].taghex, want, fam->digestsize);
        CHECK(wantlen == fam->digestsize, "RFC 2202 KAT length mismatch");
        gotlen = hmac_oneshot(fam->digestname, key, keylen, data, datalen,
                              got, fam->digestsize);
        snprintf(msg, sizeof(msg),
                 "RFC 2202 %s TC%zu one-shot does not match the KAT",
                 fam->digestname, kats[t].tc);
        expect_equal(msg, got, gotlen, want, wantlen);

        got2len = hmac_chunked(fam->digestname, key, keylen, data, datalen, 1, 17,
                               got2, fam->digestsize);
        snprintf(msg, sizeof(msg),
                 "RFC 2202 %s TC%zu chunked pass differs from one-shot",
                 fam->digestname, kats[t].tc);
        expect_equal(msg, got2, got2len, got, gotlen);
    }

    free_poison_allocations();
}

/* (3) Key sizes: the empty key (zero length, non-NULL buffer) must work
 * and must equal the 32-zero-byte and 64-zero-byte keys (HMAC zero-pads
 * the key to the block size; 64 bytes is at or below every family block
 * size).  Anchored digests compare against the empty-key KAT, as the
 * monolith did; the others against the empty-key result itself. */
void hmac_test_key_sizes(const struct hmac_family *fam, const char *emptykat_hex)
{
    const char *refname = emptykat_hex != NULL ? "KAT" : "result";
    unsigned char *empty = ALLOCATE_KEY(1);
    unsigned char *key32 = ALLOCATE_KEY(32);
    unsigned char *key64 = ALLOCATE_KEY(64);
    unsigned char *want = ALLOCATE_OTHER(fam->digestsize);
    unsigned char *got_empty = ALLOCATE_OUTPUT(fam->digestsize);
    unsigned char *got = ALLOCATE_OUTPUT(fam->digestsize);
    const unsigned char *ref;
    size_t reflen, got_emptylen, gotlen;
    char msg[128];

    got_emptylen = hmac_oneshot(fam->digestname, empty, 0, NULL, 0,
                                got_empty, fam->digestsize);
    if (emptykat_hex != NULL) {
        size_t wantlen = from_hex(emptykat_hex, want, fam->digestsize);

        unpoison_all();
        snprintf(msg, sizeof(msg),
                 "HMAC-%s with the empty key does not match the KAT",
                 fam->digestname);
        CHECK(got_emptylen == wantlen && memcmp(got_empty, want, wantlen) == 0,
              msg);
        repoison_all();
        ref = want;
        reflen = wantlen;
    } else {
        /* No published empty-key vector for this digest: anchor the
         * equivalence on the empty-key result itself (so a broken
         * computation cannot pass vacuously, first check that it returned
         * the digest-size tag). */
        unpoison_all();
        snprintf(msg, sizeof(msg),
                 "HMAC-%s with the empty key did not return the digest size",
                 fam->digestname);
        CHECK(got_emptylen == fam->digestsize, msg);
        repoison_all();
        ref = got_empty;
        reflen = got_emptylen;
    }

    memset(key32, 0, 32);
    gotlen = hmac_oneshot(fam->digestname, key32, 32, NULL, 0, got,
                          fam->digestsize);
    unpoison_all();
    snprintf(msg, sizeof(msg),
             "HMAC-%s with a 32-zero-byte key differs from the empty-key %s",
             fam->digestname, refname);
    CHECK(gotlen == reflen && memcmp(got, ref, reflen) == 0, msg);
    repoison_all();

    memset(key64, 0, 64);
    gotlen = hmac_oneshot(fam->digestname, key64, 64, NULL, 0, got,
                          fam->digestsize);
    unpoison_all();
    snprintf(msg, sizeof(msg),
             "HMAC-%s with a 64-zero-byte key differs from the empty-key %s",
             fam->digestname, refname);
    CHECK(gotlen == reflen && memcmp(got, ref, reflen) == 0, msg);
    repoison_all();

    free_poison_allocations();
}

/* (4) A nonexistent digest name must fail cleanly: either the parameter
 * set or the init rejects it, never a silent success. */
void hmac_test_wrong_digest(void)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char key[4] = { 'k', 'e', 'y', 's' };
    int okset, okinit;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"HMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)"NoSuchDigest", 0);
    params[1] = OSSL_PARAM_construct_end();
    okset = EVP_MAC_CTX_set_params(ctx, params);
    okinit = okset ? EVP_MAC_init(ctx, key, sizeof(key), NULL) : 0;
    CHECK(!(okset && okinit),
          "HMAC with a nonexistent digest name did not fail");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}
