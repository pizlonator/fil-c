#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_gmac -- EVP_MAC "GMAC" (GCM restricted to AAD-only input).
 *
 * Known-answer vectors:
 *
 *   TC1 (McGrew/Viega shape): the all-zero 16-byte key, the all-zero
 *   12-byte IV, and no data at all -> tag 58e2fccefa7e3061367f1d57a4e7455a,
 *   which is exactly the published GCM test case 1 tag (GMAC with no data
 *   IS GCM with no plaintext).
 *
 *   AAD20: key feffe992...08308, IV cafebabe...f888, and the 20-byte AAD
 *   feedface...abaddad2 -> 346434fd51d5cd0c5887ec63e39b907a, the same
 *   value the AES-GCM AAD-only path produces (cross-checked inside this
 *   program against EVP's GCM cipher API, on both the system 3.0.13 and
 *   the pizfix 3.6.5 stacks).
 *
 * Note on the CLI: `openssl mac -macopt iv:...` hands the value to the
 * provider as a *text* string, so the pizfix CLI alone cannot verify a
 * hex IV; the constants above were verified through the EVP API (scratch
 * probe) and cross-checked against the GCM cipher path.
 *
 * Coverage: the TC1 KAT; the 20-byte AAD KAT plus the in-test GMAC ==
 * GCM-AAD-only cross-check; chunked AAD updates == one shot; tampered AAD
 * changes the tag (GMAC verification is a comparison, so this is not a
 * failure -- the tags must simply differ); IV length 0 must fail; 8- and
 * 16-byte IVs agree with the GCM path; and EVP_MAC_CTX_get_mac_size == 16
 * (queried after init, where it is meaningful).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_mac_gmac: %s\n", msg);
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

/* One-shot GMAC: the cipher through the "cipher" parameter, the IV
 * through OSSL_MAC_PARAM_IV, the key through EVP_MAC_init.  AAD is fed
 * with EVP_MAC_update (GMAC has no plaintext input).  Returns the tag
 * length, or 0 on failure. */
static size_t gmac_oneshot(const unsigned char *key, size_t keylen,
                           const unsigned char *iv, size_t ivlen,
                           const unsigned char *aad, size_t aadlen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "GMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[3];
    unsigned char kcopy[32], ivcopy[64];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"GMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)"AES-128-GCM", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_IV,
                                                  ivcopy, ivlen);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(ivlen <= sizeof(ivcopy), "IV longer than the working IV buffer");
    memcpy(ivcopy, iv, ivlen);
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher/iv) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && aadlen > 0)
        ok = EVP_MAC_update(ctx, aad, aadlen);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* GMAC with the AAD fed in updates of {a, b, rest} bytes. */
static size_t gmac_chunked(const unsigned char *key, size_t keylen,
                           const unsigned char *iv, size_t ivlen,
                           const unsigned char *aad, size_t aadlen,
                           size_t a, size_t b, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "GMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[3];
    unsigned char kcopy[32], ivcopy[64];
    size_t off = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"GMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)"AES-128-GCM", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_IV,
                                                  ivcopy, ivlen);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(ivlen <= sizeof(ivcopy), "IV longer than the working IV buffer");
    memcpy(ivcopy, iv, ivlen);
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher/iv) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && a > 0 && aadlen > 0) {
        size_t n = a < aadlen ? a : aadlen;
        ok = EVP_MAC_update(ctx, aad, n);
        off = n;
    }
    if (ok && b > 0 && off < aadlen) {
        size_t n = b < aadlen - off ? b : aadlen - off;
        ok = EVP_MAC_update(ctx, aad + off, n);
        off += n;
    }
    if (ok && off < aadlen)
        ok = EVP_MAC_update(ctx, aad + off, aadlen - off);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* The same AAD-only computation through EVP's GCM *cipher* API: AAD is
 * fed first with out == NULL, there is no plaintext, and the tag is read
 * with EVP_CTRL_AEAD_GET_TAG.  GMAC must agree with this exactly. */
static void gcm_aad_only_tag(const unsigned char *key, size_t keylen,
                             const unsigned char *iv, size_t ivlen,
                             const unsigned char *aad, size_t aadlen,
                             unsigned char *tag, size_t taglen)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0;
    unsigned char dummy[16];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "GCM cipher key length does not match the KAT key");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen,
                              NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    if (aadlen > 0)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, dummy, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen,
                              tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

static const char *const KEY_ZERO_HEX =
    "00000000000000000000000000000000";
static const char *const IV12_ZERO_HEX =
    "000000000000000000000000";
static const char *const KEY128_HEX =
    "feffe9928665731c6d6a8f9467308308";
static const char *const IV12_HEX =
    "cafebabefacedbaddecaf888";
static const char *const AAD20_HEX =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";

/* (1) The TC1 KAT: zero key, zero 12-byte IV, no data. */
static void test_tc1(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(12);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t wantlen, gotlen;

    from_hex(KEY_ZERO_HEX, key, 16);
    from_hex(IV12_ZERO_HEX, iv, 12);
    wantlen = from_hex("58e2fccefa7e3061367f1d57a4e7455a", want, 16);
    gotlen = gmac_oneshot(key, 16, iv, 12, NULL, 0, got, 16);
    expect_equal("GMAC TC1 (zero key, zero IV, no data) does not match the KAT",
                 got, gotlen, want, wantlen);
}

