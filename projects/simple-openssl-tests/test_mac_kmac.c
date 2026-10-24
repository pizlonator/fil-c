#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_kmac -- EVP_MAC "KMAC-128" / "KMAC-256" (NIST SP 800-185).
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
 * Portability note: the "custom" parameter must be passed as an
 * OCTET_STRING whose data_size is exactly strlen(custom) (no trailing
 * NUL).  A UTF8_STRING parameter is accepted by OpenSSL 3.0.x but
 * rejected by the 3.6.x type check, and an OCTET_STRING that includes
 * the NUL silently changes the tag (the NUL becomes part of the custom
 * string) -- both forms were probed on both stacks.
 *
 * Coverage: all nine KATs; the absent-custom == empty-custom equivalence;
 * default output sizes (32 for KMAC-128, 64 for KMAC-256); chunked
 * updates over the 200-byte message; EVP_MAC_CTX_get_mac_size reflecting
 * the requested size; and the XOF mode through OSSL_MAC_PARAM_XOF.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_mac_kmac: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* Length-and-content comparison of two working buffers. */
static void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                         const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0)
        CHECK(memcmp(got, want, wantlen) == 0, what);
}

/* One KMAC computation.  custom == NULL means: do not set the parameter
 * at all (the provider default is the empty custom string).  customlen is
 * strlen(custom) -- WITHOUT the NUL, see the header comment.  xof selects
 * the XOF mode (right_encoded(0) length encoding).  size == 0 leaves the
 * provider default (32 for KMAC-128, 64 for KMAC-256).  Returns the tag
 * length (0 on failure). */
