#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

#include <stdint.h>

/*
 * test_kdf_scrypt -- scrypt (RFC 7914) through the EVP_KDF public API:
 * the two published 64-byte RFC vectors, parameter validation (N, r) and
 * the "maxmem_bytes" memory limit.
 *
 * Known-answer vectors (verified against the authoritative pizfix OpenSSL
 * 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen 64 -kdfopt hexpass:<pass> \
 *       -kdfopt hexsalt:<salt> -kdfopt n:<N> -kdfopt r:<r> -kdfopt p:<p> \
 *       id-scrypt
 *
 * and the same command under the system OpenSSL 3.0.13):
 *
 *   RFC 7914 section 12: P="password"       S="NaCl"
 *     N=1024, r=8, p=16, dkLen=64
 *       -> fdbabe1c9d3472007856e7190d01e9fe...dfa2cc0640
 *   RFC 7914 section 12: P="pleaseletmein"  S="SodiumChloride"
 *     N=16384, r=8, p=1, dkLen=64
 *       -> 7023bdcb3afd7348461c06cd81fd38eb...7b45575887
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_scrypt: %s\n", msg);
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

/* One scrypt derive with a fresh context.  maxmem 0 means "the default".
 * Returns 1 on success, 0 if EVP_KDF_derive failed.  scrypt is core OpenSSL
 * 3 functionality: a fetch failure fails the program (never SKIP). */
static int scrypt_derive(unsigned char *pass, size_t passlen,
                         unsigned char *salt, size_t saltlen,
                         uint64_t N, uint32_t r,
                         uint32_t p, uint64_t maxmem,
                         unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "SCRYPT", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[7];
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core scrypt KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                  (void *)pass, passlen);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  (void *)salt, saltlen);
    params[2] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_N, &N);
    params[3] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_R, &r);
    params[4] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_P, &p);
    if (maxmem != 0) {
        params[5] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_MAXMEM,
                                                &maxmem);
        params[6] = OSSL_PARAM_construct_end();
    } else {
        params[5] = OSSL_PARAM_construct_end();
    }
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

static const struct scrypt_kat {
    const char *passhex;
    const char *salthex;
    unsigned long long N;
    unsigned long r;
    unsigned long p;
    const char *dkhex;
} scrypt_kats[] = {
    { /* "password" */
      "70617373776f7264",
      "4e61436c",   /* "NaCl" */
      1024, 8, 16,
      "fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b373162"
      "2eaf30d92e22a3886ff109279d9830dac727afb94a83ee6d8360cbdfa2cc0640" },
    { /* "pleaseletmein" */
      "706c656173656c65746d65696e",
      "536f6469756d43686c6f72696465",   /* "SodiumChloride" */
      16384, 8, 1,
      "7023bdcb3afd7348461c06cd81fd38ebfda8fbba904f8e3ea9b543f6545da1f2"
      "d5432955613f0fcf62d49705242a9af9e61e85dc0d651e40dfcf017b45575887" },
};

#define PASS_CAP 16
#define SALT_CAP 16
#define DK_CAP 64

/* (1) The two RFC 7914 vectors, 64 bytes each. */
static void test_kats(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(DK_CAP);
    size_t k;
    char msg[128];

    for (k = 0; k < sizeof(scrypt_kats) / sizeof(scrypt_kats[0]); k++) {
        const struct scrypt_kat *kat = &scrypt_kats[k];
        size_t passlen = from_hex(kat->passhex, pass, PASS_CAP);
        size_t saltlen = from_hex(kat->salthex, salt, SALT_CAP);
        size_t wantlen = from_hex(kat->dkhex, want, DK_CAP);

        CHECK(scrypt_derive(pass, passlen, salt, saltlen, kat->N, kat->r,
                            kat->p, 0, dk, DK_CAP),
              "scrypt derive failed");
        snprintf(msg, sizeof(msg), "N=%llu r=%lu p=%lu does not match the KAT",
                 kat->N, kat->r, kat->p);
        expect_equal(msg, dk, DK_CAP, want, wantlen);
    }
}

/* (2) Parameter validation: N must be > 1 and a power of two (and >= 2),
 * r must be > 0.  These fail while the params are processed, so
 * EVP_KDF_derive reports failure. */
static void test_invalid_params(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen = from_hex(scrypt_kats[0].passhex, pass, PASS_CAP);
    size_t saltlen = from_hex(scrypt_kats[0].salthex, salt, SALT_CAP);

    CHECK(!scrypt_derive(pass, passlen, salt, saltlen, 3, 8, 16, 0,
                         dk, DK_CAP),
          "scrypt accepted N=3 (not a power of two)");
    CHECK(!scrypt_derive(pass, passlen, salt, saltlen, 0, 8, 16, 0,
                         dk, DK_CAP),
          "scrypt accepted N=0");
    CHECK(!scrypt_derive(pass, passlen, salt, saltlen, 1024, 0, 16, 0,
                         dk, DK_CAP),
          "scrypt accepted r=0");
}

/* (3) maxmem_bytes is respected: with a 1 MB limit the N=16384 vector (which
 * needs 128*r*N = 16 MiB of memory) fails, while a 32 MB limit is enough. */
static void test_maxmem(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen = from_hex(scrypt_kats[1].passhex, pass, PASS_CAP);
    size_t saltlen = from_hex(scrypt_kats[1].salthex, salt, SALT_CAP);
    size_t wantlen = from_hex(scrypt_kats[1].dkhex, want, DK_CAP);

    CHECK(!scrypt_derive(pass, passlen, salt, saltlen, 16384, 8, 1,
                         1000000ULL, dk, DK_CAP),
          "scrypt ignored a 1 MB maxmem_bytes limit");
    CHECK(scrypt_derive(pass, passlen, salt, saltlen, 16384, 8, 1,
                        33554432ULL, dk, DK_CAP),
          "scrypt rejected a 32 MB maxmem_bytes limit");
    expect_equal("derive with a 32 MB maxmem_bytes does not match the KAT",
                 dk, DK_CAP, want, wantlen);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_invalid_params();
    test_maxmem();

    printf("test_kdf_scrypt: all %u checks passed\n", g_checks);
    return 0;
}
