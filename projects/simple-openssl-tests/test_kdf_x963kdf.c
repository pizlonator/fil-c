#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_x963kdf -- the X9.63 KDF (ANSI X9.63, a.k.a. the "concat KDF"
 * used by SEC1 ECDH) through the EVP_KDF public API: the NIST SHA-256
 * vector with output lengths that span counter increments (32, 80 and 103
 * bytes), the prefix property, and the missing-key error path.
 *
 * Known-answer vector (NIST value as distributed in OpenSSL's own test
 * data test/recipes/30-test_evp_data/evpkdf_x963.txt; every length below
 * re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:SHA256 \
 *       -kdfopt hexsecret:<secret> -kdfopt hexinfo:<sharedinfo> X963KDF
 *
 * and the same command under the system OpenSSL 3.0.13):
 *
 *   secret (Z) = 22518b10e70f2a3f243810ae3254139efbee04aa57c7af7d  (24 bytes)
 *   sharedinfo    = 75eef81aa3041e33b80971203d2c0c52              (16 bytes)
 *
 * The 103-byte output spans four SHA-256 counter blocks.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_x963kdf: %s\n", msg);
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

/* One X963KDF derive with a fresh context.  Returns 1 on success, 0 on
 * failure.  X963KDF is core OpenSSL 3 functionality: a fetch failure fails
 * the program (never SKIP). */
static int x963_derive(const char *digest, const unsigned char *key,
                       size_t keylen, const unsigned char *sharedinfo,
                       size_t silen, unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "X963KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[5];
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core X963KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  (void *)key, keylen);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  (void *)sharedinfo,
                                                  silen);
    params[3] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

static const char *const Z_HEX =
    "22518b10e70f2a3f243810ae3254139efbee04aa57c7af7d";
static const char *const SI_HEX =
    "75eef81aa3041e33b80971203d2c0c52";
static const char *const OUT103_HEX =
    "c498af77161cc59f2962b9a713e2b215152d139766ce34a776df11866a69bf2e"
    "52a13d9c7c6fc878c50c5ea0bc7b00e0da2447cfd874f6cf92f30d0097111485"
    "500c90c3af8b487872d04685d14c8d1dc8d7fa08beb0ce0ababc11f0bd496269"
    "142d43525a78e5";

#define Z_CAP 24
#define SI_CAP 16
#define OUT_CAP 103

/* (1) The NIST vector at 103 bytes and the 32/80-byte prefixes (each output
 * length is the prefix of any longer output because the counter only ever
 * increments; 32 -> one block, 80 -> three, 103 -> four). */
static void test_lengths(void)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *si = ALLOCATE_SALT(SI_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zlen = from_hex(Z_HEX, z, Z_CAP);
    size_t silen = from_hex(SI_HEX, si, SI_CAP);
    size_t wantlen = from_hex(OUT103_HEX, want, OUT_CAP);
    static const size_t lens[] = { 32, 80, 103 };
    size_t i;

    for (i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        char msg[128];
        CHECK(x963_derive("SHA256", z, zlen, si, silen, out, lens[i]),
              "X963KDF derive failed");
        snprintf(msg, sizeof(msg),
                 "X963KDF output of %zu bytes does not match the KAT",
                 lens[i]);
        expect_equal(msg, out, lens[i], want, lens[i]);
    }
    /* And a 103-byte derive fills the whole reference value. */
    CHECK(x963_derive("SHA256", z, zlen, si, silen, out, 103),
          "X963KDF 103-byte derive failed");
    expect_equal("X963KDF full output does not match the NIST vector",
                 out, 103, want, wantlen);
}

/* (2) Deriving without the shared secret must fail. */
static void test_missing_key(void)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "X963KDF", NULL);
    EVP_KDF_CTX *ctx;
    unsigned char *si = ALLOCATE_SALT(SI_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[3];
    size_t silen = from_hex(SI_HEX, si, SI_CAP);

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core X963KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA256", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  si, silen);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(EVP_KDF_derive(ctx, out, 32, params) <= 0,
          "X963KDF accepted a missing shared secret");
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_lengths();
    test_missing_key();

    printf("test_kdf_x963kdf: all %u checks passed\n", g_checks);
    return 0;
}