static size_t kmac_oneshot(const char *macname, const unsigned char *key,
                           size_t keylen, const unsigned char *custom,
                           size_t customlen, int xof, size_t size,
                           const unsigned char *data, size_t datalen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, macname, NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[4];
    unsigned char kcopy[64];
    char customcopy[64];
    size_t n = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch failed for KMAC-128/KMAC-256");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0)
        params[n++] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    if (custom != NULL) {
        CHECK(customlen < sizeof(customcopy),
              "custom string longer than the working buffer");
        memcpy(customcopy, custom, customlen);
        /* OCTET_STRING with exactly customlen bytes: portable across
         * OpenSSL 3.0 - 3.6 (see the header comment). */
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

/* KMAC with the message fed in updates of {a, rest} bytes. */
static size_t kmac_chunked(const char *macname, const unsigned char *key,
                           size_t keylen, const unsigned char *custom,
                           size_t customlen, int xof, size_t size,
                           const unsigned char *data, size_t datalen,
                           size_t a, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, macname, NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[4];
    unsigned char kcopy[64];
    char customcopy[64];
    size_t n = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch failed for KMAC-128/KMAC-256");
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

static const char *const KEY32_HEX =
    "404142434445464748494a4b4c4d4e4f"
    "505152535455565758595a5b5c5d5e5f";
static const char *const CUSTOM = "My Tagged Application";

static const struct kmac_kat {
    const char *macname;
    const char *msghex;     /* NULL: the 200-byte 00..c7 message */
    size_t msglen;
    const char *customhex;  /* NULL: no custom parameter at all */
    int xof;
    size_t size;
    const char *taghex;
} kmac_kats[] = {
    { "KMAC-128", "00010203", 4, "", 0, 32,
      "e5780b0d3ea6f7d3a429c5706aa43a00fadbd7d49628839e3187243f456ee14e" },
    { "KMAC-128", "00010203", 4, "My Tagged Application", 0, 32,
      "3b1fba963cd8b0b59e8c1a6d71888b7143651af8ba0a7070c0979e2811324aa5" },
    { "KMAC-128", NULL, 200, "My Tagged Application", 0, 32,
      "1f5b4e6cca02209e0dcb5ca635b89a15e271ecc760071dfd805faa38f9729230" },
    { "KMAC-256", "00010203", 4, "My Tagged Application", 0, 64,
      "20c570c31346f703c9ac36c61c03cb64c3970d0cfc787e9b79599d273a68d2f7f69d4cc3de9d104a351689f27cf6f5951f0103f33f4f24871024d9c27773a8dd" },
    { "KMAC-256", NULL, 200, "", 0, 64,
      "75358cf39e41494e949707927cee0af20a3ff553904c86b08f21cc414bcfd691589d27cf5e15369cbbff8b9a4c2eb17800855d0235ff635da82533ec6b759b69" },
    { "KMAC-256", NULL, 200, "My Tagged Application", 0, 64,
      "b58618f71f92e1d56c1b8c55ddd7cd188b97b4ca4d99831eb2699a837da2e4d970fbacfde50033aea585f1a2708510c32d07880801bd182898fe476876fc8965" },
    { "KMAC-128", "00010203", 4, NULL, 1, 32,
      "cd83740bbd92ccc8cf032b1481a0f4460e7ca9dd12b08a0c4031178bacd6ec35" },
    { "KMAC-128", "00010203", 4, "My Tagged Application", 1, 32,
      "31a44527b4ed9f5c6101d11de6d26f0620aa5c341def41299657fe9df1a3b16c" },
    { "KMAC-256", "00010203", 4, "My Tagged Application", 1, 64,
      "1755133f1534752aad0748f2c706fb5c784512cab835cd15676b16c0c6647fa96faa7af634a0bf8ff6df39374fa00fad9a39e322a7c92065a64eb1fb0801eb2b" },
};

/* (1) All nine KATs. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t k, keylen, wantlen, gotlen, i;
    char msgbuf[128];

    keylen = from_hex(KEY32_HEX, key, 32);
    for (i = 0; i < 200; i++)
        msg[i] = (unsigned char)i;

    for (k = 0; k < sizeof(kmac_kats) / sizeof(kmac_kats[0]); k++) {
        const struct kmac_kat *kat = &kmac_kats[k];
        size_t customlen = kat->customhex ? strlen(kat->customhex) : 0;

        wantlen = from_hex(kat->taghex, want, 64);
        gotlen = kmac_oneshot(kat->macname, key, keylen,
                              (const unsigned char *)kat->customhex, customlen,
                              kat->xof, kat->size, msg, kat->msglen, got, 64);
        snprintf(msgbuf, sizeof(msgbuf), "%s sample %zu does not match the KAT",
                 kat->macname, k + 1);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }
}

/* (2) Absent custom parameter == empty custom string (both are the empty
 * custom string per SP 800-185), and the default output sizes are 32/64. */
static void test_custom_and_sizes(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(4);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "KMAC-128", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t keylen, wantlen, gotlen, macsize;

    keylen = from_hex(KEY32_HEX, key, 32);
    msg[0] = 0;
    msg[1] = 1;
    msg[2] = 2;
    msg[3] = 3;
    wantlen = from_hex(kmac_kats[0].taghex, want, 64);

    gotlen = kmac_oneshot("KMAC-128", key, keylen, NULL, 0, 0, 32, msg, 4,
                          got, 64);
    expect_equal("absent custom string differs from the empty-custom KAT",
                 got, gotlen, want, wantlen);

    CHECK(mac != NULL, "EVP_MAC_fetch(\"KMAC-128\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params with no params failed");
    memcpy(kcopy, key, keylen);
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1,
          "KMAC-128 init failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    CHECK(macsize == 32, "KMAC-128 default output size is not 32");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

/* (3) Chunked updates: the 200-byte sample 3 message split as {7, rest}
 * and {64, rest} must reproduce the one-shot tags. */
static void test_chunked(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t keylen, wantlen, gotlen, i, customlen;

    keylen = from_hex(KEY32_HEX, key, 32);
    for (i = 0; i < 200; i++)
        msg[i] = (unsigned char)i;
    customlen = strlen(CUSTOM);
    wantlen = from_hex(kmac_kats[2].taghex, want, 64);

    gotlen = kmac_chunked("KMAC-128", key, keylen,
                          (const unsigned char *)CUSTOM, customlen, 0, 32,
                          msg, 200, 7, got, 64);
    expect_equal("{7,rest} chunked KMAC-128 differs from the sample 3 KAT",
                 got, gotlen, want, wantlen);

    gotlen = kmac_chunked("KMAC-128", key, keylen,
                          (const unsigned char *)CUSTOM, customlen, 0, 32,
                          msg, 200, 64, got, 64);
    expect_equal("{64,rest} chunked KMAC-128 differs from the sample 3 KAT",
                 got, gotlen, want, wantlen);

    wantlen = from_hex(kmac_kats[5].taghex, want, 64);
    gotlen = kmac_chunked("KMAC-256", key, keylen,
                          (const unsigned char *)CUSTOM, customlen, 0, 64,
                          msg, 200, 7, got, 64);
    expect_equal("{7,rest} chunked KMAC-256 differs from the sample 6 KAT",
                 got, gotlen, want, wantlen);
}

/* (4) The mac size tracks the requested size (KMAC-256 asked for 40). */
static void test_mac_size_param(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(4);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    unsigned char *got_short = ALLOCATE_OUTPUT(64);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "KMAC-256", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t keylen, size = 40, macsize, gotlen, got_shortlen;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"KMAC-256\") failed");
    keylen = from_hex(KEY32_HEX, key, 32);
    msg[0] = 0;
    msg[1] = 1;
    msg[2] = 2;
    msg[3] = 3;

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(size=40) failed");
    memcpy(kcopy, key, keylen);
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1, "KMAC-256 init failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    CHECK(macsize == 40, "KMAC-256 mac size does not track the size param");
    EVP_MAC_CTX_free(ctx);

    /* SP 800-185 encodes the requested length in the KMAC input, so a
     * 40-byte KMAC is a different function from a 64-byte KMAC (no
     * truncation relation); just verify determinism and the length. */
    gotlen = kmac_oneshot("KMAC-256", key, keylen, NULL, 0, 0, 40, msg, 4,
                          got, 64);
    got_shortlen = kmac_oneshot("KMAC-256", key, keylen, NULL, 0, 0, 40,
                                msg, 4, got_short, 64);
    CHECK(gotlen == 40 && got_shortlen == 40,
          "KMAC-256 with size 40 did not produce 40 bytes");
    CHECK(memcmp(got, got_short, 40) == 0,
          "two identical KMAC computations disagree");
    EVP_MAC_free(mac);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_custom_and_sizes();
    test_chunked();
    test_mac_size_param();

    printf("test_mac_kmac: all %u checks passed\n", g_checks);
    return 0;
}