/* (2) The 20-byte AAD KAT, plus the in-test cross-check against the GCM

 * cipher's AAD-only tag, plus chunked AAD updates. */

static void test_aad20(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    unsigned char *got2 = ALLOCATE_OUTPUT(16);
    unsigned char *gcm_tag = ALLOCATE_OTHER(16);
    size_t wantlen, gotlen, got2len;

    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 12);
    from_hex(AAD20_HEX, aad, 20);
    wantlen = from_hex("346434fd51d5cd0c5887ec63e39b907a", want, 16);

    gotlen = gmac_oneshot(key, 16, iv, 12, aad, 20, got, 16);
    expect_equal("GMAC over the 20-byte AAD does not match the KAT",
                 got, gotlen, want, wantlen);

    gcm_aad_only_tag(key, 16, iv, 12, aad, 20, gcm_tag, 16);
    expect_equal("GMAC tag differs from the GCM AAD-only tag",
                 got, gotlen, gcm_tag, 16);

    got2len = gmac_chunked(key, 16, iv, 12, aad, 20, 5, 7, got2, 16);
    expect_equal("chunked AAD updates differ from the one-shot GMAC",
                 got2, got2len, got, gotlen);
}

/* (3) A tampered AAD must produce a different tag: GMAC verification is

 * a comparison of tags, so the test only requires the tags to differ. */

static void test_tampered_aad(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    unsigned char *got_bad = ALLOCATE_OUTPUT(16);
    size_t gotlen, got_badlen;

    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 12);
    from_hex(AAD20_HEX, aad, 20);

    gotlen = gmac_oneshot(key, 16, iv, 12, aad, 20, got, 16);
    aad[0] ^= 0x01;
    got_badlen = gmac_oneshot(key, 16, iv, 12, aad, 20, got_bad, 16);
    CHECK(gotlen == 16 && got_badlen == 16, "GMAC tag lengths are wrong");
    CHECK(memcmp(got, got_bad, 16) != 0,
          "tampering with one AAD bit did not change the GMAC tag");
}

/* (4) IV lengths: 0 must fail; 8 and 16 must agree with the GCM path. */
static void test_iv_lengths(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv8 = ALLOCATE_IV(8);
    unsigned char *iv16 = ALLOCATE_IV(16);
    unsigned char *iv0 = ALLOCATE_IV(1);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    unsigned char *gcm_tag = ALLOCATE_OTHER(16);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "GMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[3];
    unsigned char kcopy[16];
    size_t i, gotlen, gcm_len = 16;
    int okset, okinit;

    from_hex(KEY128_HEX, key, 16);
    from_hex(AAD20_HEX, aad, 20);
    for (i = 0; i < 8; i++)
        iv8[i] = (unsigned char)(0x10 + i);
    for (i = 0; i < 16; i++)
        iv16[i] = (unsigned char)(0x20 + i);
    iv0[0] = 0;

    /* IV length 0: the parameter set itself is rejected on every OpenSSL
     * 3.x tested (gmac_set_ctx_params -> SET_IVLEN 0), and init must not
     * succeed either way. */
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)"AES-128-GCM", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_IV,
                                                  iv0, 0);
    params[2] = OSSL_PARAM_construct_end();
    okset = EVP_MAC_CTX_set_params(ctx, params);
    memcpy(kcopy, key, 16);
    okinit = okset ? EVP_MAC_init(ctx, kcopy, 16, NULL) : 0;
    CHECK(!(okset && okinit), "GMAC accepted a zero-length IV");
    EVP_MAC_CTX_free(ctx);

    /* 8- and 16-byte IVs: GMAC must agree with the GCM cipher path. */
    gotlen = gmac_oneshot(key, 16, iv8, 8, aad, 20, got, 16);
    CHECK(gotlen == 16, "8-byte IV GMAC failed");
    gcm_aad_only_tag(key, 16, iv8, 8, aad, 20, gcm_tag, gcm_len);
    CHECK(memcmp(got, gcm_tag, 16) == 0,
          "8-byte IV GMAC differs from the GCM AAD-only tag");

    gotlen = gmac_oneshot(key, 16, iv16, 16, aad, 20, got, 16);
    CHECK(gotlen == 16, "16-byte IV GMAC failed");
    gcm_aad_only_tag(key, 16, iv16, 16, aad, 20, gcm_tag, gcm_len);
    CHECK(memcmp(got, gcm_tag, 16) == 0,
          "16-byte IV GMAC differs from the GCM AAD-only tag");

    EVP_MAC_free(mac);
}

/* (5) The tag size is the GCM tag size, 16 bytes (queried after init). */
static void test_mac_size(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(12);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "GMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[3];
    unsigned char kcopy[16], ivcopy[12];
    size_t macsize;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"GMAC\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 12);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)"AES-128-GCM", 0);
    memcpy(ivcopy, iv, 12);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_IV,
                                                  ivcopy, 12);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher/iv) failed");
    memcpy(kcopy, key, 16);
    CHECK(EVP_MAC_init(ctx, kcopy, 16, NULL) == 1, "EVP_MAC_init failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    CHECK(macsize == 16, "GMAC EVP_MAC_CTX_get_mac_size is not 16");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_tc1();
    test_aad20();
    test_tampered_aad();
    test_iv_lengths();
    test_mac_size();

    printf("test_mac_gmac: all %u checks passed\n", g_checks);
    return 0;
}
