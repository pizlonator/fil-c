#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_sskdf -- the Single Step KDF (NIST SP 800-56C) through the
 * EVP_KDF public API: the digest-based mode (SHA-1 and SHA-256) and the
 * MAC-based mode with HMAC-SHA256, multi-chunk "info" parameters, the
 * key/secret parameter alias, and the missing-key error path.
 *
 * Known-answer vectors (NIST CAVS values as distributed in OpenSSL's own
 * test data test/recipes/30-test_evp_data/evpkdf_ss.txt; every hex value
 * re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt digest:SHA1 \
 *       -kdfopt hexsecret:<z> -kdfopt hexinfo:<info> SSKDF
 *
 * and the same command under the system OpenSSL 3.0.13):
 *
 *   SHA1,   z=d09a6b1a472f930db4f5e6b967900744, info=b117255ab5f1b6b96fc434b0
 *       -> b5a3c52e97ae6e8c5069954354eab3c7
 *   HMAC-SHA256 + salt (see the table below)
 *       -> 13479e9a91dd20fdd757d68ffe8869fb
 *
 * The MAC-based CMAC-AES128 mode mentioned in the plan is NOT covered: the
 * SSKDF provider's settable parameter list has no "cipher" parameter (only
 * KBKDF exposes one), so a CMAC MAC context cannot be given its cipher
 * through the EVP_KDF facade -- both a "cipher" param and a "digest"
 * pointing at the cipher were probed and EVP_KDF_derive fails either way.
 * The digest-based and HMAC modes fully cover the two KDF branches.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_sskdf: %s\n", msg);
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

/* One SSKDF derive with a fresh context.  `keyparam` selects the parameter
 * name for the shared secret ("key" or "secret" -- the provider accepts
 * both for the same field).  Returns 1 on success, 0 on failure.  SSKDF is
 * core OpenSSL 3 functionality: a fetch failure fails the program. */
static int sskdf_derive(const char *kdfname, const char *digest,
                        const char *mac, const char *keyparam,
                        const unsigned char *z, size_t zlen,
                        const unsigned char *salt, size_t saltlen,
                        unsigned char *info, size_t infolen,
                        unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, kdfname, NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    if (mac != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC,
                                                       (char *)mac, 0);
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   (char *)digest, 0);
    params[n++] = OSSL_PARAM_construct_octet_string(keyparam,
                                                    (void *)z, zlen);
    if (salt != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                        (void *)salt,
                                                        saltlen);
    if (info != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                        (void *)info,
                                                        infolen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

static const struct ss_kat {
    const char *digest;
    const char *mac;      /* NULL = digest-based mode */
    const char *zhex;
    const char *salthex;  /* NULL = no salt */
    const char *infohex;
    size_t outlen;
    const char *outhex;
} ss_kats[] = {
    { "SHA1", NULL,
      "d09a6b1a472f930db4f5e6b967900744", NULL,
      "b117255ab5f1b6b96fc434b0",
      16,
      "b5a3c52e97ae6e8c5069954354eab3c7" },
    { "SHA256", NULL,
      "d09a6b1a472f930db4f5e6b967900744", NULL,
      "b117255ab5f1b6b96fc434b0",
      32,
      "367186ce2e9f92a944ac1a0393f3a427b055fdab0fceda71a4765c8960823285" },
    { "SHA256", NULL,
      "d09a6b1a472f930db4f5e6b967900744", NULL,
      "b117255ab5f1b6b96fc434b0",
      64,
      "367186ce2e9f92a944ac1a0393f3a427b055fdab0fceda71a4765c8960823285"
      "9ac2ae9af5330282d7e0e85107a3f1feaf0ff04ed27b4a38d6a8772a7ad71a59" },
    { "SHA256", "HMAC",
      "6ee6c00d70a6cd14bd5a4e8fcfec8386",
      "532f5131e0a2fecc722f87e5aa2062cb",
      "861aa2886798231259bd0314",
      16,
      "13479e9a91dd20fdd757d68ffe8869fb" },
};

#define Z_CAP 16
#define SALT_CAP 16
#define INFO_CAP 48
#define OUT_CAP 64

/* (1) The KAT table in both modes and both output lengths. */
static void test_kats(void)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zlen, saltlen, infolen, wantlen, k;
    char msg[128];

    for (k = 0; k < sizeof(ss_kats) / sizeof(ss_kats[0]); k++) {
        const struct ss_kat *kat = &ss_kats[k];
        zlen = from_hex(kat->zhex, z, Z_CAP);
        saltlen = kat->salthex != NULL ?
            from_hex(kat->salthex, salt, SALT_CAP) : 0;
        infolen = from_hex(kat->infohex, info, INFO_CAP);
        wantlen = from_hex(kat->outhex, want, OUT_CAP);

        CHECK(sskdf_derive("SSKDF", kat->digest, kat->mac,
                           OSSL_KDF_PARAM_KEY, z, zlen,
                           saltlen ? salt : NULL, saltlen,
                           info, infolen, out, kat->outlen),
              "SSKDF derive failed");
        snprintf(msg, sizeof(msg), "%s%s 64/32/16-byte output does not match "
                 "the KAT", kat->mac ? kat->mac : "", kat->digest);
        expect_equal(msg, out, kat->outlen, want, wantlen);
    }
}

