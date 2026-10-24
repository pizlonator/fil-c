#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_tls13kdf -- the TLS 1.3 key-schedule KDF through the EVP_KDF
 * public API: extract-stage secrets (RFC 8446 HKDF-Extract and the
 * derive-secret-and-extract step) and expand-stage HKDF-Expand-Label
 * derivations, plus the error paths.
 *
 * Availability note: TLS13-KDF is fetchable on both the system OpenSSL
 * 3.0.13 and the pizfix OpenSSL 3.6.5, so there is no SKIP path here -- a
 * failed fetch fails the program.  (The KDF accepts only EXTRACT_ONLY and
 * EXPAND_ONLY modes; EXTRACT_AND_EXPAND is rejected.)
 *
 * Parameters: "key" is the input/IKM secret for extract and the PRK for
 * expand; "salt" is the previous-generation secret for extract; "prefix"
 * and "label" form the HKDF-Expand-Label label bytes ("tls13 " +
 * label); "data" is the transcript hash for expand.
 *
 * Known-answer vectors: ACVP test vectors as distributed in OpenSSL's own
 * test data (test/recipes/30-test_evp_data/evpkdf_tls13_kdf.txt); every
 * value re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt mode:EXTRACT_ONLY \
 *       -kdfopt digest:SHA2-256 -kdfopt hexkey:<secret> TLS13-KDF
 *
 * and the same command under the system OpenSSL 3.0.13.  The keyless
 * extract reproduces the RFC 8448 early secret (HKDF-Extract with an
 * all-zero salt and empty IKM, exactly the RFC 8446 key schedule).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_tls13kdf: %s\n", msg);
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

/* One TLS13-KDF derive with a fresh context; every octet input is
 * optional (pass NULL to omit).  Returns 1 on success, 0 on failure. */
