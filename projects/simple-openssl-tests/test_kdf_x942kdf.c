#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_x942kdf -- the X9.42 KDF (ANSI X9.42, the RFC 2631 KEK
 * derivation, ASN.1 OtherInfo form) through the EVP_KDF public API: the
 * RFC 3565 example, the UKM form, a partyU-info variant and a 64-byte
 * multi-block output, plus the error paths.
 *
 * Spec note: the brief's "plain digest form" does not exist in the
 * provider -- a recognized "cekalg" (content-encryption key algorithm,
 * e.g. id-aes128-wrap) is mandatory (MISSING_CEK_ALG otherwise), because
 * the OtherInfo DER blob always embeds the key-wrap algorithm OID and the
 * bit length of the derived key.  partyU-info serves as the free-form
 * "info" input.
 *
 * Known-answer vectors (RFC 3565 B.2 / OpenSSL test data
 * test/recipes/30-test_evp_data/evpkdf_x942.txt; every value re-verified
 * with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt digest:SHA1 \
 *       -kdfopt hexsecret:<ZZ> -kdfopt cekalg:id-aes128-wrap X942KDF
 *
 * and the same command under the system OpenSSL 3.0.13).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_x942kdf: %s\n", msg);
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

/* One X942KDF derive with a fresh context.  Returns 1 on success, 0 on
 * failure.  X942KDF is core OpenSSL 3 functionality: a fetch failure fails
 * the program (never SKIP). */
static int x942_derive(const char *digest, const unsigned char *zz,
                       size_t zzlen, const char *cekalg,
                       const unsigned char *ukm, size_t ukmlen,
                       const unsigned char *partyu, size_t partyulen,
                       unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "X942KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core X942KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   (char *)digest, 0);
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET,
                                                    (void *)zz, zzlen);
    if (cekalg != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CEK_ALG,
                                                       (char *)cekalg, 0);
    if (ukm != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_UKM,
                                                        (void *)ukm, ukmlen);
    if (partyu != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_X942_PARTYUINFO, (void *)partyu, partyulen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* The RFC 3565 shared secret ZZ = 000102..13 (20 bytes). */
static const char *const ZZ_HEX =
    "000102030405060708090a0b0c0d0e0f10111213";
static const char *const UKM_HEX =
    "0123456789abcdeffedcba98765432010123456789abcdeffedcba9876543201"
    "0123456789abcdeffedcba98765432010123456789abcdeffedcba9876543201";

#define ZZ_CAP 20
#define UKM_CAP 64
#define INFO_CAP 16
#define OUT_CAP 64

/* (1) KATs: the RFC 3565 example, the UKM form, a partyU-info variant and
 * a 64-byte (multi-block) output. */
static void test_kats(void)
{
    unsigned char *zz = ALLOCATE_KEY(ZZ_CAP);
    unsigned char *ukm = ALLOCATE_SALT(UKM_CAP);
    unsigned char *partyu = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zzlen = from_hex(ZZ_HEX, zz, ZZ_CAP);
    size_t ukmlen, partyulen, wantlen;

    /* RFC 3565 B.2: AES-128 key wrap KEK, 16 bytes. */
    wantlen = from_hex("d6d6b094c1027a7de6e3117294a35364", want, OUT_CAP);
    CHECK(x942_derive("SHA1", zz, zzlen, "id-aes128-wrap", NULL, 0,
                      NULL, 0, out, 16),
          "X942KDF RFC 3565 derive failed");
    expect_equal("RFC 3565 AES-128-wrap KEK does not match the KAT",
                 out, 16, want, wantlen);

    /* AES-256 key wrap with the 64-byte UKM, 24 bytes. */
    ukmlen = from_hex(UKM_HEX, ukm, UKM_CAP);
    wantlen = from_hex("8890585c4e281a5c1167caa530bed59b3230d893cba8f922",
                       want, OUT_CAP);
    CHECK(x942_derive("SHA1", zz, zzlen, "id-aes256-wrap", ukm, ukmlen,
                      NULL, 0, out, 24),
          "X942KDF AES-256-wrap + UKM derive failed");
    expect_equal("AES-256-wrap KEK with a UKM does not match the KAT",
                 out, 24, want, wantlen);

    /* The same inputs, 64 bytes: spans several hash blocks. */
    wantlen = from_hex("bf18251eb937b8c61a4a936fdf498e941ca88a5fe79f4aae"
                       "62a40ac3dd40e7ba79a15adfdbd8dbd0e29ecf8744ab62d7"
                       "cf4ef943325bbfe250108d7f1b2e42d8",
                       want, OUT_CAP);
    CHECK(x942_derive("SHA1", zz, zzlen, "id-aes256-wrap", NULL, 0,
                      NULL, 0, out, 64),
          "X942KDF 64-byte derive failed");
    expect_equal("64-byte X942KDF output does not match the CLI value",
                 out, 64, want, wantlen);

    /* partyU-info as the free-form info input, 16 bytes. */
    partyulen = from_hex("0123456789abcdef", partyu, INFO_CAP);
    wantlen = from_hex("bac52e37cdcfe3b06f6618545d9697eb", want, OUT_CAP);
    CHECK(x942_derive("SHA1", zz, zzlen, "id-aes128-wrap", NULL, 0,
                      partyu, partyulen, out, 16),
          "X942KDF partyU-info derive failed");
    expect_equal("partyU-info output does not match the KAT",
                 out, 16, want, wantlen);
}

/* (2) Error paths: the cekalg and the shared secret are mandatory, and an
 * unrecognized cekalg is rejected. */
static void test_negative(void)
{
    unsigned char *zz = ALLOCATE_KEY(ZZ_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zzlen = from_hex(ZZ_HEX, zz, ZZ_CAP);

    CHECK(!x942_derive("SHA1", zz, zzlen, NULL, NULL, 0, NULL, 0, out, 16),
          "X942KDF accepted a missing cekalg");
    CHECK(!x942_derive("SHA1", NULL, 0, "id-aes128-wrap", NULL, 0, NULL, 0,
                       out, 16),
          "X942KDF accepted a missing shared secret");
    CHECK(!x942_derive("SHA1", zz, zzlen, "id-aes128-cbc", NULL, 0, NULL, 0,
                       out, 16),
          "X942KDF accepted an unknown cekalg");
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_negative();

    printf("test_kdf_x942kdf: all %u checks passed\n", g_checks);
    return 0;
}
