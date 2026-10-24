#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_hmac -- EVP_MAC "HMAC" with digests SHA256/SHA384/SHA512
 * (RFC 4231) and SHA1/MD5 (RFC 2202).
 *
 * Known-answer vectors, all verified two ways before embedding: with
 * pizfix/bin/openssl, e.g.
 *
 *   printf '<hex data>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -digest sha256 -macopt hexkey:<key> HMAC
 *
 * and independently with Python's hmac module.  The RFC 4231 vectors are
 * test cases 1-4, 6 and 7 for SHA256/384/512 (test case 5 is a truncation
 * case, which EVP cannot express and is therefore omitted); the RFC 2202
 * vectors are test cases 1-4, 6 and 7 for SHA1 (20-byte keys) and MD5
 * (16-byte keys).  TC6/TC7 use 131-byte (RFC 4231) and 80-byte (RFC 2202)
 * keys, both larger than the respective block sizes, and TC7 uses data
 * longer than one block.
 *
 * Coverage: one-shot KATs for every vector; chunked data updates == one
 * shot; key sizes 0 (empty key: a zero-length but non-NULL key, which
 * HMAC pads with zeros), 20, 32 and 131 bytes; the empty-key/zero-key
 * equivalence; EVP_MAC_CTX_get_mac_size == digest size; and a clean
 * failure when the digest parameter names a nonexistent digest.
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
    fprintf(stderr, "FAIL test_mac_hmac: %s\n", msg);
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
    static const char tc6[] =
        "Test Using Larger Than Block-Size Key - Hash Key First";
    static const char tc7[] =
        "This is a test using a larger than block-size key and a larger "
        "than block-size data. The key needs to be hashed before being "
        "used by the HMAC algorithm.";
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

/* (1) + (2) RFC 4231 test cases 1-4, 6 and 7 for SHA256/SHA384/SHA512:

 * one-shot == KAT and a chunked pass == one-shot. */

static void test_rfc4231(void)
{
    static const char *const digests[] = { "SHA256", "SHA384", "SHA512" };
    static const size_t maclens[] = { 32, 48, 64 };
    static const char *const katnames[] = {
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
        "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b", "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
        "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6", "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649", "88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e55966144b2a5ab39dc13814b94e3ab6e101a34f27",
        "3e8a69b7783c25851933ab6290af6ca77a9981480850009cc5577c6e1f573b4e6801dd23c4a7d679ccf8a386c674cffb", "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952", "6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82461e99c5a678cc31e799176d3860e6110c46523e",
        "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854", "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737", "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb",
        "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3dba91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd", "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598", "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58",
    };
    unsigned char *key_tc1 = ALLOCATE_KEY(20);
    unsigned char *key_tc4 = ALLOCATE_KEY(25);
    unsigned char *key_tc6 = ALLOCATE_KEY(131);
    unsigned char *data1 = ALLOCATE_INPUT(8);
    unsigned char *data2 = ALLOCATE_INPUT(32);
    unsigned char *data3 = ALLOCATE_INPUT(50);
    unsigned char *data4 = ALLOCATE_INPUT(50);
    unsigned char *data6 = ALLOCATE_INPUT(54);
    unsigned char *data7 = ALLOCATE_INPUT(152);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    unsigned char *got2 = ALLOCATE_OUTPUT(64);
    size_t len_tc7 = 0, d, t;
    char msg[128];

    rfc4231_material(key_tc1, key_tc4, key_tc6, data1, data2, data3, data4,
                     data6, data7, &len_tc7);

    for (d = 0; d < 3; d++) {
        EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
        EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
        OSSL_PARAM params[2];
        unsigned char probekey[20];
        size_t macsize, i;

        for (i = 0; i < sizeof(probekey); i++)
            probekey[i] = 0x0b;
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                     (char *)digests[d], 0);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
              "EVP_MAC_CTX_set_params(digest) failed");
        /* EVP_MAC_CTX_get_mac_size is only meaningful once the context
         * has been initialized. */
        CHECK(EVP_MAC_init(ctx, probekey, sizeof(probekey), NULL) == 1,
              "EVP_MAC_init with a probe key failed");
        macsize = EVP_MAC_CTX_get_mac_size(ctx);
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        snprintf(msg, sizeof(msg), "%s: EVP_MAC_CTX_get_mac_size is not the digest size",
                 digests[d]);
        CHECK(macsize == maclens[d], msg);

        for (t = 0; t < 6; t++) {
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
            wantlen = from_hex(katnames[d * 6 + t], want, 64);
            gotlen = hmac_oneshot(digests[d], key, keylen, data, datalen, got, 64);
            snprintf(msg, sizeof(msg), "RFC 4231 %s TC%d one-shot does not match the KAT",
                     digests[d], (int[]){1, 2, 3, 4, 6, 7}[t]);
            expect_equal(msg, got, gotlen, want, wantlen);

            got2len = hmac_chunked(digests[d], key, keylen, data, datalen, 1, 17,
                                   got2, 64);
            snprintf(msg, sizeof(msg), "RFC 4231 %s TC%d chunked pass differs from one-shot",
                     digests[d], (int[]){1, 2, 3, 4, 6, 7}[t]);
            expect_equal(msg, got2, got2len, got, gotlen);
        }
    }
}

/* (3) RFC 2202 test cases 1-4, 6 and 7 for SHA1 (20-byte keys) and MD5

 * (16-byte keys). */

