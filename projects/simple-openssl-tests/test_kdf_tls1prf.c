#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_tls1prf -- the TLS 1.0/1.1 and TLS 1.2 PRFs through the EVP_KDF
 * public API: NIST-style "master secret" KATs for the MD5-SHA1 combo (TLS
 * <= 1.1, which splits the secret into two halves) and for SHA-256 (TLS
 * 1.2), multi-chunk seeds, an odd-length secret, and the error paths.
 *
 * Known-answer vectors (verified against the authoritative pizfix OpenSSL
 * 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen 48 -kdfopt digest:MD5-SHA1 \
 *       -kdfopt hexsecret:<secret> -kdfopt 'seed:master secret' \
 *       -kdfopt hexseed:<client_random> -kdfopt hexseed:<server_random> \
 *       TLS1-PRF
 *
 * and the same command under the system OpenSSL 3.0.13; the values are the
 * NIST vectors distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_tls11_prf.txt and
 * evpkdf_tls12_prf.txt)):
 *
 *   TLS 1.1, MD5-SHA1, 48-byte secret, seed = "master secret" ||
 *   client_random || server_random, 48 bytes out
 *     -> 2f6962dfbc744c4b2138bb6b3d33054c...e998f62
 *   TLS 1.2, SHA-256, same shape
 *     -> 202c88c00f84a17a2002707960478746...51288bf
 *
 * (The task brief's "202c88c2f1816c92..." memory was a typo: the real
 * SHA-256 output starts 202c88c0..., as both CLI builds and OpenSSL's own
 * test data agree.)
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_tls1prf: %s\n", msg);
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

