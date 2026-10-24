#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_krb5kdf -- the Kerberos 5 RFC 3961 "DK" (encryption key) KDF
 * through the EVP_KDF public API: RFC 3961-derived AES KATs, the n-fold
 * boundary case of a block-size constant, and the error paths.
 *
 * Spec note: the task brief called for a "digest" parameter, but the KDF is
 * cipher-based: the provider takes "cipher" (AES-128-CBC / AES-256-CBC for
 * the aes128-cts-hmac-sha1-96 / aes256-cts-hmac-sha1-96 enctypes), "key"
 * (the input key) and "constant" (the key-derivation constant).  The
 * RFC 3961 DES-EDE3-CBC vectors are not covered because DES lives in the
 * legacy provider, which these programs do not load.
 *
 * Known-answer vectors (Kerberos working-group / RFC 3961 n-fold test
 * values as distributed in OpenSSL's own test data
 * test/recipes/30-test_evp_data/evpkdf_krb5.txt; every value re-verified
 * with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt cipher:AES-128-CBC \
 *       -kdfopt hexkey:<key> -kdfopt hexconstant:<constant> KRB5KDF
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
    fprintf(stderr, "FAIL test_kdf_krb5kdf: %s\n", msg);
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

/* One KRB5KDF derive with a fresh context.  cipher/key/constant may be
 * NULL to exercise the "missing ..." error paths.  Returns 1 on success,
 * 0 on failure.  KRB5KDF is core OpenSSL 3 functionality: a fetch failure
 * fails the program (never SKIP). */
static int krb5_derive(const char *cipher, const unsigned char *key,
                       size_t keylen, const unsigned char *constant,
                       size_t constantlen, unsigned char *out,
                       size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "KRB5KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[5];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core KRB5KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    if (cipher != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CIPHER,
                                                       (char *)cipher, 0);
    if (key != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                        (void *)key, keylen);
    if (constant != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_CONSTANT,
                                                        (void *)constant,
                                                        constantlen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* RFC 3961-derived AES-128 key material (from OpenSSL's krb5 test data). */
static const char *const KEY128_HEX = "42263c6e89f4fc28b8df68ee09799f15";
static const char *const KEY256_HEX =
    "fe697b52bc0d3ce14432ba036a92e65bbb52280990a2fa27883998d72af30161";

static const struct krb5_kat {
    const char *cipher;
    const char *keyhex;
    const char *constanthex;
    size_t outlen;
    const char *outhex;
} krb5_kats[] = {
    { "AES-128-CBC", KEY128_HEX, "0000000299", 16,
      "34280a382bc92769b2da2f9ef066854b" },
    { "AES-128-CBC", KEY128_HEX, "00000002aa", 16,
      "5b14fc4e250e14ddf9dccf1af6674f53" },
    { "AES-128-CBC", KEY128_HEX, "0000000255", 16,
      "4ed31063621684f09ae8d89991af3e8f" },
    { "AES-256-CBC", KEY256_HEX, "0000000299", 32,
      "bfab388bdcb238e9f9c98d6a878304f04d30c82556375ac507a7a852790f4674" },
};

#define KEY_CAP 32
#define CONST_CAP 17
#define OUT_CAP 32

/* (1) The KAT table: three constants for AES-128 and one AES-256 vector. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen, constantlen, wantlen, k;
    char msg[128];

    for (k = 0; k < sizeof(krb5_kats) / sizeof(krb5_kats[0]); k++) {
        const struct krb5_kat *kat = &krb5_kats[k];
        keylen = from_hex(kat->keyhex, key, KEY_CAP);
        constantlen = from_hex(kat->constanthex, constant, CONST_CAP);
        wantlen = from_hex(kat->outhex, want, OUT_CAP);

        CHECK(krb5_derive(kat->cipher, key, keylen, constant, constantlen,
                          out, kat->outlen),
              "KRB5KDF derive failed");
        snprintf(msg, sizeof(msg), "%s constant %s does not match the KAT",
                 kat->cipher, kat->constanthex);
        expect_equal(msg, out, kat->outlen, want, wantlen);
    }
}

/* (2) The constant may be up to the cipher block size; a full 16-byte
 * constant takes the n-fold identity path.  Value CLI-verified on both
 * the 3.6.5 and the 3.0.13 builds. */
static void test_blocksize_constant(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(KEY128_HEX, key, KEY_CAP);
    size_t constantlen, wantlen;

    constantlen = from_hex("000102030405060708090a0b0c0d0e0f", constant,
                           CONST_CAP);
    wantlen = from_hex("b6838592d1101aa673277d35a505b052", want, OUT_CAP);
    CHECK(krb5_derive("AES-128-CBC", key, keylen, constant, constantlen,
                      out, 16),
          "KRB5KDF derive with a block-size constant failed");
    expect_equal("block-size constant output does not match the KAT",
                 out, 16, want, wantlen);
}

/* (3) Error paths: the output length must equal the cipher key length, the
 * constant must be non-empty and at most the block size, and the cipher,
 * key and constant are all mandatory. */
static void test_negative(void)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *longconst = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(KEY128_HEX, key, KEY_CAP);
    size_t constantlen = from_hex("0000000299", constant, CONST_CAP);
    size_t longlen = from_hex("000102030405060708090a0b0c0d0e0f10", longconst,
                              CONST_CAP);

    CHECK(!krb5_derive("AES-128-CBC", key, keylen, constant, constantlen,
                       out, 24),
          "KRB5KDF accepted a 24-byte output for AES-128");
    /* An explicitly-set zero-length constant is rejected from OpenSSL 3.1
     * on ("invalid constant length"); 3.0.x only rejects an absent
     * constant and accepts the empty one, so this check is adaptive. */
    if (!krb5_derive("AES-128-CBC", key, keylen, constant, 0, out, 16)) {
        g_checks++;
    } else {
        printf("note test_kdf_krb5kdf: this OpenSSL accepts an explicitly "
               "empty constant (pre-3.1 behavior)\n");
    }    CHECK(!krb5_derive("AES-128-CBC", key, keylen, longconst, longlen,
                       out, 16),
          "KRB5KDF accepted a constant longer than the block size");
    CHECK(!krb5_derive(NULL, key, keylen, constant, constantlen, out, 16),
          "KRB5KDF accepted a missing cipher");
    CHECK(!krb5_derive("AES-128-CBC", NULL, 0, constant, constantlen,
                       out, 16),
          "KRB5KDF accepted a missing key");
    CHECK(!krb5_derive("AES-128-CBC", key, keylen, NULL, 0, out, 16),
          "KRB5KDF accepted a missing constant");
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_blocksize_constant();
    test_negative();

    printf("test_kdf_krb5kdf: all %u checks passed\n", g_checks);
    return 0;
}
