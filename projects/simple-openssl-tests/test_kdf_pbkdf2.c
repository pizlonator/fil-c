#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_pbkdf2 -- PBKDF2-HMAC-SHA1 and PBKDF2-HMAC-SHA256 through the
 * EVP_KDF public API: RFC 6070 known-answer vectors, passwords crossing the
 * HMAC block size, and the SP 800-132 lower-bound checks behind the
 * "pkcs5" parameter.
 *
 * Known-answer vectors (every hex value verified against the authoritative
 * pizfix OpenSSL 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:sha1 \
 *       -kdfopt hexpass:<pass> -kdfopt hexsalt:<salt> -kdfopt iter:<c> PBKDF2
 *
 * and the same command under the system OpenSSL 3.0.13; the SHA-256 values
 * additionally match OpenSSL's own test data
 * test/recipes/30-test_evp_data/evpkdf_pbkdf2.txt):
 *
 *   RFC 6070, P="password"  S="salt"                             SHA-1
 *     c=1     -> 0c60c80f961f0e71f3a9b524af6012062fe037a6
 *     c=2     -> ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957
 *     c=4096  -> 4b007901b765489abead49d926f721d065a429c1
 *   RFC 6070, P="passwordPASSWORDpassword" (24 bytes),
 *             S="saltSALTsaltSALTsaltSALTsaltSALTsalt" (36 bytes), SHA-1
 *     c=4096, dkLen=25 -> 3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038
 *
 * The c=16777216 RFC 6070 vector is deliberately not run: it costs minutes.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_pbkdf2: %s\n", msg);
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

/* One PBKDF2 derive with a fresh context.  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  PBKDF2 is core OpenSSL 3 functionality, so a
 * fetch failure fails the program (never SKIP). */
static int pbkdf2_derive(const char *digest, unsigned char *pass,
                         size_t passlen, unsigned char *salt,
                         size_t saltlen, unsigned long iter, int pkcs5,
                         unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "PBKDF2", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok;
    int i5 = pkcs5;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core PBKDF2 KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                  pass, passlen);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  salt, saltlen);
    params[3] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iter);
    params[4] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS5, &i5);
    params[5] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* RFC 6070 and the CLI/test-data SHA-256 vectors. */
static const struct pbkdf2_kat {
    const char *digest;
    const char *passhex;
    const char *salthex;
    unsigned long iter;
    size_t dklen;
    const char *dkhex;
} pbkdf2_kats[] = {
    { "sha1",   /* "password" */
      "70617373776f7264",
      "73616c74",   /* "salt" */
      1, 20,
      "0c60c80f961f0e71f3a9b524af6012062fe037a6" },
    { "sha1",
      "70617373776f7264",
      "73616c74",
      2, 20,
      "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957" },
    { "sha1",
      "70617373776f7264",
      "73616c74",
      4096, 20,
      "4b007901b765489abead49d926f721d065a429c1" },
    { "sha1",   /* "passwordPASSWORDpassword" */
      "70617373776f726450415353574f524470617373776f7264",
      /* "saltSALTsaltSALTsaltSALTsaltSALTsalt" */
      "73616c7453414c5473616c7453414c5473616c7453414c5473616c7453414c54"
      "73616c74",
      4096, 25,
      "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038" },
    { "sha256",
      "70617373776f7264",
      "73616c74",
      1, 32,
      "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b" },
    { "sha256",
      "70617373776f7264",
      "73616c74",
      2, 32,
      "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43" },
    { "sha256",
      "70617373776f7264",
      "73616c74",
      4096, 32,
      "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a" },
    /* A 70-byte password crosses the SHA-256/SHA-1 HMAC block size (64). */
    { "sha256",
      "7070707070707070707070707070707070707070707070707070707070707070"
      "7070707070707070707070707070707070707070707070707070707070707070"
      "707070707070",
      "73616c7453414c5473616c7453414c5473616c7453414c5473616c7453414c54"
      "73616c74",
      1000, 32,
      "5597152377c2edd3ba6cf1b5cb23638cc926927fa859282a2e144f8dd2f0686a" },
};

#define PASS_CAP 70
#define SALT_CAP 36
#define DK_CAP 64

