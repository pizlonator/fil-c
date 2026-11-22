/*
 * test_mac_kmac_common.c -- shared implementation of the KMAC-128 and
 * KMAC-256 MAC tests (API in test_mac_kmac_common.h; one binary per MAC,
 * one struct kmac_family row and one KAT row set per binary:
 * test_mac_kmac128.c and test_mac_kmac256.c).
 *
 * Known-answer vectors: the NIST samples distributed in OpenSSL's own
 * test data (test/recipes/30-test_evp_data/evpmac_common.txt, "KMAC
 * Tests (From NIST)" and "KMAC XOF Tests (From NIST)"), every one
 * re-verified with pizfix/bin/openssl, e.g.
 *
 *   printf '<hex msg>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -macopt hexkey:404142...5f -macopt size:32 \
 *       -macopt custom:'My Tagged Application' KMAC-128
 *
 * The sample key is the 32-byte 404142...5f (not 16 bytes), the short
 * message is 00010203, and the long message is the 200-byte 000102...c7.
 * Samples: KMAC128/00010203/""/32 = e5780b0d..., KMAC128/00010203/
 * "My Tagged Application"/32 = 3b1fba96..., KMAC128/200-byte/custom/32 =
 * 1f5b4e6c..., KMAC256/00010203/custom/64 = 20c570c3..., KMAC256/
 * 200-byte/""/64 = 75358cf3..., KMAC256/200-byte/custom/64 = b58618f7...,
 * plus the XOF forms cd83740b..., 31a44527... and 1755133f....
 *
 * Each per-MAC binary carries its own MAC's rows of that sample table
 * (the sample numbers of the original nine-row table are preserved in the
 * rows for the failure messages) and runs the absent-custom equivalence,
 * the chunked updates and the output-size parameter for its own MAC.
 *
 * Portability note: the "custom" parameter must be passed as an
 * OCTET_STRING whose data_size is exactly strlen(custom) (no trailing
 * NUL).  A UTF8_STRING parameter is accepted by OpenSSL 3.0.x but
 * rejected by the 3.6.x type check, and an OCTET_STRING that includes
 * the NUL silently changes the tag (the NUL becomes part of the custom
 * string) -- both forms were probed on both stacks.
 */

#include "test_mac_kmac_common.h"

#include <openssl/core_names.h>

/* The NIST sample key, shared by every row of both MACs.  The constant is
 * an array (not a pointer) so that it is an address constant usable where
 * the per-MAC binaries need it. */
const char kmac_key32_hex[] =
    "404142434445464748494a4b4c4d4e4f"
    "505152535455565758595a5b5c5d5e5f";

/* The shared 200-byte long sample message, 000102...c7 (built from the
 * byte pattern, exactly as the original table's NULL msghex rows did). */
static void kmac_fill_msg200(unsigned char *msg)
{
    size_t i;

    for (i = 0; i < 200; i++)
        msg[i] = (unsigned char)i;
}

/* One KMAC computation.  custom == NULL means: do not set the parameter
 * at all (the provider default is the empty custom string).  customlen is
 * strlen(custom) -- WITHOUT the NUL, see the portability note above.  xof
 * selects the XOF mode (right_encoded(0) length encoding).  size == 0
 * leaves the provider default (32 for KMAC-128, 64 for KMAC-256).  The
 * key always comes from the shared kmac_key32_hex pattern.  Returns the
 * tag length (0 on failure). */
