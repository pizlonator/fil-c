#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/opensslv.h>

/*
 * test_kdf_kbkdf -- SP 800-108 key-based KDF (KBKDF) through the EVP_KDF
 * public API: counter and feedback modes with CMAC-AES128 and HMAC PRFs,
 * the use-l / use-separator switches, the "r" counter width where the
 * release supports it, the "seed" parameter as the feedback-mode IV, and
 * the error paths.
 *
 * Parameter naming quirk: KBKDF's "salt" parameter is the SP 800-108
 * *Label*, "info" is the *Context*, and "seed" is the feedback-mode IV.
 *
 * Known-answer vectors: NIST CAVS values as distributed in OpenSSL's own
 * test data (test/recipes/30-test_evp_data/evpkdf_kbkdf_counter.txt), plus
 * feedback-mode values derived with the authoritative pizfix OpenSSL 3.6.5
 * and cross-checked under the system OpenSSL 3.0.13 (both builds agree on
 * every value below):
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt mode:COUNTER \
 *       -kdfopt cipher:AES128 -kdfopt mac:CMAC -kdfopt use-l:0 \
 *       -kdfopt use-separator:0 [-kdfopt r:<8|32>] \
 *       -kdfopt hexkey:<key> -kdfopt hexinfo:<info> KBKDF
 *
 * The "r" parameter does not exist before OpenSSL 3.1, so the r-width
 * vectors are feature-detected via EVP_KDF_settable_ctx_params() and
 * skipped (with a note) where it is absent; KMAC support (3.1+) is
 * detected via EVP_MAC_fetch().
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_kbkdf: %s\n", msg);
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

/* TRUE if the KDF's settable parameter list contains `name`. */
static int kdf_has_param(const char *kdfname, const char *name)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, kdfname, NULL);
    const OSSL_PARAM *defs;
    int found = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core KBKDF KDF");
    for (defs = EVP_KDF_settable_ctx_params(kdf); defs != NULL &&
         defs->key != NULL; defs++) {
        if (strcmp(defs->key, name) == 0) {
            found = 1;
            break;
        }
    }
    EVP_KDF_free(kdf);
    return found;
}

/* One KBKDF derive with a fresh context.  Pass -1 for use_l, use_sep or r
 * to leave that parameter unset; pass NULL buffers to omit optional octet
 * params.  Returns 1 on success, 0 if EVP_KDF_derive failed. */