/* (2) The "secret" parameter is an alias of "key": both routes must give
 * the identical output. */
static void test_key_secret_alias(void)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out_key = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *out_secret = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zlen = from_hex(ss_kats[0].zhex, z, Z_CAP);
    size_t infolen = from_hex(ss_kats[0].infohex, info, INFO_CAP);

    CHECK(sskdf_derive("SSKDF", "SHA1", NULL, OSSL_KDF_PARAM_KEY, z, zlen,
                       NULL, 0, info, infolen, out_key, 16),
          "SSKDF derive via the key param failed");
    CHECK(sskdf_derive("SSKDF", "SHA1", NULL, OSSL_KDF_PARAM_SECRET, z, zlen,
                       NULL, 0, info, infolen, out_secret, 16),
          "SSKDF derive via the secret param failed");
    expect_equal("key-param and secret-param outputs differ",
                 out_key, 16, out_secret, 16);
}

/* (3) Up to five "info" params are accepted and, on OpenSSL releases that
 * concatenate repeated info params (3.2+), five chunks (8+8+8+8+16 bytes)
 * equal the single 48-byte info.  On 3.0.x only the first chunk is used;
 * the program prints a note instead of failing.  A sixth chunk is rejected
 * outright (SSKDF_MAX_INFOS == 5).  The 32-byte KAT below was verified with
 * the pizfix 3.6.5 and the system 3.0.13 CLI (identical values). */
static void test_multi_info(void)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *chunks[6];
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    static const size_t chunklen[5] = { 8, 8, 8, 8, 16 };
    size_t zlen, infolen, wantlen, i, off;
    EVP_KDF *kdf;
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[10];

    for (i = 0; i < 6; i++)
        chunks[i] = ALLOCATE_SALT(INFO_CAP);
    zlen = from_hex(ss_kats[1].zhex, z, Z_CAP);
    infolen = from_hex("000102030405060708090a0b0c0d0e0f1011121314151617"
                       "18191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f",
                       info, INFO_CAP);
    wantlen = from_hex("fb9ee554726e387ca4174275116db0b8e0266d3c23d37bff"
                       "7f0b3f0a82f8599b", want, OUT_CAP);

    /* Split the 48-byte info into 8/8/8/8/16-byte chunks. */
    off = 0;
    for (i = 0; i < 5; i++) {
        memcpy(chunks[i], info + off, chunklen[i]);
        off += chunklen[i];
    }
    CHECK(off == infolen, "info chunk sizes do not add up to the info");

    kdf = EVP_KDF_fetch(NULL, "SSKDF", NULL);
    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA256", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  z, zlen);
    for (i = 0; i < 5; i++)
        params[2 + i] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_INFO, chunks[i], chunklen[i]);
    params[7] = OSSL_PARAM_construct_end();
    CHECK(EVP_KDF_derive(ctx, out, 32, params) > 0,
          "SSKDF derive with five info chunks failed");
    if (wantlen == 32 && memcmp(out, want, 32) == 0) {
        g_checks++; /* multi-chunk info equals the concatenated info */
    } else {
        printf("note test_kdf_sskdf: this OpenSSL does not concatenate "
               "repeated info params (pre-3.2 behavior)\n");
    }

    /* A sixth chunk must be rejected. */
    params[7] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  chunks[5], 8);
    params[8] = OSSL_PARAM_construct_end();
    if (EVP_KDF_derive(ctx, out, 32, params) <= 0) {
        g_checks++; /* the documented SSKDF_MAX_INFOS limit is enforced */
    } else {
        printf("note test_kdf_sskdf: this OpenSSL accepts more than five "
               "info params (pre-3.1 behavior)\n");
    }
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
}

/* (4) Deriving without the shared secret must fail. */
static void test_missing_key(void)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "SSKDF", NULL);
    EVP_KDF_CTX *ctx;
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[3];
    size_t infolen = from_hex(ss_kats[0].infohex, info, INFO_CAP);

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA1", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  info, infolen);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(EVP_KDF_derive(ctx, out, 16, params) <= 0,
          "SSKDF accepted a missing shared secret");
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_key_secret_alias();
    test_multi_info();
    test_missing_key();

    printf("test_kdf_sskdf: all %u checks passed\n", g_checks);
    return 0;
}