/* (1) Every KAT through the default (pkcs5=1, SP 800-132 checks off) path. */
static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(pbkdf2_kats) / sizeof(pbkdf2_kats[0]); k++) {
        const struct pbkdf2_kat *kat = &pbkdf2_kats[k];
        unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
        unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
        unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
        unsigned char *want = ALLOCATE_OUTPUT(DK_CAP);
        size_t passlen, saltlen, wantlen;
        char msg[128];

        passlen = from_hex(kat->passhex, pass, PASS_CAP);
        saltlen = from_hex(kat->salthex, salt, SALT_CAP);
        wantlen = from_hex(kat->dkhex, want, DK_CAP);

        CHECK(pbkdf2_derive(kat->digest, pass, passlen, salt, saltlen,
                            kat->iter, 1, dk, kat->dklen),
              "PBKDF2 derive failed");
        snprintf(msg, sizeof(msg),
                 "%s c=%lu dkLen=%zu does not match the KAT",
                 kat->digest, kat->iter, kat->dklen);
        expect_equal(msg, dk, kat->dklen, want, wantlen);
    }
}

/* (2) SP 800-132 lower bounds behind "pkcs5"=0: iteration count >= 1000,
 * salt >= 128 bits, derived key >= 112 bits.  With pkcs5=1 (the default)
 * the same weak inputs are accepted.  On 3.0.x the bound is enforced while
 * the params are set (inside derive); on 3.1+ during derive itself -- the
 * observable behavior (EVP_KDF_derive fails) is the same. */
static void test_pkcs5_bounds(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *salt16 = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want_strong = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want_weak = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen, saltlen, wantlen;
    const char *passhex = "70617373776f7264";           /* "password" */
    const char *salthex = "000102030405060708090a0b0c0d0e0f"; /* 16 bytes */

    passlen = from_hex(passhex, pass, PASS_CAP);
    saltlen = from_hex(salthex, salt16, SALT_CAP);
    /* pkcs5=0, c=1000, 16-byte salt, 16-byte key (CLI-verified value). */
    wantlen = from_hex("25eb86acc76e43018f18b9a8f90c2fed", want_strong, DK_CAP);

    /* pkcs5=0 with valid inputs works. */
    CHECK(pbkdf2_derive("sha256", pass, passlen, salt16, saltlen, 1000, 0,
                        dk, 16),
          "pkcs5=0 rejected a compliant derive");
    expect_equal("pkcs5=0 derive does not match the CLI-verified value",
                 dk, 16, want_strong, wantlen);

    /* pkcs5=0 forbids iteration counts below 1000... */
    CHECK(!pbkdf2_derive("sha256", pass, passlen, salt16, saltlen, 1, 0,
                         dk, 16),
          "pkcs5=0 accepted an iteration count of 1");
    /* ...salts shorter than 128 bits... */
    saltlen = from_hex("000102030405060708090a0b0c0d0e", salt, SALT_CAP);
    CHECK(!pbkdf2_derive("sha256", pass, passlen, salt, saltlen, 1000, 0,
                         dk, 16),
          "pkcs5=0 accepted a 15-byte salt");
    /* ...and keys shorter than 112 bits. */
    CHECK(!pbkdf2_derive("sha256", pass, passlen, salt16, 16, 1000, 0,
                         dk, 8),
          "pkcs5=0 accepted an 8-byte derived key");

    /* pkcs5=1 accepts all of the above. */
    wantlen = from_hex("120fb6cffcf8b32c43e7225256c4f837", want_weak, DK_CAP);
    CHECK(pbkdf2_derive("sha256", pass, passlen, salt16, 16, 1, 1, dk, 16),
          "pkcs5=1 rejected an iteration count of 1");
    saltlen = from_hex("73616c74", salt, SALT_CAP); /* "salt", 4 bytes */
    CHECK(pbkdf2_derive("sha256", pass, passlen, salt, saltlen, 1, 1,
                        dk, 16),
          "pkcs5=1 rejected a 4-byte salt");
    expect_equal("pkcs5=1 c=1 derive is not the well-known PBKDF2-HMAC-SHA256 "
                 "prefix",
                 dk, 16, want_weak, wantlen);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_pkcs5_bounds();

    printf("test_kdf_pbkdf2: all %u checks passed\n", g_checks);
    return 0;
}