static int kbkdf_derive(const char *mode, const char *mac, const char *cipher,
                        const char *digest, const unsigned char *key,
                        size_t keylen, const unsigned char *salt,
                        size_t saltlen, const unsigned char *const *infos,
                        const size_t *infolens, size_t ninfos,
                        const unsigned char *iv, size_t ivlen,
                        int use_l, int use_sep, int r,
                        unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "KBKDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[11];
    int ok, n = 0;
    static int use_l_on = 1, use_l_off = 0;
    static int sep_on = 1, sep_off = 0;
    int rval = r;
    size_t i;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core KBKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                   (char *)mode, 0);
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC,
                                                   (char *)mac, 0);
    if (cipher != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CIPHER,
                                                       (char *)cipher, 0);
    if (digest != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                       (char *)digest, 0);
    params[n++] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_KBKDF_USE_L,
                                           use_l < 0 ? &use_l_on :
                                           (use_l ? &use_l_on : &use_l_off));
    params[n++] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_KBKDF_USE_SEPARATOR,
                                           use_sep < 0 ? &sep_on :
                                           (use_sep ? &sep_on : &sep_off));
    if (r >= 0)
        params[n++] = OSSL_PARAM_construct_int("r", &rval);
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                    (void *)key, keylen);
    if (salt != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                        (void *)salt,
                                                        saltlen);
    for (i = 0; i < ninfos; i++)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                        (void *)infos[i],
                                                        infolens[i]);
    if (iv != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                        (void *)iv, ivlen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* CAVS counter-mode vector, CMAC-AES128, RLEN=32 bits. */
static const char *const K32_HEX = "c10b152e8c97b77e18704e0f0bd38305";
static const char *const I32_HEX =
    "98cd4cbbbebe15d17dc86e6dbad800a2dcbd64f7c7ad0e78e9cf94ffdba89d03"
    "e97eadf6c4f7b806caf52aa38f09d0eb71d71f497bcc6906b48d36c4";
static const char *const OUT32_HEX = "26faf61908ad9ee881b8305c221db53f";

/* CAVS counter-mode vector, CMAC-AES128, RLEN=8 bits (needs "r"). */
static const char *const K8_HEX = "dff1e50ac0b69dc40f1051d46c2b069c";
static const char *const I8_HEX =
    "c16e6e02c5a3dcc8d78b9ac1306877761310455b4e41469951d9e6c2245a064b"
    "33fd8c3b01203a7824485bf0a64060c4648b707d2607935699316ea5";
static const char *const OUT8_HEX = "8be8f0869b3c0ba97b71863d1b9f7813";

/* CAVS counter-mode vectors, HMAC (need "r"). */
static const char *const KSHA1_HEX = "00a39bd547fb88b2d98727cf64c195c61e1cad6c";
static const char *const ISHA1_HEX =
    "98132c1ffaf59ae5cbc0a3133d84c551bb97e0c75ecaddfc30056f6876f598030"
    "09bffc7d75c4ed46f40b8f80426750d15bc1ddb14ac5dcb69a68242";
static const char *const OUTSHA1_HEX = "0611e1903609b47ad7a5fc2c82e47702";
static const char *const OUTSHA256R8_HEX =
    "9de01850c9d3d1cd0270b3ea9c9c683d";

/* Small HMAC-SHA256 material for mode/switch behavior. */
static const char *const KSMALL_HEX = "0ef9";
static const char *const ISMALL_HEX = "56ec";
static const char *const IV32_HEX =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

#define KEY_CAP 64
#define INFO_CAP 64
#define IV_CAP 32
#define OUT_CAP 48

/* (1) Counter mode, CMAC-AES128, default 32-bit counter: the CAVS KAT. */
static void test_counter_cmac(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(K32_HEX, key, KEY_CAP);
    size_t infolen = from_hex(I32_HEX, info, INFO_CAP);
    size_t wantlen = from_hex(OUT32_HEX, want, OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];

    infos[0] = info;
    infolens[0] = infolen;
    CHECK(kbkdf_derive("COUNTER", "CMAC", "AES128", NULL, key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                       out, 16),
          "KBKDF counter/CMAC derive failed");
    expect_equal("counter-mode CMAC-AES128 output does not match the CAVS KAT",
                 out, 16, want, wantlen);
}

/* (2) HMAC PRFs in counter mode: the CAVS HMAC-SHA1 r=8 vector, the CAVS
 * HMAC-SHA256 r=8 vector, and the switch behavior (use-l=1 and
 * use-separator=1 both change the output; the values below are
 * CLI-verified on both builds).  The r=8 vectors need the "r" parameter,
 * which OpenSSL 3.0 does not have -- feature-detected, with a note. */
static void test_counter_hmac(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen, infolen, wantlen;
    int has_r = kdf_has_param("KBKDF", "r");

    /* HMAC-SHA256 with both switches off and the default counter width:
     * identical on 3.0.13 and 3.6.5. */
    keylen = from_hex(KSMALL_HEX, key, KEY_CAP);
    infolen = from_hex(ISMALL_HEX, info, INFO_CAP);
    wantlen = from_hex("7ab5f99606c3801a1d4239b53bed3a51", want, OUT_CAP);
    infos[0] = info;
    infolens[0] = infolen;
    CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                       out, 16),
          "KBKDF counter/HMAC-SHA256 derive failed");
    expect_equal("counter-mode HMAC-SHA256 output does not match the KAT",
                 out, 16, want, wantlen);

    /* use-l=1 appends the output length to the fixed data. */
    wantlen = from_hex("00f64fac5b9c4de1dea987ab8bfb5cf8", want, OUT_CAP);
    CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 1, 0, -1,
                       out, 16),
          "KBKDF counter/HMAC-SHA256 use-l=1 derive failed");
    expect_equal("use-l=1 output does not match the KAT", out, 16, want,
                 wantlen);

    /* use-separator=1 inserts a zero byte before the fixed data. */
    wantlen = from_hex("d3958375825f4663e485093d936e8f45", want, OUT_CAP);
    CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 1, -1,
                       out, 16),
          "KBKDF counter/HMAC-SHA256 use-separator=1 derive failed");
    expect_equal("use-separator=1 output does not match the KAT",
                 out, 16, want, wantlen);

    if (has_r) {
        /* CAVS CMAC-AES128, r=8. */
        keylen = from_hex(K8_HEX, key, KEY_CAP);
        infolen = from_hex(I8_HEX, info, INFO_CAP);
        infolens[0] = infolen;
        wantlen = from_hex(OUT8_HEX, want, OUT_CAP);
        CHECK(kbkdf_derive("COUNTER", "CMAC", "AES128", NULL, key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, 8,
                           out, 16),
              "KBKDF counter/CMAC-AES128 r=8 derive failed");
        expect_equal("CMAC-AES128 r=8 output does not match the CAVS KAT",
                     out, 16, want, wantlen);

        /* CAVS HMAC-SHA1, r=8. */
        keylen = from_hex(KSHA1_HEX, key, KEY_CAP);
        infolen = from_hex(ISHA1_HEX, info, INFO_CAP);
        infolens[0] = infolen;
        wantlen = from_hex(OUTSHA1_HEX, want, OUT_CAP);
        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA1", key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, 8,
                           out, 16),
              "KBKDF counter/HMAC-SHA1 r=8 derive failed");
        expect_equal("HMAC-SHA1 r=8 output does not match the CAVS KAT",
                     out, 16, want, wantlen);

        /* CAVS HMAC-SHA256, r=8. */
        keylen = from_hex(KSMALL_HEX, key, KEY_CAP);
        infolen = from_hex(ISMALL_HEX, info, INFO_CAP);
        infolens[0] = infolen;
        wantlen = from_hex(OUTSHA256R8_HEX, want, OUT_CAP);
        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, 8,
                           out, 16),
              "KBKDF counter/HMAC-SHA256 r=8 derive failed");
        expect_equal("HMAC-SHA256 r=8 output does not match the CAVS KAT",
                     out, 16, want, wantlen);

        /* An unsupported counter width must be rejected. */
        CHECK(!kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", key, keylen,
                            NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, 12,
                            out, 16),
              "KBKDF accepted r=12");
    } else {
        printf("note test_kdf_kbkdf: this OpenSSL has no \"r\" parameter; "
               "skipping the r=8 CAVS vectors\n");
    }
}