static int tls13_derive(const char *mode, const char *digest,
                        const unsigned char *key, size_t keylen,
                        const unsigned char *salt, size_t saltlen,
                        const unsigned char *prefix, size_t prefixlen,
                        const unsigned char *label, size_t labellen,
                        const unsigned char *data, size_t datalen,
                        unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "TLS13-KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[8];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for TLS13-KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                   (char *)mode, 0);
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   (char *)digest, 0);
    if (key != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                        (void *)key, keylen);
    if (salt != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                        (void *)salt, saltlen);
    if (prefix != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PREFIX,
                                                        (void *)prefix,
                                                        prefixlen);
    if (label != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_LABEL,
                                                        (void *)label,
                                                        labellen);
    if (data != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_DATA,
                                                        (void *)data, datalen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

static const char *const EARLY_IKM_HEX =
    "f8af6aea2d397baf2948a25b2834200692cff17eee9165e4e27babee9edefd05";
static const char *const EARLY_SECRET_HEX =
    "153b6394a9c03cf3f5accc6e455a7693281138a1bcfa3803c26735dd1194d216";
static const char *const RFC8448_EARLY_HEX =
    "33ad0a1c607ec03b09e6cd9893680ce210adf300aa1f2660e1b22e10f170f92a";
static const char *const DERIVED_IKM_HEX =
    "8e27fad32236cb11bb497eb878d636c3f1599f5ffdfae784cbf73e74746769d4";
static const char *const DERIVED_SALT_HEX =
    "153b6394a9c03cf3f5accc6e455a7693281138a1bcfa3803c26735dd1194d216";
static const char *const DERIVED_OUT_HEX =
    "cbb0740fd37e5eff32b76cf88511eb83fc9694da4130ca48de754c7f80f561bd";
static const char *const PREFIX_HEX = "746c73313320";  /* "tls13 " */
static const char *const DATA_HEX =
    "7c92f68bd5bf3638ea338a6494722e1b44127e1b7e8aad535f2322a644ff22b3";
static const char *const CETRAFFIC_OUT_HEX =
    "c80583a90e995c489600492a5da642e6b1f679ba674828792df087b939636171";
static const char *const EXPMaster_OUT_HEX =
    "1fe336ab43b4a69b11ebc64c8343ed21a9c7f6724702d9d63970e8ff72a4bde7";

#define SECRET_CAP 32
#define PREFIX_CAP 8
#define LABEL_CAP 16
#define DATA_CAP 32
#define OUT_CAP 32

/* (1) Extract-stage KATs: the plain early secret, the derive-secret step
 * with a previous-generation salt, and the keyless extract (zeros, the
 * RFC 8446 "early secret" shape, matching the RFC 8448 value). */
static void test_extract(void)
{
    unsigned char *key = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *salt = ALLOCATE_SALT(SECRET_CAP);
    unsigned char *prefix = ALLOCATE_SALT(PREFIX_CAP);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen, saltlen, prefixlen, labellen, wantlen;

    keylen = from_hex(EARLY_IKM_HEX, key, SECRET_CAP);
    wantlen = from_hex(EARLY_SECRET_HEX, want, OUT_CAP);
    CHECK(tls13_derive("EXTRACT_ONLY", "SHA2-256", key, keylen, NULL, 0,
                       NULL, 0, NULL, 0, NULL, 0, out, 32),
          "TLS13-KDF early-secret extract failed");
    expect_equal("early secret does not match the ACVP KAT",
                 out, 32, want, wantlen);

    saltlen = from_hex(DERIVED_SALT_HEX, salt, SECRET_CAP);
    prefixlen = from_hex(PREFIX_HEX, prefix, PREFIX_CAP);
    labellen = from_str("derived", label, LABEL_CAP);
    keylen = from_hex(DERIVED_IKM_HEX, key, SECRET_CAP);
    wantlen = from_hex(DERIVED_OUT_HEX, want, OUT_CAP);
    CHECK(tls13_derive("EXTRACT_ONLY", "SHA2-256", key, keylen, salt, saltlen,
                       prefix, prefixlen, label, labellen, NULL, 0, out, 32),
          "TLS13-KDF derive-secret extract failed");
    expect_equal("derive-secret extract does not match the ACVP KAT",
                 out, 32, want, wantlen);

    wantlen = from_hex(RFC8448_EARLY_HEX, want, OUT_CAP);
    CHECK(tls13_derive("EXTRACT_ONLY", "SHA2-256", NULL, 0, NULL, 0,
                       NULL, 0, NULL, 0, NULL, 0, out, 32),
          "TLS13-KDF keyless extract failed");
    expect_equal("keyless extract is not the RFC 8448 early secret",
                 out, 32, want, wantlen);
}

/* (2) Expand-stage KATs: HKDF-Expand-Label with the "tls13 " prefix and
 * the transcript hash as data. */
static void test_expand(void)
{
    unsigned char *key = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *prefix = ALLOCATE_SALT(PREFIX_CAP);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *data = ALLOCATE_SALT(DATA_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen, prefixlen, labellen, datalen, wantlen;

    keylen = from_hex(EARLY_SECRET_HEX, key, SECRET_CAP);
    prefixlen = from_hex(PREFIX_HEX, prefix, PREFIX_CAP);
    datalen = from_hex(DATA_HEX, data, DATA_CAP);

    labellen = from_str("c e traffic", label, LABEL_CAP);
    wantlen = from_hex(CETRAFFIC_OUT_HEX, want, OUT_CAP);
    CHECK(tls13_derive("EXPAND_ONLY", "SHA2-256", key, keylen, NULL, 0,
                       prefix, prefixlen, label, labellen, data, datalen,
                       out, 32),
          "TLS13-KDF c e traffic expand failed");
    expect_equal("c e traffic expansion does not match the ACVP KAT",
                 out, 32, want, wantlen);

    labellen = from_str("e exp master", label, LABEL_CAP);
    wantlen = from_hex(EXPMaster_OUT_HEX, want, OUT_CAP);
    CHECK(tls13_derive("EXPAND_ONLY", "SHA2-256", key, keylen, NULL, 0,
                       prefix, prefixlen, label, labellen, data, datalen,
                       out, 32),
          "TLS13-KDF e exp master expand failed");
    expect_equal("e exp master expansion does not match the ACVP KAT",
                 out, 32, want, wantlen);
}

/* (3) Error paths: a missing digest, the unsupported
 * EXTRACT_AND_EXPAND mode and a keyless expand all fail; the keyless
 * extract is legal (zeros, see test_extract). */
static void test_negative(void)
{
    unsigned char *key = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *prefix = ALLOCATE_SALT(PREFIX_CAP);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(EARLY_IKM_HEX, key, SECRET_CAP);
    size_t prefixlen = from_hex(PREFIX_HEX, prefix, PREFIX_CAP);
    size_t labellen = from_str("derived", label, LABEL_CAP);

    CHECK(!tls13_derive("EXTRACT_ONLY", NULL, key, keylen, NULL, 0,
                        NULL, 0, NULL, 0, NULL, 0, out, 32),
          "TLS13-KDF accepted a missing digest");
    CHECK(!tls13_derive("EXTRACT_AND_EXPAND", "SHA2-256", key, keylen,
                        NULL, 0, NULL, 0, NULL, 0, NULL, 0, out, 32),
          "TLS13-KDF accepted EXTRACT_AND_EXPAND");
    CHECK(!tls13_derive("EXPAND_ONLY", "SHA2-256", NULL, 0, NULL, 0,
                        prefix, prefixlen, label, labellen, NULL, 0,
                        out, 32),
          "TLS13-KDF accepted a keyless expand");
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_extract();
    test_expand();
    test_negative();

    printf("test_kdf_tls13kdf: all %u checks passed\n", g_checks);
    return 0;
}