static size_t kmac_oneshot(const char *macname, const unsigned char *key,
                           size_t keylen, const unsigned char *custom,
                           size_t customlen, int xof, size_t size,
                           const unsigned char *data, size_t datalen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[4];
    unsigned char kcopy[64];
    char customcopy[64];
    char msgbuf[128];
    size_t n = 0, outlen = 0;
    int ok;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed", macname);
    mac = EVP_MAC_fetch(NULL, macname, NULL);
    CHECK(mac != NULL, msgbuf);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0)
        params[n++] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    if (custom != NULL) {
        CHECK(customlen < sizeof(customcopy),
              "custom string longer than the working buffer");
        memcpy(customcopy, custom, customlen);
        /* OCTET_STRING with exactly customlen bytes: portable across
         * OpenSSL 3.0 - 3.6 (see the portability note above). */
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_CUSTOM,
                                                        customcopy, customlen);
    }
    if (xof >= 0)
        params[n++] = OSSL_PARAM_construct_int(OSSL_MAC_PARAM_XOF, &xof);
    params[n] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(size/custom/xof) failed");
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

/* KMAC with the message fed in updates of {a, rest} bytes.  Must agree
 * with kmac_oneshot. */
static size_t kmac_chunked(const char *macname, const unsigned char *key,
                           size_t keylen, const unsigned char *custom,
                           size_t customlen, int xof, size_t size,
                           const unsigned char *data, size_t datalen,
                           size_t a, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[4];
    unsigned char kcopy[64];
    char customcopy[64];
    char msgbuf[128];
    size_t n = 0, outlen = 0;
    int ok;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed", macname);
    mac = EVP_MAC_fetch(NULL, macname, NULL);
    CHECK(mac != NULL, msgbuf);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0)
        params[n++] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    if (custom != NULL) {
        CHECK(customlen < sizeof(customcopy),
              "custom string longer than the working buffer");
        memcpy(customcopy, custom, customlen);
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_CUSTOM,
                                                        customcopy, customlen);
    }
    if (xof >= 0)
        params[n++] = OSSL_PARAM_construct_int(OSSL_MAC_PARAM_XOF, &xof);
    params[n] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(size/custom/xof) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && a > 0 && datalen > 0) {
        size_t take = a < datalen ? a : datalen;
        ok = EVP_MAC_update(ctx, data, take);
    }
    if (ok && datalen > 0)
        ok = EVP_MAC_update(ctx, data + a, datalen - a);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* (1) The one-shot KATs of nkats rows. */
void kmac_test_kats(const struct kmac_family *fam,
                    const struct kmac_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t k, keylen, wantlen, gotlen;
    char msgbuf[128];

    keylen = from_hex(kmac_key32_hex, key, 32);
    kmac_fill_msg200(msg);

    for (k = 0; k < nkats; k++) {
        const struct kmac_kat *kat = &kats[k];
        size_t customlen = kat->customhex ? strlen(kat->customhex) : 0;

        /* The message always comes from the shared 200-byte pattern
         * buffer, of which kat->msglen bytes are used (the short rows'
         * 00010203 is its 4-byte prefix), exactly as the original sample
         * table did. */
        wantlen = from_hex(kat->taghex, want, 64);
        gotlen = kmac_oneshot(fam->macname, key, keylen,
                              (const unsigned char *)kat->customhex, customlen,
                              kat->xof, kat->size, msg, kat->msglen, got, 64);
        snprintf(msgbuf, sizeof(msgbuf), "%s sample %zu does not match the KAT",
                 fam->macname, kat->sample);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }

    free_poison_allocations();
}

/* (2) The absent-custom == empty-custom equivalence over the message of
 * the empty-custom KAT row, and the provider default output size. */
void kmac_test_absent_custom(const struct kmac_family *fam,
                             const struct kmac_kat *empty_row)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    char msgbuf[128];
    size_t keylen, wantlen, gotlen, macsize;

    keylen = from_hex(kmac_key32_hex, key, 32);
    kmac_fill_msg200(msg);
    wantlen = from_hex(empty_row->taghex, want, 64);

    /* No custom parameter at all must produce the empty-custom tag. */
    gotlen = kmac_oneshot(fam->macname, key, keylen, NULL, 0, empty_row->xof,
                          empty_row->size, msg, empty_row->msglen, got, 64);
    expect_equal("absent custom string differs from the empty-custom KAT",
                 got, gotlen, want, wantlen);

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed",
             fam->macname);
    mac = EVP_MAC_fetch(NULL, fam->macname, NULL);
    CHECK(mac != NULL, msgbuf);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params with no params failed");
    memcpy(kcopy, key, keylen);
    snprintf(msgbuf, sizeof(msgbuf), "%s init failed", fam->macname);
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1, msgbuf);
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    snprintf(msgbuf, sizeof(msgbuf), "%s default output size is not %zu",
             fam->macname, fam->defsize);
    CHECK(macsize == fam->defsize, msgbuf);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);

    free_poison_allocations();
}