/* (3) Feedback mode with and without the IV ("seed"), whose length must
 * equal the PRF output size. */
static void test_feedback(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *iv = ALLOCATE_SALT(IV_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen, infolen, ivlen, wantlen;

    keylen = from_hex(KSMALL_HEX, key, KEY_CAP);
    infolen = from_hex(ISMALL_HEX, info, INFO_CAP);
    infos[0] = info;
    infolens[0] = infolen;

    /* No IV: the first block is just HMAC(key, info). */
    wantlen = from_hex("7ab5f99606c3801a1d4239b53bed3a51ad39f7da479dc406e"
                       "6bdb22d974c6ce6", want, OUT_CAP);
    CHECK(kbkdf_derive("FEEDBACK", "HMAC", NULL, "SHA256", key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                       out, 32),
          "KBKDF feedback/HMAC without an IV failed");
    expect_equal("feedback-mode output without an IV does not match the KAT",
                 out, 32, want, wantlen);

    /* 32-byte IV (== the SHA-256 output size). */
    ivlen = from_hex(IV32_HEX, iv, IV_CAP);
    wantlen = from_hex("3979b30978322e221c0bb9a45d76ca1e6d54bd7dc8e8df46"
                       "55c0cb82590eb391", want, OUT_CAP);
    CHECK(kbkdf_derive("FEEDBACK", "HMAC", NULL, "SHA256", key, keylen,
                       NULL, 0, infos, infolens, 1, iv, ivlen, 0, 0, -1,
                       out, 32),
          "KBKDF feedback/HMAC with a 32-byte IV failed");
    expect_equal("feedback-mode output with an IV does not match the KAT",
                 out, 32, want, wantlen);

    /* A wrongly sized IV must be rejected. */
    CHECK(!kbkdf_derive("FEEDBACK", "HMAC", NULL, "SHA256", key, keylen,
                        NULL, 0, infos, infolens, 1, iv, ivlen - 1, 0, 0,
                        -1, out, 32),
          "KBKDF accepted a 31-byte feedback IV for SHA-256");

    /* Feedback mode with CMAC-AES128 and a 16-byte IV. */
    keylen = from_hex(K8_HEX, key, KEY_CAP);
    infolen = from_hex(I8_HEX, info, INFO_CAP);
    infolens[0] = infolen;
    ivlen = from_hex("000102030405060708090a0b0c0d0e0f", iv, IV_CAP);
    wantlen = from_hex("c498818b00696bf1fa9f289d61cbb627", want, OUT_CAP);
    CHECK(kbkdf_derive("FEEDBACK", "CMAC", "AES128", NULL, key, keylen,
                       NULL, 0, infos, infolens, 1, iv, ivlen, 0, 0, -1,
                       out, 16),
          "KBKDF feedback/CMAC with a 16-byte IV failed");
    expect_equal("feedback-mode CMAC output does not match the KAT",
                 out, 16, want, wantlen);
}

/* (4) KMAC128 as the PRF (OpenSSL 3.1+); multi-chunk info (OpenSSL 3.2+
 * concatenates repeated info params -- the CAVS-style three-chunk vector
 * reproduces the single-chunk result there). */
static void test_kmac_and_multi_info(void)
{
    /* KBKDF accepts KMAC PRFs from OpenSSL 3.1 on (3.0 has the KMAC MAC
     * itself but KBKDF rejects it as a PRF), so gate on the version. */
    int has_kmac = OPENSSL_version_major() > 3 ||
        (OPENSSL_version_major() == 3 && OPENSSL_version_minor() >= 1);
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *label = ALLOCATE_SALT(INFO_CAP);
    unsigned char *chunks[3];
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[3];
    size_t infolens[3];
    size_t keylen, labellen, infolen, wantlen, i, off;
    int has_r = kdf_has_param("KBKDF", "r");

    if (!has_kmac) {
        printf("note test_kdf_kbkdf: KBKDF does not support KMAC PRFs in "
               "this OpenSSL; skipping the KMAC check\n");
    } else {
        keylen = from_hex("00112233445566778899aabbccddeeff", key, KEY_CAP);
        labellen = from_hex("0102030405060708", label, INFO_CAP);
        infolen = from_hex("aabbccdd", info, INFO_CAP);
        wantlen = from_hex("9609c1c2b59258b88427e569c8f4ba9a1b2003274435e2"
                           "97578890281a64694b", want, OUT_CAP);
        infos[0] = info;
        infolens[0] = infolen;
        CHECK(kbkdf_derive("COUNTER", "KMAC128", NULL, NULL, key, keylen,
                           label, labellen, infos, infolens, 1, NULL, 0,
                           -1, -1, -1, out, 32),
              "KBKDF KMAC128 derive failed");
        expect_equal("KMAC128 output does not match the KAT",
                     out, 32, want, wantlen);
    }

    /* Three info chunks (HMAC-SHA512, r=32 CAVS data) reproduce the
     * single-chunk result where both "r" and repeated-info concat exist. */
    if (!has_r)
        return;
    keylen = from_hex("a9f4a2c5af839867f5db5a1e520ab3cca72a166ca60de512fd"
                      "7fe7e64cf94f92cf1d8b636175f293e003275e021018c3f0ede"
                      "495997a505ec9a2afeb0495be57", key, KEY_CAP);
    infolen = from_hex("8e9db3335779db688bcfe096668d9c3bc64e193e3529c430"
                       "e68d09d56c837dd6c0f94678f121a68ee1feea4735da85a49"
                       "d34a5290aa39f7b40de435f", info, INFO_CAP);
    wantlen = from_hex("6db880daac98b078ee389a2164252ded61322d661e2b4924"
                       "7ea921e544675d8f17af2bf66dd40d81af9388c6537bdee9",
                       want, OUT_CAP);
    for (i = 0; i < 3; i++)
        chunks[i] = ALLOCATE_SALT(INFO_CAP);
    off = 0;
    for (i = 0; i < 3; i++) {
        size_t n = (i < 2) ? 28 : infolen - off;
        memcpy(chunks[i], info + off, n);
        infos[i] = chunks[i];
        infolens[i] = n;
        off += n;
    }
    CHECK(off == infolen, "info chunks do not add up to the info");
    CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA512", key, keylen,
                       NULL, 0, infos, infolens, 3, NULL, 0, 0, 0, 32,
                       out, 48),
          "KBKDF three-info-chunk derive failed");
    if (wantlen == 48 && memcmp(out, want, 48) == 0) {
        g_checks++; /* multi-chunk info equals the concatenated info */
    } else {
        printf("note test_kdf_kbkdf: this OpenSSL does not concatenate "
               "repeated info params (pre-3.2 behavior)\n");
    }
}

/* (5) A zero output length must fail, and so must a missing key. */
static void test_negative(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen = from_hex(KSMALL_HEX, key, KEY_CAP);
    size_t infolen = from_hex(ISMALL_HEX, info, INFO_CAP);

    infos[0] = info;
    infolens[0] = infolen;
    CHECK(!kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", key, keylen,
                        NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                        out, 0),
          "KBKDF accepted a zero output length");
    CHECK(!kbkdf_derive("COUNTER", "HMAC", NULL, "SHA256", NULL, 0,
                        NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                        out, 16),
          "KBKDF accepted a missing key");
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_counter_cmac();
    test_counter_hmac();
    test_feedback();
    test_kmac_and_multi_info();
    test_negative();

    printf("test_kdf_kbkdf: all %u checks passed\n", g_checks);
    return 0;
}