/* Copies an ASCII string into a labeled buffer; returns its length. */
static size_t from_str(const char *s, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(s);
    if (n >= dstcap)
        test_failed("string constant larger than its destination buffer");
    memcpy(dst, s, n);
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

/* One TLS1-PRF derive with a fresh context; up to three seed chunks.
 * A NULL chunk pointer terminates the list.  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  TLS1-PRF is core OpenSSL 3 functionality: a
 * fetch failure fails the program (never SKIP). */
static int tls1prf_derive(const char *digest, const unsigned char *secret,
                          size_t secretlen, const unsigned char *seed1,
                          size_t seed1len, const unsigned char *seed2,
                          size_t seed2len, const unsigned char *seed3,
                          size_t seed3len, unsigned char *out,
                          size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "TLS1-PRF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[7];
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core TLS1-PRF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    if (secret != NULL) {
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET,
                                                      (void *)secret,
                                                      secretlen);
        params[2] = OSSL_PARAM_construct_end();
    } else {
        params[1] = OSSL_PARAM_construct_end();
    }
    ok = EVP_KDF_CTX_set_params(ctx, params) == 1;
    if (ok) {
        OSSL_PARAM seedparams[5];
        seedparams[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                          (void *)seed1,
                                                          seed1len);
        seedparams[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                          (void *)seed2,
                                                          seed2len);
        seedparams[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                          (void *)seed3,
                                                          seed3len);
        seedparams[3] = OSSL_PARAM_construct_end();
        ok = EVP_KDF_derive(ctx, out, outlen, seedparams) > 0;
    }
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* NIST TLS PRF vectors as distributed in OpenSSL's test data. */
static const char *const SECRET11_HEX =
    "bded7fa5c1699c010be23dd06ada3a48349f21e5f86263d512c0c5cc379f0e78"
    "0ec55d9844b2f1db02a96453513568d0";
static const char *const CLIENT_RANDOM11_HEX =
    "e5acaf549cd25c22d964c0d930fa4b5261d2507fad84c33715b7b9a864020693";
static const char *const SERVER_RANDOM11_HEX =
    "135e4d557fdf3aa6406d82975d5c606a9734c9334b42136e96990fbd5358cdb2";
static const char *const OUT11_HEX =
    "2f6962dfbc744c4b2138bb6b3d33054c5ecc14f24851d9896395a44ab3964efc"
    "2090c5bf51a0891209f46c1e1e998f62";

static const char *const SECRET12_HEX =
    "f8938ecc9edebc5030c0c6a441e213cd24e6f770a50dda07876f8d55da062bca"
    "db386b411fd4fe4313a604fce6c17fbc";
static const char *const CLIENT_RANDOM12_HEX =
    "36c129d01a3200894b9179faac589d9835d58775f9b5ea3587cb8fd0364cae8c";
static const char *const SERVER_RANDOM12_HEX =
    "f6c9575ed7ddd73e1f7d16eca115415812a43c2b747daaaae043abfb50053fce";
static const char *const OUT12_HEX =
    "202c88c00f84a17a20027079604787461176455539e705be730890602c289a50"
    "01e34eeb3a043e5d52a65e66125188bf";

#define SECRET_CAP 48
#define SEED_CAP 32
#define LABEL_CAP 16
#define OUT_CAP 48

/* (1) The two NIST master-secret KATs, with the seed delivered as three
 * chunks (label, client_random, server_random) -- TLS1-PRF accepts up to
 * six seed chunks (TLSPRF_MAX_SEEDS). */
static void test_kats_multi_seed(void)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *cr = ALLOCATE_SALT(SEED_CAP);
    unsigned char *sr = ALLOCATE_SALT(SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen, labellen, crlen, srlen;
    char msg[128];

    secretlen = from_hex(SECRET11_HEX, secret, SECRET_CAP);
    labellen = from_str("master secret", label, LABEL_CAP);
    crlen = from_hex(CLIENT_RANDOM11_HEX, cr, SEED_CAP);
    srlen = from_hex(SERVER_RANDOM11_HEX, sr, SEED_CAP);

    CHECK(tls1prf_derive("MD5-SHA1", secret, secretlen, label, labellen,
                         cr, crlen, sr, srlen, out, OUT_CAP),
          "TLS 1.1 (MD5-SHA1) master-secret derive failed");
    from_hex(OUT11_HEX, want, OUT_CAP);
    expect_equal("TLS 1.1 MD5-SHA1 output does not match the KAT",
                 out, OUT_CAP, want, OUT_CAP);

    /* The same KAT truncated to 16 bytes (prefix property). */
    CHECK(tls1prf_derive("MD5-SHA1", secret, secretlen, label, labellen,
                         cr, crlen, sr, srlen, out, 16),
          "TLS 1.1 16-byte derive failed");
    expect_equal("TLS 1.1 16-byte output is not the KAT prefix",
                 out, 16, want, 16);

    secretlen = from_hex(SECRET12_HEX, secret, SECRET_CAP);
    crlen = from_hex(CLIENT_RANDOM12_HEX, cr, SEED_CAP);
    srlen = from_hex(SERVER_RANDOM12_HEX, sr, SEED_CAP);
    CHECK(tls1prf_derive("SHA256", secret, secretlen, label, labellen,
                         cr, crlen, sr, srlen, out, OUT_CAP),
          "TLS 1.2 (SHA-256) master-secret derive failed");
    from_hex(OUT12_HEX, want, OUT_CAP);
    snprintf(msg, sizeof(msg),
             "TLS 1.2 SHA-256 output does not match the KAT");
    expect_equal(msg, out, OUT_CAP, want, OUT_CAP);
}

/* (2) Seed chunks may also be pre-concatenated: one seed with the label and
 * both randoms glued together must give the identical output. */
static void test_concat_seed(void)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *concat = ALLOCATE_SALT(LABEL_CAP + 2 * SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen, concatlen;

    secretlen = from_hex(SECRET11_HEX, secret, SECRET_CAP);
    concatlen = from_str("master secret", concat, LABEL_CAP + 2 * SEED_CAP);
    concatlen += from_hex(CLIENT_RANDOM11_HEX, concat + concatlen,
                          2 * SEED_CAP);
    concatlen += from_hex(SERVER_RANDOM11_HEX, concat + concatlen,
                          2 * SEED_CAP);
    from_hex(OUT11_HEX, want, OUT_CAP);

    CHECK(tls1prf_derive("MD5-SHA1", secret, secretlen, concat, concatlen,
                         NULL, 0, NULL, 0, out, OUT_CAP),
          "TLS1-PRF derive with a single concatenated seed failed");
    expect_equal("three-seed and concatenated-seed outputs differ",
                 out, OUT_CAP, want, OUT_CAP);
}

/* (3) An odd-length secret (TLS <= 1.1 splits it into two halves sharing
 * the middle byte) and an explicitly empty secret both derive
 * deterministically.  The empty-secret value is an OpenSSL edge case: an
 * explicitly set zero-length secret is accepted (and both the 3.0.13 and
 * 3.6.5 CLI agree on the result), while omitting the secret entirely is
 * rejected -- see test_missing_inputs(). */
static void test_secret_shapes(void)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *empty = ALLOCATE_KEY(1);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *seed = ALLOCATE_SALT(SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen, labellen, seedlen, wantlen;

    /* Drop the last byte of the 48-byte TLS 1.1 secret -> 47 bytes. */
    secretlen = from_hex(SECRET11_HEX, secret, SECRET_CAP) - 1;
    labellen = from_str("master secret", label, LABEL_CAP);
    wantlen = from_hex("cfb8713445f0cf55d2c70823f9cbad483d27c8306edfbfcaa"
                       "b43d162c2452114", want, OUT_CAP);
    CHECK(tls1prf_derive("MD5-SHA1", secret, secretlen, label, labellen,
                         NULL, 0, NULL, 0, out, 32),
          "derive with a 47-byte (odd) secret failed");
    expect_equal("odd-length secret output does not match the KAT",
                 out, 32, want, wantlen);

    seedlen = from_hex("01020304050607", seed, SEED_CAP);
    wantlen = from_hex("59238473e0148cb7595d8d80a82ee5a9", want, OUT_CAP);
    CHECK(tls1prf_derive("SHA256", empty, 0, seed, seedlen,
                         NULL, 0, NULL, 0, out, 16),
          "derive with an explicitly empty secret failed");
    expect_equal("empty-secret output does not match the KAT",
                 out, 16, want, wantlen);
}

/* (4) Omitting the secret or the seed must fail. */
static void test_missing_inputs(void)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *seed = ALLOCATE_SALT(SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen = from_hex(SECRET11_HEX, secret, SECRET_CAP);
    size_t seedlen = from_hex("01020304050607", seed, SEED_CAP);

    CHECK(!tls1prf_derive("SHA256", NULL, 0, seed, seedlen,
                          NULL, 0, NULL, 0, out, 16),
          "TLS1-PRF accepted a missing secret");
    CHECK(!tls1prf_derive("SHA256", secret, secretlen, NULL, 0,
                          NULL, 0, NULL, 0, out, 16),
          "TLS1-PRF accepted a missing seed");
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats_multi_seed();
    test_concat_seed();
    test_secret_shapes();
    test_missing_inputs();

    printf("test_kdf_tls1prf: all %u checks passed\n", g_checks);
    return 0;
}
