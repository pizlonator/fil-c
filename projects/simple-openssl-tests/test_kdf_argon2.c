#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_argon2 -- Argon2d / Argon2i / Argon2id (RFC 9106) through the
 * EVP_KDF public API: the three RFC 9106 section 5 test vectors (which
 * include the 8-byte secret and 12 bytes of associated data), the
 * distinctness of the three variants, the version parameter, and the
 * parameter-validation error paths.
 *
 * OpenSSL 3.2+ only: on older releases (e.g. the system OpenSSL 3.0.13)
 * EVP_KDF_fetch("ARGON2ID") fails and the program takes the SKIP path.
 *
 * KAT correction vs the task brief: the tag 0d640df58d78766c...e6b01e659
 * is the RFC 9106 section 5.3 Argon2id vector, whose inputs are P = 01 x 32,
 * S = 02 x 16, secret = 03 x 8, associated data = 04 x 12 (t=3 passes,
 * m=32 KiB, p=4 lanes, v=0x13) -- NOT the ASCII strings "password"/
 * "somesalt" (those inputs yield bb0cc80a3e671149...a6ab05c under the same
 * cost parameters; that value is embedded below as an extra KAT).
 *
 * Every hex value below was verified with the authoritative pizfix
 * OpenSSL 3.6.5 CLI:
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt hexpass:<P> \
 *       -kdfopt hexsalt:<S> -kdfopt hexsecret:<K> -kdfopt hexad:<X> \
 *       -kdfopt lanes:4 -kdfopt threads:1 -kdfopt memcost:32 \
 *       -kdfopt iter:3 ARGON2ID
 *
 * (threads:1 because the CLI's library context has no thread pool; threads
 * only controls OS parallelism, never the derivation result).  The
 * Argon2-specific parameter names are passed as string literals because
 * the OpenSSL 3.0 headers (which still compile this file for the SKIP
 * path) do not define the OSSL_KDF_PARAM_ARGON2_* / _THREADS macros.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_argon2: %s\n", msg);
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

/* The RFC 9106 section 5 vector inputs. */
static const char *const RFC_PASS_HEX =
    "0101010101010101010101010101010101010101010101010101010101010101";
static const char *const RFC_SALT_HEX = "02020202020202020202020202020202";
static const char *const RFC_SECRET_HEX = "0303030303030303";
static const char *const RFC_AD_HEX = "040404040404040404040404";

static const struct argon2_kat {
    const char *algname;
    const char *taghex;
} argon2_kats[] = {
    { "ARGON2D", "512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb" },
    { "ARGON2I", "c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8" },
    { "ARGON2ID", "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659" },
};

/* Extra CLI-derived KATs: the draft-vector inputs from the task brief
 * ("password"/"somesalt") and the version 1.0 behavior. */
static const char *const PW_HEX = "70617373776f7264";
static const char *const SOMESALT_HEX = "736f6d6573616c74";
static const char *const PW_TAG_HEX =
    "bb0cc80a3e671149526915418c6eefe761bb19d5d2d567a017703e0cea6ab05c";
static const char *const V10_TAG_HEX =
    "7dda2727d1d6662b9f4b53e3828a9ce7787d1a4cc7dc5bce617f11821d57d9f2";

#define PASS_CAP 32
#define SALT_CAP 16
#define SECRET_CAP 8
#define AD_CAP 12
#define OUT_CAP 64

/* One Argon2 derive with a fresh context fetched by algorithm name
 * (ARGON2D/ARGON2I/ARGON2ID are separate fetchable algorithms; the
 * variant is not a settable parameter).  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  The SKIP decision has already been made. */