static void test_rfc2202(void)
{
    static const struct {
        const char *digest;
        size_t keylen;   /* TC1-TC4 key length */
    } variants[] = {
        { "SHA1", 20 },
        { "MD5", 16 },
    };
    static const char *const katnames[] = {
        "b617318655057264e28bc0b6fb378c8ef146be00", "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79", "125d7342b9ac11cd91a39af48aa17b4f63f175d3",
        "fcce74c708f32de54cc7e91045628f9575dde8cf", "aa4ae5e15272d00e95705637ce8a3b55ed402112", "ee8089bab17872f013f572dba60ef38e32573f15",
        "9294727a3638bb1c13f48ef8158bfc9d", "750c783e6ab0b503eaa86e310a5db738", "56be34521d144c88dbb8c733f0e8b3f6",
        "997cf5e360d235a8d1f38022d1d8aa29", "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd", "038bcf9d93e2454bcc86a53c20b9e329",
    };
    unsigned char *key = ALLOCATE_KEY(80);
    unsigned char *data1 = ALLOCATE_INPUT(8);
    unsigned char *data2 = ALLOCATE_INPUT(32);
    unsigned char *data3 = ALLOCATE_INPUT(50);
    unsigned char *data4 = ALLOCATE_INPUT(50);
    unsigned char *data6 = ALLOCATE_INPUT(54);
    unsigned char *data7 = ALLOCATE_INPUT(152);
    unsigned char *want = ALLOCATE_OTHER(20);
    unsigned char *got = ALLOCATE_OUTPUT(20);
    size_t len_tc7 = 0, v, t, i;
    char msg[128];

    {
        static const char tc6[] =
            "Test Using Larger Than Block-Size Key - Hash Key First";
        static const char tc7[] =
            "This is a test using a larger than block-size key and a larger "
            "than block-size data. The key needs to be hashed before being "
            "used by the HMAC algorithm.";
        memcpy(data6, tc6, sizeof(tc6) - 1);
        memcpy(data7, tc7, sizeof(tc7) - 1);
        len_tc7 = sizeof(tc7) - 1;
    }
    memcpy(data1, "Hi There", 8);
    memcpy(data2, "what do ya want for nothing?", 28);
    for (i = 0; i < 50; i++) {
        data3[i] = 0xdd;
        data4[i] = 0xcd;
    }

    for (v = 0; v < 2; v++) {
        size_t macsize = (v == 0) ? 20 : 16;
        for (t = 0; t < 6; t++) {
            size_t keylen, datalen, gotlen, wantlen;
            const unsigned char *data;
            size_t tn = (size_t)(int[]){1, 2, 3, 4, 6, 7}[t];

            switch (t) {
            case 0: keylen = variants[v].keylen; data = data1; datalen = 8; break;
            case 1: keylen = 4; data = data2; datalen = 28; break;
            case 2: keylen = variants[v].keylen; data = data3; datalen = 50; break;
            case 3: keylen = variants[v].keylen; data = data4; datalen = 50; break;
            case 4: keylen = 80; data = data6; datalen = 54; break;
            default: keylen = 80; data = data7; datalen = len_tc7; break;
            }
            for (i = 0; i < keylen; i++)
                key[i] = (t == 0) ? 0x0b : (t == 1 || t == 3) ? (unsigned char)(i + 1)
                                                              : 0xaa;
            if (t == 1)
                memcpy(key, "Jefe", 4);
            wantlen = from_hex(katnames[v * 6 + t], want, 20);
            CHECK(wantlen == macsize, "RFC 2202 KAT length mismatch");
            gotlen = hmac_oneshot(variants[v].digest, key, keylen, data, datalen,
                                  got, 20);
            snprintf(msg, sizeof(msg), "RFC 2202 %s TC%zu one-shot does not match the KAT",
                     variants[v].digest, tn);
            expect_equal(msg, got, gotlen, want, wantlen);
        }
    }
}

/* (4) Key sizes: the empty key (zero length, non-NULL buffer) must work

 * and must equal the 32-zero-byte and 64-zero-byte keys (HMAC zero-pads

 * the key to the block size). */

static void test_key_sizes(void)
{
    static const char *const zerokats[] = {
        "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad"
    };
    unsigned char *empty = ALLOCATE_KEY(1);
    unsigned char *key32 = ALLOCATE_KEY(32);
    unsigned char *key64 = ALLOCATE_KEY(64);
    unsigned char *want = ALLOCATE_OTHER(32);
    unsigned char *got = ALLOCATE_OUTPUT(32);
    size_t wantlen, gotlen;

    wantlen = from_hex(zerokats[0], want, 32);

    gotlen = hmac_oneshot("SHA256", empty, 0, NULL, 0, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "HMAC-SHA256 with the empty key does not match the KAT");

    memset(key32, 0, 32);
    gotlen = hmac_oneshot("SHA256", key32, 32, NULL, 0, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "HMAC-SHA256 with a 32-zero-byte key differs from the empty-key KAT");

    memset(key64, 0, 64);
    gotlen = hmac_oneshot("SHA256", key64, 64, NULL, 0, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "HMAC-SHA256 with a 64-zero-byte key differs from the empty-key KAT");
}

/* (5) A nonexistent digest name must fail cleanly: either the parameter

 * set or the init rejects it, never a silent success. */

static void test_wrong_digest(void)
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

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_rfc4231();
    test_rfc2202();
    test_key_sizes();
    test_wrong_digest();

    printf("test_mac_hmac: all %u checks passed\n", g_checks);
    return 0;
}