/* (3) Chunked updates: the row's message fed as updates of {a, rest}
 * bytes for each `a` must reproduce the row's one-shot tag. */
void kmac_test_chunked(const struct kmac_family *fam,
                       const struct kmac_kat *kat,
                       const size_t *firsts, size_t nfirsts)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t keylen, wantlen, gotlen, i, customlen;
    char msgbuf[128];

    keylen = from_hex(kmac_key32_hex, key, 32);
    kmac_fill_msg200(msg);
    customlen = kat->customhex ? strlen(kat->customhex) : 0;
    wantlen = from_hex(kat->taghex, want, 64);

    for (i = 0; i < nfirsts; i++) {
        gotlen = kmac_chunked(fam->macname, key, keylen,
                              (const unsigned char *)kat->customhex, customlen,
                              kat->xof, kat->size, msg, kat->msglen, firsts[i],
                              got, 64);
        snprintf(msgbuf, sizeof(msgbuf),
                 "{%zu,rest} chunked %s differs from the sample %zu KAT",
                 firsts[i], fam->macname, kat->sample);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }

    free_poison_allocations();
}

/* (4) The mac size tracks the requested size, and two identical
 * computations at that size agree. */
void kmac_test_size_param(const struct kmac_family *fam, size_t size)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(4);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    unsigned char *got_short = ALLOCATE_OUTPUT(64);
    EVP_MAC *mac;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    char msgbuf[128];
    size_t keylen, macsize, gotlen, got_shortlen;

    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_fetch(\"%s\") failed",
             fam->macname);
    mac = EVP_MAC_fetch(NULL, fam->macname, NULL);
    CHECK(mac != NULL, msgbuf);
    keylen = from_hex(kmac_key32_hex, key, 32);
    msg[0] = 0;
    msg[1] = 1;
    msg[2] = 2;
    msg[3] = 3;

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    params[1] = OSSL_PARAM_construct_end();
    snprintf(msgbuf, sizeof(msgbuf), "EVP_MAC_CTX_set_params(size=%zu) failed",
             size);
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1, msgbuf);
    memcpy(kcopy, key, keylen);
    snprintf(msgbuf, sizeof(msgbuf), "%s init failed", fam->macname);
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1, msgbuf);
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    snprintf(msgbuf, sizeof(msgbuf), "%s mac size does not track the size param",
             fam->macname);
    CHECK(macsize == size, msgbuf);
    EVP_MAC_CTX_free(ctx);

    /* SP 800-185 encodes the requested length in the KMAC input, so a
     * size-byte KMAC is a different function from a default-size KMAC (no
     * truncation relation); just verify determinism and the length. */
    gotlen = kmac_oneshot(fam->macname, key, keylen, NULL, 0, 0, size, msg, 4,
                          got, 64);
    got_shortlen = kmac_oneshot(fam->macname, key, keylen, NULL, 0, 0, size,
                                msg, 4, got_short, 64);
    snprintf(msgbuf, sizeof(msgbuf),
             "%s with size %zu did not produce %zu bytes",
             fam->macname, size, size);
    CHECK(gotlen == size && got_shortlen == size, msgbuf);
    unpoison_all();
    CHECK(memcmp(got, got_short, size) == 0,
          "two identical KMAC computations disagree");
    repoison_all();
    EVP_MAC_free(mac);

    free_poison_allocations();
}