static int argon2_derive(const char *algname, const unsigned char *pass,
                         size_t passlen, const unsigned char *salt,
                         size_t saltlen, const unsigned char *secret,
                         size_t secretlen, const unsigned char *ad,
                         size_t adlen, unsigned long lanes,
                         unsigned long threads, unsigned long memcost,
                         unsigned long iter, long version,
                         unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, algname, NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[11];
    unsigned int lanes32 = (unsigned int)lanes;
    unsigned int threads32 = (unsigned int)threads;
    unsigned int memcost32 = (unsigned int)memcost;
    unsigned int iter32 = (unsigned int)iter;
    unsigned int version32 = (unsigned int)version;
    int ok, n = 0;
    size_t i;

    if (kdf == NULL)
        return 0;
    for (i = 0; i < outlen; i++)
        out[i] = 0;
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                    (void *)pass, passlen);
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                    (void *)salt, saltlen);
    if (secret != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET,
                                                        (void *)secret,
                                                        secretlen);
    if (ad != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string("ad", (void *)ad,
                                                        adlen);
    params[n++] = OSSL_PARAM_construct_uint32("lanes", &lanes32);
    params[n++] = OSSL_PARAM_construct_uint32("threads", &threads32);
    params[n++] = OSSL_PARAM_construct_uint32("memcost", &memcost32);
    params[n++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iter32);
    if (version >= 0)
        params[n++] = OSSL_PARAM_construct_uint32("version", &version32);
    params[n] = OSSL_PARAM_construct_end();
    ctx = EVP_KDF_CTX_new(kdf);
    if (ctx == NULL) {
        EVP_KDF_free(kdf);
        return 0;
    }
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

int main(int argc, char **argv)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *ad = ALLOCATE_SALT(AD_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t passlen, saltlen, secretlen, adlen, wantlen, k;

    poison_parse_args(argc, argv);

    /* Argon2 arrived in OpenSSL 3.2: a failed fetch takes the SKIP path. */
    {
        EVP_KDF *probe = EVP_KDF_fetch(NULL, "ARGON2ID", NULL);
        if (probe == NULL) {
            printf("SKIP test_kdf_argon2: ARGON2 is unavailable\n");
            return 0;
        }
        EVP_KDF_free(probe);
    }

    /* (1) The three RFC 9106 section 5 tags with t=3, m=32, p=4. */
    passlen = from_hex(RFC_PASS_HEX, pass, PASS_CAP);
    saltlen = from_hex(RFC_SALT_HEX, salt, SALT_CAP);
    secretlen = from_hex(RFC_SECRET_HEX, secret, SECRET_CAP);
    adlen = from_hex(RFC_AD_HEX, ad, AD_CAP);

    for (k = 0; k < sizeof(argon2_kats) / sizeof(argon2_kats[0]); k++) {
        wantlen = from_hex(argon2_kats[k].taghex, want, OUT_CAP);
        CHECK(argon2_derive(argon2_kats[k].algname, pass, passlen, salt,
                            saltlen, secret, secretlen, ad, adlen, 4, 1, 32,
                            3, -1, out, 32),
              "Argon2 RFC 9106 derive failed");
        CHECK(memcmp(out, want, 32) == 0,
              "an RFC 9106 section 5 tag does not match");
    }

    /* (2) Without the secret and associated data the three variants still
     * produce three different tags. */
    {
        unsigned char d[OUT_CAP], it[OUT_CAP], idt[OUT_CAP];
        CHECK(argon2_derive("ARGON2D", pass, passlen, salt, saltlen, NULL, 0,
                            NULL, 0, 4, 1, 32, 3, -1, d, 32),
              "plain Argon2d derive failed");
        CHECK(argon2_derive("ARGON2I", pass, passlen, salt, saltlen, NULL, 0,
                            NULL, 0, 4, 1, 32, 3, -1, it, 32),
              "plain Argon2i derive failed");
        CHECK(argon2_derive("ARGON2ID", pass, passlen, salt, saltlen, NULL, 0,
                            NULL, 0, 4, 1, 32, 3, -1, idt, 32),
              "plain Argon2id derive failed");
        CHECK(memcmp(d, it, 32) != 0 && memcmp(d, idt, 32) != 0 &&
              memcmp(it, idt, 32) != 0,
              "the three Argon2 variants produced identical output");
    }

    /* (3) The draft-vector inputs from the task brief, and version 1.0. */
    passlen = from_hex(PW_HEX, pass, PASS_CAP);
    saltlen = from_hex(SOMESALT_HEX, salt, SALT_CAP);
    wantlen = from_hex(PW_TAG_HEX, want, OUT_CAP);
    CHECK(argon2_derive("ARGON2ID", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, -1, out, 32),
          "Argon2id password/somesalt derive failed");
    expect_equal("Argon2id password/somesalt output does not match the "
                 "CLI-verified value", out, 32, want, wantlen);

    passlen = from_hex(RFC_PASS_HEX, pass, PASS_CAP);
    saltlen = from_hex(RFC_SALT_HEX, salt, SALT_CAP);
    wantlen = from_hex(V10_TAG_HEX, want, OUT_CAP);
    CHECK(argon2_derive("ARGON2ID", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, 16, out, 32),
          "Argon2id version=1.0 derive failed");
    expect_equal("version 1.0 output does not match the CLI-verified value",
                 out, 32, want, wantlen);

    /* (4) Validation: a missing salt, lanes=0, memcost<8, iter=0, a short
     * salt and a too-short output must all fail. */
    CHECK(!argon2_derive("ARGON2D", pass, passlen, salt, 0, NULL, 0, NULL, 0,
                         4, 1, 32, 3, -1, out, 32),
          "Argon2 accepted a missing salt");
    CHECK(!argon2_derive("ARGON2D", pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 0, 1, 32, 3, -1, out, 32),
          "Argon2 accepted lanes=0");
    CHECK(!argon2_derive("ARGON2D", pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 4, 1, 7, 3, -1, out, 32),
          "Argon2 accepted memcost=7");
    CHECK(!argon2_derive("ARGON2D", pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 4, 1, 32, 0, -1, out, 32),
          "Argon2 accepted iter=0");
    CHECK(!argon2_derive("ARGON2D", pass, passlen, salt, 2, NULL, 0, NULL, 0,
                         4, 1, 32, 3, -1, out, 32),
          "Argon2 accepted a 2-byte salt");
    CHECK(!argon2_derive("ARGON2D", pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 4, 1, 32, 3, -1, out, 3),
          "Argon2 accepted a 3-byte output");

    printf("test_kdf_argon2: all %u checks passed\n", g_checks);
    return 0;
}
